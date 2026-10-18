#pragma once
// Integration prototype, not enabled in the production shell backend. Tested
// with a self-created Windows XAML island, not taskbar/start/notification roots.
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
    float originalOpacity=1;bool hidden=false,busy=false,queued=false,stopped=false,presented=false,captureCancelled=false,contentReady=false,suspended=false;
    uint64_t lastFrame=0;
    uint64_t contentRevision=0;
    HWND nativeWindow{};HWINEVENTHOOK nativeEvents{};
    inline static thread_local std::map<HWINEVENTHOOK,std::weak_ptr<NativeContentSurface>> windowWatches;
    static void CALLBACK nativeChanged(HWINEVENTHOOK hook,DWORD,HWND window,LONG object,LONG,DWORD,DWORD){
        if(object!=OBJID_WINDOW||!window)return;auto i=windowWatches.find(hook);if(i==windowWatches.end())return;
        if(auto self=i->second.lock();self&&GetAncestor(window,GA_ROOT)==GetAncestor(self->nativeWindow,GA_ROOT))self->request();
    }
    void restoreVisual()noexcept{if(hidden){try{if(visual&&visual.Opacity()==0)visual.Opacity(originalOpacity);}catch(...){}hidden=false;}}
    void request(){if(!stopped&&!suspended&&window&&!queued){queued=true;PostMessageW(window,Refresh,0,0);}}
    void schedule(){queued=false;if(stopped||suspended)return;if(busy){queued=true;return;}
        auto now=GetTickCount64();if(now-lastFrame<67){SetTimer(window,1,UINT(67-(now-lastFrame)),nullptr);return;}
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
            winrt::Windows::Foundation::IInspectable bitmap{nullptr};
            winrt::check_hresult(RoActivateInstance(reinterpret_cast<HSTRING>(winrt::get_abi(winrt::hstring(L"Windows.UI.Xaml.Media.Imaging.RenderTargetBitmap"))),reinterpret_cast<IInspectable**>(winrt::put_abi(bitmap))));
            winrt::com_ptr<NativeRenderBitmap> native;
            const GUID id{0x500dee81,0x893c,0x4c0a,{0x8f,0xec,0x46,0x78,0xac,0x71,0x75,0x89}};
            winrt::check_hresult(winrt::get_unknown(bitmap)->QueryInterface(id,native.put_void()));
            winrt::Windows::Foundation::IAsyncAction action{nullptr};
            auto ui=e.as<winrt::Windows::UI::Xaml::UIElement>();
            winrt::check_hresult(native->RenderSized(winrt::get_abi(ui),updated.width,updated.height,winrt::put_abi(action)));
            self->operation=action.as<winrt::Windows::Foundation::IAsyncInfo>();if(!SetTimer(self->window,2,1500,nullptr)){self->operation.Cancel();winrt::throw_last_error();}
            co_await action;
            if(self->stopped||self->captureCancelled)throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_CANCELLED));
            winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> pixels{nullptr};
            winrt::check_hresult(native->Pixels(winrt::put_abi(pixels)));self->operation=pixels.as<winrt::Windows::Foundation::IAsyncInfo>();auto buffer=co_await pixels;
            if(self->stopped||self->captureCancelled)throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_CANCELLED));
            INT32 pixelWidth=0,pixelHeight=0;winrt::check_hresult(native->Width(&pixelWidth));winrt::check_hresult(native->Height(&pixelHeight));
            size_t count=size_t(updated.width)*updated.height;
            // DPI clipping or unsupported render sizes must never stretch a
            // partial snapshot over native controls or hide their live content.
            if(pixelWidth!=updated.width||pixelHeight!=updated.height||buffer.Length()!=count*4)throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
            std::vector<uint32_t> content(count);
            auto reader=winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
            reader.ReadBytes(winrt::array_view<uint8_t>(reinterpret_cast<uint8_t*>(content.data()),reinterpret_cast<uint8_t*>(content.data()+count)));
            size_t ink=0,opaque=0;for(auto pixel:content){if(pixel>>24)++ink;if((pixel>>24)>250)++opaque;}
            // An unsupported visual can snapshot as empty; an opaque backing
            // would cover the glass. Neither result may hide the native UI.
            if(!ink||opaque>count*9/10)throw winrt::hresult_error(E_NOTIMPL);
            self->client->setContents(std::move(content));self->contentReady=true;self->client->show();++self->contentRevision;self->lastFrame=GetTickCount64();
        }catch(...){self->contentReady=false;self->restoreVisual();self->presented=false;if(self->client)self->client->show(false);if(!self->stopped)self->status(winrt::to_hresult());}
        if(self->window)KillTimer(self->window,2);self->operation=nullptr;self->busy=false;
        if(self->queued&&!self->stopped){self->queued=false;self->request();}
    }
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
        auto p=reinterpret_cast<NativeContentSurface*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){p=static_cast<NativeContentSurface*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
        if(p&&!p->stopped){auto keep=p->shared_from_this();try{
            if(m==Refresh){p->schedule();return 0;}
            if(m==WM_TIMER&&w==1){KillTimer(h,1);p->schedule();return 0;}
            if(m==WM_TIMER&&w==2){KillTimer(h,2);p->captureCancelled=true;p->contentReady=false;p->queued=false;p->restoreVisual();p->presented=false;p->client->show(false);if(p->operation)p->operation.Cancel();p->status(HRESULT_FROM_WIN32(WAIT_TIMEOUT));return 0;}
            if(m==Event){
                if(w==Shown){auto e=p->element.get();auto current=p->spec;if(!p->suspended&&p->contentReady&&!p->captureCancelled&&e&&e.IsLoaded()&&p->place(current)&&current.width==p->spec.width&&current.height==p->spec.height&&current.x==p->spec.x&&current.y==p->spec.y&&current.scale==p->spec.scale){p->presented=true;if(!p->hidden){p->originalOpacity=p->visual.Opacity();p->visual.Opacity(0);p->hidden=true;}p->status(S_OK);}else{p->restoreVisual();p->presented=false;p->client->show(false);if(e&&e.IsLoaded()&&!p->suspended)p->request();}}
                else if(w==Lost||w==Failed||w==Hidden){p->restoreVisual();p->presented=false;if(w!=Hidden)p->status(HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED));}
                else if(w==Restarted||w==Connected)p->request();return 0;
            }
        }catch(...){p->restoreVisual();p->status(winrt::to_hresult());}}
        return DefWindowProcW(h,m,w,l);
    }
    NativeContentSurface(Element const&e,Spec s,std::function<bool(Spec&)> position,std::function<void(HRESULT)> result):element(winrt::make_weak(e)),spec(std::move(s)),place(std::move(position)),status(std::move(result)){}
