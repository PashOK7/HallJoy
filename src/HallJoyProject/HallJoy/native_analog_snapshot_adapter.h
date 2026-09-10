#pragma once

#include <cstddef>
#include <cstdint>

#include "analog_provider_v2.h"

namespace halljoy::native_analog_snapshot
{
// Input from one exact native HID interface.  `ownedHid` and `milli` are both
// indexed by ordinary USB HID usage and contain 256 entries.
struct LegacyMilliSourceV1
{
    std::uint64_t exactInterfaceId = 0;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint16_t usagePage = 0;
    std::uint16_t usage = 0;
    std::uint32_t protocolId = 0;
    bool connected = false;
    bool protocolProven = false;
    bool layoutProven = false;
    bool topologyComplete = false;
    const std::uint8_t* ownedHid = nullptr;
    const std::uint16_t* milli = nullptr;
};

struct OutputV1
{
    analog_provider_v2::AnalogSnapshotHeaderV2* header = nullptr;
    analog_provider_v2::AnalogDeviceV2* devices = nullptr;
    std::size_t deviceCapacity = 0;
    analog_provider_v2::AnalogSampleV2* samples = nullptr;
    std::size_t sampleCapacity = 0;
};

std::uint64_t StableProviderId(const char* asciiId) noexcept;
std::uint64_t StableDeviceId(std::uint64_t providerId,
    std::uint64_t exactInterfaceId, std::uint16_t vendorId,
    std::uint16_t productId) noexcept;

// Adapts an explicitly legacy [0..1000] source to the common V2 format.  It
// never labels the value as device-native raw precision.  The header is valid
// on success even if capacity causes a deliberately truncated publication.
bool PublishLegacyMilli(std::uint64_t providerId,
    std::uint64_t providerGeneration, std::uint64_t publicationGeneration,
    std::uint64_t publicationTimestampUs, const LegacyMilliSourceV1* sources,
    std::size_t sourceCount, OutputV1 output) noexcept;
}
