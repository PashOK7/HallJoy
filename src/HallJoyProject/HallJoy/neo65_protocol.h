#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
namespace halljoy::neo65 {
constexpr std::uint16_t kVid=0xe560, kAnsi=0xee65, kIso=0xef65;
using Matrix=std::array<std::uint16_t,80>;
using Report=std::array<unsigned char,33>;
inline constexpr Matrix kAnsiMap={41,30,31,32,33,34,35,36,37,38,39,45,46,42,76,0,43,20,26,8,21,23,28,24,12,18,19,47,48,49,75,0,57,4,22,7,9,10,11,13,14,15,51,52,0,40,78,0,225,0,29,27,6,25,5,17,16,54,55,56,80,82,77,0,224,227,226,0,0,0,0,44,0,0,230,1033,229,81,79,0};
inline constexpr Matrix kIsoMap={41,30,31,32,33,34,35,36,37,38,39,45,46,42,76,0,43,20,26,8,21,23,28,24,12,18,19,47,48,40,75,0,57,4,22,7,9,10,11,13,14,15,51,52,0,50,78,0,225,100,29,27,6,25,5,17,16,54,55,56,80,82,77,0,224,227,226,0,0,0,0,44,0,0,230,1033,229,81,79,0};
inline const Matrix& Factory(std::uint16_t pid) {return pid==kIso?kIsoMap:kAnsiMap;}
inline unsigned Be16(const unsigned char* p) {return (unsigned(p[0])<<8)|p[1];}
inline bool Echo(const Report& p,unsigned opcode,unsigned start,unsigned count) {
 return p[0]==0 && p[1]==0xd0 && p[2]==opcode && p[3]==start && p[4]==count;
}
inline Report Request(unsigned opcode,unsigned start=0,unsigned count=0) {
 Report p{};p[1]=0xd0;p[2]=static_cast<unsigned char>(opcode);
 p[3]=static_cast<unsigned char>(start);p[4]=static_cast<unsigned char>(count);return p;
}
inline bool ParseDepth(const Report& p,unsigned start,unsigned count,Matrix& values) {
 if(!count || count>14 || start>=80 || count>80-start || !Echo(p,0xa6,start,count))return false;
 for(unsigned i=0;i<count;++i)if(Be16(&p[5+i*2])>40000)return false;
 for(unsigned i=0;i<count;++i)values[start+i]=static_cast<std::uint16_t>(Be16(&p[5+i*2]));
 return true;
}
struct Range {std::uint16_t low=0,high=0;};
using Ranges=std::array<Range,80>;
inline bool ParseRanges(const Report& p,unsigned start,unsigned count,unsigned stride,Ranges& ranges) {
 if(stride<9 || stride>28 || !count || count>28/stride || start>=80 || count>80-start || !Echo(p,0xaa,start,count))return false;
 auto next=ranges;
 for(unsigned i=0;i<count;++i) {
  const auto* q=&p[5+i*stride];const unsigned axis=(Be16(q+1)/100)*100;
  const unsigned top=q[0]?Be16(q+5):0,bottom=q[0]?Be16(q+7):0;
  if(!axis) {next[start+i]={};continue;} // unused slots; populated slots checked by caller
  if(axis>40000 || axis<1000 || top+bottom>=axis)return false;
  next[start+i]={static_cast<std::uint16_t>(top),static_cast<std::uint16_t>(axis-bottom)};
 }
 ranges=next;return true;
}
inline std::uint16_t Normalize(unsigned raw,const Range& r) {
 if(r.high<=r.low || raw<=r.low)return 0;
 if(raw>=r.high)return 1000;
 return static_cast<std::uint16_t>(((raw-r.low)*1000+(r.high-r.low)/2)/(r.high-r.low));
}
}
