#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cstdint>
namespace halljoy::physical_analog {
constexpr std::size_t kHidCount = 0x410;
// Physical-key publication prevents one remapped alias from releasing another.
// Each HID walks only its own short alias chain; no full-keyboard scan on reads.
class Publication {
public:
    struct Value { bool fresh = false; std::uint16_t milli = 0; };
    bool Clear() noexcept {
        bool changed = false;
        for (auto& s : stamps_) s.store(0, std::memory_order_release);
        for (auto& v : values_) changed = v.exchange(0, std::memory_order_acq_rel) != 0 || changed;
        for (auto& f : first_) f.store(0, std::memory_order_release);
        for (auto& n : next_) n.store(0, std::memory_order_relaxed);
        for (auto& b : bound_) b.store(false, std::memory_order_relaxed);
        return changed;
    }
    bool Bind(std::uint8_t id, std::uint16_t hid) noexcept {
        if (!id || !hid || hid >= kHidCount || bound_[id].exchange(true)) return false;
        const auto previous = first_[hid].load(std::memory_order_relaxed);
        next_[id].store(previous, std::memory_order_relaxed);
        first_[hid].store(id, std::memory_order_release);
        return true;
    }
    bool Publish(std::uint8_t id, std::uint16_t milli, std::uint64_t now) noexcept {
        if (!id || !bound_[id].load(std::memory_order_relaxed)) return false;
        milli = std::min<std::uint16_t>(milli, 1000);
        const auto old = values_[id].exchange(milli, std::memory_order_acq_rel);
        stamps_[id].store(now, std::memory_order_release);
        return old != milli;
    }
    bool Owns(std::uint16_t hid) const noexcept {
        return hid && hid<kHidCount && first_[hid].load(std::memory_order_acquire)!=0;
    }
    Value Read(std::uint16_t hid, std::uint64_t now, std::uint64_t freshMs = 500) const noexcept {
        Value value{};
        if (!hid || hid >= kHidCount) return value;
        auto id = first_[hid].load(std::memory_order_acquire);
        for (unsigned visits = 0; id && visits < 255; ++visits) {
            const auto stamp = stamps_[id].load(std::memory_order_acquire);
            if (stamp && now >= stamp && now - stamp <= freshMs) {
                value.fresh = true;
                value.milli = std::max(value.milli, values_[id].load(std::memory_order_acquire));
            }
            id = next_[id].load(std::memory_order_relaxed);
        }
        return value;
    }
    unsigned Active(std::uint64_t now) const noexcept {
        unsigned count = 0;
        for (unsigned id = 1; id < 256; ++id) {
            const auto stamp = stamps_[id].load(std::memory_order_acquire);
            if (stamp && now >= stamp && now - stamp <= 500 && values_[id].load(std::memory_order_relaxed)) ++count;
        }
        return count;
    }
private:
    std::array<std::atomic<std::uint16_t>, 256> values_{};
    std::array<std::atomic<std::uint64_t>, 256> stamps_{};
    std::array<std::atomic<std::uint8_t>, kHidCount> first_{};
    std::array<std::atomic<std::uint8_t>, 256> next_{};
    std::array<std::atomic<bool>, 256> bound_{};
};
} // namespace halljoy::physical_analog
