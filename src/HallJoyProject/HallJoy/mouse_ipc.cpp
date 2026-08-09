#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

#include "mouse_ipc.h"
#include "stability_trace.h"

static HANDLE g_mouseIpcMap = nullptr;
static HallJoyMouseIpcShared* g_mouseIpc = nullptr;
static LONG g_lastAsiHeartbeat = 0;
static ULONGLONG g_lastAsiHeartbeatTick = 0;

namespace
{
    LONG AtomicRead(volatile LONG* value) noexcept
    {
        return InterlockedCompareExchange(value, 0, 0);
    }

    void CloseMapping(HANDLE& mapping, HallJoyMouseIpcShared*& shared) noexcept
    {
        if (shared)
        {
            UnmapViewOfFile(shared);
            shared = nullptr;
        }
        if (mapping)
        {
            CloseHandle(mapping);
            mapping = nullptr;
        }
    }

    bool OpenPublisherMapping(
        const wchar_t* name,
        HANDLE& mappingOut,
        HallJoyMouseIpcShared*& sharedOut,
        bool& createdOut) noexcept
    {
        mappingOut = nullptr;
        sharedOut = nullptr;
        createdOut = false;

        HANDLE mapping = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(sizeof(HallJoyMouseIpcShared)),
            name);
        if (!mapping)
            return false;

        // GetLastError belongs to CreateFileMappingW and must be captured before
        // MapViewOfFile or any other Win32 call can overwrite it.
        const DWORD createError = GetLastError();
        const bool created = createError != ERROR_ALREADY_EXISTS;
        auto* shared = static_cast<HallJoyMouseIpcShared*>(MapViewOfFile(
            mapping,
            FILE_MAP_WRITE | FILE_MAP_READ,
            0,
            0,
            sizeof(HallJoyMouseIpcShared)));
        if (!shared)
        {
            CloseHandle(mapping);
            return false;
        }

        if (created)
        {
            ZeroMemory(shared, sizeof(*shared));
            InterlockedExchange(&shared->version, static_cast<LONG>(kHallJoyMouseIpcVersion));
            InterlockedExchange(&shared->structSize, static_cast<LONG>(sizeof(*shared)));
            MemoryBarrier();
            // Publish magic last so an opener never accepts a partial schema.
            InterlockedExchange(&shared->magic, static_cast<LONG>(kHallJoyMouseIpcMagic));
        }
        else
        {
            const LONG magic = AtomicRead(&shared->magic);
            const LONG version = AtomicRead(&shared->version);
            const LONG structSize = AtomicRead(&shared->structSize);
            if (magic != static_cast<LONG>(kHallJoyMouseIpcMagic) ||
                version != static_cast<LONG>(kHallJoyMouseIpcVersion) ||
                (structSize != 0 && structSize != static_cast<LONG>(sizeof(*shared))))
            {
                UnmapViewOfFile(shared);
                CloseHandle(mapping);
                SetLastError(ERROR_REVISION_MISMATCH);
                return false;
            }
            if (structSize == 0)
            {
                // The old v1 ABI called this slot reserved1. Claim it without
                // moving any field so existing ASI binaries remain compatible.
                InterlockedCompareExchange(
                    &shared->structSize,
                    static_cast<LONG>(sizeof(*shared)),
                    0);
            }
        }

        mappingOut = mapping;
        sharedOut = shared;
        createdOut = created;
        return true;
    }

#if defined(HALLJOY_ANALOG_SIMULATOR)
    std::wstring TestMappingName(const wchar_t* suffix)
    {
        wchar_t name[160]{};
        swprintf_s(name, L"Local\\HallJoy_MouseIpcTest_%08lX_%016llX_%ls",
            GetCurrentProcessId(), GetTickCount64(), suffix);
        return name;
    }
#endif
}

bool MouseIpc_InitPublisher()
{
    if (g_mouseIpc) return true;

    bool created = false;
    if (!OpenPublisherMapping(kHallJoyMouseIpcName, g_mouseIpcMap, g_mouseIpc, created))
    {
        StabilityTrace_Write(L"WARN", L"mouse-ipc", L"init.failed",
            L"error=%lu", static_cast<unsigned long>(GetLastError()));
        return false;
    }
    StabilityTrace_Write(L"INFO", L"mouse-ipc", L"init.ok",
        L"created=%d schema_valid=1 size=%zu", created ? 1 : 0, sizeof(*g_mouseIpc));
    return true;
}

