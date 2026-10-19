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
    }
};
}