public:
    static std::shared_ptr<NativeContentSurface> create(Element const&e,Options options,Spec spec,std::function<bool(Spec&)> place,std::function<void(HRESULT)> status){
        auto self=std::shared_ptr<NativeContentSurface>(new NativeContentSurface(e,spec,std::move(place),std::move(status)));
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
        if(thread!=GetCurrentThreadId()||pid!=GetCurrentProcessId())throw winrt::hresult_error(E_ACCESSDENIED);
        if(nativeEvents){windowWatches.erase(nativeEvents);UnhookWinEvent(nativeEvents);}
        nativeWindow=target;nativeEvents=SetWinEventHook(EVENT_OBJECT_SHOW,EVENT_OBJECT_LOCATIONCHANGE,nullptr,nativeChanged,pid,thread,WINEVENT_OUTOFCONTEXT);
        if(!nativeEvents)winrt::throw_last_error();windowWatches[nativeEvents]=shared_from_this();
    }
    unsigned hostProcessId()const{return client?client->hostProcessId():0;}
    uint64_t publishedContentRevision()const{return contentRevision;}
    void stop()noexcept{if(stopped)return;stopped=true;captureCancelled=true;contentReady=false;restoreVisual();if(nativeEvents){windowWatches.erase(nativeEvents);UnhookWinEvent(nativeEvents);nativeEvents=nullptr;}try{if(operation)operation.Cancel();if(auto e=element.get()){e.LayoutUpdated(layout);e.Unloaded(unloaded);}}catch(...){}client.reset();if(window){KillTimer(window,1);KillTimer(window,2);DestroyWindow(window);window=nullptr;}}
    ~NativeContentSurface(){stop();}
};
}