void MouseIpc_ShutdownPublisher()
{
    if (g_mouseIpc)
    {
        UnmapViewOfFile(g_mouseIpc);
        g_mouseIpc = nullptr;
    }
    if (g_mouseIpcMap)
    {
        CloseHandle(g_mouseIpcMap);
        g_mouseIpcMap = nullptr;
    }
    g_lastAsiHeartbeat = 0;
    g_lastAsiHeartbeatTick = 0;
}

void MouseIpc_PublishState(bool blockMouseWanted, bool blockMouseActive, bool mouseToStickEnabled, bool pauseByRShift)
{
    if (!g_mouseIpc) return;

    InterlockedExchange(&g_mouseIpc->blockMouseWanted, blockMouseWanted ? 1 : 0);
    InterlockedExchange(&g_mouseIpc->blockMouseActive, blockMouseActive ? 1 : 0);
    InterlockedExchange(&g_mouseIpc->mouseToStickEnabled, mouseToStickEnabled ? 1 : 0);
    InterlockedExchange(&g_mouseIpc->pauseByRShift, pauseByRShift ? 1 : 0);
    InterlockedIncrement(&g_mouseIpc->heartbeat);
}

bool MouseIpc_IsAsiConnected()
{
    if (!g_mouseIpc) return false;
    if (AtomicRead(&g_mouseIpc->asiAttached) == 0) return false;

    LONG hb = AtomicRead(&g_mouseIpc->asiHeartbeat);
    ULONGLONG now = GetTickCount64();
    if (hb != g_lastAsiHeartbeat)
    {
        g_lastAsiHeartbeat = hb;
        g_lastAsiHeartbeatTick = now;
        return true;
    }
    return (g_lastAsiHeartbeatTick != 0 && (now - g_lastAsiHeartbeatTick) <= 1500);
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
bool MouseIpc_RunPolicySelfTest()
{
    HANDLE ownerMapping = nullptr;
    HallJoyMouseIpcShared* ownerShared = nullptr;
    HANDLE existingMapping = nullptr;
    HallJoyMouseIpcShared* existingShared = nullptr;
    bool ownerCreated = false;
    bool existingCreated = true;
    const std::wstring validName = TestMappingName(L"valid");

    bool passed = OpenPublisherMapping(validName.c_str(), ownerMapping, ownerShared, ownerCreated) &&
        ownerCreated && ownerShared != nullptr;
    if (passed)
    {
        InterlockedExchange(&ownerShared->blockMouseWanted, 0x13572468);
        InterlockedExchange(&ownerShared->asiHeartbeat, 77);
        InterlockedExchange(&ownerShared->structSize, 0); // legacy v1 reserved1
        passed = OpenPublisherMapping(
            validName.c_str(), existingMapping, existingShared, existingCreated) &&
            !existingCreated && existingShared != nullptr &&
            AtomicRead(&existingShared->blockMouseWanted) == 0x13572468 &&
            AtomicRead(&existingShared->asiHeartbeat) == 77 &&
            AtomicRead(&existingShared->structSize) == kHallJoyMouseIpcStructSize;
    }
    CloseMapping(existingMapping, existingShared);
    CloseMapping(ownerMapping, ownerShared);

    HANDLE invalidOwner = nullptr;
    HallJoyMouseIpcShared* invalidShared = nullptr;
    HANDLE rejectedMapping = nullptr;
    HallJoyMouseIpcShared* rejectedShared = nullptr;
    bool invalidCreated = false;
    bool rejectedCreated = true;
    const std::wstring invalidName = TestMappingName(L"invalid");
    passed = passed && OpenPublisherMapping(
        invalidName.c_str(), invalidOwner, invalidShared, invalidCreated) && invalidCreated;
    if (invalidShared)
    {
        InterlockedExchange(&invalidShared->blockMouseWanted, 0x24681357);
        InterlockedExchange(&invalidShared->magic, 0x11111111);
        const bool unexpectedlyOpened = OpenPublisherMapping(
            invalidName.c_str(), rejectedMapping, rejectedShared, rejectedCreated);
        passed = passed && !unexpectedlyOpened && rejectedMapping == nullptr &&
            rejectedShared == nullptr &&
            AtomicRead(&invalidShared->blockMouseWanted) == 0x24681357;
    }
    else
    {
        passed = false;
    }
    CloseMapping(rejectedMapping, rejectedShared);
    CloseMapping(invalidOwner, invalidShared);
    return passed;
}
#endif
