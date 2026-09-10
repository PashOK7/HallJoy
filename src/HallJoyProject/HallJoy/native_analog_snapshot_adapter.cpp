#include "native_analog_snapshot_adapter.h"

#include <algorithm>

namespace halljoy::native_analog_snapshot
{
namespace
{
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

void AddByte(std::uint64_t* hash, std::uint8_t value) noexcept
{
    *hash ^= value;
    *hash *= kFnvPrime;
}

void AddU64(std::uint64_t* hash, std::uint64_t value) noexcept
{
    for (unsigned i = 0; i < 8; ++i)
        AddByte(hash, static_cast<std::uint8_t>(value >> (i * 8u)));
}

void AddU16(std::uint64_t* hash, std::uint16_t value) noexcept
{
    AddByte(hash, static_cast<std::uint8_t>(value));
    AddByte(hash, static_cast<std::uint8_t>(value >> 8u));
}
}

std::uint64_t StableProviderId(const char* asciiId) noexcept
{
    if (!asciiId || !*asciiId)
        return 0;
    std::uint64_t hash = kFnvOffset;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(asciiId); *p; ++p)
        AddByte(&hash, *p);
    return hash == 0 ? 1 : hash;
}

std::uint64_t StableDeviceId(std::uint64_t providerId,
    std::uint64_t exactInterfaceId, std::uint16_t vendorId,
    std::uint16_t productId) noexcept
{
    if (providerId == 0 || exactInterfaceId == 0 || vendorId == 0 || productId == 0)
        return 0;
    std::uint64_t hash = kFnvOffset;
    AddU64(&hash, providerId);
    AddU64(&hash, exactInterfaceId);
    AddU16(&hash, vendorId);
    AddU16(&hash, productId);
    return hash == 0 ? 1 : hash;
}

bool PublishLegacyMilli(std::uint64_t providerId,
    std::uint64_t providerGeneration, std::uint64_t publicationGeneration,
    std::uint64_t publicationTimestampUs, const LegacyMilliSourceV1* sources,
    std::size_t sourceCount, OutputV1 output) noexcept
{
    using namespace analog_provider_v2;
    if (!output.header || providerId == 0 || providerGeneration == 0 ||
        publicationGeneration == 0 || publicationTimestampUs == 0 ||
        (sourceCount != 0 && !sources))
        return false;

    std::uint32_t requiredDevices = 0;
    std::uint32_t requiredSamples = 0;
    bool topologyComplete = true;
    for (std::size_t i = 0; i < sourceCount; ++i)
    {
        const auto& source = sources[i];
        if (!source.connected)
            continue;
        if (StableDeviceId(providerId, source.exactInterfaceId,
                source.vendorId, source.productId) == 0 ||
            !source.ownedHid || !source.milli)
            return false;
        ++requiredDevices;
        topologyComplete = topologyComplete && source.topologyComplete;
        for (std::size_t hid = 1; hid < 256; ++hid)
            requiredSamples += source.ownedHid[hid] != 0 ? 1u : 0u;
    }

    const bool truncated = requiredDevices > output.deviceCapacity ||
        requiredSamples > output.sampleCapacity;
    const std::uint32_t deviceCount = static_cast<std::uint32_t>(
        std::min<std::size_t>(requiredDevices, output.deviceCapacity));
    if (deviceCount != 0 && !output.devices)
        return false;
    if (output.sampleCapacity != 0 && !output.samples)
        return false;

    std::uint32_t writtenDevices = 0;
    std::uint32_t writtenSamples = 0;
    for (std::size_t sourceIndex = 0;
        sourceIndex < sourceCount && writtenDevices < deviceCount; ++sourceIndex)
    {
        const auto& source = sources[sourceIndex];
        if (!source.connected)
            continue;

        auto& device = output.devices[writtenDevices];
        device = AnalogDeviceV2{};
        device.deviceId = StableDeviceId(providerId, source.exactInterfaceId,
            source.vendorId, source.productId);
        device.exactInterfaceId = source.exactInterfaceId;
        device.flags = AnalogDeviceFlag_Connected |
            AnalogDeviceFlag_StableIdentity |
            AnalogDeviceFlag_ExactInterfaceAssigned;
        if (source.protocolProven)
            device.flags |= AnalogDeviceFlag_ProtocolProven;
        if (source.layoutProven)
            device.flags |= AnalogDeviceFlag_LayoutProven;
        device.protocolId = source.protocolId;
        device.vendorId = source.vendorId;
        device.productId = source.productId;
        device.usagePage = source.usagePage;
        device.usage = source.usage;

        for (std::size_t hid = 1; hid < 256 && writtenSamples < output.sampleCapacity; ++hid)
        {
            if (source.ownedHid[hid] == 0)
                continue;
            auto& sample = output.samples[writtenSamples++];
            sample = AnalogSampleV2{};
            sample.key = UsbHidKey(0x07u, static_cast<std::uint32_t>(hid));
            sample.deviceIndex = writtenDevices;
            sample.flags = AnalogSampleFlag_Owned | AnalogSampleFlag_ValueValid |
                AnalogSampleFlag_Fresh;
            sample.value = ValueFromLegacyMilli(source.milli[hid]);
        }
        ++writtenDevices;
    }

    auto& header = *output.header;
    header = AnalogSnapshotHeaderV2{};
    header.providerId = providerId;
    header.providerGeneration = providerGeneration;
    header.sampleGeneration = publicationGeneration;
    header.valueGeneration = publicationGeneration;
    header.ownershipGeneration = publicationGeneration;
    header.sampleTimestampUs = publicationTimestampUs;
    header.valueTimestampUs = publicationTimestampUs;
    header.deviceCount = writtenDevices;
    header.deviceCapacity = static_cast<std::uint32_t>(output.deviceCapacity);
    header.requiredDeviceCount = requiredDevices;
    header.sampleCount = writtenSamples;
    header.sampleCapacity = static_cast<std::uint32_t>(output.sampleCapacity);
    header.requiredSampleCount = requiredSamples;
    if (truncated)
        header.flags |= AnalogSnapshotFlag_Truncated;
    else if (topologyComplete)
        header.flags |= AnalogSnapshotFlag_Complete;
    return ValidateSnapshot(header, output.devices, output.deviceCapacity,
        output.samples, output.sampleCapacity) == SnapshotValidationError::None;
}
}
