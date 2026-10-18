#pragma once
#include <cstdint>
#include <cstring>
#include <string>
// The optional optical renderer stays outside Explorer. Only this
// controller's child process may acknowledge a preview window.
namespace lgpreview {
constexpr UINT Event = WM_APP + 16;
constexpr ULONG_PTR WireTag = 0x4c475031;
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
    HANDLE process{},job{};DWORD pid{};HWND remote{};Settings settings{};
    DWORD error{};HWND ownerWindow{};bool sent=false;
    bool validRemote()const{DWORD source=0;return remote&&running()&&GetWindowThreadProcessId(remote,&source)&&source==pid;}
public:
    ~Controller(){stop();}
    DWORD lastError()const{return error;}
    bool running()const{return process&&WaitForSingleObject(process,0)==WAIT_TIMEOUT;}
    bool connected()const{return validRemote();}
    bool synchronized()const{return validRemote()&&sent;}
    DWORD processId()const{return pid;}
    bool accept(HWND window) {
        DWORD source=0;GetWindowThreadProcessId(window,&source);
        if(!running()||!window||source!=pid)return false;
        remote=window;sent=false;return true;
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
        CloseHandle(child.hThread);return true;
    }
    void stop(){
        if(validRemote()){DWORD_PTR result=0;SendMessageTimeoutW(remote,WM_CLOSE,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,150,&result);}
        // Closing the job also covers startup failure and controller crashes.
        if(job){CloseHandle(job);job=nullptr;}
        if(process){CloseHandle(process);process=nullptr;}
        remote=nullptr;pid=0;sent=false;
    }
};
}
