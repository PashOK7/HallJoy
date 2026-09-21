#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "native_layout_state.h"

namespace halljoy::sayo::o3c {
inline constexpr std::array<std::uint16_t,3> Factory{0x1d,0x1b,0x06};
inline constexpr std::uint8_t DepthEcho=0x12, ConfigEcho=0x13;
inline std::uint16_t U16(const std::uint8_t* p) noexcept {
    return std::uint16_t(p[0] | (std::uint16_t(p[1])<<8));
}
struct Frame { const std::uint8_t* payload=nullptr; std::size_t size=0;
    std::uint8_t command=0,index=0,echo=0; };
inline bool Parse(const std::uint8_t* p,std::size_t n,Frame& f) noexcept {
    f={};
    if (!p || n<8 || p[0]!=0x22) return false;
    const auto length=U16(p+4);
    // Reject continuation/error flags; only complete single-command replies apply.
    if (length<4 || length>1020 || std::size_t(length)+4>n) return false;
    const std::size_t end=(std::size_t(length)+5)&~std::size_t(1);
    if (end>n) return false;
    std::uint16_t sum=0;
    for (std::size_t i=0;i<end;i+=2) if (i!=2) sum=std::uint16_t(sum+U16(p+i));
    if (sum!=U16(p+2)) return false;
    f={p+8,std::size_t(length-4),p[6],p[7],p[1]};return true;
}
// Empty payload is the firmware's read branch for Info and KeyInfo. Never send Save.
inline std::array<std::uint8_t,1024> Read(std::uint8_t command,std::uint8_t index=0) noexcept {
    std::array<std::uint8_t,1024> p{};
    p[0]=0x22;p[1]=ConfigEcho;p[4]=4;p[6]=command;p[7]=index;
    const auto sum=std::uint16_t(U16(p.data())+4+U16(p.data()+6));
    p[2]=std::uint8_t(sum);p[3]=std::uint8_t(sum>>8);return p;
}
inline bool Identity(const Frame& f) noexcept {
    return f.echo==ConfigEcho && f.command==0 && f.index==0 && f.size>=4 && U16(f.payload)==9;
}
inline bool Binding(const Frame& f,std::uint8_t index,std::uint16_t& hid) noexcept {
    hid=0;
    if (index>=3 || f.echo!=ConfigEcho || f.command!=0x10 || f.index!=index || f.size!=56) return false;
    const auto* p=f.payload;
    // Audited O3C v1 geometry: three 1800x1800 keys at x=1000/3000/5000, y=3000.
    if (p[0]!=1 || U16(p+4)!=1000+index*2000 || U16(p+6)!=3000 ||
        U16(p+8)!=1800 || U16(p+10)!=1800) return false;
    // Base-layer default action only. Chords, scripts and multi-step actions
    // cannot be represented faithfully by a single HID usage.
    if (p[16]!=0 || p[22]!=0 || p[23]!=0) return false;
    const auto modifiers=p[20],key=p[21];
    if (modifiers && key) return false;
    if (modifiers) {
        if (modifiers & (modifiers-1)) return false;
        for (unsigned bit=0;bit<8;++bit) if (modifiers==(1u<<bit)) hid=std::uint16_t(0xe0+bit);
    } else {
        if (key && (key<4 || key>0xe7)) return false;
        hid=key;
    }
    return true;
}
inline bool Depth(const Frame& f,std::array<std::uint16_t,3>& raw) noexcept {
    if(f.echo!=DepthEcho || f.command!=0x15 || f.index!=1 || f.size!=6) return false;
    for(std::size_t i=0;i<3;++i) {
        raw[i]=U16(f.payload+2*i);
        if(raw[i]>8000) return false;
    }
    return true;
}
inline std::uint16_t Normalize(std::uint16_t raw) noexcept {
    unsigned m=(unsigned(raw)*1000+2000)/4000;
    return std::uint16_t(m<4?0:m>1000?1000:m);
}
inline std::array<native_layout::Key,3> Layout(const std::array<std::uint16_t,3>& bindings) noexcept {
    std::array<native_layout::Key,3> keys{};
    for(std::size_t i=0;i<3;++i) {
        keys[i].factory=Factory[i]; keys[i].assigned=std::uint16_t(keycode::kO3cFirst+i);
        keys[i].labelHid=bindings[i];
        if(!bindings[i]) {keys[i].label={L'K',L'e',L'y',L' ',wchar_t(L'1'+i),0};}
    }
    return keys;
}
inline bool Matches(std::size_t index,std::uint16_t hid,bool automatic,std::uint16_t binding) noexcept {
    if(index>=3 || !hid) return false;
    return automatic ? hid==keycode::kO3cFirst+index || (binding && hid==binding) : hid==Factory[index];
}
inline std::uint16_t Value(std::uint16_t hid,const std::array<std::uint16_t,3>& assigned,
                          const std::array<std::uint16_t,3>& depth) noexcept {
    std::uint16_t result=0;
    if(hid) for(std::size_t i=0;i<3;++i) if(assigned[i]==hid && depth[i]>result) result=depth[i];
    return result;
}
} // namespace halljoy::sayo::o3c
