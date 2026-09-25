#include "attackshark_pro_diagnostic_model.h"
#include <cassert>
#include "attackshark_pro_native_model.h"
using namespace halljoy::sharkdiag;
int main(){
 using namespace halljoy::sharkplay;
 assert(Factory[65]==1033);
 assert(std::size(Profiles)==37);
 for(const auto& p:Profiles){
  assert(CandidatePid(p.pid));
  for(unsigned pid:{0x5029u,0x502du,0x502fu,0x5030u})assert(Known(p.id,pid)==(p.pid==pid || (p.id==3650 && pid==0x5029)));
  unsigned counts[4]{};for(unsigned cycle=0;cycle<128;++cycle)++counts[Page(cycle,p.fnSlot/32,UsesFourthPage(p))];
  for(unsigned slot=0;slot<128;++slot)if(p.factory[slot] || p.fn[slot])assert(counts[slot/32]>=30);
 }
 assert(!CandidatePid(0x9999) && !Known(1466,0x502d));
 assert(FreshBudget(1)==150 && FreshBudget(5)==200 && FreshBudget(10)==300);
 assert(Fresh(0,200,FreshBudget(10)) && !Fresh(0,301,FreshBudget(10)));
 for(const auto& p:Profiles){
  assert(p.factory[p.fnSlot]==1033 && p.factory[127]==0);
  assert(Known(p.id,p.pid));assert(p.factory[14]==26 && p.factory[9]==4);
  for(unsigned slot=0;slot<128;++slot)if(p.fn[slot]){
   LayerState l;assert(l.Route(slot,175,true,p)==p.fn[slot]);
   assert(l.Route(slot,175,false,p)==p.fn[slot]);l.Route(slot,0,false,p);
   assert(l.Route(slot,175,false,p)==p.factory[slot]);
  }
 }
 assert(Milli(350,200)==500 && Milli(700,200)==1000 && Milli(35,10)==1000);

 for(unsigned n=0;n<12;++n){
  LayerState layers;const unsigned slot=7+6*n,base=Factory[slot],f=58+n;
  assert(layers.Route(slot,175,true)==f);
  assert(layers.Route(slot,175,false)==f);
  assert(layers.Route(slot,0,false)==base);
  assert(layers.Route(slot,175,false)==base);
  assert(layers.Route(slot,175,true)==base);
  assert(layers.Route(slot,0,true)==base);
  assert(layers.Route(slot,175,true)==f);
  assert(layers.Route(slot,0,true)==base);
 }

 assert(Supported(2308,0x314) && Supported(2938,0x500) && !Supported(9999,0x500) && !Supported(2308,0));
 assert(Factory[14]==26 && Factory[9]==4 && Factory[15]==22 && Factory[21]==7 && Factory[127]==0);
 assert(Milli(0)==0 && Milli(175)==500 && Milli(350)==1000 && Milli(400)==1000);
 assert(Fresh(0xfffffff0,10) && !Fresh(0xfffffff0,200));
 unsigned count[4]{};for(unsigned i=0;i<128;++i){auto page=Page(i);assert(page<4);++count[page];}
 assert(count[0]>count[1] && count[1]>=30 && count[2]>=30 && count[3]==1);

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
