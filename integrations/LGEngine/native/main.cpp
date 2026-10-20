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
#include <dwmapi.h>
#pragma comment(lib,"dwmapi.lib")
#include "../client/lg_client.h"
#include "../../../src/lgengine_preview.h"
#include "osd.h"
#include "../../../src/optical_labels.h"

namespace {
constexpr UINT HostEvent=WM_APP+60,OsdEvent=WM_APP+61,MsgVolume=WM_APP+62,MsgDevice=WM_APP+63,MsgOwner=WM_APP+64,MsgForeground=WM_APP+65,MsgFlyout=WM_APP+66,TitleEvent=WM_APP+67;
enum : int {IdArrival=101,IdIndicator,IdSlosh,IdKnob,IdReplay,IdSample,IdOn,IdLabel1=111,IdLabel2,IdLabel3,IdLabel4};
enum : UINT_PTR {TimerAnim=2,TimerHide=3,TimerReplay=4,TimerRecheck=5,TimerTitle=6};
// Language order everywhere: English, Simplified Chinese, Traditional Chinese, Polish, Vietnamese.
using namespace opticalui;
HWND gWindow{},gOwner{};
void CALLBACK ownerEvent(HWINEVENTHOOK,DWORD,HWND hwnd,LONG object,LONG,DWORD,DWORD){if(hwnd&&hwnd==gOwner&&object==OBJID_WINDOW&&gWindow)PostMessageW(gWindow,MsgOwner,0,0);}
void CALLBACK foregroundEvent(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD){if(gWindow)PostMessageW(gWindow,MsgForeground,0,0);}
// Explorer showed one of its island windows: it may be Windows' own volume/brightness pop-up, appearing for the first time.
void CALLBACK flyoutEvent(HWINEVENTHOOK,DWORD,HWND hwnd,LONG object,LONG child,DWORD,DWORD){wchar_t cls[64]{};if(hwnd&&object==OBJID_WINDOW&&child==CHILDID_SELF&&gWindow&&GetClassNameW(hwnd,cls,64)&&!wcscmp(cls,lgpreview::FlyoutClass))PostMessageW(gWindow,MsgFlyout,0,0);}
VOID CALLBACK parentGone(PVOID window,BOOLEAN){PostMessageW(static_cast<HWND>(window),WM_CLOSE,0,0);}
// Title-bar strip of a window: frame = visible bounds (DWM), window = GetWindowRect, buttons = caption button bounds (relative to the window rect). Pure, so selfTest can call it.
RECT titleBar(RECT frame,RECT window,RECT buttons,bool haveButtons,UINT dpi,int inset){
    int bottom=haveButtons&&buttons.bottom>buttons.top?window.top+buttons.bottom:frame.top+GetSystemMetricsForDpi(SM_CYCAPTION,dpi)+GetSystemMetricsForDpi(SM_CYFRAME,dpi)+GetSystemMetricsForDpi(SM_CXPADDEDBORDER,dpi);
    int lo=MulDiv(16,int(dpi),96),hi=MulDiv(64,int(dpi),96),h=std::max<int>(lo,std::min<int>(hi,int(bottom-frame.top)));RECT r{frame.left,frame.top,frame.right,frame.top+h};
    int i=MulDiv(inset,int(dpi),96),minH=MulDiv(12,int(dpi),96),vi=h>=minH?std::min<int>(i,(h-minH)/2):0,hi2=std::min<int>(i,std::max<int>(0,int(r.right-r.left-1)/2));
    r.left+=hi2;r.right-=hi2;r.top+=vi;r.bottom-=vi;return r;}
void CALLBACK titleEvent(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD);
struct App {
    HWND window{},owner{};lghost::Handle parent;lgpreview::Settings settings;
    std::unique_ptr<lghost::Client> client,osdClient;lghost::Spec spec,lens;bool visible=true,failed=false,pageEnabled=true,hostSeen=false,cardPlaced=false,lensPlaced=false;UINT specDpi=0;
    // lensOptions has no host path: only the main client launches the host, so the two clients never race to start two.
    // lensEver: the lens client connected once (it then reconnects by itself). lensRejected: connected, but the host refused its Create.
    lghost::Options lensOptions;bool lensEver=false,lensRejected=false;
    // Starts the lens client once the main client is connected, and again after a lens client that gave up without ever connecting.
    bool startLens(){if(osdClient&&(lensEver||osdClient->connected()))return false;if(osdLive())osdStop();osdClient.reset();lensRejected=false;
        try{osdClient=std::make_unique<lghost::Client>(lensOptions,lens);}catch(...){return false;}apply();return true;}
    bool lensAtCard=false,deferring=false; // lensAtCard: the Sample button's sweep sits under the card, real level changes where Windows shows its indicator; deferring: Liquid Glass shows its own glass indicator, so the lens stands down
    POINT cardAt{},lensAt{};
    UINT dpi()const{return owner?GetDpiForWindow(owner):GetDpiForWindow(window);}
    bool rescale(){UINT d=dpi();if(!d||d==specDpi)return false;specDpi=d;spec.scale=d/96.f;spec.width=MulDiv(420,int(d),96);spec.height=MulDiv(owner?64:200,int(d),96);return true;}
    bool osdLive()const{return osdShown||osdAnimating||state.sampling;}
    HANDLE wait{};HWINEVENTHOOK hooks[5]{};HPOWERNOTIFY power{},power2{};DWORD displayState=1;ULONGLONG quietUntil=0;HFONT uiFont{};DWORD ownerPid{};unsigned labelLang=99;
    osd::State state;osd::AudioWatch audio;std::vector<uint32_t> osdFrame;osd::Slider slider;int drawnNumber=-1,drawnW=0,drawnH=0;float drawnK=0;wchar_t drawnGlyph=0;
    lghost::Look lensLook{5,32,90,68,30,24,100,50};int lensShape=0;ULONGLONG kickUntil=0; // the lens's own look and profile (Controls.shape), and when the last kick's slosh is over
    bool osdAnimating=false,osdShown=false,osdIn=false;bool osdDirty=false;LONGLONG lastTick=0; // osdIn: the lens is fully in (the Shown event), which is when the owner kicks it
    const wchar_t* title(unsigned event)const{
        static const wchar_t* connected[]={L"LGEngine host connected — drag to move",L"LGEngine 宿主已连接 — 拖动移动",L"LGEngine 宿主已連線 — 拖動移動",L"Połączono z LGEngine — przeciągnij",L"Đã kết nối LGEngine — kéo để di chuyển"};
        static const wchar_t* lost[]={L"LGEngine disconnected — reconnecting",L"LGEngine 已断开 — 正在重连",L"LGEngine 已中斷 — 正在重新連線",L"LGEngine rozłączony — ponowne łączenie",L"LGEngine ngắt kết nối — đang kết nối lại"};
        static const wchar_t* failure[]={L"LGEngine host unavailable",L"LGEngine 宿主不可用",L"LGEngine 宿主無法使用",L"Host LGEngine niedostępny",L"LGEngine host không khả dụng"};
        return (event==lghost::Lost?lost:event==lghost::Failed?failure:connected)[settings.language];
    }
    int sel(int id)const{LRESULT n=SendDlgItemMessageW(window,id,CB_GETCURSEL,0,0);return n<0?0:int(n);}
    bool reduced()const{return settings.reducedMotion!=0;}
    bool indicatorOn()const{return IsDlgButtonChecked(window,IdOn)==BST_CHECKED;}
    void place(){RECT r{};GetWindowRect(window,&r);spec.x=r.left;spec.y=r.bottom+8;if(owner){POINT p{MulDiv(40,int(dpi()),96),MulDiv(365,int(dpi()),96)};ClientToScreen(owner,&p);spec.x=p.x;spec.y=p.y;
            // Keep the card off the owner's sliders: outside its window, in its monitor's work area, with room for card+gap+lens.
            RECT R{};MONITORINFO mi{};mi.cbSize=sizeof(mi);if(GetWindowRect(owner,&R)&&GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&mi)){
                const RECT& W=mi.rcWork;int g=int(std::lround(8*spec.scale)),total=spec.height+g+(osdClient?lens.height:0),cx=(R.left+R.right-spec.width)/2,x,y;
                auto fits=[&](int px,int py){return px>=W.left&&py>=W.top&&px+spec.width<=W.right&&py+total<=W.bottom;};
                if(fits(cx,R.bottom+g)){x=cx;y=R.bottom+g;}else if(fits(cx,R.top-g-total)){x=cx;y=R.top-g-total;}else if(fits(R.right+g,R.top)){x=R.right+g;y=R.top;}else if(fits(R.left-g-spec.width,R.top)){x=R.left-g-spec.width;y=R.top;}else{x=cx;y=W.bottom-total;}
                x=std::max<int>(W.left,std::min<int>(x,W.right-spec.width));y=std::max<int>(W.top,std::min<int>(y,W.bottom-total));spec.x=x;spec.y=y;}}
        // Foreground and setting changes call this often: send a place only when it moved.
        if(client&&(!cardPlaced||cardAt.x!=spec.x||cardAt.y!=spec.y)){cardPlaced=true;cardAt=POINT{spec.x,spec.y};client->setPlace(spec.x,spec.y);}
        if(osdClient){lens.x=spec.x+(spec.width-lens.width)/2;lens.y=spec.y+spec.height+int(std::lround(8*spec.scale));if(!lensAtCard||!visible)systemPlace();
            if(!lensPlaced||lensAt.x!=lens.x||lensAt.y!=lens.y){lensPlaced=true;lensAt=POINT{lens.x,lens.y};osdClient->setPlace(lens.x,lens.y);}}}
    // Where Windows shows its own indicator: centred on its pop-up when there is one, else centred low on the primary work
    // area; the plate's bottom 14 DIP above the bottom of either, as Windows draws it.
    void systemPlace(){RECT r{};bool found=false;lgpreview::eachFlyout([&](HWND h){if(!found&&GetWindowRect(h,&r))found=true;});
        if(!found){MONITORINFO mi{};mi.cbSize=sizeof(mi);if(!GetMonitorInfoW(MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY),&mi))return;r=mi.rcWork;}
        lens.x=r.left+(r.right-r.left-lens.width)/2;lens.y=r.bottom-lens.height-int(std::lround(14*spec.scale));}
    // Windows' own pop-up stays invisible while the lens replaces it and comes back the moment it does not. While Liquid
    // Glass runs (it hides the pop-up for its own glass indicator, and gives up for good on one it finds already hidden by
    // someone else), the lens stands down and leaves the pop-up alone instead of showing a second indicator.
    void syncFlyout(){if(lensRejected&&osdClient&&osdClient->surfaceId())lensRejected=false; // a later Create was accepted
        bool was=deferring;deferring=lgpreview::flyoutOwner()==1||lgpreview::ownerRunning();bool want=osdOk()&&osdClient->connected()&&!deferring;
        if(want){lgpreview::eachFlyout([](HWND h){lgpreview::hideFlyout(h);});lgpreview::restoreStrays();}else lgpreview::restoreFlyouts();
        if(was!=deferring&&owner&&hostSeen)PostMessageW(owner,lgpreview::HostState,1,reinterpret_cast<LPARAM>(window));} // the owner reads the status again (bit 64) and says why the lens stays off
    void hookShell(){if(hooks[4]){UnhookWinEvent(hooks[4]);hooks[4]=nullptr;}DWORD shell=0;HWND desktop=GetShellWindow();
        if(desktop&&GetWindowThreadProcessId(desktop,&shell))hooks[4]=SetWinEventHook(EVENT_OBJECT_SHOW,EVENT_OBJECT_SHOW,nullptr,flyoutEvent,shell,0,WINEVENT_OUTOFCONTEXT);}
    // The preview window's own Win32 controls.
    void fill(int id,const wchar_t* const* items,int count,const wchar_t* first){
        int keep=sel(id);SendDlgItemMessageW(window,id,CB_RESETCONTENT,0,0);if(first)SendDlgItemMessageW(window,id,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(first));
        for(int i=0;i<count;++i)SendDlgItemMessageW(window,id,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(items[i]));SendDlgItemMessageW(window,id,CB_SETCURSEL,WPARAM(keep),0);}
    void relabel(){
        if(labelLang==settings.language)return;labelLang=settings.language;auto ui=kUi[labelLang];
        for(int i=0;i<4;++i)SetDlgItemTextW(window,IdLabel1+i,ui[i]);SetDlgItemTextW(window,IdReplay,ui[4]);SetDlgItemTextW(window,IdSample,ui[5]);SetDlgItemTextW(window,IdOn,ui[6]);
        fill(IdArrival,kArrival[labelLang],9,nullptr);fill(IdIndicator,kArrival[labelLang],9,ui[7]);fill(IdSlosh,kSlosh[labelLang],4,nullptr);fill(IdKnob,kKnob[labelLang],3,nullptr);}
    void buildUi(HINSTANCE instance){
        struct Item {int id;const wchar_t* cls;DWORD style;};
        static const Item items[]={{IdLabel1,L"STATIC",0},{IdLabel2,L"STATIC",0},{IdLabel3,L"STATIC",0},{IdLabel4,L"STATIC",0},{IdArrival,L"COMBOBOX",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP},{IdIndicator,L"COMBOBOX",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP},
            {IdSlosh,L"COMBOBOX",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP},{IdKnob,L"COMBOBOX",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP},{IdReplay,L"BUTTON",BS_PUSHBUTTON|WS_TABSTOP},{IdSample,L"BUTTON",BS_PUSHBUTTON|WS_TABSTOP},{IdOn,L"BUTTON",BS_AUTOCHECKBOX|WS_TABSTOP}};
        for(auto&item:items)CreateWindowExW(0,item.cls,L"",WS_CHILD|WS_VISIBLE|item.style,0,0,0,0,window,reinterpret_cast<HMENU>(UINT_PTR(item.id)),instance,nullptr);
        relabel();SendDlgItemMessageW(window,IdArrival,CB_SETCURSEL,5,0);SendDlgItemMessageW(window,IdIndicator,CB_SETCURSEL,5,0);SendDlgItemMessageW(window,IdSlosh,CB_SETCURSEL,0,0);SendDlgItemMessageW(window,IdKnob,CB_SETCURSEL,2,0);
        CheckDlgButton(window,IdOn,BST_CHECKED);layoutUi(true);}
    void layoutUi(bool resize){
        UINT wd=GetDpiForWindow(window);auto S=[wd](int v){return MulDiv(v,int(wd),96);};
        NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof(metrics);SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0,wd);
        HFONT font=CreateFontIndirectW(&metrics.lfMessageFont);
        auto put=[&](int id,int x,int y,int w,int h){HWND c=GetDlgItem(window,id);SetWindowPos(c,nullptr,S(x),S(y),S(w),S(h),SWP_NOZORDER|SWP_NOACTIVATE);SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);};
        const int r1=72,r2=104,r3=136;
        put(IdLabel1,12,r1+4,110,18);put(IdArrival,124,r1,160,200);put(IdLabel2,296,r1+4,128,18);put(IdIndicator,426,r1,132,200);
        put(IdLabel3,12,r2+4,110,18);put(IdSlosh,124,r2,160,200);put(IdLabel4,296,r2+4,128,18);put(IdKnob,426,r2,132,200);
        put(IdReplay,12,r3,100,26);put(IdSample,124,r3,170,26);put(IdOn,306,r3,200,24);
        for(int id:{IdArrival,IdIndicator,IdSlosh,IdKnob})SendDlgItemMessageW(window,id,CB_SETDROPPEDWIDTH,WPARAM(S(240)),0);
        if(uiFont)DeleteObject(uiFont);uiFont=font;
        if(resize){RECT rc{0,0,S(560),S(172)};DWORD style=DWORD(GetWindowLongPtrW(window,GWL_STYLE)),ex=DWORD(GetWindowLongPtrW(window,GWL_EXSTYLE));AdjustWindowRectExForDpi(&rc,style,FALSE,ex,wd);
            SetWindowPos(window,nullptr,0,0,rc.right-rc.left,rc.bottom-rc.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);}
    }
    void content(){
        // Only app-owned text pixels are drawn here. All glass stays in the host.
        HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=spec.width;info.bmiHeader.biHeight=-spec.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        uint32_t*pixels{};HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        if(!dc||!bitmap){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);throw std::runtime_error("content bitmap");}
        auto old=SelectObject(dc,bitmap);memset(pixels,0,size_t(spec.width)*spec.height*4);SetTextColor(dc,RGB(254,254,254));SetBkMode(dc,TRANSPARENT);
        HFONT font=CreateFontW(-MulDiv(20,int(dpi()),96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
        // Three chips (host glow buttons): the labels are ink here and their pixels are the glow masks.
        std::vector<lghost::Button> chips;int bottom=spec.height-20;
        if(client->hostMinor()>=1){
            unsigned lang=settings.language;const wchar_t* label[3]={kArrival[lang][reduced()?0:sel(IdArrival)],kSlosh[lang][reduced()?3:sel(IdSlosh)],kKnob[lang][sel(IdKnob)]};
            HFONT chipFont=CreateFontW(-MulDiv(13,int(dpi()),96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");auto keep=SelectObject(dc,chipFont);int width[3]{};
            for(int i=0;i<3;++i){RECT m{};DrawTextW(dc,label[i],-1,&m,DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);width[i]=m.right-m.left;}
            auto boxes=osd::chipBoxes(spec.width,spec.height,spec.scale,width);
            for(int i=0;i<3;++i){RECT t=boxes[size_t(i)];DrawTextW(dc,label[i],-1,&t,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);}
            GdiFlush();
            for(int i=0;i<3;++i){const RECT&b=boxes[size_t(i)];int w=b.right-b.left,h=b.bottom-b.top;std::vector<uint8_t> mask(size_t(w)*size_t(h));
                for(int y=0;y<h;++y)for(int x=0;x<w;++x)mask[size_t(y)*size_t(w)+size_t(x)]=osd::coverage(pixels[size_t(b.top+y)*size_t(spec.width)+size_t(b.left+x)]);
                chips.push_back(osd::chipButton(b,std::move(mask)));osd::tightenMask(chips.back());}osd::fitMasks(chips);
            SelectObject(dc,keep);DeleteObject(chipFont);bottom=boxes[0].top-4;
        }
        auto previous=SelectObject(dc,font);
        static const wchar_t* text[]={L"LGEngine host\nBackground-adaptive liquid glass",L"LGEngine 宿主\n根据背景自适应的液态玻璃",L"LGEngine 宿主\n根據背景自適應的液態玻璃",L"Host LGEngine\nSzkło dopasowane do tła",L"LGEngine host\nKính tự điều chỉnh theo nền"};
        RECT bounds{20,spec.height/3,spec.width-20,bottom};if(!owner)DrawTextW(dc,text[settings.language],-1,&bounds,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);GdiFlush();
        std::vector<uint32_t> image(pixels,pixels+size_t(spec.width)*spec.height);
        for(auto&p:image)p=osd::inkOf(p); // LGEngine's even-alpha ink convention, with smooth edges.
        SelectObject(dc,previous);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);client->setContents(std::move(image));
        if(client->hostMinor()>=1)client->setButtons(std::move(chips),false); // decorative glow only: the host does not forward clicks
    }
    void apply(){
        if(!client)return;
        relabel();bool rm=reduced();for(int id:{IdArrival,IdIndicator,IdSlosh})EnableWindow(GetDlgItem(window,id),!rm);
        spec.look.dark=settings.opacity;spec.look.blur=(settings.blur*100+20)/40;spec.look.round=settings.radius;
        spec.savePower=settings.savePower!=0;spec.hover=!rm;spec.zoom=10;spec.liquid=60;spec.bounce=45;spec.travel=-1;
        int arrival=sel(IdArrival),indicator=sel(IdIndicator)-1;spec.motion=rm?0:arrival;spec.indicator=rm?0:indicator;
        // Protocol v1 has background adaptation but no explicit light override.
        spec.adapt=true;spec.inShots=true;client->setSpec(spec); // inShots: the card and the lens show in screenshots
        if(osdClient){ // the lens: its own look and profile (Controls), the owner's motion numbers (lensSpec), the arrival and indicator of the combos
            static const char* const profiles[]={"","Tighter rim","Thin lens","Flat pane"};
            lens=osd::lensSpec(spec,spec.scale);lens.inShots=true;lens.look=lensLook;lens.profile=profiles[lensShape<0||lensShape>3?0:lensShape];lens.savePower=settings.savePower!=0;lens.hover=false;
            lens.motion=rm?0:arrival;lens.indicator=spec.indicator;osdClient->setSpec(lens);}
        cardPlaced=lensPlaced=false;place(); // setSpec carried the spec's own place (the lens's is under the card): send the real one now
        int slosh=rm?3:sel(IdSlosh);client->setSlosh(slosh,60);if(osdClient)osdClient->setSlosh(slosh,50);
        client->setPointerArea(RECT{0,0,spec.width,spec.height});
        content();client->show(visible);osdRender(true);InvalidateRect(window,nullptr,TRUE);
    }
    // The same recompute the 250 ms timer ran, now driven by events (parent wait, WM_SETTINGCHANGE, WM_SIZE, owner win events).
    void refresh(){
        if(!client)return;
        HIGHCONTRASTW high{};high.cbSize=sizeof(high);SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0);
        bool ownerAway=owner&&GetParent(owner)!=HWND_MESSAGE&&(!IsWindowVisible(owner)||IsIconic(owner)||GetForegroundWindow()!=owner);
        bool now=pageEnabled&&!ownerAway&&!IsIconic(window)&&!(high.dwFlags&HCF_HIGHCONTRASTON);
        if(now!=visible){visible=now;client->show(now);}place();if(!osdOk()&&osdLive())osdStop();
        if(owner&&rescale()){place();apply();}
        syncFlyout();
    }
    // OSD lens
    bool osdOk()const{HIGHCONTRASTW high{};high.cbSize=sizeof(high);SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0);return osdClient&&(!lensRejected||osdClient->surfaceId())&&client&&indicatorOn()&&client->hostMinor()>=1&&!(high.dwFlags&HCF_HIGHCONTRASTON);}
    void osdRender(bool full){
        // Glass.cs ShowPreview/RefreshPreview/LevelChanged: the sample is made at the card size and scale, the bar adopted out of it, and the number made again when the level (or the symbol) changes; only the slider strip moves otherwise.
        if(!osdClient)return;int W=lens.width,H=lens.height;float k=lens.scale;size_t count=size_t(W)*size_t(H);int number=state.percent();wchar_t glyph=state.glyph();
        if(W!=drawnW||H!=drawnH||k!=drawnK){slider=osd::Slider{};drawnW=W;drawnH=H;drawnK=k;drawnNumber=-1;}
        if(number!=drawnNumber||glyph!=drawnGlyph||osdFrame.size()!=count){osdFrame=osd::drawIndicatorSample(W,H,k,number,glyph);osd::adoptBar(osdFrame,W,H,k,slider);drawnNumber=number;drawnGlyph=glyph;full=true;}
        if(osdFrame.size()!=count)return;
        if(state.knob.at>=0)osd::drawSlider(osdFrame.data(),W,slider,state.knob,osd::styleOf(sel(IdKnob)),k,osdClient->light());
        osdClient->setContents(osdFrame,full?RECT{}:slider.strip);
    }
    void osdKick(int way){if(!osdClient||!osdIn)return;osdClient->kick(way);kickUntil=GetTickCount64()+700;} // Glass.cs Kick/LevelChanged: only a lens that is fully in is kicked; the frames carry on 700 ms for the slosh
    LONGLONG now()const{LARGE_INTEGER t{};QueryPerformanceCounter(&t);return t.QuadPart;}
    void osdWake(){if(osdAnimating)return;osdAnimating=true;lastTick=now();SetTimer(window,TimerAnim,16,nullptr);osdClient->setBusy(true);}
    void osdStop(){KillTimer(window,TimerAnim);KillTimer(window,TimerHide);osdAnimating=false;osdShown=false;osdIn=false;osdDirty=false;state.sampling=false;if(state.knob.at>=0)state.knob.jump(state.current());if(osdClient){osdClient->setBusy(false);osdClient->hide(false);}}
    void osdTick(){
        if(!osdClient)return;LARGE_INTEGER f{};QueryPerformanceFrequency(&f);LONGLONG t=now();float dt=float(double(t-lastTick)/double(f.QuadPart));lastTick=t;
        bool was=state.sampling;int kick=0;bool changed=state.tick(dt,kick);if(kick)osdKick(kick);if(changed||kick||osdDirty){osdDirty=false;osdRender(false);}
        if(was&&!state.sampling)SetTimer(window,TimerHide,1800,nullptr);
        if(!state.knob.active()&&!state.sampling&&GetTickCount64()>=kickUntil){KillTimer(window,TimerAnim);osdAnimating=false;osdClient->setBusy(false);}
    }
    void osdShowChange(int way){
        if(!osdShown)syncFlyout(); // already showing: coalesce, the MsgFlyout hook covers a re-shown pop-up
        if(!osdOk()||deferring){state.knob.jump(state.current());return;} // not shown: snap, so the next show does not glide from an old level
        if(osdShown&&!lensAtCard){osdDirty=true;osdKick(way);osdWake();SetTimer(window,TimerHide,1800,nullptr);return;} // no place/render here: the tick redraws once
        lensAtCard=false;place();osdRender(!osdShown);osdKick(way);osdClient->show(true);osdShown=true;osdWake();SetTimer(window,TimerHide,1800,nullptr);
    }
    void sample(){ // a sweep on the lens only: nothing here reads or writes the system volume or brightness
        if(!osdOk())return;lensAtCard=visible;place();KillTimer(window,TimerHide);state.startSample();osdRender(true);osdClient->show(true);osdShown=true;osdWake();
    }
    void replay(){if(!client||!visible)return;client->hide(true);SetTimer(window,TimerReplay,UINT(spec.outMs+150),nullptr);}
    // Title-bar glass: a third client puts a glass strip over the title bar of the target window (the host surface is click-through, so the window stays usable). No contents are published: the host zero-fills a size that has none, which is plain glass with no ink.
    std::unique_ptr<lghost::Client> titleClient;lghost::Spec titleSpec;lgpreview::AppGlass glass{};HWND titleTarget{};HWINEVENTHOOK titleHooks[4]{};bool titleMoving=false,titleFailed=false,titleShown=false;
    void titleUnhook(){for(auto&hook:titleHooks)if(hook){UnhookWinEvent(hook);hook=nullptr;}}
    void titleHide(){if(titleClient&&titleShown){titleShown=false;titleClient->show(false);}}
    void setTitle(const lgpreview::AppGlass& v){
        glass=v;HWND t=v.on?reinterpret_cast<HWND>(static_cast<ULONG_PTR>(v.target)):nullptr;DWORD pid=0;
        if(t&&(!IsWindow(t)||!GetWindowThreadProcessId(t,&pid)||pid==GetCurrentProcessId()))t=nullptr;
        if(t!=titleTarget){titleUnhook();titleTarget=t;titleMoving=false;titleFailed=false;
            if(t){titleHooks[0]=SetWinEventHook(EVENT_OBJECT_DESTROY,EVENT_OBJECT_HIDE,nullptr,titleEvent,pid,0,WINEVENT_OUTOFCONTEXT);titleHooks[1]=SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART,EVENT_SYSTEM_MINIMIZEEND,nullptr,titleEvent,pid,0,WINEVENT_OUTOFCONTEXT);
                titleHooks[2]=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,titleEvent,pid,0,WINEVENT_OUTOFCONTEXT);titleHooks[3]=SetWinEventHook(EVENT_OBJECT_CLOAKED,EVENT_OBJECT_UNCLOAKED,nullptr,titleEvent,pid,0,WINEVENT_OUTOFCONTEXT);}}
        titlePlace();}
    void titlePlace(){
        HWND t=titleTarget;if(!glass.on||!t||titleMoving||titleFailed||!hostSeen||!IsWindow(t)||!IsWindowVisible(t)||IsIconic(t)||GetForegroundWindow()!=t||displayState!=1){titleHide();return;}
        DWORD cloaked=0;if(SUCCEEDED(DwmGetWindowAttribute(t,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&cloaked){titleHide();return;}
        ULONGLONG tick=GetTickCount64();if(tick<quietUntil){titleHide();SetTimer(window,TimerTitle,UINT(quietUntil-tick+50),nullptr);return;} // re-arm: nothing else triggers the place after sleep or display-on
        RECT win{},frame{},buttons{};if(!GetWindowRect(t,&win)){titleHide();return;}
        MONITORINFO mi{};mi.cbSize=sizeof(mi);if(GetMonitorInfoW(MonitorFromWindow(t,MONITOR_DEFAULTTONEAREST),&mi)&&win.left<=mi.rcMonitor.left&&win.top<=mi.rcMonitor.top&&win.right>=mi.rcMonitor.right&&win.bottom>=mi.rcMonitor.bottom&&((GetWindowLongPtrW(t,GWL_STYLE)&WS_CAPTION)!=WS_CAPTION||!IsZoomed(t)||!EqualRect(&mi.rcWork,&mi.rcMonitor))){titleHide();return;} // full-screen: no caption, not maximized (a full-screen UWP app keeps its caption style), or over a visible taskbar (a maximized window stops at it)
        if(FAILED(DwmGetWindowAttribute(t,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame))))frame=win;
        bool haveButtons=SUCCEEDED(DwmGetWindowAttribute(t,DWMWA_CAPTION_BUTTON_BOUNDS,&buttons,sizeof(buttons)));
        UINT d=GetDpiForWindow(t);if(!d)d=96; // the target's own DPI (place() uses the adapter's window, which is the wrong monitor here)
        RECT bar=titleBar(frame,win,buttons,haveButtons,d,glass.inset);if(bar.right<=bar.left||bar.bottom<=bar.top){titleHide();return;}
        lghost::Spec s;s.kind=0;s.scale=d/96.f;s.x=bar.left;s.y=bar.top;s.width=bar.right-bar.left;s.height=bar.bottom-bar.top;
        if(s.width>4096||s.height>4096){titleHide();return;} // past the host's surface limit (ultrawide or two monitors): skip without latching, so a smaller size shows again
        s.look=lghost::Look{glass.dark,glass.blur,glass.refraction,glass.rim,glass.shadow,glass.round,glass.intensity,glass.elevation};
        s.hover=false;s.inShots=false;s.adapt=true;s.savePower=true;s.label="LiquidGlassControl.TitleBar";s.motion=0;s.inMs=160;s.outMs=120; // motion 0: the plainest fade
        try{
            if(!titleClient||lghost::wire::needsCreate(titleSpec,s,titleClient->hostMinor())){titleClient.reset();titleShown=false;lghost::Options o=lensOptions;o.message=TitleEvent;titleClient=std::make_unique<lghost::Client>(o,s);}
            else if(s.x!=titleSpec.x||s.y!=titleSpec.y||s.width!=titleSpec.width||s.height!=titleSpec.height||!(s.look==titleSpec.look)){
                if(s.width==titleSpec.width&&s.height==titleSpec.height&&s.look==titleSpec.look)titleClient->setPlace(s.x,s.y);else titleClient->setSpec(s);}
            titleSpec=s;if(!titleShown){titleShown=true;titleClient->show(true);}
        }catch(...){titleFailed=true;titleClient.reset();titleShown=false;}}
    bool valid(lgpreview::Settings s){return s.version==2&&s.opacity>=5&&s.opacity<=90&&s.blur<=40&&s.radius<=28&&s.tint<=2&&s.savePower<=1&&s.reducedMotion<=1&&s.language<=4;}
};
void CALLBACK titleEvent(HWINEVENTHOOK,DWORD event,HWND hwnd,LONG object,LONG child,DWORD,DWORD){
    auto app=gWindow?reinterpret_cast<App*>(GetWindowLongPtrW(gWindow,GWLP_USERDATA)):nullptr;if(!app||!hwnd||hwnd!=app->titleTarget||object!=OBJID_WINDOW||child!=CHILDID_SELF)return;
    switch(event){
    case EVENT_SYSTEM_MOVESIZESTART:app->titleMoving=true;app->titleHide();break;
    case EVENT_SYSTEM_MOVESIZEEND:app->titleMoving=false;app->titlePlace();break;
    case EVENT_SYSTEM_MINIMIZESTART:case EVENT_OBJECT_HIDE:case EVENT_OBJECT_CLOAKED:app->titleHide();break;
    case EVENT_SYSTEM_MINIMIZEEND:case EVENT_OBJECT_SHOW:case EVENT_OBJECT_UNCLOAKED:app->titlePlace();break; // uncloaked: a desktop switch or app launch can bring the foreground first
    case EVENT_OBJECT_DESTROY:app->titleHide();app->titleUnhook();app->titleTarget=nullptr;break;
    case EVENT_OBJECT_LOCATIONCHANGE:if(!app->titleMoving)SetTimer(app->window,TimerTitle,30,nullptr);break; // coalesces
    }}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    auto app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);app->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(!app)return DefWindowProcW(hwnd,msg,w,l);
    static const UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    if(msg==taskbarCreated&&taskbarCreated){app->hookShell();app->syncFlyout();return 0;} // Explorer restarted: a new process to watch, new pop-up windows
    try {switch(msg){
    case HostEvent:
        if(w==lghost::Failed){app->failed=true;SetWindowTextW(hwnd,app->title(unsigned(w)));
            wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);std::wstring path=executable;path.resize(path.find_last_of(L"\\")+1);path+=L"host-client.log";
            auto file=_wfopen(path.c_str(),L"a");if(file){auto text=app->client->failureText();fprintf(file,"Win32 %lu: %s\n",(unsigned long)app->client->lastError(),text.c_str());fclose(file);}
            // Only the client giving up (never connected, or its host relaunch blocked by policy) is fatal; a rejected Create, look or extra keeps the pipe and the OSD lens.
            // HostState 3 carries the Win32 code first, so the app can tell a policy-blocked host from a plain failure.
            DWORD code=app->client->lastError();if(app->owner&&(!app->hostSeen||code==4551||code==1260||code==577)){PostMessageW(app->owner,lgpreview::HostState,3,LPARAM(code));PostMessageW(app->owner,lgpreview::Event,2,reinterpret_cast<LPARAM>(hwnd));app->client->show(false);if(app->osdLive())app->osdStop();}}
        else if(w==lghost::Lost||w==lghost::Shown)SetWindowTextW(hwnd,app->title(unsigned(w)));
        else if(w==lghost::Connected||w==lghost::Restarted){app->hostSeen=true;if(app->owner)PostMessageW(app->owner,lgpreview::HostState,1,reinterpret_cast<LPARAM>(hwnd));if(!app->startLens()){app->content();app->osdRender(true);}app->titleFailed=false;app->titlePlace();} // the strip waits for the host: its client has no host path to start one
        app->syncFlyout();return 0; // the host minor is known now: the chips and the lens can appear
    case OsdEvent:if(w==lghost::LightMoved&&app->osdShown)app->osdRender(false);else if(w==lghost::Connected||w==lghost::Restarted){app->lensEver=true;app->lensRejected=false;app->osdIn=false;app->osdRender(true);app->syncFlyout();}
        else if(w==lghost::Shown){if(app->osdShown)app->osdIn=true;}else if(w==lghost::Hidden){if(!app->osdShown)app->osdIn=false;}
        else if(w==lghost::Lost||w==lghost::Failed){ // no lens to show: Windows' own pop-up comes back
            // Failed with a host code while still connected and without a surface: the host refused the lens Create, so stop
            // hiding Windows' pop-up. The code-less Failed comes just before a disconnect, which Lost handles.
            if(w==lghost::Failed&&l)app->lensRejected=app->osdClient&&app->osdClient->connected()&&!app->osdClient->surfaceId();
            if(!app->osdOk()&&app->osdLive())app->osdStop();app->syncFlyout();}
        return 0;
    case MsgVolume:{int way=app->state.onVolume(float(w)/1000.f,l!=0);if(way)app->osdShowChange(way);return 0;}
    case MsgDevice:{float level=0;bool mute=false;if(app->audio.attach()&&app->audio.read(level,mute))app->state.resetVolume(level,mute);return 0;}
    case MsgOwner:app->refresh();SetTimer(hwnd,TimerRecheck,400,nullptr);return 0; // the event can come before the minimize/restore finishes: look again once
    case TitleEvent:if(w==lghost::Connected||w==lghost::Restarted)app->titlePlace();else if(w==lghost::Failed){app->titleFailed=true;app->titleShown=false;app->titleClient.reset();}return 0; // no retry loop: a new target clears titleFailed
    case MsgForeground:app->refresh();if(app->titleTarget)app->titlePlace();return 0;
    case MsgFlyout:app->syncFlyout();return 0;
    case WM_DISPLAYCHANGE:if(app->osdLive())app->osdStop();app->place();app->titlePlace();return 0;
    case WM_POWERBROADCAST:
        if(w==PBT_APMSUSPEND||w==PBT_APMRESUMEAUTOMATIC||w==PBT_APMRESUMESUSPEND){if(app->osdLive())app->osdStop();app->titleHide();app->quietUntil=GetTickCount64()+3000;if(app->titleTarget)SetTimer(hwnd,TimerTitle,3050,nullptr);} // a stuck card after sleep; the resume brightness restore stays silent
        if(w==PBT_POWERSETTINGCHANGE&&l){auto s=reinterpret_cast<POWERBROADCAST_SETTING*>(l);
            if(s->PowerSetting==osd::BrightnessGuid&&s->DataLength==sizeof(DWORD)){DWORD percent=0;memcpy(&percent,s->Data,sizeof(percent));int level=int(std::min<DWORD>(percent,100));bool quiet=app->displayState!=1||GetTickCount64()<app->quietUntil; // idle dimming and display changes move the brightness too: re-baseline silently
                if(quiet)app->state.resetBrightness(level);else{int way=app->state.onBrightness(level);if(way)app->osdShowChange(way);}}
            else if(s->PowerSetting==osd::ConsoleDisplayGuid&&s->DataLength==sizeof(DWORD)){memcpy(&app->displayState,s->Data,sizeof(DWORD));app->quietUntil=GetTickCount64()+3000;if(app->displayState==0&&app->osdLive())app->osdStop();if(app->displayState!=1)app->titleHide();else if(app->titleTarget)SetTimer(hwnd,TimerTitle,3050,nullptr);}} // display off (Modern Standby sends no suspend)
        return TRUE;
    case WM_COPYDATA:{auto p=reinterpret_cast<COPYDATASTRUCT*>(l);
        if(app->owner&&reinterpret_cast<HWND>(w)==app->owner&&p&&p->dwData==lgpreview::AppGlassTag){if(p->cbData!=sizeof(lgpreview::AppGlass)||!p->lpData)return 0;lgpreview::AppGlass v;memcpy(&v,p->lpData,sizeof(v));
            v.inset=std::max(0,std::min(16,v.inset));for(int*f:{&v.dark,&v.blur,&v.refraction,&v.rim,&v.shadow,&v.elevation})*f=std::max(0,std::min(100,*f));v.round=std::max(0,std::min(28,v.round));v.intensity=std::max(0,std::min(150,v.intensity));app->setTitle(v);return 1;}
        if(reinterpret_cast<HWND>(w)==app->owner&&p&&p->dwData==lgpreview::StatusTag&&p->cbData==0){return (app->client&&app->client->hostMinor()>=1?1:0)|(app->visible?2:0)|(app->osdShown?4:0)|(IsWindowVisible(hwnd)?8:0)|(app->osdOk()?16:0)|(app->client&&app->client->connected()?32:0)|(app->deferring?64:0);}if(reinterpret_cast<HWND>(w)==app->owner&&p&&p->dwData==lgpreview::ControlsTag&&p->cbData==sizeof(lgpreview::Controls)&&p->lpData){lgpreview::Controls c;memcpy(&c,p->lpData,sizeof(c));if(c.arrival<0||c.arrival>8||c.indicator<0||c.indicator>9||c.slosh<0||c.slosh>3||c.knob<0||c.knob>2||c.action<0||c.action>2)return 0;
            for(int v:{c.dark,c.blur,c.refraction,c.rim,c.shadow,c.elevation})if(v<0||v>100)return 0;if(c.round<0||c.round>28||c.intensity<0||c.intensity>150||c.shape<0||c.shape>3)return 0;
            app->lensLook=lghost::Look{c.dark,c.blur,c.refraction,c.rim,c.shadow,c.round,c.intensity,c.elevation};app->lensShape=c.shape; // the lens's own look and profile: apply() sends them in place
            int ids[]={IdArrival,IdIndicator,IdSlosh,IdKnob};int values[]={c.arrival,c.indicator,c.slosh,c.knob};for(int i=0;i<4;++i)SendDlgItemMessageW(hwnd,ids[i],CB_SETCURSEL,values[i],0);CheckDlgButton(hwnd,IdOn,c.osd?BST_CHECKED:BST_UNCHECKED);app->pageEnabled=c.enabled!=0;app->refresh();app->apply();if(!c.osd&&app->osdLive())app->osdStop();if(c.action==1)app->replay();if(c.action==2)app->sample();return 1;}if(reinterpret_cast<HWND>(w)!=app->owner||!p||p->dwData!=lgpreview::WireTag||p->cbData!=sizeof(lgpreview::Settings)||!p->lpData)return 0;
        lgpreview::Settings next;memcpy(&next,p->lpData,sizeof(next));if(!app->valid(next))return 0;app->settings=next;app->apply();return lgpreview::digest(next);}
    case WM_COMMAND:{int id=LOWORD(w),code=HIWORD(w);
        if(code==CBN_SELCHANGE&&id>=IdArrival&&id<=IdKnob){app->apply();return 0;}
        if(code==BN_CLICKED){if(id==IdReplay)app->replay();else if(id==IdSample)app->sample();else if(id==IdOn){EnableWindow(GetDlgItem(hwnd,IdSample),app->indicatorOn());if(!app->indicatorOn()&&app->osdLive())app->osdStop();app->syncFlyout();}}
        return 0;}
    case WM_TIMER:
        if(w==TimerRecheck){KillTimer(hwnd,TimerRecheck);app->refresh();}else if(w==TimerAnim)app->osdTick();
        else if(w==TimerTitle){KillTimer(hwnd,TimerTitle);app->titlePlace();}
        else if(w==TimerHide){KillTimer(hwnd,TimerHide);if(app->osdClient){app->osdClient->hide(true);app->osdShown=false;}}
        else if(w==TimerReplay){KillTimer(hwnd,TimerReplay);if(app->visible&&app->client){app->client->show(true);app->client->kick(1);}}
        return 0;
    case WM_SETTINGCHANGE:case WM_SIZE:app->refresh();break;
    case WM_MOVE:app->place();return 0;
    case WM_DPICHANGED:{auto r=reinterpret_cast<RECT*>(l);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
        app->layoutUi(false);if(app->rescale()){app->place();app->apply();}return 0;}
    case WM_APP+17:if(!app->owner){ShowWindow(hwnd,SW_RESTORE);SetForegroundWindow(hwnd);}return 0;
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:
        for(UINT_PTR timer:{TimerAnim,TimerHide,TimerReplay,TimerRecheck,TimerTitle})KillTimer(hwnd,timer);app->titleUnhook();app->titleTarget=nullptr;app->titleClient.reset();
        if(app->wait){UnregisterWaitEx(app->wait,INVALID_HANDLE_VALUE);app->wait=nullptr;}
        for(auto&hook:app->hooks)if(hook){UnhookWinEvent(hook);hook=nullptr;}
        lgpreview::restoreFlyouts(); // Windows' own pop-up comes back with the lens gone
        if(app->power){UnregisterPowerSettingNotification(app->power);app->power=nullptr;}if(app->power2){UnregisterPowerSettingNotification(app->power2);app->power2=nullptr;}
        app->audio.stop();app->osdClient.reset();app->client.reset();if(app->uiFont){DeleteObject(app->uiFont);app->uiFont=nullptr;}PostQuitMessage(0);return 0;
    case WM_CTLCOLORSTATIC:SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    case WM_PAINT:{PAINTSTRUCT p{};auto dc=BeginPaint(hwnd,&p);RECT r{};GetClientRect(hwnd,&r);FillRect(dc,&r,GetSysColorBrush(COLOR_WINDOW));SetBkMode(dc,TRANSPARENT);r.bottom=MulDiv(64,int(GetDpiForWindow(hwnd)),96);
        static const wchar_t* note[]={L"C++ → pipe + shared bitmap → LGEngineHost\nGlass adapts to the background.\nHost v1: manual Light/Dark is unavailable.",L"C++ → 管道与共享位图 → LGEngineHost\n玻璃根据背景自动调整。\n宿主 v1 不支持手动浅色／深色。",L"C++ → 管道與共用點陣圖 → LGEngineHost\n玻璃根據背景自動調整。\n宿主 v1 不支援手動淺色／深色。",L"C++ → potok i wspólna bitmapa → LGEngineHost\nSzkło dostosowuje się do tła.\nHost v1: brak ręcznego trybu Jasny/Ciemny.",L"C++ → pipe và bitmap dùng chung → LGEngineHost\nKính tự điều chỉnh theo nền.\nHost v1 chưa hỗ trợ chọn Sáng/Tối."};
        DrawTextW(dc,note[app->settings.language],-1,&r,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);EndPaint(hwnd,&p);return 0;}
    }}catch(...){app->failed=true;if(app->owner)PostMessageW(app->owner,lgpreview::Event,2,reinterpret_cast<LPARAM>(hwnd));}
    return DefWindowProcW(hwnd,msg,w,l);
}
int selfTest(){lghost::Spec spec;auto frame=lghost::wire::frame(3,42,lghost::wire::create(1,spec));int bad=0;
    auto check=[&](bool ok,const char*label){printf("%s %s\n",ok?"PASS":"FAIL",label);if(!ok)++bad;};
    check(lghost::wire::valid(spec),"default spec valid");check(lghost::wire::frame(frame),"frame validates");frame[8]++;check(!lghost::wire::frame(frame),"bad length rejected");spec.width=0;check(!lghost::wire::valid(spec),"zero size rejected");
    spec.width=420;spec.scale=std::numeric_limits<float>::quiet_NaN();check(!lghost::wire::valid(spec),"nonfinite scale rejected");
    // OSD: springs
    osd::Knob knob;knob.jump(0);knob.set(1);int steps=0;float top=0;while(knob.active()&&steps<400){knob.step(.016f);top=std::max(top,knob.at);++steps;}
    check(steps<400&&knob.at==1.f&&knob.speed==0.f&&top<=1.f,"knob settles at target, never past it");check(knob.lift==0.f&&knob.liftSpeed==0.f&&knob.flow==0.f,"lift returns to 0");
    osd::State state;check(state.onVolume(.5f,false)==0&&state.onBrightness(40)==0,"first readings only initialise");check(state.onVolume(.7f,false)==1&&state.onVolume(.2f,false)==-1&&state.onBrightness(60)==1,"changes give their way");
    state.startSample();int kicks=0,ticks=0;while((state.sampling||state.knob.active())&&ticks<1000){int kick=0;state.tick(.016f,kick);kicks+=kick!=0;++ticks;}
    check(ticks<1000&&kicks>=3&&std::fabs(state.knob.at-state.current())<.0001f,"sample sweep returns to the real level");
    // OSD: alpha convention (slider odd, ink even, premultiplied)
    {const int W=192,H=47;osd::Slider sl;sl.bar.r=RECT{50,20,140,26};sl.strip=RECT{40,0,150,H};sl.base.assign(size_t(110)*size_t(H),0);std::vector<uint32_t> pixels(size_t(W)*size_t(H),0);bool final=true,any=false;
        for(int style=0;style<3;++style){osd::Knob k;k.jump(.6f);k.lift=.8f;for(auto&p:pixels)p=0;osd::drawSlider(pixels.data(),W,sl,k,style,1.f,style*.5f);
            for(auto p:pixels)if(p){any=true;if(!osd::isFinal(p))final=false;}}
        check(any&&final,"slider pixels have odd alpha and are premultiplied");}
    {osd::GdiSession gdi;bool sizesOk=true,barsOk=true,premulOk=true,inkOk=true;
        for(float k:{1.f,1.5f,2.f}){int W=int(std::lround(192*k)),H=int(std::lround(47*k));
            for(int percent:{0,37,100}){auto img=osd::drawIndicatorSample(W,H,k,percent);sizesOk=sizesOk&&img.size()==size_t(W)*size_t(H);
                RECT found{};float level=0;bool got=osd::findBar(img.data(),W,H,RECT{0,0,W,H},k,found,level);barsOk=barsOk&&got&&std::fabs(level-float(percent)/100.f)<=.05f; // (the round caps add a few pixels of colour at either end)
                for(auto p:img){bool even=(p>>24)%2==0;uint32_t a=(p>>24)+(even?1u:0u);if(((p>>16)&255)>a||((p>>8)&255)>a||(p&255)>a)premulOk=false;if(!even&&!osd::isFinal(p))inkOk=false;} // (ink may sit one over its alpha: the owner's MarkInk drops alpha's low bit and keeps the colour)
                osd::Slider sl;barsOk=barsOk&&osd::adoptBar(img,W,H,k,sl)&&!sl.bar.empty()&&sl.strip.right>sl.strip.left&&sl.strip.bottom<=H;}}
        check(gdi.ok&&sizesOk,"indicator sample has the card size at 100-200%");check(gdi.ok&&barsOk,"bar found at 0/37/100%, level within 0.05");check(premulOk,"sample pixels are premultiplied");check(inkOk,"sample pixels are ink or final (even or odd alpha)");}
    // OSD: lens spec and chips
    lghost::Spec card;card.x=100;card.y=100;card.width=420;card.height=200;bool lensOk=true;
    for(float scale:{1.f,1.5f,2.f,3.f}){card.scale=scale;card.width=int(std::lround(420*scale));card.height=int(std::lround(200*scale));auto lens=osd::lensSpec(card,scale);lensOk=lensOk&&lghost::wire::valid(lens)&&lens.kind==1&&lens.y>=card.y+card.height&&lens.width==int(std::lround(192*scale))&&lens.height==int(std::lround(47*scale));}
    check(lensOk,"lens spec valid at 100-300%, 192x47 DIP, below the card");
    int widths[3]={80,60,50};auto boxes=osd::chipBoxes(420,200,1.f,widths);std::vector<lghost::Button> chips;bool fits=true;
    for(size_t i=0;i<3;++i){const RECT&b=boxes[i];fits=fits&&b.left>=0&&b.right<=420&&b.top>=0&&b.bottom<=200&&(i==0||b.left>=boxes[i-1].right);
        chips.push_back(osd::chipButton(b,std::vector<uint8_t>(size_t(b.right-b.left)*size_t(b.bottom-b.top),255)));}
    bool sizes=true;for(auto&c:chips)sizes=sizes&&c.mask.size()==size_t(c.maskBox.right-c.maskBox.left)*size_t(c.maskBox.bottom-c.maskBox.top);
    check(fits&&sizes&&lghost::wire::buttonsValid(chips),"chip masks match their maskBox");chips[0].mask.push_back(0);check(!lghost::wire::buttonsValid(chips),"chip mask of the wrong size rejected");
    {int wide[3]={420,150,150};auto big=osd::chipBoxes(1260,600,3.f,wide);std::vector<lghost::Button> list;for(size_t i=0;i<3;++i){const RECT&b=big[i];list.push_back(osd::chipButton(b,std::vector<uint8_t>(size_t(b.right-b.left)*size_t(b.bottom-b.top),255)));osd::tightenMask(list.back());}
        osd::fitMasks(list);size_t total=0;for(auto&c:list)total+=c.mask.size();check(total<=60000&&lghost::wire::buttonsValid(list)&&lghost::wire::setButtons(1,list,false).bytes.size()<=65536,"chip masks fit the byte budget at 300%");}
    {RECT fr{100,100,500,400},win{92,92,508,408},btn{300,0,408,31};UINT sys=GetSystemMetricsForDpi(SM_CYCAPTION,96)+GetSystemMetricsForDpi(SM_CYFRAME,96)+GetSystemMetricsForDpi(SM_CXPADDEDBORDER,96);
        RECT a=titleBar(fr,win,btn,true,96,0),b=titleBar(fr,win,RECT{},false,96,0),c=titleBar(fr,win,btn,true,96,4);
        check(a.left==100&&a.right==500&&a.top==100&&a.bottom==123,"title bar: caption button bounds give the height");
        check(b.bottom-b.top==std::max<int>(16,std::min<int>(64,int(sys)))&&b.left==100&&b.right==500,"title bar: metric fallback, clamped");
        check(c.right-c.left==392&&c.left==104&&c.top==104&&c.bottom-c.top>=12,"title bar: inset 4 shrinks the width by 8 at 96 dpi");}
    return bad?1:0;}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    osd::GdiSession gdi; // GDI+ for the indicator sample, shut down after the app is gone
    int argc{};auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 2;
    if(argc==2&&!wcscmp(argv[1],L"--self-test")){LocalFree(argv);return selfTest();}
    App app;bool argsOk=argc==2&&!wcscmp(argv[1],L"--standalone");
    if(argc==5&&!wcscmp(argv[1],L"--owner")&&!wcscmp(argv[3],L"--pid")){
        wchar_t*end{};auto h=wcstoull(argv[2],&end,10);if(*end){LocalFree(argv);return 2;}app.owner=reinterpret_cast<HWND>(h);auto requested=wcstoul(argv[4],&end,10);DWORD pid{};
        argsOk=!*end&&requested&&GetWindowThreadProcessId(app.owner,&pid)&&pid==requested;if(argsOk)app.parent.reset(OpenProcess(SYNCHRONIZE,FALSE,pid));argsOk=argsOk&&app.parent.value;app.ownerPid=pid;
    }LocalFree(argv);if(!argsOk)return 2;
    app.pageEnabled=app.visible=!app.owner;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LiquidGlass.LGHost.Client";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(UINT_PTR(COLOR_WINDOW+1));
    if(!RegisterClassW(&cls))return 1;auto hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"Connecting to LGEngineHost",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,100,100,600,150,nullptr,nullptr,instance,&app);if(!hwnd)return 1;
    gWindow=hwnd;gOwner=app.owner;
    lgpreview::restoreFlyouts(); // a run that ended without tidying may have left Windows' pop-up hidden
    try{wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);std::wstring path=executable;path.resize(path.find_last_of(L"\\")+1);lghost::Options options;options.hostPath=lghost::bundledHostPath(path);options.suffix=L"lgcontrol11";options.notify=hwnd;options.message=HostEvent;
        app.buildUi(instance);
        app.rescale();app.place();
        app.spec.inShots=true;app.client=std::make_unique<lghost::Client>(options,app.spec);
        app.lens=osd::lensSpec(app.spec,app.spec.scale);app.lens.inShots=true;app.lens.hover=false;app.lensOptions=options;app.lensOptions.message=OsdEvent;app.lensOptions.hostPath.clear(); // the lens client starts on HostEvent Connected
        app.apply();
        float level=0;bool mute=false;if(app.audio.start(hwnd,MsgVolume,MsgDevice)&&app.audio.read(level,mute))app.state.onVolume(level,mute); // the first reading only initialises
        app.power2=RegisterPowerSettingNotification(hwnd,&osd::ConsoleDisplayGuid,DEVICE_NOTIFY_WINDOW_HANDLE);app.power=RegisterPowerSettingNotification(hwnd,&osd::BrightnessGuid,DEVICE_NOTIFY_WINDOW_HANDLE);
    }catch(...){DestroyWindow(hwnd);return 1;}
    if(app.parent.value)RegisterWaitForSingleObject(&app.wait,app.parent.value,parentGone,hwnd,INFINITE,WT_EXECUTEONLYONCE); // parent exit -> WM_CLOSE
    if(app.owner&&app.ownerPid){app.hooks[0]=SetWinEventHook(EVENT_OBJECT_SHOW,EVENT_OBJECT_HIDE,nullptr,ownerEvent,app.ownerPid,0,WINEVENT_OUTOFCONTEXT);
        app.hooks[2]=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,ownerEvent,app.ownerPid,0,WINEVENT_OUTOFCONTEXT);app.hooks[1]=SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART,EVENT_SYSTEM_MINIMIZEEND,nullptr,ownerEvent,app.ownerPid,0,WINEVENT_OUTOFCONTEXT);
        app.hooks[3]=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,foregroundEvent,0,0,WINEVENT_OUTOFCONTEXT);}
    app.hookShell();
    ShowWindow(hwnd,app.owner?SW_HIDE:SW_SHOWNORMAL);app.refresh();if(app.owner)PostMessageW(app.owner,lgpreview::Event,1,reinterpret_cast<LPARAM>(hwnd));
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return app.failed?1:0;
}
