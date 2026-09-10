#include "keyboard_support_status.h"
#include <atomic>

namespace halljoy::keyboard_support
{
namespace
{
std::atomic<unsigned> g_snapshot{0};
}

void SetSearchObservation(bool searchCompleted, bool connected) noexcept
{
    g_snapshot.store((searchCompleted ? 1u : 0u) | (searchCompleted && connected ? 2u : 0u), std::memory_order_release);
}

StatusSnapshot GetStatusSnapshot() noexcept
{
    const unsigned snapshot = g_snapshot.load(std::memory_order_acquire);
    return { (snapshot & 1u) != 0, (snapshot & 2u) != 0 };
}
}
