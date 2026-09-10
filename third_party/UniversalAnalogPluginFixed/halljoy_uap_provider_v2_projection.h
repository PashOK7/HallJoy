#pragma once

#include "halljoy_uap_provider_v2.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace HallJoyUapProviderV2
{
using halljoy::analog_provider_v2::AnalogDeviceV2;
using halljoy::analog_provider_v2::AnalogSampleV2;
using halljoy::analog_provider_v2::AnalogSnapshotHeaderV2;
using halljoy::analog_provider_v2::KeyIdentityV1;

struct ProjectionKey final
{
    KeyIdentityV1 identity{};
    std::uint32_t sourceIndex = 0;
};

struct ProjectionDevice final
{
    AnalogDeviceV2 descriptor{};
    const float* values = nullptr;
    std::size_t valueCount = 0;
    std::uint64_t sampleTimestampUs = 0;
};

struct ProjectionGeneration final
{
    std::uint64_t provider = 0;
    std::uint64_t sample = 0;
    std::uint64_t value = 0;
    std::uint64_t ownership = 0;
    std::uint64_t valueTimestampUs = 0;
    std::uint64_t fallbackSampleTimestampUs = 0;
};

enum class ProjectionError : std::uint32_t
{
    None = 0,
    InvalidArgument,
    InvalidIdentity,
    DuplicateIdentity,
    InvalidDevice,
    InvalidValue,
    CapacityOverflow,
};

inline ProjectionError BuildSnapshot(
    const ProjectionKey* keys,
    std::size_t keyCount,
    const ProjectionDevice* availableDevices,
    std::size_t availableDeviceCount,
    std::size_t requiredDeviceCount,
    const ProjectionGeneration& generation,
    AnalogSnapshotHeaderV2* header,
    AnalogDeviceV2* deviceBuffer,
    std::size_t deviceCapacity,
    AnalogSampleV2* sampleBuffer,
    std::size_t sampleCapacity) noexcept
{
    using namespace halljoy::analog_provider_v2;
    if (!header || (keyCount != 0 && !keys) ||
        (availableDeviceCount != 0 && !availableDevices) ||
        (deviceCapacity != 0 && !deviceBuffer) ||
        (sampleCapacity != 0 && !sampleBuffer) ||
        requiredDeviceCount < availableDeviceCount ||
        generation.provider == 0 || generation.sample == 0 ||
        generation.value == 0 || generation.ownership == 0)
    {
        return ProjectionError::InvalidArgument;
    }
    if (requiredDeviceCount > std::numeric_limits<std::uint32_t>::max() ||
        keyCount > std::numeric_limits<std::uint32_t>::max() ||
        deviceCapacity > std::numeric_limits<std::uint32_t>::max() ||
        sampleCapacity > std::numeric_limits<std::uint32_t>::max() ||
        (keyCount != 0 && requiredDeviceCount >
            std::numeric_limits<std::uint32_t>::max() / keyCount))
    {
        return ProjectionError::CapacityOverflow;
    }

    for (std::size_t i = 0; i < keyCount; ++i)
    {
        if (!IsValid(keys[i].identity))
            return ProjectionError::InvalidIdentity;
        for (std::size_t earlier = 0; earlier < i; ++earlier)
        {
            if (keys[earlier].identity == keys[i].identity)
                return ProjectionError::DuplicateIdentity;
        }
    }
    for (std::size_t i = 0; i < availableDeviceCount; ++i)
    {
        const auto& device = availableDevices[i];
        if (device.descriptor.deviceId == 0 ||
            (device.valueCount != 0 && !device.values))
        {
            return ProjectionError::InvalidDevice;
        }
        for (std::size_t earlier = 0; earlier < i; ++earlier)
        {
            if (availableDevices[earlier].descriptor.deviceId ==
                device.descriptor.deviceId)
            {
                return ProjectionError::InvalidDevice;
            }
        }
        for (std::size_t key = 0; key < keyCount; ++key)
        {
            if (keys[key].sourceIndex >= device.valueCount ||
                !std::isfinite(device.values[keys[key].sourceIndex]))
            {
                return ProjectionError::InvalidValue;
            }
        }
    }

    // Report the capacity of this captured generation, not merely the size of
    // the caller's buffers.  availableDeviceCount may be lower than
    // requiredDeviceCount when the registry exceeded the pinned transport
    // window.  Advertising the larger caller capacity in that case would make
    // a genuinely truncated snapshot internally inconsistent.
    const std::size_t effectiveDeviceCapacity =
        (std::min)(availableDeviceCount, deviceCapacity);
    const std::size_t copiedDevices = effectiveDeviceCapacity;
    const std::size_t availableSampleCapacity = copiedDevices * keyCount;
    const std::size_t effectiveSampleCapacity =
        (std::min)(availableSampleCapacity, sampleCapacity);
    std::size_t copiedSamples = 0;
    std::uint64_t sampleTimestampUs = generation.fallbackSampleTimestampUs;
    for (std::size_t deviceIndex = 0; deviceIndex < copiedDevices; ++deviceIndex)
    {
        const auto& source = availableDevices[deviceIndex];
        deviceBuffer[deviceIndex] = source.descriptor;
        sampleTimestampUs = (std::max)(sampleTimestampUs,
            source.sampleTimestampUs);
        for (std::size_t key = 0; key < keyCount; ++key)
        {
            if (copiedSamples == effectiveSampleCapacity)
                break;
            auto& output = sampleBuffer[copiedSamples++];
            output = AnalogSampleV2{};
            output.key = keys[key].identity;
            output.deviceIndex = static_cast<std::uint32_t>(deviceIndex);
            output.flags = AnalogSampleFlag_Owned |
                AnalogSampleFlag_ValueValid | AnalogSampleFlag_Fresh;
            output.value.normalized = std::clamp(
                source.values[keys[key].sourceIndex], 0.0f, 1.0f);
        }
    }

    const std::size_t requiredSamples = requiredDeviceCount * keyCount;
    *header = AnalogSnapshotHeaderV2{};
    header->providerId = kProviderId;
    header->providerGeneration = generation.provider;
    header->sampleGeneration = generation.sample;
    header->valueGeneration = generation.value;
    header->ownershipGeneration = generation.ownership;
    header->sampleTimestampUs = sampleTimestampUs;
    header->valueTimestampUs = generation.valueTimestampUs != 0
        ? generation.valueTimestampUs : sampleTimestampUs;
    header->deviceCount = static_cast<std::uint32_t>(copiedDevices);
    header->deviceCapacity = static_cast<std::uint32_t>(
        effectiveDeviceCapacity);
    header->requiredDeviceCount = static_cast<std::uint32_t>(requiredDeviceCount);
    header->sampleCount = static_cast<std::uint32_t>(copiedSamples);
    header->sampleCapacity = static_cast<std::uint32_t>(
        effectiveSampleCapacity);
    header->requiredSampleCount = static_cast<std::uint32_t>(requiredSamples);
    const bool complete = copiedDevices == requiredDeviceCount &&
        copiedSamples == requiredSamples;
    header->flags = complete
        ? AnalogSnapshotFlag_Complete : AnalogSnapshotFlag_Truncated;
    return ProjectionError::None;
}

inline bool ProjectOrdinaryUsbHidDense(
    const AnalogSnapshotHeaderV2& header,
    const AnalogSampleV2* samples,
    float* denseValues,
    std::size_t denseCount) noexcept
{
    using namespace halljoy::analog_provider_v2;
    const bool authoritative =
        (header.flags & AnalogSnapshotFlag_Complete) != 0 &&
        (header.flags & AnalogSnapshotFlag_Truncated) == 0 &&
        header.deviceCount == header.requiredDeviceCount &&
        header.sampleCount == header.requiredSampleCount;
    if (!authoritative || (header.sampleCount != 0 && !samples) ||
        !denseValues || denseCount == 0)
    {
        return false;
    }
    std::fill_n(denseValues, denseCount, 0.0f);
    for (std::size_t i = 0; i < header.sampleCount; ++i)
    {
        const auto& sample = samples[i];
        if (sample.key.keyNamespace != KeyNamespace::UsbHidUsage ||
            sample.key.usagePage != 0x07u || sample.key.usage >= denseCount)
        {
            continue;
        }
        denseValues[sample.key.usage] = (std::max)(
            denseValues[sample.key.usage], sample.value.normalized);
    }
    return true;
}

// Characterisation helper for one device inside a freshly built captured
// snapshot.  Unlike the merged public projection above it deliberately accepts
// a topology-truncated header: every sample for this captured device must still
// be present, and callers compare it while the same provider locks are held.
inline bool ProjectCapturedDeviceOrdinaryUsbHidDense(
    const AnalogSnapshotHeaderV2& header,
    const AnalogSampleV2* samples,
    std::uint32_t deviceIndex,
    float* denseValues,
    std::size_t denseCount) noexcept
{
    using namespace halljoy::analog_provider_v2;
    if (deviceIndex >= header.deviceCount ||
        (header.sampleCount != 0 && !samples) ||
        !denseValues || denseCount == 0)
    {
        return false;
    }
    std::fill_n(denseValues, denseCount, 0.0f);
    for (std::size_t i = 0; i < header.sampleCount; ++i)
    {
        const auto& sample = samples[i];
        if (sample.deviceIndex != deviceIndex)
            continue;
        if (sample.key.keyNamespace != KeyNamespace::UsbHidUsage ||
            sample.key.usagePage != 0x07u || sample.key.usage >= denseCount)
        {
            continue;
        }
        denseValues[sample.key.usage] = (std::max)(
            denseValues[sample.key.usage], sample.value.normalized);
    }
    return true;
}
}
