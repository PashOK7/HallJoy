#include "../HallJoy/analog_provider_v2.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using namespace halljoy::analog_provider_v2;

namespace
{
AnalogDeviceV2 Device(std::uint64_t id)
{
    AnalogDeviceV2 device{};
    device.deviceId = id;
    device.exactInterfaceId = id + 1000;
    device.flags = AnalogDeviceFlag_Connected |
        AnalogDeviceFlag_StableIdentity |
        AnalogDeviceFlag_ExactInterfaceAssigned |
        AnalogDeviceFlag_ProtocolProven |
        AnalogDeviceFlag_LayoutProven;
    device.protocolId = 1;
    device.layoutId = 1;
    device.vendorId = 0x352d;
    device.productId = 0x2382;
    device.usagePage = 0xff00;
    device.usage = 1;
    return device;
}

AnalogSampleV2 Sample(std::uint32_t deviceIndex, KeyIdentityV1 key,
    AnalogValueV2 value)
{
    AnalogSampleV2 sample{};
    sample.deviceIndex = deviceIndex;
    sample.key = key;
    sample.flags = AnalogSampleFlag_Owned |
        AnalogSampleFlag_ValueValid | AnalogSampleFlag_Fresh;
    sample.value = value;
    return sample;
}

AnalogSnapshotHeaderV2 Header(std::uint32_t deviceCount,
    std::uint32_t sampleCount)
{
    AnalogSnapshotHeaderV2 header{};
    header.providerId = 0x554150ull;
    header.providerGeneration = 1;
    header.sampleGeneration = 1;
    header.valueGeneration = 1;
    header.ownershipGeneration = 1;
    header.sampleTimestampUs = 100;
    header.valueTimestampUs = 100;
    header.flags = AnalogSnapshotFlag_Complete;
    header.deviceCount = deviceCount;
    header.deviceCapacity = deviceCount;
    header.requiredDeviceCount = deviceCount;
    header.sampleCount = sampleCount;
    header.sampleCapacity = sampleCount;
    header.requiredSampleCount = sampleCount;
    return header;
}
}

