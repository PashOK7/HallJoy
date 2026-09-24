#include "rongyuan_snapshot_protocol.h"
#include "slice75_protocol.h"
#include "physical_analog_state.h"
#include "keyboard_support_status.h"
#include "support_notice_catalog.h"
#include "generated/layout_pipeline/identities.h"
#include <cassert>
#include <iostream>
#include <set>
namespace ry=halljoy::rongyuan;
namespace sl=halljoy::slice75;
int main(){
  const auto request=ry::Request(0xe5,0xfe,1,3);
  assert(request[8]==0x18);
  ry::Values values{};assert(!ry::ParseTravel(request,200,values));
  // Every partial forward shared-buffer copy remains invalid, including one
  // with a valid first depth. No previous page is published under a new index.
  ry::Report reply{};
  for(unsigned i=0;i<32;++i){unsigned v=i*20;reply[1+2*i]=v&255;reply[2+2*i]=v>>8;}
  for(unsigned n=0;n<64;++n){auto partial=request;
    std::copy_n(reply.begin()+1,n,partial.begin()+1);
    assert(!ry::ParseTravel(partial,200,values));
  }
  assert(ry::ParseTravel(reply,200,values));assert(values[31]==620);
  const auto mapRequest=ry::Request(0x8a,0,255,0);
  ry::Report mapReply{};std::copy_n(ry::kMatrix2819.begin(),64,mapReply.begin()+1);
  assert(ry::ValidAssignments(mapReply));
  for(unsigned n=0;n<64;++n){auto partial=mapRequest;
    std::copy_n(mapReply.begin()+1,n,partial.begin()+1);
    assert(!ry::ValidAssignments(partial));
  }

  assert(ry::Normalize(360,200,3600)==500);
  assert(ry::Normalize(350,100,3500)==1000);
  assert(ry::Normalize(3500,1000,3500)==1000);
  ry::Report feature{};feature[1]=0xe6;feature[2]=0xaa;
  assert(ry::Units(0x500,feature)==100);feature[3]=1;assert(ry::Units(0x300,feature)==200);
  feature[3]=2;assert(ry::Units(0x300,feature)==1000);feature[3]=3;assert(ry::Units(0x500,feature)==0);
  assert(ry::Units(0x500,{})==200 && ry::Units(0x300,{})==100);
  assert(!ry::Find(2949) && !ry::Find(2308));
  for(const auto& m:ry::kModels){
    std::set<unsigned> keys;unsigned fn=0;
    for(unsigned i=0;i<128;++i){auto hid=ry::Decode(m.matrix->data()+4*i);if(hid)keys.insert(hid);if(hid==0x409)fn=i;}
    assert(keys.size()==(m.board==2819?82:84));assert(fn==(m.board==2819?65:71));
    auto token=halljoy::layout_identity::Token("rongyuan-snapshot",m.product);
    assert(token && halljoy::keyboard_support::NativeNotice(21,token,true)==32768);
  }
  // All physical aliases stay independent; identical samples refresh holds.
  halljoy::physical_analog::Publication pub;
  assert(pub.Bind(1,26) && pub.Bind(2,26));
  for(unsigned t=1;t<=10000;t+=10){pub.Publish(1,500,t);pub.Publish(2,250,t);assert(pub.Read(26,t,150).milli==500);}
  pub.Publish(1,0,10001);assert(pub.Read(26,10001,150).milli==250);
  pub.Publish(2,0,10002);assert(pub.Read(26,10002,150).milli==0);
  pub.Publish(1,1000,10003);assert(pub.Read(26,10154,150).milli==0);
  pub.Clear();assert(!pub.Owns(26));
  // Slice75: all three reports, including the four-byte continuation, required.
  std::array<unsigned char,132> raw{};raw[0]=0x5c;raw[1]=128;raw[2]=0x92;raw[5]=2;
  raw[130]=0xe4;raw[131]=0x0c;raw[3]=sl::Checksum(raw.data());
  sl::Frame frame;
  for(unsigned part=0;part<3;++part){sl::Report r{};
    for(unsigned i=0;i<64 && part*64+i<raw.size();++i)r[i+1]=raw[part*64+i];
    assert(frame.Push(r,65,0x12));assert(frame.Complete()==(part==2));}
  sl::Values v{};assert(sl::ParseTravel(frame,v)&&v[62]==3300);
  assert(sl::Normalize(v[62])==1000 && sl::Normalize(1650)==500);
  unsigned count=0;for(auto h:sl::kFactoryActions)count+=h!=0;assert(count==80);
  assert(sl::kFactoryActions[116]==0xf001);
  assert(sl::ExactModel(0x1ca3,0x0701,L"SLICE75 HE"));
  assert(!sl::ExactModel(0x1ca3,0x0703,L"SLICE75 HE"));
  using namespace halljoy::keyboard_support;
  SetSearchObservation(true,true,Slice75|RongYuan);
  assert(GetStatusSnapshot().frozenModels==(Slice75|RongYuan));
  SetSearchObservation(false,false,Slice75|RongYuan);assert(!GetStatusSnapshot().frozenModels);
  std::cout<<"THREE_KEYBOARD_PROTOCOL=PASS\n";
}
