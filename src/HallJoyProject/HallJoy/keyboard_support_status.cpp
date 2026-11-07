#include "keyboard_support_status.h"
#include <atomic>
#include <cstdint>

namespace halljoy::keyboard_support
{
namespace
{
// One atomic snapshot: state bits0..2, 48 notice bits3..50, catalog index51..58.
// Keep notice bits separate from the unsupported catalog index when widening.
constexpr std::uint64_t kNoticeBits = (1ull << 48) - 1;
std::atomic<std::uint64_t> g_snapshot{0};
std::atomic<unsigned> g_anomalies[256]{};
}

void ReportCommunicationAnomaly(unsigned source) noexcept { if(source<256)g_anomalies[source].fetch_add(1,std::memory_order_relaxed); }
unsigned CommunicationAnomalySequence(unsigned source) noexcept { return source<256?g_anomalies[source].load(std::memory_order_relaxed):0; }

bool ReportCommunicationAccessFailure(unsigned source,unsigned error) noexcept {
    if(source>=256 || (error!=32 && error!=33))return false;
    ReportCommunicationAnomaly(source);return true;
}

void SetSearchObservation(bool searchCompleted, bool connected, NoticeMask frozenModels, bool communicationWarning,
                          unsigned unsupportedModel) noexcept
{
    std::uint64_t snapshot = 0;
    if (searchCompleted) {
        snapshot = 1u;
        if (connected) snapshot |= 2u;
        if (communicationWarning) snapshot |= 4u;
        snapshot |= (frozenModels & kNoticeBits) << 3;
        snapshot |= static_cast<std::uint64_t>(unsupportedModel & 0xffu) << 51;
    }
    g_snapshot.store(snapshot, std::memory_order_release);
}

StatusSnapshot GetStatusSnapshot() noexcept
{
    const std::uint64_t snapshot = g_snapshot.load(std::memory_order_acquire);
    return { (snapshot & 1u) != 0, (snapshot & 2u) != 0,
             (snapshot >> 3) & kNoticeBits, (snapshot & 4u) != 0,
             static_cast<unsigned>((snapshot >> 51) & 0xffu) };
}
}
