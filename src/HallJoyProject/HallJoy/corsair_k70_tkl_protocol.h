#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace halljoy::corsair_k70 {
inline constexpr std::uint16_t kVid = 0x1b1c, kPid = 0x2b14;
inline constexpr std::uint32_t kFirmware = 0x00360c01; // 1.12.54
inline constexpr std::size_t kReportBytes = 257, kMaxSensors = 67, kDataOffset = 4;
inline constexpr std::size_t kTravelSteps = 40;
using Frame = std::array<std::uint8_t, kReportBytes>;
using Samples = std::array<std::uint16_t, kMaxSensors>;

struct Layout {
    unsigned region, count;
    std::string_view product;
    Samples hid;
    std::array<std::uint8_t, kMaxSensors> curve;
};

// Marshall 1.12.54 maps sensor slots to physical keys and assigns three
// factory transfer curves. Two placeholder slots in each layout are unmapped.
inline constexpr Layout kLayouts[] = {
    {1, 63, "K70PROTKL-1B1C-2B14-ANSI",
      {
        0xe2, 0x0, 0xe3, 0xe0, 0xe1, 0x39, 0x2b, 0x35, 0x1e, 0x14, 0x4, 0x1d, 0x1b, 0x16,
        0x1a, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x8,
        0x7, 0x6, 0x9, 0x15, 0x17, 0x1c, 0x18, 0xc, 0x12, 0x13, 0x2f, 0x30, 0x31, 0xf,
        0xe, 0xd, 0xb, 0xa, 0x19, 0x5, 0x11, 0x10, 0x36, 0x37, 0x33, 0x34, 0x28, 0x38,
        0xe5, 0x0, 0xe4, 0x65, 0x409, 0xe6, 0x2c,
      },
      {
        0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 2,
      }},
    {2, 64, "K70PROTKL-1B1C-2B14-ISO",
      {
        0xe2, 0x0, 0xe3, 0xe0, 0xe1, 0x39, 0x2b, 0x35, 0x1e, 0x14, 0x4, 0x64, 0x1d, 0x1b,
        0x16, 0x1a, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a,
        0x8, 0x7, 0x6, 0x9, 0x15, 0x17, 0x1c, 0x18, 0xc, 0x12, 0x13, 0x2f, 0x30, 0xf,
        0xe, 0xd, 0xb, 0xa, 0x19, 0x5, 0x11, 0x10, 0x36, 0x37, 0x33, 0x34, 0x32, 0x28,
        0x38, 0xe5, 0x0, 0xe4, 0x65, 0x409, 0xe6, 0x2c,
      },
      {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 2,
      }},
    {4, 67, "K70PROTKL-1B1C-2B14-JIS",
      {
        0x8b, 0xe2, 0x0, 0xe3, 0xe0, 0xe1, 0x39, 0x2b, 0x35, 0x1e, 0x14, 0x4, 0x1d, 0x1b,
        0x16, 0x1a, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x89,
        0x2a, 0x8, 0x7, 0x6, 0x9, 0x15, 0x17, 0x1c, 0x18, 0xc, 0x12, 0x13, 0x2f, 0x30,
        0xf, 0xe, 0xd, 0xb, 0xa, 0x19, 0x5, 0x11, 0x10, 0x36, 0x37, 0x33, 0x34, 0x32,
        0x28, 0x38, 0x87, 0xe5, 0x0, 0xe4, 0x65, 0x409, 0x88, 0x8a, 0x2c,
      },
      {
        0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        2,
      }},
};
inline constexpr std::uint16_t kCurves[][kTravelSteps + 1] = {
    {
        0, 8, 9, 11, 15, 20, 25, 30, 35, 41, 47, 52, 60, 68,
        75, 83, 93, 100, 108, 118, 129, 139, 152, 164, 177, 192, 200, 217,
        235, 253, 270, 289, 310, 332, 352, 376, 404, 429, 462, 478, 490,
    },
    {
        0, 8, 9, 11, 16, 21, 25, 30, 35, 41, 47, 53, 60, 67,
        73, 81, 89, 98, 106, 114, 125, 137, 149, 160, 172, 186, 201, 218,
        227, 246, 264, 285, 306, 326, 351, 376, 404, 434, 452, 470, 485,
    },
    {
        0, 8, 10, 12, 17, 21, 26, 32, 37, 42, 47, 53, 60, 67,
        75, 81, 89, 98, 106, 116, 122, 134, 145, 158, 170, 182, 196, 212,
        230, 239, 258, 277, 298, 319, 341, 367, 394, 424, 441, 458, 478,
    },
};

constexpr const Layout* FindLayout(unsigned region) noexcept {
    for (const auto& layout : kLayouts)
        if (layout.region == region) return &layout;
    return nullptr;
}

enum class Property : std::uint8_t {
    Vendor = 0x11, Product = 0x12, Firmware = 0x13, Region = 0x41,
    Calibration = 0xc6, Snapshot = 0xc7,
};

constexpr Frame Request(Property property, std::uint8_t subcommand = 0) noexcept {
    Frame frame{};
    frame[1] = 0x08;
    frame[2] = 0x02;
    frame[3] = static_cast<std::uint8_t>(property);
    frame[5] = subcommand;
    return frame;
}

constexpr bool ReplyHeader(const Frame& reply) noexcept {
    return reply[0] == 0 && reply[1] == 0 && reply[2] == 2 && reply[3] == 0;
}

constexpr std::uint16_t U16(const Frame& frame, std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(frame[offset] | (unsigned(frame[offset + 1]) << 8));
}

constexpr std::uint32_t U32(const Frame& frame, std::size_t offset) noexcept {
    return U16(frame, offset) | (std::uint32_t(U16(frame, offset + 2)) << 16);
}

inline bool ReadWords(const Frame& frame, const Layout& layout, Samples& out) noexcept {
    const auto end = kDataOffset + 2 * layout.count;
    for (std::size_t i = end; i < frame.size(); ++i)
        if (frame[i]) return false;
    for (std::size_t i = 0; i < layout.count; ++i)
        out[i] = U16(frame, kDataOffset + 2 * i);
    return true;
}

struct Profile {
    const Layout* layout = nullptr;
    Samples rest{};
    Samples scale{};
};

inline unsigned BuildProfile(const Layout& layout, const Samples& rest,
                             const Samples& bottom, Profile& out) noexcept {
    Profile profile{&layout, rest};
    unsigned invalid = 0;
    for (std::size_t i = 0; i < layout.count; ++i) {
        if (!layout.hid[i]) continue;
        if (!rest[i] || bottom[i] >= 0x4000 || bottom[i] <= rest[i] ||
            bottom[i] - rest[i] < 500) {
            ++invalid;
            continue;
        }
        profile.scale[i] = static_cast<std::uint16_t>((bottom[i] - rest[i]) / 500);
    }
    if (!invalid) out = profile;
    return invalid;
}

// Match the stock depth search: a step is reached only above its threshold.
inline std::uint16_t Normalize(std::uint16_t raw, const Profile& profile, std::size_t i) noexcept {
    if (raw <= profile.rest[i]) return 0;
    const auto delta = unsigned(raw - profile.rest[i]);
    const auto& curve = kCurves[profile.layout->curve[i]];
    for (unsigned step = kTravelSteps; step; --step)
        if (delta > curve[step] * unsigned(profile.scale[i]))
            return static_cast<std::uint16_t>(step * 25);
    return 0;
}
} // namespace halljoy::corsair_k70
