#pragma once
#include <cstdint>
#include "sparklink_official_identities.h"
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
 default:return 0; // MG75 Max, Everglide SU75 Pro (0x3002), Weikav EG75 HE (0x5a01) and AULA AG75 are Supported (see ExactModel).
 }
}
// Exact models HallJoy lists by name: the confirmed MG75 Max (0x0529), the Supported
// Everglide SU75 Pro (0x3002, xsyd.top; catalog public v2, FFB0/1; tester-confirmed
// 2026-10-02), the Supported Weikav EG75 HE (0x5a01, official list; tester log 2026-10-26)
// and the experimental identities above.
// AULA AG75 (Supported; user report 2026-10-29): the PIDs of the official AULA web driver's
// isAG75 list (heu.aulacn.com), all on the official SparkLink list (FFB0/1).
constexpr bool AulaAg75(unsigned vid,unsigned pid) noexcept {
 return vid==0x38a6 && (pid==0x28de || pid==0x28df || pid==0x28e0 || pid==0x28e1 ||
                        pid==0x28f0 || pid==0x28f1 || pid==0x28f2);
}
constexpr bool ExactModel(unsigned vid,unsigned pid) noexcept {
 return (vid==0x1ca6 && (pid==0x0529 || pid==0x3002 || pid==0x5a01 || ExperimentalToken(vid,pid,0xffb0)!=0)) ||
        AulaAg75(vid,pid);
}
// The vendor's own driver lists this VID:PID as a SparkLink V2 keyboard (usage page 0xFFB0,
// usage 1). Identifiers only; the table is generated and sorted.
constexpr bool OfficialListed(unsigned vid,unsigned pid) noexcept {
 if(vid>0xffffu || pid>0xffffu)return false;
 const std::uint32_t key=(static_cast<std::uint32_t>(vid)<<16)|static_cast<std::uint32_t>(pid);
 std::size_t lo=0,hi=kOfficialIdentities.size();
 while(lo<hi) {
  const std::size_t mid=lo+(hi-lo)/2;
  if(kOfficialIdentities[mid]==key)return true;
  if(kOfficialIdentities[mid]<key)lo=mid+1; else hi=mid;
 }
 return false;
}
// Release switch: off in the 1.6.9.1 hotfix (owner decision 2026-10-12), enabled for 1.6.10
// (owner decision 2026-10-19). The generic path is software-tested, not hardware-tested.
inline constexpr bool kGenericAdmission=true;
// Official-list identity that is not a named model: connected through the protocol match
// only. The backend must prove the device-info reply and live travel reads before it is
// shown as connected, and shows the "keyboard not in the list" notice.
constexpr bool GenericOnly(unsigned vid,unsigned pid) noexcept {
 return kGenericAdmission && OfficialListed(vid,pid) && !ExactModel(vid,pid);
}
// Admission before any SparkLink command, not merely a status classification. Sending
// 01 02 to an unlisted device is unsafe (it is a reset command on SteelSeries Apex Pro),
// so the gate is the exact model list or the vendor's official identity list, never a
// bare vendor usage page.
constexpr bool ProbeIdentity(unsigned vid,unsigned pid) noexcept {
 return ExactModel(vid,pid) || GenericOnly(vid,pid);
}
constexpr bool ProbeInterface(unsigned vid,unsigned pid,unsigned page,unsigned usage) noexcept {
 return ProbeIdentity(vid,pid) && page==0xffb0 && usage==1;
}
}
