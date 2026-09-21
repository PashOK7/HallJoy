#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include "analog_key_codes.h"

// Session-owned remap data, separate from persistent layout geometry.
// UI consumes copies; the realtime read path only reads the selected token.
namespace halljoy::native_layout {
constexpr std::size_t kMaxKeys = 256;
constexpr std::uint16_t kHidCount = static_cast<std::uint16_t>(halljoy::keycode::kCount);
struct Key {
    std::uint16_t factory = 0, assigned = 0;
    // Optional display identity, independent of the physical analog channel.
    std::uint16_t labelHid = 0;
    std::array<wchar_t,16> label{};
};
struct Snapshot {
    std::uint64_t token = 0, revision = 0;
    bool complete = false;
    std::size_t count = 0;
    std::array<Key, kMaxKeys> keys{};
};
inline std::atomic<bool> enabled{true};
inline std::atomic<std::uint64_t> activeToken{0};
inline std::mutex mutex;
inline std::array<Snapshot,16> sources{};
inline std::uint64_t revision = 0;
inline bool UsesRemapping(std::uint64_t token) noexcept {
    return token && enabled.load(std::memory_order_acquire) &&
        activeToken.load(std::memory_order_acquire)==token;
}
inline bool Publish(std::uint64_t token, const Key* keys, std::size_t count) {
    if (!token || !keys || !count || count>kMaxKeys) return false;
    Snapshot next{}; next.token=token; next.count=count; next.complete=true;
    std::array<bool,kHidCount> seen{};
    for (std::size_t i=0;i<count;++i) {
        if (!keys[i].factory || keys[i].factory>=kHidCount ||
            keys[i].assigned>=kHidCount || seen[keys[i].factory]) return false;
        if(keys[i].labelHid>=kHidCount || keys[i].label.back()!=0) return false;
        seen[keys[i].factory]=true;next.keys[i]=keys[i];
    }
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& slot:sources) if (slot.token==token) {
        next.revision=++revision; slot=next;return true;
    }
    for (auto& slot:sources) if (!slot.token) {
        next.revision=++revision; slot=next;return true;
    }
    return false;
}
inline Snapshot Read(std::uint64_t token) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& slot:sources) if (slot.token==token) return slot;
    return {};
}
inline void Clear(std::uint64_t token) noexcept {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& slot:sources) if (slot.token==token) slot={};
    if (activeToken.load()==token) activeToken.store(0,std::memory_order_release);
}
} // namespace halljoy::native_layout
