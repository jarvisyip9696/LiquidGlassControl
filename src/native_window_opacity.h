#pragma once
#include <windows.h>
#include <winrt/base.h>
#include <stdexcept>
namespace liquid {
// A nonzero HWND alpha removes residual native backing while retaining native
// hit testing. This is leased, process-local state, never a persisted setting.
class NativeWindowOpacityLease {
    HWND window{};bool owned=false,wasLayered=false;COLORREF oldKey=0;BYTE oldAlpha=255;DWORD oldFlags=LWA_ALPHA;
    static constexpr wchar_t Tag[]=L"LiquidGlass.Taskbar.NativeOpacity.Owner";
public:
    explicit NativeWindowOpacityLease(HWND value):window(value){}
    void prepare(){
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId()||!IsWindow(window))throw winrt::hresult_error(E_ACCESSDENIED);
        if(owned){COLORREF key=0;BYTE alpha=0;DWORD flags=0;if(GetPropW(window,Tag)!=this||!GetLayeredWindowAttributes(window,&key,&alpha,&flags)||key!=0||alpha!=1||flags!=LWA_ALPHA)throw winrt::hresult_error(E_ABORT);return;}
        if(GetPropW(window,Tag))throw winrt::hresult_error(E_ABORT);
        auto style=GetWindowLongPtrW(window,GWL_EXSTYLE);wasLayered=(style&WS_EX_LAYERED)!=0;
        if(wasLayered&&!GetLayeredWindowAttributes(window,&oldKey,&oldAlpha,&oldFlags))throw winrt::hresult_error(E_NOTIMPL);
        if(!SetPropW(window,Tag,this))winrt::throw_last_error();
        if(!wasLayered)SetWindowLongPtrW(window,GWL_EXSTYLE,style|WS_EX_LAYERED);
        owned=true;
        if(!(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_LAYERED)||!SetLayeredWindowAttributes(window,0,1,LWA_ALPHA)){auto error=GetLastError();restore();throw winrt::hresult_error(HRESULT_FROM_WIN32(error?error:ERROR_NOT_SUPPORTED));}
    }
    void restore()noexcept{
        if(!owned)return;owned=false;if(!IsWindow(window)||GetPropW(window,Tag)!=this)return;
        COLORREF key=0;BYTE alpha=0;DWORD flags=0;
        if(GetLayeredWindowAttributes(window,&key,&alpha,&flags)&&key==0&&alpha==1&&flags==LWA_ALPHA){
            if(wasLayered)SetLayeredWindowAttributes(window,oldKey,oldAlpha,oldFlags);
            else SetWindowLongPtrW(window,GWL_EXSTYLE,GetWindowLongPtrW(window,GWL_EXSTYLE)&~LONG_PTR(WS_EX_LAYERED));
            RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN);
        }
        RemovePropW(window,Tag);
    }
    ~NativeWindowOpacityLease(){restore();}
};
}
