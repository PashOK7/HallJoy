#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace halljoy::drunkdeer_identity {
// Append only: values travel through the plugin telemetry and first-run selection.
enum class Model : std::uint8_t { Unknown, A75Ansi, A75Pro, A75Iso, G60, G65, G75Ansi, G75Jis, A75Ultra };
// Tracking report formats of the official Antler driver (index.CJWCGjvj.js):
// Legacy        B6 03 01 -> three B7 chunks, u8 travel, HallJoy's nominal 4.0 mm = 40.
// HighPrecision same B7 frames on newer A75 firmware (precision 2 with
//               oldOpenHighPrecision), u8 in 0.01 mm; the driver's full bar is 200.
// Wide          A75 Ultra: FD 03 01 -> five FD 06 chunks of 30/30/30/30/6 u16 values,
//               0.005 mm; the driver's full bar is 3.3 mm * 200 = 660.
enum class Tracking : std::uint8_t { Legacy, HighPrecision, Wide };
struct Identity {
    Model model=Model::Unknown;
    std::array<std::uint8_t,3> signature{};
    // Driver: Number(String(payload[8]) + String(payload[7])), e.g. 3,5 -> 35.
    unsigned firmware=0;
};
inline constexpr std::array<std::uint8_t,64> Request() {
    std::array<std::uint8_t,64> r{};
    r[0]=4; r[1]=0xA0; r[2]=2;
    return r;
}
// Native full report includes report ID; Antler payload offsets are one less.
// Unknown signatures, short replies and another command never become A75.
inline Identity Parse(const std::uint8_t* r,std::size_t size) noexcept {
    Identity id;
    if (!r || size!=64 || r[0]!=4 || r[1]!=0xA0 || r[2]!=2 || r[3]!=0) return id;
    id.signature={r[5],r[6],r[7]};
    unsigned scale=10;
    for (unsigned low=r[8]; low>=10; low/=10) scale*=10;
    id.firmware=unsigned(r[9])*scale+r[8];
    if (id.signature==std::array<std::uint8_t,3>{11,1,1} || id.signature==std::array<std::uint8_t,3>{11,4,1}) id.model=Model::A75Ansi;
    else if (id.signature==std::array<std::uint8_t,3>{11,4,3}) id.model=Model::A75Pro;
    else if (id.signature==std::array<std::uint8_t,3>{11,4,2}) id.model=Model::A75Iso;
    else if (id.signature==std::array<std::uint8_t,3>{11,3,1}) id.model=Model::G60;
    else if (id.signature==std::array<std::uint8_t,3>{11,2,1} || id.signature==std::array<std::uint8_t,3>{15,1,1}) id.model=Model::G65;
    else if (id.signature==std::array<std::uint8_t,3>{11,4,5}) id.model=Model::G75Ansi;
    else if (id.signature==std::array<std::uint8_t,3>{11,4,7}) id.model=Model::G75Jis;
    else if (id.signature==std::array<std::uint8_t,3>{11,4,4}) id.model=Model::A75Ultra;
    return id;
}
inline constexpr const char* Name(Model model) noexcept {
    switch(model) {
    case Model::A75Ansi:return "DrunkDeer A75 ANSI";
    case Model::A75Pro:return "DrunkDeer A75 Pro";
    case Model::A75Iso:return "DrunkDeer A75 ISO";
    case Model::G60:return "DrunkDeer G60 ANSI";
    case Model::G65:return "DrunkDeer G65 ANSI";
    case Model::G75Ansi:return "DrunkDeer G75 ANSI";
    case Model::G75Jis:return "DrunkDeer G75 JIS";
    case Model::A75Ultra:return "DrunkDeer A75 Ultra";
    default:return nullptr;
    }
}
inline constexpr std::uint16_t Product(Model model) noexcept {
    switch(model) {
    case Model::A75Ansi:case Model::A75Pro:case Model::A75Iso:case Model::A75Ultra:return 0x2383;
    case Model::G65:return 0x2382;
    case Model::G60:return 0x2384;
    case Model::G75Ansi:return 0x2386;
    case Model::G75Jis:return 0x2391;
    default:return 0;
    }
}
inline constexpr bool MatchesProduct(Model model,std::uint16_t pid) noexcept {
    return model!=Model::Unknown && Product(model)==pid;
}
// Firmware thresholds where the official driver switches A75 boards to
// high-precision B7 values (A75 >= 35, A75 Pro >= 17, A75 ISO >= 25).
inline constexpr Tracking TrackingMode(Model model,unsigned firmware) noexcept {
    switch(model) {
    case Model::A75Ultra:return Tracking::Wide;
    case Model::A75Ansi:return firmware>=35?Tracking::HighPrecision:Tracking::Legacy;
    case Model::A75Pro:return firmware>=17?Tracking::HighPrecision:Tracking::Legacy;
    case Model::A75Iso:return firmware>=25?Tracking::HighPrecision:Tracking::Legacy;
    default:return Tracking::Legacy;
    }
}
inline constexpr unsigned FullScale(Tracking mode) noexcept {
    return mode==Tracking::Wide?660u:mode==Tracking::HighPrecision?200u:40u;
}
// The driver zeroes values below these (HighPrecision w<2, Wide a<40); Legacy is
// unchanged from the shipped decoder.
inline constexpr unsigned NoiseFloor(Tracking mode) noexcept {
    return mode==Tracking::Wide?40u:mode==Tracking::HighPrecision?2u:1u;
}
inline constexpr float Travel(Tracking mode,unsigned raw) noexcept {
    if (raw<NoiseFloor(mode)) return 0.0f;
    const float value=static_cast<float>(raw)/static_cast<float>(FullScale(mode));
    return value>1.0f?1.0f:value;
}
// A75 Ultra uses the A75 ANSI tracking positions; its one extra cell (15, F13)
// has no entry in that map and stays unbound.
inline constexpr Model TrackingSource(Model model) noexcept {
    return model==Model::A75Ultra?Model::A75Ansi:model;
}
inline constexpr std::array<std::uint8_t,64> WideTrackingRequest() {
    std::array<std::uint8_t,64> r{};
    r[0]=4; r[1]=0xFD; r[2]=3; r[3]=1;
    return r;
}
inline constexpr std::size_t kWideChunks=5;
inline constexpr std::size_t WideChunkValues(std::size_t chunk) noexcept { return chunk<4?30:6; }
// Native report with report ID: 04 FD 06 <chunk> then little-endian u16 values.
inline bool WideChunk(const std::uint8_t* r,std::size_t size,std::size_t& chunk) noexcept {
    if (!r || size!=64 || r[0]!=4 || r[1]!=0xFD || r[2]!=6 || r[3]>=kWideChunks) return false;
    chunk=r[3];
    return true;
}
inline constexpr unsigned WideValue(const std::uint8_t* r,std::size_t index) noexcept {
    return unsigned(r[4+2*index])|(unsigned(r[5+2*index])<<8);
}
// DrunkDeer boards on the NuPhy USB VID (official driver drunkdeer.keybord.net.cn,
// wired usage 1:0): A75 Ultra FB3E, A75 Master FB5E, A75 Pro FB5C/FB63, X60 FB50,
// X60 HE FB42, A75 Air FB7B. Their A0 records carry travel as big-endian u16 in
// bytes 6..7 in 0.01 mm (driver: F([e[7],e[6]]), switch travel 400 = 4.00 mm);
// bytes 4..5 are not travel. NuPhy's own boards keep their bytes 4..5 decoder.
inline constexpr bool IsNuphyPlatformBoard(std::uint16_t vid,std::uint16_t pid) noexcept {
    if (vid!=0x19F5) return false;
    switch(pid) {
    case 0xFB3E:case 0xFB5E:case 0xFB5C:case 0xFB63:case 0xFB50:case 0xFB42:case 0xFB7B:return true;
    default:return false;
    }
}
inline constexpr unsigned kNuphyPlatformFullScale=400;
inline constexpr unsigned NuphyPlatformTravel(const std::uint8_t* r) noexcept {
    const unsigned raw=(unsigned(r[6])<<8)|r[7];
    return raw>kNuphyPlatformFullScale?kNuphyPlatformFullScale:raw;
}
// A0 key records carry the key's type (official driver K(): 16 key with modifier mask,
// 240 Fn layer keys, 224 other layer key). Anything else starting with A0 is a reply
// to the vendor command 0xA0 (key trigger table), not travel.
inline constexpr bool IsNuphyPlatformKeyType(std::uint8_t type) noexcept {
    return type==16 || type==240 || type==224;
}
// Physical key of an A0 record, as the official driver's K(type, b2, b3) (it matches the
// result against the layout's factory codes, so remaps do not change it): type 16 = HID
// usage in b3, or a modifier from the single-bit mask in b2; Fn1 = 240/255/1 -> HallJoy Fn
// 0x409; Fn2 = 240/255/2 or 224/0/x -> HallJoy Fn2 0x403. 0 = not a key.
inline constexpr std::uint16_t NuphyPlatformKey(std::uint8_t type,std::uint8_t b2,std::uint8_t b3) noexcept {
    if (type==16) {
        switch(b2) {
        case 0:return b3;
        case 1:return 224; case 2:return 225; case 4:return 226; case 8:return 227;
        case 16:return 228; case 32:return 229; case 64:return 230; case 128:return 231;
        default:return b3;
        }
    }
    if (type==240 && b2==255 && b3==1) return 0x409;
    if (type==240 && b2==255 && b3==2) return 0x403;
    if (type==224 && b2==0) return 0x403;
    return 0;
}
}
