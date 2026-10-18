#define _WIN32_WINNT 0x0A00
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <memory>
#include <limits>
#include <cstdio>
#include "../client/lg_client.h"
#include "../../../src/lgengine_preview.h"

namespace {
constexpr UINT HostEvent=WM_APP+60;
struct App {
    HWND window{},owner{};lghost::Handle parent;lgpreview::Settings settings;
    std::unique_ptr<lghost::Client> client;lghost::Spec spec;bool visible=true,failed=false;
    const wchar_t* title(unsigned event)const{
        static const wchar_t* connected[]={L"LGEngine host connected — drag to move",L"LGEngine 宿主已连接 — 拖动移动",L"LGEngine 宿主已連線 — 拖動移動",L"Połączono z LGEngine — przeciągnij",L"Đã kết nối LGEngine — kéo để di chuyển"};
        static const wchar_t* lost[]={L"LGEngine disconnected — reconnecting",L"LGEngine 已断开 — 正在重连",L"LGEngine 已中斷 — 正在重新連線",L"LGEngine rozłączony — ponowne łączenie",L"LGEngine ngắt kết nối — đang kết nối lại"};
        static const wchar_t* failure[]={L"LGEngine host unavailable",L"LGEngine 宿主不可用",L"LGEngine 宿主無法使用",L"Host LGEngine niedostępny",L"LGEngine host không khả dụng"};
        return (event==lghost::Lost?lost:event==lghost::Failed?failure:connected)[settings.language];
    }
    void place(){RECT r{};GetWindowRect(window,&r);spec.x=r.left;spec.y=r.bottom+8;if(client)client->setPlace(spec.x,spec.y);}
    void content(){
        // Only app-owned text pixels are drawn here. All glass stays in the host.
        HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=spec.width;info.bmiHeader.biHeight=-spec.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        uint32_t*pixels{};HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        if(!dc||!bitmap){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);throw std::runtime_error("content bitmap");}
        auto old=SelectObject(dc,bitmap);memset(pixels,0,size_t(spec.width)*spec.height*4);SetTextColor(dc,RGB(254,254,254));SetBkMode(dc,TRANSPARENT);
        HFONT font=CreateFontW(-MulDiv(20,GetDpiForWindow(window),96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Segoe UI");auto previous=SelectObject(dc,font);
        static const wchar_t* text[]={L"LGEngine host\nBackground-adaptive liquid glass",L"LGEngine 宿主\n根据背景自适应的液态玻璃",L"LGEngine 宿主\n根據背景自適應的液態玻璃",L"Host LGEngine\nSzkło dopasowane do tła",L"LGEngine host\nKính tự điều chỉnh theo nền"};
        RECT bounds{20,spec.height/3,spec.width-20,spec.height-20};DrawTextW(dc,text[settings.language],-1,&bounds,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);GdiFlush();
        std::vector<uint32_t> image(pixels,pixels+size_t(spec.width)*spec.height);
        for(auto&p:image)if(p&0xffffff)p=0xfefefefe; // LGEngine's even-alpha ink convention.
        SelectObject(dc,previous);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);client->setContents(std::move(image));
    }
    void apply(){
        if(!client)return;
        spec.look.dark=settings.opacity;spec.look.blur=(settings.blur*100+20)/40;spec.look.round=settings.radius;
        spec.savePower=settings.savePower!=0;spec.hover=!settings.reducedMotion;spec.motion=settings.reducedMotion?0:4;
        // Protocol v1 has background adaptation but no explicit light override.
        spec.adapt=true;client->setSpec(spec);content();client->show(visible);InvalidateRect(window,nullptr,TRUE);
    }
    bool valid(lgpreview::Settings s){return s.version==2&&s.opacity>=5&&s.opacity<=90&&s.blur<=40&&s.radius<=28&&s.tint<=2&&s.savePower<=1&&s.reducedMotion<=1&&s.language<=4;}
};
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    auto app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);app->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(!app)return DefWindowProcW(hwnd,msg,w,l);
    try {switch(msg){
    case HostEvent:
        if(w==lghost::Failed){app->failed=true;SetWindowTextW(hwnd,app->title(unsigned(w)));
            wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);std::wstring path=executable;path.resize(path.find_last_of(L"\\")+1);path+=L"host-client.log";
            auto file=_wfopen(path.c_str(),L"a");if(file){auto text=app->client->failureText();fprintf(file,"Win32 %lu: %s\n",(unsigned long)app->client->lastError(),text.c_str());fclose(file);}
            if(app->owner)PostMessageW(app->owner,lgpreview::Event,2,reinterpret_cast<LPARAM>(hwnd));}
        else if(w==lghost::Lost||w==lghost::Shown)SetWindowTextW(hwnd,app->title(unsigned(w)));return 0;
    case WM_COPYDATA:{auto p=reinterpret_cast<COPYDATASTRUCT*>(l);if(reinterpret_cast<HWND>(w)!=app->owner||!p||p->dwData!=lgpreview::WireTag||p->cbData!=sizeof(lgpreview::Settings)||!p->lpData)return 0;
        lgpreview::Settings next;memcpy(&next,p->lpData,sizeof(next));if(!app->valid(next))return 0;app->settings=next;app->apply();return lgpreview::digest(next);}
    case WM_TIMER:{if(app->parent.value&&WaitForSingleObject(app->parent.value,0)!=WAIT_TIMEOUT){DestroyWindow(hwnd);return 0;}
        HIGHCONTRASTW high{};high.cbSize=sizeof(high);SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0);
        bool ownerHidden=app->owner&&GetParent(app->owner)!=HWND_MESSAGE&&(!IsWindowVisible(app->owner)||IsIconic(app->owner));
        bool visible=!ownerHidden&&!IsIconic(hwnd)&&!(high.dwFlags&HCF_HIGHCONTRASTON);
        if(visible!=app->visible){app->visible=visible;app->client->show(visible);}return 0;}
    case WM_MOVE:app->place();return 0;
    case WM_DPICHANGED:{auto r=reinterpret_cast<RECT*>(l);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
        app->spec.scale=GetDpiForWindow(hwnd)/96.f;app->spec.width=MulDiv(420,GetDpiForWindow(hwnd),96);app->spec.height=MulDiv(200,GetDpiForWindow(hwnd),96);app->apply();return 0;}
    case WM_APP+17:ShowWindow(hwnd,SW_RESTORE);SetForegroundWindow(hwnd);return 0;
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:KillTimer(hwnd,1);app->client.reset();PostQuitMessage(0);return 0;
    case WM_PAINT:{PAINTSTRUCT p{};auto dc=BeginPaint(hwnd,&p);RECT r{};GetClientRect(hwnd,&r);FillRect(dc,&r,GetSysColorBrush(COLOR_WINDOW));SetBkMode(dc,TRANSPARENT);
        static const wchar_t* note[]={L"C++ → pipe + shared bitmap → LGEngineHost\nGlass adapts to the background.\nHost v1: manual Light/Dark is unavailable.",L"C++ → 管道与共享位图 → LGEngineHost\n玻璃根据背景自动调整。\n宿主 v1 不支持手动浅色／深色。",L"C++ → 管道與共用點陣圖 → LGEngineHost\n玻璃根據背景自動調整。\n宿主 v1 不支援手動淺色／深色。",L"C++ → potok i wspólna bitmapa → LGEngineHost\nSzkło dostosowuje się do tła.\nHost v1: brak ręcznego trybu Jasny/Ciemny.",L"C++ → pipe và bitmap dùng chung → LGEngineHost\nKính tự điều chỉnh theo nền.\nHost v1 chưa hỗ trợ chọn Sáng/Tối."};
        DrawTextW(dc,note[app->settings.language],-1,&r,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);EndPaint(hwnd,&p);return 0;}
    }}catch(...){app->failed=true;if(app->owner)PostMessageW(app->owner,lgpreview::Event,2,reinterpret_cast<LPARAM>(hwnd));}
    return DefWindowProcW(hwnd,msg,w,l);
}
int selfTest(){lghost::Spec spec;auto frame=lghost::wire::frame(3,42,lghost::wire::create(1,spec));int bad=0;
    auto check=[&](bool ok,const char*label){printf("%s %s\n",ok?"PASS":"FAIL",label);if(!ok)++bad;};
    check(lghost::wire::valid(spec),"default spec valid");check(lghost::wire::frame(frame),"frame validates");frame[8]++;check(!lghost::wire::frame(frame),"bad length rejected");spec.width=0;check(!lghost::wire::valid(spec),"zero size rejected");
    spec.width=420;spec.scale=std::numeric_limits<float>::quiet_NaN();check(!lghost::wire::valid(spec),"nonfinite scale rejected");return bad?1:0;}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    int argc{};auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 2;
    if(argc==2&&!wcscmp(argv[1],L"--self-test")){LocalFree(argv);return selfTest();}
    App app;bool argsOk=argc==2&&!wcscmp(argv[1],L"--standalone");
    if(argc==5&&!wcscmp(argv[1],L"--owner")&&!wcscmp(argv[3],L"--pid")){
        wchar_t*end{};auto h=wcstoull(argv[2],&end,10);if(*end){LocalFree(argv);return 2;}app.owner=reinterpret_cast<HWND>(h);auto requested=wcstoul(argv[4],&end,10);DWORD pid{};
        argsOk=!*end&&requested&&GetWindowThreadProcessId(app.owner,&pid)&&pid==requested;if(argsOk)app.parent.reset(OpenProcess(SYNCHRONIZE,FALSE,pid));argsOk=argsOk&&app.parent.value;
    }LocalFree(argv);if(!argsOk)return 2;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LiquidGlass.LGHost.Client";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(!RegisterClassW(&cls))return 1;auto hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"Connecting to LGEngineHost",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,100,100,600,150,nullptr,nullptr,instance,&app);if(!hwnd)return 1;
    try{wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);std::wstring path=executable;path.resize(path.find_last_of(L"\\")+1);lghost::Options options;options.hostPath=lghost::bundledHostPath(path);options.notify=hwnd;options.message=HostEvent;
        app.spec.scale=GetDpiForWindow(hwnd)/96.f;app.spec.width=MulDiv(420,GetDpiForWindow(hwnd),96);app.spec.height=MulDiv(200,GetDpiForWindow(hwnd),96);app.place();
        app.client=std::make_unique<lghost::Client>(options,app.spec);app.apply();
    }catch(...){DestroyWindow(hwnd);return 1;}
    SetTimer(hwnd,1,250,nullptr);ShowWindow(hwnd,SW_SHOWNORMAL);if(app.owner)PostMessageW(app.owner,lgpreview::Event,1,reinterpret_cast<LPARAM>(hwnd));
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return app.failed?1:0;
}
