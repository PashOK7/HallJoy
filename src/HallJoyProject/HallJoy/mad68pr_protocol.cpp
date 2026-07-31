#include "mad68pr_protocol.h"

#include <algorithm>

namespace mad68pr
{
std::array<std::uint8_t, kPayloadBytes> MakeZeroPayloadRequest(
    std::uint8_t opcode,
    std::uint8_t framingHeader,
    std::uint8_t xorKey) noexcept
{
    std::array<std::uint8_t, kPayloadBytes> packet{};
    packet[0] = framingHeader;
    packet[1] = opcode;

    if (framingHeader == kNormalRequestHeader)
    {
        packet[2] = xorKey;
        packet[3] = 0;
        if (xorKey != 0)
        {
            for (std::size_t i = 3; i <= 7; ++i)
                packet[i] ^= xorKey;
        }
    }
    return packet;
}

int KeyIndexFromHid(std::uint16_t hid) noexcept
{
    if (hid == 0) return -1;
    for (std::size_t i = 0; i < kKeyDescriptors.size(); ++i)
        if (kKeyDescriptors[i].hid == hid) return static_cast<int>(i);
    return -1;
}

int KeyIndexFromDescriptor(const std::uint8_t* descriptor3) noexcept
{
    if (!descriptor3) return -1;
    for (std::size_t i = 0; i < kKeyDescriptors.size(); ++i)
    {
        const auto& d = kKeyDescriptors[i].bytes;
        if (descriptor3[0] == d[0] && descriptor3[1] == d[1] && descriptor3[2] == d[2])
            return static_cast<int>(i);
    }
    return -1;
}

const char* KeyName(std::size_t index) noexcept
{
    return index < kKeyDescriptors.size() ? kKeyDescriptors[index].name : "?";
}

bool IsPublishedHid(std::uint16_t hid) noexcept
{
    return KeyIndexFromHid(hid) >= 0;
}

bool IsWasdHid(std::uint16_t hid) noexcept
{
    return hid == 0x1A || hid == 0x04 || hid == 0x16 || hid == 0x07;
}

bool AnalogTransitionMatchesDigital(
    bool expectedDown,
    std::uint16_t threshold,
    std::uint16_t rawAtEvent,
    std::uint16_t currentRaw) noexcept
{
    const std::uint16_t pressFloor = std::max<std::uint16_t>(16, threshold / 4);
    const std::uint16_t releaseCeiling = std::max<std::uint16_t>(32, threshold / 2);
    if (expectedDown)
    {
        const std::uint16_t changedFloor = static_cast<std::uint16_t>(
            std::min<unsigned>(kAnalogFullScale, static_cast<unsigned>(rawAtEvent) + 8u));
        return currentRaw >= pressFloor || currentRaw >= changedFloor;
    }
    return currentRaw <= releaseCeiling ||
        static_cast<unsigned>(currentRaw) + 8u <= rawAtEvent;
}

bool IsPostSweepAnalogProof(
    std::uint16_t previousRaw,
    std::uint16_t currentRaw,
    std::uint16_t threshold) noexcept
{
    if (threshold == 0 || threshold > kAnalogFullScale ||
        previousRaw > kAnalogFullScale || currentRaw > kAnalogFullScale)
        return false;

    const unsigned delta = previousRaw > currentRaw
        ? static_cast<unsigned>(previousRaw - currentRaw)
        : static_cast<unsigned>(currentRaw - previousRaw);
    if (delta < kSteadyProofMinDelta) return false;

    const bool pressedAcrossThreshold = previousRaw < threshold && currentRaw >= threshold;
    const bool releasedAcrossThreshold = previousRaw >= threshold && currentRaw < threshold;
    return pressedAcrossThreshold || releasedAcrossThreshold;
}

bool DecodeKeySample(const std::uint8_t* payload, std::size_t bytes, KeySample& out) noexcept
{
    if (!payload || bytes < 20 || payload[0] != kStreamHeader) return false;

    const int keyIndex = KeyIndexFromDescriptor(payload + 1);
    if (keyIndex < 0) return false;

    const std::uint16_t raw = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(payload[4]) << 8) | payload[5]);
    if (raw > kAnalogFullScale) return false;

    const auto& descriptor = kKeyDescriptors[static_cast<std::size_t>(keyIndex)];
    out = {};
    out.keyIndex = static_cast<std::size_t>(keyIndex);
    out.scannerSlot = descriptor.scannerSlot;
    out.internalId = descriptor.internalId;
    out.hid = descriptor.hid;
    out.raw = raw;
    out.milli = static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(raw) * 1000u + kAnalogFullScale / 2u) /
        kAnalogFullScale);
    out.state = payload[10];
    out.threshold = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(payload[14]) << 8) | payload[15]);
    out.baseline = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(payload[18]) << 8) | payload[19]);
    return true;
}

ControlResponse DecodeControlResponse(
    const std::uint8_t* payload,
    std::size_t bytes,
    std::uint8_t expectedRequestHeader,
    std::uint8_t expectedOpcode) noexcept
{
    ControlResponse out{};
    if (!payload || bytes < kPayloadBytes) return out;

    out.header = payload[0];
    out.opcode = payload[1];

    if (expectedRequestHeader == kRawRequestHeader)
    {
        if (payload[0] != kRawRequestHeader) return out;
        out.kind = payload[1] == expectedOpcode
            ? ControlResponseKind::Valid
            : ControlResponseKind::Invalid;
        return out;
    }

    if (payload[0] != kNormalResponseHeader && payload[0] != kChecksumErrorHeader)
        return out;

    std::array<std::uint8_t, kPayloadBytes> decoded{};
    std::copy(payload, payload + kPayloadBytes, decoded.begin());
    out.xorKey = decoded[2];
    if (out.xorKey != 0)
    {
        for (std::size_t i = 3; i <= 7; ++i)
            decoded[i] ^= out.xorKey;
    }

    out.length = decoded[4];
    if (out.length > 0x38)
    {
        out.kind = ControlResponseKind::Invalid;
        return out;
    }

    if (out.xorKey != 0)
    {
        for (std::size_t i = 8; i < 8u + out.length; ++i)
            decoded[i] ^= out.xorKey;
    }

    std::uint32_t sum = 0;
    for (std::size_t i = 4; i < 8u + out.length; ++i)
        sum += decoded[i];
    out.checksum = decoded[3];
    out.expectedChecksum = static_cast<std::uint8_t>(sum & 0xFFu);
    out.opcode = decoded[1];

    if (payload[0] == kChecksumErrorHeader)
    {
        out.kind = ControlResponseKind::ChecksumError;
        return out;
    }
    out.kind = (out.opcode == expectedOpcode && out.checksum == out.expectedChecksum)
        ? ControlResponseKind::Valid
        : ControlResponseKind::Invalid;
    return out;
}
} // namespace mad68pr
