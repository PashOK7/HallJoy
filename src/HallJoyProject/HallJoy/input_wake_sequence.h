#pragma once

#include <atomic>
#include <cstdint>

namespace halljoy::realtime
{
    // Process-lifetime monotonic sequence. Notifications are never reset when
    // the worker restarts, so an input edge published before Start or just
    // before WaitOnAddress remains observable by the next loop iteration.
    class InputWakeSequence final
    {
    public:
        [[nodiscard]] std::uint64_t Notify() noexcept
        {
            return notified_.fetch_add(1u, std::memory_order_release) + 1u;
        }

        [[nodiscard]] std::uint64_t Observe() const noexcept
        {
            return notified_.load(std::memory_order_acquire);
        }

        [[nodiscard]] std::uint64_t Consumed() const noexcept
        {
            return consumed_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool Pending(std::uint64_t consumed) const noexcept
        {
            return Observe() != consumed;
        }

        void MarkConsumed(std::uint64_t observed) noexcept
        {
            consumed_.store(observed, std::memory_order_release);
        }

    private:
        std::atomic<std::uint64_t> notified_{ 0 };
        std::atomic<std::uint64_t> consumed_{ 0 };
    };
}
