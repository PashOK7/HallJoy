#include "steelseries_apex_protocol.h"
#include <cassert>
#include "support_notice_catalog.h"
#include <cstring>
#include <string_view>
using namespace halljoy::apex;
static void put(Report& p,unsigned off,unsigned v){p[off]=v&255;p[off+1]=v>>8;}
#include "steelseries_apex_gen3_protocol.h"
#include "generated/layout_pipeline/identities.h"
#include "physical_analog_state.h"
#include "apex_gen3_replay_vectors.h"
namespace tg=halljoy::apex::gen3;
static Report fromhex(const char* text){
 Report p{};unsigned n=1;
 auto nibble=[](char c){return c>='0' && c<='9'?unsigned(c-'0'):unsigned(c-'a'+10);};
 while(*text){assert(text[1] && n<p.size());p[n++]=static_cast<unsigned char>((nibble(text[0])<<4)|nibble(text[1]));text+=2;}return p;
}
static void TestGen3(){
 assert(tg::Identity(0x1038,0x1642));assert(!tg::Identity(0x1038,0x1644));assert(!tg::Identity(0x1038,0x1646));assert(!tg::Identity(0x1038,0x1640));
 assert(tg::Collection(0xffc0,1,65,65,645));assert(!tg::Collection(0xffc0,1,65,65,643));assert(!tg::Collection(0xffc1,1,65,0,0));
 for(unsigned variant=0;variant<3;++variant){
  unsigned physical=0;for(unsigned i=0;i<68;++i)physical+=tg::PhysicalKey(variant,kMap[i]);assert(physical==(variant==0?61u:variant==1?62u:65u));
  auto token=halljoy::layout_identity::Token("steelseries-apex",tg::LayoutProduct(variant));assert(token && halljoy::layout_identity::Match(token));
 }
 assert(!halljoy::layout_identity::Token("steelseries-apex",tg::LayoutProduct(255)));
 assert(!tg::ReadOnlyRequest(DepthRequest(1)));assert(!ReadOnlyRequest(tg::DepthRequest(0,20)));
 unsigned tested=0;
 for(const auto& vector:kApexGen3Replay){
  const auto req=fromhex(vector.request),p=fromhex(vector.reply);const std::string_view kind=vector.kind;
  if(kind=="oversize_refused"){assert(!tg::ReadOnlyRequest(req));assert(p[1]==0xf3 && p[2]==2);continue;}
  assert(tg::ReadOnlyRequest(req));++tested;
  if(kind=="depth" || kind=="ranges" || kind=="live_mapping_model"){
   unsigned start=68;const unsigned stride=kind=="depth"?3:kind=="ranges"?5:6;
   const unsigned count=kind=="ranges"?req[4]:req[3],offset=kind=="ranges"?5:4;
   for(unsigned i=0;i<68;++i)if(kMap[i]==req[offset])start=i;
   assert(start<68 && start+count<=68);Matrix values{};Ranges ranges{};tg::Mappings maps{};
   if(kind=="depth"){
    assert(req==tg::DepthRequest(start,count));assert(tg::ParseDepth(p,start,count,values)==tg::Refusal::None);
    const unsigned level[]={500,550,1800,3200,1800,550,500,500};
    for(unsigned i=0;i<count;++i)assert(values[start+i]==level[vector.phase]+start+i);
    auto damaged=p;damaged[2]=1;const auto prior=values;assert(tg::ParseDepth(damaged,start,count,values)==tg::Refusal::Header && values==prior);
    damaged=p;damaged[5]^=1;assert(tg::ParseDepth(damaged,start,count,values)==tg::Refusal::Hid && values==prior);
    damaged=p;put(damaged,6,4096);assert(tg::ParseDepth(damaged,start,count,values)==tg::Refusal::RawRange && values==prior);
   }else if(kind=="ranges"){
    assert(req==tg::RangeRequest(start,count));assert(tg::ParseRanges(p,start,count,ranges)==tg::Refusal::None);
    for(unsigned i=0;i<count;++i){assert(ranges[start+i].high==3200+start+i);assert(ranges[start+i].low==500+start+i);}
    auto damaged=p;damaged[4]=1;assert(tg::ParseRanges(damaged,start,count,ranges)==tg::Refusal::Namespace);
    damaged=p;damaged[6]^=1;assert(tg::ParseRanges(damaged,start,count,ranges)==tg::Refusal::Hid);
   }else{
    assert(req==tg::MappingRequest(start,count));assert(tg::ParseMappings(p,start,count,maps)==tg::Refusal::None);
    for(unsigned i=0;i<count;++i)assert(maps[start+i].assigned==(kMap[start+i]+1)%255 && maps[start+i].kind==tg::MappingKind::Keyboard);
    auto damaged=p;damaged[5]^=1;const auto prior=maps;assert(tg::ParseMappings(damaged,start,count,maps)==tg::Refusal::Hid && maps==prior);
   }
   for(unsigned i=0;i<count;++i)assert(req[offset+stride*i]==kMap[start+i]);
  }else if(kind=="version"){assert(req==VersionRequest() && tg::Version(p));auto bad=p;bad[3]='9';assert(!tg::Version(bad));}
  else if(req[1]==0xf2){unsigned layout=255;assert(tg::Layout(p,layout) && layout<3);}
  else if(req[1]==0xf5){unsigned region=255;assert(tg::Region(p,region));}
 }
 assert(tested==102);
 Matrix physical{};Ranges calibrated{};
 for(unsigned i=0;i<68;++i){physical[i]=500;calibrated[i]={390,2425};}
 assert(tg::SensorFaults(0,physical,calibrated)==0);
 physical[16]=123;assert(tg::SensorFaults(0,physical,calibrated)==1);
 physical[16]=124;assert(tg::SensorFaults(0,physical,calibrated)==0);
 physical[16]=3970;assert(tg::SensorFaults(0,physical,calibrated)==0);
 physical[16]=3971;assert(tg::SensorFaults(0,physical,calibrated)==1);
 physical[16]=500;physical[13]=0;assert(tg::SensorFaults(0,physical,calibrated)==0);assert(tg::SensorFaults(2,physical,calibrated)==1);
 assert(halljoy::keyboard_support::NativeNotice(25,halljoy::layout_identity::Token("steelseries-apex",tg::LayoutProduct(0)),true,0x1642)==1048576);

 for(unsigned start=0;start<68;++start)for(unsigned count=1;count<=20 && count<=68-start;++count){
  auto req=tg::DepthRequest(start,count);assert(tg::ReadOnlyRequest(req));req[1]=0x73;assert(!tg::ReadOnlyRequest(req));
 }
 auto p=fromhex("b6000001355104000000");tg::Mappings maps{};
 assert(tg::ParseMappings(p,0,1,maps)==tg::Refusal::None && maps[0].assigned==4);
 p[7]=0;p[8]=1;assert(tg::ParseMappings(p,0,1,maps)==tg::Refusal::None && maps[0].assigned==53 && maps[0].kind==tg::MappingKind::Chord);
 p[6]=0x71;assert(tg::ParseMappings(p,0,1,maps)==tg::Refusal::None && maps[0].assigned==53 && maps[0].kind==tg::MappingKind::Macro);
 halljoy::physical_analog::Publication publication;
 assert(publication.Bind(1,4) && publication.Bind(2,4));publication.Publish(1,1000,100);publication.Publish(2,500,100);
 assert(publication.Read(4,100).milli==1000);publication.Publish(1,0,101);assert(publication.Read(4,101).milli==500);
 publication.Publish(2,0,102);assert(publication.Read(4,102).milli==0);assert(!publication.Read(4,203,100).fresh);
}

