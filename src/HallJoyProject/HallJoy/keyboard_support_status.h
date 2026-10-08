#pragma once

#include <cstdint>
#include <string_view>
#include "keyboard_communication_health.h"
#include "jingtai_v1_profiles.h"
namespace halljoy::keyboard_support
{
enum FrozenModel : unsigned { NA87 = 1, NA87Pro = 2, ND75 = 4, Hero84 = 8, Azoth96 = 16, X68 = 32, FamilyCandidate = 64, Mg75Pro = 128, AttackShark = 256, GravaStar = 512, Ipi = 1024, Redragon = 2048, NuPhy = 4096, AulaRm = 8192, Slice75 = 16384, RongYuan = 32768, RongYuanStream = 65536, TartarusPro = 131072, Neo65 = 262144, SparkLinkV2 = 524288, SteelSeriesApex = 1048576, Mad68DualLimited = 2097152, Mix87Limited = 4194304, Alumix104Research = 8388608, AtkHex80Family = 16777216, MchoseFamily = 33554432, RoyalKludgeHe = 67108864, LogitechRapid = 134217728, RedSquareAlumix68 = 268435456, GenericProtocol = 536870912 };
inline constexpr unsigned LimitedModels = Mad68DualLimited | Mix87Limited;
// Protocol 28 is the exact 0C45:80AC research backend. Present means the
// physical device was found; connected would falsely imply usable analog.
// Protocol 31 is the Alumix 68 analog backend: present without connected means
// the device is on the bus but its firmware version was not recognized, so the
// RAM depth path is withheld (the yellow notice then asks for the log).
inline unsigned ResearchNotice(unsigned protocol, unsigned vid, unsigned pid,
                               bool present, bool connected) noexcept {
    if (protocol == 28 && vid == 0x0c45 && pid == 0x80ac && present && !connected)
        return static_cast<unsigned>(Alumix104Research);
    if (protocol == 31 && vid == 0x0c45 && pid == 0x80a2 && present && !connected)
        return static_cast<unsigned>(RedSquareAlumix68);
    return 0u;
}
inline constexpr unsigned ImplementedModels = NA87 | Hero84 | Mg75Pro | AttackShark | GravaStar | Ipi | Redragon | AulaRm | Slice75 | RongYuan | RongYuanStream | TartarusPro | Neo65 | SparkLinkV2 | SteelSeriesApex | AtkHex80Family | MchoseFamily | RoyalKludgeHe | LogitechRapid | RedSquareAlumix68 | GenericProtocol;
// A keyboard outside the catalog connected through a protocol-family match
// (BY/IPI UUID platform, Logitech HID++ 0x1B08): yellow, model unverified.
inline unsigned GenericNotice(bool connected, bool genericProtocol) noexcept {
    return connected && genericProtocol ? static_cast<unsigned>(GenericProtocol) : 0u;
}
// Metadata-only classification. Shared USB IDs never prove the model alone.
inline unsigned ClassifyFrozen(unsigned vid, unsigned pid, std::wstring_view name) noexcept {
    if (vid == 0x28e9 && pid == 0x3265) return Mad68DualLimited;
    // Mix87 III and Jet 75 II are supported; admission remains enforced by their backends.
    if (vid == 0x3837 && pid == 0x300d) return 0;
    // WLMOUSE Ying75 (36A7:F887) is Supported; admission remains enforced by its backend.
    if (halljoy::jingtai_v1::Find(vid,pid,name)) return Mg75Pro;
    if (vid == 0x19f5 && (pid == 0x6130 || pid == 0x6132 || pid == 0x6112 || pid == 0xa011)) return NuPhy;
    if (vid == 0x0416 && pid == 0x7372) {
        if (name == L"GK8260HERGB" || name == L"NA87 MAG") return 0;
        if (name == L"ND75" || name == L"IROK ND75") return ND75;
    }
    if (((vid == 0x1c4f && pid == 0xee88) || (vid == 0x1ca2 && pid == 0x0401)) &&
        (name == L"NA87 PRO" || name == L"IROK NA87 PRO")) return NA87Pro;
    if (vid == 0x372e && pid == 0x103e &&
        (name == L"HERO84 HE" || name == L"AULA HERO84 HE" || name == L"HERO84HE")) return Hero84;
    if (vid == 0x1ca5 && pid == 0x0807 && name == L"IROK MG75 PRO") return Mg75Pro;
    if (vid == 0x0b05 && pid == 0x1c10) return Azoth96;
    if (vid == 0x3151 && (pid == 0x5029 || pid == 0x502d || pid == 0x502f || pid == 0x5030))
        return FamilyCandidate;
    if ((vid==0x0416 && pid==0x7372) || (vid==0x1c4f && pid==0xee88) ||
        (vid==0x1ca2 && pid==0x0401) || (vid==0x372e && pid==0x103e)) return FamilyCandidate;
    return 0;
}
struct StatusSnapshot final
{
    bool searchCompleted = false;
    bool analogSourceConnected = false;
    unsigned frozenModels = 0;
    bool communicationWarning = false;
};

// Known low-quality firmware is a permanent limitation, not an automatic log incident.
inline bool ShouldAutoSaveSupportLog(const StatusSnapshot& s) noexcept {
    if (!s.searchCompleted || s.communicationWarning) return false;
    const auto advisoryOnly = LimitedModels | Alumix104Research;
    const auto other = s.frozenModels & ~advisoryOnly;
    if ((s.frozenModels & advisoryOnly) && !other) return false;
    return !s.analogSourceConnected || s.frozenModels != 0;
}

// A negative result is meaningful only after the engine owns an active
// generation. Until then the UI must remain silent while discovery starts.
void SetSearchObservation(bool searchCompleted, bool connected, unsigned frozenModels = 0, bool communicationWarning = false) noexcept;
[[nodiscard]] StatusSnapshot GetStatusSnapshot() noexcept;
}
