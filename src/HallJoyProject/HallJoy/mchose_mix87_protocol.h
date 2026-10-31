#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace halljoy::mix87 {
using Report = std::array<std::uint8_t,64>;
using Settings = std::array<std::uint8_t,256>;
using SettingsPage = std::array<std::uint8_t,8192>;
using Base = std::array<std::uint8_t,8>;
constexpr std::uint16_t Vid=0x3837, Pid=0x300d;
constexpr std::uint32_t BaseAddress=0x2a000, SettingsAddress=0x2c000;
inline std::uint8_t Checksum(const Report& p) noexcept {
    unsigned sum=p[4];
    if(p[4]>56)return 0;
    for(unsigned i=5;i<8u+p[4];++i)sum+=p[i];
    return static_cast<std::uint8_t>(sum);
}
inline Report Read(std::uint32_t address,std::uint8_t size) noexcept {
    Report p{};
    if(size>56 || address>=0x80000 || address+size>0x80000)return p;
    p[0]=0x55;p[1]=0xe0;p[4]=size;
    p[5]=static_cast<std::uint8_t>(address);p[6]=static_cast<std::uint8_t>(address>>8);p[7]=static_cast<std::uint8_t>(address>>16);
    p[3]=Checksum(p);return p;
}
inline bool Reply(const Report& request,const Report& reply) noexcept {
    return reply[0]==0xaa && reply[1]==request[1] && reply[2]==0 &&
        reply[4]<=56 && reply[4]==request[4] &&
        std::equal(reply.begin()+5,reply.begin()+8,request.begin()+5) && reply[3]==Checksum(reply);
}
inline bool Profile(const Base& b,unsigned& profile) noexcept {
    if(!b[1] || b[1]>4 || b[0]>=b[1] || b[2+b[0]]>=4)return false;
    profile=b[2+b[0]];return true;
}
inline Report FlagWrite(unsigned profile,std::uint8_t previous,bool enabled) noexcept {
    Report p{};if(profile>=4)return p;
    p[0]=0x55;p[1]=6;p[4]=1;p[5]=static_cast<std::uint8_t>(profile*64+7);
    p[8]=enabled?static_cast<std::uint8_t>(previous|8):static_cast<std::uint8_t>(previous&~8u);
    p[3]=Checksum(p);return p;
}
// Immutable factory descriptor, NOT the user's remapped key assignment.
inline unsigned Hid(std::uint8_t type,std::uint8_t modifier,std::uint8_t code) noexcept {
    if(type!=0x10)return 0;
    if(!modifier)return code>=4 && code<=0x73?code:0;
    if(code || (modifier&(modifier-1)))return 0;
    unsigned bit=0;while((modifier>>bit)!=1)++bit;
    return 0xe0+bit;
}
struct Sample {unsigned hid=0;std::uint16_t milli=0;};
// Mix87 III 1.22 (fingerprinted) has only 341; other Mix87 III versions and
// the other ARM boards are not version-bound and some keep the switch table in
// RAM, so they accept the plausible range used for the RISC-V family.
inline bool Decode(const Report& p,const std::array<bool,256>& allowed,Sample& s,bool exact341=true) noexcept {
    if(p[0]!=0xa0)return false;
    const auto hid=Hid(p[1],p[2],p[3]);
    const unsigned depth=(p[6]<<8)|p[7], maximum=(p[14]<<8)|p[15];
    if(!hid || !allowed[hid] || (exact341?maximum!=341:(maximum<200 || maximum>600)) || depth>maximum)return false;
    s={hid,static_cast<std::uint16_t>((depth*1000u+maximum/2)/maximum)};return true;
}
// Other M HUB ARM boards on the Mix87 III design (2026-10-02 review of each
// official image: same 55/AA dispatcher, 03 (31-byte reply), E0 flash read,
// 06 writer staging 256 bytes of 0x2C000 and reloading the active profile,
// A0 report bytes 0..15 and the byte-7 bit-3 service gate; replayed in
// Unicorn). Factory A0 descriptor keys per model (+ Fn, not reported).
inline constexpr std::uint8_t Ace68Keys[]={
    4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
    40,41,42,43,44,45,46,47,48,49,51,52,54,55,56,57,73,75,76,78,79,80,81,82,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6};
inline constexpr std::uint8_t Ace75Keys[]={
    4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
    40,41,42,43,44,45,46,47,48,49,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,74,75,76,77,78,
    79,80,81,82,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5};
// Mix87 III factory A0 descriptor keys (1.22 table at 0x15da6, + Fn, not reported).
// Used for every other Mix87 III firmware version, which is not fingerprinted.
inline constexpr std::uint8_t Mix87IIIKeys[]={
    4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
    40,41,42,43,44,45,46,47,48,49,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,
    75,76,77,78,79,80,81,82,101,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6};
