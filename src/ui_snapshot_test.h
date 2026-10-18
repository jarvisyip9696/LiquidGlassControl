#pragma once
// Render this process's own controls to a bitmap. No desktop or foreign app capture.
inline bool saveUiProof(HWND root,const std::wstring&path){
    DWORD pid=0;GetWindowThreadProcessId(root,&pid);if(pid!=GetCurrentProcessId())return false;
    RECT r{};GetClientRect(root,&r);if(r.right<=0||r.bottom<=0)return false;
    HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    uint32_t*pixels{};HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
    if(!dc||!bitmap){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return false;}
    auto old=SelectObject(dc,bitmap);memset(pixels,0,size_t(r.right)*r.bottom*4);SendMessageW(root,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_ERASEBKGND);
    struct Context{HWND root;HDC dc;}context{root,dc};
    EnumChildWindows(root,[](HWND child,LPARAM data)->BOOL{auto&ctx=*reinterpret_cast<Context*>(data);
        for(HWND node=child;node&&node!=ctx.root;node=GetParent(node))if(!(GetWindowLongPtrW(node,GWL_STYLE)&WS_VISIBLE))return TRUE;
        RECT r{};GetClientRect(child,&r);POINT at{};ClientToScreen(child,&at);ScreenToClient(ctx.root,&at);int saved=SaveDC(ctx.dc);SetViewportOrgEx(ctx.dc,at.x,at.y,nullptr);IntersectClipRect(ctx.dc,0,0,r.right,r.bottom);
        SendMessageW(child,WM_PRINT,reinterpret_cast<WPARAM>(ctx.dc),PRF_CLIENT|PRF_ERASEBKGND);RestoreDC(ctx.dc,saved);return TRUE;
    },reinterpret_cast<LPARAM>(&context));GdiFlush();for(size_t i=0;i<size_t(r.right)*r.bottom;++i)pixels[i]|=0xff000000;
    BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);DWORD bytes=DWORD(size_t(r.right)*r.bottom*4);header.bfSize=header.bfOffBits+bytes;
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);bool ok=false;DWORD written=0;
    if(file!=INVALID_HANDLE_VALUE){ok=WriteFile(file,&header,sizeof(header),&written,nullptr)&&written==sizeof(header)&&WriteFile(file,&info.bmiHeader,sizeof(BITMAPINFOHEADER),&written,nullptr)&&written==sizeof(BITMAPINFOHEADER)&&WriteFile(file,pixels,bytes,&written,nullptr)&&written==bytes;CloseHandle(file);}
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return ok;
}
