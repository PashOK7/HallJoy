#include "provider_v2_data_plane_windows.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace halljoy::provider_v2_data_plane
{
namespace
{
void SetError(DWORD* output, DWORD value) noexcept
{
    if (output)
        *output = value;
    SetLastError(value);
}

bool IsReadOnlyProtection(DWORD protection) noexcept
{
    const DWORD base = protection & 0xffu;
    return base == PAGE_READONLY || base == PAGE_EXECUTE_READ;
}
}

ParentPlaneOwner::ReadLease::~ReadLease() noexcept
{
    Release();
}

ParentPlaneOwner::ReadLease::ReadLease(ReadLease&& other) noexcept
    : owner_(other.owner_)
{
    other.owner_ = nullptr;
}

ParentPlaneOwner::ReadLease& ParentPlaneOwner::ReadLease::operator=(
    ReadLease&& other) noexcept
{
    if (this != &other)
    {
        Release();
        owner_ = other.owner_;
        other.owner_ = nullptr;
    }
    return *this;
}

void ParentPlaneOwner::ReadLease::Release() noexcept
{
    if (owner_)
    {
        owner_->ReleaseReader();
        owner_ = nullptr;
    }
}

const void* ParentPlaneOwner::ReadLease::Mapping() const noexcept
{
    return owner_ ? owner_->readView_ : nullptr;
}

std::size_t ParentPlaneOwner::ReadLease::MappingBytes() const noexcept
{
    return owner_ ? owner_->mappingBytes_ : 0;
}

std::uint64_t ParentPlaneOwner::ReadLease::PlaneGeneration() const noexcept
{
    return owner_ ? owner_->planeGeneration_ : 0;
}

std::uint64_t ParentPlaneOwner::ReadLease::LaunchNonce() const noexcept
{
    return owner_ ? owner_->launchNonce_ : 0;
}

ValidationError ParentPlaneOwner::ReadLease::ValidateSlot(
    std::uint32_t slotIndex, std::uint64_t transactionToken,
    ConstSlotViewV1* out) const noexcept
{
    if (!owner_)
        return ValidationError::MissingStorage;
    return ValidateCommittedSlot(owner_->readView_, owner_->mappingBytes_,
        slotIndex, owner_->planeGeneration_, owner_->launchNonce_,
        transactionToken, out);
}

ParentPlaneOwner::~ParentPlaneOwner() noexcept
{
    DWORD ignored = ERROR_SUCCESS;
    (void)Retire(1000, &ignored);
}

void ParentPlaneOwner::CloseResources() noexcept
{
    if (readView_)
        UnmapViewOfFile(readView_);
    if (readHandle_)
        CloseHandle(readHandle_);
    if (writerHandle_)
        CloseHandle(writerHandle_);
    readView_ = nullptr;
    readHandle_ = nullptr;
    writerHandle_ = nullptr;
    mappingBytes_ = 0;
    planeGeneration_ = 0;
    launchNonce_ = 0;
    layout_ = LayoutV1{};
    parentWriteMappingRejected_ = false;
}

bool ParentPlaneOwner::Prepare(std::uint64_t planeGeneration,
    std::uint64_t launchNonce, std::uint32_t deviceCapacity,
    std::uint32_t sampleCapacity, DWORD* win32Error) noexcept
{
    if (win32Error)
        *win32Error = ERROR_SUCCESS;
    if (OwnsResources() || published_.load(std::memory_order_acquire) ||
        readers_.load(std::memory_order_acquire) != 0 ||
        planeGeneration == 0 || launchNonce == 0)
    {
        SetError(win32Error, ERROR_INVALID_STATE);
        return false;
    }

    LayoutV1 candidate{};
    if (CalculateLayout(deviceCapacity, sampleCapacity, &candidate) !=
        ValidationError::None || candidate.mappingBytes == 0)
    {
        SetError(win32Error, ERROR_INVALID_DATA);
        return false;
    }
    if (candidate.mappingBytes >
        static_cast<std::size_t>((std::numeric_limits<std::uint64_t>::max)()))
    {
        SetError(win32Error, ERROR_ARITHMETIC_OVERFLOW);
        return false;
    }

    const std::uint64_t bytes = static_cast<std::uint64_t>(candidate.mappingBytes);
    HANDLE creator = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE, static_cast<DWORD>(bytes >> 32),
        static_cast<DWORD>(bytes & 0xffffffffu), nullptr);
    if (!creator)
    {
        SetError(win32Error, GetLastError());
        return false;
    }

    void* initializationView = MapViewOfFile(creator,
        FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, candidate.mappingBytes);
    if (!initializationView)
    {
        const DWORD error = GetLastError();
        CloseHandle(creator);
        SetError(win32Error, error);
        return false;
    }
    const auto initialized = InitializeMapping(initializationView,
        candidate.mappingBytes, planeGeneration, launchNonce,
        deviceCapacity, sampleCapacity);
    UnmapViewOfFile(initializationView);
    if (initialized != ValidationError::None)
    {
        CloseHandle(creator);
        SetError(win32Error, ERROR_INVALID_DATA);
        return false;
    }

    HANDLE parentRead = nullptr;
    HANDLE childWrite = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), creator, GetCurrentProcess(),
        &parentRead, FILE_MAP_READ, FALSE, 0) ||
        !DuplicateHandle(GetCurrentProcess(), creator, GetCurrentProcess(),
            &childWrite, FILE_MAP_READ | FILE_MAP_WRITE, TRUE, 0))
    {
        const DWORD error = GetLastError();
        if (parentRead) CloseHandle(parentRead);
        if (childWrite) CloseHandle(childWrite);
        CloseHandle(creator);
        SetError(win32Error, error);
        return false;
    }
    CloseHandle(creator);

    const void* parentView = MapViewOfFile(parentRead, FILE_MAP_READ,
        0, 0, candidate.mappingBytes);
    if (!parentView)
    {
        const DWORD error = GetLastError();
        CloseHandle(childWrite);
        CloseHandle(parentRead);
        SetError(win32Error, error);
        return false;
    }
    LayoutV1 validated{};
    MEMORY_BASIC_INFORMATION memory{};
    const bool layoutValid = ValidateMapping(parentView,
        candidate.mappingBytes, &validated) == ValidationError::None;
    const bool protectionValid = VirtualQuery(parentView, &memory,
        sizeof(memory)) == sizeof(memory) && IsReadOnlyProtection(memory.Protect);
    void* forbiddenWrite = MapViewOfFile(parentRead, FILE_MAP_WRITE,
        0, 0, candidate.mappingBytes);
    const DWORD forbiddenError = forbiddenWrite ? ERROR_SUCCESS : GetLastError();
    if (forbiddenWrite)
        UnmapViewOfFile(forbiddenWrite);
    const bool writeRejected = !forbiddenWrite &&
        forbiddenError == ERROR_ACCESS_DENIED;
    if (!layoutValid || !protectionValid || !writeRejected)
    {
        UnmapViewOfFile(parentView);
        CloseHandle(childWrite);
        CloseHandle(parentRead);
        SetError(win32Error, layoutValid && protectionValid
            ? ERROR_ACCESS_DENIED : ERROR_INVALID_DATA);
        return false;
    }

    readHandle_ = parentRead;
    writerHandle_ = childWrite;
    readView_ = parentView;
    mappingBytes_ = candidate.mappingBytes;
    planeGeneration_ = planeGeneration;
    launchNonce_ = launchNonce;
    layout_ = validated;
    parentWriteMappingRejected_ = true;
    published_.store(true, std::memory_order_release);
    return true;
}

