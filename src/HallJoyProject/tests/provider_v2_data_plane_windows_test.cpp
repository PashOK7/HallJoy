#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <cwchar>
#include <string>
#include <vector>

#include "../HallJoy/provider_v2_data_plane_windows.h"

namespace
{
using namespace halljoy::analog_provider_v2;
using namespace halljoy::provider_v2_data_plane;

constexpr std::uint64_t kNonce = 0x6A6F79504C414E45ull;

std::uint64_t ParseU64(const char* text, int base)
{
    if (!text || !*text)
        return 0;
    char* end = nullptr;
    const auto value = std::strtoull(text, &end, base);
    return end != text && *end == '\0'
        ? static_cast<std::uint64_t>(value) : 0;
}

AnalogSnapshotHeaderV2 MakeHeader(std::uint32_t deviceCount,
    std::uint32_t sampleCount)
{
    AnalogSnapshotHeaderV2 header{};
    header.providerId = 0x554150;
    header.providerGeneration = 9;
    header.sampleGeneration = 71;
    header.valueGeneration = 70;
    header.ownershipGeneration = 11;
    header.sampleTimestampUs = 100000;
    header.valueTimestampUs = 99990;
    header.flags = AnalogSnapshotFlag_Complete;
    header.deviceCount = deviceCount;
    header.deviceCapacity = deviceCount;
    header.requiredDeviceCount = deviceCount;
    header.sampleCount = sampleCount;
    header.sampleCapacity = sampleCount;
    header.requiredSampleCount = sampleCount;
    return header;
}

AnalogDeviceV2 MakeDevice(std::uint32_t index)
{
    AnalogDeviceV2 device{};
    device.deviceId = 0x1000u + index;
    device.exactInterfaceId = 0x900000u + index;
    device.flags = AnalogDeviceFlag_Connected | AnalogDeviceFlag_StableIdentity;
    device.vendorId = 0x3434;
    device.productId = 0x0e40;
    device.usagePage = 0xff60;
    device.usage = 0x61;
    return device;
}

AnalogSampleV2 MakeSample(std::uint32_t deviceIndex, std::uint32_t index)
{
    AnalogSampleV2 sample{};
    sample.deviceIndex = deviceIndex;
    sample.key = UsbHidKey(0x07, 4 + (index % 80));
    sample.flags = AnalogSampleFlag_Owned |
        AnalogSampleFlag_ValueValid | AnalogSampleFlag_Fresh;
    sample.value.normalized = static_cast<float>((index % 100) + 1) / 100.0f;
    return sample;
}

int RunChild(int argc, char** argv)
{
    if (argc != 9)
        return 31;
    const std::string mode = argv[1];
    HANDLE mapping = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(
        ParseU64(argv[2], 16)));
    const auto mappingBytes = static_cast<std::size_t>(ParseU64(argv[3], 16));
    const auto generation = ParseU64(argv[4], 16);
    const auto nonce = ParseU64(argv[5], 16);
    const auto token = ParseU64(argv[6], 16);
    const auto deviceCount = static_cast<std::uint32_t>(ParseU64(argv[7], 10));
    const auto sampleCount = static_cast<std::uint32_t>(ParseU64(argv[8], 10));

    if (mode == "invalid")
    {
        ChildPlaneWriter rejected;
        DWORD error = ERROR_SUCCESS;
        return !rejected.Open(mapping, mappingBytes, generation, nonce, &error)
            ? 41 : 42;
    }

    if (mode == "odd")
    {
        void* const view = MapViewOfFile(mapping,
            FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, mappingBytes);
        if (!view)
            return 51;
        LayoutV1 layout{};
        MutableSlotViewV1 slot{};
        const auto* const header = static_cast<const MappingHeaderV1*>(view);
        if (ValidateMapping(view, mappingBytes, &layout) != ValidationError::None ||
            header->planeGeneration != generation ||
            header->launchNonce != nonce ||
            GetMutableSlot(view, mappingBytes, 0, &slot) != ValidationError::None)
        {
            return 52;
        }
        InterlockedExchange64(reinterpret_cast<volatile LONG64*>(
            &slot.commit->sequence), 1);
        MemoryBarrier();
        TerminateProcess(GetCurrentProcess(), 77);
        return 53;
    }

