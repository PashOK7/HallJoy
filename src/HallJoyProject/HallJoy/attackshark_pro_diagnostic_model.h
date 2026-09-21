#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include "attackshark_pro_native_model.h"
namespace halljoy::sharkdiag {
using Report=std::array<std::uint8_t,65>;
inline bool Known(unsigned id,unsigned pid){
 const auto* p=sharkplay::Find(id);return p && p->pid==pid;
}
inline bool CandidatePid(unsigned pid){
 for(const auto& p:sharkplay::Profiles)if(p.pid==pid)return true;
 return false;
}
inline Report Request(unsigned command,unsigned page=0){
 Report r{};
 if(command!=0x8f && command!=0x80 && command!=0xe5)return r;
 if(command==0xe5 && page>=4)return r;
 r[1]=static_cast<std::uint8_t>(command);
 if(command==0xe5){r[2]=0xfe;r[3]=1;r[4]=static_cast<std::uint8_t>(page);}
 unsigned sum=0;for(unsigned i=1;i<8;++i)sum+=r[i];r[8]=static_cast<std::uint8_t>(255-sum);return r;
}
inline unsigned Identity(const Report& r){
 if(r[0]!=0 || r[1]!=0x8f)return 0;
 return unsigned(r[2])|(unsigned(r[3])<<8)|(unsigned(r[4])<<16)|(unsigned(r[5])<<24);
}
inline bool Decode(const Report& r,std::array<unsigned,32>& out){
 if(r[0]!=0)return false;
 // A pending/echoed command is not a raw sample page.
 if(r[1]==0xe5 && r[2]==0xfe && r[3]==1)return false;
 for(unsigned i=0;i<32;++i){out[i]=unsigned(r[1+i*2])|(unsigned(r[2+i*2])<<8);if(out[i]>4096)return false;}
 return true;
}
struct Key {
 std::uint64_t count=0,positive=0,zero=0,changed=0,released=0,increase=0,decrease=0;
 unsigned low=65535,high=0,last=0;
 void Add(unsigned value){
  if(count){if(value!=last)++changed;if(value>last)++increase;if(value<last)++decrease;if(last && !value)++released;}
  ++count;if(value)++positive;else ++zero;low=(std::min)(low,value);high=(std::max)(high,value);last=value;
 }
};
struct Metrics {
 std::array<Key,128> keys{};std::array<std::uint64_t,4> pages{};
 std::uint64_t invalid=0,chordFrames=0,independentChanges=0;unsigned peakPage0=0;
 bool Add(unsigned page,const Report& r,unsigned digitalWasd){
  std::array<unsigned,32> values{};if(page>=4 || !Decode(r,values)){++invalid;return false;}
  bool moving=false,steady=false;unsigned active=0;
  for(unsigned i=0;i<32;++i){auto& k=keys[page*32+i];if(values[i])++active;
   if(k.count && values[i] && k.last){moving=moving || values[i]!=k.last;steady=steady || values[i]==k.last;}
   k.Add(values[i]);}
  ++pages[page];if(page==0){peakPage0=(std::max)(peakPage0,active);
   if(active>=2 && (digitalWasd&(digitalWasd-1))){++chordFrames;if(moving && steady)++independentChanges;}}
  return true;
 }
 unsigned Varying()const{unsigned n=0;for(const auto& k:keys)if(k.positive && k.changed)++n;return n;}
 bool Enough(unsigned presses,unsigned releases)const{
  if(presses<4 || releases<4 || !chordFrames || !independentChanges)return false;
  for(auto slot:{14,9,15,21}){const auto& k=keys[slot];if(!k.positive || !k.released || !k.increase || !k.decrease)return false;}
  for(auto n:pages)if(!n)return false;
  return true;
 }
};
}
