#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "provider_v2_data_plane_layout.h"

namespace halljoy::provider_v2_data_plane
{
class ParentPlaneOwner final
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
        const void* Mapping() const noexcept;
        std::size_t MappingBytes() const noexcept;
        std::uint64_t PlaneGeneration() const noexcept;
        std::uint64_t LaunchNonce() const noexcept;
        ValidationError ValidateSlot(std::uint32_t slotIndex,
            std::uint64_t transactionToken, ConstSlotViewV1* out) const noexcept;

    private:
        friend class ParentPlaneOwner;
        explicit ReadLease(ParentPlaneOwner* owner) noexcept : owner_(owner) {}
        void Release() noexcept;
        ParentPlaneOwner* owner_ = nullptr;
    };

    ParentPlaneOwner() noexcept = default;
    ~ParentPlaneOwner() noexcept;
    ParentPlaneOwner(const ParentPlaneOwner&) = delete;
    ParentPlaneOwner& operator=(const ParentPlaneOwner&) = delete;

    bool Prepare(std::uint64_t planeGeneration, std::uint64_t launchNonce,
        std::uint32_t deviceCapacity, std::uint32_t sampleCapacity,
        DWORD* win32Error = nullptr) noexcept;
    bool Retire(DWORD readerDrainTimeoutMs,
        DWORD* win32Error = nullptr) noexcept;
    ReadLease AcquireRead() noexcept;

    HANDLE InheritableWriterHandle() const noexcept { return writerHandle_; }
    void CloseWriterHandleInParent() noexcept;
    bool OwnsResources() const noexcept;
    bool IsPublished() const noexcept
    {
        return published_.load(std::memory_order_acquire);
    }
    bool ParentWriteMappingRejected() const noexcept
    {
        return parentWriteMappingRejected_;
    }
    std::size_t MappingBytes() const noexcept { return mappingBytes_; }
    std::uint64_t PlaneGeneration() const noexcept { return planeGeneration_; }
    std::uint64_t LaunchNonce() const noexcept { return launchNonce_; }
    const LayoutV1& Layout() const noexcept { return layout_; }

private:
    void ReleaseReader() noexcept;
    void CloseResources() noexcept;

    std::atomic<bool> published_{ false };
    std::atomic<std::uint32_t> readers_{ 0 };
    HANDLE readHandle_ = nullptr;
    HANDLE writerHandle_ = nullptr;
    const void* readView_ = nullptr;
    std::size_t mappingBytes_ = 0;
    std::uint64_t planeGeneration_ = 0;
    std::uint64_t launchNonce_ = 0;
    LayoutV1 layout_{};
    bool parentWriteMappingRejected_ = false;
};

class ChildPlaneWriter final
{
public:
    ChildPlaneWriter() noexcept = default;
    ~ChildPlaneWriter() noexcept;
    ChildPlaneWriter(const ChildPlaneWriter&) = delete;
    ChildPlaneWriter& operator=(const ChildPlaneWriter&) = delete;

    bool Open(HANDLE inheritedMapping, std::size_t mappingBytes,
        std::uint64_t expectedPlaneGeneration,
        std::uint64_t expectedLaunchNonce,
        DWORD* win32Error = nullptr) noexcept;
    void Close() noexcept;
    bool Publish(const analog_provider_v2::AnalogSnapshotHeaderV2& snapshot,
        const analog_provider_v2::AnalogDeviceV2* devices,
        const analog_provider_v2::AnalogSampleV2* samples,
        std::uint64_t transactionToken, std::uint32_t* committedSlot,
        DWORD* win32Error = nullptr) noexcept;

    bool IsOpen() const noexcept { return view_ != nullptr; }
    std::uint64_t PlaneGeneration() const noexcept { return planeGeneration_; }
    std::uint64_t LaunchNonce() const noexcept { return launchNonce_; }
    const LayoutV1& Layout() const noexcept { return layout_; }

private:
    HANDLE mapping_ = nullptr;
    void* view_ = nullptr;
    std::size_t mappingBytes_ = 0;
    std::uint64_t planeGeneration_ = 0;
    std::uint64_t launchNonce_ = 0;
    LayoutV1 layout_{};
    std::uint64_t slotSequences_[kSlotCount]{};
    std::uint32_t nextSlot_ = 0;
};
}