bool ParentPlaneOwner::Retire(DWORD readerDrainTimeoutMs,
    DWORD* win32Error) noexcept
{
    if (win32Error)
        *win32Error = ERROR_SUCCESS;
    published_.store(false, std::memory_order_release);
    const ULONGLONG deadline = GetTickCount64() + readerDrainTimeoutMs;
    while (readers_.load(std::memory_order_acquire) != 0)
    {
        if (readerDrainTimeoutMs == 0 || GetTickCount64() >= deadline)
        {
            SetError(win32Error, ERROR_TIMEOUT);
            return false;
        }
        Sleep(1);
    }
    CloseResources();
    return true;
}

ParentPlaneOwner::ReadLease ParentPlaneOwner::AcquireRead() noexcept
{
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (!published_.load(std::memory_order_acquire))
            return ReadLease{};
        readers_.fetch_add(1, std::memory_order_acq_rel);
        if (published_.load(std::memory_order_acquire) && readView_)
            return ReadLease(this);
        readers_.fetch_sub(1, std::memory_order_acq_rel);
    }
    return ReadLease{};
}

void ParentPlaneOwner::ReleaseReader() noexcept
{
    readers_.fetch_sub(1, std::memory_order_acq_rel);
}

void ParentPlaneOwner::CloseWriterHandleInParent() noexcept
{
    if (writerHandle_)
    {
        CloseHandle(writerHandle_);
        writerHandle_ = nullptr;
    }
}

bool ParentPlaneOwner::OwnsResources() const noexcept
{
    return readHandle_ || writerHandle_ || readView_;
}

ChildPlaneWriter::~ChildPlaneWriter() noexcept
{
    Close();
}

