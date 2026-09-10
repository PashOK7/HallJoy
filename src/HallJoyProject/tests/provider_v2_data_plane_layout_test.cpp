#include "../HallJoy/provider_v2_data_plane_layout.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using namespace halljoy::analog_provider_v2;
using namespace halljoy::provider_v2_data_plane;

namespace
{
class AlignedBytes
{
public:
    explicit AlignedBytes(std::size_t size)
        : storage_(size + kLayoutAlignment - 1)
    {
        const auto address = reinterpret_cast<std::uintptr_t>(storage_.data());
        const auto aligned = (address + kLayoutAlignment - 1) &
            ~(static_cast<std::uintptr_t>(kLayoutAlignment) - 1);
        data_ = reinterpret_cast<std::byte*>(aligned);
        size_ = size;
        std::fill_n(data_, size_, std::byte{});
    }

    void* data() noexcept { return data_; }
    const void* data() const noexcept { return data_; }
    std::size_t size() const noexcept { return size_; }

private:
    std::vector<std::byte> storage_;
    std::byte* data_ = nullptr;
    std::size_t size_ = 0;
};

AnalogDeviceV2 Device(std::uint64_t id)
{
    AnalogDeviceV2 device{};
    device.deviceId = id;
    device.exactInterfaceId = 0x1000 + id;
    device.flags = AnalogDeviceFlag_Connected |
        AnalogDeviceFlag_StableIdentity;
    device.vendorId = 0x0416;
    device.productId = 0x7372;
    device.usagePage = 0xff1b;
    device.usage = 0x0091;
    return device;
}

AnalogSampleV2 Sample(std::uint32_t deviceIndex, std::uint32_t usage,
    float value)
{
    AnalogSampleV2 sample{};
    sample.deviceIndex = deviceIndex;
    sample.key = UsbHidKey(0x07, usage);
    sample.flags = AnalogSampleFlag_Owned |
        AnalogSampleFlag_ValueValid | AnalogSampleFlag_Fresh;
    sample.value.normalized = value;
    return sample;
}

AnalogSnapshotHeaderV2 Header(std::uint32_t deviceCount,
    std::uint32_t sampleCount, std::uint32_t deviceCapacity,
    std::uint32_t sampleCapacity)
{
    AnalogSnapshotHeaderV2 header{};
    header.providerId = 0x554150;
    header.providerGeneration = 7;
    header.sampleGeneration = 41;
    header.valueGeneration = 39;
    header.ownershipGeneration = 12;
    header.sampleTimestampUs = 10000;
    header.valueTimestampUs = 9990;
    header.flags = AnalogSnapshotFlag_Complete;
    header.deviceCount = deviceCount;
    header.deviceCapacity = deviceCapacity;
    header.requiredDeviceCount = deviceCount;
    header.sampleCount = sampleCount;
    header.sampleCapacity = sampleCapacity;
    header.requiredSampleCount = sampleCount;
    return header;
}
}

