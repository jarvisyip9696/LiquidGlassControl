#pragma once
#include "lgengine_native_place.h"
#include "glass_protocol.h"
#include "native_window_opacity.h"
#include "taskbar_content_motion.h"
#include <winrt/Windows.UI.Xaml.Shapes.h>
namespace liquid {
inline HWND taskbarNativeOwner(HWND island){
    struct Search {HMONITOR monitor;int width,height;HWND found=nullptr;bool ambiguous=false;};RECT client{};GetClientRect(island,&client);
    Search search{MonitorFromWindow(island,MONITOR_DEFAULTTONEAREST),client.right,client.bottom};
    EnumWindows([](HWND h,LPARAM context)->BOOL{auto& s=*reinterpret_cast<Search*>(context);DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(pid!=GetCurrentProcessId()||!IsWindowVisible(h)||MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST)!=s.monitor)return TRUE;
        wchar_t name[64]{};GetClassNameW(h,name,64);if(wcscmp(name,L"Shell_TrayWnd")&&wcscmp(name,L"Shell_SecondaryTrayWnd"))return TRUE;RECT r{};if(!GetClientRect(h,&r)||std::abs(r.right-s.width)>2||std::abs(r.bottom-s.height)>2)return TRUE;
        if(s.found&&s.found!=h)s.ambiguous=true;s.found=h;return TRUE;},reinterpret_cast<LPARAM>(&search));
    return search.ambiguous?nullptr:search.found;
}
inline bool taskbarPlace(winrt::Windows::UI::Xaml::FrameworkElement const& element,HWND island,HWND owner,lghost::Spec& spec){
    DWORD pid=0;GetWindowThreadProcessId(owner,&pid);if(pid!=GetCurrentProcessId()||!IsWindowVisible(owner)||!lghost::nativePlace(element,island,spec))return false;
    POINT source{},native{};if(!ClientToScreen(island,&source)||!ClientToScreen(owner,&native))return false;
    spec.x+=native.x-source.x;spec.y+=native.y-source.y;return lghost::wire::valid(spec);
}
struct TaskbarMaterialLease {
    using Element=winrt::Windows::UI::Xaml::FrameworkElement;
    struct Saved {winrt::weak_ref<Element> element;double opacity=1;bool owned=false;};
    std::vector<Saved> saved;
    void prepare(){for(auto& s:saved)if(auto e=s.element.get()){
        if(s.owned&&e.Opacity()!=0)throw winrt::hresult_error(E_ABORT);
        if(!s.owned){s.opacity=e.Opacity();s.owned=true;e.Opacity(0);}
    }else throw winrt::hresult_error(E_ABORT);}
    void restore()noexcept{for(auto& s:saved)if(s.owned){try{if(auto e=s.element.get();e&&e.Opacity()==0)e.Opacity(s.opacity);}catch(...){}s.owned=false;}}
    ~TaskbarMaterialLease(){restore();}
};
// Only bridge a measured, loaded native taskbar island. The content bridge
// rejects empty/opaque snapshots and keeps the native visual until Shown.
inline std::shared_ptr<lghost::NativeContentSurface> taskbarOpticalBridge(
    winrt::Windows::UI::Xaml::FrameworkElement const& element,
    glass::Config const& config,std::wstring const& controllerDirectory,
    std::function<void(HRESULT)> status,std::wstring suffix={},std::function<void(std::wstring const&)> diagnostic={},HWND ownerHint=nullptr){
    if(config.economy||!(config.features&glass::Taskbar)||!(config.features&glass::Capsule))return {};
    if(element.Name()!=L"RootGrid"||!element.IsLoaded())return {};
    auto island=lghost::nativeIslandWindow(element);if(!island)return {};
    lghost::Spec spec;spec.hover=false;spec.motion=0;spec.adapt=true;spec.inShots=true;
    BOOL animate=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0);
    spec.motion=animate?1:0;spec.inMs=220;spec.outMs=180;
    spec.label="LiquidGlassControl.Taskbar";
    spec.savePower=false;spec.fps=60;spec.look.dark=int(config.opacity);
    spec.look.blur=int(config.blur)*100/40;spec.look.round=int(config.radius);
    spec.look.refraction=80;spec.look.rim=60;
    if(!lghost::nativePlace(element,island,spec))return {};
    lghost::Options options;
    options.suffix=std::move(suffix);
    options.hostPath=lghost::bundledHostPath(controllerDirectory+L"lgengine\\");
    if(GetFileAttributesW(options.hostPath.c_str())==INVALID_FILE_ATTRIBUTES){status(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));return {};}
    HWND owner=ownerHint?ownerHint:taskbarNativeOwner(island);if(!owner){if(diagnostic)diagnostic(L"native taskbar owner is unavailable or ambiguous");status(E_PENDING);return {};}
    using namespace winrt::Windows::UI::Xaml;
    Shapes::Rectangle fill{nullptr};bool tray=false,ambiguous=false;
    auto material=std::make_shared<TaskbarMaterialLease>();
    auto capture=element;std::vector<DependencyObject> pending;unsigned visited=0;
    for(unsigned depth=0;depth<8;++depth){
      fill=nullptr;tray=false;ambiguous=false;material->saved.clear();pending={capture};visited=0;
      while(!pending.empty()&&++visited<=4096){auto node=pending.back();pending.pop_back();
        if(auto frame=node.try_as<FrameworkElement>()){
            if(frame.Name()==L"SystemTrayFrameGrid")tray=true;
            if(frame.Name()==L"BackgroundFill")if(auto rectangle=frame.try_as<Shapes::Rectangle>()){
                if(fill)ambiguous=true;fill=rectangle;
                material->saved.push_back({winrt::make_weak(frame),1,false});
            }
            if(frame.Name()==L"BackgroundStroke")material->saved.push_back({winrt::make_weak(frame),1,false});
        }
        auto count=Media::VisualTreeHelper::GetChildrenCount(node);
        for(int i=0;i<count;++i)pending.push_back(Media::VisualTreeHelper::GetChild(node,i));
      }
      if(diagnostic)diagnostic(L"scope depth="+std::to_wstring(depth)+L" type="+std::wstring(winrt::get_class_name(capture).c_str())+L" visited="+std::to_wstring(visited)+L" tray="+std::to_wstring(tray)+L" fill="+std::to_wstring(bool(fill))+L" ambiguous="+std::to_wstring(ambiguous));
      if(pending.empty()&&tray&&fill&&!ambiguous)break;
      auto parent=Media::VisualTreeHelper::GetParent(capture).try_as<FrameworkElement>();
      if(!parent||parent.XamlRoot()!=element.XamlRoot())break;capture=parent;
    }
    // A root without the tray is not a complete native taskbar. Never hide it
    // and cover an independent tray with an incomplete snapshot.
    if(!pending.empty()||!tray||!fill||ambiguous){status(E_NOTIMPL);return {};}
    auto weak=winrt::make_weak(capture);
    auto weakFill=winrt::make_weak(fill);
    auto locate=[weak,weakFill,island,owner,diagnostic](lghost::Spec& next,bool geometryOnly){auto e=weak.get();auto f=weakFill.get();if(!e||!f)return false;
        auto full=next;if(!taskbarPlace(e,island,owner,full)){if(diagnostic)diagnostic(L"placement source not ready");return false;}
        if(!taskbarPlace(f,island,owner,next)){if(diagnostic)diagnostic(L"placement material not ready width="+std::to_wstring(f.ActualWidth())+L" height="+std::to_wstring(f.ActualHeight()));return false;}
        int padding=std::max(1,int(std::lround(next.scale*2)));
        int right=std::min(full.x+full.width,next.x+next.width+padding),bottom=std::min(full.y+full.height,next.y+next.height+padding);
        next.x=std::max(full.x,next.x-padding);next.y=std::max(full.y,next.y-padding);
        next.width=right-next.x;next.height=bottom-next.y;
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);RECT exposed{},candidate{next.x,next.y,next.x+next.width,next.y+next.height};
        if(!geometryOnly&&GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor)&&(!IntersectRect(&exposed,&candidate,&monitor.rcMonitor)||exposed.bottom-exposed.top<std::max(3,next.height/5)))return false;
        HWND foreground=GetForegroundWindow();RECT front{},area{next.x,next.y,next.x+next.width,next.y+next.height},overlap{};
        bool above=false;unsigned windows=0;
        if(foreground&&foreground!=owner)for(HWND h=GetTopWindow(nullptr);h&&++windows<4096;h=GetWindow(h,GW_HWNDNEXT)){if(h==owner)break;if(h==foreground){above=true;break;}}
        // Maximized apps can extend underneath an auto-hide taskbar. Only a
        // window actually above the native taskbar may occlude its glass.
        if(!geometryOnly&&above&&!IsIconic(foreground)&&GetWindowRect(foreground,&front)&&IntersectRect(&overlap,&front,&area)){if(diagnostic)diagnostic(L"placement occluded by foreground window");return false;}
        next.look.round=int(std::lround(f.RadiusX()));return lghost::wire::valid(next);
    };
    // Keep a waiting bridge while an auto-hide taskbar is covered. Its native
    // owner events wake capture when it unfolds; never require an app reload.
    if(!locate(spec,true)){status(E_PENDING);return {};}
    auto place=[locate](lghost::Spec& next){return locate(next,false);};
    lghost::NativeCaptureHooks hooks;
    hooks.sourcePlace=[weak,island,owner](lghost::Spec& next){auto e=weak.get();return e&&taskbarPlace(e,island,owner,next);};
    auto nativeOpacity=std::make_shared<NativeWindowOpacityLease>(owner);
    hooks.prepare=[material,nativeOpacity]{material->prepare();nativeOpacity->prepare();};hooks.restore=[material,nativeOpacity]{nativeOpacity->restore();material->restore();};
    auto bridge=lghost::NativeContentSurface::create(capture,options,spec,
        std::move(place),std::move(status),std::move(hooks));
    bridge->watchNativeWindow(owner);
    std::weak_ptr<lghost::NativeContentSurface> weakBridge=bridge;
    bridge->retainNativeMotion(TaskbarContentMotion::create(element,[weakBridge]{if(auto b=weakBridge.lock())b->invalidateContents();},[island,owner](FrameworkElement const& image,RECT& area){lghost::Spec at;if(!taskbarPlace(image,island,owner,at))return false;area={at.x-6,at.y-6,at.x+at.width+6,at.y+at.height+6};return true;}));
    return bridge;
}
}