    if (mode != "publish")
        return 32;
    ChildPlaneWriter writer;
    DWORD error = ERROR_SUCCESS;
    if (!writer.Open(mapping, mappingBytes, generation, nonce, &error))
        return 61;
    std::vector<AnalogDeviceV2> devices(deviceCount);
    std::vector<AnalogSampleV2> samples(sampleCount);
    for (std::uint32_t i = 0; i < deviceCount; ++i)
        devices[i] = MakeDevice(i);
    for (std::uint32_t i = 0; i < sampleCount; ++i)
        samples[i] = MakeSample(deviceCount == 0 ? 0 : i % deviceCount, i);
    const auto header = MakeHeader(deviceCount, sampleCount);
    std::uint32_t slot = kSlotCount;
    if (!writer.Publish(header,
        devices.empty() ? nullptr : devices.data(),
        samples.empty() ? nullptr : samples.data(), token, &slot, &error))
    {
        return 62;
    }
    return slot == 0 ? 0 : 63;
}

bool StartChild(const wchar_t* executable, HANDLE inheritedMapping,
    std::uintptr_t mappingArgument, std::size_t mappingBytes,
    std::uint64_t generation, std::uint64_t nonce, std::uint64_t token,
    std::uint32_t devices, std::uint32_t samples, const wchar_t* mode,
    PROCESS_INFORMATION* process)
{
    wchar_t command[2048]{};
    const int length = swprintf_s(command,
        L"\"%s\" %s %llX %llX %llX %llX %llX %u %u",
        executable, mode,
        static_cast<unsigned long long>(mappingArgument),
        static_cast<unsigned long long>(mappingBytes),
        static_cast<unsigned long long>(generation),
        static_cast<unsigned long long>(nonce),
        static_cast<unsigned long long>(token), devices, samples);
    if (length <= 0 || static_cast<std::size_t>(length) >= std::size(command))
        return false;

    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    if (attributeBytes == 0)
        return false;
    std::vector<std::byte> storage(attributeBytes);
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(
        storage.data());
    if (!InitializeProcThreadAttributeList(startup.lpAttributeList,
        1, 0, &attributeBytes))
    {
        return false;
    }
    HANDLE handles[]{ inheritedMapping };
    const bool attributed = UpdateProcThreadAttribute(startup.lpAttributeList,
        0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, sizeof(handles),
        nullptr, nullptr) != FALSE;
    ZeroMemory(process, sizeof(*process));
    const bool created = attributed && CreateProcessW(executable, command,
        nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW,
        nullptr, nullptr, &startup.StartupInfo, process) != FALSE;
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    return created;
}

DWORD WaitChild(PROCESS_INFORMATION* process)
{
    const DWORD wait = WaitForSingleObject(process->hProcess, 5000);
    if (wait != WAIT_OBJECT_0)
    {
        TerminateProcess(process->hProcess, STILL_ACTIVE);
        WaitForSingleObject(process->hProcess, 5000);
    }
    DWORD code = STILL_ACTIVE;
    GetExitCodeProcess(process->hProcess, &code);
    assert(CloseHandle(process->hThread));
    assert(CloseHandle(process->hProcess));
    *process = {};
    return code;
}

void PublishGeneration(const wchar_t* executable, std::uint64_t generation,
    std::uint32_t devices, std::uint32_t samples, DWORD* lastChildPid)
{
    constexpr std::uint64_t tokenBase = 0xB200000000ull;
    const std::uint64_t token = tokenBase + generation;
    ParentPlaneOwner owner;
    DWORD error = ERROR_SUCCESS;
    assert(owner.Prepare(generation, kNonce, devices, samples, &error));
    assert(owner.ParentWriteMappingRejected());
    const HANDLE writer = owner.InheritableWriterHandle();
    assert(writer != nullptr);
    PROCESS_INFORMATION process{};
    assert(StartChild(executable, writer,
        reinterpret_cast<std::uintptr_t>(writer), owner.MappingBytes(),
        generation, kNonce, token, devices, samples, L"publish", &process));
    *lastChildPid = process.dwProcessId;
    owner.CloseWriterHandleInParent();
    assert(owner.InheritableWriterHandle() == nullptr);
    assert(WaitChild(&process) == 0);

    auto lease = owner.AcquireRead();
    assert(lease);
    ConstSlotViewV1 slot{};
    assert(lease.ValidateSlot(0, token, &slot) == ValidationError::None);
    assert(slot.snapshot->deviceCount == devices);
    assert(slot.snapshot->sampleCount == samples);
    if (samples != 0)
        assert(slot.samples[samples - 1].deviceIndex < devices);
    lease = {};
    assert(owner.Retire(1000, &error));
    assert(!owner.OwnsResources());
}

