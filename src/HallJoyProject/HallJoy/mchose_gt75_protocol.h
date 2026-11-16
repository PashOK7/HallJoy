#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "mchose_gt75_factory.h"
#include "generated/layout_pipeline/identities.h"
namespace halljoy::gt75 {
constexpr unsigned kSlots=128,kPhysical=81;
using Report=std::array<std::uint8_t,65>;
using Base=std::array<std::uint8_t,8>;
using Settings=std::array<std::uint8_t,256>;
using Assignments=std::array<std::uint16_t,kSlots>;
struct Candidate {std::wstring path,name;unsigned vid=0,pid=0,page=0,usage=0,inputBytes=0,outputBytes=0;};
struct Info {unsigned version=0;};
inline std::wstring Lower(std::wstring s){for(auto& c:s)if(c>=L'A' && c<=L'Z')c+=L'a'-L'A';return s;}
inline bool IdentityCandidate(const Candidate& c){return c.vid==0x3837 && c.pid==0x302d;}
inline bool Collection(const Candidate& c){return IdentityCandidate(c) && c.page==1 && c.usage==0 && c.inputBytes==65 && c.outputBytes==65;}
inline void PreferWired(std::vector<Candidate>&){}
inline bool NeedsSwitch(const Candidate& c,const std::vector<Candidate>& all){return std::none_of(all.begin(),all.end(),[&](const auto& x){return x.path==c.path;});}
inline std::uint64_t Token(const Candidate&){return layout_identity::Token("mchose-gt75","GT75-3837-302D-ANSI");}
inline unsigned Offset(const Report& r){return r[6]|(unsigned(r[7])<<8)|(unsigned(r[8])<<16);}
inline std::uint8_t Checksum(const Report& r){unsigned n=r[5];if(n>56)return 0;unsigned sum=n;for(unsigned i=6;i<9+n;++i)sum+=r[i];return std::uint8_t(sum);}
inline Report Request(unsigned op,unsigned n,unsigned offset=0){Report r{};r[1]=0x55;r[2]=std::uint8_t(op);r[5]=std::uint8_t(n);r[6]=std::uint8_t(offset);r[7]=std::uint8_t(offset>>8);r[8]=std::uint8_t(offset>>16);r[4]=Checksum(r);return r;}
inline bool Reply(const Report& q,const Report& r,unsigned bytes,bool info=false){return bytes==65 && !r[0] && r[1]==0xaa && r[2]==q[2] && !r[3] && r[5]<=56 && (info?r[5]>=2:r[5]==q[5]) && Offset(r)==Offset(q) && r[4]==Checksum(r);}
inline unsigned Profile(const Base& b){const auto p=b[2+(b[0]>3?0:b[0])];return p>3?0:p;}
inline bool ValidProfile(const Settings& s,unsigned p){return p<4 && s[p*64+2]==0xaa && s[p*64+3]==0xbb;}
inline void RemoveProjection(Settings& s){for(unsigned p=0;p<4;++p)s[p*64+32]=0;}
inline Report Flag(unsigned p,unsigned prior,bool on){auto r=Request(6,1,p*64+7);r[9]=std::uint8_t(on?prior|8:prior&~8u);r[4]=Checksum(r);return r;}
inline std::vector<unsigned> Positions(){std::vector<unsigned> p;for(unsigned i=0;i<96;++i)if(Factory[i])p.push_back(i);return p;}
struct Sample {unsigned slot=0,raw=0,maximum=0;};
inline unsigned Hid(unsigned type,unsigned modifier,unsigned code){
    if(type==0xf0 && modifier==0xff && code==1)return 0x409;
    if(type!=0x10)return 0;
    if(!modifier)return code>=4 && code<=0xa4?code:0;
    if(code || (modifier&(modifier-1)))return 0;
    for(unsigned i=0;i<8;++i)if(modifier==(1u<<i))return 224+i;
    return 0;
}
inline bool Decode(const Report& r,unsigned bytes,Sample& s){
    if(bytes!=65 || r[0] || r[1]!=0xa0)return false;
    const auto hid=Hid(r[2],r[3],r[4]);if(!hid)return false;
    const unsigned raw=(unsigned(r[7])<<8)|r[8],max=(unsigned(r[15])<<8)|r[16];
    if(!max || raw>max)return false;
    for(unsigned slot=0;slot<96;++slot)if(Factory[slot]==hid){s={slot,raw,max};return true;}
    return false;
}
inline unsigned Milli(unsigned raw,unsigned maximum){return maximum?(raw*1000u+maximum/2)/maximum:0;}
} // namespace halljoy::gt75
