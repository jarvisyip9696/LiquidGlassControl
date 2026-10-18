// Small GDI renderer: no continuous UI animation loop, GPU polling or web runtime.
#include <functional>
#include "smooth_motion.h"
std::function<void()> controlPainted;
// WM_PRINTCLIENT must use the same renderer as normal painting. The DC belongs
// to the caller; no screen pixels or glass windows are accessed here.
inline void printControlClient(HWND h,HDC dc){
    auto previous=GetPropW(h,L"LG.PrintDC");int saved=SaveDC(dc);SetPropW(h,L"LG.PrintDC",dc);
    SendMessageW(h,WM_PAINT,0,0);
    if(previous)SetPropW(h,L"LG.PrintDC",previous);else RemovePropW(h,L"LG.PrintDC");RestoreDC(dc,saved);
}
// Each custom control presents one complete frame, including its background.
// Fall back to the paint DC if a compatible buffer cannot be allocated.
class ControlPaint {
    HWND window;PAINTSTRUCT ps{};HDC output{},buffer{};HBITMAP bitmap{};HGDIOBJ previous{};RECT bounds{};bool printing=false;
public:
    HDC dc{};
    explicit ControlPaint(HWND h):window(h){if(auto supplied=GetPropW(h,L"LG.PrintDC")){printing=true;dc=static_cast<HDC>(supplied);GetClientRect(h,&bounds);return;}output=BeginPaint(h,&ps);dc=output;bounds=ps.rcPaint;int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;if(!output||width<=0||height<=0)return;buffer=CreateCompatibleDC(output);if(!buffer)return;bitmap=CreateCompatibleBitmap(output,width,height);if(!bitmap){DeleteDC(buffer);buffer=nullptr;return;}previous=SelectObject(buffer,bitmap);SetViewportOrgEx(buffer,-bounds.left,-bounds.top,nullptr);dc=buffer;}
    ~ControlPaint(){if(printing)return;if(buffer){if(GetGraphicsMode(buffer)==GM_ADVANCED)ModifyWorldTransform(buffer,nullptr,MWT_IDENTITY);BitBlt(output,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,buffer,bounds.left,bounds.top,SRCCOPY);SelectObject(buffer,previous);DeleteObject(bitmap);DeleteDC(buffer);}EndPaint(window,&ps);if(controlPainted)controlPainted();}
    ControlPaint(const ControlPaint&)=delete;ControlPaint&operator=(const ControlPaint&)=delete;
};
bool showFocus(HWND h){return GetFocus()==h&&!(SendMessageW(h,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS);}
HFONT headingFont{},actionFont{};HBRUSH cardBrush{};int fontLanguage=-1;
int colorTheme=0;
struct UiPalette { COLORREF accent,hover,pressed,selection,canvas,surface,edge; };
COLORREF mix(COLORREF a,COLORREF b,float t);
inline bool reducedMotion();
UiPalette previousPalette{};ULONGLONG themeStart=0;bool themeMoving=false;
const UiPalette palettes[]={
    {RGB(44,103,221),RGB(31,88,198),RGB(23,72,169),RGB(228,238,255),RGB(246,248,252),RGB(239,243,249),RGB(222,230,240)},
    {RGB(112,73,218),RGB(94,56,194),RGB(76,42,166),RGB(239,232,255),RGB(249,247,254),RGB(243,239,251),RGB(229,220,244)},
    {RGB(23,137,122),RGB(16,116,104),RGB(12,94,85),RGB(220,244,237),RGB(245,250,248),RGB(234,245,241),RGB(211,232,224)},
    {RGB(206,92,32),RGB(180,74,23),RGB(152,60,17),RGB(255,235,218),RGB(253,249,245),RGB(251,240,230),RGB(239,222,207)},
    {RGB(188,64,114),RGB(164,48,96),RGB(140,36,80),RGB(252,228,239),RGB(253,247,250),RGB(250,237,243),RGB(239,217,228)},
    {RGB(14,125,175),RGB(9,105,151),RGB(6,85,125),RGB(221,242,252),RGB(245,250,253),RGB(232,244,250),RGB(209,230,240)},
    {RGB(83,99,121),RGB(66,81,103),RGB(50,64,84),RGB(231,235,241),RGB(247,248,250),RGB(238,241,245),RGB(220,226,234)}
};
struct UiPreferences {bool contrast=false,animations=true,economical=false;};
UiPreferences readUiPreferences(){UiPreferences result;HIGHCONTRASTW contrast{};contrast.cbSize=sizeof(contrast);result.contrast=SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0)&&(contrast.dwFlags&HCF_HIGHCONTRASTON);BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);result.animations=animations!=FALSE;SYSTEM_POWER_STATUS power{};if(GetSystemPowerStatus(&power))result.economical=power.ACLineStatus==0||power.SystemStatusFlag==1;return result;}
UiPreferences&uiPreferences(){static UiPreferences cached=readUiPreferences();return cached;}
void refreshUiPreferences(){uiPreferences()=readUiPreferences();}
UINT uiFrameInterval(){return uiPreferences().economical?32u:16u;}
bool highContrast(){return uiPreferences().contrast;}
const UiPalette& palette(){static UiPalette accessible,animated;if(highContrast()){accessible={GetSysColor(COLOR_HIGHLIGHT),GetSysColor(COLOR_HIGHLIGHT),GetSysColor(COLOR_HIGHLIGHT),GetSysColor(COLOR_BTNFACE),GetSysColor(COLOR_WINDOW),GetSysColor(COLOR_BTNFACE),GetSysColor(COLOR_WINDOWTEXT)};return accessible;}if(themeMoving){float t=std::min(1.f,(GetTickCount64()-themeStart)/180.f);t=t*t*(3-2*t);auto&b=palettes[colorTheme];animated={mix(previousPalette.accent,b.accent,t),mix(previousPalette.hover,b.hover,t),mix(previousPalette.pressed,b.pressed,t),mix(previousPalette.selection,b.selection,t),mix(previousPalette.canvas,b.canvas,t),mix(previousPalette.surface,b.surface,t),mix(previousPalette.edge,b.edge,t)};return animated;}return palettes[colorTheme];}
COLORREF tint(COLORREF color,int amount){return RGB((GetRValue(color)*amount+255*(100-amount))/100,(GetGValue(color)*amount+255*(100-amount))/100,(GetBValue(color)*amount+255*(100-amount))/100);}
COLORREF cardColor(){return highContrast()?GetSysColor(COLOR_BTNFACE):tint(palette().accent,9);}
COLORREF textColor(){return highContrast()?GetSysColor(COLOR_BTNTEXT):RGB(GetRValue(palette().pressed)/3,GetGValue(palette().pressed)/3,GetBValue(palette().pressed)/3);}
void themeWindow(HWND h){
    COLORREF caption=palette().selection,text=textColor();
    DwmSetWindowAttribute(h,35,&caption,sizeof(caption));
    DwmSetWindowAttribute(h,36,&text,sizeof(text));
}
void applyColorTheme(int index,bool save){
    if(save&&mainWindow&&!reducedMotion()&&IsWindowVisible(mainWindow)){previousPalette=palette();themeStart=GetTickCount64();themeMoving=true;SetTimer(mainWindow,12,uiFrameInterval(),nullptr);}
    colorTheme=index>=0&&index<7?index:0;
    HBRUSH old=background;background=CreateSolidBrush(highContrast()?GetSysColor(COLOR_WINDOW):tint(palette().accent,18));
    HBRUSH oldCard=cardBrush;cardBrush=CreateSolidBrush(cardColor());if(oldCard)DeleteObject(oldCard);
    if(mainWindow){themeWindow(mainWindow);SetClassLongPtrW(mainWindow,GCLP_HBRBACKGROUND,reinterpret_cast<LONG_PTR>(background));RedrawWindow(mainWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_FRAME);}
    if(old)DeleteObject(old);
    if(save){auto value=std::to_wstring(colorTheme);WritePrivateProfileStringW(L"UI",L"ColorTheme",value.c_str(),iniPath.c_str());}
}
void tickColorTheme(){if(reducedMotion()||GetTickCount64()-themeStart>=180||!IsWindowVisible(mainWindow)){themeMoving=false;KillTimer(mainWindow,12);}applyColorTheme(colorTheme,false);}
struct PagePosition {HWND h;RECT rect;};std::vector<PagePosition> pagePositions;ULONGLONG pageStart=0;int pageOffset=0;
void movePageControls(int offset){
    if(offset==pageOffset)return;
    auto batch=BeginDeferWindowPos((int)pagePositions.size());
    if(batch)for(auto&p:pagePositions){batch=DeferWindowPos(batch,p.h,nullptr,p.rect.left,p.rect.top+offset,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);if(!batch)break;}
    if(!batch||!EndDeferWindowPos(batch))for(auto&p:pagePositions)SetWindowPos(p.h,nullptr,p.rect.left,p.rect.top+offset,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    pageOffset=offset;
}
void finishPageMotion(){KillTimer(mainWindow,13);movePageControls(0);pagePositions.clear();}
void beginPageMotion(){finishPageMotion();} // Stable layout; only control emphasis animates.
void tickPageMotion(){float t=std::min(1.f,(GetTickCount64()-pageStart)/160.f);int offset=(int)(MulDiv(8,dpi,96)*std::pow(1-t,3));if(t>=1||offset==0||reducedMotion()||!IsWindowVisible(mainWindow)){finishPageMotion();return;}movePageControls(offset);}
void createFonts(){
    if(fontLanguage==int(l10n::current)&&font)return;
    HFONT old=font,oldHeading=headingFont,oldAction=actionFont;
    const wchar_t*face=l10n::current==l10n::Simplified?L"Microsoft YaHei UI":l10n::current==l10n::Traditional?L"Microsoft JhengHei UI":L"Segoe UI";
    auto make=[&](int size,int weight){return CreateFontW(-MulDiv(size,dpi,96),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,face);};
    font=make(15,FW_NORMAL);headingFont=make(23,FW_SEMIBOLD);actionFont=make(15,FW_SEMIBOLD);fontLanguage=int(l10n::current);
    if(!cardBrush)cardBrush=CreateSolidBrush(RGB(255,255,255));
    EnumChildWindows(mainWindow,[](HWND child,LPARAM)->BOOL{int id=GetDlgCtrlID(child);const wchar_t*button=L"Button";wchar_t cls[32]{};GetClassNameW(child,cls,32);HFONT f=id==Header?headingFont:_wcsicmp(cls,button)==0?actionFont:font;SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(f),TRUE);return TRUE;},0);
    if(old)DeleteObject(old);if(oldHeading)DeleteObject(oldHeading);if(oldAction)DeleteObject(oldAction);
}
void destroyFonts(){if(font)DeleteObject(font);if(headingFont)DeleteObject(headingFont);if(actionFont)DeleteObject(actionFont);if(cardBrush)DeleteObject(cardBrush);font=headingFont=actionFont=nullptr;cardBrush=nullptr;}
// Shared motion for every Liquid Glass control. Each channel eases toward its target using elapsed time (exponential approach, about 200 ms to settle, no overshoot). A control owns a timer only while one of its channels is still moving; the timer stops when all channels settle, the page hides the control, or the control is destroyed.
#include <cmath>
#include <algorithm>
enum MotionChannel {kCheck=0,kPress=1,kHover=2,kSelect=3,kTabHover0=4};
constexpr int kMotionChannels=7;
const UINT_PTR kMotionTimer=0x5A1;
// Each channel moves from the value it had when its target changed, over its own duration, with an ease-in-out curve. Progress is measured from real time, so the motion does not depend on frame rate.
struct Motion {float v[kMotionChannels]{},t[kMotionChannels]{},velocity[kMotionChannels]{};LONGLONG lastTick=0;float tabWeights[3]{},tabFromWeights[3]{};int hotTab=-1;bool running=false,painted=false;};
std::set<HWND> animatedControls;
inline float channelDuration(int channel){return (channel==kHover||channel>=kTabHover0)?0.16f:channel==kPress?0.11f:channel==kSelect?0.18f:0.26f;}
inline float easeInOut(float p){return p<0.5f?4*p*p*p:1-(float)std::pow(-2*p+2,3)/2;}
inline Motion* motionOf(HWND h){auto mo=(Motion*)GetPropW(h,L"LG.Motion");if(!mo){mo=new Motion();SetPropW(h,L"LG.Motion",(HANDLE)mo);}return mo;}
// Reduced-motion and high-contrast users get the final state immediately.
inline bool reducedMotion(){return !uiPreferences().animations||uiPreferences().contrast;}
inline void motionTick(HWND h);
inline bool stepMotion(HWND h);
inline LONGLONG motionNow(){LARGE_INTEGER now{};QueryPerformanceCounter(&now);return now.QuadPart;}
inline double motionFrequency(){static const double frequency=[] {LARGE_INTEGER freq{};QueryPerformanceFrequency(&freq);return (double)freq.QuadPart;}();return frequency;}
// A new target starts from the channel's current value, so a reversal mid-motion continues from where the thumb is.
inline void setMotion(HWND h,int channel,float target){
    auto mo=motionOf(h);if(mo->t[channel]==target)return;
    if(mo->running)stepMotion(h);
    if(reducedMotion()||!mo->painted||!IsWindowVisible(h)||IsIconic(mainWindow)){mo->t[channel]=mo->v[channel]=target;mo->velocity[channel]=0;InvalidateRect(h,nullptr,FALSE);return;}
    mo->t[channel]=target;
    if(!mo->running){mo->lastTick=motionNow();bool needsTimer=animatedControls.empty();animatedControls.insert(h);mo->running=!needsTimer||SetTimer(mainWindow,kMotionTimer,uiFrameInterval(),nullptr)!=0;if(!mo->running){animatedControls.erase(h);for(int i=0;i<kMotionChannels;++i)mo->v[i]=mo->t[i];InvalidateRect(h,nullptr,FALSE);}}
}
// Returns true while any channel is still moving. Uses real elapsed time, so a slow frame does not slow the animation down.
inline bool stepMotion(HWND h){
    auto mo=motionOf(h);LONGLONG now=motionNow();float dt=mo->lastTick?float((now-mo->lastTick)/motionFrequency()):0;mo->lastTick=now;dt=std::min(.1f,std::max(0.f,dt));bool moving=false;
    for(int i=0;i<kMotionChannels;++i){
        if(mo->v[i]==mo->t[i]&&mo->velocity[i]==0)continue;
        moving=uiMotion::advance(mo->v[i],mo->velocity[i],mo->t[i],dt,channelDuration(i))||moving;
    }
    return moving;
}
inline void motionTick(HWND h){
    auto mo=motionOf(h);
    float before[kMotionChannels];std::copy(mo->v,mo->v+kMotionChannels,before);
    if(!IsWindowVisible(h)||IsIconic(mainWindow)||reducedMotion()){for(int i=0;i<kMotionChannels;++i){mo->v[i]=mo->t[i];mo->velocity[i]=0;}}
    bool moving=stepMotion(h)&&IsWindowVisible(h);bool changed=false;for(int i=0;i<kMotionChannels;++i)if(int(before[i]*255+.5f)!=int(mo->v[i]*255+.5f))changed=true;if(changed)InvalidateRect(h,nullptr,FALSE);
    if(!moving){animatedControls.erase(h);mo->running=false;}
}
inline void tickControlMotion(){for(auto i=animatedControls.begin();i!=animatedControls.end();){HWND h=*i++;if(IsWindow(h))motionTick(h);else animatedControls.erase(h);}if(animatedControls.empty())KillTimer(mainWindow,kMotionTimer);}
inline void motionRelease(HWND h){animatedControls.erase(h);if(animatedControls.empty())KillTimer(mainWindow,kMotionTimer);if(auto mo=(Motion*)GetPropW(h,L"LG.Motion")){delete mo;RemovePropW(h,L"LG.Motion");}}
inline void motionFinish(HWND h){animatedControls.erase(h);if(animatedControls.empty())KillTimer(mainWindow,kMotionTimer);if(auto mo=(Motion*)GetPropW(h,L"LG.Motion")){for(int i=0;i<kMotionChannels;++i){mo->v[i]=mo->t[i];mo->velocity[i]=0;}mo->running=false;}}
inline void resetInteraction(HWND h){RemovePropW(h,L"LG.Hover");TRACKMOUSEEVENT tracking{};tracking.cbSize=sizeof(tracking);tracking.dwFlags=TME_CANCEL|TME_LEAVE;tracking.hwndTrack=h;TrackMouseEvent(&tracking);if(auto mo=(Motion*)GetPropW(h,L"LG.Motion")){mo->hotTab=-1;for(int i=0;i<kMotionChannels;++i)if(i==kHover||i==kPress||i>=kTabHover0){mo->v[i]=mo->t[i]=0;mo->velocity[i]=0;}}InvalidateRect(h,nullptr,FALSE);}
inline void trackHover(HWND h){if(GetPropW(h,L"LG.Hover"))return;SetPropW(h,L"LG.Hover",(HANDLE)1);TRACKMOUSEEVENT tracking{};tracking.cbSize=sizeof(tracking);tracking.dwFlags=TME_LEAVE;tracking.hwndTrack=h;TrackMouseEvent(&tracking);}
inline COLORREF mix(COLORREF a,COLORREF b,float t){t=t<0?0:t>1?1:t;auto c=[&](int x,int y){return (int)(x+(y-x)*t+0.5f);};return RGB(c(GetRValue(a),GetRValue(b)),c(GetGValue(a),GetGValue(b)),c(GetBValue(a),GetBValue(b)));}
LRESULT CALLBACK buttonHover(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if((m==WM_ENABLE&&!w)||(m==WM_SHOWWINDOW&&!w)||m==WM_CANCELMODE)resetInteraction(h);
    if(m==WM_SHOWWINDOW&&!w)motionFinish(h);
    if(m==WM_SETFOCUS||m==WM_KILLFOCUS||m==WM_UPDATEUISTATE)InvalidateRect(h,nullptr,FALSE);
    if(m==WM_TIMER&&w==kMotionTimer){motionTick(h);return 0;}
    if(m==WM_NCDESTROY){motionRelease(h);RemovePropW(h,L"LG.Checked");RemovePropW(h,L"LG.IsSwitch");RemovePropW(h,L"LG.Hover");}
    if(m==WM_MOUSEMOVE&&IsWindowEnabled(h)){trackHover(h);setMotion(h,kHover,1.f);}
    if(m==WM_MOUSELEAVE){RemovePropW(h,L"LG.Hover");setMotion(h,kHover,0.f);setMotion(h,kPress,0.f);}
    if((m==WM_LBUTTONDOWN||(m==WM_KEYDOWN&&w==VK_SPACE))&&IsWindowEnabled(h))setMotion(h,kPress,1.f);
    if(m==WM_LBUTTONUP||m==WM_CAPTURECHANGED||m==WM_KILLFOCUS||(m==WM_KEYUP&&w==VK_SPACE))setMotion(h,kPress,0.f);
    return DefSubclassProc(h,m,w,l);
}
LRESULT CALLBACK listHover(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data){
    if(m==WM_MOUSEMOVE){LRESULT hit=SendMessageW(h,LB_ITEMFROMPOINT,0,l);INT_PTR next=HIWORD(hit)?0:LOWORD(hit)+1;if((INT_PTR)GetPropW(h,L"LG.HotRow")!=next){SetPropW(h,L"LG.HotRow",(HANDLE)next);InvalidateRect(h,nullptr,FALSE);}}
    if(m==WM_MOUSELEAVE){RemovePropW(h,L"LG.HotRow");InvalidateRect(h,nullptr,FALSE);}
    if(m==WM_NCDESTROY)RemovePropW(h,L"LG.HotRow");
    return buttonHover(h,m,w,l,id,data);
}
void roundFill(HDC dc,RECT r,COLORREF fill,COLORREF edge,int radius,int stroke=1){
    if(r.right<=r.left||r.bottom<=r.top)return;
    HBRUSH b=CreateSolidBrush(fill);HPEN p=CreatePen(PS_SOLID,stroke,edge);auto oldBrush=SelectObject(dc,b),oldPen=SelectObject(dc,p);RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(b);DeleteObject(p);
}
void drawFocusRing(HDC dc,RECT r){
    InflateRect(&r,-2,-2);if(r.right<=r.left||r.bottom<=r.top)return;
    HPEN pen=CreatePen(PS_SOLID,std::max(1,MulDiv(2,dpi,96)),highContrast()?GetSysColor(COLOR_HIGHLIGHT):palette().accent);
    auto oldPen=SelectObject(dc,pen),oldBrush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));int radius=MulDiv(10,dpi,96);RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);
}
void drawChevron(HDC dc,RECT r,COLORREF color,bool open){int cx=(r.left+r.right)/2,cy=(r.top+r.bottom)/2,half=std::max(2,MulDiv(4,dpi,96)),rise=std::max(1,MulDiv(2,dpi,96));HPEN pen=CreatePen(PS_SOLID,std::max(1,MulDiv(2,dpi,96)),color);auto old=SelectObject(dc,pen);MoveToEx(dc,cx-half,cy+(open?rise:-rise),nullptr);LineTo(dc,cx,cy+(open?-rise:rise));LineTo(dc,cx+half,cy+(open?rise:-rise));SelectObject(dc,old);DeleteObject(pen);}
void drawThemeSwatch(HDC dc,RECT&text,int index){if(index<0||index>=7)return;int size=MulDiv(12,dpi,96),cy=(text.top+text.bottom)/2;RECT swatch{text.left,cy-size/2,text.left+size,cy+(size+1)/2};roundFill(dc,swatch,palettes[index].accent,highContrast()?GetSysColor(COLOR_WINDOWTEXT):tint(palettes[index].pressed,70),size);text.left+=MulDiv(24,dpi,96);}
void paintCompactSwitch(HWND h,HDC dc){RECT r{};GetClientRect(h,&r);FillRect(dc,&r,cardBrush);
        auto px=[](int n){return MulDiv(n,dpi,96);};bool active=IsWindowEnabled(h),focused=showFocus(h);Motion*mo=motionOf(h);mo->painted=true;
        float check=mo->v[kCheck],press=active?mo->v[kPress]:0.f,hover=active?mo->v[kHover]:0.f;bool on=SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED;
        // Rasterize the small track with fractional edge coverage. GDI's
        // integer RoundRect/Ellipse otherwise produces jagged outlines at DPI
        // scaling, especially while the thumb moves between physical pixels.
        int w=std::min(px(58),int(r.right)),hgt=std::min(px(36),int(r.bottom));if(w<1||hgt<1)return;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-hgt;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        std::vector<unsigned long> pixels(size_t(w)*hgt);
        float unit=dpi/96.f,scale=1.f+.06f*hover-.035f*press,cx=28*unit,cy=hgt/2.f;
        COLORREF rest=active?RGB(155,165,181):RGB(200,206,215),fill=mix(rest,palette().accent,active?check:0.f);
        float thumbWidth=(22-3*press)*unit,thumbX=(5+24*check)*unit+(22*unit-thumbWidth)/2;
        auto coverage=[](float distance){return std::max(0.f,std::min(1.f,.5f-distance));};
        auto rgb=[](COLORREF c){return (unsigned long)((GetRValue(c)<<16)|(GetGValue(c)<<8)|GetBValue(c));};
        for(int y=0;y<hgt;++y)for(int x=0;x<w;++x){
            float xx=cx+(x+.5f-cx)/scale,yy=cy+(y+.5f-cy)/scale;
            float nearest=std::max(16*unit,std::min(40*unit,xx));
            float dx=xx-nearest,dy=yy-cy;
            float track=(std::sqrt(dx*dx+dy*dy)-14*unit)*scale;
            COLORREF color=mix(cardColor(),fill,coverage(track));
            if(focused)color=mix(color,highContrast()?GetSysColor(COLOR_HIGHLIGHT):palette().accent,coverage(track)-coverage(track+2*unit));
            float tx=(xx-(thumbX+thumbWidth/2))/(thumbWidth/2),ty=(yy-cy)/(11*unit);
            float thumb=(std::sqrt(tx*tx+ty*ty)-1)*std::min(thumbWidth/2,11*unit)*scale;
            color=mix(color,RGB(255,255,255),coverage(thumb));pixels[size_t(y)*w+x]=rgb(color);
        }
        SetDIBitsToDevice(dc,0,0,w,hgt,0,0,0,hgt,pixels.data(),&info,DIB_RGB_COLORS);
        RECT caption=r;caption.left=px(62);wchar_t label[512]{};if(r.right>px(150))GetWindowTextW(h,label,512);SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,active?textColor():GetSysColor(COLOR_GRAYTEXT));DrawTextW(dc,*label?label:on?tr(L"开",L"On"):tr(L"关",L"Off"),-1,&caption,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
}
LRESULT CALLBACK compactSwitch(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if(m==WM_PRINTCLIENT){printControlClient(h,reinterpret_cast<HDC>(w));return 0;}
    if((m==WM_ENABLE&&!w)||(m==WM_SHOWWINDOW&&!w)||m==WM_CANCELMODE)resetInteraction(h);
    if(m==WM_SHOWWINDOW&&!w)motionFinish(h);
    if(m==WM_TIMER&&w==kMotionTimer){motionTick(h);return 0;}
    if(m==WM_NCDESTROY){motionRelease(h);RemovePropW(h,L"LG.Checked");RemovePropW(h,L"LG.IsSwitch");RemovePropW(h,L"LG.Hover");}
    if(m==WM_ERASEBKGND)return 1;
    if(m==WM_PAINT){ControlPaint paint(h);paintCompactSwitch(h,paint.dc);return 0;}
    if(m==BM_GETCHECK)return GetPropW(h,L"LG.Checked")?BST_CHECKED:BST_UNCHECKED;
    if(m==BM_SETCHECK){if(w==BST_CHECKED)SetPropW(h,L"LG.Checked",(HANDLE)1);else RemovePropW(h,L"LG.Checked");setMotion(h,kCheck,w==BST_CHECKED?1.f:0.f);InvalidateRect(h,nullptr,FALSE);return 0;}

    if(m==WM_MOUSEMOVE&&IsWindowEnabled(h)){trackHover(h);setMotion(h,kHover,1.f);}
    if(m==WM_MOUSELEAVE){RemovePropW(h,L"LG.Hover");setMotion(h,kHover,0.f);setMotion(h,kPress,0.f);}
    if((m==WM_LBUTTONDOWN||(m==WM_KEYDOWN&&w==VK_SPACE))&&IsWindowEnabled(h))setMotion(h,kPress,1.f);
    if(m==WM_LBUTTONUP||m==WM_CAPTURECHANGED||m==WM_KILLFOCUS||(m==WM_KEYUP&&w==VK_SPACE))setMotion(h,kPress,0.f);
    auto result=DefSubclassProc(h,m,w,l);
    // Commands update the custom checked state; native checkbox rendering is never used.
    if(m==WM_LBUTTONUP||m==WM_KEYUP||m==BM_CLICK||m==BM_SETCHECK)setMotion(h,kCheck,SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED?1.f:0.f);
    if(m==WM_UPDATEUISTATE||m==BM_SETCHECK||m==WM_ENABLE||m==WM_SETFOCUS||m==WM_KILLFOCUS||m==WM_KEYUP||m==WM_LBUTTONUP)InvalidateRect(h,nullptr,FALSE);
    return result;
}
bool drawModernControl(DRAWITEMSTRUCT* item);
LRESULT CALLBACK themedControl(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR comboControl){
    if(m==WM_PRINTCLIENT){printControlClient(h,reinterpret_cast<HDC>(w));return 0;}
    if((m==WM_ENABLE&&!w)||(m==WM_SHOWWINDOW&&!w)||m==WM_CANCELMODE)resetInteraction(h);
    if(m==WM_SHOWWINDOW&&!w)motionFinish(h);
    if(m==WM_TIMER&&w==kMotionTimer){motionTick(h);return 0;}
    if(m==WM_NCDESTROY){motionRelease(h);RemovePropW(h,L"LG.Checked");RemovePropW(h,L"LG.IsSwitch");RemovePropW(h,L"LG.Hover");}
    if(comboControl){
        if(m==WM_MOUSEMOVE&&IsWindowEnabled(h)){trackHover(h);setMotion(h,kHover,1.f);}
        if(m==WM_MOUSELEAVE){RemovePropW(h,L"LG.Hover");setMotion(h,kHover,0.f);}
    }else{
        if((m==WM_MOUSEMOVE||m==WM_LBUTTONDOWN||m==WM_LBUTTONUP)&&IsWindowEnabled(h)){trackHover(h);TCHITTESTINFO hit{};hit.pt={static_cast<short>(LOWORD(l)),static_cast<short>(HIWORD(l))};int index=TabCtrl_HitTest(h,&hit);auto mo=motionOf(h);if(mo->hotTab!=index){mo->hotTab=index;InvalidateRect(h,nullptr,FALSE);}for(int i=0;i<3;++i)setMotion(h,kTabHover0+i,index==i?1.f:0.f);}
        if(m==WM_MOUSELEAVE){RemovePropW(h,L"LG.Hover");motionOf(h)->hotTab=-1;for(int i=0;i<3;++i)setMotion(h,kTabHover0+i,0.f);InvalidateRect(h,nullptr,FALSE);}
    }
    if(!comboControl&&m==WM_PAINT){ControlPaint paint(h);HDC dc=paint.dc;RECT r;GetClientRect(h,&r);FillRect(dc,&r,(h==settingsTabs||h==systemTabs)?cardBrush:background);for(int i=0;i<TabCtrl_GetItemCount(h);++i){DRAWITEMSTRUCT item{};item.CtlType=ODT_TAB;item.CtlID=GetDlgCtrlID(h);item.itemID=i;item.hwndItem=h;item.hDC=dc;item.itemAction=ODA_DRAWENTIRE;if(showFocus(h)&&TabCtrl_GetCurFocus(h)==i)item.itemState=ODS_FOCUS;TabCtrl_GetItemRect(h,i,&item.rcItem);drawModernControl(&item);}return 0;}
    if(m==WM_ERASEBKGND){RECT r;GetClientRect(h,&r);FillRect(reinterpret_cast<HDC>(w),&r,background);return 1;}
    if(m==WM_PAINT&&comboControl){
        ControlPaint paint(h);HDC dc=paint.dc;RECT r;GetClientRect(h,&r);
        // Same hover treatment as the push buttons: the surface lifts toward the selection colour and the border takes the accent.
        // Clear the complete buffer first: RoundRect leaves its corners alone.
        // Uninitialized corner pixels were the black triangles in the dropdowns.
        FillRect(dc,&r,cardBrush);float hover=IsWindowEnabled(h)?motionOf(h)->v[kHover]:0.f;motionOf(h)->painted=true;
        int inset=MulDiv(2,dpi,96)-(int)(MulDiv(1,dpi,96)*hover);InflateRect(&r,-inset,-inset);
        COLORREF fill=mix(cardColor(),palette().selection,hover);roundFill(dc,r,fill,mix(fill,palette().accent,hover),MulDiv(8,dpi,96),std::max(1,MulDiv(1,dpi,96)));
        RECT text=r;text.left+=MulDiv(8,dpi,96);text.right-=MulDiv(25,dpi,96);
        if(GetDlgCtrlID(h)==ColorTheme)drawThemeSwatch(dc,text,selection(h));
        wchar_t label[256]{};GetWindowTextW(h,label,256);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,IsWindowEnabled(h)?textColor():GetSysColor(COLOR_GRAYTEXT));auto oldFont=SelectObject(dc,font);
        DrawTextW(dc,label,-1,&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        RECT arrow=r;arrow.left=arrow.right-MulDiv(24,dpi,96);drawChevron(dc,arrow,IsWindowEnabled(h)?textColor():GetSysColor(COLOR_GRAYTEXT),SendMessageW(h,CB_GETDROPPEDSTATE,0,0)!=0);
        if(showFocus(h))drawFocusRing(dc,r);SelectObject(dc,oldFont);return 0;
    }
    auto result=DefSubclassProc(h,m,w,l);
    if(comboControl&&(m==WM_SETFOCUS||m==WM_KILLFOCUS||m==CB_SETCURSEL||m==CB_SHOWDROPDOWN||m==WM_KEYUP||m==WM_LBUTTONUP||m==WM_ENABLE||m==WM_UPDATEUISTATE))InvalidateRect(h,nullptr,FALSE);
    return result;
}
// Retain the native trackbar's keyboard, accessibility and hit testing. Only
// drawing is replaced; dragging follows the pointer, keyboard changes ease.
LRESULT CALLBACK modernSlider(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR){
    if(m==WM_PRINTCLIENT){printControlClient(h,reinterpret_cast<HDC>(w));return 0;}
    if((m==WM_ENABLE&&!w)||(m==WM_SHOWWINDOW&&!w)||m==WM_CANCELMODE)resetInteraction(h);
    if(m==WM_SHOWWINDOW&&!w)motionFinish(h);
    if(m==WM_TIMER&&w==kMotionTimer){motionTick(h);return 0;}
    if(m==WM_NCDESTROY){motionRelease(h);RemovePropW(h,L"LG.Checked");RemovePropW(h,L"LG.IsSwitch");RemovePropW(h,L"LG.Hover");}
    if(m==WM_MOUSEMOVE&&IsWindowEnabled(h)){trackHover(h);setMotion(h,kHover,1);}
    if(m==WM_MOUSELEAVE){RemovePropW(h,L"LG.Hover");setMotion(h,kHover,0);}
    if(m==WM_LBUTTONDOWN&&IsWindowEnabled(h))setMotion(h,kPress,1);
    if(m==WM_LBUTTONUP||m==WM_CAPTURECHANGED||m==WM_KILLFOCUS)setMotion(h,kPress,0);
    if(m==WM_ERASEBKGND)return 1;
    if(m==WM_PAINT){ControlPaint paint(h);HDC dc=paint.dc;RECT r{},track{},thumb{};GetClientRect(h,&r);FillRect(dc,&r,cardBrush);SendMessageW(h,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&track));SendMessageW(h,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
        auto mo=motionOf(h);int lo=(int)SendMessageW(h,TBM_GETRANGEMIN,0,0),hi=(int)SendMessageW(h,TBM_GETRANGEMAX,0,0),pos=(int)SendMessageW(h,TBM_GETPOS,0,0);float target=hi>lo?float(pos-lo)/(hi-lo):0;
        setMotion(h,kCheck,target);mo->painted=true;if(GetCapture()==h)mo->v[kCheck]=mo->t[kCheck];
        int cy=(thumb.top+thumb.bottom)/2,half=(thumb.right-thumb.left)/2;
        int left=track.left+half,right=track.right-half,x=left+(int)((right-left)*mo->v[kCheck]);int thickness=std::max(2,MulDiv(4,dpi,96));
        RECT rail{left,cy-thickness/2,right,cy+thickness/2};roundFill(dc,rail,palette().edge,palette().edge,thickness);RECT fill=rail;fill.right=x;COLORREF accent=IsWindowEnabled(h)?palette().accent:GetSysColor(COLOR_GRAYTEXT);roundFill(dc,fill,accent,accent,thickness);
        int radius=MulDiv(8,dpi,96)+(int)(MulDiv(2,dpi,96)*mo->v[kHover])-(int)(MulDiv(1,dpi,96)*mo->v[kPress]);RECT knob{x-radius,cy-radius,x+radius,cy+radius};roundFill(dc,knob,highContrast()?GetSysColor(COLOR_WINDOW):RGB(255,255,255),accent,2*radius);
        if(showFocus(h)){InflateRect(&r,-1,-1);drawFocusRing(dc,r);}return 0;}
    auto result=DefSubclassProc(h,m,w,l);if(m==TBM_SETPOS||m==WM_KEYDOWN||m==WM_MOUSEMOVE||m==WM_SETFOCUS||m==WM_KILLFOCUS||m==WM_UPDATEUISTATE||m==WM_ENABLE)InvalidateRect(h,nullptr,FALSE);return result;
}
bool drawModernControl(DRAWITEMSTRUCT* item){
    if(!item)return false;HDC dc=item->hDC;RECT r=item->rcItem;if(item->CtlType==ODT_BUTTON&&GetPropW(item->hwndItem,L"LG.IsSwitch")){int saved=SaveDC(dc);paintCompactSwitch(item->hwndItem,dc);RestoreDC(dc,saved);return true;}
    if(item->CtlType==ODT_COMBOBOX){int saved=SaveDC(dc);bool selected=(item->itemState&ODS_SELECTED)!=0;HBRUSH brush=CreateSolidBrush(selected?palette().selection:cardColor());FillRect(dc,&r,brush);DeleteObject(brush);if(item->itemID!=static_cast<UINT>(-1)){wchar_t label[512]{};SendMessageW(item->hwndItem,CB_GETLBTEXT,item->itemID,reinterpret_cast<LPARAM>(label));r.left+=MulDiv(8,dpi,96);if(GetDlgCtrlID(item->hwndItem)==ColorTheme)drawThemeSwatch(dc,r,(int)item->itemID);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,textColor());SelectObject(dc,font);DrawTextW(dc,label,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);}RestoreDC(dc,saved);return true;}
    if(item->CtlType==ODT_LISTBOX){int saved=SaveDC(dc);bool selected=(item->itemState&ODS_SELECTED)!=0;auto mo=motionOf(item->hwndItem);mo->painted=true;bool hot=(INT_PTR)GetPropW(item->hwndItem,L"LG.HotRow")==INT_PTR(item->itemID)+1;HBRUSH b=CreateSolidBrush(selected?palette().selection:mix(cardColor(),palette().selection,hot?mo->v[kHover]:0));FillRect(dc,&r,b);DeleteObject(b);if(item->itemID<entries.size()){auto&e=entries[item->itemID];int iconSize=MulDiv(28,dpi,96),gap=MulDiv(10,dpi,96);if(e.icon)DrawIconEx(dc,r.left+gap,r.top+(r.bottom-r.top-iconSize)/2,e.icon,iconSize,iconSize,0,nullptr,DI_NORMAL);RECT text=r;text.left+=iconSize+2*gap;text.top+=MulDiv(3,dpi,96);text.bottom=text.top+MulDiv(23,dpi,96);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,textColor());SelectObject(dc,actionFont);DrawTextW(dc,e.exe.c_str(),-1,&text,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);text.top+=MulDiv(21,dpi,96);text.bottom=r.bottom;SetTextColor(dc,palette().pressed);SelectObject(dc,font);DrawTextW(dc,e.title.c_str(),-1,&text,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);}if((item->itemState&ODS_FOCUS)&&!(item->itemState&ODS_NOFOCUSRECT)){InflateRect(&r,-2,-2);drawFocusRing(dc,r);}RestoreDC(dc,saved);return true;}
    bool tab=item->CtlType==ODT_TAB;if(!tab&&item->CtlType!=ODT_BUTTON)return false;int saved=SaveDC(dc);
    bool primary=tab?(int)item->itemID==(item->hwndItem==settingsTabs?settingsPage:currentPage):(item->CtlID==Apply||item->CtlID==RestoreOriginal);
    if(!tab){if(item->CtlID==VolumeOn)primary=volumeOn;if(item->CtlID==BrightnessOn)primary=brightnessOn;}
    bool disabled=(item->itemState&ODS_DISABLED)!=0;
    Motion*mo=motionOf(item->hwndItem);mo->painted=true;float hoverAmount=disabled?0.f:mo->v[kHover],pressAmount=disabled?0.f:mo->v[kPress];
    // Fade selection between the old and new tabs. Spatial overlap used to
    // light up an unrelated middle tab when jumping across two positions.
    float tabCover=0.f;
    if(tab){
        int selected=item->hwndItem==systemTabs?systemSection:item->hwndItem==settingsTabs?(settingsPage==2?1:0):currentPage;
        int last=(int)(INT_PTR)GetPropW(item->hwndItem,L"LG.Sel")-1;
        if(last!=selected){
            if(last>=0){stepMotion(item->hwndItem);float progress=mo->v[kSelect];for(int i=0;i<3;++i)mo->tabWeights[i]=mo->tabFromWeights[i]+((i==last?1.f:0.f)-mo->tabFromWeights[i])*progress;}
            std::copy(mo->tabWeights,mo->tabWeights+3,mo->tabFromWeights);
            SetPropW(item->hwndItem,L"LG.Sel",(HANDLE)(INT_PTR)(selected+1));
            if(last<0)mo->v[kSelect]=mo->t[kSelect]=1.f;
            else{mo->v[kSelect]=mo->t[kSelect]=0.f;setMotion(item->hwndItem,kSelect,1.f);}
        }
        int index=(int)item->itemID;
        tabCover=mo->tabFromWeights[index]+((index==selected?1.f:0.f)-mo->tabFromWeights[index])*mo->v[kSelect];
        mo->tabWeights[index]=tabCover;
    }
    HBRUSH base=CreateSolidBrush(tab?((item->hwndItem==settingsTabs||item->hwndItem==systemTabs)?cardColor():highContrast()?GetSysColor(COLOR_WINDOW):tint(palette().accent,18)):cardColor());FillRect(dc,&r,base);DeleteObject(base);
    if(tab)hoverAmount=disabled?0.f:mo->v[kTabHover0+(int)item->itemID];
    // Stable geometry keeps text and edges crisp while colors ease.
    int inset=MulDiv(2,dpi,96);InflateRect(&r,-inset,-inset);
    COLORREF fill=primary?palette().accent:palette().surface,edge=primary?palette().accent:mix(palette().edge,palette().accent,.25f);
    if(tab&&!disabled){fill=mix(palette().surface,palette().accent,tabCover);fill=mix(fill,tabCover>.5f?palette().hover:palette().selection,hoverAmount*.25f);}
    if(tab)edge=fill;
    if(!tab&&!disabled){COLORREF hot=primary?palette().hover:palette().selection,hotEdge=primary?hot:palette().accent;fill=mix(fill,hot,hoverAmount);edge=mix(edge,hotEdge,hoverAmount);}
    if(!tab&&!disabled&&pressAmount>0.f)fill=mix(fill,primary?palette().pressed:palette().selection,pressAmount);
    // The one-DIP palette outline fades in only while this control is hovered.
    // Matching the fill at rest removes the border without changing geometry.
    // The old tab may still ease back to its resting size, but its outline
    // must stop immediately once the pointer has moved to another tab.
    float outlineAmount=tab&&mo->hotTab!=(int)item->itemID?0.f:hoverAmount;
    edge=mix(fill,palette().accent,outlineAmount);
    if(disabled){fill=highContrast()?GetSysColor(COLOR_BTNFACE):RGB(239,241,245);edge=fill;}
    roundFill(dc,r,fill,edge,MulDiv(14,dpi,96),std::max(1,MulDiv(1,dpi,96)));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,disabled?GetSysColor(COLOR_GRAYTEXT):tab?mix(textColor(),highContrast()?GetSysColor(COLOR_HIGHLIGHTTEXT):RGB(255,255,255),tabCover):primary?(highContrast()?GetSysColor(COLOR_HIGHLIGHTTEXT):RGB(255,255,255)):textColor());SelectObject(dc,actionFont?actionFont:font);
    wchar_t text[256]{};if(tab){TCITEMW entry{};entry.mask=TCIF_TEXT;entry.pszText=text;entry.cchTextMax=256;TabCtrl_GetItem(item->hwndItem,(int)item->itemID,&entry);}else GetWindowTextW(item->hwndItem,text,256);
    DrawTextW(dc,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
    if((item->itemState&ODS_FOCUS)&&!(item->itemState&ODS_NOFOCUSRECT))drawFocusRing(dc,r);RestoreDC(dc,saved);return true;
}
void paintChrome(HWND h){
    ControlPaint paint(h);HDC dc=paint.dc;RECT r;GetClientRect(h,&r);if(r.right<=0||r.bottom<=0)return;FillRect(dc,&r,background);
    auto px=[](int v){return MulDiv(v,dpi,96);};RECT card={px(16),px(96),r.right-px(16),px(544)};roundFill(dc,card,cardColor(),palette().edge,px(22));
    HICON icon=(HICON)GetClassLongPtrW(h,GCLP_HICON);if(icon)DrawIconEx(dc,px(24),px(17),icon,px(32),px(32),0,nullptr,DI_NORMAL);
}
