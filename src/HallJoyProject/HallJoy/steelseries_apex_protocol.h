#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace halljoy::apex {
constexpr std::uint16_t kVid=0x1038,kPid=0x1610;
inline bool SupportedIdentity(unsigned vid,unsigned pid){return vid==kVid && (pid==kPid || pid==0x1614 || pid==0x1640 || pid==0x1642);}
inline const wchar_t* ModelName(unsigned pid){
 return pid==0x1642?L"SteelSeries Apex Pro TKL Gen 3":pid==0x1614?L"SteelSeries Apex Pro TKL":pid==0x1640?L"SteelSeries Apex Pro Gen 3":L"SteelSeries Apex Pro";
}
inline bool SupportedCollection(unsigned page,unsigned usage,unsigned in,unsigned out,unsigned feature){
 return page==0xffc0 && usage==1 && in==65 && out==65 && feature==643;
}

using Report=std::array<unsigned char,65>;
using Matrix=std::array<std::uint16_t,70>;
// Official keyboard-app 4.16.8, inverse ROM map at 0x08023dd4.
inline constexpr Matrix kMap={53,30,31,32,33,34,35,36,37,38,39,45,46,137,43,20,26,8,21,23,28,24,12,18,19,47,48,49,57,4,22,7,9,10,11,13,14,15,51,52,50,40,225,100,29,27,6,25,5,17,16,54,55,56,135,229,224,227,226,139,44,138,136,230,231,240,228,42,0,0};
struct Range {std::uint16_t low=0,high=0;};
using Ranges=std::array<Range,70>;
inline unsigned Le16(const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8);}
inline Report VersionRequest(){Report p{};p[1]=0x90;return p;}
inline bool KnownVersion(const Report& p){return p[0]==0 && std::memcmp(p.data()+1,"4.16.8",7)==0;}
inline Report DepthRequest(unsigned bank){Report p{};if(bank>=1 && bank<=5){p[1]=0xd7;p[2]=static_cast<unsigned char>(bank);}return p;}
inline Report RangeRequest(unsigned start,unsigned count){
 Report p{};if(!count || count>12 || start>=68 || count>68-start)return p;
 p[1]=0xda;p[2]=static_cast<unsigned char>(count);
 for(unsigned i=0;i<count;++i)p[3+i*5]=static_cast<unsigned char>(kMap[start+i]);
 return p;
}
inline bool ReadOnlyRequest(const Report& p){
 if(p[0])return false;
 if(p[1]==0x90)return p==VersionRequest();
 if(p[1]==0xd7)return p[2]>=1 && p[2]<=5 && p==DepthRequest(p[2]);
 if(p[1]!=0xda || !p[2] || p[2]>12)return false;
 for(unsigned s=0;s<68;++s)if(p==RangeRequest(s,p[2]))return true;
 return false;
}
inline bool ParseRanges(const Report& p,unsigned start,unsigned count,Ranges& ranges){
 if(p[0] || !count || count>12 || start>=68 || count>68-start)return false;
 auto next=ranges;
 for(unsigned i=0;i<count;++i){const auto* q=p.data()+1+i*5;
  if(q[0]!=kMap[start+i])return false;
  const unsigned hi=Le16(q+1),lo=Le16(q+3);
  if(hi>4095 || lo>4095)return false;
  // Unpopulated regional positions retain reversed factory extrema. Never invent ranges.
  next[start+i]=hi>lo+16 ? Range{static_cast<std::uint16_t>(lo),static_cast<std::uint16_t>(hi)}:Range{};
 }
 for(unsigned i=1+count*5;i<65;++i)if(p[i])return false;
 ranges=next;return true;
}
inline bool ParseDepth(const Report& p,unsigned bank,Matrix& values){
 if(p[0] || bank<1 || bank>5)return false;
 for(unsigned i=57;i<65;++i)if(p[i])return false;
 for(unsigned i=0;i<28;++i)if(Le16(p.data()+1+2*i)>4095)return false;
 for(unsigned i=0;i<14;++i)values[(bank-1)*14+i]=static_cast<std::uint16_t>(Le16(p.data()+29+2*i));
 return true;
}
inline std::uint16_t Normalize(unsigned raw,const Range& r){
 if(r.high<=r.low || raw<=r.low)return 0;
 if(raw>=r.high)return 1000;
 return static_cast<std::uint16_t>(((raw-r.low)*1000+(r.high-r.low)/2)/(r.high-r.low));
}
}
