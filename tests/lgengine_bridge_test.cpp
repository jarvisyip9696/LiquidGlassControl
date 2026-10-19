#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include "../src/lgengine_preview.h"

// A real protocol integration test, not an alternate application entry point.
// It never loads the shell engine or applies system styles.
namespace {
lgpreview::Controller bridge;
bool renderFailed=false;
unsigned checks=0,failures=0;
void check(bool value,const char*name){++checks;if(!value)++failures;printf("%s: %s\n",value?"PASS":"FAIL",name);}
LRESULT CALLBACK ownerProc(HWND h,UINT message,WPARAM w,LPARAM l){
    if(message==lgpreview::Event){
        if(bridge.accept(reinterpret_cast<HWND>(l))&&w==2)renderFailed=true;
        return 0;
    }
    return DefWindowProcW(h,message,w,l);
}
template<class Predicate> bool pumpUntil(Predicate condition,DWORD timeout=5000){
    auto until=GetTickCount64()+timeout;
    while(GetTickCount64()<until){
        MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(condition())return true;
        MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);
    }
    return condition();
}
}
int main(int argc,char**argv){
    WNDCLASSW cls{};cls.lpfnWndProc=ownerProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"LiquidGlass.Tests.OpticalOwner";
    if(!RegisterClassW(&cls))return 2;
    HWND owner=CreateWindowExW(0,cls.lpszClassName,L"Protocol test owner",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);
    if(argc==3&&strcmp(argv[1],"--owner-crash")==0){
        HANDLE release=reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(_strtoui64(argv[2],nullptr,10)));
        if(!owner||!bridge.start(owner)||!pumpUntil([]{return bridge.connected();}))ExitProcess(3);
        DWORD pid=bridge.processId(),written=0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),&pid,sizeof(pid),&written,nullptr);
        WaitForSingleObject(release,5000);
        // Deliberately bypass C++ destructors. The job and adapter parent-exit
        // observer must clean up without Controller::stop being called.
        ExitProcess(0);
    }
    check(owner!=nullptr,"create isolated message owner");
    lgpreview::Settings settings;settings.opacity=46;settings.blur=23;settings.radius=19;settings.tint=2;settings.language=1;
    check(!bridge.publish(settings),"settings before startup are retained but not acknowledged");
    check(bridge.start(owner),"launch real optical adapter");
    if(!bridge.running())printf("Launch Win32 error: %lu\n",(unsigned long)bridge.lastError());
    bool connected=bridge.running()&&pumpUntil([]{return bridge.connected();});
    check(connected,"adapter authenticates owner and sends handshake");
    if(connected){
        check(pumpUntil([&]{return bridge.publish(settings);}),"exact parsed settings digest acknowledged");
        check(bridge.synchronized(),"acknowledgement marks settings synchronized");
        check(pumpUntil([&]{return (bridge.status()&1)!=0;},12000),"real host negotiated protocol 1.1");
        lgpreview::Controls controls;controls.enabled=0;check(bridge.controls(controls),"integrated page hides preview");controls.enabled=1;controls.arrival=8;controls.slosh=2;controls.knob=1;controls.action=2;check(bridge.controls(controls),"integrated page configures and samples preview");controls.enabled=0;controls.action=2;check(bridge.controls(controls),"sample starts while preview page is hidden");
        check(pumpUntil([&]{unsigned s=bridge.status();return (s&21)==21&&(s&10)==0;}),"background OSD shown with hidden card and no floating controls");
        controls.action=0;controls.osd=0;check(bridge.controls(controls)&&!(bridge.status()&20),"OSD off hides lens and stops background display");
        controls.osd=1;controls.action=0;check(bridge.controls(controls),"background OSD re-enabled");
        controls.knob=7;check(!bridge.controls(controls),"invalid page controls rejected");
        auto pid=bridge.processId();check(bridge.start(owner)&&bridge.processId()==pid,"reopen reuses child process");
        check(!bridge.accept(owner),"reject window from another process");
        check(bridge.connected(),"rejected acknowledgement leaves valid connection intact");
        settings.opacity=5;settings.blur=0;settings.radius=0;settings.tint=1;settings.language=4;settings.savePower=0;settings.reducedMotion=1;
        check(pumpUntil([&]{return bridge.publish(settings);}),"changed parameters and language acknowledged");
        check(bridge.publish(settings),"unchanged settings keep acknowledged state");
        auto invalid=settings;invalid.language=9;
        check(!bridge.publish(invalid)&&!bridge.synchronized(),"rejected settings do not report synchronization");
        check(pumpUntil([&]{return bridge.publish(settings);}),"valid update recovers after rejected settings");
        // Let the shader run after coalescing; a transport acknowledgement alone
        // does not prove that creating or updating a glass surface succeeded.
        auto wait=GetTickCount64()+1000;pumpUntil([&]{return GetTickCount64()>=wait;},1500);
        check(!renderFailed,"no optical surface failure during updates");
        HANDLE child=OpenProcess(SYNCHRONIZE,FALSE,pid);bridge.stop();
        check(child&&WaitForSingleObject(child,3000)==WAIT_OBJECT_0,"stop terminates the child and releases its windows");
        if(child)CloseHandle(child);
        check(!bridge.running()&&!bridge.connected()&&!bridge.synchronized(),"stopped controller clears connection state");
    }
    bridge.stop();
    if(connected){
        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};HANDLE read{},write{};
        HANDLE release=CreateEventW(&security,TRUE,FALSE,nullptr);
        bool pipe=CreatePipe(&read,&write,&security,0)!=FALSE;
        if(pipe)SetHandleInformation(read,HANDLE_FLAG_INHERIT,0);
        wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
        auto command=L"\""+std::wstring(executable)+L"\" --owner-crash "+std::to_wstring(reinterpret_cast<UINT_PTR>(release));
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;startup.hStdOutput=write;startup.hStdError=write;
        PROCESS_INFORMATION helper{};
        bool started=release&&pipe&&CreateProcessW(executable,command.data(),nullptr,nullptr,TRUE,0,nullptr,nullptr,&startup,&helper);
        check(started,"start isolated crash owner fixture");
        if(write)CloseHandle(write);
        if(started){
            DWORD available=0,pid=0,received=0;
            bool reported=pumpUntil([&]{return !PeekNamedPipe(read,nullptr,0,nullptr,&available,nullptr)||available>=sizeof(pid);});
            reported=reported&&available>=sizeof(pid)&&ReadFile(read,&pid,sizeof(pid),&received,nullptr)&&received==sizeof(pid);
            HANDLE child=reported?OpenProcess(SYNCHRONIZE,FALSE,pid):nullptr;
            check(child!=nullptr,"observe child before simulated owner crash");
            SetEvent(release);
            check(WaitForSingleObject(helper.hProcess,5000)==WAIT_OBJECT_0,"crash fixture exits without controller destructor");
            check(child&&WaitForSingleObject(child,5000)==WAIT_OBJECT_0,"abrupt owner exit leaves no adapter child");
            if(child)CloseHandle(child);CloseHandle(helper.hThread);CloseHandle(helper.hProcess);
        }
        if(read)CloseHandle(read);if(release)CloseHandle(release);
    }
    if(owner)DestroyWindow(owner);UnregisterClassW(cls.lpszClassName,cls.hInstance);
    printf("bridge: %u/%u checks OK\n",checks-failures,checks);
    return failures?1:0;
}
