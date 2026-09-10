#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "analog_provider_v2.h"
#include "halljoy_dense_snapshot.h"
#include "halljoy_uap_provider_v2.h"

namespace halljoy::uap_parent_snapshot
{
static_assert(HallJoyDenseSnapshot::kMaxDevices ==
    HallJoyUapProviderV2::kMaxDevices);

// One immutable parent-side copy of a single analog-host publication. The
// dense compatibility plane remains usable when Provider V2 is unavailable.
// Variable Provider V2 payload ownership lives in the separate snapshot broker.
struct SnapshotV1
{
    std::uint64_t publicationGeneration = 0;
    std::uint64_t publicationTimestampUs = 0;
    std::uint64_t providerV2PlaneGeneration = 0;
    std::uint64_t providerV2PlaneTransactionToken = 0;
    std::uint32_t denseActiveKeyCount = 0;
    std::uint32_t denseDeviceCount = 0;
    bool providerV2PlaneDualCoherent = false;
    std::array<float, HallJoyDenseSnapshot::kKeyCount> denseValues{};
    std::array<HallJoyDenseSnapshot::DeviceV1,
        HallJoyDenseSnapshot::kMaxDevices> denseDevices{};
};

enum class ValidationError : std::uint32_t
{
    None = 0,
    InvalidPublication,
    InvalidDenseCount,
    InvalidDenseDevice,
    InvalidDenseValue,
    DenseActiveCountMismatch,
    DenseAggregateMismatch,
    InvalidProviderV2,
    DualViewMismatch,
};

ValidationError ValidateDualViews(
    const analog_provider_v2::AnalogSnapshotHeaderV2& header,
    const analog_provider_v2::AnalogDeviceV2* devices,
    std::size_t deviceCapacity,
    const analog_provider_v2::AnalogSampleV2* samples,
    std::size_t sampleCapacity,
    const HallJoyDenseSnapshot::DeviceV1* denseDevices,
    std::size_t denseDeviceCount) noexcept;

ValidationError Validate(const SnapshotV1& snapshot) noexcept;
}
