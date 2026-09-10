#include "provider_v2_controller_shadow.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace halljoy::provider_v2_shadow
{
namespace
{
bool ToBindableCode(const analog_provider_v2::KeyIdentityV1& identity,
    std::uint16_t* code) noexcept
{
    using analog_provider_v2::KeyNamespace;
    if (!code)
        return false;

    if (identity.keyNamespace == KeyNamespace::UsbHidUsage)
    {
        if (identity.usagePage != 0x07u || identity.usage == 0u ||
            identity.usage >= keycode::kStandardHidCount)
            return false;
        *code = static_cast<std::uint16_t>(identity.usage);
        return true;
    }

    if (identity.keyNamespace == KeyNamespace::UapExtended)
    {
        if (identity.usage < keycode::kStandardHidCount ||
            identity.usage > std::numeric_limits<std::uint16_t>::max())
            return false;
        const auto candidate = static_cast<std::uint16_t>(identity.usage);
        if (!keycode::IsSupported(candidate))
            return false;
        *code = candidate;
        return true;
    }
    return false;
}
}

ProjectionError ProjectCapturedSnapshot(
    const analog_provider_v2::AnalogSnapshotHeaderV2& header,
    const analog_provider_v2::AnalogDeviceV2* devices,
    std::size_t deviceStorageCount,
    const analog_provider_v2::AnalogSampleV2* samples,
    std::size_t sampleStorageCount,
    RawInputMapV1* output) noexcept
{
    using namespace analog_provider_v2;
    if (!output)
        return ProjectionError::Unavailable;
    if (!IsAuthoritative(header) ||
        header.deviceCount != header.requiredDeviceCount ||
        header.sampleCount != header.requiredSampleCount)
        return ProjectionError::NotAuthoritative;
    if (header.deviceCount > deviceStorageCount ||
        header.sampleCount > sampleStorageCount)
        return ProjectionError::CapacityMismatch;
    if (ValidateSnapshot(header, devices, deviceStorageCount, samples,
            sampleStorageCount) != SnapshotValidationError::None)
    {
        return ProjectionError::InvalidSnapshot;
    }

    RawInputMapV1 projected{};
    for (std::size_t i = 0; i < header.sampleCount; ++i)
    {
        const auto& sample = samples[i];
        constexpr std::uint32_t requiredFlags = AnalogSampleFlag_Owned |
            AnalogSampleFlag_ValueValid | AnalogSampleFlag_Fresh;
        if (sample.deviceIndex >= header.deviceCount ||
            (sample.flags & requiredFlags) != requiredFlags ||
            !std::isfinite(sample.value.normalized) ||
            sample.value.normalized < 0.0f || sample.value.normalized > 1.0f)
            return ProjectionError::InvalidSample;

        std::uint16_t code = 0;
        if (!ToBindableCode(sample.key, &code))
        {
            ++projected.ignoredSampleCount;
            continue;
        }
        projected.owned.set(code);
        projected.values[code] = (std::max)(projected.values[code],
            sample.value.normalized);
        ++projected.mappedSampleCount;
    }

    *output = projected;
    return ProjectionError::None;
}

bool MatchesCapturedPublication(
    const provider_v2_snapshot_broker::SnapshotMetadataV1& provider,
    const uap_parent_snapshot::SnapshotV1& dense) noexcept
{
    return dense.providerV2PlaneDualCoherent &&
        provider.planeGeneration != 0 && provider.transactionToken != 0 &&
        provider.publicationGeneration != 0 &&
        provider.planeGeneration == dense.providerV2PlaneGeneration &&
        provider.transactionToken ==
            dense.providerV2PlaneTransactionToken &&
        provider.publicationGeneration == dense.publicationGeneration &&
        provider.publicationTimestampUs == dense.publicationTimestampUs;
}

ArbitrationResultV1 Arbitrate(const ArbitrationInputV1& input) noexcept
{
    ArbitrationResultV1 result{};
    if (!keycode::IsSupported(input.keyCode))
        return result;

    const auto add = [&result](const AnalogSourceStateV1& source,
        std::uint32_t mask) noexcept {
        if (!source.available || !source.owned || !source.fresh ||
            !std::isfinite(source.value))
            return;
        const float value = std::clamp(source.value, 0.0f, 1.0f);
        result.value = (std::max)(result.value, value);
        result.sourceMask |= mask;
    };

    add(input.native, ArbitrationSource_Native);
    if (input.provider.available && input.provider.owned && input.provider.fresh &&
        (!input.native.owned || keycode::IsStandardHid(input.keyCode)))
    {
        add(input.provider, ArbitrationSource_Provider);
    }
    if (input.allowDigitalFallback && keycode::IsStandardHid(input.keyCode) &&
        !input.native.owned && result.value <= 0.001f)
    {
        add(input.digitalFallback, ArbitrationSource_DigitalFallback);
    }
    return result;
}

float MergeWithNative(std::uint16_t keyCode,
    bool nativeOwned, float nativeValue,
    bool providerOwned, float providerValue) noexcept
{
    return Arbitrate({ keyCode,
        { nativeOwned, nativeOwned, nativeOwned, nativeValue },
        { providerOwned, providerOwned, providerOwned, providerValue },
        {}, false }).value;
}

std::uint32_t CompareFrames(
    const controller::VirtualControllerFrameV1& qualified,
    const controller::VirtualControllerFrameV1& shadow) noexcept
{
    std::uint32_t mismatch = FrameMismatch_None;
    if (qualified.buttons != shadow.buttons)
        mismatch |= FrameMismatch_Buttons;
    if (qualified.leftTrigger != shadow.leftTrigger)
        mismatch |= FrameMismatch_LeftTrigger;
    if (qualified.rightTrigger != shadow.rightTrigger)
        mismatch |= FrameMismatch_RightTrigger;
    if (qualified.leftStickX != shadow.leftStickX)
        mismatch |= FrameMismatch_LeftStickX;
    if (qualified.leftStickY != shadow.leftStickY)
        mismatch |= FrameMismatch_LeftStickY;
    if (qualified.rightStickX != shadow.rightStickX)
        mismatch |= FrameMismatch_RightStickX;
    if (qualified.rightStickY != shadow.rightStickY)
        mismatch |= FrameMismatch_RightStickY;
    return mismatch;
}
}