int RunParent()
{
    wchar_t executable[32768]{};
    assert(GetModuleFileNameW(nullptr, executable,
        static_cast<DWORD>(std::size(executable))) != 0);
    // A live reader prevents replacement; after release the same mapping can
    // retire cleanly and no writer capability remains in the parent.
    {
        ParentPlaneOwner owner;
        DWORD error = ERROR_SUCCESS;
        assert(owner.Prepare(1, kNonce, 2, 4, &error));
        auto lease = owner.AcquireRead();
        assert(lease);
        assert(!owner.Retire(0, &error));
        assert(error == ERROR_TIMEOUT && owner.OwnsResources());
        assert(!owner.Prepare(2, kNonce, 4, 8, &error));
        lease = {};
        assert(owner.Retire(1000, &error));
    }

    // A numeric handle not present in the explicit list is rejected by the
    // child even though the real writer capability was inherited. Repeat the
    // path so the first CreateProcess lazy initialization cannot hide a leak.
    DWORD steadyHandleCount = 0;
    for (const std::uint64_t generation : { 2ull, 3ull })
    {
        ParentPlaneOwner owner;
        DWORD error = ERROR_SUCCESS;
        assert(owner.Prepare(generation, kNonce, 1, 2, &error));
        const HANDLE writer = owner.InheritableWriterHandle();
        const auto invalid = reinterpret_cast<std::uintptr_t>(writer) +
            static_cast<std::uintptr_t>(0x100000000ull);
        PROCESS_INFORMATION process{};
        assert(StartChild(executable, writer, invalid, owner.MappingBytes(),
            generation, kNonce, 0xB200000000ull + generation,
            1, 2, L"invalid", &process));
        owner.CloseWriterHandleInParent();
        assert(WaitChild(&process) == 41);
        assert(owner.Retire(1000, &error));
        DWORD count = 0;
        assert(GetProcessHandleCount(GetCurrentProcess(), &count));
        if (steadyHandleCount == 0)
            steadyHandleCount = count;
        else
            assert(count <= steadyHandleCount + 1);
    }

    // A child terminated after the odd commit edge cannot expose a partial
    // payload and releases its only writer handle when the process is reaped.
    {
        ParentPlaneOwner owner;
        DWORD error = ERROR_SUCCESS;
        assert(owner.Prepare(4, kNonce, 1, 2, &error));
        const HANDLE writer = owner.InheritableWriterHandle();
        PROCESS_INFORMATION process{};
        assert(StartChild(executable, writer,
            reinterpret_cast<std::uintptr_t>(writer), owner.MappingBytes(),
            4, kNonce, 0xB200000004ull, 1, 2, L"odd", &process));
        owner.CloseWriterHandleInParent();
        assert(WaitChild(&process) == 77);
        auto lease = owner.AcquireRead();
        ConstSlotViewV1 slot{};
        assert(lease.ValidateSlot(0, 0xB200000004ull, &slot) ==
            ValidationError::UncommittedSlot);
        lease = {};
        assert(owner.Retire(1000, &error));
    }

    DWORD lastChildPid = 0;
    std::uint64_t generation = 10;
    for (const auto devices : { 1u, 8u, 12u, 32u, 8u, 1u })
    {
        PublishGeneration(executable, generation++, devices, devices * 2,
            &lastChildPid);
    }
    assert(lastChildPid != 0);
    HANDLE survivor = OpenProcess(SYNCHRONIZE, FALSE, lastChildPid);
    if (survivor)
    {
        assert(WaitForSingleObject(survivor, 0) == WAIT_OBJECT_0);
        CloseHandle(survivor);
    }
    assert(PlanCapacity(32, 64, kMaximumDevices + 1, 0).action ==
        CapacityAction::Reject);

    DWORD handlesAfter = 0;
    assert(GetProcessHandleCount(GetCurrentProcess(), &handlesAfter));
    assert(handlesAfter <= steadyHandleCount + 1);
    std::cout << "PROVIDER_V2_WINDOWS_PROCESS_TEST=PASS"
              << " explicit_handle_list=1 parent_read_only=1"
              << " invalid_handle_rejected=1 odd_publish_rejected=1"
              << " reader_resize_blocked=1 topology_generations=6"
              << " surviving_writer=0 surviving_child=0\n";
    return 0;
}
}

int main(int argc, char** argv)
{
    return argc > 1 ? RunChild(argc, argv) : RunParent();
}

#else
int main()
{
    return 0;
}
#endif
