#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "mchose_mix87_protocol.h"

// MCHOSE Jet 75 II (41E4:211A), reviewed on stock firmware 1.16, and every
// other M HUB WCH RISC-V board in Models[] (2026-10-02 review of each official
// image: identical 03/04/05 handlers, 06 writer reloading the active profile,
// A0 report layout and byte-7 bit-3 service gate; differences are the key set
// and the writer's reboot rule). Same M HUB framing,
// A0 report and debug flag (profile byte 7 bit 3) as Mix87 III, but this
// firmware has no memory-read command: settings are read with opcode 05
// (flash 0x20100 + offset) and the profile base with opcode 04 (0x20200).
// Admission is NOT tied to the firmware version: every step is guarded by the
// data itself (settings layout before the one-bit write, full readback after,
// strict A0 validation while streaming), so a firmware update that keeps the
// protocol keeps working and one that changes it fails closed.
namespace halljoy::jet75 {
using mix87::Report;
using mix87::Checksum;
using mix87::Reply;
using mix87::Hid;
using Settings = std::array<std::uint8_t,256>;
using Base = std::array<std::uint8_t,6>;
constexpr std::uint16_t Vid=0x41e4, Pid=0x211a;
constexpr std::uint8_t ReadBaseOp=4, ReadSettingsOp=5, WriteSettingsOp=6, InfoOp=3;
// Opcode 03 reply: little-endian version (0x0116 reviewed), then a build stamp.
constexpr std::uint16_t ReviewedVersion=0x0116;
constexpr std::uint8_t InfoLength=22;
inline Report Read(std::uint8_t op,std::uint8_t offset,std::uint8_t size) noexcept {
    Report p{};
    if((op!=ReadBaseOp && op!=ReadSettingsOp) || !size || size>56 || offset+size>256)return p;
    p[0]=0x55;p[1]=op;p[4]=size;p[5]=offset;p[3]=Checksum(p);return p;
}
inline Report Info() noexcept {
    Report p{};p[0]=0x55;p[1]=InfoOp;p[4]=InfoLength;p[3]=Checksum(p);return p;
}
inline std::uint16_t Version(const Report& reply) noexcept {
    return static_cast<std::uint16_t>(reply[8]|(reply[9]<<8));
}
// How each firmware picks the active profile from the base {current, count,
// table[4]} at boot (2026-10-24 review of every image's boot code). The flag
// must go to that profile, so HallJoy resolves the base exactly the same way
// instead of rejecting bases the keyboard itself accepts:
// - Counted (Jet 75 II, Zero75X; 6-byte base): current<count ? current : 0;
//   table entry >3 -> profile 0. A current >=4 below count indexes RAM past
//   the base, which HallJoy cannot know: rejected.
// - Bounded (Ace 60 Pro, Ace 60 Pro Nordic, Ace 60X II, Ace 68 Air II; 8-byte
//   base): count is ignored at boot; current >3 -> 0; table entry >3 -> 0.
// - BoundedUnchecked (Ace68-I/II, Jet 75 I, Ace 60X I, Mix 87 I): current >3
//   -> 0; the table entry is used unchecked, so an entry >3 (a profile outside
//   the settings area) is rejected.
enum class BaseRule : std::uint8_t { Counted, Bounded, BoundedUnchecked };
inline bool Profile(const Base& b,unsigned& profile,BaseRule rule=BaseRule::Counted) noexcept {
    unsigned current=0;
    if(rule==BaseRule::Counted){current=b[0]<b[1]?b[0]:0;if(current>3)return false;}
    else current=b[0]>3?0:b[0];
    const unsigned entry=b[2+current];
    if(entry>3){if(rule==BaseRule::BoundedUnchecked)return false;profile=0;return true;}
    profile=entry;return true;
}
// Six base bytes, little-endian, for the support log.
inline std::uint64_t Pack(const Base& b) noexcept {
    std::uint64_t v=0;for(std::size_t i=b.size();i-->0;)v=(v<<8)|b[i];return v;
}
// Valid stored profile: firmware magic AA BB at bytes 2..3.
inline bool ProfileValid(const Settings& s,unsigned profile) noexcept {
    return profile<4 && s[profile*64+2]==0xaa && s[profile*64+3]==0xbb;
}
// The Jet 75 II / Zero75X / Ace68-II writer reboots the keyboard after any
// settings write while byte 0 bit 0 of the area is set (left by the first boot
// after flashing or by the factory-reset key). The write still lands; no reply
// is sent. The other writers reboot only when profile byte 4's low nibble
// changes, which the one-bit flag write never does.
inline bool WriteReboots(const Settings& s) noexcept { return (s[0]&1)!=0; }
inline Report FlagWrite(unsigned profile,std::uint8_t previous,bool enabled) noexcept {
    Report p{};if(profile>=4)return p;
    p[0]=0x55;p[1]=WriteSettingsOp;p[4]=1;p[5]=static_cast<std::uint8_t>(profile*64+7);
    p[8]=enabled?static_cast<std::uint8_t>(previous|8):static_cast<std::uint8_t>(previous&~8u);
    p[3]=Checksum(p);return p;
}
// Factory descriptors of the 79 HID keys (slots 0..79 minus Fn) in stock 1.16.
inline constexpr std::uint8_t Keys[]={
    4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
    40,41,42,43,44,45,46,47,48,49,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,74,75,76,78,79,
    80,81,82,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5};
// Ace68-II stock 1.21 descriptor table (0x12D04): 68 active slots in 0..79,
// 67 HID keys + Fn, no duplicates; equal to the official M HUB Ace68 layout.
// Same set in Ace68-I 1.09 and Ace 68 Air-II 1.17.
inline constexpr std::uint8_t Ace68Keys[]={
    4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
    40,41,42,43,44,45,46,47,48,49,51,52,54,55,56,57,73,75,76,78,79,80,81,82,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6};
// Ace 60 Pro 1.18, Ace60X-I 1.05, Ace60X-II 1.04: 60 HID keys + Fn.
inline constexpr std::uint8_t Ace60Keys[]={
    4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
    40,41,42,43,44,45,46,47,48,49,51,52,54,55,56,57,101,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6};
// Ace60 Pro Nordic 1.07 (ISO): Ace 60 plus the ISO key 0x64.
inline constexpr std::uint8_t Ace60NordicKeys[]={
    4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
    40,41,42,43,44,45,46,47,48,49,51,52,54,55,56,57,100,101,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6};
// Mix 87-I 1.12: 86 HID keys + Fn (G1/RT/Light buttons have no A0 slot).
inline constexpr std::uint8_t Mix87Keys[]={
    4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,
    40,41,42,43,44,45,46,47,48,49,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,
    75,76,77,78,79,80,81,82,101,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6};
struct Model {
    std::uint16_t vid,pid,reviewed;const wchar_t* name;const wchar_t* shortName;const char* layoutProtocol;const char* layoutProduct;
    const std::uint8_t* keys;std::size_t keyCount;
    bool freshFlashReboot; // writer resets the MCU while settings byte 0 bit 0 is set
    BaseRule baseRule;     // boot-time active profile selection (see Profile)
};
// USB product strings in quotes where the M HUB catalog name differs.
inline constexpr Model Models[]={
    {Vid,Pid,ReviewedVersion,L"MCHOSE Jet 75 II",L"Jet 75 II","mchose-jet75","JET75II-41E4-211A",Keys,std::size(Keys),true,BaseRule::Counted},
    {0x41e4,0x2116,0x0121,L"MCHOSE Ace 68 (Ace68-II)",L"Ace 68","mchose-ace68","ACE68II-41E4-2116",Ace68Keys,std::size(Ace68Keys),true,BaseRule::BoundedUnchecked},
    {0x41e4,0x211c,0x0114,L"MCHOSE Zero75X",L"Zero75X","mchose-jet75","ZERO75X-41E4-211C",Keys,std::size(Keys),true,BaseRule::Counted},
    {0x41e4,0x2118,0x0109,L"MCHOSE Jet 75 I",L"Jet 75 I","mchose-jet75","JET75I-41E4-2118",Keys,std::size(Keys),false,BaseRule::BoundedUnchecked},
    {0x41e4,0x2114,0x0109,L"MCHOSE Ace 68 (Ace68-I)",L"Ace 68","mchose-ace68","ACE68I-41E4-2114",Ace68Keys,std::size(Ace68Keys),false,BaseRule::BoundedUnchecked},
    {0x41e4,0x2120,0x0117,L"MCHOSE Ace 68 Air II",L"Ace 68 Air II","mchose-ace68","ACE68AIRII-41E4-2120",Ace68Keys,std::size(Ace68Keys),false,BaseRule::Bounded},
    {0x41e4,0x2103,0x0118,L"MCHOSE Ace 60 Pro",L"Ace 60 Pro","mchose-ace60","ACE60PRO-41E4-2103",Ace60Keys,std::size(Ace60Keys),false,BaseRule::Bounded},
    {0x41e4,0x2126,0x0105,L"MCHOSE Ace 60X I",L"Ace 60X I","mchose-ace60","ACE60XI-41E4-2126",Ace60Keys,std::size(Ace60Keys),false,BaseRule::BoundedUnchecked},
    {0x41e4,0x2112,0x0104,L"MCHOSE Ace 60X II",L"Ace 60X II","mchose-ace60","ACE60XII-41E4-2112",Ace60Keys,std::size(Ace60Keys),false,BaseRule::Bounded},
    {0x3837,0x3002,0x0107,L"MCHOSE Ace 60 Pro Nordic",L"Ace 60 Pro Nordic","mchose-ace60","ACE60PRONORDIC-3837-3002",Ace60NordicKeys,std::size(Ace60NordicKeys),false,BaseRule::Bounded},
    {0x41e4,0x2122,0x0112,L"MCHOSE Mix 87 I",L"Mix 87 I","mchose-mix87","MIX87I-41E4-2122",Mix87Keys,std::size(Mix87Keys),false,BaseRule::BoundedUnchecked},
};
inline const Model* FindModel(std::uint16_t vid,std::uint16_t pid) noexcept {
    for(const auto& m:Models)if(m.vid==vid && m.pid==pid)return &m;
    return nullptr;
}
inline std::array<bool,256> Allowed(const Model& model) noexcept {
    std::array<bool,256> map{};for(std::size_t i=0;i<model.keyCount;++i)map[model.keys[i]]=true;return map;
}
inline std::array<bool,256> Allowed() noexcept { return Allowed(Models[0]); }
// Stock switch tables 311..351 (0.01 mm) across the reviewed images; Nordic 1.07
// keeps them in RAM, so this check is its only guard. A range, so a switch type
// added by a later firmware is not rejected; zero or absurd values still are.
inline bool KnownMaximum(unsigned maximum) noexcept { return maximum>=200 && maximum<=600; }
struct Sample {unsigned hid=0;std::uint16_t milli=0;};
inline bool Decode(const Report& p,const std::array<bool,256>& allowed,Sample& s) noexcept {
    if(p[0]!=0xa0)return false;
    const auto hid=Hid(p[1],p[2],p[3]);
    const unsigned depth=(p[6]<<8)|p[7], maximum=(p[14]<<8)|p[15];
    if(!hid || !allowed[hid] || !KnownMaximum(maximum) || depth>maximum)return false;
    s={hid,static_cast<std::uint16_t>((depth*1000u+maximum/2)/maximum)};return true;
}
enum class ChangeResult { Verified, Unchanged, StaleProfile, ReadFailed, ReservedData, Uncertain, Rebooting };
// One guarded flag transaction; lifecycle policy is owned by the caller.
// readBase(Base&) / readSettings(Settings&) / exchange(request,reply) must use
// the SAME open handle. No retry or calibration inside this transaction.
template<class ReadBase,class ReadSettings,class Exchange>
ChangeResult ChangeFlag(const Base& consentBase,bool enabled,ReadBase readBase,ReadSettings readSettings,Exchange exchange,
                        bool freshFlashReboot=true,BaseRule rule=BaseRule::Counted) {
    Base base{},check{};Settings before{},after{};unsigned profile=0;
    if(!readBase(base) || !readSettings(before))return ChangeResult::ReadFailed;
    if(base!=consentBase || !Profile(base,profile,rule))return ChangeResult::StaleProfile;
    if(!ProfileValid(before,profile))return ChangeResult::ReservedData;
    const auto offset=profile*64+7;
    if(bool(before[offset]&8)==enabled)return ChangeResult::Unchanged;
    if(!readBase(check))return ChangeResult::ReadFailed;
    if(check!=base)return ChangeResult::StaleProfile;
    const bool reboots=freshFlashReboot && WriteReboots(before);
    const auto request=FlagWrite(profile,before[offset],enabled);Report ack{};
    const bool accepted=exchange(request,ack); // exactly ONE write attempt
    if(reboots)return ChangeResult::Rebooting;  // the device resets; verify on reconnect
    if(!readSettings(after) || !readBase(check))return ChangeResult::Uncertain;
    before[offset]=request[8];
    return accepted && before==after && check==base?ChangeResult::Verified:ChangeResult::Uncertain;
}
}
