#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
namespace halljoy::mini60 {
inline constexpr std::uint64_t LayoutToken=0x4d363050414e5349ull;
inline constexpr const wchar_t* Preset=L"Aula MINI60 HE Pro ANSI";
inline constexpr unsigned FreshMs=50;
// Physical positions from pinned HFD layout o and master map y (61 keys).
inline constexpr std::array<std::uint16_t,126> Factory={
41,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,31,32,33,34,35,36,37,38,39,45,46,0,0,0,43,20,26,8,21,23,28,24,12,18,19,47,48,0,0,0,57,4,22,7,9,10,11,13,14,15,51,52,49,0,0,0,225,29,27,6,25,5,17,16,54,55,56,229,40,0,0,0,224,227,226,44,230,1033,101,228,0,0,0,0,42,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
inline bool KeyboardUsage(unsigned u){return (u>=4 && u<=0xa4) || (u>=0xe0 && u<=0xe7);}
inline unsigned Assigned(unsigned position,const std::uint8_t* record){
    if(position>=Factory.size() || !Factory[position])return 0;
    if(record[0]==0)return Factory[position];
    if(record[0]!=2 || record[3])return 0;
    if(record[1]){
        if(record[2] || (record[1]&(record[1]-1)))return 0;
        for(unsigned i=0;i<8;++i)if(record[1]==(1u<<i))return 224+i;
    }
    return KeyboardUsage(record[2])?record[2]:0;
}
// SDK keyStroke is in 0.01 mm; reported stroke 34 is 3.4 mm.
// Observed overshoot is clipped, never learned as a new endpoint.
inline unsigned Milli(unsigned travel,unsigned stroke){
    if(stroke<10 || stroke>50 || travel>1000)return 0;
    return (std::min)(1000u,(travel*1000u)/(stroke*10u));
}
inline unsigned Read(std::uint64_t packed,std::uint32_t now){
    if(!packed || std::uint32_t(now-std::uint32_t(packed>>32))>FreshMs)return 0;
    return Milli(unsigned(packed&65535),unsigned((packed>>16)&65535));
}
inline std::uint64_t Pack(unsigned travel,unsigned stroke,std::uint32_t tick){
    return (std::uint64_t(tick)<<32)|(std::uint64_t(stroke)<<16)|travel;
}
}