struct Model {
    std::uint16_t vid,pid;const wchar_t* name;const wchar_t* shortName;const char* layoutProtocol;const char* layoutProduct;
    const std::uint8_t* keys;std::size_t keyCount; // firmware 1.22 reads them from the fingerprinted table
    bool fingerprinted; // firmware 1.22 takes the exact Mix87 III fingerprint path
};
inline constexpr Model Models[]={
    {Vid,Pid,L"MCHOSE Mix87 III",L"Mix87 III","mchose-mix87","MIX87III-3837-300D",Mix87IIIKeys,std::size(Mix87IIIKeys),true},
    {0x3837,0x3003,L"MCHOSE Ace 68 III",L"Ace 68 III","mchose-ace68","ACE68III-3837-3003",Ace68Keys,std::size(Ace68Keys),false},
    {0x41e4,0x2132,L"MCHOSE Ace 68 Air III",L"Ace 68 Air III","mchose-ace68","ACE68AIRIII-41E4-2132",Ace68Keys,std::size(Ace68Keys),false},
    {0x3837,0x300a,L"MCHOSE Ace 68 Air 2",L"Ace 68 Air 2","mchose-ace68","ACE68AIR2-3837-300A",Ace68Keys,std::size(Ace68Keys),false},
    {0x3837,0x3024,L"MCHOSE Ace 68 V2 III",L"Ace 68 V2 III","mchose-ace68","ACE68V2III-3837-3024",Ace68Keys,std::size(Ace68Keys),false},
    {0x3837,0x3028,L"MCHOSE Ace 68 Turbo 8K",L"Ace 68 Turbo 8K","mchose-ace68","ACE68TURBO8K-3837-3028",Ace68Keys,std::size(Ace68Keys),false},
    {0x3837,0x303c,L"MCHOSE Ace 75 8K",L"Ace 75 8K","mchose-ace75","ACE758K-3837-303C",Ace75Keys,std::size(Ace75Keys),false},
};
inline const Model* FindModel(std::uint16_t vid,std::uint16_t pid) noexcept {
    for(const auto& m:Models)if(m.vid==vid && m.pid==pid)return &m;
    return nullptr;
}
enum class ChangeResult { Verified, Unchanged, StaleProfile, ReadFailed, ReservedData, Uncertain };
// One guarded flag transaction; lifecycle policy is owned by the caller.
// No retry or calibration inside this transaction.
// read(address,dst,len) and exchange(request,reply) must use the SAME open handle.
template<class ReadBytes,class Exchange>
ChangeResult ChangeFlag(const Base& consentBase,bool enabled,ReadBytes read,Exchange exchange) {
    Base base{};SettingsPage before{},after{};unsigned profile=0;
    if(!read(BaseAddress,base.data(),base.size()) || !read(SettingsAddress,before.data(),before.size()))return ChangeResult::ReadFailed;
    if(base!=consentBase || !Profile(base,profile))return ChangeResult::StaleProfile;
    // Firmware aligns the erase address to 8 KiB and rewrites 256 settings bytes.
    // Reserved tail is zero padding in the pinned factory image, or erased FF
    // after a normal settings save. Reject any other contents, not these two
    // known states. Erasing factory padding is normal firmware behavior.
    const bool erased=std::all_of(before.begin()+256,before.end(),[](auto v){return v==0xff;});
    const bool factoryPadding=std::all_of(before.begin()+256,before.end(),[](auto v){return v==0;});
    if(!erased && !factoryPadding)return ChangeResult::ReservedData;
    const auto offset=profile*64+7;
    if(bool(before[offset]&8)==enabled)return ChangeResult::Unchanged;
    Base check{};
    if(!read(BaseAddress,check.data(),check.size()))return ChangeResult::ReadFailed;
    if(check!=base)return ChangeResult::StaleProfile;
    const auto request=FlagWrite(profile,before[offset],enabled);Report ack{};
    const bool accepted=exchange(request,ack); // exactly ONE write attempt
    if(!read(SettingsAddress,after.data(),after.size()) || !read(BaseAddress,check.data(),check.size()))return ChangeResult::Uncertain;
    before[offset]=request[8];
    const bool settingsMatch=std::equal(before.begin(),before.begin()+256,after.begin());
    bool paddingSafe=true;
    for(std::size_t i=256;i<after.size();++i)
        if(after[i]!=before[i] && after[i]!=0xff)paddingSafe=false;
    return accepted && settingsMatch && paddingSafe && check==base?ChangeResult::Verified:ChangeResult::Uncertain;
}
}
