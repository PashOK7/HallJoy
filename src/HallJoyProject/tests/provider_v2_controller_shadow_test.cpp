#include <cassert>
#include <array>
#include <cstdint>
#include <iostream>

#include "provider_v2_controller_shadow.h"

namespace
{
using namespace halljoy::analog_provider_v2;

AnalogSampleV2 Sample(std::uint32_t device, KeyIdentityV1 key, float value)
{
    AnalogSampleV2 sample{};
    sample.deviceIndex = device;
    sample.key = key;
    sample.flags = AnalogSampleFlag_Owned | AnalogSampleFlag_ValueValid |
        AnalogSampleFlag_Fresh;
    sample.value.normalized = value;
    return sample;
}
}

int main()
{
    using namespace halljoy;
    AnalogSnapshotHeaderV2 header{};
    header.providerId = 1;
    header.providerGeneration = 1;
    header.sampleGeneration = 1;
    header.valueGeneration = 1;
    header.ownershipGeneration = 1;
    header.flags = AnalogSnapshotFlag_Complete;
    header.deviceCount = header.requiredDeviceCount = header.deviceCapacity = 2;
    header.sampleCount = header.requiredSampleCount = header.sampleCapacity = 8;
    std::array<AnalogDeviceV2, 2> devices{};
    std::array<AnalogSampleV2, 8> samples{};
    devices[0].deviceId = 1;
    devices[1].deviceId = 2;

    samples[0] = Sample(0, UsbHidKey(0x07, 0x1A), 0.25f);
    samples[1] = Sample(1, UsbHidKey(0x07, 0x1A), 0.75f);
    samples[2] = Sample(0, UapExtendedKey(keycode::kFn), 0.0f);
    samples[3] = Sample(1, UapExtendedKey(keycode::kFn), 0.6f);
    samples[4] = Sample(0, UapExtendedKey(keycode::kOem1), 0.4f);
    samples[5] = Sample(1, UapExtendedKey(keycode::kOem1), 0.0f);
    samples[6] = Sample(0, UsbHidKey(0x0C, 0xE9), 1.0f);
    samples[7] = Sample(1, HallJoySemanticKey(1, 1), 1.0f);

    provider_v2_shadow::RawInputMapV1 raw{};
    assert(provider_v2_shadow::ProjectCapturedSnapshot(
        header, devices.data(), devices.size(), samples.data(), samples.size(),
        &raw) ==
        provider_v2_shadow::ProjectionError::None);
    assert(raw.owned.test(0x1A));
    assert(raw.values[0x1A] == 0.75f);
    assert(raw.owned.test(keycode::kFn));
    assert(raw.values[keycode::kFn] == 0.6f);
    assert(raw.owned.test(keycode::kOem1));
    assert(raw.values[keycode::kOem1] == 0.4f);
    assert(raw.mappedSampleCount == 6);
    assert(raw.ignoredSampleCount == 2);

    provider_v2_shadow::RawInputMapV1 dynamicRaw{};
    assert(provider_v2_shadow::ProjectCapturedSnapshot(
        header, devices.data(), 2, samples.data(), 8, &dynamicRaw) ==
        provider_v2_shadow::ProjectionError::None);
    assert(dynamicRaw.owned == raw.owned && dynamicRaw.values == raw.values);

    uap_parent_snapshot::SnapshotV1 densePublication{};
    densePublication.providerV2PlaneDualCoherent = true;
    densePublication.providerV2PlaneGeneration = 10;
    densePublication.providerV2PlaneTransactionToken = 20;
    densePublication.publicationGeneration = 30;
    densePublication.publicationTimestampUs = 40;
    provider_v2_snapshot_broker::SnapshotMetadataV1 metadata{
        10, 20, 30, 40 };
    assert(provider_v2_shadow::MatchesCapturedPublication(
        metadata, densePublication));
    ++metadata.transactionToken;
    assert(!provider_v2_shadow::MatchesCapturedPublication(
        metadata, densePublication));
    --metadata.transactionToken;
    ++metadata.planeGeneration;
    assert(!provider_v2_shadow::MatchesCapturedPublication(
        metadata, densePublication));
    --metadata.planeGeneration;
    ++metadata.publicationGeneration;
    assert(!provider_v2_shadow::MatchesCapturedPublication(
        metadata, densePublication));
    --metadata.publicationGeneration;
    ++metadata.publicationTimestampUs;
    assert(!provider_v2_shadow::MatchesCapturedPublication(
        metadata, densePublication));
    --metadata.publicationTimestampUs;
    densePublication.providerV2PlaneDualCoherent = false;
    assert(!provider_v2_shadow::MatchesCapturedPublication(
        metadata, densePublication));

    assert(provider_v2_shadow::MergeWithNative(
        0x1A, true, 0.3f, true, 0.8f) == 0.8f);
    assert(provider_v2_shadow::MergeWithNative(
        0x1A, true, 0.9f, true, 0.2f) == 0.9f);
    assert(provider_v2_shadow::MergeWithNative(
        keycode::kFn, true, 0.4f, true, 0.9f) == 0.4f);
    assert(provider_v2_shadow::MergeWithNative(
        keycode::kFn, false, 0.0f, true, 0.7f) == 0.7f);

    using provider_v2_shadow::AnalogSourceStateV1;
    using provider_v2_shadow::ArbitrationInputV1;
    const auto standard = provider_v2_shadow::Arbitrate({ 0x1A,
        { true, true, true, 0.30f }, { true, true, true, 0.80f }, {}, false });
    assert(standard.value == 0.80f &&
        standard.sourceMask == (provider_v2_shadow::ArbitrationSource_Native |
            provider_v2_shadow::ArbitrationSource_Provider));
    const auto extended = provider_v2_shadow::Arbitrate({ keycode::kFn,
        { true, true, true, 0.0f }, { true, true, true, 0.90f },
        { true, true, true, 1.0f }, true });
    assert(extended.value == 0.0f &&
        extended.sourceMask == provider_v2_shadow::ArbitrationSource_Native);
    const auto fallback = provider_v2_shadow::Arbitrate({ 0x1A,
        {}, { true, true, true, 0.0f }, { true, true, true, 1.0f }, true });
    assert(fallback.value == 1.0f &&
        fallback.sourceMask == (provider_v2_shadow::ArbitrationSource_Provider |
            provider_v2_shadow::ArbitrationSource_DigitalFallback));
    const auto stale = provider_v2_shadow::Arbitrate({ 0x1A,
        {}, { true, true, false, 0.90f }, {}, false });
    assert(stale.value == 0.0f && stale.sourceMask == provider_v2_shadow::ArbitrationSource_None);

    auto qualified = controller::VirtualControllerFrameV1{};
    auto shadow = qualified;
    assert(provider_v2_shadow::CompareFrames(qualified, shadow) == 0);
    shadow.buttons = 1;
    shadow.leftTrigger = 1;
    shadow.rightTrigger = 1;
    shadow.leftStickX = 1;
    shadow.leftStickY = 1;
    shadow.rightStickX = 1;
    shadow.rightStickY = 1;
    const std::uint32_t allFields =
        provider_v2_shadow::FrameMismatch_Buttons |
        provider_v2_shadow::FrameMismatch_LeftTrigger |
        provider_v2_shadow::FrameMismatch_RightTrigger |
        provider_v2_shadow::FrameMismatch_LeftStickX |
        provider_v2_shadow::FrameMismatch_LeftStickY |
        provider_v2_shadow::FrameMismatch_RightStickX |
        provider_v2_shadow::FrameMismatch_RightStickY;
    assert(provider_v2_shadow::CompareFrames(qualified, shadow) == allFields);

    assert(provider_v2_shadow::ProjectCapturedSnapshot(
        header, devices.data(), 2, samples.data(), 8, nullptr) ==
        provider_v2_shadow::ProjectionError::Unavailable);

    std::cout << "PROVIDER_V2_CONTROLLER_SHADOW_TEST=PASS "
        "identity_projection=1 owned_zero=1 multi_device_max=1 "
        "dynamic_view=1 exact_transaction_match=1 native_arbitration=1 "
        "owned_zero_blocks_fallback=1 stale_rejected=1 all_fields=1\n";
    return 0;
}
