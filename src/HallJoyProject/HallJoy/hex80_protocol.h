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

constexpr std::array<std::uint16_t, kTotalSlots> kSlotToHid{{
    // Row 0
    0x29, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0,
    // Row 1
    0x35, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A, 0x49, 0, 0,
    // Row 2
    0x2B, 0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C, 0x12, 0x13, 0x2F, 0x30, 0x31, 0x4B, 0, 0,
    // Row 3
    0x39, 0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x33, 0x34, 0, 0x28, 0x4E, 0, 0,
    // Row 4
    0xE1, 0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0, 0xE5, 0, 0x52, 0, 0,
    // Row 5 (Fn at slot 95 is vendor code 0x409 and cannot be represented by HallJoy HID<256)
    0xE0, 0xE3, 0xE2, 0, 0, 0x2C, 0, 0, 0, 0xE6, 0, 0x65, 0, 0x50, 0x51, 0x4F, 0,
    // Trailing slots
    0, 0,
}};

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

std::uint16_t NormalizeTravelToMilli(std::uint16_t travel, std::uint16_t travelMax) noexcept;

// Windows HID APIs normally include report ID 0 at byte 0, while hidapi-style
// captures often present the 128-byte payload directly. Both forms are accepted.
const std::uint8_t* FindPayload(
    const std::uint8_t* data, std::size_t bytes,
    std::uint8_t operation, std::uint8_t subcommand,
    std::size_t* outPayloadBytes = nullptr) noexcept;

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
