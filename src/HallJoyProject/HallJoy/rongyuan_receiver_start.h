#pragma once
#include "rongyuan_receiver_client.h"
namespace halljoy::ry_receiver {
// HE108's interrupt source must not depend on an optional E5 response. The
// pump starts its reader before sending1B; failed/ambiguous enable is cleaned up.
template<class Pump,class Cleanup>
bool StartBeforeTable(unsigned vid,unsigned pid,unsigned board,bool eligible,
                      bool& attempted,bool& active,Pump& pump,Cleanup cleanup){
    if(!He108Trial(vid,pid,board) || attempted)return false;
    attempted=true;active=eligible && pump.Start();
    if(!active){pump.Stop();cleanup();}
    return true;
}
template<class Caps> bool DeclaresReport5(const Caps* caps,unsigned count) noexcept {
    for(unsigned i=0;i<count;++i)if(caps[i].ReportID==5)return true;
    return false;
}
inline bool StreamCollection(unsigned page,unsigned usage,unsigned bytes,bool report5) noexcept {
    return page==0xffff && usage==1 && bytes==32 && report5;
}
}
