#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
#include "generated/layout_pipeline/identities.h"
namespace halljoy::mini60 {
inline constexpr std::uint64_t LayoutToken=0x4d363050414e5349ull;
inline constexpr const wchar_t* Preset=L"Aula MINI60 HE Pro ANSI";
inline constexpr std::uint64_t MaxLayoutToken=0x4d36304d414e5349ull;
inline constexpr const wchar_t* MaxPreset=L"Aula MINI60 HE MAX ANSI";
inline constexpr std::uint64_t BaseLayoutToken=0x4d363042414e5349ull;
inline constexpr const wchar_t* BasePreset=L"Aula MINI60 HE ANSI";
// AJAZZ AK820 MAX HE Sonix revision (Driveall "AK820MAX", 0C45:80B1). Same
// AA/55 command set and 55 FB simulation stream as MINI60. The token names the
// AK820 MAX HE ANSI geometry, whose 82 keys equal the Driveall key list.
inline constexpr unsigned Ak820Pid=0x80b1;
inline constexpr std::uint64_t Ak820LayoutToken=halljoy::layout_identity::Token("ajazz-m484","SG8994HERGB");
static_assert(Ak820LayoutToken!=0);
// Owner decision 2026-10-08: 0C45:80B1 is not supported (red). The implementation
// stays in the tree but is not admitted; set to true only with a new decision.
inline constexpr bool Ak820Admitted=false;
inline constexpr bool SupportedProduct(unsigned pid){return pid==0x80a2 || pid==0x80a1 || pid==0x8032 || (Ak820Admitted && pid==Ak820Pid);}
inline constexpr std::uint64_t Token(unsigned pid){return pid==0x80a1?MaxLayoutToken:pid==0x80a2?LayoutToken:pid==0x8032?BaseLayoutToken:pid==Ak820Pid?Ak820LayoutToken:0;}
// Driveall reads 48 device-info bytes; the AULA SDK reads 56.
inline constexpr unsigned InfoLength(unsigned pid){return pid==Ak820Pid?48:56;}
inline constexpr bool Identity(unsigned descriptorPid,unsigned vid,unsigned pid,unsigned manufacturer,unsigned product){
    // Base/MAX device-info product is supplied by the MCU identification register,
    // not a fixed value in the firmware. Keep the established PRO restriction.
    // AK820 manufacturer/product values are unknown: the device-info VID/PID
    // echo is the proof, and the values are logged.
    if(pid==Ak820Pid)return vid==0x0c45 && pid==descriptorPid;
    return vid==0x0c45 && SupportedProduct(pid) && pid==descriptorPid &&
        manufacturer==0x0166 && (pid!=0x80a2 || product==0x110c);
}
inline constexpr unsigned FreshMs=50;
// Physical positions from pinned HFD layout o and master map y (61 keys).
inline constexpr std::array<std::uint16_t,126> Factory={
41,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,31,32,33,34,35,36,37,38,39,45,46,0,0,0,43,20,26,8,21,23,28,24,12,18,19,47,48,0,0,0,57,4,22,7,9,10,11,13,14,15,51,52,49,0,0,0,225,29,27,6,25,5,17,16,54,55,56,229,40,0,0,0,224,227,226,44,230,1033,101,228,0,0,0,0,42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
// Driveall config 3141:32945 "AK820MAX" keyList: value -> keyCode (82 keys, Fn 0xAF -> 0x409).
inline constexpr std::array<std::uint16_t,126> Ak820Factory={
41,58,59,60,61,62,63,64,65,66,67,68,69,0,0,0,53,30,31,32,33,34,35,36,37,38,39,45,46,0,0,0,43,20,26,8,21,23,28,24,12,18,19,47,48,0,0,0,57,4,22,7,9,10,11,13,14,15,51,52,49,0,0,0,225,29,27,6,25,5,17,16,54,55,56,229,40,0,0,0,224,227,226,44,230,1033,0,228,80,81,82,79,42,0,0,0,0,0,0,0,0,0,0,0,74,75,76,77,78,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
inline constexpr const std::array<std::uint16_t,126>& FactoryFor(unsigned pid){return pid==Ak820Pid?Ak820Factory:Factory;}
inline constexpr unsigned KeyCount(unsigned pid){
    unsigned n=0;for(auto hid:FactoryFor(pid))n+=hid!=0;return n;
}
static_assert(KeyCount(0x80a2)==61 && KeyCount(Ak820Pid)==82);
inline bool KeyboardUsage(unsigned u){return (u>=4 && u<=0xa4) || (u>=0xe0 && u<=0xe7);}
// Driveall advanced-key pages on a physical key: DKS 8, MT 9, TGL 10,
// SOCD 11, RS 12 (AK820 advancedKeysList). The key still reports its own depth,
// so it keeps its factory key instead of becoming unassigned.
inline constexpr bool DriveallAdvanced(unsigned page){return page>=8 && page<=12;}
inline unsigned Assigned(const std::array<std::uint16_t,126>& factory,unsigned position,const std::uint8_t* record,bool driveall=false){
    if(position>=factory.size() || !factory[position])return 0;
    if(record[0]==0 || (driveall && DriveallAdvanced(record[0])))return factory[position];
    if(record[0]!=2 || record[3])return 0;
    if(record[1]){
        if(record[2] || (record[1]&(record[1]-1)))return 0;
        for(unsigned i=0;i<8;++i)if(record[1]==(1u<<i))return 224+i;
    }
    // HFD factory Fn (MAX V1.52 position 85) is a vendor action, not USB usage AF.
    // Driveall also encodes Fn as keyCode 175 (0xAF).
    if(record[2]==0xaf)return 0x409;
    return KeyboardUsage(record[2])?record[2]:0;
}
// SDK keyStroke is in 0.01 mm; reported stroke 34 is 3.4 mm.
// Driveall shows the same stream as keyStroke/100 and maxStroke/10 mm.
// Observed overshoot is clipped, never learned as a new endpoint.
inline unsigned Milli(unsigned travel,unsigned stroke){
    if(stroke<10 || stroke>50 || travel>1000)return 0;
    return (std::min)(1000u,(travel*1000u)/(stroke*10u));
}
inline unsigned Read(std::uint64_t packed,std::uint32_t now){
    if(!packed || std::uint32_t(now-std::uint32_t(packed>>32))>FreshMs)return 0;
    return Milli(unsigned(packed&65535),unsigned((packed>>16)&65535));
}
// AK820 0C45:80B1 stream, established from tester log 2026-10-08: the
// firmware reports changes only (a still held key sends nothing for seconds),
// reports no travel between 1 and 15 (0.01 mm), and often ends a release with
// a last value of 16..28 instead of 0. So a key keeps its last reported depth
// (no time expiry) and is released by a reported 0 or a value inside the
// keyboard's own top dead zone (GET_GAME_MODE, 0.01 mm). Stream loss (worker
// heartbeat) still clears every key.
inline unsigned Held(std::uint64_t packed,unsigned releaseFloor){
    const unsigned travel=unsigned(packed&65535);
    if(!packed || !travel || travel<=releaseFloor)return 0;
    return Milli(travel,unsigned((packed>>16)&65535));
}
inline std::uint64_t Pack(unsigned travel,unsigned stroke,std::uint32_t tick){
    return (std::uint64_t(tick)<<32)|(std::uint64_t(stroke)<<16)|travel;
}
}
