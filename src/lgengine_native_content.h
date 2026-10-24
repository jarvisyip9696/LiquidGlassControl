#pragma once
// Native content bridge. Taskbar integration is opt-in behind the experimental
// build switch; start/notification roots are not connected to this bridge.
// Preserve native XAML layout/input. Only the composition visual is suppressed,
// and only after the host confirms presentation of the native content bitmap.
#include "../integrations/LGEngine/client/lg_client.h"
#include <inspectable.h>
#include <roapi.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Composition.h>
#include <functional>
#include <memory>
#include <map>

namespace lghost {
struct NativeCaptureHooks {
    // A cropped surface retains native input and must contain every visible
    // source pixel. Material changes have an explicit matching restore hook.
    std::function<bool(Spec&)> sourcePlace;
    std::function<void()> prepare,restore;
};
inline bool cropNativeContent(std::vector<uint32_t> const& source,Spec const& from,Spec const& to,std::vector<uint32_t>& out){
    int left=to.x-from.x,top=to.y-from.y;
    if(from.width<1||from.height<1||to.width<1||to.height<1||left<0||top<0||left+to.width>from.width||top+to.height>from.height||source.size()!=size_t(from.width)*from.height)return false;
    for(int y=0;y<from.height;++y)for(int x=0;x<from.width;++x)
        if((x<left||x>=left+to.width||y<top||y>=top+to.height)&&(source[size_t(y)*from.width+x]>>24)>8)return false;
    out.resize(size_t(to.width)*to.height);
    for(int y=0;y<to.height;++y)std::copy_n(source.begin()+size_t(y+top)*from.width+left,to.width,out.begin()+size_t(y)*to.width);
    return true;
}
// The ABI is used to avoid mixing generated Windows SDK projection versions.
struct NativeRenderBitmap:IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Width(INT32*)=0;
    virtual HRESULT STDMETHODCALLTYPE Height(INT32*)=0;
    virtual HRESULT STDMETHODCALLTYPE Render(void*,void**)=0;
    virtual HRESULT STDMETHODCALLTYPE RenderSized(void*,INT32,INT32,void**)=0;
    virtual HRESULT STDMETHODCALLTYPE Pixels(void**)=0;
};
class NativeContentSurface:public std::enable_shared_from_this<NativeContentSurface> {
    using Element=winrt::Windows::UI::Xaml::FrameworkElement;
    static constexpr UINT Event=WM_APP+60,Refresh=WM_APP+61;
    winrt::weak_ref<Element> element;
    winrt::Windows::UI::Composition::Visual visual{nullptr};
    winrt::event_token layout{},unloaded{};
    std::unique_ptr<Client> client;HWND window{};Spec spec;
    winrt::Windows::Foundation::IAsyncInfo operation{nullptr};
    std::function<bool(Spec&)> place;std::function<void(HRESULT)> status;
    NativeCaptureHooks hooks;
    std::shared_ptr<void> nativeMotion;
    float originalOpacity=1;bool hidden=false,busy=false,queued=false,stopped=false,presented=false,captureCancelled=false,contentReady=false,suspended=false;
    uint64_t lastFrame=0;bool lastCaptureFailed=false;
    uint64_t contentRevision=0;
    std::wstring captureDetail;
    HWND nativeWindow{};HWINEVENTHOOK nativeEvents{},foregroundEvents{};bool placementVisible=true;
    inline static thread_local std::map<HWINEVENTHOOK,std::weak_ptr<NativeContentSurface>> windowWatches;
    static void CALLBACK nativeChanged(HWINEVENTHOOK hook,DWORD event,HWND window,LONG object,LONG,DWORD,DWORD){
        if((event!=EVENT_SYSTEM_FOREGROUND&&object!=OBJID_WINDOW)||!window)return;auto i=windowWatches.find(hook);if(i==windowWatches.end())return;
        if(auto self=i->second.lock();self&&(event==EVENT_SYSTEM_FOREGROUND||GetAncestor(window,GA_ROOT)==GetAncestor(self->nativeWindow,GA_ROOT)))self->request();
    }
    void restoreVisual()noexcept{if(hidden){try{if(visual&&visual.Opacity()==0)visual.Opacity(originalOpacity);}catch(...){}hidden=false;}try{if(hooks.restore)hooks.restore();}catch(...){}}
    void request(){if(!stopped&&!suspended&&window&&!queued){queued=true;PostMessageW(window,Refresh,0,0);}}
    void schedule(){queued=false;if(stopped||suspended)return;if(busy){queued=true;return;}
        auto now=GetTickCount64();unsigned interval=lastCaptureFailed?500:33;if(now-lastFrame<interval){SetTimer(window,1,UINT(interval-(now-lastFrame)),nullptr);return;}
        render(shared_from_this());
    }
    static winrt::fire_and_forget render(std::shared_ptr<NativeContentSurface> self){
        self->busy=true;self->captureCancelled=false;
        try{
            auto e=self->element.get();auto updated=self->spec;
            if(!e||!e.IsLoaded()||e.Visibility()!=winrt::Windows::UI::Xaml::Visibility::Visible||!self->place(updated)){
                self->contentReady=false;self->client->show(false);self->presented=false;self->restoreVisual();self->busy=false;co_return;
            }
            if(!wire::valid(updated))throw winrt::hresult_error(E_INVALIDARG);
            bool resized=updated.width!=self->spec.width||updated.height!=self->spec.height;
            if(resized){self->restoreVisual();self->presented=false;self->client->show(false);self->client->setSpec(updated);}
            self->spec=updated;self->client->setPlace(updated.x,updated.y);
            auto sourceSpec=updated;
            if(self->hooks.sourcePlace&&!self->hooks.sourcePlace(sourceSpec))throw winrt::hresult_error(E_PENDING);
            if(!wire::valid(sourceSpec))throw winrt::hresult_error(E_INVALIDARG);
            if(self->hooks.prepare)self->hooks.prepare();
            winrt::Windows::Foundation::IInspectable bitmap{nullptr};
            winrt::check_hresult(RoActivateInstance(reinterpret_cast<HSTRING>(winrt::get_abi(winrt::hstring(L"Windows.UI.Xaml.Media.Imaging.RenderTargetBitmap"))),reinterpret_cast<IInspectable**>(winrt::put_abi(bitmap))));
            winrt::com_ptr<NativeRenderBitmap> native;
            const GUID id{0x500dee81,0x893c,0x4c0a,{0x8f,0xec,0x46,0x78,0xac,0x71,0x75,0x89}};
            winrt::check_hresult(winrt::get_unknown(bitmap)->QueryInterface(id,native.put_void()));
            winrt::Windows::Foundation::IAsyncAction action{nullptr};
            auto ui=e.as<winrt::Windows::UI::Xaml::UIElement>();
            // RenderSized dimensions are rasterized again by the island scale.
            double raster=e.XamlRoot().RasterizationScale();if(!std::isfinite(raster)||raster<=0)throw winrt::hresult_error(E_INVALIDARG);
            winrt::check_hresult(native->RenderSized(winrt::get_abi(ui),std::max(1,int(std::lround(sourceSpec.width/raster))),std::max(1,int(std::lround(sourceSpec.height/raster))),winrt::put_abi(action)));
            self->operation=action.as<winrt::Windows::Foundation::IAsyncInfo>();if(!SetTimer(self->window,2,1500,nullptr)){self->operation.Cancel();winrt::throw_last_error();}
            co_await action;
            if(self->stopped||self->captureCancelled)throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_CANCELLED));
            winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> pixels{nullptr};
            winrt::check_hresult(native->Pixels(winrt::put_abi(pixels)));self->operation=pixels.as<winrt::Windows::Foundation::IAsyncInfo>();auto buffer=co_await pixels;
            if(self->stopped||self->captureCancelled)throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_CANCELLED));
            INT32 pixelWidth=0,pixelHeight=0;winrt::check_hresult(native->Width(&pixelWidth));winrt::check_hresult(native->Height(&pixelHeight));
            size_t count=size_t(std::max(0,pixelWidth))*std::max(0,pixelHeight);
            // DPI clipping or unsupported render sizes must never stretch a
            // partial snapshot over native controls or hide their live content.
            self->captureDetail=L"pixels="+std::to_wstring(pixelWidth)+L"x"+std::to_wstring(pixelHeight)+L" source="+std::to_wstring(sourceSpec.width)+L"x"+std::to_wstring(sourceSpec.height);
            if(pixelWidth<sourceSpec.width||pixelHeight<sourceSpec.height||pixelWidth>sourceSpec.width+1||pixelHeight>sourceSpec.height+1||buffer.Length()!=count*4)throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
            std::vector<uint32_t> content(count);
            auto reader=winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
            reader.ReadBytes(winrt::array_view<uint8_t>(reinterpret_cast<uint8_t*>(content.data()),reinterpret_cast<uint8_t*>(content.data()+count)));
            // RenderTargetBitmap can pad its raster by one transparent pixel.
            // Trim padding only after proving it contains no visible content;
            // never rescale glyphs or accept a clipped/undersized capture.
            if(pixelWidth!=sourceSpec.width||pixelHeight!=sourceSpec.height){auto padded=sourceSpec;padded.width=pixelWidth;padded.height=pixelHeight;std::vector<uint32_t> trimmed;if(!cropNativeContent(content,padded,sourceSpec,trimmed))throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));content=std::move(trimmed);count=content.size();}
            if(self->hooks.sourcePlace){std::vector<uint32_t> cropped;if(!cropNativeContent(content,sourceSpec,updated,cropped)){
                int minX=sourceSpec.width,minY=sourceSpec.height,maxX=-1,maxY=-1;for(int y=0;y<sourceSpec.height;++y)for(int x=0;x<sourceSpec.width;++x)if((content[size_t(y)*sourceSpec.width+x]>>24)>8){minX=std::min(minX,x);minY=std::min(minY,y);maxX=std::max(maxX,x);maxY=std::max(maxY,y);}
                self->captureDetail+=L" crop="+std::to_wstring(updated.x-sourceSpec.x)+L","+std::to_wstring(updated.y-sourceSpec.y)+L","+std::to_wstring(updated.width)+L","+std::to_wstring(updated.height)+L" ink="+std::to_wstring(minX)+L","+std::to_wstring(minY)+L","+std::to_wstring(maxX)+L","+std::to_wstring(maxY);
                throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));}content=std::move(cropped);count=content.size();}
            size_t ink=0,opaque=0;for(auto pixel:content){if(pixel>>24)++ink;if((pixel>>24)>250)++opaque;}
            self->captureDetail+=L" ink="+std::to_wstring(ink)+L" opaque="+std::to_wstring(opaque)+L" total="+std::to_wstring(count);
            // An unsupported visual can snapshot as empty; an opaque backing
            // would cover the glass. Neither result may hide the native UI.
            if(!ink||opaque>count*9/10)throw winrt::hresult_error(E_NOTIMPL);
            self->client->setContents(std::move(content));self->contentReady=true;self->client->show();++self->contentRevision;self->lastFrame=GetTickCount64();self->lastCaptureFailed=false;
        }catch(...){self->lastFrame=GetTickCount64();self->lastCaptureFailed=true;self->contentReady=false;self->restoreVisual();self->presented=false;if(self->client)self->client->show(false);if(!self->stopped)self->status(winrt::to_hresult());}
        if(self->window)KillTimer(self->window,2);self->operation=nullptr;self->busy=false;
        if(self->queued&&!self->stopped){self->queued=false;self->request();}
    }
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
        auto p=reinterpret_cast<NativeContentSurface*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){p=static_cast<NativeContentSurface*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
        if(p&&!p->stopped){auto keep=p->shared_from_this();try{
            if(m==Refresh){p->schedule();return 0;}
            if(m==WM_TIMER&&w==1){KillTimer(h,1);p->schedule();return 0;}
            if(m==WM_TIMER&&w==3){if(!p->suspended){auto next=p->spec;bool visible=p->place(next);if(visible!=p->placementVisible||(visible&&(next.x!=p->spec.x||next.y!=p->spec.y||next.width!=p->spec.width||next.height!=p->spec.height||next.scale!=p->spec.scale))){p->placementVisible=visible;p->request();}}return 0;}
            if(m==WM_TIMER&&w==2){KillTimer(h,2);p->captureCancelled=true;p->contentReady=false;p->queued=false;p->restoreVisual();p->presented=false;p->client->show(false);if(p->operation)p->operation.Cancel();p->status(HRESULT_FROM_WIN32(WAIT_TIMEOUT));return 0;}
            if(m==Event){
                if(w==Shown){auto e=p->element.get();auto current=p->spec;if(!p->suspended&&p->contentReady&&!p->captureCancelled&&e&&e.IsLoaded()&&p->place(current)&&current.width==p->spec.width&&current.height==p->spec.height&&current.x==p->spec.x&&current.y==p->spec.y&&current.scale==p->spec.scale){p->presented=true;if(!p->hidden){p->originalOpacity=p->visual.Opacity();p->visual.Opacity(0);p->hidden=true;}p->status(S_OK);}else{p->restoreVisual();p->presented=false;p->client->show(false);if(e&&e.IsLoaded()&&!p->suspended)p->request();}}
                else if(w==Lost||w==Failed||w==Hidden){
                    // Initial/resize Hidden acknowledgements may arrive while
                    // the first native snapshot is being prepared. They must
                    // not put its solid material back into that snapshot.
                    if(w==Hidden&&p->busy&&!p->hidden&&!p->suspended)return 0;
                    if(w!=Hidden){p->captureCancelled=true;p->contentReady=false;if(p->operation)p->operation.Cancel();}
                    p->restoreVisual();p->presented=false;if(w!=Hidden)p->status(HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED));}
                else if(w==Restarted||w==Connected)p->request();return 0;
            }
        }catch(...){p->restoreVisual();p->status(winrt::to_hresult());}}
        return DefWindowProcW(h,m,w,l);
    }
    NativeContentSurface(Element const&e,Spec s,std::function<bool(Spec&)> position,std::function<void(HRESULT)> result):element(winrt::make_weak(e)),spec(std::move(s)),place(std::move(position)),status(std::move(result)){}