bool ChildPlaneWriter::Open(HANDLE inheritedMapping,
    std::size_t mappingBytes, std::uint64_t expectedPlaneGeneration,
    std::uint64_t expectedLaunchNonce, DWORD* win32Error) noexcept
{
    if (win32Error)
        *win32Error = ERROR_SUCCESS;
    Close();
    mapping_ = inheritedMapping;
    if (!mapping_ || mappingBytes < sizeof(MappingHeaderV1) ||
        expectedPlaneGeneration == 0 || expectedLaunchNonce == 0)
    {
        SetError(win32Error, ERROR_INVALID_PARAMETER);
        Close();
        return false;
    }
    DWORD flags = 0;
    if (!GetHandleInformation(mapping_, &flags))
    {
        const DWORD error = GetLastError();
        Close();
        SetError(win32Error, error);
        return false;
    }
    view_ = MapViewOfFile(mapping_, FILE_MAP_READ | FILE_MAP_WRITE,
        0, 0, mappingBytes);
    if (!view_)
    {
        const DWORD error = GetLastError();
        Close();
        SetError(win32Error, error);
        return false;
    }
    LayoutV1 validated{};
    if (ValidateMapping(view_, mappingBytes, &validated) !=
        ValidationError::None)
    {
        Close();
        SetError(win32Error, ERROR_INVALID_DATA);
        return false;
    }
    const auto& header = *static_cast<const MappingHeaderV1*>(view_);
    if (header.planeGeneration != expectedPlaneGeneration ||
        header.launchNonce != expectedLaunchNonce)
    {
        Close();
        SetError(win32Error, ERROR_INVALID_DATA);
        return false;
    }
    mappingBytes_ = mappingBytes;
    planeGeneration_ = expectedPlaneGeneration;
    launchNonce_ = expectedLaunchNonce;
    layout_ = validated;
    return true;
}

void ChildPlaneWriter::Close() noexcept
{
    if (view_)
        UnmapViewOfFile(view_);
    if (mapping_)
        CloseHandle(mapping_);
    mapping_ = nullptr;
    view_ = nullptr;
    mappingBytes_ = 0;
    planeGeneration_ = 0;
    launchNonce_ = 0;
    layout_ = LayoutV1{};
    slotSequences_[0] = 0;
    slotSequences_[1] = 0;
    nextSlot_ = 0;
}

bool ChildPlaneWriter::Publish(
    const analog_provider_v2::AnalogSnapshotHeaderV2& snapshot,
    const analog_provider_v2::AnalogDeviceV2* devices,
    const analog_provider_v2::AnalogSampleV2* samples,
    std::uint64_t transactionToken, std::uint32_t* committedSlot,
    DWORD* win32Error) noexcept
{
    if (win32Error)
        *win32Error = ERROR_SUCCESS;
    if (committedSlot)
        *committedSlot = kSlotCount;
    if (!view_ || transactionToken == 0 ||
        snapshot.deviceCapacity > layout_.deviceCapacity ||
        snapshot.sampleCapacity > layout_.sampleCapacity ||
        !analog_provider_v2::IsAuthoritative(snapshot) ||
        analog_provider_v2::ValidateSnapshot(snapshot, devices,
            layout_.deviceCapacity, samples, layout_.sampleCapacity) !=
            analog_provider_v2::SnapshotValidationError::None)
    {
        SetError(win32Error, ERROR_INVALID_DATA);
        return false;
    }

    const std::uint32_t slotIndex = nextSlot_++ % kSlotCount;
    MutableSlotViewV1 slot{};
    if (GetMutableSlot(view_, mappingBytes_, slotIndex, &slot) !=
        ValidationError::None)
    {
        SetError(win32Error, ERROR_INVALID_DATA);
        return false;
    }
    std::uint64_t even = slotSequences_[slotIndex] + 2;
    if (even < 2 || (even & 1u) != 0)
        even = 2;
    const std::uint64_t odd = even - 1;
    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(
        &slot.commit->sequence), static_cast<LONG64>(odd));
    MemoryBarrier();
    *slot.snapshot = snapshot;
    if (snapshot.deviceCount != 0)
        std::copy_n(devices, snapshot.deviceCount, slot.devices);
    if (snapshot.sampleCount != 0)
        std::copy_n(samples, snapshot.sampleCount, slot.samples);
    slot.commit->structSize = sizeof(SlotCommitV1);
    slot.commit->snapshotHeaderSize = sizeof(snapshot);
    slot.commit->deviceCount = snapshot.deviceCount;
    slot.commit->sampleCount = snapshot.sampleCount;
    slot.commit->transactionToken = transactionToken;
    slot.commit->planeGeneration = planeGeneration_;
    slot.commit->launchNonce = launchNonce_;
    slot.commit->devicesOffset = layout_.devicesOffset;
    slot.commit->samplesOffset = layout_.samplesOffset;
    MemoryBarrier();
    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(
        &slot.commit->sequence), static_cast<LONG64>(even));
    slotSequences_[slotIndex] = even;

    ConstSlotViewV1 verified{};
    if (ValidateCommittedSlot(view_, mappingBytes_, slotIndex,
        planeGeneration_, launchNonce_, transactionToken, &verified) !=
        ValidationError::None)
    {
        InterlockedExchange64(reinterpret_cast<volatile LONG64*>(
            &slot.commit->sequence), 0);
        SetError(win32Error, ERROR_INVALID_DATA);
        return false;
    }
    if (committedSlot)
        *committedSlot = slotIndex;
    return true;
}
}
