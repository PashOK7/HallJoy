#pragma once
#include "steelseries_apex_protocol.h"
namespace halljoy::apex::gen3 {
constexpr unsigned kPid=0x1642, kDepthCount=20, kRangeCount=11, kMappingCount=10;
inline unsigned FactoryHid(unsigned index){return kMap[index]==240?0x409:kMap[index];}
inline bool PhysicalKey(unsigned layout,unsigned hid){
 if(layout>2)return false;
 if(layout==0)return hid!=50 && hid!=100 && !(hid>=135 && hid<=139);
 if(layout==1)return hid!=49 && !(hid>=135 && hid<=139);
 return hid!=49 && hid!=100 && hid!=231;
}
inline void PhysicalRanges(unsigned layout,Ranges& ranges){
 for(unsigned i=0;i<68;++i)if(!PhysicalKey(layout,kMap[i]))ranges[i]={};
}
inline bool Identity(unsigned vid,unsigned pid){return vid==apex::kVid && pid==kPid;}
inline bool Collection(unsigned page,unsigned usage,unsigned in,unsigned out,unsigned feature){
 return page==0xffc0 && usage==1 && in==65 && out==65 && feature==645;
}
// Exact sensor context0801C4A8 +30/+32: operational ADC window, not a deadzone.
inline unsigned SensorFaults(unsigned layout,const Matrix& values,const Ranges& ranges){
 unsigned faults=0;
 for(unsigned i=0;i<68;++i)if(PhysicalKey(layout,kMap[i]) && ranges[i].high>ranges[i].low &&
  (values[i]<124 || values[i]>3970))++faults;
 return faults;
}
inline Report LayoutRequest(){Report p{};p[1]=0xf2;return p;}
inline Report RegionRequest(){Report p{};p[1]=0xf5;return p;}
inline bool Header(const Report& p,unsigned cmd){return p[0]==0 && p[1]==cmd && p[2]==0;}
inline bool ZeroTail(const Report& p,unsigned from){for(;from<p.size();++from)if(p[from])return false;return true;}
inline bool Version(const Report& p){return Header(p,0x90) && std::memcmp(p.data()+3,"1.19.7",7)==0 && ZeroTail(p,10);}
inline bool Layout(const Report& p,unsigned& layout){if(!Header(p,0xf2) || p[3]>2 || !ZeroTail(p,4))return false;layout=p[3];return true;}
inline bool Region(const Report& p,unsigned& region){if(!Header(p,0xf5) || !ZeroTail(p,4))return false;region=p[3];return true;}
inline Report DepthRequest(unsigned start,unsigned count){
 Report p{};if(!count || count>kDepthCount || start>=68 || count>68-start)return p;
 p[1]=0xf3;p[2]=0x50;p[3]=static_cast<unsigned char>(count);
 for(unsigned i=0;i<count;++i)p[4+i*3]=static_cast<unsigned char>(kMap[start+i]);return p;
}
inline Report RangeRequest(unsigned start,unsigned count){
 Report p{};if(!count || count>kRangeCount || start>=68 || count>68-start)return p;
 p[1]=0xf3;p[2]=0x51;p[3]=0;p[4]=static_cast<unsigned char>(count);
 for(unsigned i=0;i<count;++i)p[5+i*5]=static_cast<unsigned char>(kMap[start+i]);return p;
}
inline Report MappingRequest(unsigned start,unsigned count){
 Report p{};if(!count || count>kMappingCount || start>=68 || count>68-start)return p;
 p[1]=0xb6;p[2]=0;p[3]=static_cast<unsigned char>(count);
 for(unsigned i=0;i<count;++i)p[4+i*6]=static_cast<unsigned char>(kMap[start+i]);return p;
}
inline bool ReadOnlyRequest(const Report& p){
 if(p==apex::VersionRequest() || p==LayoutRequest() || p==RegionRequest())return true;
 if(!p[0] && p[1]==0xb6)for(unsigned s=0;s<68;++s)if(p[3] && p==MappingRequest(s,p[3]))return true;
 if(p[0] || p[1]!=0xf3)return false;
 if(p[2]==0x50)for(unsigned s=0;s<68;++s)if(p[3] && p==DepthRequest(s,p[3]))return true;
 if(p[2]==0x51)for(unsigned s=0;s<68;++s)if(p[4] && p==RangeRequest(s,p[4]))return true;
 return false;
}
enum class Refusal : unsigned {None,RequestBounds,Header,Namespace,Count,Hid,RawRange,Padding};
inline Refusal ParseDepth(const Report& p,unsigned start,unsigned count,Matrix& values){
 if(!count || count>kDepthCount || start>=68 || count>68-start)return Refusal::RequestBounds;
 if(!Header(p,0xf3))return Refusal::Header;if(p[3]!=0x50)return Refusal::Namespace;
 if(p[4]!=count)return Refusal::Count;
 auto next=values;
 for(unsigned i=0;i<count;++i){const auto* q=p.data()+5+i*3;
  if(q[0]!=kMap[start+i])return Refusal::Hid;
  auto raw=Le16(q+1);if(raw>4095)return Refusal::RawRange;next[start+i]=static_cast<std::uint16_t>(raw);
 }
 if(!ZeroTail(p,5+count*3))return Refusal::Padding;values=next;return Refusal::None;
}
inline Refusal ParseRanges(const Report& p,unsigned start,unsigned count,Ranges& ranges){
 if(!count || count>kRangeCount || start>=68 || count>68-start)return Refusal::RequestBounds;
 if(!Header(p,0xf3))return Refusal::Header;if(p[3]!=0x51 || p[4]!=0)return Refusal::Namespace;
 if(p[5]!=count)return Refusal::Count;
 auto next=ranges;
 for(unsigned i=0;i<count;++i){const auto* q=p.data()+6+i*5;
  if(q[0]!=kMap[start+i])return Refusal::Hid;
  const auto hi=Le16(q+1),lo=Le16(q+3);if(hi>4095 || lo>4095)return Refusal::RawRange;
  // Regional positions may be unpopulated, retaining reversed factory endpoints.
  next[start+i]=hi>lo+16?Range{static_cast<std::uint16_t>(lo),static_cast<std::uint16_t>(hi)}:Range{};
 }
 if(!ZeroTail(p,6+count*5))return Refusal::Padding;ranges=next;return Refusal::None;
}
enum class MappingKind : unsigned char {Keyboard,Unbound,Mouse,Consumer,Fn,Macro,Other,Chord};
struct Mapping {
 std::uint16_t assigned=0;MappingKind kind=MappingKind::Other;
 bool operator==(const Mapping& r)const{return assigned==r.assigned && kind==r.kind;}
};
using Mappings=std::array<Mapping,70>;
inline Refusal ParseMappings(const Report& p,unsigned start,unsigned count,Mappings& maps){
 if(!count || count>kMappingCount || start>=68 || count>68-start)return Refusal::RequestBounds;
 if(!Header(p,0xb6))return Refusal::Header;if(p[3]!=0)return Refusal::Namespace;
 if(p[4]!=count)return Refusal::Count;auto next=maps;
 for(unsigned i=0;i<count;++i){const auto* q=p.data()+5+i*6;
  if(q[0]!=kMap[start+i])return Refusal::Hid;
  Mapping m{};m.assigned=static_cast<std::uint16_t>(FactoryHid(start+i));
  switch(q[1]){
   case 0:m.kind=MappingKind::Unbound;break;
   case 0x51:
    if(!q[2] && !q[3] && !q[4] && !q[5])m.kind=MappingKind::Unbound;
    else if(q[2] && !q[3] && !q[4] && !q[5]){
     m.assigned=q[2]==240?0x409:q[2];m.kind=MappingKind::Keyboard;
    }else m.kind=MappingKind::Chord;break;
   case 0x61:m.kind=MappingKind::Consumer;break;
   case 0x62:m.kind=MappingKind::Fn;break;
   case 0x71:m.kind=MappingKind::Macro;break;
   default:m.kind=(q[1]<=8 || (q[1]>=0x31 && q[1]<=0x34))?MappingKind::Mouse:MappingKind::Other;break;
  }
  next[start+i]=m;
 }
 if(!ZeroTail(p,5+count*6))return Refusal::Padding;maps=next;return Refusal::None;
}
inline const wchar_t* MappingLabel(MappingKind kind){
 switch(kind){case MappingKind::Keyboard:return L"";case MappingKind::Unbound:return L"Unbound";
 case MappingKind::Mouse:return L"Mouse";case MappingKind::Consumer:return L"Media";case MappingKind::Fn:return L"Fn";
 case MappingKind::Macro:return L"Macro";case MappingKind::Chord:return L"Chord";default:return L"Assigned";}
}
inline const char* RefusalName(Refusal why){
 switch(why){case Refusal::None:return "none";case Refusal::RequestBounds:return "request_bounds";
 case Refusal::Header:return "header_or_status";case Refusal::Namespace:return "namespace_or_layer";
 case Refusal::Count:return "count";case Refusal::Hid:return "hid_echo";case Refusal::RawRange:return "sensor_range";
 case Refusal::Padding:return "padding";}return "unknown";
}
inline const char* LayoutProduct(unsigned layout){return layout==0?"APEXPROTKLGEN3-1038-1642-ANSI":
 layout==1?"APEXPROTKLGEN3-1038-1642-ISO":layout==2?"APEXPROTKLGEN3-1038-1642-JIS":"";}
}