int main()
{
    const auto initialPlan = PlanCapacity(0, 0, 12, 60);
    assert(initialPlan.action == CapacityAction::Replace);
    assert(initialPlan.deviceCapacity == 12);
    assert(initialPlan.sampleCapacity == 60);
    const auto retainedPlan = PlanCapacity(12, 60, 8, 40);
    assert(retainedPlan.action == CapacityAction::Fits);
    assert(retainedPlan.deviceCapacity == 12);
    assert(retainedPlan.sampleCapacity == 60);
    const auto partialGrowth = PlanCapacity(12, 60, 13, 55);
    assert(partialGrowth.action == CapacityAction::Replace);
    assert(partialGrowth.deviceCapacity == 13);
    assert(partialGrowth.sampleCapacity == 60);
    assert(PlanCapacity(0, 0, 0, 1).action == CapacityAction::Reject);
    assert(PlanCapacity(kMaximumDevices, kMaximumSamples,
        kMaximumDevices + 1, 0).action == CapacityAction::Reject);

    std::size_t previousBytes = 0;
    for (const std::uint32_t devices : { 0u, 1u, 8u, 12u, 32u })
    {
        LayoutV1 layout{};
        assert(CalculateLayout(devices, devices * 5, &layout) ==
            ValidationError::None);
        assert(layout.deviceCapacity == devices);
        assert(layout.sampleCapacity == devices * 5);
        assert(layout.mappingBytes >= previousBytes);
        assert(layout.slotsOffset % kLayoutAlignment == 0);
        assert(layout.slotStride % kLayoutAlignment == 0);
        previousBytes = layout.mappingBytes;
    }

    LayoutV1 maximum{};
    assert(CalculateLayout(kMaximumDevices, kMaximumSamples, &maximum) ==
        ValidationError::None);
    assert(maximum.mappingBytes > 0);
    assert(CalculateLayout(kMaximumDevices + 1, 0, &maximum) ==
        ValidationError::InvalidCapacity);
    assert(CalculateLayout(0, kMaximumSamples + 1, &maximum) ==
        ValidationError::InvalidCapacity);
    assert(CalculateLayout(1, 1, nullptr) ==
        ValidationError::MissingStorage);

    constexpr std::uint32_t deviceCount = 12;
    constexpr std::uint32_t keysPerDevice = 5;
    constexpr std::uint32_t sampleCount = deviceCount * keysPerDevice;
    constexpr std::uint64_t planeGeneration = 3;
    constexpr std::uint64_t launchNonce = 0x123456789abcdef0ull;
    constexpr std::uint64_t token = 0x300000029ull;
    LayoutV1 layout{};
    assert(CalculateLayout(deviceCount, sampleCount, &layout) ==
        ValidationError::None);
    AlignedBytes mapping(layout.mappingBytes);
    assert(InitializeMapping(mapping.data(), mapping.size(), planeGeneration,
        launchNonce, deviceCount, sampleCount) == ValidationError::None);
    LayoutV1 validated{};
    assert(ValidateMapping(mapping.data(), mapping.size(), &validated) ==
        ValidationError::None);
    assert(validated.mappingBytes == layout.mappingBytes);

    MutableSlotViewV1 slot{};
    assert(GetMutableSlot(mapping.data(), mapping.size(), 0, &slot) ==
        ValidationError::None);
    *slot.snapshot = Header(
        deviceCount, sampleCount, deviceCount, sampleCount);
    for (std::uint32_t device = 0; device < deviceCount; ++device)
    {
        slot.devices[device] = Device(device + 1);
        for (std::uint32_t key = 0; key < keysPerDevice; ++key)
        {
            const std::uint32_t index = device * keysPerDevice + key;
            slot.samples[index] = Sample(device, 4 + key,
                static_cast<float>(index) / static_cast<float>(sampleCount));
        }
    }
    slot.commit->deviceCount = deviceCount;
    slot.commit->sampleCount = sampleCount;
    slot.commit->transactionToken = token;
    slot.commit->sequence = 2;

    ConstSlotViewV1 captured{};
    assert(ValidateCommittedSlot(mapping.data(), mapping.size(), 0,
        planeGeneration, launchNonce, token, &captured) ==
        ValidationError::None);
    assert(captured.snapshot->deviceCount == 12);
    assert(captured.snapshot->sampleCount == 60);
    assert(captured.samples[59].deviceIndex == 11);

    // Mapping allocation is retained on topology shrink. The provider header
    // describes the smaller current capture capacity, not the larger section.
    *slot.snapshot = Header(8, 40, 8, 40);
    for (std::uint32_t device = 0; device < 8; ++device)
    {
        slot.devices[device] = Device(device + 1);
        for (std::uint32_t key = 0; key < keysPerDevice; ++key)
        {
            const std::uint32_t index = device * keysPerDevice + key;
            slot.samples[index] = Sample(device, 4 + key,
                static_cast<float>(index) / 40.0f);
        }
    }
    slot.commit->deviceCount = 8;
    slot.commit->sampleCount = 40;
    assert(ValidateCommittedSlot(mapping.data(), mapping.size(), 0,
        planeGeneration, launchNonce, token, &captured) ==
        ValidationError::None);
    *slot.snapshot = Header(
        deviceCount, sampleCount, deviceCount, sampleCount);
    slot.commit->deviceCount = deviceCount;
    slot.commit->sampleCount = sampleCount;

    assert(ValidateCommittedSlot(mapping.data(), mapping.size(), 0,
        planeGeneration, launchNonce, token + 1, &captured) ==
        ValidationError::TransactionMismatch);
    assert(ValidateCommittedSlot(mapping.data(), mapping.size(), 0,
        planeGeneration + 1, launchNonce, token, &captured) ==
        ValidationError::GenerationMismatch);
    slot.commit->sequence = 3;
    assert(ValidateCommittedSlot(mapping.data(), mapping.size(), 0,
        planeGeneration, launchNonce, token, &captured) ==
        ValidationError::UncommittedSlot);
    slot.commit->sequence = 4;

    const auto savedSamplesOffset = slot.commit->samplesOffset;
    slot.commit->samplesOffset += 8;
    assert(ValidateCommittedSlot(mapping.data(), mapping.size(), 0,
        planeGeneration, launchNonce, token, &captured) ==
        ValidationError::InvalidSlot);
    slot.commit->samplesOffset = savedSamplesOffset;

    slot.snapshot->flags = AnalogSnapshotFlag_Complete |
        AnalogSnapshotFlag_Truncated;
    assert(ValidateCommittedSlot(mapping.data(), mapping.size(), 0,
        planeGeneration, launchNonce, token, &captured) ==
        ValidationError::InvalidSnapshot);
    slot.snapshot->flags = AnalogSnapshotFlag_Complete;

    auto* mappingHeader = static_cast<MappingHeaderV1*>(mapping.data());
    const auto savedStride = mappingHeader->slotStride;
    mappingHeader->slotStride += kLayoutAlignment;
    assert(ValidateMapping(mapping.data(), mapping.size()) ==
        ValidationError::InvalidLayout);
    mappingHeader->slotStride = savedStride;

    MutableSlotViewV1 stale{};
    assert(GetMutableSlot(mapping.data(), mapping.size(), 1, &stale) ==
        ValidationError::None);
    assert(ValidateCommittedSlot(mapping.data(), mapping.size(), 1,
        planeGeneration, launchNonce, token, &captured) ==
        ValidationError::UncommittedSlot);

    std::cout << "provider_v2_split_plane_layout=pass"
              << " negotiated_devices=12 negotiated_samples=60"
              << " parent_payload_route_selected=0\n";
    return 0;
}
