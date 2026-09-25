#include "keyboard_support_status.h"
#include <atomic>

namespace halljoy::keyboard_support
{
namespace
{
std::atomic<unsigned> g_snapshot{0};
std::atomic<unsigned> g_anomalies[256]{};
}

void ReportCommunicationAnomaly(unsigned source) noexcept { if(source<256)g_anomalies[source].fetch_add(1,std::memory_order_relaxed); }
unsigned CommunicationAnomalySequence(unsigned source) noexcept { return source<256?g_anomalies[source].load(std::memory_order_relaxed):0; }

void SetSearchObservation(bool searchCompleted, bool connected, unsigned frozenModels, bool communicationWarning) noexcept
{
    g_snapshot.store((searchCompleted ? 1u : 0u) | (searchCompleted && connected ? 2u : 0u) | (searchCompleted ? (frozenModels & 0x1fffffffu) << 2 : 0u) | (searchCompleted && communicationWarning ? 0x80000000u : 0u), std::memory_order_release);
}

StatusSnapshot GetStatusSnapshot() noexcept
{
    const unsigned snapshot = g_snapshot.load(std::memory_order_acquire);
    return { (snapshot & 1u) != 0, (snapshot & 2u) != 0, (snapshot & 0x7fffffffu) >> 2, (snapshot & 0x80000000u) != 0 };
}
}