int main(){
 TestGen3();
 assert(halljoy::keyboard_support::NativeNotice(25,0,true,0x1610)==0);
 assert(halljoy::keyboard_support::NativeNotice(25,0,true,0x1614)==1048576);
 assert(halljoy::keyboard_support::NativeNotice(25,0,true,0x1640)==1048576);
 assert(halljoy::keyboard_support::NativeNotice(25,0,false,0x1640)==0);

 for(unsigned pid:{0x1610u,0x1614u,0x1640u,0x1642u})assert(SupportedIdentity(0x1038,pid));
 for(unsigned pid:{0x1612u,0x1618u,0x161eu,0x1628u,0x1644u,0x1646u,0x1648u})assert(!SupportedIdentity(0x1038,pid));
 assert(!SupportedIdentity(0x1ca6,0x1614));
 assert(SupportedCollection(0xffc0,1,65,65,643));
 assert(!SupportedCollection(0xffb0,1,65,65,643));
 assert(!SupportedCollection(0xffc0,1,65,65,65));

 assert(ReadOnlyRequest(VersionRequest()));Report version{};std::memcpy(version.data()+1,"4.16.8",7);assert(KnownVersion(version));version[6]='9';assert(!KnownVersion(version));
 for(unsigned bank=1;bank<=5;++bank){
  assert(ReadOnlyRequest(DepthRequest(bank)));Report p{};Matrix m{};
  for(unsigned i=0;i<14;++i){put(p,1+2*i,1100+(bank-1)*14+i);put(p,29+2*i,1200+(bank-1)*14+i);}
  assert(ParseDepth(p,bank,m));for(unsigned i=0;i<14;++i)assert(m[(bank-1)*14+i]==1200+(bank-1)*14+i);
  const auto previous=m;p[64]=1;assert(!ParseDepth(p,bank,m));assert(m==previous);p[64]=0;put(p,1,4096);assert(!ParseDepth(p,bank,m));assert(m==previous);
 }
 assert(!ReadOnlyRequest(DepthRequest(0)));assert(!ReadOnlyRequest(DepthRequest(6)));
 Ranges ranges{};
 for(unsigned start=0;start<68;start+=12){
  const auto count=std::min(12u,68-start);auto req=RangeRequest(start,count);assert(ReadOnlyRequest(req));
  auto unsafe=req;unsafe[1]&=0x7f;assert(!ReadOnlyRequest(unsafe));
  Report p{};for(unsigned i=0;i<count;++i){p[1+i*5]=static_cast<unsigned char>(kMap[start+i]);put(p,2+i*5,3000+start+i);put(p,4+i*5,1000+start+i);}
  assert(ParseRanges(p,start,count,ranges));for(unsigned i=0;i<count;++i){assert(ranges[start+i].high==3000+start+i);assert(ranges[start+i].low==1000+start+i);}
  p[1]^=1;assert(!ParseRanges(p,start,count,ranges));
 }
 assert(!ReadOnlyRequest(RangeRequest(0,13)));assert(!ReadOnlyRequest(RangeRequest(67,2)));
 assert(Normalize(1000,{1000,3000})==0);assert(Normalize(2000,{1000,3000})==500);assert(Normalize(4000,{1000,3000})==1000);assert(Normalize(2000,{3000,1000})==0);
 unsigned previous=0;for(unsigned raw=0;raw<=4095;++raw){unsigned v=Normalize(raw,{1000,3000});assert(v>=previous && v<=1000);previous=v;}
 assert(kMap[16]==26 && kMap[29]==4 && kMap[30]==22 && kMap[31]==7);
}
