#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

namespace halljoy::profile_runtime {
inline std::atomic<std::uint32_t> readers{0};
inline std::mutex writerMutex;
inline constexpr std::uint32_t kWriter = 0x80000000u;
class ReadLease {
    bool acquired_ = false;
public:
    ReadLease() noexcept {
        // A failed CAS can mean only that another reader incremented the
        // count, not that a profile writer began. Retry a small fixed number
        // of times so a contended realtime tick does not spuriously vanish;
        // never spin or wait behind an actual writer.
        for (unsigned attempt = 0; attempt < 3u; ++attempt) {
            auto state = readers.load(std::memory_order_acquire);
            if (state & kWriter) return;
            if (readers.compare_exchange_weak(state, state + 1,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                acquired_ = true;
                return;
            }
        }
    }
    ~ReadLease() { if (acquired_) readers.fetch_sub(1, std::memory_order_release); }
    explicit operator bool() const noexcept { return acquired_; }
    ReadLease(const ReadLease&) = delete;
    ReadLease& operator=(const ReadLease&) = delete;
};
// UI only. Parsing, allocations and persistence must finish before acquisition.
class CommitLease {
    std::unique_lock<std::mutex> owner_{writerMutex, std::try_to_lock};
    bool acquired_ = false;
public:
    CommitLease() {
        if (!owner_.owns_lock()) return;
        readers.fetch_or(kWriter, std::memory_order_acq_rel);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (readers.load(std::memory_order_acquire) != kWriter) {
            if (std::chrono::steady_clock::now() >= deadline) {
                readers.fetch_and(~kWriter, std::memory_order_release); return;
            }
            std::this_thread::yield();
        }
        acquired_ = true;
    }
    ~CommitLease() { if (acquired_) readers.fetch_and(~kWriter, std::memory_order_release); }
    explicit operator bool() const noexcept { return acquired_; }
    CommitLease(const CommitLease&) = delete;
    CommitLease& operator=(const CommitLease&) = delete;
};
}
