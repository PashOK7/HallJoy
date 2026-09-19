#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace hex80
{
constexpr std::uint16_t kVendorId = 0x373B;
constexpr std::array<std::uint16_t, 3> kKnownProductIds{{ 0x1176, 0x1177, 0x1250 }};
constexpr std::uint16_t kUsagePage = 0xFF60;
constexpr std::uint16_t kUsage = 0x0061;
constexpr std::size_t kPayloadBytes = 128;
constexpr std::size_t kMinPayloadBytes = 32;
constexpr std::size_t kTotalSlots = 104;
constexpr std::size_t kChunkSize = 4;
constexpr std::uint16_t kRawDeadzone = 8;
constexpr std::uint16_t kDefaultTravelMax = 3300;
constexpr std::uint8_t kGetValue = 0x02;
constexpr std::uint8_t kSetValue = 0x03;
constexpr std::uint8_t kCustomCommand = 0x96;
constexpr std::uint8_t kCalibrationFinish = 0x19;
constexpr std::uint8_t kTravelBuffer = 0x1C;
constexpr std::uint8_t kTravelInfo = 0x24;

struct TravelEntry
{
    std::uint16_t slot = 0;
    std::uint16_t hid = 0;
    std::uint16_t adc = 0;
    std::uint16_t travel = 0;
    std::uint8_t status = 0;
    std::uint16_t milli = 0;
};

// Source-locked official matrix: slot = position.row * 17 + position.col.
// actualPosition is the compact UI index, never the analog slot address.
// Mute has no keyboard analog HID; Fn uses HallJoy extended usage 0x409.
// Reproduction: tools/check_atk_hex80_native_map.py.
constexpr std::array<std::uint16_t, kTotalSlots> kSlotToHid{{
    // Row 0
    0x29, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x0, 0x46, 0x47, 0x48,
    // Row 1
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x49, 0x4a, 0x4b,
    // Row 2
    0x2b, 0x14, 0x1a, 0x8, 0x15, 0x17, 0x1c, 0x18, 0xc, 0x12, 0x13, 0x2f, 0x30, 0x31, 0x4c, 0x4d, 0x4e,
    // Row 3
    0x39, 0x4, 0x16, 0x7, 0x9, 0xa, 0xb, 0xd, 0xe, 0xf, 0x33, 0x34, 0x0, 0x0, 0x28, 0x0, 0x0,
    // Row 4
    0xe1, 0x1d, 0x1b, 0x6, 0x19, 0x5, 0x11, 0x10, 0x36, 0x37, 0x38, 0x0, 0xe5, 0x0, 0x0, 0x52, 0x0,
    // Row 5
    0xe0, 0xe3, 0xe2, 0x0, 0x0, 0x2c, 0x0, 0x0, 0x0, 0xe6, 0xe7, 0x409, 0x0, 0xe4, 0x50, 0x51, 0x4f,
    0, 0, // Padding of the last four-slot firmware request.
}};

constexpr std::size_t kHidCount = 0x410;
constexpr std::uint64_t kFreshMs = 500;
inline bool IsFresh(std::uint64_t stamp, std::uint64_t now) noexcept {
    return stamp && now >= stamp && now - stamp <= kFreshMs;
}

constexpr bool IsKnownProductId(std::uint16_t productId) noexcept
{
    for (const auto candidate : kKnownProductIds)
        if (candidate == productId) return true;
    return false;
}

constexpr std::size_t MappedKeyCount() noexcept
{
    std::size_t count = 0;
    for (const auto hid : kSlotToHid)
        if (hid != 0) ++count;
    return count;
}

std::array<std::uint8_t, kPayloadBytes> BuildCalibrationFinishPayload() noexcept;
std::array<std::uint8_t, kPayloadBytes> BuildTravelInfoPayload() noexcept;
std::array<std::uint8_t, kPayloadBytes> BuildTravelBufferPayload(
    std::uint16_t offset, std::uint8_t size) noexcept;

// Official driver commands occupy32 bytes; legacy reports use128 bytes.
// Pad to the descriptor length, and never discard meaningful command bytes.
bool EncodeOutputReport(const std::array<std::uint8_t, kPayloadBytes>& payload,
    std::uint8_t* report, std::size_t reportBytes) noexcept;

std::uint16_t NormalizeTravelToMilli(std::uint16_t travel, std::uint16_t travelMax) noexcept;

// Windows HID APIs normally include report ID 0 at byte 0, while hidapi-style
// captures often present the 128-byte payload directly. Both forms are accepted.
const std::uint8_t* FindPayload(
    const std::uint8_t* data, std::size_t bytes,
    std::uint8_t operation, std::uint8_t subcommand,
    std::size_t* outPayloadBytes = nullptr) noexcept;

// Match a reply to the outstanding request before consuming a late HID packet.
bool MatchesRequest(const std::array<std::uint8_t, kPayloadBytes>& request,
    const std::uint8_t* data, std::size_t bytes) noexcept;

bool DecodeTravelInfo(
    const std::uint8_t* data, std::size_t bytes,
    std::uint16_t& outTravelMax) noexcept;

bool DecodeTravelChunk(
    const std::uint8_t* data, std::size_t bytes,
    std::uint16_t expectedOffset, std::uint8_t expectedSize,
    std::uint16_t travelMax,
    std::array<TravelEntry, kChunkSize>& outEntries,
    std::size_t& outCount) noexcept;
}
