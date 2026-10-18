#pragma once
#include <cstdint>
#include <limits>
namespace lghost {
// Serial capture ownership. A late completion cannot release a newer request.
class NativeCaptureLane {
    uint64_t serial=0,current=0,deadline=0;bool waiting=false;
public:
    bool busy()const{return waiting;}
    uint64_t begin(uint64_t now,uint64_t timeout=1500){
        if(waiting)return 0;if(++serial==0)++serial;current=serial;waiting=true;
        deadline=timeout>std::numeric_limits<uint64_t>::max()-now?std::numeric_limits<uint64_t>::max():now+timeout;return current;
    }
    bool complete(uint64_t token){if(!waiting||token!=current)return false;waiting=false;return true;}
    bool expire(uint64_t now){if(!waiting||now<deadline)return false;waiting=false;return true;}
    void cancel(){waiting=false;}
};
inline bool testNativeCaptureLane(){
    NativeCaptureLane lane;auto first=lane.begin(100);
    if(!first||lane.begin(110)||lane.expire(1599)||!lane.expire(1600)||lane.busy())return false;
    auto second=lane.begin(1601);if(second==first||lane.complete(first)||!lane.busy()||!lane.complete(second))return false;
    auto third=lane.begin(2000);lane.cancel();if(lane.complete(third)||lane.busy())return false;
    auto fourth=lane.begin(std::numeric_limits<uint64_t>::max()-10,100);
    return fourth!=third&&!lane.expire(std::numeric_limits<uint64_t>::max()-1)&&lane.complete(fourth)&&!lane.complete(fourth);
}
}
