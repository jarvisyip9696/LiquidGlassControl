#pragma once
// OSD lens for the LGEngine preview: knob springs, slider drawing, chip buttons, and the volume/brightness sources.
// The indicator sample, bar finding, slider drawing and ink marking are a line-for-line port of the owner's Glass.cs (DrawIndicatorSample, MarkInk,
// FindBar, AdoptBar, DrawSlider, StepSlider; the Knob springs below), except the lens knob style and its wob, which are LGEngine's own. Alpha convention (LGEngine): even alpha = grey ink that
// light glass may turn dark; odd alpha = final colour the engine leaves alone. The slider is drawn for the glass it is on
// (light 0..1) and marked odd; glyph, number and chip labels are ink (even).
#include <objbase.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include "../client/lg_client.h"
#include <objidl.h>
namespace Gdiplus {using std::min;using std::max;} // gdiplus.h wants min/max, which NOMINMAX removes
#include <gdiplus.h>
#ifdef _MSC_VER
#pragma comment(lib,"gdiplus")
#endif

namespace osd {
constexpr float SliderSpring=52.f,SliderDamping=1.f,LiftSpring=26.f,LiftDamping=.9f,LiftHold=.5f,HoverSpring=20.f,HoverDamping=.75f,PressSpring=34.f,PressDamping=.45f,WobSpring=22.f,WobDamping=.26f;
constexpr GUID ConsoleDisplayGuid={0x6fe69556,0x704a,0x47a0,{0x8f,0x24,0xc2,0x8d,0x93,0x6f,0xda,0x47}}; // GUID_CONSOLE_DISPLAY_STATE: DWORD 0 off, 1 on, 2 dimmed
constexpr GUID BrightnessGuid={0x8ffee2c6,0x2d01,0x46be,{0xad,0xb9,0x39,0x8a,0xdd,0xc5,0xb4,0xff}}; // GUID_VIDEO_CURRENT_MONITOR_BRIGHTNESS
inline float clamp01(float v){return v<0?0:(v>1?1:v);}
// Glass.cs styles: 0 lens, 1 pill, 2 round. The combo lists round, pill, lens.
inline int styleOf(int combo){return combo==2?0:(combo==1?1:2);}
// The knob rides two springs: its place glides to the level (never past it), its lift follows "moving, or moved in the last 0.5 s".
// Two more follow the pointer: hover (over the indicator) swells it a little, press (held down on it) squashes it, with a wobble back on release.
// wob (the lens's stretch) follows the glide's flow on a loose spring: it lags into a drag and swings a little past 0 when the knob stops (a liquid slosh), then settles.
struct Knob {
    float at=-1,speed=0,goal=0,lift=0,liftSpeed=0,flow=0,hold=0,hover=0,hoverSpeed=0,hoverGoal=0,press=0,pressSpeed=0,pressGoal=0,wob=0,wobSpeed=0;
    void jump(float v){at=goal=clamp01(v);speed=lift=liftSpeed=flow=hold=wob=wobSpeed=0;}
    void snap(float v){if(at<0)return;at=goal=clamp01(v);speed=flow=wob=wobSpeed=0;} // moves the knob at once but keeps its lift (a switch of kind mid-show)
    void set(float v){v=clamp01(v);if(at<0){jump(v);return;}if(std::fabs(v-goal)>.0005f){hold=LiftHold;if((v-at)*speed<0)speed=0;goal=v;}}
    void aim(bool over,bool down){hoverGoal=over||down?1.f:0.f;pressGoal=down?1.f:0.f;}
    void rest(){hover=hoverSpeed=hoverGoal=press=pressSpeed=pressGoal=0;}
    bool moving()const{return std::fabs(goal-at)>.002f||std::fabs(speed)>.02f;}
    bool touching()const{return std::fabs(hoverGoal-hover)>.002f||std::fabs(hoverSpeed)>.02f||std::fabs(pressGoal-press)>.002f||std::fabs(pressSpeed)>.02f;}
    bool active()const{return moving()||hold>0||lift!=0||liftSpeed!=0||wob!=0||wobSpeed!=0||touching();}
    bool step(float dt){ // true if the knob's place, lift, hover, press or stretch changed (a redraw is due)
        if(at<0)return false;dt=std::min(.034f,std::max(0.f,dt));float wasAt=at,wasLift=lift,wasHover=hover,wasPress=press,wasWob=wob;
        hold=std::max(0.f,hold-dt);bool going=moving();float liftGoal=going||hold>0?1.f:0.f; // (the owner's hold counts this step's time too)
        for(float left=dt;left>0;left-=.005f){float s=std::min(.005f,left),before=goal-at;
            speed+=(SliderSpring*SliderSpring*(goal-at)-2*SliderDamping*SliderSpring*speed)*s;at+=speed*s;
            if(before!=0&&(goal-at)*before<=0){at=goal;speed=0;}
            liftSpeed+=(LiftSpring*LiftSpring*(liftGoal-lift)-2*LiftDamping*LiftSpring*liftSpeed)*s;lift+=liftSpeed*s;
            hoverSpeed+=(HoverSpring*HoverSpring*(hoverGoal-hover)-2*HoverDamping*HoverSpring*hoverSpeed)*s;hover+=hoverSpeed*s;
            pressSpeed+=(PressSpring*PressSpring*(pressGoal-press)-2*PressDamping*PressSpring*pressSpeed)*s;press+=pressSpeed*s;
            flow+=(speed-flow)*std::min(1.f,s*22.f);
            wobSpeed+=(WobSpring*WobSpring*(flow-wob)-2*WobDamping*WobSpring*wobSpeed)*s;wob+=wobSpeed*s;}
        if(!going&&std::fabs(goal-at)<=.002f&&std::fabs(speed)<=.02f){at=goal;speed=0;}
        if(liftGoal==0&&std::fabs(lift)<.004f&&std::fabs(liftSpeed)<.05f){lift=liftSpeed=0;flow=0;}
        if(std::fabs(wob)<.003f&&std::fabs(wobSpeed)<.05f&&std::fabs(flow)<.003f){wob=wobSpeed=0;}
        if(std::fabs(hoverGoal-hover)<=.002f&&std::fabs(hoverSpeed)<=.02f){hover=hoverGoal;hoverSpeed=0;}
        if(std::fabs(pressGoal-press)<=.002f&&std::fabs(pressSpeed)<=.02f){press=pressGoal;pressSpeed=0;}
        return at!=wasAt||lift!=wasLift||hover!=wasHover||press!=wasPress||wob!=wasWob;
    }
};
// The sample sweep: 0.7 s per waypoint, then back to `restore`. Never touches the system.
inline float sampleGoal(float t,float restore,bool&done){static const float way[4]={.15f,.9f,.3f,.7f};int i=int(t/.7f);done=i>=5;return i<4?way[i]:restore;}
struct State {
    Knob knob;int source=0;bool muted=false,sampling=false,haveSource=false,volInit=false,briInit=false,switched=false;float vol=-1,bri=-1,sampleT=0,restore=.5f;
    float current()const{float v=source==0?vol:bri;return v<0?.5f:v;}
    // A change of kind (volume then brightness, or back) starts the knob at the new kind's last level, so it glides only that kind's step
    // instead of across from the other kind's level; switched tells the caller to redraw the number and symbol at once.
    void take(int to,float was){switched=haveSource&&source!=to;if(switched)knob.snap(was);sampling=false;source=to;haveSource=true;}
    // Return the way of a real change (+1 up, -1 down, 0 none or first reading, which only initialises).
    int onVolume(float level,bool mute){level=clamp01(level);
        if(!volInit){volInit=true;vol=level;muted=mute;if(!haveSource){haveSource=true;source=0;knob.jump(level);}return 0;}
        int way=0;if(std::fabs(level-vol)>=.0005f)way=level>vol?1:-1;else if(mute!=muted)way=mute?-1:1;
        float was=vol;vol=level;muted=mute;if(!way)return 0;take(0,was);knob.set(level);return way;}
    int onBrightness(int percent){float level=clamp01(float(percent)/100.f);
        if(!briInit){briInit=true;bri=level;if(!haveSource){haveSource=true;source=1;knob.jump(level);}return 0;}
        if(std::fabs(level-bri)<.0005f)return 0;int way=level>bri?1:-1;float was=bri;bri=level;take(1,was);knob.set(level);return way;}
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
// The owner's indicator card is 192x47 DIP; its lens has its own look and motion (Glass.cs EnsureSurface / SurfaceSettings), set here to the owner's defaults.
inline lghost::Spec lensSpec(const lghost::Spec&card,float scale){lghost::Spec s=card;s.kind=1;s.scale=scale;
    s.width=int(std::lround(192*scale));s.height=int(std::lround(47*scale));s.label="LiquidGlassControl.Host.OSD";s.profile.clear();
    s.look=lghost::Look{5,32,90,68,30,24,100,50};s.inMs=288;s.outMs=230;s.zoom=10;s.liquid=60;s.bounce=50;s.travel=96;s.fps=60;s.adapt=true;s.hover=false;
    s.x=card.x+(card.width-s.width)/2;s.y=card.y+card.height+int(std::lround(8*scale));return s;}
// Windows' level bar found in the contents (Glass.cs `bar`), and the strip round it that is drawn over (`barStrip`, `barBase`).
struct Bar {RECT r{};float level=0;bool empty()const{return r.right<=r.left;}};
struct Slider {Bar bar;RECT strip{};std::vector<uint32_t> base;};
inline void lay(float&a,float&c,float la,float lc){a=la+a*(1-la);c=lc+c*(1-la);}
inline float capsule(float px,float py,float x0,float x1,float y,float r){float dy=py-y;if(px>=x0&&px<=x1)return std::fabs(dy)-r;float dx=px<x0?x0-px:px-x1;return std::sqrt(dx*dx+dy*dy)-r;}
inline float pill(float px,float py,float cx,float cy,float hx,float hy){float r=std::min(hx,hy),qx=std::fabs(px-cx)-(hx-r),qy=std::fabs(py-cy)-(hy-r),ox=std::max(qx,0.f),oy=std::max(qy,0.f);return std::sqrt(ox*ox+oy*oy)+std::min(std::max(qx,qy),0.f)-r;}
inline float cover(float d){return d<=-.5f?1.f:(d>=.5f?0.f:.5f-d);}
inline float smooth(float t){t=clamp01(t);return t*t*(3-2*t);}
// Glass.cs DrawSlider: puts the base strip back, then draws the track, fill and knob over it in `px` (premultiplied ARGB, W wide). style: 0 lens, 1 pill, 2 round; t: glass light 0..1.
// Pill and round are the owner's. The lens is LGEngine's own: clear convex glass over the track, which is seen through it refracted (magnified in the middle,
// squeezed into a band at the rim), with a bright rim on top, a caustic glow inside the bottom and a darker edge; it is drawn out along its way by Knob::wob.
inline void drawSlider(uint32_t*px,int W,const Slider&sl,const Knob&kn,int style,float k,float t){
    const RECT&S=sl.strip;const Bar&bar=sl.bar;int sw=S.right-S.left;if(bar.empty()||sw<=0||S.bottom<=S.top||sl.base.size()<size_t(sw)*size_t(S.bottom-S.top))return;
    for(int y=0;y<S.bottom-S.top;++y)memcpy(&px[size_t(S.top+y)*size_t(W)+size_t(S.left)],&sl.base[size_t(y)*size_t(sw)],size_t(sw)*4);
    float barL=float(bar.r.left),barR=float(bar.r.right),cy=float(bar.r.top)+float(bar.r.bottom-bar.r.top)/2.f,trackR=std::max(float(bar.r.bottom-bar.r.top),4*k)/2;bool lens=style==0;float knobHy,knobHx,popUp,drawMost;
    if(lens){knobHy=trackR*3.6f;knobHx=knobHy*1.22f;popUp=.14f;drawMost=.3f;}else if(style==1){knobHy=trackR*2.3f;knobHx=knobHy*1.5f;popUp=.14f;drawMost=.1f;}else{knobHy=knobHx=trackR*2.1f;popUp=.3f;drawMost=0;}
    float x0=barL+trackR,x1=barR-trackR,travel0=barL+knobHx,travel1=std::max(travel0,barR-knobHx);
    float kx=travel0+(travel1-travel0)*std::max(-.05f,std::min(1.05f,kn.at));
    // Hover lifts the knob part way; a press flattens and widens it and presses its shadow in; the wobble back after it (press below 0) stands it a touch taller.
    // Moving draws it out behind: the pill by the glide's flow (capped), the lens by wob (softly capped), which on a stop swings a touch ahead before it settles.
    float hov=lens?std::max(0.f,std::min(1.2f,kn.hover)):0.f,prs=lens?std::max(-.35f,std::min(1.2f,kn.press)):0.f,down=clamp01(prs),bright=std::min(1.f,hov); // (the pill and round styles stay as they were)
    float lift=std::max(clamp01(kn.lift),.55f*hov),clear=lift,grow=1+popUp*lift,sway=lens?kn.wob:kn.flow,pace=std::fabs(sway)*.6f;
    float drawOut=1+(lens?drawMost*pace/(1+pace):std::min(drawMost,std::fabs(kn.flow)*.3f)),squash=1-(lens?.16f:.14f)*prs,spread=1+(lens?.14f:.12f)*prs;
    float rest=knobHx*grow*spread,extra=rest*(drawOut-1),hx=rest+extra,hy=knobHy*grow*squash/std::sqrt(drawOut),kc=kx-(sway>=0?extra:-extra);
    float shadowMost=(.24f-.06f*clear)*(1+.55f*t)*(1+.3f*down),shadowDrop=(1.5f+clear)*k*(1-.45f*down),shadowWide=(3+1.5f*clear)*k*(1-.3f*down);
    float reachX=hx+4.5f*k+1,reachUp=hy+3*k+1,reachDown=hy+7*k+1,outlineA=.14f+.26f*t,shadeBelow=.09f*t;
    // The lens is a capsule round its core line (2*half long, lensR round). A point r of the way out from the core (0..1) shows the place f(r)=r*(lensA+lensB*r*r) out:
    // magnified 1/lensA in the middle (more when lifted, less when pressed flat), f(1)=1.12 at the rim, so the band at the edge shows the track from just past it, squeezed.
    float lensR=std::min(hx,hy),half=std::max(0.f,hx-hy),invR=1/std::max(lensR,.5f),lensA=1/(1.45f+.25f*lift-.12f*down),lensB=1.12f-lensA;
    float tintA=.07f+.05f*bright+.03f*lift,tintC=tintA*(1-.25f*t),edgeA=.14f+.12f*t,glowMost=.2f+.12f*bright+.08f*lift+.06f*down,specMost=.55f+.25f*bright+.2f*down;
    if(lens){shadowMost=(.16f-.04f*lift)*(1+.55f*t)*(1+.3f*down);shadowDrop=(1.2f+1.2f*lift)*k*(1-.45f*down);shadowWide=(2.5f+1.5f*lift)*k*(1-.3f*down);
        reachUp=hy+2*k+1;reachDown=hy+shadowDrop+shadowWide+1;outlineA=.1f+.22f*t;}
    for(int y=S.top;y<S.bottom;++y){float py=float(y)+.5f;
        for(int x=S.left;x<S.right;++x){float pxf=float(x)+.5f,a=0,c=0;
            if(py>cy-trackR-1&&py<cy+trackR+1){float track=cover(capsule(pxf,py,x0,x1,cy,trackR)),fill=cover(capsule(pxf,py,x0,std::min(x1,std::max(x0,kx)),cy,trackR));
                if(track>0)lay(a,c,.3f*track,.3f*track*(1-t));if(fill>0)lay(a,c,fill,fill*(1-t));}
            if(std::fabs(pxf-kc)<reachX&&py>cy-reachUp&&py<cy+reachDown){
                if(lens){float nx=std::max(kc-half,std::min(kc+half,pxf)),vx=pxf-nx,vy=py-cy,rho=std::sqrt(vx*vx+vy*vy),d=rho-lensR,body=cover(d),sy=vy-shadowDrop; // (d: the capsule's distance, rho along its normal)
                    float shade=shadowMost*smooth(1-(std::sqrt(vx*vx+sy*sy)-lensR+.5f)/shadowWide)*(1-body);if(shade>0)lay(a,c,shade,0); // (its shadow shows round it, not through it)
                    if(body>0){float r=std::min(1.f,rho*invR),g=lensA+lensB*r*r,sharp=1/g,lx=kc+(nx-kc)*lensA+vx*g,ly=cy+vy*g; // (sharp: an edge seen magnified stays a pixel soft)
                        float lensTrack=cover(capsule(lx,ly,x0,x1,cy,trackR)*sharp),lensFill=cover(capsule(lx,ly,x0,std::min(x1,std::max(x0,kx)),cy,trackR)*sharp),va=.3f*lensTrack,vc=va*(1-t);
                        lay(va,vc,lensFill,lensFill*(1-t));lay(va,vc,tintA,tintC);float edge=smooth((r-.7f)/.3f);lay(va,vc,edgeA*edge*edge,0); // the track seen through it, the glass, its darker edge
                        float ny=vy/std::max(rho,.001f),up=std::max(0.f,-ny),low=std::max(0.f,ny),glow=glowMost*9.48f*r*r*r*(1-r)*low*low;lay(va,vc,glow,glow); // light gathered inside the bottom
                        float rimLine=cover(std::fabs(d+.9f*k)-.55f*k),spec=std::min(1.f,specMost*(up*up*(.82f*rimLine+.14f*smooth((r-.45f)/.55f))+.18f*rimLine));lay(va,vc,spec,spec); // the rim catching the light, on top
                        a+=(va-a)*body;c+=(vc-c)*body;}
                    float line=outlineA*cover(std::fabs(d)-.5f*k);if(line>0)lay(a,c,line,0);}
                else{float knob=pill(pxf,py,kc,cy,hx,hy),shade=shadowMost*smooth(1-(pill(pxf,py-shadowDrop,kc,cy,hx,hy)+.5f)/shadowWide);
                    if(shade>0)lay(a,c,shade,0);float body=cover(knob);
                    if(body>0){lay(a,c,body,body);float lower=std::max(0.f,(py-cy)/hy);if(shadeBelow>0)lay(a,c,shadeBelow*lower*lower*body,0);}
                    float line=outlineA*cover(std::fabs(knob)-.5f*k);if(line>0)lay(a,c,line,0);}}
            if(a<.002f)continue;
            size_t at=size_t(y)*size_t(W)+size_t(x);uint32_t under=px[at];int ua=int(under>>24)&255;float keep=1-a;int A=int(a*255+float(ua)*keep+.5f);
            if(A<=0){px[at]=0;continue;}if(A>255)A=255;A|=1; // odd: final, left alone by the light-glass flip
            int v=int(c*255+.5f),R=std::min(A,v+int(float((under>>16)&255)*keep)),G=std::min(A,v+int(float((under>>8)&255)*keep)),B=std::min(A,v+int(float(under&255)*keep));
            px[at]=(uint32_t(A)<<24)|(uint32_t(R)<<16)|(uint32_t(G)<<8)|uint32_t(B);}}
}
// Ink: even alpha, channels <= alpha (premultiplied). Slider pixels: odd alpha.
inline bool premultiplied(uint32_t p){uint32_t a=p>>24;return ((p>>16)&255)<=a&&((p>>8)&255)<=a&&(p&255)<=a;}
inline bool isInk(uint32_t p){return p==0||((p>>24)%2==0&&premultiplied(p));}
inline bool isFinal(uint32_t p){return p==0||((p>>24)%2==1&&premultiplied(p));}
// Greyscale-antialiased white text drawn on black: its coverage, and that coverage as ink (premultiplied white, even alpha).
inline uint8_t coverage(uint32_t p){return uint8_t(std::max((p>>16)&255,std::max((p>>8)&255,p&255)));}
inline uint32_t inkOf(uint32_t p){return (uint32_t(coverage(p))&~1u)*0x01010101u;}
// GDI+ for the process: start once before any drawing, shut down after the last bitmap and font are gone.
struct GdiSession {ULONG_PTR token=0;bool ok=false;GdiSession(){Gdiplus::GdiplusStartupInput in;ok=Gdiplus::GdiplusStartup(&token,&in,nullptr)==Gdiplus::Ok;}~GdiSession(){if(ok)Gdiplus::GdiplusShutdown(token);}
    GdiSession(const GdiSession&)=delete;GdiSession&operator=(const GdiSession&)=delete;};
// Glass.cs InkSeen/MarkPatches: the patches of colour in `seen` (aw x ah, 1 where a pixel is colourful): each pixel of a patch marked 2; of a patch big enough to be an icon or a picture, and anything else within its bounds (padded by `pad`), 4 (colour) or 3 (not).
inline void markPatches(std::vector<uint8_t>&seen,int aw,int ah,int pad){
    std::vector<int> stack;std::vector<RECT> boxes;
    for(int i=0;i<aw*ah;i++){if(seen[size_t(i)]!=1)continue;int x0=i%aw,x1=x0,y0=i/aw,y1=y0,n=0;seen[size_t(i)]=2;stack.push_back(i);
        while(!stack.empty()){int at=stack.back(),ax=at%aw,ay=at/aw;stack.pop_back();n++;
            if(ax<x0)x0=ax;else if(ax>x1)x1=ax;if(ay<y0)y0=ay;else if(ay>y1)y1=ay;
            for(int dy=-1;dy<=1;dy++){int ny=ay+dy;if(ny<0||ny>=ah)continue;
                for(int dx=-1;dx<=1;dx++){int nx=ax+dx,ni=ny*aw+nx;if(nx<0||nx>=aw||seen[size_t(ni)]!=1)continue;seen[size_t(ni)]=2;stack.push_back(ni);}}}
        if(n<24)continue; // (a fleck: a coloured fringe of text, say)
        boxes.push_back(RECT{std::max(0,x0-pad),std::max(0,y0-pad),std::min(aw-1,x1+pad)+1,std::min(ah-1,y1+pad)+1});}
    for(const RECT&box:boxes)for(int yy=box.top;yy<box.bottom;yy++)for(int xx=box.left,o=yy*aw;xx<box.right;xx++){uint8_t s=seen[size_t(o+xx)];if(s==0)seen[size_t(o+xx)]=3;else if(s==2)seen[size_t(o+xx)]=4;}}
// Glass.cs MarkInk: marks in the alpha what is to be left alone (odd: inside the bounds of a patch of colour) and what is ink that light glass may turn (even).
inline void markInk(uint32_t*px,int w,int h,RECT area){
    int left=std::max<int>(0,area.left),top=std::max<int>(0,area.top),right=std::min<int>(w,area.right),bottom=std::min<int>(h,area.bottom);if(right<=left||bottom<=top)return;
    int aw=right-left,ah=bottom-top;std::vector<uint8_t> seen(size_t(aw)*size_t(ah),0);const uint8_t Kept=2;
    for(int y=top;y<bottom;y++)for(int x=left;x<right;x++){uint32_t p=px[size_t(y)*size_t(w)+size_t(x)];int a=int(p>>24)&255;if(a<96)continue;
        int r=int(p>>16)&255,g=int(p>>8)&255,b=int(p)&255,mx=r>g?(r>b?r:b):(g>b?g:b),mn=r<g?(r<b?r:b):(g<b?g:b);
        if((mx-mn)*255>56*a)seen[size_t(y-top)*size_t(aw)+size_t(x-left)]=1;}
    markPatches(seen,aw,ah,2); // each patch of colour (touching pixels), and its bounds: a big enough one is an icon
    for(int y=top;y<bottom;y++)for(int x=left;x<right;x++){size_t at=size_t(y)*size_t(w)+size_t(x);uint32_t p=px[at];int a=int(p>>24)&255;if(a==0)continue;
        uint8_t mark=seen[size_t(y-top)*size_t(aw)+size_t(x-left)];int want=mark>=Kept?(a|1):(a&~1);if(want!=a)px[at]=(uint32_t(want)<<24)|(p&0xFFFFFFu);}}
inline std::unique_ptr<Gdiplus::Font> makeFont(const wchar_t*first,const wchar_t*second,float px){
    for(const wchar_t*face:{first,second}){if(!face||!Gdiplus::FontFamily(face).IsAvailable())continue;std::unique_ptr<Gdiplus::Font> f(new Gdiplus::Font(face,px,Gdiplus::FontStyleRegular,Gdiplus::UnitPixel));if(f->IsAvailable())return f;} // (Font(name) itself falls back to sans serif silently)
    return std::unique_ptr<Gdiplus::Font>(new Gdiplus::Font(Gdiplus::FontFamily::GenericSansSerif(),px,Gdiplus::FontStyleRegular,Gdiplus::UnitPixel));}
// Glass.cs DrawIndicatorSample: the stand-in contents of the volume/brightness indicator, a symbol, a bar and a number (premultiplied). glyph: U+E767 volume (the owner's), U+E74F muted, U+E706 brightness.
inline std::vector<uint32_t> drawIndicatorSample(int w,int h,float k,int percent,wchar_t glyph=wchar_t(0xE767)){
    using namespace Gdiplus;std::vector<uint32_t> px(w>0&&h>0?size_t(w)*size_t(h):0,0);if(px.empty())return px;
    {Bitmap bmp(w,h,PixelFormat32bppPARGB);
     {Graphics g(&bmp);auto symbol=makeFont(L"Segoe Fluent Icons",L"Segoe MDL2 Assets",16.f);auto number=makeFont(L"Segoe UI",nullptr,14.f);
      SolidBrush white(Color(255,255,255,255));Pen rest(Color(96,255,255,255),4.f);Pen done(Color(255,76,194,255),4.f); // (Windows' own: its accent colour)
      g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetPixelOffsetMode(PixelOffsetModeHighQuality);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.ScaleTransform(k,k);
      float cw=float(w)/k,ch=float(h)/k,mid=ch/2.f;
      rest.SetStartCap(LineCapRound);rest.SetEndCap(LineCapRound);done.SetStartCap(LineCapRound);done.SetEndCap(LineCapRound);
      // Laid out evenly: the speaker and the number as far in from either end, each centred on the middle line, and the bar between them. (Measured without the padding text is usually drawn with, which would push one in further than the other.)
      const float Edge=18.f,Gap=14.f;const StringFormat*tight=StringFormat::GenericTypographic();int pc=std::max(0,std::min(100,percent));
      std::wstring said=std::to_wstring(pc);wchar_t sym[2]={glyph,0};RectF area(0,0,100,999999);
      auto measure=[&](const wchar_t*text,const Font*font){RectF b;g.MeasureString(text,-1,font,area,tight,&b);return SizeF(b.Width,b.Height);};
      SizeF symbolSize=measure(sym,symbol.get()),widest=measure(L"100",number.get()),saidSize=measure(said.c_str(),number.get());
      g.DrawString(sym,-1,symbol.get(),PointF(Edge,mid-symbolSize.Height/2.f),tight,&white); // the speaker
      float from=Edge+symbolSize.Width+Gap,to=std::max(from+20,cw-Edge-widest.Width-Gap),level=from+(to-from)*float(pc)/100.f;
      g.DrawLine(&rest,from,mid,to,mid);g.DrawLine(&done,from,mid,level,mid);
      g.DrawString(said.c_str(),-1,number.get(),PointF(cw-Edge-saidSize.Width,mid-saidSize.Height/2.f),tight,&white);}
     BitmapData data;Rect all(0,0,w,h);
     if(bmp.LockBits(&all,ImageLockModeRead,PixelFormat32bppPARGB,&data)==Ok){for(int y=0;y<h;++y)memcpy(&px[size_t(y)*size_t(w)],static_cast<const uint8_t*>(data.Scan0)+ptrdiff_t(y)*ptrdiff_t(data.Stride),size_t(w)*4);bmp.UnlockBits(&data);}}
    markInk(px.data(),w,h,RECT{0,0,w,h});return px;}
// Glass.cs FindBar/Coloured/Inked: Windows' level bar in the contents (within area): the run of its accent colour (the level) and the grey track carrying on from it, across the middle. Found if it is a long, thin bar; level is how far along it the colour reaches.
inline bool coloured(uint32_t p){int a=int(p>>24)&255;if(a<160)return false;int r=int(p>>16)&255,g=int(p>>8)&255,b=int(p)&255,mx=r>g?(r>b?r:b):(g>b?g:b),mn=r<g?(r<b?r:b):(g<b?g:b);return (mx-mn)*255>60*a;}
inline bool inked(uint32_t p){return int((p>>24)&255)>=24;}
inline bool findBar(const uint32_t*px,int w,int h,RECT area,float k,RECT&found,float&level){
    found=RECT{};level=0;int aL=area.left,aT=area.top,aR=area.right,aB=area.bottom,aw=aR-aL,ah=aB-aT,left=std::max(0,aL),right=std::min(w,aR);
    int top=std::max(0,aT+ah/4),bottom=std::min(h,aB-ah/4),bestRow=-1,bestRun=0,fillL=0,fillR=-1;
    for(int y=top;y<bottom;y++){int row=y*w,runStart=-1;
        for(int x=left;x<=right;x++){bool col=x<right&&coloured(px[size_t(row+x)]);
            if(col&&runStart<0)runStart=x;else if(!col&&runStart>=0){if(x-runStart>bestRun){bestRun=x-runStart;bestRow=y;fillL=runStart;fillR=x-1;}runStart=-1;}}}
    int barL,barR,barY;
    if(float(bestRun)>=3*k){barY=bestRow;barL=fillL;barR=fillR;
        for(int x=fillR+1,gap=0;x<right&&gap<=2;x++){if(inked(px[size_t(barY*w+x)])){barR=x;gap=0;}else gap++;} // the track, carrying on from it
    }else{ // no colour (the level at 0, or a grey accent): the longest thin run of ink
        bestRun=0;barY=-1;barL=barR=0;
        for(int y=top;y<bottom;y++){int row=y*w,runStart=-1;
            for(int x=left;x<=right;x++){bool ink=x<right&&inked(px[size_t(row+x)]);
                if(ink&&runStart<0)runStart=x;else if(!ink&&runStart>=0){if(x-runStart>bestRun){bestRun=x-runStart;barY=y;barL=runStart;barR=x-1;}runStart=-1;}}}
        if(barY<0)return false;
        fillR=barL-1;for(int x=barL;x<=barR&&(int((px[size_t(barY*w+x)]>>24)&255)>=200);x++)fillR=x; // (a solid start: the level, in grey)
        if(fillR-barL<2)fillR=barL-1;}
    int width=barR-barL+1;if(width<aw/4)return false;
    int mx=(barL+barR)/2,y0=barY,y1=barY; // how thick: up and down from the middle of it while there is ink
    while(y0-1>=std::max(0,aT)&&inked(px[size_t((y0-1)*w+mx)]))y0--;
    while(y1+1<std::min(h,aB)&&inked(px[size_t((y1+1)*w+mx)]))y1++;
    if(float(y1-y0+1)>10*k)return false;
    found=RECT{barL,y0,barL+width,y1+1};level=clamp01(float(fillR-barL+1)/float(width));return true;}
inline RECT intersect(const RECT&a,const RECT&b){RECT r{std::max(a.left,b.left),std::max(a.top,b.top),std::min(a.right,b.right),std::min(a.bottom,b.bottom)};return r.right<=r.left||r.bottom<=r.top?RECT{}:r;}
// Glass.cs AdoptBar: finds the bar in `content` (W x H), keeps the strip of it that the slider is drawn over (sl.base) and clears the bar out of the content. False (sl.bar empty, content left alone) if no bar was found.
inline bool adoptBar(std::vector<uint32_t>&content,int W,int H,float k,Slider&sl){
    RECT area{0,0,W,H},found{};float level=0;
    if(content.size()!=size_t(W)*size_t(H)||!findBar(content.data(),W,H,area,k,found,level)){sl.bar=Bar{};sl.strip=RECT{};return false;}
    RECT seen=found;const Bar&bar=sl.bar;int fw=found.right-found.left,fh=found.bottom-found.top;
    if(!bar.empty()&&std::abs(int(found.left-bar.r.left))<=2&&std::abs(int(found.right-bar.r.right))<=2&&std::abs(int(found.top-bar.r.top))<=1){
        // (the same bar as before, give or take its softened edges: kept as it was, so that the knob's travel does not change with every reading)
        level=clamp01((float(found.left)+level*float(fw)-float(bar.r.left))/std::max(1.f,float(bar.r.right-bar.r.left)));found=bar.r;fw=found.right-found.left;fh=found.bottom-found.top;}
    int knobH=int(std::ceil(std::max(float(fh),4.f*k)*3.6f*1.45f))+int(14*k),cy=found.top+fh/2; // (the lens: 3.6 track radii, lifted and pressed taller, and its shadow)
    // (room either side for the lifted knob and its shadow at either end of the bar)
    int sx=found.left-int(12*k),sy=cy-knobH/2;RECT strip=intersect(RECT{sx,sy,sx+fw+int(24*k),sy+knobH},area);
    // (out with the bar: a pixel or two round it too, for its softened edges)
    RECT u{std::min(seen.left,found.left)-2,std::min(seen.top,found.top)-2,std::max(seen.right,found.right)+2,std::max(seen.bottom,found.bottom)+2},clear=intersect(u,area);
    for(int y=clear.top;y<clear.bottom;y++)memset(&content[size_t(y)*size_t(W)+size_t(clear.left)],0,size_t(clear.right-clear.left)*4);
    int sw=strip.right-strip.left,sh=strip.bottom-strip.top;sl.base.assign(size_t(std::max(0,sw))*size_t(std::max(0,sh)),0);
    for(int y=0;y<sh;y++)memcpy(&sl.base[size_t(y)*size_t(sw)],&content[size_t(strip.top+y)*size_t(W)+size_t(strip.left)],size_t(sw)*4);
    sl.bar.r=found;sl.bar.level=level;sl.strip=strip;return true;}
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