int main()
{
    constexpr auto usbKeyboardF0 = UsbHidKey(0x07, 0xf0);
    constexpr auto halljoyPointerF0 = HallJoySemanticKey(1, 0xf0);
    constexpr auto usbMenu = UsbHidKey(0x07, 0x65);
    constexpr auto consumerPlay = UsbHidKey(0x0c, 0xcd);
    constexpr auto uapFn = UapExtendedKey(0x409);
    constexpr auto uapOem1 = UapExtendedKey(0x403);
    static_assert(IsValid(usbKeyboardF0));
    static_assert(IsValid(halljoyPointerF0));
    static_assert(IsValid(usbMenu));
    static_assert(IsValid(consumerPlay));
    static_assert(IsValid(uapFn));
    static_assert(IsValid(uapOem1));
    static_assert(usbKeyboardF0 != halljoyPointerF0);
    static_assert(usbMenu != consumerPlay);
    static_assert(uapFn != UsbHidKey(0x07, 0x409));
    static_assert(!IsValid(KeyIdentityV1{}));
    static_assert(!IsValid({ kKeyIdentityVersion,
        KeyNamespace::UapExtended, 1, 0x409 }));

    const AnalogValueV2 raw1 = ValueFromRaw(1, 4095);
    const AnalogValueV2 raw2 = ValueFromRaw(2, 4095);
    assert(raw1.rawNumerator == 1 && raw2.rawNumerator == 2);
    assert(raw1.rawDomain == 4095 && raw2.rawDomain == 4095);
    assert(raw1.normalized > 0.0f && raw2.normalized > raw1.normalized);
    assert(static_cast<unsigned>(raw1.normalized * 1000.0f) == 0);
    assert(static_cast<unsigned>(raw2.normalized * 1000.0f) == 0);
    assert((ValueFromLegacyMilli(500).flags &
        AnalogValueFlag_LegacyQuantized) != 0);

    for (const std::uint32_t count : { 1u, 8u, 16u, 32u })
    {
        std::vector<AnalogDeviceV2> devices;
        std::vector<AnalogSampleV2> samples;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            devices.push_back(Device(i + 1));
            samples.push_back(Sample(i, usbMenu, ValueFromRaw(i, count)));
        }
        const auto header = Header(count, count);
        assert(IsAuthoritative(header));
        assert(ValidateSnapshot(header, devices.data(), devices.size(),
            samples.data(), samples.size()) == SnapshotValidationError::None);
    }

    std::array<AnalogDeviceV2, 2> devices{ Device(1), Device(2) };
    std::array<AnalogSampleV2, 2> twoDevices{
        Sample(0, uapFn, ValueFromRaw(0, 4095)),
        Sample(1, uapFn, ValueFromRaw(4095, 4095)),
    };
    auto header = Header(2, 2);
    assert(ValidateSnapshot(header, devices.data(), devices.size(),
        twoDevices.data(), twoDevices.size()) == SnapshotValidationError::None);
    assert(twoDevices[0].value.normalized == 0.0f); // authoritative release

    auto duplicate = twoDevices;
    duplicate[1].deviceIndex = 0;
    assert(ValidateSnapshot(header, devices.data(), devices.size(),
        duplicate.data(), duplicate.size()) ==
        SnapshotValidationError::DuplicateSample);

    auto truncated = header;
    truncated.flags = AnalogSnapshotFlag_Truncated;
    truncated.deviceCapacity = 1;
    truncated.deviceCount = 1;
    truncated.requiredDeviceCount = 2;
    truncated.sampleCapacity = 1;
    truncated.sampleCount = 1;
    truncated.requiredSampleCount = 2;
    assert(!IsAuthoritative(truncated));
    assert(ValidateSnapshot(truncated, devices.data(), devices.size(),
        twoDevices.data(), twoDevices.size()) == SnapshotValidationError::None);

    auto falseComplete = truncated;
    falseComplete.flags |= AnalogSnapshotFlag_Complete;
    assert(ValidateSnapshot(falseComplete, devices.data(), devices.size(),
        twoDevices.data(), twoDevices.size()) ==
        SnapshotValidationError::InvalidCompleteness);

    auto hiddenInsufficient = truncated;
    hiddenInsufficient.flags = AnalogSnapshotFlag_Complete;
    assert(ValidateSnapshot(hiddenInsufficient, devices.data(), devices.size(),
        twoDevices.data(), twoDevices.size()) ==
        SnapshotValidationError::InvalidCompleteness);

    auto previous = Header(0, 0);
    previous.sampleGeneration = 10;
    previous.valueGeneration = 4;
    previous.ownershipGeneration = 2;
    previous.sampleTimestampUs = 1000;
    previous.valueTimestampUs = 900;

    auto sampleOnly = previous;
    sampleOnly.sampleGeneration = 11;
    sampleOnly.sampleTimestampUs = 1100;
    assert(ValidateTransition(previous, sampleOnly) ==
        SnapshotTransitionError::None);
    assert(!RequiresRealtimeWake(previous, sampleOnly));

    auto changed = sampleOnly;
    changed.valueGeneration = 5;
    changed.valueTimestampUs = 1100;
    assert(RequiresRealtimeWake(sampleOnly, changed));

    auto regressed = previous;
    regressed.valueGeneration = 3;
    assert(ValidateTransition(previous, regressed) ==
        SnapshotTransitionError::GenerationRegressed);

    auto restarted = previous;
    restarted.providerGeneration = 2;
    restarted.sampleGeneration = 1;
    restarted.valueGeneration = 1;
    restarted.ownershipGeneration = 1;
    restarted.sampleTimestampUs = 1;
    restarted.valueTimestampUs = 1;
    assert(ValidateTransition(previous, restarted) ==
        SnapshotTransitionError::None);
    assert(RequiresRealtimeWake(previous, restarted));

    auto exhausted = previous;
    exhausted.providerGeneration = std::numeric_limits<std::uint64_t>::max();
    auto wrapped = exhausted;
    wrapped.providerGeneration = 1;
    assert(ValidateTransition(exhausted, wrapped) ==
        SnapshotTransitionError::ProviderGenerationRegressed);

    AnalogSampleV2 invalidValue = twoDevices[0];
    invalidValue.value.normalized = std::numeric_limits<float>::quiet_NaN();
    std::array<AnalogSampleV2, 1> invalidSamples{ invalidValue };
    auto one = Header(2, 1);
    assert(ValidateSnapshot(one, devices.data(), devices.size(),
        invalidSamples.data(), invalidSamples.size()) ==
        SnapshotValidationError::InvalidSample);
    return 0;
}
