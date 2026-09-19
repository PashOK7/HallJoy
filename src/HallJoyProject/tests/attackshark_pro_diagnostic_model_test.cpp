#include "attackshark_pro_diagnostic_model.h"
#include <cassert>
using namespace halljoy::sharkdiag;
int main(){
 for(unsigned command=0;command<256;++command){auto r=Request(command);assert(bool(r[1])==(command==0x8f || command==0x80 || command==0xe5));}
 for(unsigned page=0;page<4;++page){auto r=Request(0xe5,page);unsigned sum=0;for(unsigned i=1;i<=8;++i)sum+=r[i];assert((sum&255)==255);assert(r[8]==0x1b-page);}
 assert(Request(0xe5,4)[1]==0);
 Report r{};r[1]=0x8f;r[2]=4;r[3]=9;assert(Identity(r)==2308);assert(Known(2308,0x502f));assert(!Known(2308,0x5030));assert(!Known(2268,0x502f));
 Metrics m;r.fill(0);auto put=[&](unsigned slot,unsigned value){r[1+slot*2]=value&255;r[2+slot*2]=value>>8;};
 for(unsigned p=0;p<4;++p)assert(m.Add(p,r,0));
 for(unsigned slot:{14,9,15,21})put(slot,100);
 assert(m.Add(0,r,15));put(14,200);assert(m.Add(0,r,15));assert(m.independentChanges==1);
 put(14,0);assert(m.Add(0,r,14));assert(m.keys[14].released==1 && m.keys[9].last==100);
 for(unsigned slot:{9,15,21})put(slot,0);
 assert(m.Add(0,r,0));assert(m.Enough(4,4));assert(!m.Enough(4,3));assert(m.peakPage0==4);
 std::array<unsigned,32> values{};
 auto echo=Request(0xe5,2);assert(!Decode(echo,values));r.fill(255);assert(!Decode(r,values));r.fill(0);r[0]=5;assert(!Decode(r,values));
 r.fill(0);put(0,4097);assert(!m.Add(0,r,0));assert(m.keys[14].released==1);assert(!m.Add(4,r,0));
}
