#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "analog_provider_v2.h"

namespace halljoy::provider_v2_snapshot_broker
{
constexpr std::uint32_t kBrokerSlotCount = 3;
constexpr std::uint32_t kNoBrokerSlot = UINT32_MAX;

struct SnapshotMetadataV1 final
{
    std::uint64_t planeGeneration = 0;
    std::uint64_t transactionToken = 0;
    std::uint64_t publicationGeneration = 0;
    std::uint64_t publicationTimestampUs = 0;
};

enum class PublishResult : std::uint32_t
{
    Published = 0,
    Unavailable,
    WriterBusy,
    NoFreeSlot,
    GenerationMismatch,
    CapacityMismatch,
    InvalidSnapshot,
    CommitInvalidated,
};

using CommitValidator = bool(*)(void*) noexcept;

// Parent-side immutable handoff between the snapshot bridge and realtime. The
// bridge is the only writer. Prepare may allocate and is therefore permitted
// only while publication is revoked and every reader/writer has drained.
class ParentSnapshotBroker final
{
public:
    class ReadLease final
    {
    public:
        ReadLease() noexcept = default;
        ~ReadLease() noexcept;
        ReadLease(const ReadLease&) = delete;
        ReadLease& operator=(const ReadLease&) = delete;
        ReadLease(ReadLease&& other) noexcept;
        ReadLease& operator=(ReadLease&& other) noexcept;

        explicit operator bool() const noexcept { return owner_ != nullptr; }
        const SnapshotMetadataV1& Metadata() const noexcept;
        const analog_provider_v2::AnalogSnapshotHeaderV2& Header() const noexcept;
        const analog_provider_v2::AnalogDeviceV2* Devices() const noexcept;
        const analog_provider_v2::AnalogSampleV2* Samples() const noexcept;
        std::size_t DeviceCount() const noexcept;
        std::size_t SampleCount() const noexcept;

    private:
        friend class ParentSnapshotBroker;
        ReadLease(ParentSnapshotBroker* owner, std::uint32_t slot) noexcept
            : owner_(owner), slot_(slot) {}
        void Release() noexcept;

        ParentSnapshotBroker* owner_ = nullptr;
        std::uint32_t slot_ = kNoBrokerSlot;
    };

    ParentSnapshotBroker() = default;
    ParentSnapshotBroker(const ParentSnapshotBroker&) = delete;
    ParentSnapshotBroker& operator=(const ParentSnapshotBroker&) = delete;

    // Immediately prevents new acquisitions and makes the former publication
    // unreachable. Existing leases remain valid until released.
    void BeginReconfigure() noexcept;
    bool IsDrained() const noexcept;

    // Strong allocation boundary: all slot vectors are replaced together only
    // after a complete temporary allocation succeeds.
    bool Prepare(std::uint32_t deviceCapacity,
        std::uint32_t sampleCapacity) noexcept;
    bool Activate(std::uint64_t planeGeneration) noexcept;

    PublishResult TryPublish(
        const SnapshotMetadataV1& metadata,
        const analog_provider_v2::AnalogSnapshotHeaderV2& header,
        const analog_provider_v2::AnalogDeviceV2* devices,
        const analog_provider_v2::AnalogSampleV2* samples,
        CommitValidator validator = nullptr,
        void* validatorContext = nullptr) noexcept;
    ReadLease Acquire() noexcept;

    std::uint32_t DeviceCapacity() const noexcept { return deviceCapacity_; }
    std::uint32_t SampleCapacity() const noexcept { return sampleCapacity_; }
    std::uint64_t PlaneGeneration() const noexcept
    {
        return planeGeneration_.load(std::memory_order_acquire);
    }
    bool IsActive() const noexcept
    {
        return active_.load(std::memory_order_acquire);
    }

private:
    struct Slot final
    {
        std::atomic<std::uint32_t> readers{ 0 };
        SnapshotMetadataV1 metadata{};
        analog_provider_v2::AnalogSnapshotHeaderV2 header{};
        std::vector<analog_provider_v2::AnalogDeviceV2> devices;
        std::vector<analog_provider_v2::AnalogSampleV2> samples;
    };

    void ReleaseReader(std::uint32_t slot) noexcept;

    std::array<Slot, kBrokerSlotCount> slots_{};
    std::atomic<bool> active_{ false };
    std::atomic<bool> writerActive_{ false };
    std::atomic<std::uint32_t> publishedSlot_{ kNoBrokerSlot };
    std::atomic<std::uint64_t> planeGeneration_{ 0 };
    std::uint32_t deviceCapacity_ = 0;
    std::uint32_t sampleCapacity_ = 0;
};
}
