#pragma once
#include <algorithm>
#include <cmath>
namespace uiMotion {
inline bool advance(float&position,float&velocity,float target,float elapsed,float duration){
    float dt=std::max(0.f,std::min(.1f,elapsed)),omega=5.f/duration;
    float offset=position-target,j=velocity+omega*offset,e=std::exp(-omega*dt);
    position=target+(offset+j*dt)*e;velocity=(velocity-omega*j*dt)*e;
    if(position<0||position>1){position=std::max(0.f,std::min(1.f,position));velocity=0;}
    if(std::abs(position-target)<.001f&&std::abs(velocity)<.01f){position=target;velocity=0;return false;}return true;
}
struct Toggle {
    float position=0,velocity=0,target=0;bool initialized=false;
    void set(bool on,bool animate){target=on?1.f:0.f;if(!initialized||!animate){position=target;velocity=0;initialized=true;}}
    bool moving()const{return position!=target||velocity!=0;}
    bool tick(float elapsed){return advance(position,velocity,target,elapsed,.24f);}
};
inline bool test(){
    Toggle toggle;toggle.set(false,true);toggle.set(true,true);
    if(toggle.position!=0||!toggle.moving())return false;
    for(int i=0;i<6;++i)toggle.tick(1/60.f);
    float middle=toggle.position;if(middle<=0||middle>=1)return false;
    toggle.set(false,true);if(toggle.position!=middle)return false;
    for(int i=0;i<180;++i)toggle.tick(1/60.f);
    if(toggle.moving()||toggle.position!=0)return false;
    toggle.set(true,false);if(toggle.moving()||toggle.position!=1)return false;
    float a=0,av=0,b=0,bv=0;
    for(int i=0;i<6;++i)advance(a,av,1,1/60.f,.24f);
    for(int i=0;i<12;++i)advance(b,bv,1,1/120.f,.24f);
    if(std::abs(a-b)>.0001f||a<=0||a>=1)return false;
    float old=a;advance(a,av,0,0,.24f);if(a!=old)return false;
    bool active=true;for(int i=0;i<180;++i){active=advance(a,av,0,1/60.f,.24f);if(a<0||a>1||!std::isfinite(a)||!std::isfinite(av))return false;}
    return !active&&a==0&&av==0;
}
}