public:
    static std::shared_ptr<NativeContentSurface> create(Element const&e,Options options,Spec spec,std::function<bool(Spec&)> place,std::function<void(HRESULT)> status,NativeCaptureHooks hooks={}){
        auto self=std::shared_ptr<NativeContentSurface>(new NativeContentSurface(e,spec,std::move(place),std::move(status)));
        self->hooks=std::move(hooks);
        self->visual=winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(e);
        auto name=L"LGEngine.NativeContent."+std::to_wstring(reinterpret_cast<uintptr_t>(&proc));
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=proc;cls.lpszClassName=name.c_str();
        if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)winrt::throw_last_error();
        self->window=CreateWindowExW(0,name.c_str(),L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,self.get());if(!self->window)winrt::throw_last_error();
        options.notify=self->window;options.message=Event;self->client=std::make_unique<Client>(options,spec);
        auto weak=std::weak_ptr<NativeContentSurface>(self);
        // RenderAsync itself can trigger LayoutUpdated. Do not turn a native
        // content snapshot into a perpetual capture loop.
        self->layout=e.LayoutUpdated([weak](auto&&,auto&&){if(auto p=weak.lock();p&&!p->busy)p->request();});
        self->unloaded=e.Unloaded([weak](auto&&,auto&&){if(auto p=weak.lock()){p->captureCancelled=true;p->contentReady=false;p->queued=false;if(p->operation)p->operation.Cancel();p->client->show(false);p->restoreVisual();p->presented=false;}});
        self->request();return self;
    }
    // Suspend while the native owner is temporarily hidden or occluded. Keep
    // the client alive, but never leave its last snapshot above live windows.
    void suspend(){if(stopped||suspended)return;suspended=true;captureCancelled=true;contentReady=false;queued=false;KillTimer(window,1);KillTimer(window,2);restoreVisual();presented=false;client->show(false);if(operation)operation.Cancel();}
    void resume(){if(stopped||!suspended)return;suspended=false;request();}
    bool isPresented()const{return presented;}
    void invalidateContents(){request();}
    void watchNativeWindow(HWND target){
        DWORD pid{};auto thread=GetWindowThreadProcessId(target,&pid);
        if(!thread||pid!=GetCurrentProcessId())throw winrt::hresult_error(E_ACCESSDENIED);
        if(nativeEvents){windowWatches.erase(nativeEvents);UnhookWinEvent(nativeEvents);}
        nativeWindow=target;nativeEvents=SetWinEventHook(EVENT_OBJECT_SHOW,EVENT_OBJECT_LOCATIONCHANGE,nullptr,nativeChanged,pid,thread,WINEVENT_OUTOFCONTEXT);
        if(!nativeEvents)winrt::throw_last_error();windowWatches[nativeEvents]=shared_from_this();
        if(!foregroundEvents){foregroundEvents=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,nativeChanged,0,0,WINEVENT_OUTOFCONTEXT);if(foregroundEvents)windowWatches[foregroundEvents]=shared_from_this();}
        if(!SetTimer(window,3,16,nullptr))winrt::throw_last_error();
    }
    void retainNativeMotion(std::shared_ptr<void> value){nativeMotion=std::move(value);}
    unsigned hostProcessId()const{return client?client->hostProcessId():0;}
    uint64_t publishedContentRevision()const{return contentRevision;}
    std::wstring const& lastCaptureDetail()const{return captureDetail;}
    void stop()noexcept{if(stopped)return;stopped=true;nativeMotion.reset();captureCancelled=true;contentReady=false;restoreVisual();if(nativeEvents){windowWatches.erase(nativeEvents);UnhookWinEvent(nativeEvents);nativeEvents=nullptr;}if(foregroundEvents){windowWatches.erase(foregroundEvents);UnhookWinEvent(foregroundEvents);foregroundEvents=nullptr;}try{if(operation)operation.Cancel();if(auto e=element.get()){e.LayoutUpdated(layout);e.Unloaded(unloaded);}}catch(...){}client.reset();if(window){KillTimer(window,1);KillTimer(window,2);KillTimer(window,3);DestroyWindow(window);window=nullptr;}}
    ~NativeContentSurface(){stop();}
};
}
