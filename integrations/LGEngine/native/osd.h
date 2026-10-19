#pragma once
// OSD lens for the LGEngine preview: knob springs, slider drawing, chip buttons, and the volume/brightness sources.
// Slider drawing follows the user's Glass.cs DrawSlider/StepSlider. Alpha convention (LGEngine): even alpha = grey ink that
// light glass may turn dark; odd alpha = final colour the engine leaves alone. The slider is drawn for the glass it is on
// (light 0..1) and marked odd; glyph, number and chip labels are ink (even).
#include <objbase.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../client/lg_client.h"

namespace osd {
constexpr float SliderSpring=52.f,SliderDamping=1.f,LiftSpring=26.f,LiftDamping=.9f,LiftHold=.5f;
constexpr GUID ConsoleDisplayGuid={0x6fe69556,0x704a,0x47a0,{0x8f,0x24,0xc2,0x8d,0x93,0x6f,0xda,0x47}}; // GUID_CONSOLE_DISPLAY_STATE: DWORD 0 off, 1 on, 2 dimmed
constexpr GUID BrightnessGuid={0x8ffee2c6,0x2d01,0x46be,{0xad,0xb9,0x39,0x8a,0xdd,0xc5,0xb4,0xff}}; // GUID_VIDEO_CURRENT_MONITOR_BRIGHTNESS
inline float clamp01(float v){return v<0?0:(v>1?1:v);}
// Glass.cs styles: 0 lens, 1 pill, 2 round. The combo lists round, pill, lens.
inline int styleOf(int combo){return combo==2?0:(combo==1?1:2);}
// The knob rides two springs: its place glides to the level (never past it), its lift follows "moving, or moved in the last 0.5 s".
struct Knob {
    float at=-1,speed=0,goal=0,lift=0,liftSpeed=0,flow=0,hold=0;
    void jump(float v){at=goal=clamp01(v);speed=lift=liftSpeed=flow=hold=0;}
    void set(float v){v=clamp01(v);if(at<0){jump(v);return;}if(std::fabs(v-goal)>.0005f){hold=LiftHold;if((v-at)*speed<0)speed=0;goal=v;}}
    bool moving()const{return std::fabs(goal-at)>.002f||std::fabs(speed)>.02f;}
    bool active()const{return moving()||hold>0||lift!=0||liftSpeed!=0;}
    bool step(float dt){ // true if the knob's place or lift changed (a redraw is due)
        if(at<0)return false;dt=std::min(.034f,std::max(0.f,dt));float wasAt=at,wasLift=lift;
        bool going=moving();float liftGoal=going||hold>0?1.f:0.f;hold=std::max(0.f,hold-dt);
        for(float left=dt;left>0;left-=.005f){float s=std::min(.005f,left),before=goal-at;
            speed+=(SliderSpring*SliderSpring*(goal-at)-2*SliderDamping*SliderSpring*speed)*s;at+=speed*s;
            if(before!=0&&(goal-at)*before<=0){at=goal;speed=0;}
            liftSpeed+=(LiftSpring*LiftSpring*(liftGoal-lift)-2*LiftDamping*LiftSpring*liftSpeed)*s;lift+=liftSpeed*s;
            flow+=(speed-flow)*std::min(1.f,s*22.f);}
        if(!going&&std::fabs(goal-at)<=.002f&&std::fabs(speed)<=.02f){at=goal;speed=0;}
        if(liftGoal==0&&std::fabs(lift)<.004f&&std::fabs(liftSpeed)<.05f){lift=liftSpeed=0;flow=0;}
        return at!=wasAt||lift!=wasLift;
    }
};
// The sample sweep: 0.7 s per waypoint, then back to `restore`. Never touches the system.
inline float sampleGoal(float t,float restore,bool&done){static const float way[4]={.15f,.9f,.3f,.7f};int i=int(t/.7f);done=i>=5;return i<4?way[i]:restore;}
struct State {
    Knob knob;int source=0;bool muted=false,sampling=false,haveSource=false,volInit=false,briInit=false;float vol=-1,bri=-1,sampleT=0,restore=.5f;
    float current()const{float v=source==0?vol:bri;return v<0?.5f:v;}
    // Return the way of a real change (+1 up, -1 down, 0 none or first reading, which only initialises).
    int onVolume(float level,bool mute){level=clamp01(level);
        if(!volInit){volInit=true;vol=level;muted=mute;if(!haveSource){haveSource=true;source=0;knob.jump(level);}return 0;}
        int way=0;if(std::fabs(level-vol)>=.0005f)way=level>vol?1:-1;else if(mute!=muted)way=mute?-1:1;
        vol=level;muted=mute;if(!way)return 0;sampling=false;source=0;haveSource=true;knob.set(level);return way;}
    int onBrightness(int percent){float level=clamp01(float(percent)/100.f);
        if(!briInit){briInit=true;bri=level;if(!haveSource){haveSource=true;source=1;knob.jump(level);}return 0;}
        if(std::fabs(level-bri)<.0005f)return 0;int way=level>bri?1:-1;bri=level;sampling=false;source=1;haveSource=true;knob.set(level);return way;}
    void resetVolume(float level,bool mute){volInit=true;vol=clamp01(level);muted=mute;if(source==0&&haveSource&&!sampling)knob.jump(vol);} // default device changed: silent
    void resetBrightness(int percent){briInit=true;bri=clamp01(float(percent)/100.f);if(!haveSource){haveSource=true;source=1;knob.jump(bri);}else if(source==1&&!sampling)knob.jump(bri);} // dimming or adaptive change: silent
    void startSample(){restore=current();if(knob.at<0)knob.jump(restore);sampling=true;sampleT=0;}
    // Moves the sample and the knob on; kick is the way of a sample step (0 none). True if a redraw is due.
    bool tick(float dt,int&kick){kick=0;
        if(sampling){sampleT+=dt;bool done=false;float g=sampleGoal(sampleT,restore,done);if(std::fabs(g-knob.goal)>.0005f){kick=g>knob.goal?1:-1;knob.set(g);}if(done)sampling=false;}
        return knob.step(dt);}
    int percent()const{return knob.at<0?0:int(std::lround(clamp01(knob.goal)*100.f));} // the real level (the target), not the gliding knob
    wchar_t glyph()const{return sampling?wchar_t(0xE767):(source==1?wchar_t(0xE706):(muted?wchar_t(0xE74F):wchar_t(0xE767)));}
};
struct Layout {int W=0,H=0;float k=1,cy=0,barL=0,barR=0,barH=0;RECT strip{},glyph{},num{};};
inline Layout layout(int W,int H,float k){Layout L;L.W=W;L.H=H;L.k=k;L.cy=float(H)/2.f;L.barH=6*k;L.barL=66*k;L.barR=float(W)-66*k;
    L.strip=RECT{std::max(0,int(std::lround(L.barL-14*k))),0,std::min(W,int(std::lround(L.barR+14*k))),H};
    L.glyph=RECT{int(std::lround(14*k)),0,int(std::lround(46*k)),H};L.num=RECT{W-int(std::lround(52*k)),0,W-int(std::lround(10*k)),H};return L;}
inline lghost::Spec lensSpec(const lghost::Spec&card,float scale){lghost::Spec s=card;s.kind=1;s.scale=scale;
    s.width=int(std::lround(300*scale));s.height=int(std::lround(64*scale));s.label="LiquidGlassControl.Host.OSD";
    s.x=card.x+(card.width-s.width)/2;s.y=card.y+card.height+int(std::lround(8*scale));return s;}
inline void lay(float&a,float&c,float la,float lc){a=la+a*(1-la);c=lc+c*(1-la);}
inline float capsule(float px,float py,float x0,float x1,float y,float r){float dy=py-y;if(px>=x0&&px<=x1)return std::fabs(dy)-r;float dx=px<x0?x0-px:px-x1;return std::sqrt(dx*dx+dy*dy)-r;}
inline float pill(float px,float py,float cx,float cy,float hx,float hy){float r=std::min(hx,hy),qx=std::fabs(px-cx)-(hx-r),qy=std::fabs(py-cy)-(hy-r),ox=std::max(qx,0.f),oy=std::max(qy,0.f);return std::sqrt(ox*ox+oy*oy)+std::min(std::max(qx,qy),0.f)-r;}
inline float cover(float d){return d<=-.5f?1.f:(d>=.5f?0.f:.5f-d);}
inline float smooth(float t){t=clamp01(t);return t*t*(3-2*t);}
// Draws the track, fill and knob over the strip of `px` (premultiplied ARGB, W wide). style: 0 lens, 1 pill, 2 round; t: glass light 0..1.
inline void drawSlider(uint32_t*px,const Layout&L,const Knob&kn,int style,float t){
    float k=L.k,cy=L.cy,trackR=std::max(L.barH,4*k)/2;bool lens=style==0;float knobHy,knobHx,popUp,drawMost;
    if(lens){knobHy=trackR*2.6f;knobHx=knobHy*1.6f;popUp=.24f;drawMost=.2f;}else if(style==1){knobHy=trackR*2.3f;knobHx=knobHy*1.5f;popUp=.14f;drawMost=.1f;}else{knobHy=knobHx=trackR*2.1f;popUp=.3f;drawMost=0;}
    float x0=L.barL+trackR,x1=L.barR-trackR,travel0=L.barL+knobHx,travel1=std::max(travel0,L.barR-knobHx);
    float kx=travel0+(travel1-travel0)*std::max(-.05f,std::min(1.05f,kn.at));
    float lift=clamp01(kn.lift),clear=lift,grow=1+popUp*lift,drawOut=1+std::min(drawMost,std::fabs(kn.flow)*.3f);
    float rest=knobHx*grow,extra=rest*(drawOut-1),hx=rest+extra,hy=knobHy*grow/std::sqrt(drawOut),kc=kx-(kn.flow>=0?extra:-extra);
    float magnify=1.25f+.45f*clear,shadowMost=(.24f-.06f*clear)*(1+.55f*t),shadowDrop=(1.5f+clear)*k,shadowWide=(3+1.5f*clear)*k;
    float reachX=hx+4.5f*k+1,reachUp=hy+3*k+1,reachDown=hy+7*k+1,outlineA=.14f+.26f*t,rimBoost=1+2*t,shadeBelow=.09f*t;
    for(int y=L.strip.top;y<L.strip.bottom;++y){float py=float(y)+.5f;
        for(int x=L.strip.left;x<L.strip.right;++x){float pxf=float(x)+.5f,a=0,c=0;
            if(py>cy-trackR-1&&py<cy+trackR+1){float track=cover(capsule(pxf,py,x0,x1,cy,trackR)),fill=cover(capsule(pxf,py,x0,std::min(x1,std::max(x0,kx)),cy,trackR));
                if(track>0)lay(a,c,.3f*track,.3f*track*(1-t));if(fill>0)lay(a,c,fill,fill*(1-t));}
            if(std::fabs(pxf-kc)<reachX&&py>cy-reachUp&&py<cy+reachDown){
                float knob=pill(pxf,py,kc,cy,hx,hy),shade=shadowMost*smooth(1-(pill(pxf,py-shadowDrop,kc,cy,hx,hy)+.5f)/shadowWide);
                if(shade>0)lay(a,c,shade,0);float body=cover(knob);
                if(body>0){float bodyA=body,bodyC=body,milk=1;
                    if(lens){float lx=kc+(pxf-kc)/magnify,ly=cy+(py-cy)/magnify,lensTrack=cover(capsule(lx,ly,x0,x1,cy,trackR)),lensFill=cover(capsule(lx,ly,x0,std::min(x1,std::max(x0,kx)),cy,trackR));
                        float seen=lensFill+.3f*lensTrack*(1-lensFill);milk=.94f-.6f*clear;bodyA=body*(milk+(1-milk)*seen);bodyC=body*(milk+(1-milk)*seen*(1-t));}
                    lay(a,c,bodyA,bodyC);float lower=std::max(0.f,(py-cy)/hy);if(shadeBelow>0)lay(a,c,shadeBelow*lower*lower*body,0);
                    if(lens){float rim=cover(std::fabs(knob+.8f*k)-.6f*k)*body,upper=std::max(0.f,(cy-py)/hy);c=std::min(a,c+rim*.15f*(.4f+.6f*upper)*(1-milk+.3f)*rimBoost);}}
                float line=outlineA*cover(std::fabs(knob)-.5f*k);if(line>0)lay(a,c,line,0);}
            if(a<.002f)continue;
            size_t at=size_t(y)*size_t(L.W)+size_t(x);uint32_t under=px[at];int ua=int(under>>24)&255;float keep=1-a;int A=int(a*255+float(ua)*keep+.5f);
            if(A<=0){px[at]=0;continue;}if(A>255)A=255;A|=1; // odd: final, left alone by the light-glass flip
            int v=int(c*255+.5f),R=std::min(A,v+int(float((under>>16)&255)*keep)),G=std::min(A,v+int(float((under>>8)&255)*keep)),B=std::min(A,v+int(float(under&255)*keep));
            px[at]=(uint32_t(A)<<24)|(uint32_t(R)<<16)|(uint32_t(G)<<8)|uint32_t(B);}}
}
// Ink: even alpha, channels <= alpha (premultiplied). Slider pixels: odd alpha.
inline bool premultiplied(uint32_t p){uint32_t a=p>>24;return ((p>>16)&255)<=a&&((p>>8)&255)<=a&&(p&255)<=a;}
inline bool isInk(uint32_t p){return p==0||((p>>24)%2==0&&premultiplied(p));}
inline bool isFinal(uint32_t p){return p==0||((p>>24)%2==1&&premultiplied(p));}
inline BOOL CALLBACK faceFound(const LOGFONTW*,const TEXTMETRICW*,DWORD,LPARAM found){*reinterpret_cast<bool*>(found)=true;return FALSE;}
inline const wchar_t* iconFace(){static const wchar_t* face=nullptr;if(face)return face;
    HDC dc=GetDC(nullptr);LOGFONTW f{};f.lfCharSet=DEFAULT_CHARSET;wcscpy_s(f.lfFaceName,L"Segoe Fluent Icons");bool found=false;
    EnumFontFamiliesExW(dc,&f,reinterpret_cast<FONTENUMPROCW>(faceFound),reinterpret_cast<LPARAM>(&found),0);ReleaseDC(nullptr,dc);
    face=found?L"Segoe Fluent Icons":L"Segoe MDL2 Assets";return face;}
// The glyph and the number as ink (even alpha), the rest of the card clear.
inline std::vector<uint32_t> inkBase(const Layout&L,wchar_t glyph,int number){
    std::vector<uint32_t> image(size_t(L.W)*size_t(L.H),0);HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=L.W;info.bmiHeader.biHeight=-L.H;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;uint32_t*pixels{};
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
    if(!dc||!bitmap||!pixels){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return image;}
    auto old=SelectObject(dc,bitmap);memset(pixels,0,image.size()*4);SetTextColor(dc,RGB(254,254,254));SetBkMode(dc,TRANSPARENT);
    HFONT icons=CreateFontW(-int(std::lround(20*L.k)),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,iconFace());
    HFONT digits=CreateFontW(-int(std::lround(18*L.k)),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Segoe UI");
    RECT g=L.glyph,n=L.num;wchar_t text[16]{};swprintf(text,16,L"%d",number);
    auto previous=SelectObject(dc,icons);DrawTextW(dc,&glyph,1,&g,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    SelectObject(dc,digits);DrawTextW(dc,text,-1,&n,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);GdiFlush();
    for(size_t i=0;i<image.size();++i)if(pixels[i]&0xffffff)image[i]=0xfefefefeu; // LGEngine's even-alpha ink convention.
    SelectObject(dc,previous);DeleteObject(icons);DeleteObject(digits);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return image;
}
// Three chips in a row near the card's bottom; textW are the label widths in pixels.
inline std::array<RECT,3> chipBoxes(int W,int H,float k,const int textW[3]){
    int h=int(std::lround(26*k)),gap=int(std::lround(8*k)),pad=int(std::lround(20*k)),maxW=(W-2*int(std::lround(16*k))-2*gap)/3,w[3],total=2*gap;
    for(int i=0;i<3;++i){w[i]=std::max(h,std::min(textW[i]+pad,maxW));total+=w[i];}
    std::array<RECT,3> boxes{};int x=(W-total)/2,top=H-int(std::lround(52*k));
    for(int i=0;i<3;++i){boxes[size_t(i)]=RECT{x,top,x+w[i],top+h};x+=w[i]+gap;}return boxes;}
// Crops a full-box mask to its ink bounds (maskBox in card coordinates); no ink = no mask. Then fitMasks keeps the byte budget (the client drops all masks past 64 KB).
inline void tightenMask(lghost::Button&b){int w=b.maskBox.right-b.maskBox.left,h=b.maskBox.bottom-b.maskBox.top,x0=w,y0=h,x1=-1,y1=-1;if(b.mask.size()!=size_t(w)*size_t(h)){b.mask.clear();b.maskBox=RECT{};return;}
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(b.mask[size_t(y)*size_t(w)+size_t(x)]){x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);}
    if(x1<0){b.mask.clear();b.maskBox=RECT{};return;}int nw=x1-x0+1,nh=y1-y0+1;std::vector<uint8_t> cut(size_t(nw)*size_t(nh));
    for(int y=0;y<nh;++y)memcpy(&cut[size_t(y)*size_t(nw)],&b.mask[size_t(y+y0)*size_t(w)+size_t(x0)],size_t(nw));b.maskBox=RECT{b.maskBox.left+x0,b.maskBox.top+y0,b.maskBox.left+x0+nw,b.maskBox.top+y0+nh};b.mask=std::move(cut);}
inline void fitMasks(std::vector<lghost::Button>&list,size_t budget=60000){size_t used=0;for(auto&b:list){if(used+b.mask.size()>budget){b.mask.clear();b.maskBox=RECT{};}else used+=b.mask.size();}}
inline lghost::Button chipButton(const RECT&box,std::vector<uint8_t> mask){lghost::Button b;b.hit=box;b.maskBox=box;
    b.hx=float(box.right-box.left)/2.f;b.hy=float(box.bottom-box.top)/2.f;b.cx=float(box.left)+b.hx;b.cy=float(box.top)+b.hy;b.rad=std::min(b.hx,b.hy);b.mask=std::move(mask);return b;}
// Sources. Volume: the default render endpoint's callback posts (level*1000, mute) to the window; a default-device change posts devMsg.
class VolumeCb final:public IAudioEndpointVolumeCallback {
    LONG n=1;HWND h;UINT msg;
public:
    VolumeCb(HWND w,UINT m):h(w),msg(m){}
    STDMETHODIMP QueryInterface(REFIID id,void**o)override{if(!o)return E_POINTER;if(id==__uuidof(IUnknown)||id==__uuidof(IAudioEndpointVolumeCallback)){*o=static_cast<IAudioEndpointVolumeCallback*>(this);AddRef();return S_OK;}*o=nullptr;return E_NOINTERFACE;}
    STDMETHODIMP_(ULONG) AddRef()override{return ULONG(InterlockedIncrement(&n));}
    STDMETHODIMP_(ULONG) Release()override{ULONG r=ULONG(InterlockedDecrement(&n));if(!r)delete this;return r;}
    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA d)override{if(d)PostMessageW(h,msg,WPARAM(std::lround(d->fMasterVolume*1000.f)),d->bMuted?1:0);return S_OK;}
};
class NotifyCb final:public IMMNotificationClient {
    LONG n=1;HWND h;UINT msg;
public:
    NotifyCb(HWND w,UINT m):h(w),msg(m){}
    STDMETHODIMP QueryInterface(REFIID id,void**o)override{if(!o)return E_POINTER;if(id==__uuidof(IUnknown)||id==__uuidof(IMMNotificationClient)){*o=static_cast<IMMNotificationClient*>(this);AddRef();return S_OK;}*o=nullptr;return E_NOINTERFACE;}
    STDMETHODIMP_(ULONG) AddRef()override{return ULONG(InterlockedIncrement(&n));}
    STDMETHODIMP_(ULONG) Release()override{ULONG r=ULONG(InterlockedDecrement(&n));if(!r)delete this;return r;}
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR,DWORD)override{return S_OK;}
    STDMETHODIMP OnDeviceAdded(LPCWSTR)override{return S_OK;}
    STDMETHODIMP OnDeviceRemoved(LPCWSTR)override{return S_OK;}
    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow,ERole role,LPCWSTR)override{if(flow==eRender&&role==eMultimedia)PostMessageW(h,msg,0,0);return S_OK;}
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY)override{return S_OK;}
};
// UI thread only. Never sets anything: it only reads and listens.
class AudioWatch {
    IMMDeviceEnumerator*en=nullptr;IAudioEndpointVolume*ep=nullptr;VolumeCb*vcb=nullptr;NotifyCb*ncb=nullptr;HWND h{};UINT vmsg=0;bool com=false;
public:
    ~AudioWatch(){stop();}
    bool start(HWND w,UINT volumeMsg,UINT deviceMsg){h=w;vmsg=volumeMsg;com=SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
        if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&en)))){en=nullptr;return false;}
        ncb=new NotifyCb(w,deviceMsg);en->RegisterEndpointNotificationCallback(ncb);return attach();}
    bool attach(){detach();IMMDevice*d=nullptr;if(!en||FAILED(en->GetDefaultAudioEndpoint(eRender,eMultimedia,&d))||!d)return false;
        HRESULT hr=d->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&ep));d->Release();if(FAILED(hr)){ep=nullptr;return false;}
        vcb=new VolumeCb(h,vmsg);if(FAILED(ep->RegisterControlChangeNotify(vcb))){vcb->Release();vcb=nullptr;}return true;}
    bool read(float&level,bool&mute){BOOL m=FALSE;level=0;if(!ep||FAILED(ep->GetMasterVolumeLevelScalar(&level))||FAILED(ep->GetMute(&m)))return false;mute=m!=FALSE;return true;}
    void detach(){if(ep&&vcb)ep->UnregisterControlChangeNotify(vcb);if(vcb){vcb->Release();vcb=nullptr;}if(ep){ep->Release();ep=nullptr;}}
    void stop(){detach();if(en&&ncb)en->UnregisterEndpointNotificationCallback(ncb);if(ncb){ncb->Release();ncb=nullptr;}if(en){en->Release();en=nullptr;}if(com){CoUninitialize();com=false;}}
};
}
