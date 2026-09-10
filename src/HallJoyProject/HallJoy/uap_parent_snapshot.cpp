#include "uap_parent_snapshot.h"

#include "halljoy_uap_provider_v2_projection.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace halljoy::uap_parent_snapshot
{
namespace
{
bool ValidDenseDeviceHeader(const HallJoyDenseSnapshot::DeviceV1& dense) noexcept
{
    constexpr std::uint32_t allowedFlags =
        HallJoyDenseSnapshot::DeviceFlag_Connected |
        HallJoyDenseSnapshot::DeviceFlag_DuplicateSafeId |
        HallJoyDenseSnapshot::DeviceFlag_PolledTransport |
        HallJoyDenseSnapshot::DeviceFlag_StreamTransport;
    return dense.structSize == sizeof(HallJoyDenseSnapshot::DeviceV1) &&
        dense.version == HallJoyDenseSnapshot::kVersion &&
        dense.deviceId != 0 && dense.generation != 0 && dense.timestampUs != 0 &&
        (dense.flags & HallJoyDenseSnapshot::DeviceFlag_Connected) != 0 &&
        (dense.flags & ~allowedFlags) == 0;
}
}

ValidationError ValidateDualViews(
    const analog_provider_v2::AnalogSnapshotHeaderV2& header,
    const analog_provider_v2::AnalogDeviceV2* devices,
    std::size_t deviceCapacity,
    const analog_provider_v2::AnalogSampleV2* samples,
    std::size_t sampleCapacity,
    const HallJoyDenseSnapshot::DeviceV1* denseDevices,
    std::size_t denseDeviceCount) noexcept
{
    using namespace analog_provider_v2;
    if (!denseDevices || header.deviceCount != denseDeviceCount ||
        ValidateSnapshot(header, devices, deviceCapacity, samples,
            sampleCapacity) != SnapshotValidationError::None)
    {
        return ValidationError::InvalidProviderV2;
    }

    std::array<float, HallJoyDenseSnapshot::kKeyCount> projected{};
    for (std::uint32_t di = 0; di < header.deviceCount; ++di)
    {
        const auto& provider = devices[di];
        const auto& dense = denseDevices[di];
        if (!ValidDenseDeviceHeader(dense) ||
            dense.deviceId != provider.deviceId ||
            dense.vendorId != provider.vendorId ||
            dense.productId != provider.productId ||
            dense.usagePage != provider.usagePage || dense.usage != provider.usage ||
            !HallJoyUapProviderV2::ProjectCapturedDeviceOrdinaryUsbHidDense(
                header, samples, di, projected.data(), projected.size()))
        {
            return ValidationError::DualViewMismatch;
        }

        std::uint32_t active = 0;
        for (std::size_t code = 0; code < projected.size(); ++code)
        {
            const float value = dense.values[code];
            if (!std::isfinite(value) || value < 0.0f || value > 1.0f ||
                value != projected[code])
            {
                return ValidationError::DualViewMismatch;
            }
            if (value > 0.0f)
                ++active;
        }
        if (active != dense.activeKeyCount)
            return ValidationError::DualViewMismatch;
    }
    return ValidationError::None;
}

ValidationError Validate(const SnapshotV1& snapshot) noexcept
{
    if (snapshot.publicationGeneration == 0 ||
        snapshot.publicationTimestampUs == 0)
    {
        return ValidationError::InvalidPublication;
    }
    if (snapshot.denseDeviceCount > snapshot.denseDevices.size())
        return ValidationError::InvalidDenseCount;

    std::array<float, HallJoyDenseSnapshot::kKeyCount> merged{};
    for (std::uint32_t di = 0; di < snapshot.denseDeviceCount; ++di)
    {
        const auto& dense = snapshot.denseDevices[di];
        if (!ValidDenseDeviceHeader(dense))
            return ValidationError::InvalidDenseDevice;
        std::uint32_t active = 0;
        for (std::size_t code = 0; code < merged.size(); ++code)
        {
            const float value = dense.values[code];
            if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
                return ValidationError::InvalidDenseValue;
            merged[code] = (std::max)(merged[code], value);
            if (value > 0.0f)
                ++active;
        }
        if (active != dense.activeKeyCount)
            return ValidationError::DenseActiveCountMismatch;
    }

    std::uint32_t aggregateActive = 0;
    for (std::size_t code = 0; code < snapshot.denseValues.size(); ++code)
    {
        const float value = snapshot.denseValues[code];
        if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
            return ValidationError::InvalidDenseValue;
        if (value > 0.0f)
            ++aggregateActive;
        if (snapshot.denseDeviceCount != 0 && value != merged[code])
            return ValidationError::DenseAggregateMismatch;
    }
    if (aggregateActive != snapshot.denseActiveKeyCount)
        return ValidationError::DenseActiveCountMismatch;

    return ValidationError::None;
}
}
