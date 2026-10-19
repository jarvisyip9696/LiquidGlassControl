#pragma once
#include <cstdint>
#include <cstring>
#include <string>
// The optional optical renderer stays outside Explorer. Only this
// controller's child process may acknowledge a preview window.
namespace lgpreview {
constexpr UINT Event = WM_APP + 16;
// To the controller: 1 = the child's host connected (lParam: child window), 2 = the child process exited (posted by a wait).
constexpr UINT HostState = WM_APP + 19;
constexpr ULONG_PTR WireTag = 0x4c475031;
constexpr ULONG_PTR ControlsTag = 0x4c475032;
constexpr ULONG_PTR StatusTag = 0x4c475033;
struct Controls { int arrival=5, indicator=5, slosh=0, knob=2, enabled=1, osd=1, action=0;
    int dark=5, blur=32, refraction=90, rim=68, shadow=30, round=24, intensity=100, elevation=50, shape=0; };
static_assert(sizeof(Controls)==16*sizeof(int),"LGEngine controls wire size");
struct Settings {
    DWORD version=2, opacity=35, blur=12, radius=12, tint=0;
    DWORD savePower=1, reducedMotion=0, language=0;
};
static_assert(sizeof(Settings)==32,"LGEngine preview wire size");
inline DWORD digest(Settings const&value){
    DWORD hash=2166136261u;auto bytes=reinterpret_cast<const BYTE*>(&value);
    for(size_t i=0;i<sizeof(value);++i){hash^=bytes[i];hash*=16777619u;}
    return hash;
}
inline void restoreFlyouts(); // below, with the pop-up helpers
class Controller {
    HANDLE process{},job{},waiter{};DWORD pid{};HWND remote{};Settings settings{};
    DWORD error{};HWND ownerWindow{};bool sent=false;Controls lastControls{};bool controlsSent=false;
    bool validRemote()const{DWORD source=0;return remote&&running()&&GetWindowThreadProcessId(remote,&source)&&source==pid;}
    static void CALLBACK exitedCallback(PVOID window,BOOLEAN){PostMessageW(static_cast<HWND>(window),HostState,2,0);}
public:
    ~Controller(){stop();}
    DWORD lastError()const{return error;}
    unsigned status(){
        if(!validRemote())return 0;
        COPYDATASTRUCT data{StatusTag,0,nullptr};DWORD_PTR result=0;
        if(!SendMessageTimeoutW(remote,WM_COPYDATA,reinterpret_cast<WPARAM>(ownerWindow),reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&result))return 0;
        return unsigned(result);
    }
    bool controls(Controls value){
        if(!validRemote())return false;
        if(controlsSent&&value.action==0&&memcmp(&lastControls,&value,sizeof(Controls))==0)return true;
        COPYDATASTRUCT data{ControlsTag,sizeof(value),&value};DWORD_PTR result=0;
        bool ok=SendMessageTimeoutW(remote,WM_COPYDATA,reinterpret_cast<WPARAM>(ownerWindow),reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&result)&&result==1;
        if(ok){lastControls=value;lastControls.action=0;controlsSent=true;}
        return ok;
    }
    bool running()const{return process&&WaitForSingleObject(process,0)==WAIT_TIMEOUT;}
    bool exited()const{return process&&!running();}
    bool connected()const{return validRemote();}
    bool synchronized()const{return validRemote()&&sent;}
    bool owns(HWND window)const{return window&&window==remote&&validRemote();}
    DWORD processId()const{return pid;}
    bool accept(HWND window) {
        DWORD source=0;GetWindowThreadProcessId(window,&source);
        if(!running()||!window||source!=pid)return false;
        remote=window;sent=false;controlsSent=false;return true;
    }
    bool publish(Settings value){
        bool unchanged=memcmp(&settings,&value,sizeof(Settings))==0;
        settings=value;if(!validRemote())return false;if(sent&&unchanged)return true;
        COPYDATASTRUCT data{WireTag,sizeof(Settings),&settings};DWORD_PTR result=0;
        sent=SendMessageTimeoutW(remote,WM_COPYDATA,reinterpret_cast<WPARAM>(ownerWindow),reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&result)&&result==digest(settings);
        return sent;
    }
    bool start(HWND owner){
        error=0;
        if(running()){if(validRemote())PostMessageW(remote,WM_APP+17,0,0);return true;}
        stop();ownerWindow=owner;
        wchar_t path[32768]{};DWORD size=GetModuleFileNameW(nullptr,path,32768);
        if(!size||size>=32768){error=ERROR_INSUFFICIENT_BUFFER;return false;}
        std::wstring dir=path;dir.resize(dir.find_last_of(L"\\")+1);
        auto executable=dir+L"lgengine\\LiquidGlass.LGPreview.exe";
        std::wstring command=L"\""+executable+L"\" --owner "+std::to_wstring(reinterpret_cast<UINT_PTR>(owner))+L" --pid "+std::to_wstring(GetCurrentProcessId());
        if(GetFileAttributesW(executable.c_str())==INVALID_FILE_ATTRIBUTES){error=ERROR_FILE_NOT_FOUND;return false;}
        job=CreateJobObjectW(nullptr,nullptr);
        // The app-owned adapter dies with us; a shared LGEngine host may break
        // away explicitly so closing this app cannot kill other apps' glass.
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){error=GetLastError();stop();return false;}
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_SHOWNORMAL;
        PROCESS_INFORMATION child{};
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,dir.c_str(),&startup,&child)){error=GetLastError();stop();return false;}
        process=child.hProcess;pid=child.dwProcessId;
        if(!AssignProcessToJobObject(job,process)){error=GetLastError();TerminateProcess(process,1);CloseHandle(child.hThread);stop();return false;}
        if(ResumeThread(child.hThread)==DWORD(-1)){error=GetLastError();CloseHandle(child.hThread);stop();return false;}
        CloseHandle(child.hThread);
        // Notices a child that dies later (crash, blocked dependency) without polling.
        if(!RegisterWaitForSingleObject(&waiter,process,exitedCallback,owner,INFINITE,WT_EXECUTEONLYONCE))waiter=nullptr;
        return true;
    }
    void stop(){
        if(validRemote()){DWORD_PTR result=0;SendMessageTimeoutW(remote,WM_CLOSE,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,150,&result);}
        // Closing the job also covers startup failure and controller crashes.
        if(waiter){UnregisterWaitEx(waiter,nullptr);waiter=nullptr;}
        if(job){CloseHandle(job);job=nullptr;}
        if(process){CloseHandle(process);process=nullptr;}
        remote=nullptr;pid=0;sent=false;controlsSent=false;
        restoreFlyouts(); // the adapter may have been killed above (busy at WM_CLOSE): Windows' own pop-up comes back either way
    }
};
// Windows' own volume/brightness pop-up: a small topmost tool window of Explorer's (XamlExplorerHostIslandWindow),
// centred on the bottom of the work area and reused every time (it exists, hidden, between uses). The LGEngine
// indicator keeps it invisible (layered, alpha 1) while the lens replaces it, tagged with FlyoutMark so that the adapter,
// a later run of it or the controller can always undo it. Liquid Glass (the engine owner's app) does the same with
// OwnerMark when its own glass indicator replaces it; a pop-up it hides is never touched here.
constexpr const wchar_t* FlyoutMark=L"LiquidGlassControl.Opacity";
constexpr const wchar_t* OwnerMark=L"ToastMover.Opacity";
constexpr const wchar_t* FlyoutClass=L"XamlExplorerHostIslandWindow";
inline bool looksLikeFlyout(HWND h){ // the same test as Liquid Glass's Osd.cs LooksLikeIndicator
    wchar_t cls[64]{};if(!GetClassNameW(h,cls,64)||wcscmp(cls,FlyoutClass)!=0)return false;
    LONG_PTR ex=GetWindowLongPtrW(h,GWL_EXSTYLE);if((ex&(WS_EX_TOPMOST|WS_EX_TOOLWINDOW))!=(WS_EX_TOPMOST|WS_EX_TOOLWINDOW))return false;
    RECT r{};if(!GetWindowRect(h,&r))return false;UINT dpi=GetDpiForWindow(h);float k=dpi?dpi/96.f:1.f;
    float w=(r.right-r.left)/k,ht=(r.bottom-r.top)/k;if(w<100||w>380||ht<36||ht>96)return false;
    MONITORINFO mi{};mi.cbSize=sizeof(mi);if(!GetMonitorInfoW(MonitorFromRect(&r,MONITOR_DEFAULTTONEAREST),&mi))return false;
    float off=(r.left+r.right)/2.f-(mi.rcWork.left+mi.rcWork.right)/2.f;if(off<0)off=-off;
    return off<=20*k&&r.bottom>=mi.rcWork.bottom-48*k&&r.bottom<=mi.rcWork.bottom+12*k;
}
template<class F> void eachFlyout(F f){ // Explorer's indicator pop-ups, hidden or shown
    DWORD shell=0;HWND desktop=GetShellWindow();if(!desktop||!GetWindowThreadProcessId(desktop,&shell))return;
    HWND h=nullptr;for(int guard=0;guard<500&&(h=FindWindowExW(nullptr,h,FlyoutClass,nullptr))!=nullptr;++guard){DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(pid==shell&&looksLikeFlyout(h))f(h);}
}
inline bool responsive(HWND h){DWORD_PTR r=0;return SendMessageTimeoutW(h,WM_NULL,0,0,SMTO_ABORTIFHUNG,200,&r)!=0;}
inline void restoreFlyout(HWND h){
    if(!IsWindow(h)||!GetPropW(h,FlyoutMark))return;
    SetLayeredWindowAttributes(h,0,255,LWA_ALPHA); // fully visible again from here
    if(!responsive(h))return; // Explorer busy: the tag stays, so a later call (or run) finishes the tidying
    SetWindowLongPtrW(h,GWL_EXSTYLE,GetWindowLongPtrW(h,GWL_EXSTYLE)&~LONG_PTR(WS_EX_LAYERED));RemovePropW(h,FlyoutMark);
}
inline bool hideFlyout(HWND h){
    if(GetPropW(h,OwnerMark))return false; // Liquid Glass replaces it already
    LONG_PTR ex=GetWindowLongPtrW(h,GWL_EXSTYLE);bool ours=GetPropW(h,FlyoutMark)!=nullptr;
    if((ex&WS_EX_LAYERED)&&!ours)return false; // it manages its own transparency: not ours to touch
    if(!(ex&WS_EX_LAYERED)){
        if(!responsive(h)||!SetPropW(h,FlyoutMark,reinterpret_cast<HANDLE>(1)))return false;
        SetWindowLongPtrW(h,GWL_EXSTYLE,ex|WS_EX_LAYERED);
        if(!(GetWindowLongPtrW(h,GWL_EXSTYLE)&WS_EX_LAYERED)){RemovePropW(h,FlyoutMark);return false;}
    }
    if(!SetLayeredWindowAttributes(h,0,1,LWA_ALPHA)){restoreFlyout(h);return false;}
    return true;
}
// Puts back every pop-up this app left invisible, wherever it is now (also after a crash or a kill).
inline void restoreFlyouts(){HWND h=nullptr;for(int guard=0;guard<500&&(h=FindWindowExW(nullptr,h,FlyoutClass,nullptr))!=nullptr;++guard)if(GetPropW(h,FlyoutMark))restoreFlyout(h);}
// Puts back the ones that no longer look like the indicator (moved, resized, reused by Explorer for something else).
inline void restoreStrays(){HWND h=nullptr;for(int guard=0;guard<500&&(h=FindWindowExW(nullptr,h,FlyoutClass,nullptr))!=nullptr;++guard)if(GetPropW(h,FlyoutMark)&&!looksLikeFlyout(h))restoreFlyout(h);}
// Liquid Glass is running (its single-instance mutex exists).
inline bool ownerRunning(){HANDLE m=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\ToastMover.SingleInstance");if(!m)return false;CloseHandle(m);return true;}
// Who keeps Windows' pop-up hidden: 0 = nobody (Windows shows it), 1 = Liquid Glass, 2 = this app's LGEngine indicator.
inline int flyoutOwner(){int who=0;eachFlyout([&](HWND h){if(GetPropW(h,OwnerMark))who=1;else if(!who&&GetPropW(h,FlyoutMark))who=2;});return who;}
}
