#pragma once
#include <array>
#include <cstdint>
#include "analog_key_codes.h"
namespace halljoy::block_keys {
constexpr bool IsAltOrTab(unsigned hid) noexcept { return hid == 43 || hid == 226 || hid == 230; }
constexpr bool ValidShortcut(unsigned chord) noexcept {
    if (!chord) return true;
    const unsigned vk = chord & 255, mods = chord >> 8;
    return mods <= 15 && vk >= 8 && vk < 255 && vk != 16 && vk != 17 && vk != 18 &&
        vk != 91 && vk != 92 && (vk < 160 || vk > 165);
}
// The first down decides the whole press, including repeats and its release.
// Changing settings/focus mid-press must never swallow an already delivered up.
class PressRoutes {
    std::array<std::uint8_t, halljoy::keycode::kCount> routes_{};
    unsigned held_ = 0;
public:
    bool HasHeld() const noexcept { return held_ != 0; }
    void Reset() noexcept { routes_.fill(0); held_ = 0; }
    void SeedPassed(unsigned hid) noexcept {
        if (hid && hid < routes_.size() && !routes_[hid]) { routes_[hid] = 1; ++held_; }
    }
    bool Filter(unsigned hid, bool down, bool block) noexcept {
        if (!hid || hid >= routes_.size()) return false;
        auto& route = routes_[hid];
        if (down && !route) { route = block ? 2 : 1; ++held_; }
        const bool swallow = route == 2;
        if (!down && route) { route = 0; --held_; }
        return swallow;
    }
};
}
