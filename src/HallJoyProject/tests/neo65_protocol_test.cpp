#include "neo65_protocol.h"
#include "physical_analog_state.h"
#include "support_notice_catalog.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace halljoy::neo65;
int main() {
 assert(halljoy::keyboard_support::NativeNotice(24,0,true)==262144);
 assert(kAnsiMap[75]==0x409 && kIsoMap[75]==0x409);
 for(auto pid:{kAnsi,kIso}) {
  const auto& map=Factory(pid);std::set<unsigned> hids;halljoy::physical_analog::Publication pub;
  for(unsigned i=0;i<80;++i)if(map[i]) {assert(hids.insert(map[i]).second);assert(pub.Bind(i+1,map[i]));}
  assert(hids.size()==(pid==kAnsi?67u:68u));assert(map[18]==26 && map[33]==4 && map[34]==22 && map[35]==7);
  Matrix v{};
  for(unsigned start=0;start<80;start+=14) {
   unsigned count=std::min(80-start,14u);auto p=Request(0xa6,start,count);
   for(unsigned i=0;i<count;++i) {unsigned raw=(start+i)*400;p[5+i*2]=raw>>8;p[6+i*2]=raw&255;}
   assert(ParseDepth(p,start,count,v));
   auto saved=v;p[4]++;assert(!ParseDepth(p,start,count,v));assert(v==saved);p[4]--;p[5]=255;assert(!ParseDepth(p,start,count,v));assert(v==saved);
  }
  for(unsigned i=0;i<80;++i) {assert(v[i]==i*400);if(map[i])pub.Publish(i+1,Normalize(v[i],{0,32000}),100);}
  for(unsigned i=0;i<80;++i)if(map[i])assert(pub.Read(map[i],110,100).milli==Normalize(i*400,{0,32000}));
  pub.Publish(19,0,120);assert(pub.Read(26,120,100).milli==0);assert(pub.Read(4,120,100).milli>0);
  assert(!pub.Read(4,201,100).fresh);pub.Clear();assert(!pub.Owns(4));
 }
 Ranges r{};auto p=Request(0xaa,0,1);p[5]=1;p[6]=0x80;p[7]=0xe8;p[10]=1;p[11]=0x2c;p[12]=1;p[13]=0x90;
 assert(ParseRanges(p,0,1,9,r));assert(r[0].low==300 && r[0].high==32600);
 assert(Normalize(300,r[0])==0 && Normalize(32600,r[0])==1000);
 unsigned prev=0;for(unsigned raw=0;raw<=40000;++raw) {unsigned n=Normalize(raw,r[0]);assert(n>=prev && n<=1000);prev=n;}
 auto old=r;p[10]=0xff;assert(!ParseRanges(p,0,1,9,r));assert(r[0].low==old[0].low);
 assert(!ParseRanges(p,0,1,29,r));Matrix values{};assert(!ParseDepth(p,79,2,values));
 std::cout<<"NEO65_PROTOCOL=PASS all ANSI/ISO keys, range config, malformed pages, release, stale input\n";
}
