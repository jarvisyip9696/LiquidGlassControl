#define _WIN32_WINNT 0x0A00
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include "lg_client.h"
#include <shellapi.h>
#include <cstdio>
#include <memory>

// An independent consumer: no controller, XAML, capture or renderer dependency.
namespace {
constexpr UINT HostEvent=WM_APP+60;
struct App {std::unique_ptr<lghost::Client> client;bool shown=false,failed=false;};
std::vector<uint32_t> textPixels(int width,int height,bool light){
    HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    uint32_t* pixels{};auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
    if(!dc||!bitmap){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);throw std::runtime_error("content DIB");}
    auto old=SelectObject(dc,bitmap);memset(pixels,0,size_t(width)*height*4);
    auto font=CreateFontW(-24,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Segoe UI");
    auto previous=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(254,254,254));
    RECT r{24,40,width-24,height-20};DrawTextW(dc,L"C++ app\nGlass rendered by LGEngineHost",-1,&r,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);GdiFlush();
    std::vector<uint32_t> result(pixels,pixels+size_t(width)*height);
    for(auto&pixel:result)if(pixel&0xffffff)pixel=light?0xfe101010:0xfefefefe;
    SelectObject(dc,previous);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return result;
}
LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    auto app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(app&&message==HostEvent){
        if(w==lghost::Shown){app->shown=true;printf("SHOWN host=%u\n",app->client->hostProcessId());fflush(stdout);}
        if(w==lghost::Appearance)try{app->client->setContents(textPixels(420,180,l!=0));}catch(...){app->failed=true;}
        if(w==lghost::Lost)puts("LOST: retaining state for reconnection");
        if(w==lghost::Restarted)puts("RESTARTED: client recreates its surface");
        if(w==lghost::Failed){app->failed=true;printf("FAILED %lu: %s\n",static_cast<unsigned long>(app->client->lastError()),app->client->failureText().c_str());}
        return 0;
    }
    return DefWindowProcW(hwnd,message,w,l);
}
}
int wmain(int argc,wchar_t**argv){
    // Explicit host path makes this sample runnable outside the controller tree.
    if(argc<2||argc>3){puts("LGEngineExample.exe <LGEngineHost.exe> [--smoke-test]");return 2;}
    bool smoke=argc==3&&!wcscmp(argv[2],L"--smoke-test");if(argc==3&&!smoke)return 2;
    App app;WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"LGEngine.ExternalCppConsumer";
    if(!RegisterClassW(&cls))return 2;
    HWND hwnd=CreateWindowW(cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,&app);if(!hwnd)return 2;
    int result=1;
    try{
        lghost::Options options;options.hostPath=argv[1];options.notify=hwnd;options.message=HostEvent;
        lghost::Spec spec;spec.width=420;spec.height=180;spec.label="External C++ app";
        if(smoke){spec.x=spec.y=-10000;spec.motion=0;}
        app.client=std::make_unique<lghost::Client>(options,spec);
        app.client->setContents(textPixels(420,180,false));app.client->show();
        auto until=GetTickCount64()+(smoke?10000:30000);
        while(!app.failed&&GetTickCount64()<until&&(!smoke||!app.shown)){
            MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
            if(!app.failed&&(!smoke||!app.shown))MsgWaitForMultipleObjects(0,nullptr,FALSE,50,QS_ALLINPUT);
        }
        result=app.shown&&!app.failed?0:1;app.client.reset();
    }catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());}
    DestroyWindow(hwnd);return result;
}
