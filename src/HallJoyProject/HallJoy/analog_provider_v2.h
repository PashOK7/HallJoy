#pragma once

#include <cstddef>
#include "halljoy_analog_provider_v2_contract.h"

namespace halljoy::analog_provider_v2
{
AnalogValueV2 ValueFromRaw(std::uint32_t numerator, std::uint32_t domain) noexcept;
AnalogValueV2 ValueFromLegacyMilli(std::uint16_t milli) noexcept;

enum class SnapshotValidationError : std::uint32_t
{
    None = 0,
    InvalidHeader,
    InvalidGeneration,
    InvalidCapacity,
    InvalidCompleteness,
    MissingStorage,
    InvalidDevice,
    DuplicateDevice,
    InvalidSample,
    DuplicateSample,
};

SnapshotValidationError ValidateSnapshot(
    const AnalogSnapshotHeaderV2& header,
    const AnalogDeviceV2* devices,
    std::size_t deviceStorageCount,
    const AnalogSampleV2* samples,
    std::size_t sampleStorageCount) noexcept;

enum class SnapshotTransitionError : std::uint32_t
{
    None = 0,
    ProviderChanged,
    ProviderGenerationRegressed,
    GenerationRegressed,
    TimestampRegressed,
};

SnapshotTransitionError ValidateTransition(
    const AnalogSnapshotHeaderV2& previous,
    const AnalogSnapshotHeaderV2& next) noexcept;

bool RequiresRealtimeWake(
    const AnalogSnapshotHeaderV2& previous,
    const AnalogSnapshotHeaderV2& next) noexcept;

constexpr bool IsAuthoritative(const AnalogSnapshotHeaderV2& header) noexcept
{
    return (header.flags & AnalogSnapshotFlag_Complete) != 0 &&
        (header.flags & AnalogSnapshotFlag_Truncated) == 0;
}
}
