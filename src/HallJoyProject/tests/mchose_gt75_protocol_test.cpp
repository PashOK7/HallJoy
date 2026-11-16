#include "../HallJoy/mchose_gt75_protocol.h"
#include <cassert>
int main(){
    using namespace halljoy::gt75;
    assert(Positions().size()==81);unsigned fn=0;
    for(auto slot:Positions()){assert(slot<96);fn+=Factory[slot]==0x409;for(auto other:Positions())assert(slot==other || Factory[slot]!=Factory[other]);}
    assert(fn==1);
    Base b{};b[0]=255;b[1]=255;b[2]=255;assert(Profile(b)==0);b[2]=3;assert(Profile(b)==3);
    auto q=Request(3,56);auto r=q;r[1]=0xaa;r[5]=2;r[9]=8;r[10]=1;r[4]=Checksum(r);assert(Reply(q,r,65,true));assert(!Reply(q,r,64,true));
    for(auto slot:Positions()){
        Report p{};p[1]=0xa0;const auto hid=Factory[slot];
        if(hid==0x409){p[2]=0xf0;p[3]=255;p[4]=1;}else if(hid>=224){p[2]=16;p[3]=std::uint8_t(1u<<(hid-224));}else{p[2]=16;p[4]=std::uint8_t(hid);}
        p[15]=1;p[16]=85;p[7]=1;p[8]=85;Sample s{};assert(Decode(p,65,s) && s.slot==slot && Milli(s.raw,s.maximum)==1000);
        p[7]=p[8]=0;assert(Decode(p,65,s) && s.raw==0);p[15]=p[16]=0;assert(!Decode(p,65,s));
    }
    auto f=Flag(3,0xa0,true);assert(Offset(f)==199 && f[9]==0xa8 && f[4]==Checksum(f));
}
