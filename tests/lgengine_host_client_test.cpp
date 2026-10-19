#define _WIN32_WINNT 0x0A00
#define UNICODE
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <memory>
#include "../integrations/LGEngine/client/lg_client.h"
namespace {
unsigned shown[2]{},lost[2]{},restarted[2]{},failed[2]{},checks=0,failures=0;
void check(bool ok,const char*text){++checks;if(!ok)++failures;printf("%s %s\n",ok?"PASS":"FAIL",text);}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_APP+60||msg==WM_APP+61){int i=msg-WM_APP-60;if(w==lghost::Shown)++shown[i];if(w==lghost::Lost)++lost[i];if(w==lghost::Restarted)++restarted[i];if(w==lghost::Failed)++failed[i];return 0;}
    return DefWindowProcW(hwnd,msg,w,l);
}
template<class P>bool pump(P done,DWORD timeout=10000){auto end=GetTickCount64()+timeout;while(GetTickCount64()<end){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(done())return true;MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);}return done();}
}
int main(int argc,char**argv){
    if(argc!=2)return 2;int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,nullptr,0);if(!size)return 2;
    std::wstring path(size,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,path.data(),size);path.pop_back();
    WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"LGEngine.CppClient.Tests";if(!RegisterClassW(&cls))return 2;
    auto hwnd=CreateWindowW(cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);
    lghost::Options opts;opts.hostPath=path;opts.suffix=L"cpp"+std::to_wstring(GetCurrentProcessId());opts.notify=hwnd;
    lghost::Spec spec;spec.width=96;spec.height=64;spec.x=-10000;spec.y=-10000;spec.motion=0;
    try {
        auto a=std::make_unique<lghost::Client>(opts,spec);a->setContents(std::vector<uint32_t>(96*64,0));a->show();
        check(pump([&]{return shown[0]||failed[0];})&&shown[0],"original host auto-starts and shows C++ surface");
        if(!shown[0]){printf("client Win32 error %lu\n",(unsigned long)a->lastError());return 1;}
        opts.message=WM_APP+61;auto b=std::make_unique<lghost::Client>(opts,spec);b->show();
        check(pump([&]{return shown[1]||failed[1];})&&shown[1],"second C++ client creates another surface");
        check(a->hostProcessId()!=0&&a->hostProcessId()==b->hostProcessId(),"two clients share the same authenticated host process");
        bool invalid=false;try{a->setContents(std::vector<uint32_t>(1));}catch(const std::invalid_argument&){invalid=true;}check(invalid,"incorrect shared bitmap size rejected");
        a->setPlace(-9000,-9000);a->show(false);check(pump([&]{return a->connected();},200),"state updates preserve connection");
        check(b->hostMinor()<=1&&b->light()>=0&&b->light()<=1,"host minor and light are in range");
        unsigned idBefore=b->surfaceId();
        for(int change=0;change<12;++change){spec.look.dark=20+change;b->setSpec(spec);pump([]{return false;},30);}
        for(int step=1;step<=3;++step){int w=96+8*step,h=64+8*step;spec.width=w;spec.height=h;b->resize(w,h,std::vector<uint32_t>(size_t(w)*h,0));pump([]{return false;},60);}
        pump([]{return false;},200);
        check(idBefore!=0&&b->surfaceId()==idBefore&&!failed[1],"look and size changes keep the same surface");
        b->setSlosh(1,60);b->kick(1);b->setBusy(true);b->setBusy(false);b->setPointerArea(RECT{0,0,10,10});b->setButtons(std::vector<lghost::Button>(),false);
        pump([]{return false;},100);
        check(b->connected()&&!failed[1],"1.1 setters are harmless on any host");
        check(b->hostErrors()==0&&a->hostErrors()==0,"host sent no Error frames (types 10-17 are not sent to a minor-0 host)");
        unsigned beforeKind=shown[1];
        spec.kind=1;b->setSpec(spec);
        check(pump([&]{return shown[1]>beforeKind||failed[1];})&&shown[1]>beforeKind&&!failed[1]&&b->surfaceId()!=idBefore,"kind change re-creates the surface (new id, new Shown)");
        unsigned beforeRestart=shown[1];
        unsigned oldHost=a->hostProcessId();lghost::Handle host(OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,oldHost));
        check(host.value&&TerminateProcess(host.value,77),"stop only the dedicated suffixed test host");
        if(host.value)WaitForSingleObject(host.value,2000);
        check(pump([&]{return lost[0]&&lost[1];}),"both clients report host loss");
        check(pump([&]{return restarted[0]&&restarted[1]&&shown[1]>beforeRestart;},15000),"host restart restores retained visible surface");
        check(a->hostProcessId()!=oldHost&&a->hostProcessId()==b->hostProcessId(),"reconnected clients share replacement host");
        a.reset();unsigned hostB=b->hostProcessId();check(b->connected()&&hostB!=0,"closing one client preserves the other client and shared host");
        check(!failed[0]&&!failed[1],"no protocol or host surface errors");b.reset();
    }catch(const std::exception&e){printf("FAIL exception %s\n",e.what());++failures;}
    DestroyWindow(hwnd);printf("C++ host client: %u/%u checks OK\n",checks-failures,checks);return failures?1:0;
}
