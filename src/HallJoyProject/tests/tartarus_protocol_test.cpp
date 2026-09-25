#include "tartarus_protocol.h"
#include "physical_analog_state.h"
#include "keyboard_support_status.h"
#include "support_notice_catalog.h"
#include <cassert>
#include <iostream>
int main() {
 using namespace halljoy;
 tartarus::Values values{};values.fill(777);
 unsigned char report[64]{};report[0]=6;
 assert(!tartarus::Parse(nullptr,21,values));assert(!tartarus::Parse(report,20,values));
 assert(values[0]==777);report[0]=7;assert(!tartarus::Parse(report,24,values));report[0]=6;
 physical_analog::Publication state;bool seen[physical_analog::kHidCount]{};
 for(unsigned i=0;i<20;++i) {
  const auto hid=tartarus::kFactoryHids[i];assert(!seen[hid]);seen[hid]=true;
  assert(state.Bind(static_cast<unsigned char>(i+1),hid));report[i+1]=static_cast<unsigned char>(i+1);
 }
 report[21]=255;report[63]=255;assert(tartarus::Parse(report,64,values));
 for(unsigned i=0;i<20;++i)state.Publish(static_cast<unsigned char>(i+1),values[i],100);
 for(unsigned i=0;i<20;++i)assert(state.Read(tartarus::kFactoryHids[i],60100,~std::uint64_t{0}).milli==values[i]);
 // Each full report includes releases; a released position cannot clear another.
 report[8]=0;assert(tartarus::Parse(report,24,values));
 for(unsigned i=0;i<20;++i)state.Publish(static_cast<unsigned char>(i+1),values[i],60101);
 assert(state.Read(26,60102,~std::uint64_t{0}).milli==0);
 assert(state.Read(4,60102,~std::uint64_t{0}).milli>0);
 unsigned last=0;
 for(unsigned raw=0;raw<256;++raw) {
  report[1]=static_cast<unsigned char>(raw);assert(tartarus::Parse(report,24,values));
  assert(raw==0 || values[0]>last);last=values[0];
 }
 assert(last==1000);state.Clear();assert(!state.Read(4,60102,~std::uint64_t{0}).fresh);
 using namespace keyboard_support;
 assert(NativeNotice(23,0,true)==TartarusPro && NativeNotice(23,0,false)==0);
 SetSearchObservation(true,true,TartarusPro);assert(GetStatusSnapshot().frozenModels==TartarusPro);
 std::cout<<"TARTARUS_PROTOCOL=PASS positions=20 raw_levels=256 malformed release hold disconnect notice\n";
}
