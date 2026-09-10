#include "provider_v2_snapshot_broker.h"

#include <algorithm>
#include <utility>

namespace halljoy::provider_v2_snapshot_broker
{
namespace
{
const SnapshotMetadataV1 kEmptyMetadata{};
const analog_provider_v2::AnalogSnapshotHeaderV2 kEmptyHeader{};
}

ParentSnapshotBroker::ReadLease::~ReadLease() noexcept
{
    Release();
}

ParentSnapshotBroker::ReadLease::ReadLease(ReadLease&& other) noexcept
    : owner_(other.owner_), slot_(other.slot_)
{
    other.owner_ = nullptr;
    other.slot_ = kNoBrokerSlot;
}

ParentSnapshotBroker::ReadLease&
ParentSnapshotBroker::ReadLease::operator=(ReadLease&& other) noexcept
{
    if (this != &other)
    {
        Release();
        owner_ = other.owner_;
        slot_ = other.slot_;
        other.owner_ = nullptr;
        other.slot_ = kNoBrokerSlot;
    }
    return *this;
}

void ParentSnapshotBroker::ReadLease::Release() noexcept
{
    if (owner_)
    {
        owner_->ReleaseReader(slot_);
        owner_ = nullptr;
        slot_ = kNoBrokerSlot;
    }
}

const SnapshotMetadataV1&
ParentSnapshotBroker::ReadLease::Metadata() const noexcept
{
    return owner_ ? owner_->slots_[slot_].metadata : kEmptyMetadata;
}

const analog_provider_v2::AnalogSnapshotHeaderV2&
ParentSnapshotBroker::ReadLease::Header() const noexcept
{
    return owner_ ? owner_->slots_[slot_].header : kEmptyHeader;
}

const analog_provider_v2::AnalogDeviceV2*
ParentSnapshotBroker::ReadLease::Devices() const noexcept
{
    return owner_ ? owner_->slots_[slot_].devices.data() : nullptr;
}

const analog_provider_v2::AnalogSampleV2*
ParentSnapshotBroker::ReadLease::Samples() const noexcept
{
    return owner_ ? owner_->slots_[slot_].samples.data() : nullptr;
}

std::size_t ParentSnapshotBroker::ReadLease::DeviceCount() const noexcept
{
    return owner_ ? owner_->slots_[slot_].header.deviceCount : 0;
}

std::size_t ParentSnapshotBroker::ReadLease::SampleCount() const noexcept
{
    return owner_ ? owner_->slots_[slot_].header.sampleCount : 0;
}

void ParentSnapshotBroker::BeginReconfigure() noexcept
{
    active_.store(false, std::memory_order_release);
    publishedSlot_.store(kNoBrokerSlot, std::memory_order_release);
    planeGeneration_.store(0, std::memory_order_release);
}

bool ParentSnapshotBroker::IsDrained() const noexcept
{
    if (writerActive_.load(std::memory_order_acquire))
        return false;
    for (const auto& slot : slots_)
    {
        if (slot.readers.load(std::memory_order_acquire) != 0)
            return false;
    }
    return true;
}

bool ParentSnapshotBroker::Prepare(std::uint32_t deviceCapacity,
    std::uint32_t sampleCapacity) noexcept
{
    if (active_.load(std::memory_order_acquire) || !IsDrained())
        return false;

    try
    {
        std::array<std::vector<analog_provider_v2::AnalogDeviceV2>,
            kBrokerSlotCount> newDevices;
        std::array<std::vector<analog_provider_v2::AnalogSampleV2>,
            kBrokerSlotCount> newSamples;
        for (std::uint32_t i = 0; i < kBrokerSlotCount; ++i)
        {
            newDevices[i].resize(deviceCapacity);
            newSamples[i].resize(sampleCapacity);
        }
        for (std::uint32_t i = 0; i < kBrokerSlotCount; ++i)
        {
            slots_[i].devices.swap(newDevices[i]);
            slots_[i].samples.swap(newSamples[i]);
            slots_[i].metadata = SnapshotMetadataV1{};
            slots_[i].header = analog_provider_v2::AnalogSnapshotHeaderV2{};
        }
    }
    catch (...)
    {
        return false;
    }

    deviceCapacity_ = deviceCapacity;
    sampleCapacity_ = sampleCapacity;
    publishedSlot_.store(kNoBrokerSlot, std::memory_order_release);
    return true;
}

bool ParentSnapshotBroker::Activate(std::uint64_t planeGeneration) noexcept
{
    if (planeGeneration == 0 || active_.load(std::memory_order_acquire) ||
        !IsDrained())
    {
        return false;
    }
    publishedSlot_.store(kNoBrokerSlot, std::memory_order_release);
    planeGeneration_.store(planeGeneration, std::memory_order_release);
    active_.store(true, std::memory_order_release);
    return true;
}

PublishResult ParentSnapshotBroker::TryPublish(
    const SnapshotMetadataV1& metadata,
    const analog_provider_v2::AnalogSnapshotHeaderV2& header,
    const analog_provider_v2::AnalogDeviceV2* devices,
    const analog_provider_v2::AnalogSampleV2* samples,
    CommitValidator validator, void* validatorContext) noexcept
{
    if (!active_.load(std::memory_order_acquire))
        return PublishResult::Unavailable;
    if (metadata.planeGeneration == 0 || metadata.transactionToken == 0 ||
        metadata.publicationGeneration == 0 ||
        metadata.planeGeneration !=
            planeGeneration_.load(std::memory_order_acquire))
    {
        return PublishResult::GenerationMismatch;
    }
    if (header.deviceCount > deviceCapacity_ ||
        header.sampleCount > sampleCapacity_ ||
        (header.deviceCount != 0 && !devices) ||
        (header.sampleCount != 0 && !samples))
    {
        return PublishResult::CapacityMismatch;
    }
    if (!analog_provider_v2::IsAuthoritative(header) ||
        analog_provider_v2::ValidateSnapshot(header, devices, deviceCapacity_,
            samples, sampleCapacity_) !=
            analog_provider_v2::SnapshotValidationError::None)
    {
        return PublishResult::InvalidSnapshot;
    }

    bool expectedWriter = false;
    if (!writerActive_.compare_exchange_strong(expectedWriter, true,
        std::memory_order_acq_rel, std::memory_order_acquire))
    {
        return PublishResult::WriterBusy;
    }

    PublishResult result = PublishResult::Unavailable;
    if (active_.load(std::memory_order_acquire) &&
        metadata.planeGeneration ==
            planeGeneration_.load(std::memory_order_acquire))
    {
        const std::uint32_t current =
            publishedSlot_.load(std::memory_order_acquire);
        std::uint32_t target = kNoBrokerSlot;
        for (std::uint32_t i = 0; i < kBrokerSlotCount; ++i)
        {
            if (i != current &&
                slots_[i].readers.load(std::memory_order_acquire) == 0)
            {
                target = i;
                break;
            }
        }

        if (target == kNoBrokerSlot)
        {
            result = PublishResult::NoFreeSlot;
        }
        else
        {
            auto& slot = slots_[target];
            slot.metadata = metadata;
            slot.header = header;
            if (header.deviceCount != 0)
                std::copy_n(devices, header.deviceCount, slot.devices.begin());
            if (header.sampleCount != 0)
                std::copy_n(samples, header.sampleCount, slot.samples.begin());
            std::atomic_thread_fence(std::memory_order_release);
            if (validator && !validator(validatorContext))
            {
                result = PublishResult::CommitInvalidated;
            }
            else if (active_.load(std::memory_order_acquire) &&
                metadata.planeGeneration ==
                    planeGeneration_.load(std::memory_order_acquire))
            {
                publishedSlot_.store(target, std::memory_order_release);
                result = PublishResult::Published;
            }
        }
    }
    writerActive_.store(false, std::memory_order_release);
    return result;
}

ParentSnapshotBroker::ReadLease ParentSnapshotBroker::Acquire() noexcept
{
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (!active_.load(std::memory_order_acquire))
            return ReadLease{};
        const std::uint32_t slot =
            publishedSlot_.load(std::memory_order_acquire);
        if (slot >= kBrokerSlotCount)
            return ReadLease{};
        slots_[slot].readers.fetch_add(1, std::memory_order_acq_rel);
        if (active_.load(std::memory_order_acquire) &&
            publishedSlot_.load(std::memory_order_acquire) == slot)
        {
            return ReadLease(this, slot);
        }
        slots_[slot].readers.fetch_sub(1, std::memory_order_acq_rel);
    }
    return ReadLease{};
}

void ParentSnapshotBroker::ReleaseReader(std::uint32_t slot) noexcept
{
    if (slot < kBrokerSlotCount)
        slots_[slot].readers.fetch_sub(1, std::memory_order_acq_rel);
}
}
