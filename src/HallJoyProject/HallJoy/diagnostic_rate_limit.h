#pragma once
#include <atomic>
#include <cstdint>

namespace halljoy {
// A call-site budget independent of error codes and key IDs. Alternating
// failures must not defeat throttling. The next report includes suppression.
class DiagnosticRateLimit {
    std::atomic<std::uint64_t> next_{0}, suppressed_{0};
public:
    bool Take(std::uint64_t now, std::uint64_t interval,
        std::uint64_t& suppressed) noexcept {
        auto next = next_.load(std::memory_order_relaxed);
        while (now >= next) {
            if (next_.compare_exchange_weak(next, now + interval,
                std::memory_order_relaxed)) {
                suppressed = suppressed_.exchange(0, std::memory_order_relaxed);
                return true;
            }
        }
        suppressed_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
};
}
