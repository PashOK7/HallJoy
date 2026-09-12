#pragma once
#include <array>
#include <cstdint>
#include "analog_key_codes.h"
namespace halljoy::block_keys {
// Physical keypad identity is independent of Num Lock; E0 navigation keys
// retain their normal VK identity. Shared by capture and the low-level hook.
constexpr unsigned ShortcutKey(unsigned vk, unsigned scan, bool extended) noexcept {
    if (!extended) switch (scan & 255) {
    case 0x52: return 0x60; case 0x4f: return 0x61; case 0x50: return 0x62;
    case 0x51: return 0x63; case 0x4b: return 0x64; case 0x4c: return 0x65;
    case 0x4d: return 0x66; case 0x47: return 0x67; case 0x48: return 0x68;
    case 0x49: return 0x69; case 0x53: return 0x6e;
    }
    return vk;
}
class ShortcutPress {
    unsigned held_ = 0;
    std::array<bool, halljoy::keycode::kCount> down_{};
public:
    void SeedDown(unsigned hid) noexcept { if (hid < down_.size()) down_[hid] = true; }
    // Match once per physical press, also swallowing its repeats/release even
    // if modifiers or the assigned shortcut change before release.
    bool Filter(unsigned hid, bool down, unsigned key, unsigned mods,
                unsigned chord, bool enabled, bool& toggle) noexcept {
        toggle = false;
        if (!hid || hid >= down_.size()) return false;
        const bool fresh = down && !down_[hid];
        down_[hid] = down;
        if (held_ && held_ == hid) {
            if (!down) held_ = 0;
            return true;
        }
        if (fresh && !held_ && enabled && chord && key == (chord & 255) && mods == (chord >> 8)) {
            held_ = hid; toggle = true; return true;
        }
        return false;
    }
};
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
