#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_uap_provider_v2_projection.h"
#include "../HallJoy/analog_provider_v2.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using namespace halljoy::analog_provider_v2;
using namespace HallJoyUapProviderV2;

namespace
{
constexpr std::size_t kSourceKeys = 5;

AnalogDeviceV2 Device(std::uint64_t id)
{
    AnalogDeviceV2 result{};
    result.deviceId = id;
    result.flags = AnalogDeviceFlag_Connected | AnalogDeviceFlag_StableIdentity;
    result.vendorId = 0x352d;
    result.productId = static_cast<std::uint16_t>(0x2380u + id);
    result.usagePage = 0xff00;
    result.usage = 1;
    return result;
}

std::array<ProjectionKey, kSourceKeys> Keys()
{
    return {
        ProjectionKey{ IdentityFromLegacyCode(0x04), 0 },
        ProjectionKey{ IdentityFromLegacyCode(0x1a), 1 },
        ProjectionKey{ IdentityFromLegacyCode(0x65), 2 },
        ProjectionKey{ IdentityFromLegacyCode(0x3cd), 3 },
        ProjectionKey{ IdentityFromLegacyCode(0x409), 4 },
    };
}

ProjectionGeneration Generation(std::uint64_t sample)
{
    return { 7, sample, sample, 3, 900 + sample, 1000 + sample };
}
}

