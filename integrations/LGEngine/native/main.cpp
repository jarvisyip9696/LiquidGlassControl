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
#include "osd.h"
#include "../../../src/optical_labels.h"

namespace {
constexpr UINT HostEvent=WM_APP+60,OsdEvent=WM_APP+61,MsgVolume=WM_APP+62,MsgDevice=WM_APP+63,MsgOwner=WM_APP+64;
enum : int {IdArrival=101,IdIndicator,IdSlosh,IdKnob,IdReplay,IdSample,IdOn,IdLabel1=111,IdLabel2,IdLabel3,IdLabel4};
enum : UINT_PTR {TimerAnim=2,TimerHide=3,TimerReplay=4,TimerRecheck=5};
// Language order everywhere: English, Simplified Chinese, Traditional Chinese, Polish, Vietnamese.
using namespace opticalui;
HWND gWindow{},gOwner{};
void CALLBACK ownerEvent(HWINEVENTHOOK,DWORD,HWND hwnd,LONG object,LONG,DWORD,DWORD){if(hwnd&&hwnd==gOwner&&object==OBJID_WINDOW&&gWindow)PostMessageW(gWindow,MsgOwner,0,0);}
VOID CALLBACK parentGone(PVOID window,BOOLEAN){PostMessageW(static_cast<HWND>(window),WM_CLOSE,0,0);}
struct App {
    HWND window{},owner{};lghost::Handle parent;lgpreview::Settings settings;
    std::unique_ptr<lghost::Client> client,osdClient;lghost::Spec spec,lens;bool visible=true,failed=false,pageEnabled=true;
    HANDLE wait{};HWINEVENTHOOK hooks[3]{};HPOWERNOTIFY power{},power2{};DWORD displayState=1;ULONGLONG quietUntil=0;HFONT uiFont{};DWORD ownerPid{};unsigned labelLang=99;
    osd::State state;osd::AudioWatch audio;std::vector<uint32_t> osdBase,osdFrame;int drawnNumber=-1;wchar_t drawnGlyph=0;
    bool osdAnimating=false,osdShown=false;LONGLONG lastTick=0;
    const wchar_t* title(unsigned event)const{
        static const wchar_t* connected[]={L"LGEngine host connected — drag to move",L"LGEngine 宿主已连接 — 拖动移动",L"LGEngine 宿主已連線 — 拖動移動",L"Połączono z LGEngine — przeciągnij",L"Đã kết nối LGEngine — kéo để di chuyển"};
        static const wchar_t* lost[]={L"LGEngine disconnected — reconnecting",L"LGEngine 已断开 — 正在重连",L"LGEngine 已中斷 — 正在重新連線",L"LGEngine rozłączony — ponowne łączenie",L"LGEngine ngắt kết nối — đang kết nối lại"};
        static const wchar_t* failure[]={L"LGEngine host unavailable",L"LGEngine 宿主不可用",L"LGEngine 宿主無法使用",L"Host LGEngine niedostępny",L"LGEngine host không khả dụng"};
        return (event==lghost::Lost?lost:event==lghost::Failed?failure:connected)[settings.language];
    }
    int sel(int id)const{LRESULT n=SendDlgItemMessageW(window,id,CB_GETCURSEL,0,0);return n<0?0:int(n);}
    bool reduced()const{return settings.reducedMotion!=0;}
    bool indicatorOn()const{return IsDlgButtonChecked(window,IdOn)==BST_CHECKED;}
    void place(){RECT r{};GetWindowRect(window,&r);spec.x=r.left;spec.y=r.bottom+8;if(owner){POINT p{MulDiv(40,GetDpiForWindow(owner),96),MulDiv(365,GetDpiForWindow(owner),96)};ClientToScreen(owner,&p);spec.x=p.x;spec.y=p.y;}if(client)client->setPlace(spec.x,spec.y);
        if(osdClient){lens.x=spec.x+(spec.width-lens.width)/2;lens.y=spec.y+spec.height+int(std::lround(8*spec.scale));if(!visible){MONITORINFO mi{};mi.cbSize=sizeof(mi);if(GetMonitorInfoW(MonitorFromWindow(GetForegroundWindow(),MONITOR_DEFAULTTONEAREST),&mi)){lens.x=mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-lens.width)/2;lens.y=mi.rcWork.bottom-lens.height-int(std::lround(32*spec.scale));}}osdClient->setPlace(lens.x,lens.y);}}
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
        relabel();SendDlgItemMessageW(window,IdArrival,CB_SETCURSEL,4,0);SendDlgItemMessageW(window,IdIndicator,CB_SETCURSEL,0,0);SendDlgItemMessageW(window,IdSlosh,CB_SETCURSEL,0,0);SendDlgItemMessageW(window,IdKnob,CB_SETCURSEL,2,0);
        CheckDlgButton(window,IdOn,BST_CHECKED);layoutUi(true);}
    void layoutUi(bool resize){
        UINT dpi=GetDpiForWindow(window);auto S=[dpi](int v){return MulDiv(v,int(dpi),96);};
        NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof(metrics);SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0,dpi);
        HFONT font=CreateFontIndirectW(&metrics.lfMessageFont);
        auto put=[&](int id,int x,int y,int w,int h){HWND c=GetDlgItem(window,id);SetWindowPos(c,nullptr,S(x),S(y),S(w),S(h),SWP_NOZORDER|SWP_NOACTIVATE);SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);};
        const int r1=72,r2=104,r3=136;
        put(IdLabel1,12,r1+4,110,18);put(IdArrival,124,r1,160,200);put(IdLabel2,296,r1+4,128,18);put(IdIndicator,426,r1,132,200);
        put(IdLabel3,12,r2+4,110,18);put(IdSlosh,124,r2,160,200);put(IdLabel4,296,r2+4,128,18);put(IdKnob,426,r2,132,200);
        put(IdReplay,12,r3,100,26);put(IdSample,124,r3,170,26);put(IdOn,306,r3,200,24);
        for(int id:{IdArrival,IdIndicator,IdSlosh,IdKnob})SendDlgItemMessageW(window,id,CB_SETDROPPEDWIDTH,WPARAM(S(240)),0);
        if(uiFont)DeleteObject(uiFont);uiFont=font;
        if(resize){RECT rc{0,0,S(560),S(172)};DWORD style=DWORD(GetWindowLongPtrW(window,GWL_STYLE)),ex=DWORD(GetWindowLongPtrW(window,GWL_EXSTYLE));AdjustWindowRectExForDpi(&rc,style,FALSE,ex,dpi);
            SetWindowPos(window,nullptr,0,0,rc.right-rc.left,rc.bottom-rc.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);}
    }
    void content(){
        // Only app-owned text pixels are drawn here. All glass stays in the host.
        HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=spec.width;info.bmiHeader.biHeight=-spec.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        uint32_t*pixels{};HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        if(!dc||!bitmap){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);throw std::runtime_error("content bitmap");}
        auto old=SelectObject(dc,bitmap);memset(pixels,0,size_t(spec.width)*spec.height*4);SetTextColor(dc,RGB(254,254,254));SetBkMode(dc,TRANSPARENT);
        HFONT font=CreateFontW(-MulDiv(20,GetDpiForWindow(window),96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Segoe UI");
        // Three chips (host glow buttons): the labels are ink here and their pixels are the glow masks.
        std::vector<lghost::Button> chips;int bottom=spec.height-20;
        if(client->hostMinor()>=1){
            unsigned lang=settings.language;const wchar_t* label[3]={kArrival[lang][reduced()?0:sel(IdArrival)],kSlosh[lang][reduced()?3:sel(IdSlosh)],kKnob[lang][sel(IdKnob)]};
            HFONT chipFont=CreateFontW(-MulDiv(13,GetDpiForWindow(window),96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Segoe UI");auto keep=SelectObject(dc,chipFont);int width[3]{};
            for(int i=0;i<3;++i){RECT m{};DrawTextW(dc,label[i],-1,&m,DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);width[i]=m.right-m.left;}
            auto boxes=osd::chipBoxes(spec.width,spec.height,spec.scale,width);
            for(int i=0;i<3;++i){RECT t=boxes[size_t(i)];DrawTextW(dc,label[i],-1,&t,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);}
            GdiFlush();
            for(int i=0;i<3;++i){const RECT&b=boxes[size_t(i)];int w=b.right-b.left,h=b.bottom-b.top;std::vector<uint8_t> mask(size_t(w)*size_t(h));
                for(int y=0;y<h;++y)for(int x=0;x<w;++x)mask[size_t(y)*size_t(w)+size_t(x)]=pixels[size_t(b.top+y)*size_t(spec.width)+size_t(b.left+x)]&0xffffff?255:0;
                chips.push_back(osd::chipButton(b,std::move(mask)));osd::tightenMask(chips.back());}osd::fitMasks(chips);
            SelectObject(dc,keep);DeleteObject(chipFont);bottom=boxes[0].top-4;
        }
        auto previous=SelectObject(dc,font);
        static const wchar_t* text[]={L"LGEngine host\nBackground-adaptive liquid glass",L"LGEngine 宿主\n根据背景自适应的液态玻璃",L"LGEngine 宿主\n根據背景自適應的液態玻璃",L"Host LGEngine\nSzkło dopasowane do tła",L"LGEngine host\nKính tự điều chỉnh theo nền"};
        RECT bounds{20,spec.height/3,spec.width-20,bottom};if(!owner)DrawTextW(dc,text[settings.language],-1,&bounds,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);GdiFlush();
        std::vector<uint32_t> image(pixels,pixels+size_t(spec.width)*spec.height);
        for(auto&p:image)if(p&0xffffff)p=0xfefefefe; // LGEngine's even-alpha ink convention.
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
        spec.adapt=true;client->setSpec(spec);
        if(osdClient){lens=osd::lensSpec(spec,spec.scale);lens.hover=false;lens.motion=rm?0:(indicator<0?arrival:indicator);lens.indicator=spec.indicator;osdClient->setSpec(lens);}
        int slosh=rm?3:sel(IdSlosh);client->setSlosh(slosh,60);if(osdClient)osdClient->setSlosh(slosh,60);
        client->setPointerArea(RECT{0,0,spec.width,spec.height});
        content();client->show(visible);osdRender(true);InvalidateRect(window,nullptr,TRUE);
    }
    // The same recompute the 250 ms timer ran, now driven by events (parent wait, WM_SETTINGCHANGE, WM_SIZE, owner win events).
    void refresh(){
        if(!client)return;
        HIGHCONTRASTW high{};high.cbSize=sizeof(high);SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0);
        bool ownerHidden=owner&&GetParent(owner)!=HWND_MESSAGE&&(!IsWindowVisible(owner)||IsIconic(owner));
        bool now=pageEnabled&&!ownerHidden&&!IsIconic(window)&&!(high.dwFlags&HCF_HIGHCONTRASTON);
        if(now!=visible){visible=now;client->show(now);}place();if(!osdOk())osdStop();
    }
    // OSD lens
    bool osdOk()const{HIGHCONTRASTW high{};high.cbSize=sizeof(high);SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0);return osdClient&&client&&indicatorOn()&&client->hostMinor()>=1&&!(high.dwFlags&HCF_HIGHCONTRASTON);}
    void osdRender(bool full){
        if(!osdClient)return;auto L=osd::layout(lens.width,lens.height,lens.scale);size_t count=size_t(L.W)*size_t(L.H);int number=state.percent();wchar_t glyph=state.glyph();
        if(osdBase.size()!=count||osdFrame.size()!=count||number!=drawnNumber||glyph!=drawnGlyph){osdBase=osd::inkBase(L,glyph,number);osdFrame=osdBase;drawnNumber=number;drawnGlyph=glyph;full=true;}
        else for(int y=L.strip.top;y<L.strip.bottom;++y){size_t at=size_t(y)*size_t(L.W)+size_t(L.strip.left);memcpy(&osdFrame[at],&osdBase[at],size_t(L.strip.right-L.strip.left)*4);}
        osd::drawSlider(osdFrame.data(),L,state.knob,osd::styleOf(sel(IdKnob)),osdClient->light());
        osdClient->setContents(osdFrame,full?RECT{}:L.strip);
    }
    LONGLONG now()const{LARGE_INTEGER t{};QueryPerformanceCounter(&t);return t.QuadPart;}
    void osdWake(){if(osdAnimating)return;osdAnimating=true;lastTick=now();SetTimer(window,TimerAnim,16,nullptr);osdClient->setBusy(true);}
    void osdStop(){KillTimer(window,TimerAnim);KillTimer(window,TimerHide);osdAnimating=false;osdShown=false;state.sampling=false;if(state.knob.at>=0)state.knob.jump(state.current());if(osdClient){osdClient->setBusy(false);osdClient->hide(false);}}
    void osdTick(){
        if(!osdClient)return;LARGE_INTEGER f{};QueryPerformanceFrequency(&f);LONGLONG t=now();float dt=float(double(t-lastTick)/double(f.QuadPart));lastTick=t;
        bool was=state.sampling;int kick=0;bool changed=state.tick(dt,kick);if(kick)osdClient->kick(kick);if(changed||kick)osdRender(false);
        if(was&&!state.sampling)SetTimer(window,TimerHide,1800,nullptr);
        if(!state.knob.active()&&!state.sampling){KillTimer(window,TimerAnim);osdAnimating=false;osdClient->setBusy(false);}
    }
    void osdShowChange(int way){
        if(!osdOk()){state.knob.jump(state.current());return;} // not shown: snap, so the next show does not glide from an old level
        place();osdRender(true);osdClient->kick(way);osdClient->show(true);osdShown=true;osdWake();SetTimer(window,TimerHide,1800,nullptr);
    }
    void sample(){ // a sweep on the lens only: nothing here reads or writes the system volume or brightness
        if(!osdOk())return;place();KillTimer(window,TimerHide);state.startSample();osdRender(true);osdClient->show(true);osdShown=true;osdWake();
    }
    void replay(){if(!client||!visible)return;client->hide(true);SetTimer(window,TimerReplay,UINT(spec.outMs+150),nullptr);}
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
        else if(w==lghost::Lost||w==lghost::Shown)SetWindowTextW(hwnd,app->title(unsigned(w)));
        else if(w==lghost::Connected||w==lghost::Restarted){app->content();app->osdRender(true);}return 0; // the host minor is known now: the chips and the lens can appear
    case OsdEvent:if(w==lghost::LightMoved&&app->osdShown)app->osdRender(false);else if(w==lghost::Connected||w==lghost::Restarted)app->osdRender(true);return 0;
    case MsgVolume:{int way=app->state.onVolume(float(w)/1000.f,l!=0);if(way)app->osdShowChange(way);return 0;}
    case MsgDevice:{float level=0;bool mute=false;if(app->audio.attach()&&app->audio.read(level,mute))app->state.resetVolume(level,mute);return 0;}
    case MsgOwner:app->refresh();SetTimer(hwnd,TimerRecheck,400,nullptr);return 0; // the event can come before the minimize/restore finishes: look again once
    case WM_POWERBROADCAST:
        if(w==PBT_POWERSETTINGCHANGE&&l){auto s=reinterpret_cast<POWERBROADCAST_SETTING*>(l);
            if(s->PowerSetting==osd::BrightnessGuid&&s->DataLength==sizeof(DWORD)){DWORD percent=0;memcpy(&percent,s->Data,sizeof(percent));int level=int(std::min<DWORD>(percent,100));bool quiet=app->displayState!=1||GetTickCount64()<app->quietUntil; // idle dimming and display changes move the brightness too: re-baseline silently
                if(quiet)app->state.resetBrightness(level);else{int way=app->state.onBrightness(level);if(way)app->osdShowChange(way);}}
            else if(s->PowerSetting==osd::ConsoleDisplayGuid&&s->DataLength==sizeof(DWORD)){memcpy(&app->displayState,s->Data,sizeof(DWORD));app->quietUntil=GetTickCount64()+3000;}}
        return TRUE;
    case WM_COPYDATA:{auto p=reinterpret_cast<COPYDATASTRUCT*>(l);if(reinterpret_cast<HWND>(w)==app->owner&&p&&p->dwData==lgpreview::StatusTag&&p->cbData==0){return (app->client&&app->client->hostMinor()>=1?1:0)|(app->visible?2:0)|(app->osdShown?4:0)|(IsWindowVisible(hwnd)?8:0)|(app->osdOk()?16:0);}if(reinterpret_cast<HWND>(w)==app->owner&&p&&p->dwData==lgpreview::ControlsTag&&p->cbData==sizeof(lgpreview::Controls)&&p->lpData){lgpreview::Controls c;memcpy(&c,p->lpData,sizeof(c));if(c.arrival<0||c.arrival>8||c.indicator<0||c.indicator>9||c.slosh<0||c.slosh>3||c.knob<0||c.knob>2||c.action<0||c.action>2)return 0;int ids[]={IdArrival,IdIndicator,IdSlosh,IdKnob};int values[]={c.arrival,c.indicator,c.slosh,c.knob};for(int i=0;i<4;++i)SendDlgItemMessageW(hwnd,ids[i],CB_SETCURSEL,values[i],0);CheckDlgButton(hwnd,IdOn,c.osd?BST_CHECKED:BST_UNCHECKED);app->pageEnabled=c.enabled!=0;app->refresh();app->apply();if(!c.osd)app->osdStop();if(c.action==1)app->replay();if(c.action==2)app->sample();return 1;}if(reinterpret_cast<HWND>(w)!=app->owner||!p||p->dwData!=lgpreview::WireTag||p->cbData!=sizeof(lgpreview::Settings)||!p->lpData)return 0;
        lgpreview::Settings next;memcpy(&next,p->lpData,sizeof(next));if(!app->valid(next))return 0;app->settings=next;app->apply();return lgpreview::digest(next);}
    case WM_COMMAND:{int id=LOWORD(w),code=HIWORD(w);
        if(code==CBN_SELCHANGE&&id>=IdArrival&&id<=IdKnob){app->apply();return 0;}
        if(code==BN_CLICKED){if(id==IdReplay)app->replay();else if(id==IdSample)app->sample();else if(id==IdOn){EnableWindow(GetDlgItem(hwnd,IdSample),app->indicatorOn());if(!app->indicatorOn())app->osdStop();}}
        return 0;}
    case WM_TIMER:
        if(w==TimerRecheck){KillTimer(hwnd,TimerRecheck);app->refresh();}else if(w==TimerAnim)app->osdTick();
        else if(w==TimerHide){KillTimer(hwnd,TimerHide);if(app->osdClient){app->osdClient->hide(true);app->osdShown=false;}}
        else if(w==TimerReplay){KillTimer(hwnd,TimerReplay);if(app->visible&&app->client){app->client->show(true);app->client->kick(1);}}
        return 0;
    case WM_SETTINGCHANGE:case WM_SIZE:app->refresh();break;
    case WM_MOVE:app->place();return 0;
    case WM_DPICHANGED:{auto r=reinterpret_cast<RECT*>(l);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
        app->spec.scale=GetDpiForWindow(hwnd)/96.f;app->spec.width=MulDiv(420,GetDpiForWindow(hwnd),96);app->spec.height=MulDiv(200,GetDpiForWindow(hwnd),96);app->layoutUi(false);app->place();app->apply();return 0;}
    case WM_APP+17:if(!app->owner){ShowWindow(hwnd,SW_RESTORE);SetForegroundWindow(hwnd);}return 0;
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:
        for(UINT_PTR timer:{TimerAnim,TimerHide,TimerReplay,TimerRecheck})KillTimer(hwnd,timer);
        if(app->wait){UnregisterWaitEx(app->wait,INVALID_HANDLE_VALUE);app->wait=nullptr;}
        for(auto&hook:app->hooks)if(hook){UnhookWinEvent(hook);hook=nullptr;}
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
    auto L=osd::layout(300,64,1.f);std::vector<uint32_t> pixels(size_t(L.W)*size_t(L.H),0);bool final=true,any=false;
    for(int style=0;style<3;++style){osd::Knob k;k.jump(.6f);k.lift=.8f;for(auto&p:pixels)p=0;osd::drawSlider(pixels.data(),L,k,style,style*.5f);
        for(auto p:pixels)if(p){any=true;if(!osd::isFinal(p))final=false;}}
    check(any&&final,"slider pixels have odd alpha and are premultiplied");
    auto ink=osd::inkBase(L,wchar_t(0xE767),50);bool inkOk=true,inkAny=false;for(auto p:ink)if(p){inkAny=true;if(!osd::isInk(p))inkOk=false;}check(inkAny&&inkOk,"glyph and number are even-alpha ink");
    // OSD: lens spec and chips
    lghost::Spec card;card.x=100;card.y=100;card.width=420;card.height=200;bool lensOk=true;
    for(float scale:{1.f,1.5f,2.f,3.f}){card.scale=scale;card.width=int(std::lround(420*scale));card.height=int(std::lround(200*scale));auto lens=osd::lensSpec(card,scale);lensOk=lensOk&&lghost::wire::valid(lens)&&lens.kind==1&&lens.y>=card.y+card.height;}
    check(lensOk,"lens spec valid at 100-300%");
    int widths[3]={80,60,50};auto boxes=osd::chipBoxes(420,200,1.f,widths);std::vector<lghost::Button> chips;bool fits=true;
    for(size_t i=0;i<3;++i){const RECT&b=boxes[i];fits=fits&&b.left>=0&&b.right<=420&&b.top>=0&&b.bottom<=200&&(i==0||b.left>=boxes[i-1].right);
        chips.push_back(osd::chipButton(b,std::vector<uint8_t>(size_t(b.right-b.left)*size_t(b.bottom-b.top),255)));}
    bool sizes=true;for(auto&c:chips)sizes=sizes&&c.mask.size()==size_t(c.maskBox.right-c.maskBox.left)*size_t(c.maskBox.bottom-c.maskBox.top);
    check(fits&&sizes&&lghost::wire::buttonsValid(chips),"chip masks match their maskBox");chips[0].mask.push_back(0);check(!lghost::wire::buttonsValid(chips),"chip mask of the wrong size rejected");
    {int wide[3]={420,150,150};auto big=osd::chipBoxes(1260,600,3.f,wide);std::vector<lghost::Button> list;for(size_t i=0;i<3;++i){const RECT&b=big[i];list.push_back(osd::chipButton(b,std::vector<uint8_t>(size_t(b.right-b.left)*size_t(b.bottom-b.top),255)));osd::tightenMask(list.back());}
        osd::fitMasks(list);size_t total=0;for(auto&c:list)total+=c.mask.size();check(total<=60000&&lghost::wire::buttonsValid(list)&&lghost::wire::setButtons(1,list,false).bytes.size()<=65536,"chip masks fit the byte budget at 300%");}
    return bad?1:0;}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    int argc{};auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 2;
    if(argc==2&&!wcscmp(argv[1],L"--self-test")){LocalFree(argv);return selfTest();}
    App app;bool argsOk=argc==2&&!wcscmp(argv[1],L"--standalone");
    if(argc==5&&!wcscmp(argv[1],L"--owner")&&!wcscmp(argv[3],L"--pid")){
        wchar_t*end{};auto h=wcstoull(argv[2],&end,10);if(*end){LocalFree(argv);return 2;}app.owner=reinterpret_cast<HWND>(h);auto requested=wcstoul(argv[4],&end,10);DWORD pid{};
        argsOk=!*end&&requested&&GetWindowThreadProcessId(app.owner,&pid)&&pid==requested;if(argsOk)app.parent.reset(OpenProcess(SYNCHRONIZE,FALSE,pid));argsOk=argsOk&&app.parent.value;app.ownerPid=pid;
    }LocalFree(argv);if(!argsOk)return 2;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.lpszClassName=L"LiquidGlass.LGHost.Client";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(UINT_PTR(COLOR_WINDOW+1));
    if(!RegisterClassW(&cls))return 1;auto hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"Connecting to LGEngineHost",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,100,100,600,150,nullptr,nullptr,instance,&app);if(!hwnd)return 1;
    gWindow=hwnd;gOwner=app.owner;
    try{wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);std::wstring path=executable;path.resize(path.find_last_of(L"\\")+1);lghost::Options options;options.hostPath=lghost::bundledHostPath(path);options.suffix=L"lgcontrol11";options.notify=hwnd;options.message=HostEvent;
        app.buildUi(instance);
        app.spec.scale=GetDpiForWindow(hwnd)/96.f;app.spec.width=MulDiv(420,GetDpiForWindow(hwnd),96);app.spec.height=MulDiv(app.owner?64:200,GetDpiForWindow(hwnd),96);app.place();
        app.client=std::make_unique<lghost::Client>(options,app.spec);
        app.lens=osd::lensSpec(app.spec,app.spec.scale);app.lens.hover=false;lghost::Options lensOptions=options;lensOptions.message=OsdEvent;app.osdClient=std::make_unique<lghost::Client>(lensOptions,app.lens);
        app.apply();
        float level=0;bool mute=false;if(app.audio.start(hwnd,MsgVolume,MsgDevice)&&app.audio.read(level,mute))app.state.onVolume(level,mute); // the first reading only initialises
        app.power2=RegisterPowerSettingNotification(hwnd,&osd::ConsoleDisplayGuid,DEVICE_NOTIFY_WINDOW_HANDLE);app.power=RegisterPowerSettingNotification(hwnd,&osd::BrightnessGuid,DEVICE_NOTIFY_WINDOW_HANDLE);
    }catch(...){DestroyWindow(hwnd);return 1;}
    if(app.parent.value)RegisterWaitForSingleObject(&app.wait,app.parent.value,parentGone,hwnd,INFINITE,WT_EXECUTEONLYONCE); // parent exit -> WM_CLOSE
    if(app.owner&&app.ownerPid){app.hooks[0]=SetWinEventHook(EVENT_OBJECT_SHOW,EVENT_OBJECT_HIDE,nullptr,ownerEvent,app.ownerPid,0,WINEVENT_OUTOFCONTEXT);
        app.hooks[2]=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,ownerEvent,app.ownerPid,0,WINEVENT_OUTOFCONTEXT);app.hooks[1]=SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART,EVENT_SYSTEM_MINIMIZEEND,nullptr,ownerEvent,app.ownerPid,0,WINEVENT_OUTOFCONTEXT);}
    ShowWindow(hwnd,app.owner?SW_HIDE:SW_SHOWNORMAL);app.refresh();if(app.owner)PostMessageW(app.owner,lgpreview::Event,1,reinterpret_cast<LPARAM>(hwnd));
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return app.failed?1:0;
}
