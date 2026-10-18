// Original standalone material engine. Uses Windows XAML diagnostics, not the
// Windhawk runtime, SDK, settings registry, theme parser, or bundled mod DLLs.
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#include <windows.h>
#include <objbase.h>
#include <ocidl.h>
#include <roapi.h>
#include <winstring.h>
#include <xamlom.h>
#include <weakreference.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <atomic>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <set>
#include <memory>
#include "glass_protocol.h"
#include "composition_glass.h"
#include "dock_animation.h"
#include "xaml_property_watch.h"
#include "capsule_clip.h"
#include "capsule_layout.h"
#include "lgengine_native_inspection.h"
#include "native_capture_lane.h"
#include <deque>
#include "xaml_subscription.h"
#include "windhawk_materials.h"
namespace {
template<class T> struct Ptr {
    T*p=nullptr;Ptr()=default;explicit Ptr(T*v):p(v){};~Ptr(){if(p)p->Release();}
    Ptr(const Ptr&o):p(o.p){if(p)p->AddRef();}Ptr(Ptr&&o)noexcept:p(o.p){o.p=nullptr;}
    Ptr&operator=(Ptr o){std::swap(p,o.p);return *this;}T*operator->()const{return p;}explicit operator bool()const{return p!=nullptr;}
    T**put(){if(p){p->Release();p=nullptr;}return &p;}
};
struct XamlReader: IInspectable {virtual HRESULT STDMETHODCALLTYPE Load(HSTRING,IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE LoadValidated(HSTRING,IInspectable**)=0;};
constexpr GUID ReaderIds[]={{0x9891c6bd,0x534f,0x4955,{0xb8,0x5a,0x8a,0x8d,0xc0,0xdc,0xa6,0x02}},{0x82a4cd9e,0x435e,0x5aeb,{0x8c,0x4f,0x30,0x0c,0xec,0xe4,0x5c,0xae}}};
// Modern WinUI exposes cache release; system XAML on some Windows builds does
// not. Limit observations on that compatibility path instead of refusing all
// system materials merely because this optional interface is missing.
constexpr GUID ReleaseId={0x735941a2,0x3ee3,0x495a,{0x8d,0xa9,0x97,0x26,0x27,0x00,0x30,0x75}};
struct CacheRelease:IUnknown{virtual HRESULT STDMETHODCALLTYPE Unregister(InstanceHandle)=0;};
HMODULE selfModule{};HANDLE mapping{};glass::Config*shared{};HANDLE ownerHandle{},ownerWait{};DWORD ownerPid{};SRWLOCK configLock=SRWLOCK_INIT;
HANDLE statusMapping{};glass::Status*sharedStatus{};
void trace(const char*stage,HRESULT hr){static volatile LONG errors=0;if(SUCCEEDED(hr)||InterlockedIncrement(&errors)>32)return;wchar_t path[32768]{};GetModuleFileNameW(selfModule,path,32768);std::wstring file=path;file.resize(file.find_last_of(L"\\")+1);file+=L"glass-engine-"+std::to_wstring(GetCurrentProcessId())+L".log";HANDLE h=CreateFileW(file.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h!=INVALID_HANDLE_VALUE){char text[256]{};int n=snprintf(text,sizeof(text),"%s: 0x%08lx\r\n",stage,(unsigned long)hr);DWORD written=0;WriteFile(h,text,n,&written,nullptr);CloseHandle(h);}}
std::atomic<bool> nativeInspectionOnly{false};
UINT refreshMsg{};std::atomic<bool> injecting[2]{};
// Name of the process hosting this engine copy, so logs show which shell host matched a surface.
std::wstring hostExe(){wchar_t p[MAX_PATH]{};GetModuleFileNameW(nullptr,p,MAX_PATH);std::wstring s=p;return s.substr(s.find_last_of(L'\\')+1);}
// Diagnostics: one line per distinct material decision, written once. Contains only surface ids, material path and numeric parameters.
void note(const std::wstring&line){static std::set<size_t> seen,probeSeen;static SRWLOCK noteLock=SRWLOCK_INIT;AcquireSRWLockExclusive(&noteLock);bool inspection=line.rfind(L"native-probe",0)==0;auto&budget=inspection?probeSeen:seen;bool fresh=budget.size()<(inspection?64u:512u)&&budget.insert(std::hash<std::wstring>{}(line)).second;ReleaseSRWLockExclusive(&noteLock);if(!fresh)return;wchar_t path[32768]{};GetModuleFileNameW(selfModule,path,32768);std::wstring file=path;file.resize(file.find_last_of(L"\\")+1);file+=L"glass-engine-"+std::to_wstring(GetCurrentProcessId())+L".log";HANDLE h=CreateFileW(file.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h!=INVALID_HANDLE_VALUE){std::string text(line.begin(),line.end());text+="\r\n";DWORD written=0;WriteFile(h,text.data(),(DWORD)text.size(),&written,nullptr);CloseHandle(h);}}
const std::wstring&runtimePath(){static std::wstring path=[](){wchar_t value[32768]{};GetModuleFileNameW(selfModule,value,32768);return std::wstring(value);}();return path;}
const std::wstring&threadClass(){static auto name=glass::threadClassName(runtimePath());return name;}
class Session;SRWLOCK sessionsLock=SRWLOCK_INIT;std::vector<Session*> sessions;
// Ok: consistent config. Transient: the controller is mid-publish or the read tore; keep current surfaces and retry. Invalid: no usable config.
using Read=glass::ReadState;
Read snapshot(glass::Config&c){
    AcquireSRWLockShared(&configLock);Read result=Read::Invalid;
    if(shared)result=glass::readSnapshot(shared,c);
    // The controller exited: all effects must be restored regardless of what the mapping still holds.
    if((ownerHandle&&WaitForSingleObject(ownerHandle,0)==WAIT_OBJECT_0)||(!ownerHandle&&shared&&!glass::alive(shared))){c.features=0;result=Read::Ok;}
    ReleaseSRWLockShared(&configLock);return result;
}
void refreshWindows(){HWND h=nullptr;while((h=FindWindowExW(HWND_MESSAGE,h,threadClass().c_str(),nullptr))){DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(pid==GetCurrentProcessId())PostMessageW(h,refreshMsg,0,0);}}
void CALLBACK ownerEnded(void*,BOOLEAN){refreshWindows();}
HWND ownerWindow(){auto h=FindWindowW(nativeInspectionOnly?L"LiquidGlass.Control.NativeProbe":L"LiquidGlass.Control",nullptr);DWORD pid=0;if(h)GetWindowThreadProcessId(h,&pid);return pid==ownerPid?h:nullptr;}
void report(HRESULT hr,DWORD feature=0){note(L"report feature="+std::to_wstring(feature)+L" hr="+std::to_wstring((long)hr)+L" exe="+hostExe());AcquireSRWLockShared(&configLock);if(sharedStatus&&feature){if(FAILED(hr)){InterlockedOr(&sharedStatus->failed,feature);InterlockedExchange(&sharedStatus->error,hr);InterlockedAnd(&sharedStatus->success,~feature);}else{InterlockedAnd(&sharedStatus->failed,~feature);InterlockedOr(&sharedStatus->success,feature);if(hr==S_FALSE)InterlockedOr(&sharedStatus->fallback,feature);else InterlockedAnd(&sharedStatus->fallback,~feature);}}ReleaseSRWLockShared(&configLock);auto h=ownerWindow();if(h)PostMessageW(h,RegisterWindowMessageW(glass::StatusMessage),feature,(LPARAM)hr);}
// Caller holds configLock exclusively. Releases every owner-scoped resource; safe on a partially connected state.
void releaseOwnerLocked(){
    if(ownerWait){UnregisterWaitEx(ownerWait,INVALID_HANDLE_VALUE);ownerWait=nullptr;}
    if(ownerHandle){CloseHandle(ownerHandle);ownerHandle=nullptr;}
    if(shared){UnmapViewOfFile(shared);shared=nullptr;}
    if(mapping){CloseHandle(mapping);mapping=nullptr;}
    if(sharedStatus){UnmapViewOfFile(sharedStatus);sharedStatus=nullptr;}
    if(statusMapping){CloseHandle(statusMapping);statusMapping=nullptr;}
    ownerPid=0;nativeInspectionOnly=false;
}
bool connect(DWORD pid){
    AcquireSRWLockExclusive(&configLock);
    // A live owner keeps its configuration; an ended owner must be replaced.
    if(shared&&((ownerHandle&&WaitForSingleObject(ownerHandle,0)==WAIT_TIMEOUT)||(!ownerHandle&&glass::alive(shared)))){bool same=ownerPid==pid;ReleaseSRWLockExclusive(&configLock);return same;}
    releaseOwnerLocked();
    // Acquire every resource first; publish the connection only after all succeed, so a failure leaves no half-connected state and a later attempt can retry.
    auto name=glass::mappingName(pid);HANDLE m=OpenFileMappingW(FILE_MAP_READ,FALSE,(name+L".Probe").c_str());bool inspection=m!=nullptr;if(!m)m=OpenFileMappingW(FILE_MAP_READ,FALSE,name.c_str());
    if(!m){ReleaseSRWLockExclusive(&configLock);return false;}
    auto view=(glass::Config*)MapViewOfFile(m,FILE_MAP_READ,0,0,sizeof(glass::Config));
    if(!view||view->magic!=glass::Magic||view->version!=glass::Version||view->owner!=pid){if(view)UnmapViewOfFile(view);CloseHandle(m);ReleaseSRWLockExclusive(&configLock);return false;}
    HANDLE owner=OpenProcess(SYNCHRONIZE,FALSE,pid);HANDLE wait=nullptr;
    bool watched=owner&&RegisterWaitForSingleObject(&wait,owner,ownerEnded,nullptr,INFINITE,WT_EXECUTEONLYONCE);
    // Restricted hosts use the read-only heartbeat; no process ACL is changed.
    if(!watched){if(owner)CloseHandle(owner);owner=nullptr;DWORD app=0,bytes=0;HANDLE token{};if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)){GetTokenInformation(token,TokenIsAppContainer,&app,sizeof(app),&bytes);CloseHandle(token);}if(!app||!glass::alive(view)){UnmapViewOfFile(view);CloseHandle(m);ReleaseSRWLockExclusive(&configLock);return false;}}
    mapping=m;shared=view;ownerHandle=owner;ownerWait=wait;ownerPid=pid;nativeInspectionOnly=inspection;
    statusMapping=OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,glass::statusName(pid).c_str());if(statusMapping){sharedStatus=(glass::Status*)MapViewOfFile(statusMapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(glass::Status));if(sharedStatus&&(sharedStatus->magic!=glass::Magic||sharedStatus->version!=glass::Version||sharedStatus->owner!=pid)){UnmapViewOfFile(sharedStatus);sharedStatus=nullptr;}}
    refreshMsg=RegisterWindowMessageW(glass::RefreshMessage);
    // Diagnostics owns COM objects whose callbacks can outlive the hook.
    // Pin this small DLL until the shell process exits; all element references
    // and overrides are released on Off / controller exit.
    HMODULE pinned{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&connect),&pinned);
    ReleaseSRWLockExclusive(&configLock);return true;
}
struct Saved {unsigned index{};bool local=false;Ptr<IInspectable> value;winrt::Windows::UI::Xaml::DependencyProperty direct{nullptr};};
struct Node {bool nativeProbed=false;int notificationRoot=-1;std::vector<const wchar_t*> imported;Ptr<IWeakReference> weak;glass::Surface surface=glass::NoSurface;std::map<std::wstring,Saved> saved;std::map<std::wstring,std::function<void()>> watchers;std::wstring capsuleGeometry;std::shared_ptr<liquid::CapsuleLayout> capsuleLayout;DWORD appliedSignature=0;bool composition=false,fallback=false;HRESULT lastReport=S_FALSE;bool reported=false;HRESULT lastCapsule=E_NOINTERFACE;bool capsuleReported=false,capsuleDirty=true;};
class Session final:public IObjectWithSite,public IVisualTreeServiceCallback2 {
    std::atomic<ULONG> refs{1};Ptr<IUnknown> site;Ptr<IXamlDiagnostics> diag;Ptr<IVisualTreeService2> service;Ptr<CacheRelease> cache;
    struct Thread {Session*session{};HWND window{};std::map<InstanceHandle,Node> nodes;std::set<InstanceHandle> release;ULONGLONG lastMutation=0,releaseStarted=0;std::deque<std::pair<winrt::weak_ref<winrt::Windows::UI::Xaml::FrameworkElement>,int>> nativeProbes;lghost::NativeCaptureLane nativeLane;std::shared_ptr<lghost::NativeInspectionControl> nativeControl;uint64_t nativeToken=0;bool updating=false,updateQueued=false;bool taskbarPanel=false,unifiedCapsule=false;InstanceHandle notificationRoots[2]{};unsigned retries{};LONG observedSequence=-1;DWORD observedFeatures=0;bool heartbeatTimer=false;liquid::DockAnimation dock;~Thread(){if(nativeControl)nativeControl->cancel();for(auto&n:nodes)for(auto&w:n.second.watchers)w.second();}};
    std::map<DWORD,Thread*> threads;SRWLOCK lock=SRWLOCK_INIT;bool modern=false;int host=0;std::atomic<HRESULT> lastError{S_OK};std::atomic<unsigned> writes{0};
    std::atomic<bool> subscribed{false},subscriptionWorker{false};
    std::atomic<unsigned> compatibilityReports{0};
    std::atomic<LONG> notification{S_FALSE};
    void notify(HRESULT hr){if(notification.exchange(hr)!=hr)report(hr);}
    void unregister(InstanceHandle h){if(cache&&h)cache->Unregister(h);}
    bool wantsSubscription(){if(!cache&&compatibilityReports.load()>=8192)return false;glass::Config c;auto state=snapshot(c);if(state==Read::Transient)return subscribed.load();if(state!=Read::Ok)return false;DWORD mask=host==0?(glass::Taskbar|glass::Volume|glass::Brightness|glass::Dock|glass::Explorer|glass::Capsule|glass::Keyboard):host==1?glass::Start:host==2?(glass::Notifications|glass::Keyboard):(glass::Explorer|glass::Keyboard);return (c.features&mask)!=0;}
    static LRESULT CALLBACK windowProc(HWND h,UINT msg,WPARAM w,LPARAM l){
        auto t=(Thread*)GetWindowLongPtrW(h,GWLP_USERDATA);
        if(msg==WM_NCCREATE){t=(Thread*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)t);}
        if(t){if(msg==WM_TIMER&&w==9){if(t->session->subscriptionWorker)return 0;auto now=GetTickCount64();if(t->release.size()<512&&now-t->lastMutation<200&&now-t->releaseStarted<1000)return 0;unsigned budget=256;while(!t->release.empty()&&budget--){auto i=t->release.begin();auto value=*i;t->release.erase(i);t->session->unregister(value);}if(t->release.empty())KillTimer(h,9);return 0;}
            if(msg==WM_APP+4){SetTimer(h,8,30,nullptr);return 0;}if(msg==WM_TIMER&&w==8){if(t->session->subscriptionWorker)return 0;KillTimer(h,8);t->updateQueued=false;t->session->update(*t);return 0;}
            if(msg==refreshMsg){if(t->session->subscriptionWorker)SetTimer(h,10,100,nullptr);else t->session->update(*t);return 0;}
            if(msg==WM_TIMER&&w==10){if(!t->session->subscriptionWorker){KillTimer(h,10);t->session->update(*t);}return 0;}
            if(msg==WM_TIMER&&w==11){KillTimer(h,11);t->session->update(*t);return 0;}
            if(msg==WM_TIMER&&w==20){if(!t->session->subscriptionWorker){glass::Config c;if(snapshot(c)==Read::Ok&&(c.sequence!=t->observedSequence||c.features!=t->observedFeatures)){t->observedSequence=c.sequence;t->observedFeatures=c.features;t->session->update(*t);}for(auto root:t->notificationRoots){auto node=t->nodes.find(root);if(node!=t->nodes.end())t->session->recordRoot(node->second);}}return 0;}
            if(msg==WM_TIMER&&w==21){if(t->nativeLane.expire(GetTickCount64())){KillTimer(h,21);t->nativeProbes.clear();if(t->nativeControl)t->nativeControl->cancel();note(L"native-probe timed out; awaiting cancellation acknowledgement");}return 0;}
            if(msg==WM_APP+7){if(!t->nativeControl||uint64_t(w)!=t->nativeToken)return 0;KillTimer(h,21);t->nativeLane.complete(t->nativeToken);t->nativeControl.reset();if(!t->nativeProbes.empty())PostMessageW(h,WM_APP+6,0,0);return 0;}
            if(msg==WM_APP+6){glass::Config config;if(!t->session->nativeProbeEnabled()||snapshot(config)!=Read::Ok||!config.features){t->nativeProbes.clear();KillTimer(h,21);t->nativeLane.cancel();if(t->nativeControl)t->nativeControl->cancel();return 0;}if(t->nativeControl||t->nativeProbes.empty())return 0;auto request=t->nativeProbes.front();t->nativeProbes.pop_front();t->nativeToken=t->nativeLane.begin(GetTickCount64());auto token=t->nativeToken;t->nativeControl=std::make_shared<lghost::NativeInspectionControl>();if(!SetTimer(h,21,1500,nullptr)){t->nativeLane.cancel();t->nativeControl.reset();t->nativeProbes.clear();return 0;}
                if(auto e=request.first.get()){int surface=request.second;lghost::inspectNativeContent(e,[h,surface,token](HRESULT hr,lghost::NativeContentInfo info){note(L"native-probe surface="+std::to_wstring(surface)+L" hr="+std::to_wstring(long(hr))+L" stage="+std::wstring(info.stage)+L" pixelWidth="+std::to_wstring(info.pixelWidth)+L" pixelHeight="+std::to_wstring(info.pixelHeight)+L" bytes="+std::to_wstring(info.bytes)+L" geometry="+std::to_wstring(info.geometry)+L" width="+std::to_wstring(info.width)+L" height="+std::to_wstring(info.height)+L" rootWidth="+std::to_wstring(info.rootWidth)+L" rootHeight="+std::to_wstring(info.rootHeight)+L" ink="+std::to_wstring(info.ink)+L" opaque="+std::to_wstring(info.opaque)+L" unchanged="+std::to_wstring(info.unchanged));PostMessageW(h,WM_APP+7,WPARAM(token),0);},t->nativeControl);}else PostMessageW(h,WM_APP+7,WPARAM(token),0);return 0;}
            if(msg==WM_APP+2)return (LRESULT)t->session->writes.load();
            if(msg==WM_APP+3)return (LRESULT)t->session->lastError.load();
            if(msg==WM_NCDESTROY){auto s=t->session;AcquireSRWLockExclusive(&s->lock);s->threads.erase(GetCurrentThreadId());ReleaseSRWLockExclusive(&s->lock);SetWindowLongPtrW(h,GWLP_USERDATA,0);delete t;}
        }return DefWindowProcW(h,msg,w,l);
    }
    Thread&thread(){DWORD id=GetCurrentThreadId();AcquireSRWLockExclusive(&lock);auto&ptr=threads[id];
        if(!ptr){ptr=new Thread;ptr->session=this;WNDCLASSW c{};c.lpfnWndProc=windowProc;c.hInstance=selfModule;c.lpszClassName=threadClass().c_str();RegisterClassW(&c);ptr->window=CreateWindowW(c.lpszClassName,L"XAML material session",0,0,0,0,0,HWND_MESSAGE,nullptr,selfModule,ptr);}
        if(ptr->window&&!ownerHandle&&!ptr->heartbeatTimer)ptr->heartbeatTimer=SetTimer(ptr->window,20,1000,nullptr)!=0;
        ReleaseSRWLockExclusive(&lock);return *ptr;
    }
    void recordRoot(Node&node){if(node.notificationRoot<0)return;glass::RootStatus state;try{Ptr<IInspectable> value;if(node.weak)node.weak->Resolve(__uuidof(IInspectable),value.put());if(value){winrt::Windows::Foundation::IInspectable object{nullptr};winrt::copy_from_abi(object,value.p);auto element=object.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();if(element){state.width=(LONG)element.ActualWidth();state.height=(LONG)element.ActualHeight();auto root=element.XamlRoot();state.visible=root&&root.IsHostVisible()&&element.Visibility()==winrt::Windows::UI::Xaml::Visibility::Visible;auto grid=element.try_as<winrt::Windows::UI::Xaml::Controls::Grid>();if(grid){auto brush=grid.Background();state.brush=!brush?0:brush.try_as<winrt::Windows::UI::Xaml::Media::AcrylicBrush>()?1:brush.try_as<winrt::Windows::UI::Xaml::Media::XamlCompositionBrushBase>()?2:brush.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>()?3:4;}}}}catch(...){state.error=winrt::to_hresult();}AcquireSRWLockShared(&configLock);if(sharedStatus){auto&previous=sharedStatus->roots[node.notificationRoot];state.brushWhenVisible=state.visible?state.brush:previous.brushWhenVisible;state.visibleSamples=previous.visibleSamples+(state.visible?1:0);previous=state;}ReleaseSRWLockShared(&configLock);}
    Ptr<IInspectable> from(InstanceHandle h){Ptr<IInspectable> p;diag->GetIInspectableFromHandle(h,p.put());return p;}
    static void scheduleUpdate(Thread&t){if(!t.updateQueued){t.updateQueued=PostMessageW(t.window,WM_APP+4,0,0)!=FALSE;}}
    void releaseLater(Thread&t,InstanceHandle h){if(cache&&h){bool first=t.release.empty();t.release.insert(h);t.lastMutation=GetTickCount64();if(first){t.releaseStarted=t.lastMutation;SetTimer(t.window,9,250,nullptr);}}}
    Ptr<IInspectable> brush(const glass::Config&c,bool transparent,InstanceHandle handle,DWORD feature,bool&custom,bool&fallback){custom=false;fallback=false;
        if(!transparent)try{auto target=from(handle);if(target){winrt::Windows::Foundation::IInspectable obj{nullptr};winrt::copy_from_abi(obj,target.p);Ptr<IInspectable> result;if(modern){auto element=obj.try_as<winrt::Microsoft::UI::Xaml::UIElement>();if(element){auto value=winrt::make<liquid::ModernBrush>(element,c,[feature](HRESULT hr){trace("composition material",hr);report(hr,feature);});winrt::get_unknown(value)->QueryInterface(__uuidof(IInspectable),(void**)result.put());}}else{auto element=obj.try_as<winrt::Windows::UI::Xaml::UIElement>();if(element){auto value=winrt::make<liquid::SystemBrush>(element,c,[feature](HRESULT hr){trace("composition material",hr);report(hr,feature);},feature==glass::Keyboard||c.tint==3);winrt::get_unknown(value)->QueryInterface(__uuidof(IInspectable),(void**)result.put());}}if(result){custom=true;return result;}}}catch(...){fallback=true;trace("composition brush fallback",winrt::to_hresult());}
        Ptr<XamlReader> reader;const wchar_t*cls=modern?L"Microsoft.UI.Xaml.Markup.XamlReader":L"Windows.UI.Xaml.Markup.XamlReader";
        HSTRING className{};WindowsCreateString(cls,(UINT32)wcslen(cls),&className);HRESULT hr=RoGetActivationFactory(className,ReaderIds[modern?1:0],reinterpret_cast<void**>(reader.put()));
        if(FAILED(hr)&&modern){auto mod=GetModuleHandleW(L"Microsoft.UI.Xaml.dll");auto fn=mod?reinterpret_cast<HRESULT(WINAPI*)(HSTRING,IActivationFactory**)>(GetProcAddress(mod,"DllGetActivationFactory")):nullptr;Ptr<IActivationFactory> factory;if(fn&&SUCCEEDED(fn(className,factory.put())))hr=factory->QueryInterface(ReaderIds[1],reinterpret_cast<void**>(reader.put()));}
        WindowsDeleteString(className);if(FAILED(hr)||!reader){lastError=hr;notify(hr);return {};}
        bool dark=c.tint==2;if(c.tint==0){DWORD light=1,size=4;RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);dark=!light;}
        auto color=dark?L"#FF18202C":L"#FFF4F7FC";std::wstring xml;
        if(transparent)xml=L"<SolidColorBrush xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" Color=\"Transparent\"/>";
        else xml=L"<AcrylicBrush xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "+std::wstring(modern?L"":L"BackgroundSource=\"HostBackdrop\" ")+L"TintColor=\""+std::wstring(color)+L"\" TintOpacity=\""+std::to_wstring(c.opacity/100.0)+L"\" FallbackColor=\""+color+L"\" AlwaysUseFallback=\""+(c.economy?L"True":L"False")+L"\"/>";
        HSTRING source{};WindowsCreateString(xml.c_str(),(UINT32)xml.size(),&source);Ptr<IInspectable> result;hr=reader->Load(source,result.put());WindowsDeleteString(source);if(FAILED(hr)){lastError=hr;notify(hr);}return result;
    }
    bool save(Node&n,InstanceHandle h,const std::wstring&prop,unsigned&index){
        auto existing=n.saved.find(prop);if(existing!=n.saved.end()){index=existing->second.index;return true;}
        unsigned ns=0,nv=0;PropertyChainSource*s=nullptr;PropertyChainValue*v=nullptr;HRESULT hr=service->GetPropertyValuesChain(h,&ns,&s,&nv,&v);if(FAILED(hr)){trace("snapshot property",hr);return false;}
        bool local=false,binding=false,found=false;
        for(unsigned i=0;i<nv;++i)if(v[i].PropertyName&&prop==v[i].PropertyName&&!v[i].Overridden){index=v[i].Index;found=true;local=v[i].PropertyChainIndex<ns&&s[v[i].PropertyChainIndex].Source==BaseValueSourceLocal;binding=(v[i].MetadataBits&IsValueBindingExpression)!=0;break;}
        for(unsigned i=0;i<ns;++i){SysFreeString(s[i].TargetType);SysFreeString(s[i].Name);SysFreeString(s[i].SrcInfo.FileName);SysFreeString(s[i].SrcInfo.Hash);}CoTaskMemFree(s);
        for(unsigned i=0;i<nv;++i){SysFreeString(v[i].Type);SysFreeString(v[i].DeclaringType);SysFreeString(v[i].ValueType);SysFreeString(v[i].ItemType);SysFreeString(v[i].Value);SysFreeString(v[i].PropertyName);}CoTaskMemFree(v);
        if(!found){trace("property missing from chain",(HRESULT)nv);return false;}if(binding){trace("binding skipped",S_FALSE);return false;}Saved old;old.index=index;old.local=local;
        if(local){InstanceHandle value=0;hr=service->GetProperty(h,index,&value);if(FAILED(hr))return false;if(value){old.value=from(value);unregister(value);if(!old.value)return false;}}
        n.saved.emplace(prop,std::move(old));return true;
    }
    HRESULT setNotificationBackground(Node&n,InstanceHandle h,IInspectable*value){try{
        using namespace winrt::Windows::UI::Xaml;auto p=from(h);if(!p)return E_FAIL;winrt::Windows::Foundation::IInspectable object{nullptr};winrt::copy_from_abi(object,p.p);auto target=object.try_as<DependencyObject>();if(!target)return E_NOTIMPL;DependencyProperty property{nullptr};
        if(object.try_as<Controls::Panel>())property=Controls::Panel::BackgroundProperty();else if(object.try_as<Controls::Border>())property=Controls::Border::BackgroundProperty();else if(object.try_as<Controls::Control>())property=Controls::Control::BackgroundProperty();if(!property)return E_NOTIMPL;
        if(!n.saved.count(L"Background")){auto local=target.ReadLocalValue(property);if(local&&std::wstring_view(winrt::get_class_name(local)).find(L"BindingExpression")!=std::wstring_view::npos)return E_INVALIDARG;Saved old;old.direct=property;old.local=local!=DependencyProperty::UnsetValue();if(old.local&&local)winrt::get_unknown(local)->QueryInterface(__uuidof(IInspectable),(void**)old.value.put());n.saved.emplace(L"Background",std::move(old));}
        winrt::Windows::Foundation::IInspectable brush{nullptr};if(value)winrt::copy_from_abi(brush,value);target.SetValue(property,brush);auto actual=target.GetValue(property);if(actual!=brush)return E_FAIL;return S_OK;
    }catch(...){return winrt::to_hresult();}}
    bool set(Node&n,InstanceHandle h,const std::wstring&prop,IInspectable*value,bool required=true){
        HRESULT hr=(host==2||n.surface==glass::KeyboardPlate)&&prop==L"Background"?setNotificationBackground(n,h,value):E_NOTIMPL;
        if(hr==E_NOTIMPL){unsigned index=0;if(!save(n,h,prop,index)){if(required)report(E_INVALIDARG,glass::feature(n.surface));return false;}
            InstanceHandle v=0;if(value&&FAILED(diag->RegisterInstance(value,&v)))return false;hr=service->SetProperty(h,v,index);if(v)unregister(v);}
        trace("set property",hr);if(FAILED(hr)){lastError=hr;if(required)report(hr,glass::feature(n.surface));}else{++writes;// S_OK: custom composition blur applied. S_FALSE: system acrylic, whose blur strength cannot be set by this engine.
if(required&&!(n.surface==glass::TaskbarFill&&thread().taskbarPanel)&&glass::feature(n.surface)&&n.surface!=glass::StartOverlay&&n.surface!=glass::TaskbarOverlay&&n.surface!=glass::StartClear&&n.surface!=glass::NotificationClear&&n.surface!=glass::ExplorerClear){n.lastReport=n.composition?S_OK:S_FALSE;n.reported=true;report(n.lastReport,glass::feature(n.surface));}}return SUCCEEDED(hr);
    }
    void restore(Node&n,InstanceHandle h){for(auto i=n.saved.begin();i!=n.saved.end();){auto&v=i->second;HRESULT hr=S_OK;InstanceHandle old=0;
            if(v.direct){try{Ptr<IInspectable> p;if(n.weak)n.weak->Resolve(__uuidof(IInspectable),p.put());if(p){winrt::Windows::Foundation::IInspectable object{nullptr},value{nullptr};winrt::copy_from_abi(object,p.p);if(v.value)winrt::copy_from_abi(value,v.value.p);auto target=object.as<winrt::Windows::UI::Xaml::DependencyObject>();if(v.local)target.SetValue(v.direct,value);else target.ClearValue(v.direct);}}catch(...){hr=winrt::to_hresult();}}
            else if(v.local){if(v.value)hr=diag->RegisterInstance(v.value.p,&old);if(SUCCEEDED(hr))hr=service->SetProperty(h,old,v.index);if(old)unregister(old);}else hr=service->ClearProperty(h,v.index);
            trace("restore property",hr);if(SUCCEEDED(hr))i=n.saved.erase(i);else{lastError=hr;report(hr,glass::feature(n.surface));++i;}}if(n.saved.empty()){for(auto&w:n.watchers)w.second();n.watchers.clear();n.capsuleGeometry.clear();n.appliedSignature=0;n.reported=false;n.capsuleReported=false;}
    }
    Ptr<IInspectable> markup(const std::wstring&xml){
        Ptr<XamlReader> reader;auto cls=modern?L"Microsoft.UI.Xaml.Markup.XamlReader":L"Windows.UI.Xaml.Markup.XamlReader";HSTRING name{};WindowsCreateString(cls,(UINT32)wcslen(cls),&name);
        HRESULT hr=RoGetActivationFactory(name,ReaderIds[modern?1:0],(void**)reader.put());
        if(FAILED(hr)&&modern){auto module=GetModuleHandleW(L"Microsoft.UI.Xaml.dll");auto fn=module?(HRESULT(WINAPI*)(HSTRING,IActivationFactory**))GetProcAddress(module,"DllGetActivationFactory"):nullptr;Ptr<IActivationFactory> factory;if(fn&&SUCCEEDED(fn(name,factory.put())))hr=factory->QueryInterface(ReaderIds[1],(void**)reader.put());}
        WindowsDeleteString(name);if(FAILED(hr)||!reader)return {};HSTRING text{};WindowsCreateString(xml.c_str(),(UINT32)xml.size(),&text);Ptr<IInspectable> result;reader->Load(text,result.put());WindowsDeleteString(text);return result;
    }
    bool typed(Node&n,InstanceHandle h,const wchar_t*property,const wchar_t*type,const std::wstring&value){
        auto prefix=modern?L"Microsoft.UI.Xaml.":L"Windows.UI.Xaml.";std::wstring full=wcscmp(type,L"Double")==0?L"Double":std::wstring(prefix)+type;
        BSTR t=SysAllocString(full.c_str()),v=SysAllocString(value.c_str());InstanceHandle instance=0;
        bool ok=false;if(SUCCEEDED(service->CreateInstance(t,v,&instance))){auto p=from(instance);if(p)ok=set(n,h,property,p.p,false);unregister(instance);}SysFreeString(t);SysFreeString(v);return ok;
    }
    void edge(Node&n,InstanceHandle h,const glass::Config&c){
        bool rectangle=n.surface==glass::TaskbarFill;
        auto rim=markup(L"<LinearGradientBrush xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" StartPoint=\"0,0\" EndPoint=\"0,1\"><GradientStop Color=\"#80FFFFFF\" Offset=\"0\"/><GradientStop Color=\"#18303030\" Offset=\"0.5\"/><GradientStop Color=\"#60FFFFFF\" Offset=\"1\"/></LinearGradientBrush>");
        if(rim)set(n,h,rectangle?L"Stroke":L"BorderBrush",rim.p,false);
        typed(n,h,rectangle?L"StrokeThickness":L"BorderThickness",rectangle?L"Double":L"Thickness",L"1");
        if(rectangle){typed(n,h,L"RadiusX",L"Double",std::to_wstring(c.radius));typed(n,h,L"RadiusY",L"Double",std::to_wstring(c.radius));}
    }
    std::vector<const wchar_t*> importedStyles(IInspectable*p){
        std::vector<imported_glass::Element> ancestry;winrt::Windows::Foundation::IInspectable object{nullptr};winrt::copy_from_abi(object,p);
        try{for(unsigned i=0;object&&i<24;++i){std::wstring name;winrt::Windows::Foundation::IInspectable parent{nullptr};
            if(modern){auto e=object.try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>();if(e)name=e.Name();auto d=object.try_as<winrt::Microsoft::UI::Xaml::DependencyObject>();if(d)parent=winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetParent(d);}
            else{auto e=object.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();if(e)name=e.Name();auto d=object.try_as<winrt::Windows::UI::Xaml::DependencyObject>();if(d)parent=winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(d);}
            ancestry.push_back({winrt::get_class_name(object).c_str(),name});object=parent;}
        }catch(...){trace("theme ancestry",winrt::to_hresult());}
        return imported_glass::styles(imported_glass::theme(host),ancestry);
    }
    bool applyImported(Node&n,InstanceHandle h,const glass::Config&c){bool applied=false;
        for(auto rule:n.imported){std::wstring property,value;if(!imported_glass::assignment(rule,property,value))continue;value=imported_glass::resolve(imported_glass::theme(host),value);
            if(property==L"Background"||property==L"Fill"||property==L"BorderBrush"||property==L"Stroke"){
                bool background=property==L"Background"||property==L"Fill";Ptr<IInspectable> result;
                if(value.find(L"<WindhawkBlur")==0||(c.tint==3&&background&&value.find(L"AcrylicBrush")!=std::wstring::npos)){bool custom=false,fallback=false;result=brush(c,false,h,background?glass::feature(n.surface):0,custom,fallback);if(background){n.composition=custom;n.fallback=fallback;}}
                else if(value==L"Transparent"||value.empty()){bool custom=false,fallback=false;result=brush(c,true,h,glass::feature(n.surface),custom,fallback);}
                else if(!value.empty()&&value.front()==L'<'){auto at=value.find(L' ');if(at!=value.npos)value.insert(at,L" xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"");result=markup(value);}
                if(result){bool ok=set(n,h,property,result.p,background);applied=ok||applied;}
            }else if(property==L"CornerRadius")typed(n,h,property.c_str(),L"CornerRadius",std::to_wstring(c.radius));
            else if(property==L"BorderThickness")typed(n,h,property.c_str(),L"Thickness",value);
            else if(property==L"StrokeThickness")typed(n,h,property.c_str(),L"Double",value);
        }return applied;
    }
    // Restores only the named properties, so the capsule geometry can be removed without dropping the material.
    void restoreNamed(Node&n,InstanceHandle h,std::initializer_list<const wchar_t*> names){
        n.capsuleGeometry.clear();
        for(auto i=n.saved.begin();i!=n.saved.end();){bool mine=false;for(auto name:names)if(i->first==name)mine=true;if(!mine){++i;continue;}
            auto&v=i->second;HRESULT hr=S_OK;InstanceHandle old=0;
            if(v.local){if(v.value)hr=diag->RegisterInstance(v.value.p,&old);if(SUCCEEDED(hr))hr=service->SetProperty(h,old,v.index);if(old)unregister(old);}else hr=service->ClearProperty(h,v.index);
            if(SUCCEEDED(hr))i=n.saved.erase(i);else{lastError=hr;++i;}}
    }
    // Reversible XAML transforms move app and tray hit targets together; the native taskbar window stays unchanged.
    void applyCapsule(Node&n,InstanceHandle h,const glass::Config&c){
        if(!(c.features&glass::Capsule))return;n.capsuleDirty=false;
        // Find the icon content through the taskbar subtree, rather than measuring
        // the background's siblings (which can be only the system tray).
        HRESULT result=E_NOINTERFACE;
        if(!modern)try{
            auto p=from(h);winrt::Windows::Foundation::IInspectable obj{nullptr};if(p)winrt::copy_from_abi(obj,p.p);
            auto fill=obj.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();
            auto host=fill?winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(fill).try_as<winrt::Windows::UI::Xaml::FrameworkElement>():winrt::Windows::UI::Xaml::FrameworkElement{nullptr};
            if(host&&host.ActualWidth()>0&&host.ActualHeight()>0){
                double width=host.ActualWidth(),height=host.ActualHeight();
                using Tree=winrt::Windows::UI::Xaml::Media::VisualTreeHelper;
                using Element=winrt::Windows::UI::Xaml::FrameworkElement;
                Element root=host;bool taskbar=false;
                for(int depth=0;root&&depth<16;++depth){if(winrt::get_class_name(root)==L"Taskbar.TaskbarFrame"){taskbar=true;break;}root=Tree::GetParent(root).try_as<Element>();}
                if(taskbar&&!n.watchers.count(L"CapsuleLayout")){auto&t=thread();auto token=root.LayoutUpdated([&t,h](auto const&,auto const&){if(t.updating)return;auto i=t.nodes.find(h);if(i==t.nodes.end()||i->second.capsuleDirty)return;i->second.capsuleDirty=true;scheduleUpdate(t);});auto weak=winrt::make_weak(root);n.watchers[L"CapsuleLayout"]=[weak,token]{try{if(auto e=weak.get())e.LayoutUpdated(token);}catch(...){}};}
                double left=width,right=0;bool joined=false;std::vector<std::pair<double,double>> auxiliary;
                Element trayElement{nullptr};std::vector<Element> layoutIcons;
                Element scanRoot=root;if(taskbar)for(unsigned depth=0;depth<8;++depth){auto parent=Tree::GetParent(scanRoot).try_as<Element>();if(!parent)break;scanRoot=parent;}
                std::vector<winrt::Windows::UI::Xaml::DependencyObject> pending;if(taskbar)pending.push_back(scanRoot);
                unsigned visited=0;while(!pending.empty()&&visited++<2048){auto object=pending.back();pending.pop_back();auto child=object.try_as<Element>();
                    if(child&&child.Visibility()!=winrt::Windows::UI::Xaml::Visibility::Visible)continue;
                    auto type=winrt::get_class_name(object);
                    if(child&&child.Name()==L"SystemTrayFrameGrid"&&child.ActualWidth()>0)try{auto origin=child.TransformToVisual(host).TransformPoint({0,0});double a=std::max(0.0,(double)origin.X),b=std::min(width,(double)origin.X+child.ActualWidth());if(b>a)trayElement=child;continue;}catch(...){}
                    bool icon=type==L"Taskbar.TaskListButton"||type==L"Taskbar.TaskListLabeledButton"||type==L"Taskbar.ExperienceToggleButton"||type==L"SearchUx.SearchUI.SearchIconButton"||type==L"SearchUx.SearchUI.SearchBoxButton";
                    if(icon&&child&&child.ActualWidth()>0){auto origin=child.TransformToVisual(host).TransformPoint({0,0});double a=std::max(0.0,(double)origin.X),b=std::min(width,(double)origin.X+child.ActualWidth());
                        if(b>a){layoutIcons.push_back(child);if(type==L"Taskbar.TaskListButton"||type==L"Taskbar.TaskListLabeledButton"){left=std::min(left,a);right=std::max(right,b);}else if(type!=L"Taskbar.ExperienceToggleButton"||b-a<=height*2)auxiliary.emplace_back(a,b);}continue;}
                    int count=Tree::GetChildrenCount(object);for(int i=0;i<count;++i)pending.push_back(Tree::GetChild(object,i));}
                // Only include controls contiguous with the task buttons. A
                // separate Widgets button at the screen edge must not stretch
                // the icon capsule across the whole taskbar.
                if(right>left)for(size_t pass=0;pass<auxiliary.size();++pass){bool changed=false;for(auto span:auxiliary)if(span.first<=right+height&&span.second>=left-height){double a=std::min(left,span.first),b=std::max(right,span.second);changed=changed||a!=left||b!=right;left=a;right=b;}if(!changed)break;}
                std::vector<Element> cluster;for(auto const&e:layoutIcons){auto origin=e.TransformToVisual(host).TransformPoint({0,0});if(origin.X+e.ActualWidth()>=left-.5&&origin.X<=right+.5)cluster.push_back(e);}
                if(right>left)for(auto const&entry:thread().nodes){if(entry.second.surface!=glass::TaskbarRoot)continue;try{Ptr<IInspectable> value;if(entry.second.weak)entry.second.weak->Resolve(__uuidof(IInspectable),value.put());if(!value)continue;winrt::Windows::Foundation::IInspectable object{nullptr};winrt::copy_from_abi(object,value.p);auto tray=object.try_as<Element>();if(!tray||tray.Name()!=L"SystemTrayFrameGrid"||tray.ActualWidth()<=0)continue;auto origin=tray.TransformToVisual(host).TransformPoint({0,0});double a=std::max(0.0,(double)origin.X),b=std::min(width,(double)origin.X+tray.ActualWidth());if(b>a&&!trayElement)trayElement=tray;}catch(...){}}
                if(!n.capsuleLayout)n.capsuleLayout=std::make_shared<liquid::CapsuleLayout>([](auto const&line){note(line);});
                if(!n.watchers.count(L"CapsuleContent")){auto layout=n.capsuleLayout;n.watchers[L"CapsuleContent"]=[layout]{layout->stop();};}
                auto placement=n.capsuleLayout->apply(cluster,trayElement,host);
                if(placement.valid){left=placement.left;right=placement.right;joined=true;}
                else{n.lastCapsule=E_NOINTERFACE;n.capsuleReported=true;report(E_NOINTERFACE,glass::Capsule);return;}
                if(right>left){
                    double a=std::max(0.0,left-8),b=std::max(0.0,width-right-8),vertical=std::min(height/4,std::max(2.0,(height-44.0)/2)),radius=std::min(height-2*vertical,width-a-b)/2;
                    auto geometry=std::to_wstring(a)+L","+std::to_wstring(vertical)+L","+std::to_wstring(b)+L","+std::to_wstring(vertical);
                    if(n.capsuleGeometry==geometry)return;
                    bool ok=typed(n,h,L"Margin",L"Thickness",geometry);
                    ok=typed(n,h,L"RadiusX",L"Double",std::to_wstring(radius))&&ok;ok=typed(n,h,L"RadiusY",L"Double",std::to_wstring(radius))&&ok;
                    if(ok){result=S_OK;n.capsuleGeometry=geometry;auto&t=thread();if(t.unifiedCapsule!=joined){t.unifiedCapsule=joined;scheduleUpdate(t);}}else{n.capsuleLayout->stop();restoreNamed(n,h,{L"Margin",L"RadiusX",L"RadiusY"});}
                }
            }
        }
        catch(...){trace("capsule geometry",winrt::to_hresult());}
        n.lastCapsule=result;n.capsuleReported=true;report(result,glass::Capsule);
    }
    template<class Element,class Tree>bool keyboardPanelTree(Element const&root){
        if(!root)return false;
        using Object=decltype(Tree::GetChild(root,0));std::vector<Object> pending{root};unsigned seen=0;bool header=false,list=false,footer=false,description=false;
        while(!pending.empty()&&seen++<256){auto object=pending.back();pending.pop_back();if(auto e=object.template try_as<Element>()){
            auto name=e.Name();header=header||name==L"HeaderPanel";list=list||name==L"List";footer=footer||name==L"Footer";description=description||name==L"KeyboardDescription";
        }if(header&&list&&footer&&description)return true;int count=Tree::GetChildrenCount(object);for(int i=0;i<count;++i)pending.push_back(Tree::GetChild(object,i));}
        return false;
    }
    bool keyboardPanel(InstanceHandle h){try{auto p=from(h);if(!p)return false;winrt::Windows::Foundation::IInspectable object{nullptr};winrt::copy_from_abi(object,p.p);
        if(modern)return keyboardPanelTree<winrt::Microsoft::UI::Xaml::FrameworkElement,winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper>(object.try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>());
        return keyboardPanelTree<winrt::Windows::UI::Xaml::FrameworkElement,winrt::Windows::UI::Xaml::Media::VisualTreeHelper>(object.try_as<winrt::Windows::UI::Xaml::FrameworkElement>());
    }catch(...){return false;}}
    void clearKeyboardBacking(Node&n,InstanceHandle h){
        if(modern)return;
        try{
            using namespace winrt::Windows::UI::Xaml;
            auto p=from(h);winrt::Windows::Foundation::IInspectable object{nullptr};if(!p)return;winrt::copy_from_abi(object,p.p);
            auto root=object.try_as<FrameworkElement>();using Tree=Media::VisualTreeHelper;
            std::vector<DependencyObject> pending;int count=Tree::GetChildrenCount(root);for(int i=0;i<count;++i)pending.push_back(Tree::GetChild(root,i));
            unsigned visited=0,cleared=0;
            while(!pending.empty()&&visited++<256){auto child=pending.back();pending.pop_back();auto e=child.try_as<FrameworkElement>();
                // Only wrappers containing the entire keyboard panel. Never
                // clear list-item selection, hover or individual control fills.
                if(!e||!keyboardPanelTree<FrameworkElement,Tree>(e))continue;
                DependencyProperty property{nullptr};if(child.try_as<Controls::Panel>())property=Controls::Panel::BackgroundProperty();else if(child.try_as<Controls::Border>())property=Controls::Border::BackgroundProperty();else if(child.try_as<Controls::Control>())property=Controls::Control::BackgroundProperty();
                if(property){InstanceHandle id=0;if(SUCCEEDED(diag->GetHandleFromIInspectable(reinterpret_cast<IInspectable*>(winrt::get_abi(e)),&id))){auto key=L"KeyboardBacking."+std::to_wstring(id);
                    auto local=child.ReadLocalValue(property);bool bound=local&&std::wstring_view(winrt::get_class_name(local)).find(L"BindingExpression")!=std::wstring_view::npos;
                    if(!bound){if(!n.watchers.count(key)){auto weak=winrt::make_weak(child);bool hadLocal=local!=DependencyProperty::UnsetValue();n.watchers[key]=[weak,property,local,hadLocal]{try{if(auto target=weak.get()){if(hadLocal)target.SetValue(property,local);else target.ClearValue(property);}}catch(...){}};}
                        child.SetValue(property,Media::SolidColorBrush(winrt::Windows::UI::Color{0,0,0,0}));++cleared;}
                }}
                int children=Tree::GetChildrenCount(child);for(int i=0;i<children;++i)pending.push_back(Tree::GetChild(child,i));
            }
            note(L"keyboard backing wrappers cleared="+std::to_wstring(cleared));
        }catch(...){trace("keyboard backing",winrt::to_hresult());}
    }
    bool nativeProbeEnabled(){return nativeInspectionOnly.load();}
    void apply(Node&n,InstanceHandle h,const glass::Config&c){
        if(nativeProbeEnabled()){
            if(n.nativeProbed||!(n.surface==glass::TaskbarRoot||n.surface==glass::VolumePlate||n.surface==glass::BrightnessPlate||n.surface==glass::StartPlate||n.surface==glass::NotificationPlate||n.surface==glass::KeyboardPlate))return;
            note(L"native-probe observed surface="+std::to_wstring(int(n.surface))+L" modern="+std::to_wstring(modern));
            if(modern){n.nativeProbed=true;note(L"native-probe modern-unsupported surface="+std::to_wstring(int(n.surface)));return;}
            try{auto p=from(h);if(!p)return;winrt::Windows::Foundation::IInspectable object{nullptr};winrt::copy_from_abi(object,p.p);auto e=object.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();if(!e||!e.IsLoaded()||!e.XamlRoot()||!e.XamlRoot().IsHostVisible()||e.ActualWidth()<1||e.ActualHeight()<1)return;auto parent=e.as<winrt::Windows::UI::Xaml::DependencyObject>();for(unsigned i=0;parent&&i<128;++i){if(auto ui=parent.try_as<winrt::Windows::UI::Xaml::UIElement>();ui&&ui.Visibility()!=winrt::Windows::UI::Xaml::Visibility::Visible)return;parent=winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(parent);}n.nativeProbed=true;auto&t=thread();t.nativeProbes.push_back({winrt::make_weak(e),int(n.surface)});note(L"native-probe queued surface="+std::to_wstring(int(n.surface)));PostMessageW(t.window,WM_APP+6,0,0);}catch(...){trace("native-probe",winrt::to_hresult());}return;
        }

        if(n.surface==glass::DockRoot||n.surface==glass::DockIcon){auto&t=thread();if(!(c.features&glass::Dock)||c.economy){t.dock.stop();return;}auto p=from(h);if(p){if(n.surface==glass::DockRoot){if(!t.dock.attach(p.p))report(E_NOINTERFACE,glass::Dock);}else t.dock.add(p.p);if(t.dock.ready())report(S_OK,glass::Dock);}return;}
        if(!glass::active(n.surface,c.features)){restore(n,h);return;}
        if(n.surface==glass::KeyboardPlate&&!keyboardPanel(h))return;
        DWORD signature=1|(c.opacity<<1)|(c.radius<<8)|(c.tint<<13)|(c.economy?1u<<15:0)|(c.blur<<16)|((c.features&glass::Capsule)?1u<<24:0)|((c.features&glass::Taskbar)?1u<<25:0)|((n.surface==glass::TaskbarRoot&&thread().unifiedCapsule)?1u<<26:0);
        // Unchanged parameters are not written again, but the controller may have cleared its status after a rediscovery; repeat the last result so status matches the live surface.
        if(n.appliedSignature==signature){if(n.surface==glass::TaskbarFill&&n.capsuleDirty&&(c.features&glass::Capsule))applyCapsule(n,h,c);if(n.reported)report(n.lastReport,glass::feature(n.surface));if(n.capsuleReported&&(c.features&glass::Capsule))report(n.lastCapsule,glass::Capsule);return;}
        // Capsule-only: the material is removed, the geometry remains. Otherwise the geometry is reset before the material and capsule are applied again.
        if(n.surface==glass::TaskbarFill&&!(c.features&glass::Taskbar)&&!(c.features&glass::Capsule)){restore(n,h);return;}
        if(n.surface==glass::TaskbarFill)restoreNamed(n,h,{L"Margin",L"RadiusX",L"RadiusY"});
        bool iconPanel=false;if(!modern&&n.surface==glass::TaskbarRoot){auto p=from(h);winrt::Windows::Foundation::IInspectable obj{nullptr};if(p)winrt::copy_from_abi(obj,p.p);auto e=obj.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();iconPanel=e&&e.Name()==L"RootGrid";}
        bool joinedTray=n.surface==glass::TaskbarRoot&&!iconPanel&&thread().unifiedCapsule&&(c.features&glass::Capsule);
        if(n.surface==glass::TaskbarRoot&&(!(c.features&glass::Capsule)||joinedTray)){auto i=n.watchers.find(L"CapsuleClip");if(i!=n.watchers.end()){i->second();n.watchers.erase(i);}}
        if((iconPanel||joinedTray)&&(c.features&glass::Capsule))restoreNamed(n,h,{L"BorderBrush",L"BorderThickness"});
        if(n.surface==glass::KeyboardPlate)clearKeyboardBacking(n,h);
        bool applied=false;if((iconPanel||joinedTray)&&(c.features&glass::Capsule)){auto b=brush(c,true,h,0,n.composition,n.fallback);if(b)applied=set(n,h,L"Background",b.p,false);}
        else if(!n.imported.empty())applied=applyImported(n,h,c);else{auto b=brush(c,(n.surface==glass::TaskbarFill&&thread().taskbarPanel&&!(c.features&glass::Capsule))||n.surface==glass::OsdPlate||n.surface==glass::StartOverlay||n.surface==glass::TaskbarOverlay||n.surface==glass::StartClear||n.surface==glass::NotificationClear||n.surface==glass::ExplorerClear,h,glass::feature(n.surface),n.composition,n.fallback);if(!b)return;
        applied=set(n,h,(n.surface==glass::TaskbarFill||n.surface==glass::TaskbarOverlay)?L"Fill":L"Background",b.p);
        if(applied&&!(iconPanel&&(c.features&glass::Capsule))&&(n.surface==glass::TaskbarRoot||n.surface==glass::TaskbarFill||n.surface==glass::StartPlate||n.surface==glass::NotificationPlate||n.surface==glass::ExplorerPlate||n.surface==glass::KeyboardPlate))edge(n,h,c);
        if(applied&&n.surface==glass::TaskbarFill)applyCapsule(n,h,c);
        // Round only native material plates; never change layout constraints.
        if(n.surface==glass::StartPlate||n.surface==glass::NotificationPlate||n.surface==glass::TaskbarRoot||n.surface==glass::VolumePlate||n.surface==glass::BrightnessPlate||n.surface==glass::KeyboardPlate){
            InstanceHandle r=0;BSTR type=SysAllocString(modern?L"Microsoft.UI.Xaml.CornerRadius":L"Windows.UI.Xaml.CornerRadius");auto text=std::to_wstring(c.radius);BSTR value=SysAllocString(text.c_str());
            if(SUCCEEDED(service->CreateInstance(type,value,&r))){auto p=from(r);if(p)set(n,h,L"CornerRadius",p.p,false);unregister(r);}SysFreeString(type);SysFreeString(value);
        }
        }
        // Native OSD pills use their own radius, independent of shell corners.
        if(applied&&(n.surface==glass::VolumePlate||n.surface==glass::BrightnessPlate||n.surface==glass::OsdPlate)){
            if(!n.watchers.count(L"OsdPillRadius"))try{
                auto p=from(h);winrt::Windows::Foundation::IInspectable object{nullptr};winrt::copy_from_abi(object,p.p);
                auto applyRadius=[this,h](float r){auto&t=thread();auto i=t.nodes.find(h);if(i!=t.nodes.end())typed(i->second,h,L"CornerRadius",L"CornerRadius",std::to_wstring(r));};
                std::function<void()> cleanup;
                if(modern){if(auto e=object.try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>())cleanup=liquid::watchPillRadius(e,applyRadius);}
                else if(auto e=object.try_as<winrt::Windows::UI::Xaml::FrameworkElement>())cleanup=liquid::watchPillRadius(e,applyRadius);
                if(cleanup)n.watchers.emplace(L"OsdPillRadius",std::move(cleanup));
            }catch(...){trace("OSD pill radius",winrt::to_hresult());}
            // Remove the native rectangular rim beneath the rounded plate.
            typed(n,h,L"BorderThickness",L"Thickness",L"0");
            auto clear=markup(L"<SolidColorBrush xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" Color=\"Transparent\"/>");if(clear)set(n,h,L"BorderBrush",clear.p,false);
            if(!modern&&!n.watchers.count(L"OsdCapsuleClip"))try{auto p=from(h);auto cleanup=liquid::clipCapsule(p.p);if(cleanup)n.watchers.emplace(L"OsdCapsuleClip",std::move(cleanup));}catch(...){trace("OSD capsule clip",winrt::to_hresult());}
        }
        // Imported material rules and the built-in brush path both need the
        // tray clip. Keep geometry outside the material-selection branch.
        if(applied&&!modern&&n.surface==glass::TaskbarRoot&&!iconPanel&&!joinedTray&&(c.features&glass::Capsule)&&!n.watchers.count(L"CapsuleClip")){try{auto p=from(h);auto cleanup=liquid::clipCapsule(p.p);if(cleanup)n.watchers.emplace(L"CapsuleClip",std::move(cleanup));}catch(...){trace("tray capsule clip",winrt::to_hresult());}}
        if(applied)note(L"surface="+std::to_wstring((int)n.surface)+L" path="+(!n.imported.empty()?L"imported":n.composition?L"composition":n.fallback?L"acrylic-after-composition-error":L"acrylic")+L" blur="+std::to_wstring(c.blur)+L" opacity="+std::to_wstring(c.opacity)+L" tint="+std::to_wstring(c.tint)+L" radius="+std::to_wstring(c.radius));
        if(applied){n.appliedSignature=signature;for(auto property:{L"Background",L"Fill",L"BorderBrush",L"Stroke"})if(n.saved.count(property)&&!n.watchers.count(property))try{auto p=from(h);auto&t=thread();auto callback=liquid::watch(p.p,modern,property,[&t,h]{if(t.updating)return;auto node=t.nodes.find(h);if(node!=t.nodes.end()){node->second.appliedSignature=0;scheduleUpdate(t);}});if(callback)n.watchers.emplace(property,std::move(callback));}catch(...){trace("watch property",winrt::to_hresult());}}
    }
    void update(Thread&t){if(t.updating)return;glass::Config c;Read state=snapshot(c);
        // A torn read keeps the current surfaces (no flicker while a slider is dragged). Retries are bounded; a controller that stays unreadable is treated as gone and effects are restored.
        if(state==Read::Transient){if(t.retries<4){++t.retries;SetTimer(t.window,11,25,nullptr);return;}state=Read::Invalid;}
        t.retries=0;if(state!=Read::Ok)c.features=0;
        if(!c.features){t.nativeProbes.clear();KillTimer(t.window,21);t.nativeLane.cancel();if(t.nativeControl)t.nativeControl->cancel();}
        t.updating=true;if(!(c.features&glass::Capsule))t.unifiedCapsule=false;if(!(c.features&glass::Dock)||c.economy)t.dock.stop();
        for(auto i=t.nodes.begin();i!=t.nodes.end();){Ptr<IInspectable> p;if(i->second.weak) i->second.weak->Resolve(__uuidof(IInspectable),p.put());if(!p){for(auto&w:i->second.watchers)w.second();i=t.nodes.erase(i);continue;}
            InstanceHandle h=0;if(SUCCEEDED(diag->RegisterInstance(p.p,&h))){apply(i->second,h,c);releaseLater(t,h);}
            // Keep only weak descriptors for inactive surfaces. Another switch
            // can be enabled while this diagnostics subscription is already on;
            // Windows does not enumerate the existing tree a second time then.
            ++i;}
        t.updating=false;ensureSubscription();
    }
public:
    void ensureSubscription(){
        if(!service||wantsSubscription()==subscribed.load()||subscriptionWorker.exchange(true))return;
        AddRef();auto worker=CreateThread(nullptr,0,[](void*ctx)->DWORD{auto s=(Session*)ctx;CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            for(int attempts=0;attempts<4;++attempts){bool wanted=s->wantsSubscription();if(wanted==s->subscribed.load())break;
                HRESULT hr=S_OK;if(wanted){xaml_subscription::Scope scope(s->service.p);hr=scope.status();if(SUCCEEDED(hr))hr=s->service->AdviseVisualTreeChange(s);}else hr=s->service->UnadviseVisualTreeChange(s);trace("subscription result",hr);s->lastError=hr;if(FAILED(hr)){s->notify(hr);break;}s->subscribed=wanted;AcquireSRWLockShared(&configLock);if(sharedStatus){if(wanted)InterlockedOr(&sharedStatus->subscribed,1<<s->host);else InterlockedAnd(&sharedStatus->subscribed,~(1<<s->host));}ReleaseSRWLockShared(&configLock);}
            s->subscriptionWorker=false;s->Release();CoUninitialize();return 0;
        },this,0,nullptr);if(worker)CloseHandle(worker);else{subscriptionWorker=false;Release();}
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void**out)override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IObjectWithSite)*out=static_cast<IObjectWithSite*>(this);else if(id==__uuidof(IVisualTreeServiceCallback)||id==__uuidof(IVisualTreeServiceCallback2))*out=static_cast<IVisualTreeServiceCallback2*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetSite(REFIID id,void**out)override{return site?site->QueryInterface(id,out):E_FAIL;}
    HRESULT STDMETHODCALLTYPE SetSite(IUnknown*p)override{
        if(!p)return S_OK;site=Ptr<IUnknown>(p);p->AddRef();HRESULT hr=p->QueryInterface(__uuidof(IXamlDiagnostics),reinterpret_cast<void**>(diag.put()));if(FAILED(hr)){trace("IXamlDiagnostics",hr);return hr;}
        hr=diag->QueryInterface(__uuidof(IVisualTreeService3),reinterpret_cast<void**>(service.put()));trace("visual tree service 3",hr);if(FAILED(hr))return hr;
        hr=diag->QueryInterface(ReleaseId,reinterpret_cast<void**>(cache.put()));trace("cache release (optional)",hr);
        BSTR init=nullptr;if(SUCCEEDED(diag->GetInitializationData(&init))&&init){std::wstring v=init;modern=v.find(L"modern")!=std::wstring::npos;DWORD owner=wcstoul(v.c_str(),nullptr,10);SysFreeString(init);if(!connect(owner))return E_FAIL;}
        if(!shared)return E_FAIL;wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);std::wstring path=exe;
        if(glass::startHost(glass::runtimeName(path)))host=1;else if(path.find(L"ShellExperienceHost.exe")!=std::wstring::npos||path.find(L"ShellHost.exe")!=std::wstring::npos)host=2;else host=modern?3:0;
        AcquireSRWLockShared(&configLock);if(sharedStatus)InterlockedOr(&sharedStatus->connected,1<<host);ReleaseSRWLockShared(&configLock);
        // Keep one reusable session per framework, rather than creating a new
        // diagnostics tree on every On/Off cycle. Off unsubscribes callbacks.
        AddRef();AcquireSRWLockExclusive(&sessionsLock);sessions.push_back(this);ReleaseSRWLockExclusive(&sessionsLock);ensureSubscription();return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation relation,VisualElement element,VisualMutationType mutation)override{
        try{auto&t=thread();if(!cache&&++compatibilityReports>=8192)ensureSubscription();if(mutation==Add){auto surface=glass::identify(element.Name?element.Name:L"",element.Type?element.Type:L"",host);
                if(host==0&&surface==glass::NoSurface)surface=glass::identify(element.Name?element.Name:L"",element.Type?element.Type:L"",3);
                if(host==0&&surface==glass::NoSurface&&element.Name&&wcscmp(element.Name,L"RootGrid")==0&&relation.Parent){auto parent=from(relation.Parent);HSTRING type{};if(parent&&SUCCEEDED(parent->GetRuntimeClassName(&type))){auto name=WindowsGetStringRawBuffer(type,nullptr);if(name&&wcscmp(name,L"Taskbar.TaskbarFrame")==0){surface=glass::TaskbarRoot;t.taskbarPanel=true;}WindowsDeleteString(type);}}

                glass::Config diagnosticConfig;bool keyboardDiagnostics=(host==0||host==3)&&snapshot(diagnosticConfig)==Read::Ok&&(diagnosticConfig.features&glass::Keyboard);if(surface!=glass::NoSurface||((host==2||keyboardDiagnostics)&&element.Name&&*element.Name))note(L"match surface="+std::to_wstring((int)surface)+L" host="+std::to_wstring(host)+L" exe="+hostExe()+L" type="+(element.Type?element.Type:L"")+L" name="+(element.Name?element.Name:L""));
                auto target=from(element.Handle);auto imported=target&&imported_glass::candidate(imported_glass::theme(host),{element.Type?element.Type:L"",element.Name?element.Name:L""})?importedStyles(target.p):std::vector<const wchar_t*>{};
                if(!imported.empty()&&surface==glass::NoSurface)surface=host==1?glass::StartPlate:host==2?glass::NotificationAuxiliary:host==3?glass::ExplorerPlate:glass::TaskbarAuxiliary;
                if(surface!=glass::NoSurface){auto p=std::move(target);Ptr<IWeakReferenceSource> source;if(p&&SUCCEEDED(p->QueryInterface(__uuidof(IWeakReferenceSource),reinterpret_cast<void**>(source.put())))){
                    Node n;if(host==2&&element.Name){if(wcscmp(element.Name,L"NotificationCenterGrid")==0)n.notificationRoot=0;else if(wcscmp(element.Name,L"CalendarCenterGrid")==0)n.notificationRoot=1;if(n.notificationRoot>=0)t.notificationRoots[n.notificationRoot]=element.Handle;}n.imported=std::move(imported);n.surface=surface;source->GetWeakReference(n.weak.put());auto old=t.nodes.find(element.Handle);if(old!=t.nodes.end()){t.updating=true;restore(old->second,element.Handle);t.updating=false;if(!old->second.saved.empty()){releaseLater(t,element.Handle);return S_OK;}}t.nodes[element.Handle]=std::move(n);
                    glass::Config c;if(snapshot(c)==Read::Ok){t.updating=true;apply(t.nodes[element.Handle],element.Handle,c);t.updating=false;}
                }}glass::Config keyboardConfig;if(snapshot(keyboardConfig)==Read::Ok&&(keyboardConfig.features&glass::Keyboard))for(auto const&entry:t.nodes)if(entry.second.surface==glass::KeyboardPlate&&!entry.second.appliedSignature){scheduleUpdate(t);break;}releaseLater(t,element.Handle);releaseLater(t,relation.Parent);
            }else if(mutation==Remove){auto i=t.nodes.find(element.Handle);if(i!=t.nodes.end()){t.updating=true;restore(i->second,element.Handle);t.updating=false;for(auto&w:i->second.watchers)w.second();t.nodes.erase(i);}releaseLater(t,element.Handle);}
        }catch(...){lastError=E_FAIL;}return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle,VisualElementState,LPCWSTR)override{return S_OK;}
};
class Factory final:public IClassFactory {std::atomic<ULONG>refs{1};public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void**p)override{if(!p)return E_POINTER;*p=nullptr;if(id!=IID_IUnknown&&id!=IID_IClassFactory)return E_NOINTERFACE;*p=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown*outer,REFIID id,void**p)override{if(outer)return CLASS_E_NOAGGREGATION;auto s=new Session;auto hr=s->QueryInterface(id,p);s->Release();return hr;}
    HRESULT STDMETHODCALLTYPE LockServer(BOOL)override{return S_OK;}
};
DWORD WINAPI attachFramework(void*arg){int modern=(int)(INT_PTR)arg;CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    HMODULE module=GetModuleHandleW(modern?L"Microsoft.Internal.FrameworkUdk.dll":L"Windows.UI.Xaml.dll");
    auto fn=module?reinterpret_cast<decltype(&InitializeXamlDiagnosticsEx)>(GetProcAddress(module,"InitializeXamlDiagnosticsEx")):nullptr;
    HRESULT result=E_FAIL;if(fn){wchar_t path[32768]{};GetModuleFileNameW(selfModule,path,32768);auto init=std::to_wstring(ownerPid)+(modern?L" modern":L" classic");
        for(int i=1;i<=128;++i){auto endpoint=std::wstring(modern?L"WinUIVisualDiagConnection":L"VisualDiagConnection")+std::to_wstring(i);result=fn(endpoint.c_str(),GetCurrentProcessId(),L"",path,glass::runtimeTapId(runtimePath()),init.c_str());if(result!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND))break;}
    }trace(modern?"initialize modern":"initialize system",result);if(FAILED(result)){injecting[modern]=false;report(result);}CoUninitialize();return 0;
}
}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID id,REFIID iid,void**p){if(id!=glass::TapId&&id!=glass::runtimeTapId(runtimePath()))return CLASS_E_CLASSNOTAVAILABLE;auto f=new Factory;auto hr=f->QueryInterface(iid,p);f->Release();return hr;}
extern "C" HRESULT WINAPI DllCanUnloadNow(){return S_FALSE;}
extern "C" __declspec(dllexport) HRESULT WINAPI GlassGraphicsTest(){
    if(!liquid::testTint()||!imported_glass::test()||!liquid::testCapsulePlacement()||!liquid::testDockWeight())return E_FAIL;
    auto subscriptionTest=xaml_subscription::test();if(FAILED(subscriptionTest))return subscriptionTest;
    struct QueueOptions{DWORD size;int thread,apartment;};
    HMODULE messaging=LoadLibraryExW(L"CoreMessaging.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!messaging)return HRESULT_FROM_WIN32(GetLastError());
    auto create=(HRESULT(WINAPI*)(QueueOptions,void**))GetProcAddress(messaging,"CreateDispatcherQueueController");Ptr<IUnknown> queue;
    HRESULT hr=create?create({sizeof(QueueOptions),2,2},(void**)queue.put()):E_NOINTERFACE;if(FAILED(hr))return hr;
    try{auto gaussian=winrt::make_self<liquid::Effect>();GUID id{};UINT count=0;
        if(FAILED(gaussian->GetEffectId(&id))||id!=CLSID_D2D1GaussianBlur||FAILED(gaussian->GetPropertyCount(&count))||count!=3)return E_FAIL;
        for(UINT i=0;i<3;++i){Ptr<IUnknown> value;if(FAILED(gaussian->GetProperty(i,value.put()))||!value)return E_FAIL;}
        auto compositor=winrt::Windows::UI::Composition::Compositor();glass::Config config;
        auto brush=liquid::makeGlass(compositor,config);if(!brush)return E_FAIL;
        config.blur=0;config.opacity=5;auto clear=liquid::makeGlass(compositor,config);if(!clear)return E_FAIL;
        config.blur=40;config.opacity=90;config.tint=2;auto dark=liquid::makeGlass(compositor,config);if(!dark)return E_FAIL;
        config.tint=3;auto adaptive=liquid::makeGlass(compositor,config,true);if(!adaptive)return E_FAIL;
        return S_OK;
    }catch(...){return winrt::to_hresult();}
}
// Exercises connect() inside the loaded engine. A config whose owner process does not exist must be refused and leave no state; the controller's own published config must connect, be idempotent, and release cleanly.
extern "C" __declspec(dllexport) HRESULT WINAPI GlassConnectTest(){
    auto idle=[]{AcquireSRWLockShared(&configLock);bool clean=!shared&&!mapping&&!ownerHandle&&!ownerWait&&ownerPid==0;ReleaseSRWLockShared(&configLock);return clean;};
    if(!idle())return E_FAIL;
    const DWORD missing=0x7ffffff0;auto name=glass::mappingName(missing);
    HANDLE fake=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(glass::Config),name.c_str());if(!fake)return HRESULT_FROM_WIN32(GetLastError());
    if(auto view=(glass::Config*)MapViewOfFile(fake,FILE_MAP_WRITE,0,0,sizeof(glass::Config))){glass::Config c;c.owner=missing;*view=c;UnmapViewOfFile(view);}
    bool refused=!connect(missing)&&idle();CloseHandle(fake);
    if(!refused)return E_FAIL;
    DWORD self=GetCurrentProcessId();
    if(!connect(self)||!connect(self))return E_FAIL;
    AcquireSRWLockShared(&configLock);bool connected=shared&&ownerHandle&&ownerWait&&ownerPid==self;ReleaseSRWLockShared(&configLock);
    AcquireSRWLockExclusive(&configLock);releaseOwnerLocked();ReleaseSRWLockExclusive(&configLock);
    return connected&&idle()?S_OK:E_FAIL;
}
void bootstrap(DWORD owner){if(connect(owner)){
        AcquireSRWLockShared(&sessionsLock);for(auto s:sessions)s->ensureSubscription();ReleaseSRWLockShared(&sessionsLock);
        for(int modern=0;modern<2;++modern)if(GetModuleHandleW(modern?L"Microsoft.Internal.FrameworkUdk.dll":L"Windows.UI.Xaml.dll")&&!injecting[modern].exchange(true)){HANDLE t=CreateThread(nullptr,0,attachFramework,(void*)(INT_PTR)modern,0,nullptr);if(t)CloseHandle(t);else injecting[modern]=false;}
        refreshWindows();
    auto controller=ownerWindow();if(controller)PostMessageW(controller,RegisterWindowMessageW(glass::AttachAckMessage),GetCurrentThreadId(),0);
}}
extern "C" __declspec(dllexport) LRESULT CALLBACK GlassHook(int code,WPARAM w,LPARAM l){
    if(code>=0){auto m=(CWPSTRUCT*)l;if(m->message==RegisterWindowMessageW(glass::AttachMessage))bootstrap((DWORD)m->wParam);}return CallNextHookEx(nullptr,code,w,l);
}
extern "C" __declspec(dllexport) LRESULT CALLBACK GlassMessageHook(int code,WPARAM w,LPARAM l){
    if(code>=0&&w==PM_REMOVE){auto m=(MSG*)l;if(m->message==RegisterWindowMessageW(glass::AttachMessage)){bootstrap((DWORD)m->wParam);m->message=WM_NULL;}}return CallNextHookEx(nullptr,code,w,l);
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){selfModule=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
