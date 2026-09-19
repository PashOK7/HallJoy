#pragma once
#include "physical_analog_state.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <atomic>

// IPI BY MagneticSwitch frames include report ID09. Payload starts at byte7.
// Only documented reads are constructed here; no calibration writes.
namespace ipi {
using Frame = std::array<std::uint8_t, 64>;
constexpr std::size_t kBatchKeys = 9;
constexpr std::size_t kHidCount = 0x410;
struct Calibration { std::uint16_t released = 0, bottom = 0; };
struct Sample { std::uint8_t id = 0; std::uint16_t raw = 0, secondary = 0; bool pressed = false; };
inline std::uint16_t BE16(const std::uint8_t* p) noexcept { return (std::uint16_t(p[0]) << 8) | p[1]; }
inline std::uint32_t BE32(const std::uint8_t* p) noexcept {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3];
}
inline void Finish(Frame& f) noexcept {
    unsigned sum = 0; for (std::size_t i = 0; i < 63; ++i) sum += f[i];
    f[63] = static_cast<std::uint8_t>(0xFFu - sum);
}
inline bool Header(const Frame& f, std::uint8_t cmd, std::uint8_t sub, std::size_t length) noexcept {
    unsigned sum = 0; for (auto b : f) sum += b;
    return (sum & 255) == 255 && f[0] == 9 && f[1] == cmd && f[2] == sub &&
        f[3] == 0 && f[4] == 1 && f[5] == 0 && f[6] == length && length <= 54;
}
inline Frame UuidRequest() noexcept {
    Frame f{}; f[0] = 9; f[1] = 0x82; f[2] = 1; f[4] = 1; f[6] = 6; Finish(f); return f;
}
inline std::uint64_t ParseUuid(const Frame& f) noexcept {
    if (!Header(f, 0x82, 1, 6)) return 0;
    std::uint64_t value = 0; for (std::size_t i = 7; i < 13; ++i) value = (value << 8) | f[i];
    return value;
}
inline bool Request(std::uint8_t cmd, std::uint8_t sub, const std::uint8_t* ids,
                    std::size_t count, Frame& f) noexcept {
    if (!ids || !count || count > kBatchKeys ||
        !((cmd == 0x83 && sub == 0) || (cmd == 0x94 && (sub == 2 || sub == 5)))) return false;
    std::array<bool, 256> seen{}; Frame next{};
    next[0] = 9; next[1] = cmd; next[2] = sub; next[4] = 1; next[6] = std::uint8_t(count * 2);
    for (std::size_t i = 0; i < count; ++i) {
        if (!ids[i] || seen[ids[i]]) return false;
        seen[ids[i]] = true; next[8 + i * 2] = ids[i];
    }
    Finish(next); f = next; return true;
}
inline bool Records(const Frame& f, std::uint8_t cmd, std::uint8_t sub,
                    const std::uint8_t* ids, std::size_t count) noexcept {
    if (!ids || !count || count > kBatchKeys || !Header(f, cmd, sub, count * 6)) return false;
    std::array<bool, 256> expected{}, seen{};
    for (std::size_t i = 0; i < count; ++i) {
        if (!ids[i] || expected[ids[i]]) return false;
        expected[ids[i]] = true;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto id = BE16(f.data() + 7 + i * 6);
        if (!id || id > 255 || !expected[id] || seen[id]) return false;
        seen[id] = true;
    }
    return true;
}
inline std::uint16_t Hid(std::uint32_t keycode) noexcept {
    if (keycode < 256) return static_cast<std::uint16_t>(keycode);
    if (keycode == 0x0D000000u) return 0x409; // Vendor Fn physical binding.
    const auto bits = keycode >> 16;
    if ((keycode & 0xFFFFu) || !bits || bits > 128 || (bits & (bits - 1))) return 0;
    for (unsigned i = 0; i < 8; ++i) if (bits == (1u << i)) return std::uint16_t(224 + i);
    return 0;
}
inline bool Valid(Calibration c) noexcept {
    return c.bottom > 0 && c.released <= 0x7FFF && unsigned(c.released) > unsigned(c.bottom) + 256;
}
inline bool Map(const Frame& f, const std::uint8_t* ids, std::size_t count,
                std::array<std::uint16_t, 256>& out) noexcept {
    if (!Records(f, 0x83, 0, ids, count)) return false;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* p = f.data() + 7 + i * 6; out[BE16(p)] = Hid(BE32(p + 2));
    }
    return true;
}
inline bool Calibrations(const Frame& f, const std::uint8_t* ids, std::size_t count,
                         std::array<Calibration, 256>& out) noexcept {
    if (!Records(f, 0x94, 5, ids, count)) return false;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* p = f.data() + 7 + i * 6;
        if (!Valid({BE16(p + 2), BE16(p + 4)})) return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto* p = f.data() + 7 + i * 6; out[BE16(p)] = {BE16(p + 2), BE16(p + 4)};
    }
    return true;
}
inline bool Samples(const Frame& f, const std::uint8_t* ids, std::size_t count,
                    std::array<Sample, kBatchKeys>& out) noexcept {
    if (!Records(f, 0x94, 2, ids, count)) return false;
    auto next = out;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* p = f.data() + 7 + i * 6; const auto value = BE16(p + 2);
        const auto raw = std::uint16_t(value & 0x7FFF);
        if (!raw) return false;
        next[i] = {std::uint8_t(BE16(p)), raw, BE16(p + 4), (value & 0x8000) != 0};
    }
    out = next; return true;
}
inline std::uint16_t Normalise(std::uint16_t raw, Calibration c) noexcept {
    if (!raw || !Valid(c) || raw >= c.released) return 0;
    if (raw <= c.bottom) return 1000;
    const unsigned span = c.released - c.bottom;
    const auto value = std::uint16_t(((c.released - raw) * 1000u + span / 2u) / span);
    return value < 8 ? 0 : value;
}
using Publication = halljoy::physical_analog::Publication;
} // namespace ipi
