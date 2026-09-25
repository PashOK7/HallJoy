#pragma once
#include <cstdint>
namespace halljoy::sparklink {
// Exact wired JingTai V2 identities in the official IROK and EWEADN clients. The ordinary
// backend still proves device/layout/travel replies before claiming the HID path.
constexpr std::uint64_t ExperimentalToken(unsigned vid,unsigned pid,unsigned page) noexcept {
 if(vid!=0x1ca6 || page!=0xffb0)return 0;
 switch(pid) {
 case 0x0528: // IROK RA68 (not HuoChaiRen RA68 revision)
 case 0x052a: // IROK MG68 Plus
 case 0x052b: // CAROTMAS Mars75 / Mars75 Pro (same wire protocol)
 case 0x052d: // CAROTMAS Mer68 Max
 case 0x052c: // IROK ND68 Pro
 case 0x0540: // CAROTMAS Mer68 SE, JingTai V2 only
 case 0x0531: // IROK ND63 Ultra
 case 0x1c0a: // EWEADN: X87HE
 case 0x1c0c: // EWEADN: DEEP80 Max HE (magnetic version)
 case 0x1c12: // EWEADN: DK68 HE
 case 0x1c14: // EWEADN: DEEP68 Pro HE
 case 0x1c1a: // EWEADN: DK68 V2 HE
 case 0x1c1f: // EWEADN: DK68 HE
 case 0x1c23: // EWEADN: DK68 Pro HE
 case 0x1c24: // EWEADN: ES68
 case 0x1c2b: // EWEADN: DK68 Star HE, DK63 Star HE
 case 0x1c2c: // EWEADN: DK68 Pro HE
 case 0x1c2f: // EWEADN: ES68 EVO
 case 0x1c37: // EWEADN: Gamma75 HE (EXX collaboration)
 case 0x1c3c: // EWEADN: DK68 V2 HE
 case 0x1c3d: // EWEADN: DK80 HE
 case 0x1c45: // EWEADN: Gamma75 HE (EXX collaboration)
 case 0x1c4a: // EWEADN: ES68 Lite
 case 0x1c4c: // EWEADN: ES68 EVO
 case 0x2708: // EWEADN: DK75 E HE
 case 0x2709: // EWEADN: DK75 Pro HE
 case 0x270a: // EWEADN: DK75 HE
 case 0x5e01: // EWEADN: DK63 HE
  return (static_cast<std::uint64_t>(vid)<<16)|pid;
 default:return 0; // MG75 Max retains its confirmed status.
 }
}
}
