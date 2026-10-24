#include "keyboard_support_status.h"
#include <atomic>
#include <cstdint>

namespace halljoy::keyboard_support
{
namespace
{
// bit 0 search completed, bit 1 analog source connected, bit 2 communication warning, bits 3..34 frozen-model flags
// (all 32 FrozenModel bits; the earlier 32-bit packing silently dropped bits 29 and 30, which hid the
// "Keyboard not in the list" and MelGeek MADE68 Air notices together with their Open log button),
// bits 35..42 unsupported keyboard catalog index.
std::atomic<std::uint64_t> g_snapshot{0};
std::atomic<unsigned> g_anomalies[256]{};
}

void ReportCommunicationAnomaly(unsigned source) noexcept { if(source<256)g_anomalies[source].fetch_add(1,std::memory_order_relaxed); }
unsigned CommunicationAnomalySequence(unsigned source) noexcept { return source<256?g_anomalies[source].load(std::memory_order_relaxed):0; }

bool ReportCommunicationAccessFailure(unsigned source,unsigned error) noexcept {
    if(source>=256 || (error!=32 && error!=33))return false;
    ReportCommunicationAnomaly(source);return true;
}

void SetSearchObservation(bool searchCompleted, bool connected, unsigned frozenModels, bool communicationWarning,
                          unsigned unsupportedModel) noexcept
{
    std::uint64_t snapshot = 0;
    if (searchCompleted) {
        snapshot = 1u;
        if (connected) snapshot |= 2u;
        if (communicationWarning) snapshot |= 4u;
        snapshot |= static_cast<std::uint64_t>(frozenModels) << 3;
        snapshot |= static_cast<std::uint64_t>(unsupportedModel & 0xffu) << 35;
    }
    g_snapshot.store(snapshot, std::memory_order_release);
}

StatusSnapshot GetStatusSnapshot() noexcept
{
    const std::uint64_t snapshot = g_snapshot.load(std::memory_order_acquire);
    return { (snapshot & 1u) != 0, (snapshot & 2u) != 0,
             static_cast<unsigned>(snapshot >> 3), (snapshot & 4u) != 0,
             static_cast<unsigned>((snapshot >> 35) & 0xffu) };
}
}