int main()
{
    const auto keys = Keys();
    static_assert(IdentityFromLegacyCode(0x65) == UsbHidKey(0x07, 0x65));
    static_assert(IdentityFromLegacyCode(0x3cd) == UsbHidKey(0x0c, 0xcd));
    static_assert(IdentityFromLegacyCode(0x409) == UapExtendedKey(0x409));

    std::array<float, kSourceKeys> values0{ 0.20f, 0.80f, 0.70f, 0.40f, 0.35f };
    std::array<float, kSourceKeys> values1{ 0.60f, 0.30f, 0.10f, 0.90f, 0.00f };
    std::array<ProjectionDevice, 2> inputs{
        ProjectionDevice{ Device(1), values0.data(), values0.size(), 1001 },
        ProjectionDevice{ Device(2), values1.data(), values1.size(), 1002 },
    };
    std::array<AnalogDeviceV2, 8> devices{};
    std::array<AnalogSampleV2, 64> samples{};
    AnalogSnapshotHeaderV2 header{};
    assert(BuildSnapshot(keys.data(), keys.size(), inputs.data(), inputs.size(),
        inputs.size(), Generation(10), &header, devices.data(), devices.size(),
        samples.data(), samples.size()) == ProjectionError::None);
    assert(IsAuthoritative(header));
    assert(header.deviceCount == 2 && header.sampleCount == 10);
    assert(ValidateSnapshot(header, devices.data(), devices.size(), samples.data(),
        samples.size()) == SnapshotValidationError::None);

    std::array<float, 256> projected{};
    assert(ProjectOrdinaryUsbHidDense(header, samples.data(), projected.data(),
        projected.size()));
    std::array<float, 256> legacy{};
    for (std::size_t device = 0; device < inputs.size(); ++device)
    {
        for (std::size_t key = 0; key < keys.size(); ++key)
        {
            const auto identity = keys[key].identity;
            if (identity.keyNamespace == KeyNamespace::UsbHidUsage &&
                identity.usagePage == 0x07u && identity.usage < legacy.size())
            {
                legacy[identity.usage] = (std::max)(legacy[identity.usage],
                    inputs[device].values[keys[key].sourceIndex]);
            }
        }
    }
    assert(projected == legacy);
    assert(projected[0x04] == 0.60f);
    assert(projected[0x1a] == 0.80f);
    assert(projected[0x65] == 0.70f);
    // Consumer media and UAP Fn remain in V2 but cannot impersonate page-07 HID.
    assert(projected[0xcd] == 0.0f);
    std::array<float, 256> projectedDevice{};
    assert(ProjectCapturedDeviceOrdinaryUsbHidDense(header, samples.data(), 0,
        projectedDevice.data(), projectedDevice.size()));
    assert(projectedDevice[0x04] == 0.20f);
    assert(projectedDevice[0x1a] == 0.80f);
    assert(projectedDevice[0x65] == 0.70f);
    assert(ProjectCapturedDeviceOrdinaryUsbHidDense(header, samples.data(), 1,
        projectedDevice.data(), projectedDevice.size()));
    assert(projectedDevice[0x04] == 0.60f);
    assert(projectedDevice[0x1a] == 0.30f);
    bool sawConsumer = false;
    bool sawFnRelease = false;
    for (std::size_t i = 0; i < header.sampleCount; ++i)
    {
        sawConsumer = sawConsumer || samples[i].key == UsbHidKey(0x0c, 0xcd);
        sawFnRelease = sawFnRelease ||
            (samples[i].deviceIndex == 1 && samples[i].key == UapExtendedKey(0x409) &&
                samples[i].value.normalized == 0.0f);
    }
    assert(sawConsumer && sawFnRelease);

    // A release is a complete owned zero generation, not an omitted sparse key.
    values0.fill(0.0f);
    values1.fill(0.0f);
    assert(BuildSnapshot(keys.data(), keys.size(), inputs.data(), inputs.size(),
        inputs.size(), Generation(11), &header, devices.data(), devices.size(),
        samples.data(), samples.size()) == ProjectionError::None);
    assert(ProjectOrdinaryUsbHidDense(header, samples.data(), projected.data(),
        projected.size()));
    for (const float value : projected)
        assert(value == 0.0f);

    // More registry devices than the pinned transport capacity must be reported
    // as truncated. The first eight devices may be copied, but never presented
    // as the whole provider topology.
    // Oversized caller buffers must not hide the smaller pinned capture window.
    assert(BuildSnapshot(keys.data(), keys.size(), inputs.data(), inputs.size(),
        12, Generation(12), &header, devices.data(), devices.size(),
        samples.data(), samples.size()) == ProjectionError::None);
    assert(!IsAuthoritative(header));
    assert(header.deviceCount == 2 && header.requiredDeviceCount == 12);
    assert(header.sampleCount == 10 && header.requiredSampleCount == 60);
    assert(header.deviceCapacity == 2 && header.sampleCapacity == 10);
    assert(ValidateSnapshot(header, devices.data(), devices.size(), samples.data(),
        samples.size()) == SnapshotValidationError::None);
    assert(!ProjectOrdinaryUsbHidDense(header, samples.data(), projected.data(),
        projected.size()));

    // Once the producer negotiates exact storage, the same twelve-device
    // topology must become one complete authoritative generation.
    std::array<std::array<float, kSourceKeys>, 12> negotiatedValues{};
    std::array<ProjectionDevice, 12> negotiatedInputs{};
    for (std::size_t device = 0; device < negotiatedInputs.size(); ++device)
    {
        for (std::size_t key = 0; key < kSourceKeys; ++key)
            negotiatedValues[device][key] =
                static_cast<float>((device + key) % 11u) / 10.0f;
        negotiatedInputs[device] = ProjectionDevice{
            Device(device + 1), negotiatedValues[device].data(),
            negotiatedValues[device].size(), 2000 + device };
    }
    std::vector<AnalogDeviceV2> negotiatedDevices(negotiatedInputs.size());
    std::vector<AnalogSampleV2> negotiatedSamples(
        negotiatedInputs.size() * keys.size());
    assert(BuildSnapshot(keys.data(), keys.size(), negotiatedInputs.data(),
        negotiatedInputs.size(), negotiatedInputs.size(), Generation(13),
        &header, negotiatedDevices.data(), negotiatedDevices.size(),
        negotiatedSamples.data(), negotiatedSamples.size()) ==
        ProjectionError::None);
    assert(IsAuthoritative(header));
    assert(header.deviceCount == 12 && header.requiredDeviceCount == 12);
    assert(header.sampleCount == 60 && header.requiredSampleCount == 60);
    assert(ValidateSnapshot(header, negotiatedDevices.data(),
        negotiatedDevices.size(), negotiatedSamples.data(),
        negotiatedSamples.size()) == SnapshotValidationError::None);

    auto duplicateKeys = keys;
    duplicateKeys[4].identity = duplicateKeys[0].identity;
    assert(BuildSnapshot(duplicateKeys.data(), duplicateKeys.size(), inputs.data(),
        inputs.size(), inputs.size(), Generation(14), &header, devices.data(),
        devices.size(), samples.data(), samples.size()) ==
        ProjectionError::DuplicateIdentity);

    values0[0] = std::numeric_limits<float>::quiet_NaN();
    assert(BuildSnapshot(keys.data(), keys.size(), inputs.data(), inputs.size(),
        inputs.size(), Generation(15), &header, devices.data(), devices.size(),
        samples.data(), samples.size()) == ProjectionError::InvalidValue);

    std::cout << "UAP_PROVIDER_V2_PROJECTION_TEST=PASS devices=2 samples=10 "
                 "ordinary_hid_equivalence=1 special_keys_retained=1 "
                 "release_zero=1 hidden_truncation_rejected=1 "
                 "negotiated_12_devices=1\n";
    return 0;
}
