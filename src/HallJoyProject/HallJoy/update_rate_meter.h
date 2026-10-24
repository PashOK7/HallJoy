#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace halljoy {

// Keyboard update rate measured from a backend's successful-update counter,
// sampled whenever telemetry is collected (irregular instants). Backends that
// report only changes send nothing while keys are still, so "now" is honestly 0
// then; "peak" is the 99th percentile of the windows that carried updates, i.e.
// the highest sustained rate without the top 1% of outliers.
class UpdateRateMeter final {
public:
    static constexpr std::uint64_t kWindowUs = 500000;
    static constexpr std::size_t kHistory = 1024;

    void Reset() noexcept { *this = UpdateRateMeter{}; }

    void Observe(std::uint64_t count, std::uint64_t nowUs) noexcept {
        if (!started_ || count < windowCount_ || nowUs < windowStartUs_) {
            // First sample or a new session (counter restarted): start a fresh window.
            started_ = true; windowStartUs_ = nowUs; windowCount_ = count; currentHz10_ = 0;
            return;
        }
        const auto elapsed = nowUs - windowStartUs_;
        if (elapsed < kWindowUs) return;
        const auto delta = count - windowCount_;
        currentHz10_ = static_cast<std::uint32_t>((std::min)(delta * 10000000ull / elapsed, 0xffffffffull));
        if (delta) {
            history_[next_] = currentHz10_;
            next_ = (next_ + 1) % kHistory;
            filled_ = (std::min)(filled_ + 1, kHistory);
        }
        windowStartUs_ = nowUs; windowCount_ = count;
    }

    std::uint32_t CurrentHz10() const noexcept { return currentHz10_; }

    std::uint32_t PeakHz10() const noexcept {
        if (!filled_) return 0;
        std::array<std::uint32_t, kHistory> sorted{};
        std::copy_n(history_.begin(), filled_, sorted.begin());
        // Nearest rank: the value below which 99% of active windows fall.
        const std::size_t rank = (filled_ * 99 + 99) / 100;
        std::nth_element(sorted.begin(), sorted.begin() + (rank - 1), sorted.begin() + filled_);
        return sorted[rank - 1];
    }

private:
    bool started_ = false;
    std::uint64_t windowStartUs_ = 0, windowCount_ = 0;
    std::uint32_t currentHz10_ = 0;
    std::array<std::uint32_t, kHistory> history_{};
    std::size_t next_ = 0, filled_ = 0;
};

}
