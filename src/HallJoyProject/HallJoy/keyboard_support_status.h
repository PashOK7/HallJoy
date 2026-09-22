#pragma once

#include <cstdint>
#include <string_view>
namespace halljoy::keyboard_support
{
enum FrozenModel : unsigned { NA87 = 1, NA87Pro = 2, ND75 = 4, Hero84 = 8, Azoth96 = 16, X68 = 32, FamilyCandidate = 64, Mg75Pro = 128, AttackShark = 256, GravaStar = 512, Ipi = 1024, Redragon = 2048, NuPhy = 4096, AulaRm = 8192 };
inline constexpr unsigned ImplementedModels = NA87 | Hero84 | Mg75Pro | AttackShark | GravaStar | Ipi | Redragon | AulaRm;
// Metadata-only classification. Shared USB IDs never prove the model alone.
inline unsigned ClassifyFrozen(unsigned vid, unsigned pid, std::wstring_view name) noexcept {
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
};

// A negative result is meaningful only after the engine owns an active
// generation. Until then the UI must remain silent while discovery starts.
void SetSearchObservation(bool searchCompleted, bool connected, unsigned frozenModels = 0) noexcept;
[[nodiscard]] StatusSnapshot GetStatusSnapshot() noexcept;
}
