#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "analog_provider_v2.h"

namespace halljoy::provider_v2_data_plane
{
constexpr std::uint32_t kMagic = 0x32564a48u; // "HJV2"
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kSlotCount = 2;
constexpr std::uint32_t kMaximumDevices = 4096;
constexpr std::uint32_t kMaximumSamples = 1048576;
constexpr std::size_t kLayoutAlignment = 64;

struct alignas(kLayoutAlignment) MappingHeaderV1
{
    std::uint32_t magic = kMagic;
    std::uint32_t version = kVersion;
    std::uint32_t structSize = sizeof(MappingHeaderV1);
    std::uint32_t slotCount = kSlotCount;
    std::uint32_t deviceCapacity = 0;
    std::uint32_t sampleCapacity = 0;
    std::uint32_t deviceElementSize =
        sizeof(analog_provider_v2::AnalogDeviceV2);
    std::uint32_t sampleElementSize =
        sizeof(analog_provider_v2::AnalogSampleV2);
    std::uint64_t mappingBytes = 0;
    std::uint64_t planeGeneration = 0;
    std::uint64_t launchNonce = 0;
    std::uint64_t slotsOffset = 0;
    std::uint64_t slotStride = 0;
    std::uint64_t reserved[7]{};
};

struct alignas(kLayoutAlignment) SlotCommitV1
{
    std::uint32_t structSize = sizeof(SlotCommitV1);
    std::uint32_t snapshotHeaderSize =
        sizeof(analog_provider_v2::AnalogSnapshotHeaderV2);
    std::uint32_t deviceCount = 0;
    std::uint32_t sampleCount = 0;
    alignas(8) volatile std::uint64_t sequence = 0;
    std::uint64_t transactionToken = 0;
    std::uint64_t planeGeneration = 0;
    std::uint64_t launchNonce = 0;
    std::uint64_t devicesOffset = 0;
    std::uint64_t samplesOffset = 0;
};

struct LayoutV1
{
    std::size_t mappingBytes = 0;
    std::size_t slotsOffset = 0;
    std::size_t slotStride = 0;
    std::size_t snapshotHeaderOffset = 0;
    std::size_t devicesOffset = 0;
    std::size_t samplesOffset = 0;
    std::uint32_t deviceCapacity = 0;
    std::uint32_t sampleCapacity = 0;
};

enum class CapacityAction : std::uint32_t
{
    Fits = 0,
    Replace,
    Reject,
};

struct CapacityPlanV1
{
    CapacityAction action = CapacityAction::Reject;
    std::uint32_t deviceCapacity = 0;
    std::uint32_t sampleCapacity = 0;
};

enum class ValidationError : std::uint32_t
{
    None = 0,
    MissingStorage,
    MisalignedStorage,
    InvalidCapacity,
    ArithmeticOverflow,
    InvalidHeader,
    InvalidLayout,
    InvalidSlot,
    UncommittedSlot,
    TransactionMismatch,
    GenerationMismatch,
    InvalidSnapshot,
};

struct MutableSlotViewV1
{
    SlotCommitV1* commit = nullptr;
    analog_provider_v2::AnalogSnapshotHeaderV2* snapshot = nullptr;
    analog_provider_v2::AnalogDeviceV2* devices = nullptr;
    analog_provider_v2::AnalogSampleV2* samples = nullptr;
};

struct ConstSlotViewV1
{
    const SlotCommitV1* commit = nullptr;
    const analog_provider_v2::AnalogSnapshotHeaderV2* snapshot = nullptr;
    const analog_provider_v2::AnalogDeviceV2* devices = nullptr;
    const analog_provider_v2::AnalogSampleV2* samples = nullptr;
    std::uint64_t observedSequence = 0;
};

ValidationError CalculateLayout(
    std::uint32_t deviceCapacity,
    std::uint32_t sampleCapacity,
    LayoutV1* out) noexcept;

CapacityPlanV1 PlanCapacity(
    std::uint32_t currentDeviceCapacity,
    std::uint32_t currentSampleCapacity,
    std::uint32_t requiredDeviceCount,
    std::uint32_t requiredSampleCount) noexcept;

ValidationError InitializeMapping(
    void* mapping,
    std::size_t mappingBytes,
    std::uint64_t planeGeneration,
    std::uint64_t launchNonce,
    std::uint32_t deviceCapacity,
    std::uint32_t sampleCapacity) noexcept;

ValidationError ValidateMapping(
    const void* mapping,
    std::size_t mappingBytes,
    LayoutV1* out = nullptr) noexcept;

ValidationError GetMutableSlot(
    void* mapping,
    std::size_t mappingBytes,
    std::uint32_t slotIndex,
    MutableSlotViewV1* out) noexcept;

ValidationError ValidateCommittedSlot(
    const void* mapping,
    std::size_t mappingBytes,
    std::uint32_t slotIndex,
    std::uint64_t expectedPlaneGeneration,
    std::uint64_t expectedLaunchNonce,
    std::uint64_t expectedTransactionToken,
    ConstSlotViewV1* out) noexcept;

static_assert(sizeof(MappingHeaderV1) == 128);
static_assert(sizeof(SlotCommitV1) == 64);
static_assert(alignof(MappingHeaderV1) == kLayoutAlignment);
static_assert(alignof(SlotCommitV1) == kLayoutAlignment);
static_assert(std::is_standard_layout_v<MappingHeaderV1>);
static_assert(std::is_trivially_copyable_v<MappingHeaderV1>);
static_assert(std::is_standard_layout_v<SlotCommitV1>);
static_assert(std::is_trivially_copyable_v<SlotCommitV1>);
}
