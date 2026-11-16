#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "aula_f75_factory.h"
#include "boom68_factory.h"
#include "generated/layout_pipeline/identities.h"
namespace halljoy::f75 {
// Owner ended stock F75 trials on 2026-11-16; preserve research and BOOM68.
inline constexpr bool kStockF75TrialEnabled=false;
constexpr unsigned kSlots=128, kPhysical=80;
enum class Result { Retry, Foreign, Stopped };
using Report=std::array<std::uint8_t,65>;
using Assignments=std::array<std::uint16_t,kSlots>;
struct Candidate {
    std::wstring path,name;
    unsigned vid=0,pid=0,page=0,usage=0,inputBytes=0,outputBytes=0,version=0;
};
struct Info {unsigned vid=0,pid=0,version=0,manufacturer=0,precision=0,frame=0;};
inline std::wstring Lower(std::wstring x) {
    for(auto& c:x) if(c>=L'A' && c<=L'Z') c+=L'a'-L'A';
    return x;
}
inline bool Boom(const Candidate& c){return c.vid==0x0c45 && c.pid==0x80b5 && Lower(c.name)==L"boom68-usb";}
inline const Assignments& FactoryFor(const Candidate& c){return Boom(c)?halljoy::boom68::Factory:Factory;}
inline bool Radio(const Candidate& c) {return c.vid==0x0c45 && c.pid==0xfefe;}
inline bool Wired(unsigned vid,unsigned pid) {return vid==0x38a6 && (pid==0x2729 || pid==0x2742);}
inline bool IdentityCandidate(const Candidate& c) {
    return Boom(c) || Wired(c.vid,c.pid) || (Radio(c) && Lower(c.name)==L"aula f75 he dongle");
}
inline void PreferWired(std::vector<Candidate>& candidates) {
    std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){
        return Wired(a.vid,a.pid) && !Wired(b.vid,b.pid);
    });
}
inline bool NeedsSwitch(const Candidate& current,const std::vector<Candidate>& candidates) {
    const bool present=std::any_of(candidates.begin(),candidates.end(),[&](const auto& c){return c.path==current.path;});
    const bool wired=std::any_of(candidates.begin(),candidates.end(),[](const auto& c){return Wired(c.vid,c.pid);});
    return !present || (Radio(current) && wired);
}
inline bool Collection(const Candidate& c) {
    return IdentityCandidate(c) && c.usage==0x61 && c.page==(Radio(c)?0xff60u:0xff68u) &&
        (c.inputBytes==33 || c.inputBytes==65) && (c.outputBytes==33 || c.outputBytes==65);
}
inline bool RuntimeCollection(const Candidate& c) {return (kStockF75TrialEnabled || Boom(c)) && Collection(c);}
inline bool CompatibleInfo(const Candidate& c,const Info& i) {
    return Wired(i.vid,i.pid) || (Radio(c) && i.vid==c.vid && i.pid==c.pid);
}
inline const char* Product(const Candidate& c) {
    return Boom(c)?"BOOM68-0C45-80B5-ANSI":Radio(c)?"F75HE-0C45-FEFE-ANSI":c.pid==0x2729?"F75HE-38A6-2729-ANSI":"F75HE-38A6-2742-ANSI";
}
inline std::uint64_t Token(const Candidate& c) {return halljoy::layout_identity::Token(Boom(c)?"langtu-sonix":"aula-f75",Product(c));}
inline unsigned Word(const std::uint8_t* b) {return unsigned(b[0])|(unsigned(b[1])<<8);}
inline Report Request(unsigned cmd,unsigned bytes,unsigned offset=0,bool last=true) {
    Report r{};r[1]=0xaa;r[2]=std::uint8_t(cmd);r[3]=std::uint8_t(bytes);
    r[4]=std::uint8_t(offset&255);r[5]=std::uint8_t(offset>>8);r[7]=last?1:0;return r;
}
inline bool Header(const Report& r,unsigned transferred,unsigned cmd,unsigned bytes,unsigned offset) {
    return transferred>=9+bytes && !r[0] && r[1]==0x55 && r[2]==cmd && r[3]==bytes && Word(r.data()+4)==offset;
}
inline Info DecodeInfo(const std::array<std::uint8_t,56>& b) {
    return {Word(b.data()+4),Word(b.data()+6),Word(b.data()+8),Word(b.data()+12),b[29],b[30]};
}
inline std::uint16_t Assigned(const std::uint8_t* r,unsigned slot,const Assignments& factory=Factory) {
    if(!r[0])return factory[slot];
    if(r[0]!=2 || r[3])return 0;
    if(r[1]) {
        if(r[2] || (r[1]&(r[1]-1)))return 0;
        for(unsigned i=0;i<8;++i)if(r[1]==(1u<<i))return std::uint16_t(224+i);
    }
    if(r[2]==0xaf)return 0x409;
    return (r[2]>=4 && r[2]<=0xa4)||(r[2]>=0xe0 && r[2]<=0xe7)?r[2]:0;
}
inline unsigned Milli(unsigned raw,unsigned stroke=34) {
    return stroke?std::min(1000u,(raw*1000u)/(stroke*10u)):0;
}
inline std::vector<unsigned> Positions() {
    std::vector<unsigned> keys;
    for(unsigned i=1;i<kSlots;++i)if(Factory[i])keys.push_back(i);
    if(Factory[0])keys.push_back(0);
    return keys;
}
} // namespace halljoy::f75
