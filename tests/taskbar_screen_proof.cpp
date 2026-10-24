#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <cstdio>
#include <algorithm>
// Read-only visual proof of the real taskbar, including capturable LGEngine
// layers. No window activation, input, taskbar configuration or security change.
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 10;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);RECT r{};
    if(!taskbar||!IsWindowVisible(taskbar)||!GetWindowRect(taskbar,&r))return 1;
    r.left=std::max<LONG>(r.left,GetSystemMetrics(SM_XVIRTUALSCREEN));r.top=std::max<LONG>(r.top,GetSystemMetrics(SM_YVIRTUALSCREEN));
    r.right=std::min<LONG>(r.right,GetSystemMetrics(SM_XVIRTUALSCREEN)+GetSystemMetrics(SM_CXVIRTUALSCREEN));
    r.bottom=std::min<LONG>(r.bottom,GetSystemMetrics(SM_YVIRTUALSCREEN)+GetSystemMetrics(SM_CYVIRTUALSCREEN));
    int w=r.right-r.left,h=r.bottom-r.top;if(w<1||h<1||w>8192||h>512)return 2;
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);if(!screen||!dc)return 3;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bitmap){DeleteDC(dc);ReleaseDC(nullptr,screen);return 4;}
    auto old=SelectObject(dc,bitmap);DwmFlush();BOOL copied=BitBlt(dc,0,0,w,h,screen,r.left,r.top,SRCCOPY|CAPTUREBLT);GdiFlush();
    HANDLE file=copied?CreateFileW(argv[1],GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr):INVALID_HANDLE_VALUE;
    bool ok=false;if(file!=INVALID_HANDLE_VALUE){BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+w*h*4;DWORD n=0;
        ok=WriteFile(file,&header,sizeof(header),&n,nullptr)&&n==sizeof(header)&&WriteFile(file,&info.bmiHeader,sizeof(info.bmiHeader),&n,nullptr)&&n==sizeof(info.bmiHeader)&&WriteFile(file,pixels,w*h*4,&n,nullptr)&&n==DWORD(w*h*4);CloseHandle(file);}
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
    printf("taskbar proof %dx%d: %s\n",w,h,ok?"saved":"failed");return ok?0:5;
}
