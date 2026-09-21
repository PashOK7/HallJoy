#pragma once
#include <cstdint>

// HallJoy physical aliases, not invented keyboard usages or digital depths.
// V2 records already contain matrix position and independent 10-bit travel.
// Retain the ordinary scancode channel for existing bindings and additionally
// publish these positions. The selected split layout uses only physical aliases.
namespace halljoy::wooting_physical {
inline constexpr std::uint16_t kLeftSpace = 0x480;
inline constexpr std::uint16_t kRightSpace = 0x481;
inline constexpr std::uint16_t kCenterFn = 0x482;
inline constexpr std::uint16_t kRightFn = 0x483;
constexpr bool IsCode(std::uint16_t code) noexcept {
    return code >= kLeftSpace && code <= kRightFn;
}
constexpr int Slot(std::uint16_t vendor, std::uint16_t product,
    std::uint8_t matrix) noexcept {
    // Exact manufacturer model PID. Never infer another Wooting matrix from
    // vendor ID, marketing name, remapped scancode or Windows key state.
    if (vendor != 0x31e3 || product != 0x1340) return -1;
    switch (matrix) {
    case (5u << 5u) | 4u: return 0;
    case (5u << 5u) | 8u: return 1;
    case (5u << 5u) | 6u: return 2;
    case (5u << 5u) | 13u: return 3;
    default: return -1;
    }
}
constexpr std::uint16_t Code(unsigned slot) noexcept {
    return slot < 4 ? static_cast<std::uint16_t>(kLeftSpace + slot) : 0;
}
}
