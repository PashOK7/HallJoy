#include "../HallJoy/provider_v2_snapshot_broker.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace halljoy::analog_provider_v2;
using namespace halljoy::provider_v2_snapshot_broker;

namespace
{
AnalogSnapshotHeaderV2 Header(std::uint64_t generation, float value)
{
    AnalogSnapshotHeaderV2 header{};
    header.providerId = 0x554150ull;
    header.providerGeneration = 1;
    header.sampleGeneration = generation;
    header.valueGeneration = generation;
    header.ownershipGeneration = 1;
    header.sampleTimestampUs = generation * 100;
    header.valueTimestampUs = generation * 100;
    header.flags = AnalogSnapshotFlag_Complete;
    header.deviceCount = 1;
    header.deviceCapacity = 1;
    header.requiredDeviceCount = 1;
    header.sampleCount = 1;
    header.sampleCapacity = 1;
    header.requiredSampleCount = 1;
    (void)value;
    return header;
}

AnalogDeviceV2 Device()
{
    AnalogDeviceV2 device{};
    device.deviceId = 7;
    device.flags = AnalogDeviceFlag_Connected;
    device.vendorId = 0x352d;
    device.productId = 0x2382;
    device.usagePage = 0xff00;
    device.usage = 1;
    return device;
}

AnalogSampleV2 Sample(float value)
{
    AnalogSampleV2 sample{};
    sample.deviceIndex = 0;
    sample.key = UsbHidKey(0x07, 0x04);
    sample.flags = AnalogSampleFlag_Owned |
        AnalogSampleFlag_ValueValid | AnalogSampleFlag_Fresh;
    sample.value.normalized = value;
    return sample;
}

PublishResult Publish(ParentSnapshotBroker& broker, std::uint64_t generation,
    float value, std::uint64_t planeGeneration = 10)
{
    const auto header = Header(generation, value);
    const auto device = Device();
    const auto sample = Sample(value);
    const SnapshotMetadataV1 metadata{
        planeGeneration, generation, generation, generation * 100 };
    return broker.TryPublish(metadata, header, &device, &sample);
}

bool RejectCommit(void*) noexcept
{
    return false;
}
}

int main()
{
    ParentSnapshotBroker broker;
    assert(!broker.IsActive());
    assert(broker.IsDrained());
    assert(broker.Prepare(1, 1));
    assert(broker.DeviceCapacity() == 1);
    assert(broker.SampleCapacity() == 1);
    assert(broker.Activate(10));
    assert(!broker.Acquire());

    assert(Publish(broker, 1, 0.25f) == PublishResult::Published);
    auto first = broker.Acquire();
    assert(first);
    assert(first.Metadata().planeGeneration == 10);
    assert(first.Metadata().transactionToken == 1);
    assert(first.Header().sampleGeneration == 1);
    assert(first.DeviceCount() == 1 && first.SampleCount() == 1);
    assert(first.Samples()[0].value.normalized == 0.25f);

    // A leased old slot stays immutable while the writer advances through the
    // other two slots.
    assert(Publish(broker, 2, 0.50f) == PublishResult::Published);
    auto second = broker.Acquire();
    assert(second && second.Samples()[0].value.normalized == 0.50f);
    assert(Publish(broker, 3, 0.75f) == PublishResult::Published);
    auto third = broker.Acquire();
    assert(third && third.Samples()[0].value.normalized == 0.75f);
    assert(first.Samples()[0].value.normalized == 0.25f);

    // All non-current slots are leased, so the bridge drops an intermediate
    // publication instead of waiting for realtime.
    assert(Publish(broker, 4, 1.0f) == PublishResult::NoFreeSlot);

    broker.BeginReconfigure();
    assert(!broker.IsActive());
    assert(!broker.Acquire());
    assert(!broker.IsDrained());
    assert(!broker.Prepare(2, 2));
    first = {};
    second = {};
    third = {};
    assert(broker.IsDrained());
    assert(broker.Prepare(2, 2));
    assert(broker.Activate(11));
    assert(Publish(broker, 5, 0.4f, 10) ==
        PublishResult::GenerationMismatch);
    assert(Publish(broker, 5, 0.4f, 11) == PublishResult::Published);

    const auto rejectedHeader = Header(6, 0.6f);
    const auto rejectedDevice = Device();
    const auto rejectedSample = Sample(0.6f);
    assert(broker.TryPublish({ 11, 6, 6, 600 }, rejectedHeader,
        &rejectedDevice, &rejectedSample, RejectCommit, nullptr) ==
        PublishResult::CommitInvalidated);
    auto stillFive = broker.Acquire();
    assert(stillFive && stillFive.Metadata().transactionToken == 5);
    stillFive = {};

    auto invalidHeader = Header(6, 0.5f);
    invalidHeader.flags = AnalogSnapshotFlag_Truncated;
    const auto device = Device();
    const auto sample = Sample(0.5f);
    assert(broker.TryPublish({ 11, 6, 6, 600 }, invalidHeader,
        &device, &sample) == PublishResult::InvalidSnapshot);

    broker.BeginReconfigure();
    assert(broker.IsDrained());
    assert(broker.Prepare(12, 60));
    assert(broker.Activate(12));
    AnalogSnapshotHeaderV2 expanded{};
    expanded.providerId = 0x554150ull;
    expanded.providerGeneration = 1;
    expanded.sampleGeneration = 7;
    expanded.valueGeneration = 7;
    expanded.ownershipGeneration = 2;
    expanded.sampleTimestampUs = 700;
    expanded.valueTimestampUs = 700;
    expanded.flags = AnalogSnapshotFlag_Complete;
    expanded.deviceCount = expanded.deviceCapacity =
        expanded.requiredDeviceCount = 12;
    expanded.sampleCount = expanded.sampleCapacity =
        expanded.requiredSampleCount = 60;
    std::vector<AnalogDeviceV2> expandedDevices(12);
    std::vector<AnalogSampleV2> expandedSamples(60);
    for (std::uint32_t di = 0; di < 12; ++di)
    {
        expandedDevices[di] = Device();
        expandedDevices[di].deviceId = 100 + di;
        for (std::uint32_t key = 0; key < 5; ++key)
        {
            auto& item = expandedSamples[di * 5 + key];
            item = Sample(static_cast<float>(di * 5 + key) / 60.0f);
            item.deviceIndex = di;
            item.key = UsbHidKey(0x07, 0x04 + key);
        }
    }
    assert(broker.TryPublish({ 12, 7, 7, 700 }, expanded,
        expandedDevices.data(), expandedSamples.data()) ==
        PublishResult::Published);
    auto expandedLease = broker.Acquire();
    assert(expandedLease && expandedLease.DeviceCount() == 12 &&
        expandedLease.SampleCount() == 60);
    assert(expandedLease.Devices()[11].deviceId == 111);
    assert(expandedLease.Samples()[59].deviceIndex == 11);
    expandedLease = {};
    broker.BeginReconfigure();
    assert(broker.IsDrained());
    std::cout << "PROVIDER_V2_SNAPSHOT_BROKER_TARGETED=PASS "
        "negotiated_devices=12 negotiated_samples=60 immutable_leases=1 "
        "commit_recheck=1\n";
    return 0;
}
