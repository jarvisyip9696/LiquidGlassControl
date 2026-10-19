// Console test of the LGEngine C++ client's wire layer. Needs no host: it checks golden bytes (little-endian; str = u16 length +
// UTF-8 bytes), the dirty-plan helper, the re-create rule, the backoff and valid() on the protocol 1.1 fields.
#define _WIN32_WINNT 0x0A00
#define UNICODE
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdio>
#include "../integrations/LGEngine/client/lg_client.h"
namespace {
using Bytes=std::vector<unsigned char>;
unsigned checks=0,failures=0;
void check(bool ok,const char*text){++checks;if(!ok)++failures;printf("%s %s\n",ok?"PASS":"FAIL",text);}
bool same(const lghost::wire::Writer&w,const Bytes&golden){return w.bytes==golden;}
Bytes cat(std::initializer_list<Bytes> parts){Bytes all;for(auto&p:parts)all.insert(all.end(),p.begin(),p.end());return all;}
bool rectIs(const RECT&r,LONG l,LONG t,LONG rt,LONG b){return r.left==l&&r.top==t&&r.right==rt&&r.bottom==b;}
}
int main(){
    using namespace lghost;
    // Hello: u16 min=1, u16 max=1, str "LiquidGlassControl C++" (22 bytes = 0x16), u16 clientMinor.
    Bytes name{'L','i','q','u','i','d','G','l','a','s','s','C','o','n','t','r','o','l',' ','C','+','+'};
    check(same(wire::hello(1),cat({{1,0,1,0,0x16,0},name,{1,0}})),"Hello carries clientMinor 1");
    check(same(wire::hello(0),cat({{1,0,1,0,0x16,0},name,{0,0}})),"Hello with minor 0");
    // Frame header: "LGE1", version 1, type 17, length 4, seq 5, then the payload.
    check(wire::frame(17,5,wire::cut(7))==Bytes({0x4c,0x47,0x45,0x31,1,0,17,0,4,0,0,0,5,0,0,0,7,0,0,0}),"Cut frame (type 17)");
    check(same(wire::cut(9),{9,0,0,0}),"Cut payload");
    // SetLook (6): id, 8 x i32 (dark 35, blur 30, refraction 80, rim 60, shadow 28, round 12, intensity 100, elevation 50), str "ab".
    check(same(wire::setLook(0x01020304,Look(),"ab"),{4,3,2,1, 0x23,0,0,0, 0x1e,0,0,0, 0x50,0,0,0, 0x3c,0,0,0, 0x1c,0,0,0, 0x0c,0,0,0, 0x64,0,0,0, 0x32,0,0,0, 2,0,'a','b'}),"SetLook payload");
    // Resize (7): id, w=300, h=200, flags=1 (flow).
    check(same(wire::resize(9,300,200,1),{9,0,0,0, 0x2c,1,0,0, 0xc8,0,0,0, 1,0,0,0}),"Resize payload with flow flag");
    check(same(wire::resize(9,300,200),{9,0,0,0, 0x2c,1,0,0, 0xc8,0,0,0, 0,0,0,0}),"Resize payload without flags");
    // SetMotionEx (10): id, zoom 10, liquid 60, bounce 45, travel -1, indicator 8.
    check(same(wire::setMotionEx(2,10,60,45,-1,8),{2,0,0,0, 10,0,0,0, 0x3c,0,0,0, 0x2d,0,0,0, 0xff,0xff,0xff,0xff, 8,0,0,0}),"SetMotionEx payload");
    // SetSlosh (11): id, style 2, amount 100.
    check(same(wire::setSlosh(3,2,100),{3,0,0,0, 2,0,0,0, 0x64,0,0,0}),"SetSlosh payload");
    // Kick (12): id, f32 (2.5f = 0x40200000, -1.0f = 0xbf800000).
    check(same(wire::kick(4,2.5f),{4,0,0,0, 0,0,0x20,0x40}),"Kick payload +2.5");
    check(same(wire::kick(4,-1.f),{4,0,0,0, 0,0,0x80,0xbf}),"Kick payload -1");
    // SetButtons (13): id, u8 carry, u16 count; per button hit x,y,w,h, f32 cx,cy,hx,hy,rad, i32 style, mark, box x,y,w,h, u32 maskLen + bytes.
    Button one;one.hit=RECT{10,20,110,70};one.cx=1;one.cy=2;one.hx=3;one.hy=4;one.rad=.5f;one.style=1;one.mark=2;one.maskBox=RECT{0,0,2,2};one.mask={1,2,3,4};
    check(same(wire::setButtons(5,{one},true),cat({{5,0,0,0, 1, 1,0},
        {0x0a,0,0,0, 0x14,0,0,0, 0x64,0,0,0, 0x32,0,0,0},
        {0,0,0x80,0x3f, 0,0,0,0x40, 0,0,0x40,0x40, 0,0,0x80,0x40, 0,0,0,0x3f},
        {1,0,0,0, 2,0,0,0},{0,0,0,0, 0,0,0,0, 2,0,0,0, 2,0,0,0},{4,0,0,0, 1,2,3,4}})),"SetButtons payload with a mask");
    check(same(wire::setButtons(5,{},false),{5,0,0,0, 0, 0,0}),"SetButtons with no buttons");
    Button big;big.maskBox=RECT{0,0,200,150};big.mask.assign(30000,7);
    check(wire::setButtons(1,{big,big},false).bytes.size()==60135,"two 30000-byte masks fit 64 KB and are kept");
    auto dropped=wire::setButtons(1,{big,big,big},false);
    check(dropped.bytes.size()==7+3*64&&dropped.bytes.size()<=65536,"masks are dropped when the frame would exceed 64 KB");
    // SetPointerArea (14): id, x, y, w, h.
    check(same(wire::setPointerArea(6,RECT{10,20,110,70}),{6,0,0,0, 0x0a,0,0,0, 0x14,0,0,0, 0x64,0,0,0, 0x32,0,0,0}),"SetPointerArea payload");
    // SetUpdates (15): id, fps 30, adapt 1, savePower 0.
    check(same(wire::setUpdates(7,30,true,false),{7,0,0,0, 0x1e,0,0,0, 1, 0}),"SetUpdates payload");
    // SetBusy (16).
    check(same(wire::setBusy(8,true),{8,0,0,0, 1}),"SetBusy payload");
    // Create writes zoom/liquid/bounce at byte 42 (4 cookie + 1 kind + 4 scale + 16 size/place + 1 flags + 16 fps/motion/in/out).
    Spec spec;spec.zoom=20;spec.liquid=30;spec.bounce=40;auto created=wire::create(1,spec).bytes;
    check(created.size()>54&&Bytes(created.begin()+42,created.begin()+54)==Bytes({20,0,0,0, 30,0,0,0, 40,0,0,0}),"Create writes zoom, liquid and bounce");
    Spec def;auto plain=wire::create(1,def).bytes;
    check(Bytes(plain.begin()+42,plain.begin()+54)==Bytes({10,0,0,0, 0x3c,0,0,0, 0x2d,0,0,0}),"Create defaults stay 10/60/45");
    // Kick push: way*4.4*amount/60; pulse forces way=1; none for amount<=0 or style>=3.
    check(std::fabs(wire::kickPush(0,60,-1)+4.4f)<1e-5f&&std::fabs(wire::kickPush(1,30,1)-2.2f)<1e-5f,"kickPush lean/bounce scale with amount");
    check(std::fabs(wire::kickPush(2,60,-1)-4.4f)<1e-5f,"kickPush pulse forces way 1");
    check(wire::kickPush(0,0,1)==0.f&&wire::kickPush(3,60,1)==0.f&&wire::kickPush(1,-5,1)==0.f,"kickPush is 0 for amount<=0 or style>=3");
    // Dirty plan on a 100x50 card.
    wire::DirtyPlan plan;plan.reset(100,50);
    check(rectIs(plan.next(RECT{10,10,30,30},1,100,50),0,0,100,50),"first publish copies the whole card into the blank back buffer");
    check(rectIs(plan.next(RECT{30,30,70,70},0,100,50),0,0,100,50),"second publish: the other buffer is still blank, whole card");
    check(rectIs(plan.next(RECT{0,0,5,5},1,100,50),0,0,70,50),"third publish: previous dirty united with this one, clipped to the card");
    check(rectIs(plan.next(RECT{50,10,60,20},0,100,50),0,0,60,20),"fourth publish: union of the two dirty rects");
    check(wire::rectEmpty(plan.next(RECT{},1,100,50))&&wire::rectEmpty(plan.next(RECT{200,200,300,300},1,100,50)),"empty or off-card dirty copies nothing");
    check(rectIs(plan.next(RECT{-10,-10,500,500},1,100,50),0,0,100,50),"dirty is clipped to the card");
    // Re-create rule.
    Spec a,b;
    check(!wire::needsCreate(a,b,0)&&!wire::needsCreate(a,b,1),"identical specs never re-create");
    b=a;b.kind=1;check(wire::needsCreate(a,b,1),"kind re-creates");
    b=a;b.scale=2;check(wire::needsCreate(a,b,1),"scale re-creates");
    b=a;b.label="x";check(wire::needsCreate(a,b,1),"label re-creates");
    b=a;b.inShots=true;check(wire::needsCreate(a,b,1),"inShots re-creates");
    b=a;b.look.dark=1;b.profile="p";b.width=50;b.x=5;b.motion=2;b.travel=3;b.indicator=2;check(!wire::needsCreate(a,b,0)&&!wire::needsCreate(a,b,1),"look, profile, size, place, motion, travel and indicator are in place");
    b=a;b.fps=30;check(!wire::needsCreate(a,b,1)&&wire::needsCreate(a,b,0),"fps is in place on 1.1, Create-only on 1.0");
    b=a;b.zoom=20;check(!wire::needsCreate(a,b,1)&&wire::needsCreate(a,b,0),"zoom is in place on 1.1, Create-only on 1.0");
    b=a;b.adapt=false;b.savePower=false;b.liquid=1;b.bounce=1;check(!wire::needsCreate(a,b,1)&&wire::needsCreate(a,b,0),"adapt/savePower/liquid/bounce likewise");
    // Backoff: 100, 400, 1000, 2000, then doubling, capped at 60 s; rnd%51 picks 75%..125%.
    unsigned expect[]={100,400,1000,2000,4000,8000,16000,32000,60000,60000};bool ok=true;
    for(unsigned i=0;i<10;++i)ok=ok&&wire::backoffMs(i,25)==expect[i];
    check(ok,"backoff sequence at 100% jitter");
    check(wire::backoffMs(0,0)==75&&wire::backoffMs(0,50)==125&&wire::backoffMs(1,0)==300&&wire::backoffMs(3,50)==2500,"backoff jitter spans -25%..+25%");
    check(wire::backoffMs(8,50)==60000&&wire::backoffMs(1000,50)==60000&&wire::backoffMs(7,50)==40000,"backoff is capped at 60 s");
    // valid() on the new fields.
    Spec v;check(wire::valid(v),"default spec is valid");
    auto bad=[&](void(*mutate)(Spec&)){Spec s;mutate(s);return !wire::valid(s);};
    check(bad([](Spec&s){s.zoom=51;})&&bad([](Spec&s){s.zoom=-1;})&&bad([](Spec&s){s.liquid=101;})&&bad([](Spec&s){s.bounce=-1;}),"zoom/liquid/bounce out of range are invalid");
    check(bad([](Spec&s){s.travel=-2;})&&bad([](Spec&s){s.travel=161;})&&bad([](Spec&s){s.indicator=9;})&&bad([](Spec&s){s.indicator=-2;}),"travel/indicator out of range are invalid");
    v.zoom=50;v.liquid=100;v.bounce=0;v.travel=160;v.indicator=8;check(wire::valid(v),"range ends are valid");
    v.travel=-1;v.indicator=-1;v.zoom=0;check(wire::valid(v),"-1 travel/indicator (the spec's) and zoom 0 are valid");
    v.profile="";check(wire::valid(v),"empty profile is valid");v.profile="Some Preset";check(wire::valid(v),"a bare preset name is valid");
    v.profile="name=x;top=0,0;slab=2";check(wire::valid(v),"a key=value profile with top= is valid");
    check(bad([](Spec&s){s.profile="name=x;slab=2";})&&bad([](Spec&s){s.profile=std::string("a\0b",3);})&&bad([](Spec&s){s.profile=std::string(2401,'a');}),"profile without top=, with NUL or over 2400 is invalid");
    // setButtons validation.
    std::vector<Button> list(33);check(!wire::buttonsValid(list),"more than 32 buttons are invalid");
    list.resize(32);check(wire::buttonsValid(list),"32 buttons are valid");
    Button nan;nan.hx=std::nanf("");check(!wire::buttonsValid({nan}),"a NaN float is invalid");
    Button neg;neg.rad=-1;check(!wire::buttonsValid({neg}),"a negative radius is invalid");
    Button wrong;wrong.maskBox=RECT{0,0,2,2};wrong.mask={1,2,3};check(!wire::buttonsValid({wrong}),"a mask that does not match its box is invalid");
    Button huge;huge.hit=RECT{0,0,5000,10};check(!wire::buttonsValid({huge}),"a hit rect wider than 4096 is invalid");
    printf("wire: %u/%u checks OK\n",checks-failures,checks);return failures?1:0;
}
