#include "provider_v2_data_plane_layout.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>

namespace halljoy::provider_v2_data_plane
{
namespace
{
bool CheckedAdd(std::size_t left, std::size_t right,
    std::size_t* out) noexcept
{
    if (!out || right > (std::numeric_limits<std::size_t>::max)() - left)
        return false;
    *out = left + right;
    return true;
}

bool CheckedMultiply(std::size_t left, std::size_t right,
    std::size_t* out) noexcept
{
    if (!out || (left != 0 &&
        right > (std::numeric_limits<std::size_t>::max)() / left))
    {
        return false;
    }
    *out = left * right;
    return true;
}

bool CheckedAlign(std::size_t value, std::size_t alignment,
    std::size_t* out) noexcept
{
    if (!out || alignment == 0 || (alignment & (alignment - 1)) != 0)
        return false;
    std::size_t expanded = 0;
    if (!CheckedAdd(value, alignment - 1, &expanded))
        return false;
    *out = expanded & ~(alignment - 1);
    return true;
}

std::byte* Bytes(void* pointer) noexcept
{
    return static_cast<std::byte*>(pointer);
}

const std::byte* Bytes(const void* pointer) noexcept
{
    return static_cast<const std::byte*>(pointer);
}
}

ValidationError CalculateLayout(std::uint32_t deviceCapacity,
    std::uint32_t sampleCapacity, LayoutV1* out) noexcept
{
    if (!out)
        return ValidationError::MissingStorage;
    *out = LayoutV1{};
    if (deviceCapacity > kMaximumDevices || sampleCapacity > kMaximumSamples)
        return ValidationError::InvalidCapacity;

    LayoutV1 result{};
    result.deviceCapacity = deviceCapacity;
    result.sampleCapacity = sampleCapacity;
    result.slotsOffset = sizeof(MappingHeaderV1);
    result.snapshotHeaderOffset = sizeof(SlotCommitV1);

    std::size_t cursor = 0;
    if (!CheckedAdd(result.snapshotHeaderOffset,
        sizeof(analog_provider_v2::AnalogSnapshotHeaderV2), &cursor) ||
        !CheckedAlign(cursor, alignof(analog_provider_v2::AnalogDeviceV2),
            &result.devicesOffset))
    {
        return ValidationError::ArithmeticOverflow;
    }

    std::size_t deviceBytes = 0;
    if (!CheckedMultiply(deviceCapacity,
        sizeof(analog_provider_v2::AnalogDeviceV2), &deviceBytes) ||
        !CheckedAdd(result.devicesOffset, deviceBytes, &cursor) ||
        !CheckedAlign(cursor, alignof(analog_provider_v2::AnalogSampleV2),
            &result.samplesOffset))
    {
        return ValidationError::ArithmeticOverflow;
    }

    std::size_t sampleBytes = 0;
    if (!CheckedMultiply(sampleCapacity,
        sizeof(analog_provider_v2::AnalogSampleV2), &sampleBytes) ||
        !CheckedAdd(result.samplesOffset, sampleBytes, &cursor) ||
        !CheckedAlign(cursor, kLayoutAlignment, &result.slotStride))
    {
        return ValidationError::ArithmeticOverflow;
    }

    std::size_t slotsBytes = 0;
    if (!CheckedMultiply(result.slotStride, kSlotCount, &slotsBytes) ||
        !CheckedAdd(result.slotsOffset, slotsBytes, &result.mappingBytes))
    {
        return ValidationError::ArithmeticOverflow;
    }
    *out = result;
    return ValidationError::None;
}

CapacityPlanV1 PlanCapacity(std::uint32_t currentDeviceCapacity,
    std::uint32_t currentSampleCapacity,
    std::uint32_t requiredDeviceCount,
    std::uint32_t requiredSampleCount) noexcept
{
    CapacityPlanV1 result{};
    if (currentDeviceCapacity > kMaximumDevices ||
        currentSampleCapacity > kMaximumSamples ||
        requiredDeviceCount > kMaximumDevices ||
        requiredSampleCount > kMaximumSamples ||
        (requiredDeviceCount == 0 && requiredSampleCount != 0))
    {
        return result;
    }

    result.deviceCapacity = (std::max)(
        currentDeviceCapacity, requiredDeviceCount);
    result.sampleCapacity = (std::max)(
        currentSampleCapacity, requiredSampleCount);
    result.action = requiredDeviceCount <= currentDeviceCapacity &&
        requiredSampleCount <= currentSampleCapacity
        ? CapacityAction::Fits
        : CapacityAction::Replace;
    return result;
}

ValidationError InitializeMapping(void* mapping, std::size_t mappingBytes,
    std::uint64_t planeGeneration, std::uint64_t launchNonce,
    std::uint32_t deviceCapacity, std::uint32_t sampleCapacity) noexcept
{
    if (!mapping)
        return ValidationError::MissingStorage;
    if ((reinterpret_cast<std::uintptr_t>(mapping) &
        (kLayoutAlignment - 1)) != 0)
    {
        return ValidationError::MisalignedStorage;
    }
    if (planeGeneration == 0 || launchNonce == 0)
        return ValidationError::GenerationMismatch;
    LayoutV1 layout{};
    const auto calculated = CalculateLayout(
        deviceCapacity, sampleCapacity, &layout);
    if (calculated != ValidationError::None)
        return calculated;
    if (mappingBytes != layout.mappingBytes)
        return ValidationError::InvalidLayout;

    auto* header = reinterpret_cast<MappingHeaderV1*>(mapping);
    *header = MappingHeaderV1{};
    header->deviceCapacity = deviceCapacity;
    header->sampleCapacity = sampleCapacity;
    header->mappingBytes = layout.mappingBytes;
    header->planeGeneration = planeGeneration;
    header->launchNonce = launchNonce;
    header->slotsOffset = layout.slotsOffset;
    header->slotStride = layout.slotStride;

    for (std::uint32_t index = 0; index < kSlotCount; ++index)
    {
        auto* commit = reinterpret_cast<SlotCommitV1*>(
            Bytes(mapping) + layout.slotsOffset +
            static_cast<std::size_t>(index) * layout.slotStride);
        *commit = SlotCommitV1{};
        commit->planeGeneration = planeGeneration;
        commit->launchNonce = launchNonce;
        commit->devicesOffset = layout.devicesOffset;
        commit->samplesOffset = layout.samplesOffset;
    }
    return ValidationError::None;
}

ValidationError ValidateMapping(const void* mapping, std::size_t mappingBytes,
    LayoutV1* out) noexcept
{
    if (out)
        *out = LayoutV1{};
    if (!mapping || mappingBytes < sizeof(MappingHeaderV1))
        return ValidationError::MissingStorage;
    if ((reinterpret_cast<std::uintptr_t>(mapping) &
        (kLayoutAlignment - 1)) != 0)
    {
        return ValidationError::MisalignedStorage;
    }
    const auto& header = *reinterpret_cast<const MappingHeaderV1*>(mapping);
    if (header.magic != kMagic || header.version != kVersion ||
        header.structSize != sizeof(MappingHeaderV1) ||
        header.slotCount != kSlotCount || header.mappingBytes != mappingBytes ||
        header.planeGeneration == 0 || header.launchNonce == 0 ||
        header.deviceElementSize !=
            sizeof(analog_provider_v2::AnalogDeviceV2) ||
        header.sampleElementSize !=
            sizeof(analog_provider_v2::AnalogSampleV2))
    {
        return ValidationError::InvalidHeader;
    }
    for (const auto reserved : header.reserved)
    {
        if (reserved != 0)
            return ValidationError::InvalidHeader;
    }

    LayoutV1 layout{};
    const auto calculated = CalculateLayout(
        header.deviceCapacity, header.sampleCapacity, &layout);
    if (calculated != ValidationError::None)
        return calculated;
    if (layout.mappingBytes != mappingBytes ||
        header.slotsOffset != layout.slotsOffset ||
        header.slotStride != layout.slotStride)
    {
        return ValidationError::InvalidLayout;
    }
    if (out)
        *out = layout;
    return ValidationError::None;
}

ValidationError GetMutableSlot(void* mapping, std::size_t mappingBytes,
    std::uint32_t slotIndex, MutableSlotViewV1* out) noexcept
{
    if (!out)
        return ValidationError::MissingStorage;
    *out = MutableSlotViewV1{};
    LayoutV1 layout{};
    const auto valid = ValidateMapping(mapping, mappingBytes, &layout);
    if (valid != ValidationError::None)
        return valid;
    if (slotIndex >= kSlotCount)
        return ValidationError::InvalidSlot;
    std::byte* const slot = Bytes(mapping) + layout.slotsOffset +
        static_cast<std::size_t>(slotIndex) * layout.slotStride;
    out->commit = reinterpret_cast<SlotCommitV1*>(slot);
    out->snapshot = reinterpret_cast<
        analog_provider_v2::AnalogSnapshotHeaderV2*>(
        slot + layout.snapshotHeaderOffset);
    out->devices = reinterpret_cast<analog_provider_v2::AnalogDeviceV2*>(
        slot + layout.devicesOffset);
    out->samples = reinterpret_cast<analog_provider_v2::AnalogSampleV2*>(
        slot + layout.samplesOffset);
    return ValidationError::None;
}

ValidationError ValidateCommittedSlot(const void* mapping,
    std::size_t mappingBytes, std::uint32_t slotIndex,
    std::uint64_t expectedPlaneGeneration, std::uint64_t expectedLaunchNonce,
    std::uint64_t expectedTransactionToken, ConstSlotViewV1* out) noexcept
{
    if (!out)
        return ValidationError::MissingStorage;
    *out = ConstSlotViewV1{};
    LayoutV1 layout{};
    const auto valid = ValidateMapping(mapping, mappingBytes, &layout);
    if (valid != ValidationError::None)
        return valid;
    if (slotIndex >= kSlotCount)
        return ValidationError::InvalidSlot;
    if (expectedPlaneGeneration == 0 || expectedLaunchNonce == 0)
        return ValidationError::GenerationMismatch;
    if (expectedTransactionToken == 0)
        return ValidationError::TransactionMismatch;

    const std::byte* const slot = Bytes(mapping) + layout.slotsOffset +
        static_cast<std::size_t>(slotIndex) * layout.slotStride;
    const auto* commit = reinterpret_cast<const SlotCommitV1*>(slot);
    const std::uint64_t before = commit->sequence;
    if (before == 0 || (before & 1u) != 0)
        return ValidationError::UncommittedSlot;
    std::atomic_thread_fence(std::memory_order_acquire);
    if (commit->structSize != sizeof(SlotCommitV1) ||
        commit->snapshotHeaderSize !=
            sizeof(analog_provider_v2::AnalogSnapshotHeaderV2) ||
        commit->devicesOffset != layout.devicesOffset ||
        commit->samplesOffset != layout.samplesOffset ||
        commit->deviceCount > layout.deviceCapacity ||
        commit->sampleCount > layout.sampleCapacity)
    {
        return ValidationError::InvalidSlot;
    }
    if (commit->planeGeneration != expectedPlaneGeneration ||
        commit->launchNonce != expectedLaunchNonce)
    {
        return ValidationError::GenerationMismatch;
    }
    if (commit->transactionToken != expectedTransactionToken)
        return ValidationError::TransactionMismatch;

    const auto* snapshot = reinterpret_cast<const
        analog_provider_v2::AnalogSnapshotHeaderV2*>(
        slot + layout.snapshotHeaderOffset);
    const auto* devices = reinterpret_cast<const
        analog_provider_v2::AnalogDeviceV2*>(slot + layout.devicesOffset);
    const auto* samples = reinterpret_cast<const
        analog_provider_v2::AnalogSampleV2*>(slot + layout.samplesOffset);
    if (snapshot->deviceCount != commit->deviceCount ||
        snapshot->sampleCount != commit->sampleCount ||
        snapshot->deviceCapacity > layout.deviceCapacity ||
        snapshot->sampleCapacity > layout.sampleCapacity ||
        !analog_provider_v2::IsAuthoritative(*snapshot) ||
        analog_provider_v2::ValidateSnapshot(*snapshot, devices,
            layout.deviceCapacity, samples, layout.sampleCapacity) !=
            analog_provider_v2::SnapshotValidationError::None)
    {
        return ValidationError::InvalidSnapshot;
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    const std::uint64_t after = commit->sequence;
    if (before != after || (after & 1u) != 0)
        return ValidationError::UncommittedSlot;

    out->commit = commit;
    out->snapshot = snapshot;
    out->devices = devices;
    out->samples = samples;
    out->observedSequence = after;
    return ValidationError::None;
}
}
