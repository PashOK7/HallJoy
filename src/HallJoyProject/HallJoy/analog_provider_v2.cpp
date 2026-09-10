#include "analog_provider_v2.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace halljoy::analog_provider_v2
{
namespace
{
constexpr std::uint32_t kKnownValueFlags =
    AnalogValueFlag_RawValid | AnalogValueFlag_LegacyQuantized;
constexpr std::uint32_t kKnownDeviceFlags =
    AnalogDeviceFlag_Connected |
    AnalogDeviceFlag_StableIdentity |
    AnalogDeviceFlag_ExactInterfaceAssigned |
    AnalogDeviceFlag_ProtocolProven |
    AnalogDeviceFlag_LayoutProven;
constexpr std::uint32_t kKnownSampleFlags =
    AnalogSampleFlag_Owned | AnalogSampleFlag_ValueValid |
    AnalogSampleFlag_Fresh;
constexpr std::uint32_t kKnownSnapshotFlags =
    AnalogSnapshotFlag_Complete | AnalogSnapshotFlag_Truncated;

bool ValueIsValid(const AnalogValueV2& value) noexcept
{
    if ((value.flags & ~kKnownValueFlags) != 0 ||
        !std::isfinite(value.normalized) ||
        value.normalized < 0.0f || value.normalized > 1.0f)
        return false;

    if ((value.flags & AnalogValueFlag_RawValid) != 0)
        return value.rawDomain != 0 && value.rawNumerator <= value.rawDomain;
    return value.rawNumerator == 0 && value.rawDomain == 0 &&
        (value.flags & AnalogValueFlag_LegacyQuantized) == 0;
}
}

AnalogValueV2 ValueFromRaw(std::uint32_t numerator, std::uint32_t domain) noexcept
{
    if (domain == 0 || numerator > domain)
        return {};
    AnalogValueV2 value{};
    value.normalized = static_cast<float>(
        static_cast<double>(numerator) / static_cast<double>(domain));
    value.rawNumerator = numerator;
    value.rawDomain = domain;
    value.flags = AnalogValueFlag_RawValid;
    return value;
}

AnalogValueV2 ValueFromLegacyMilli(std::uint16_t milli) noexcept
{
    const std::uint32_t bounded = std::min<std::uint32_t>(milli, 1000u);
    AnalogValueV2 value = ValueFromRaw(bounded, 1000u);
    value.flags |= AnalogValueFlag_LegacyQuantized;
    return value;
}

SnapshotValidationError ValidateSnapshot(
    const AnalogSnapshotHeaderV2& header,
    const AnalogDeviceV2* devices,
    std::size_t deviceStorageCount,
    const AnalogSampleV2* samples,
    std::size_t sampleStorageCount) noexcept
{
    if (header.schemaVersion != kSnapshotSchemaVersion ||
        header.structSize < sizeof(AnalogSnapshotHeaderV2) ||
        (header.flags & ~kKnownSnapshotFlags) != 0 || header.providerId == 0)
        return SnapshotValidationError::InvalidHeader;
    if (header.providerGeneration == 0 || header.sampleGeneration == 0 ||
        header.valueGeneration == 0 || header.ownershipGeneration == 0)
        return SnapshotValidationError::InvalidGeneration;
    if (header.deviceCount > header.deviceCapacity ||
        header.sampleCount > header.sampleCapacity ||
        header.requiredDeviceCount < header.deviceCount ||
        header.requiredSampleCount < header.sampleCount)
        return SnapshotValidationError::InvalidCapacity;

    const bool complete = (header.flags & AnalogSnapshotFlag_Complete) != 0;
    const bool truncated = (header.flags & AnalogSnapshotFlag_Truncated) != 0;
    const bool insufficient =
        header.requiredDeviceCount > header.deviceCapacity ||
        header.requiredSampleCount > header.sampleCapacity;
    if ((complete && truncated) || (truncated != insufficient) ||
        (!truncated && (header.requiredDeviceCount != header.deviceCount ||
            header.requiredSampleCount != header.sampleCount)))
        return SnapshotValidationError::InvalidCompleteness;

    if (header.deviceCount > deviceStorageCount ||
        header.sampleCount > sampleStorageCount ||
        (header.deviceCount != 0 && devices == nullptr) ||
        (header.sampleCount != 0 && samples == nullptr))
        return SnapshotValidationError::MissingStorage;

    for (std::size_t i = 0; i < header.deviceCount; ++i)
    {
        const AnalogDeviceV2& device = devices[i];
        if (device.schemaVersion != kDeviceSchemaVersion ||
            device.structSize < sizeof(AnalogDeviceV2) || device.deviceId == 0 ||
            (device.flags & ~kKnownDeviceFlags) != 0)
            return SnapshotValidationError::InvalidDevice;
        for (std::size_t earlier = 0; earlier < i; ++earlier)
        {
            if (devices[earlier].deviceId == device.deviceId)
                return SnapshotValidationError::DuplicateDevice;
        }
    }

    for (std::size_t i = 0; i < header.sampleCount; ++i)
    {
        const AnalogSampleV2& sample = samples[i];
        if (sample.schemaVersion != kSampleSchemaVersion ||
            sample.structSize < sizeof(AnalogSampleV2) ||
            sample.deviceIndex >= header.deviceCount || !IsValid(sample.key) ||
            (sample.flags & ~kKnownSampleFlags) != 0 ||
            (sample.flags & (AnalogSampleFlag_Owned |
                AnalogSampleFlag_ValueValid | AnalogSampleFlag_Fresh)) !=
                (AnalogSampleFlag_Owned |
                    AnalogSampleFlag_ValueValid | AnalogSampleFlag_Fresh) ||
            !ValueIsValid(sample.value))
            return SnapshotValidationError::InvalidSample;

        for (std::size_t earlier = 0; earlier < i; ++earlier)
        {
            if (samples[earlier].deviceIndex == sample.deviceIndex &&
                samples[earlier].key == sample.key)
                return SnapshotValidationError::DuplicateSample;
        }
    }
    return SnapshotValidationError::None;
}

SnapshotTransitionError ValidateTransition(
    const AnalogSnapshotHeaderV2& previous,
    const AnalogSnapshotHeaderV2& next) noexcept
{
    if (previous.providerId != next.providerId)
        return SnapshotTransitionError::ProviderChanged;
    if (next.providerGeneration < previous.providerGeneration)
        return SnapshotTransitionError::ProviderGenerationRegressed;
    if (next.providerGeneration != previous.providerGeneration)
        return SnapshotTransitionError::None;
    if (next.sampleGeneration < previous.sampleGeneration ||
        next.valueGeneration < previous.valueGeneration ||
        next.ownershipGeneration < previous.ownershipGeneration)
        return SnapshotTransitionError::GenerationRegressed;
    if (next.sampleTimestampUs < previous.sampleTimestampUs ||
        next.valueTimestampUs < previous.valueTimestampUs)
        return SnapshotTransitionError::TimestampRegressed;
    return SnapshotTransitionError::None;
}

bool RequiresRealtimeWake(
    const AnalogSnapshotHeaderV2& previous,
    const AnalogSnapshotHeaderV2& next) noexcept
{
    return previous.providerId != next.providerId ||
        previous.providerGeneration != next.providerGeneration ||
        previous.valueGeneration != next.valueGeneration ||
        previous.ownershipGeneration != next.ownershipGeneration;
}
}
