#include "hex80_protocol.h"

#include <algorithm>

namespace hex80
{
namespace
{
std::array<std::uint8_t, kPayloadBytes> BuildBase(
    std::uint8_t operation, std::uint8_t subcommand) noexcept
{
    std::array<std::uint8_t, kPayloadBytes> payload{};
    payload[0] = operation;
    payload[1] = kCustomCommand;
    payload[2] = subcommand;
    return payload;
}
}

std::array<std::uint8_t, kPayloadBytes> BuildCalibrationFinishPayload() noexcept
{
    return BuildBase(kSetValue, kCalibrationFinish);
}

std::array<std::uint8_t, kPayloadBytes> BuildTravelInfoPayload() noexcept
{
    return BuildBase(kGetValue, kTravelInfo);
}

std::array<std::uint8_t, kPayloadBytes> BuildTravelBufferPayload(
    std::uint16_t offset, std::uint8_t size) noexcept
{
    auto payload = BuildBase(kGetValue, kTravelBuffer);
    payload[5] = static_cast<std::uint8_t>((offset >> 8) & 0xFFu);
    payload[6] = static_cast<std::uint8_t>(offset & 0xFFu);
    payload[7] = size;
    return payload;
}

std::uint16_t NormalizeTravelToMilli(std::uint16_t travel, std::uint16_t travelMax) noexcept
{
    if (travel <= kRawDeadzone || travelMax <= kRawDeadzone)
        return 0;
    if (travel >= travelMax)
        return 1000;
    const std::uint32_t numerator =
        static_cast<std::uint32_t>(travel - kRawDeadzone) * 1000u;
    const std::uint32_t denominator =
        static_cast<std::uint32_t>(travelMax - kRawDeadzone);
    const std::uint32_t milli = (numerator + denominator / 2u) / denominator;
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(milli, 1000u));
}

const std::uint8_t* FindPayload(
    const std::uint8_t* data, std::size_t bytes,
    std::uint8_t operation, std::uint8_t subcommand,
    std::size_t* outPayloadBytes) noexcept
{
    if (outPayloadBytes) *outPayloadBytes = 0;
    if (!data || bytes < 3) return nullptr;

    if (data[0] == operation && data[1] == kCustomCommand && data[2] == subcommand)
    {
        if (outPayloadBytes) *outPayloadBytes = bytes;
        return data;
    }
    if (bytes >= 4 && data[0] == 0 &&
        data[1] == operation && data[2] == kCustomCommand && data[3] == subcommand)
    {
        if (outPayloadBytes) *outPayloadBytes = bytes - 1u;
        return data + 1u;
    }
    return nullptr;
}

bool DecodeTravelInfo(
    const std::uint8_t* data, std::size_t bytes,
    std::uint16_t& outTravelMax) noexcept
{
    outTravelMax = 0;
    std::size_t payloadBytes = 0;
    const auto* payload = FindPayload(data, bytes, kGetValue, kTravelInfo, &payloadBytes);
    if (!payload || payloadBytes < 5) return false;
    const std::uint16_t value = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(payload[3]) << 8) | payload[4]);
    // Reject accidental prefix collisions and obviously nonsensical scales.
    if (value < 256 || value > 20000) return false;
    outTravelMax = value;
    return true;
}

bool DecodeTravelChunk(
    const std::uint8_t* data, std::size_t bytes,
    std::uint16_t expectedOffset, std::uint8_t expectedSize,
    std::uint16_t travelMax,
    std::array<TravelEntry, kChunkSize>& outEntries,
    std::size_t& outCount) noexcept
{
    outEntries = {};
    outCount = 0;
    if (expectedSize == 0 || expectedSize > kChunkSize ||
        expectedOffset >= kTotalSlots ||
        static_cast<std::size_t>(expectedOffset) + expectedSize > kTotalSlots)
        return false;

    std::size_t payloadBytes = 0;
    const auto* payload = FindPayload(data, bytes, kGetValue, kTravelBuffer, &payloadBytes);
    if (!payload || payloadBytes < 8) return false;

    const std::uint16_t returnedOffset = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(payload[5]) << 8) | payload[6]);
    const std::uint8_t returnedSize = payload[7];
    if (returnedOffset != expectedOffset || returnedSize != expectedSize)
        return false;

    const std::size_t needed = 8u + static_cast<std::size_t>(returnedSize) * 5u;
    if (payloadBytes < needed) return false;

    const std::uint32_t plausibleLimit = std::min<std::uint32_t>(
        0xFFFFu, std::max<std::uint32_t>(travelMax, kDefaultTravelMax) * 2u);
    std::size_t cursor = 8;
    for (std::size_t index = 0; index < returnedSize; ++index, cursor += 5)
    {
        const std::uint16_t slot = static_cast<std::uint16_t>(returnedOffset + index);
        TravelEntry entry{};
        entry.slot = slot;
        entry.hid = kSlotToHid[slot];
        entry.adc = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[cursor]) << 8) | payload[cursor + 1]);
        entry.travel = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[cursor + 2]) << 8) | payload[cursor + 3]);
        entry.status = payload[cursor + 4];
        if (entry.travel > plausibleLimit) return false;
        entry.milli = NormalizeTravelToMilli(entry.travel, travelMax);
        outEntries[index] = entry;
    }
    outCount = returnedSize;
    return true;
}
}
