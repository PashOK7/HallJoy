#include "rongyuan_stream_protocol.h"
#include "physical_analog_state.h"
#include "keyboard_support_status.h"
#include "support_notice_catalog.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <iostream>
using namespace halljoy;
int main() {
 using namespace ry_stream;
 const auto cmd=Request(0x1b,1);assert(cmd[0]==0 && cmd[1]==0x1b && cmd[2]==1 && cmd[8]==0xe3);
 assert(Request(0x1b,0)[8]==0xe4);
 Sample sample{7,8};unsigned char packet[32]={5,0x1b,0x81,1,14};
 assert(Parse(packet,32,sample) && sample.slot==14 && sample.raw==385);
 packet[31]=1;assert(Parse(packet,32,sample));packet[31]=0;
 packet[0]=4;assert(!Parse(packet,32,sample));packet[0]=5;
 packet[1]=0x1c;assert(!Parse(packet,32,sample));packet[1]=0x1b;
 assert(!Parse(packet,31,sample));packet[4]=128;assert(!Parse(packet,32,sample));packet[4]=14;
 Report features{};assert(!Units(0,features,false) && !Units(0x300,features,true));
 assert(Units(0x300,features,false)==100 && Units(0x500,features,false)==200);
 features[1]=0xe6;features[2]=0xaa;features[3]=2;assert(Units(0x300,features,true)==1000);
 features[3]=3;assert(!Units(0x300,features,true));
 for(const auto& alias:kUsbAliases){
  assert(Candidate(alias.vid,alias.pid));
  assert(Find(alias.board,alias.vid,alias.pid)==Find(alias.board,alias.canonicalVid,alias.canonicalPid));
  assert(!Find(alias.board,alias.vid^1,alias.pid));
 }
 assert(!Find(2609,12625,20513)); // Historical board-number collision: YC3123.
 assert(!Find(2368,12625,20528)); // Different old factory map, not an alias.
 unsigned revisions=0;
 for(const auto& m:kModels) {
  assert(Find(m.board,m.vid,m.pid)==&m && !Find(m.board,m.vid,m.pid^1));
  assert(!Find(m.board+100000,m.vid,m.pid));
  physical_analog::Publication publication;
  unsigned w=999,a=999,s=999,d=999;
  for(unsigned slot=0;slot<128;++slot) {
   const auto hid=Decode(m.matrix.data()+slot*4);if(!hid)continue;
   assert(publication.Bind(static_cast<unsigned char>(slot+1),hid));
   if(hid==26)w=slot;
   if(hid==4)a=slot;
   if(hid==22)s=slot;
   if(hid==7)d=slot;
  }
  assert(w<128 && a<128 && s<128 && d<128);
  // Every factory position must remain independently addressable, not just WASD.
  unsigned expected[physical_analog::kHidCount]{};
  for(unsigned slot=0;slot<128;++slot) {
   const auto hid=Decode(m.matrix.data()+slot*4);if(!hid)continue;
   expected[hid]=std::max(expected[hid],slot+1);
   publication.Publish(static_cast<unsigned char>(slot+1),static_cast<unsigned short>(slot+1),90);
  }
  for(unsigned slot=0;slot<128;++slot) {
   const auto hid=Decode(m.matrix.data()+slot*4);if(!hid)continue;
   assert(publication.Read(hid,91,~std::uint64_t{0}).milli==expected[hid]);
  }
  // Factory split keys may share a HID. Releasing one must preserve the others.
  for(unsigned slot=0;slot<128;++slot) {
   const auto hid=Decode(m.matrix.data()+slot*4);if(!hid)continue;
   publication.Publish(static_cast<unsigned char>(slot+1),0,92);
   unsigned remaining=0;
   for(unsigned other=slot+1;other<128;++other)
    if(Decode(m.matrix.data()+other*4)==hid)remaining=std::max(remaining,other+1);
   assert(publication.Read(hid,93,~std::uint64_t{0}).milli==remaining);
  }

  publication.Publish(static_cast<unsigned char>(w+1),250,100);
  publication.Publish(static_cast<unsigned char>(a+1),500,101);
  // No delta during a stationary hold must not synthesize a release.
  assert(publication.Read(26,60100,~std::uint64_t{0}).milli==250);
  assert(publication.Read(4,60100,~std::uint64_t{0}).milli==500);
  publication.Publish(static_cast<unsigned char>(w+1),0,60101);
  assert(publication.Read(26,60102,~std::uint64_t{0}).milli==0);
  assert(publication.Read(4,60102,~std::uint64_t{0}).milli==500);
  publication.Clear();assert(!publication.Read(4,60103,~std::uint64_t{0}).fresh);
  ++revisions;
 }
 assert(revisions==253);
 physical_analog::Publication aliases;assert(aliases.Bind(1,26)&&aliases.Bind(2,26));
 aliases.Publish(1,500,100);aliases.Publish(2,700,101);aliases.Publish(2,0,102);
 assert(aliases.Read(26,60000,~std::uint64_t{0}).milli==500);
 const auto fixture=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()/"docs/research/rongyuan-stream/gamakay-capture.bin";
 std::ifstream file(fixture,std::ios::binary);assert(file);
 unsigned frames=0,maximum=0;while(file.read(reinterpret_cast<char*>(packet),32)) {assert(Parse(packet,32,sample));maximum=std::max(maximum,sample.raw);++frames;}
 assert(frames==4898 && maximum==385 && file.gcount()==0);
 using namespace keyboard_support;
 assert(NativeNotice(22,0,true)==RongYuanStream && NativeNotice(22,0,false)==0);
 SetSearchObservation(true,true,RongYuanStream);assert(GetStatusSnapshot().frozenModels==RongYuanStream);
 std::cout<<"RONGYUAN_STREAM=PASS revisions="<<revisions<<" captured_frames="<<frames<<" stationary_hold release aliases disconnect malformed precision identity notices\n";
}
