#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <dbghelp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "analog_host_client.h"
#include "analog_host_shared.h"
#include "debug_log.h"
#include "embedded_analog_stack.h"
#include "realtime_loop.h"
#include "stability_trace.h"
#include "worker_exception_barrier.h"
#include "worker_lifecycle.h"
#include "windows_command_line.h"

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shell32.lib")

namespace
{
    using namespace HallJoyAnalogHost;

    constexpr wchar_t kHostArg[] = L"--halljoy-analog-host";
    constexpr DWORD kHostReadyTimeoutMs = 12000;
    constexpr DWORD kHeartbeatTimeoutMs = 2000;
    constexpr DWORD kRestartDelayMs = 750;
    constexpr ULONGLONG kHostStatsIntervalMs = 10000;
#if defined(HALLJOY_DIAGNOSTIC)
    constexpr bool kUseChildDebugger = true;
#else
    // Production builds supervise the isolated plugin host as a normal child.
    // Attaching a debugger to every child start is unnecessary for normal use,
    // complicates shutdown, and is a common heuristic signal for AV products.
    constexpr bool kUseChildDebugger = false;
#endif

    struct ClientState
    {
        SRWLOCK lock = SRWLOCK_INIT;
        HANDLE mapping = nullptr;
        SharedState* shared = nullptr;
        HANDLE stopEvent = nullptr;
        HANDLE snapshotEvent = nullptr;
        HANDLE snapshotBridgeThread = nullptr;
        HANDLE supervisorThread = nullptr;
        HANDLE supervisorReadyEvent = nullptr;
        HANDLE job = nullptr;
        DWORD ownerPid = 0;
        unsigned long long nonce = 0;
        std::wstring mappingName;
        std::wstring stopEventName;
        std::wstring snapshotEventName;
        std::wstring privatePluginPath;
        std::atomic<bool> stopping{ false };
        std::atomic<bool> restartBlocked{ false };
        bool injectSupervisorCppFault = false;
        bool injectChildReapTimeout = false;
        halljoy::lifecycle::WorkerLifecycle lifecycle;
    };

    ClientState g_client;
    std::atomic<SharedState*> g_hostFaultShared{ nullptr };
    std::atomic<HANDLE> g_hostFaultSnapshotEvent{ nullptr };

    struct TelemetryRateState
    {
        SRWLOCK lock = SRWLOCK_INIT;
        ULONGLONG lastSampleMs = 0;
        std::uint64_t lastPolls = 0;
        std::uint64_t lastSuccessful = 0;
        std::uint32_t pollHz10 = 0;
        std::uint32_t successfulHz10 = 0;
    };

    TelemetryRateState g_telemetryRate;

    struct DeviceInfoCache
    {
        SRWLOCK lock = SRWLOCK_INIT;
        std::array<WootingAnalog_DeviceInfo_FFI, kMaxDevices> info{};
        std::array<std::array<char, 48>, kMaxDevices> manufacturers{};
        std::array<std::array<char, 80>, kMaxDevices> names{};
    };

    DeviceInfoCache g_deviceInfoCache;

    std::wstring BuildPathNearExe(const wchar_t* fileName)
    {
        std::vector<wchar_t> buf(1024);
        for (;;)
        {
            DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
            if (len == 0 || len >= 65535)
                return fileName ? fileName : L"";
            if (len < buf.size())
            {
                std::wstring path(buf.data(), len);
                const size_t slash = path.find_last_of(L"\\/");
                if (slash != std::wstring::npos)
                    path.erase(slash + 1);
                else
                    path.clear();
                if (fileName)
                    path += fileName;
                return path;
            }
            buf.resize(buf.size() * 2);
        }
    }

    void DeleteFilesNearExe(const wchar_t* pattern)
    {
        WIN32_FIND_DATAW data{};
        const std::wstring search = BuildPathNearExe(pattern);
        HANDLE find = FindFirstFileW(search.c_str(), &data);
        if (find == INVALID_HANDLE_VALUE)
            return;
        do
        {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                DeleteFileW(BuildPathNearExe(data.cFileName).c_str());
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }

    std::wstring BuildIpcName(const wchar_t* kind, DWORD ownerPid, unsigned long long nonce)
    {
        wchar_t text[192]{};
        _snwprintf_s(text, _countof(text), _TRUNCATE,
            L"Local\\HallJoyAnalog%s_%lu_%016llX",
            kind ? kind : L"Ipc", ownerPid, nonce);
        return text;
    }

    const wchar_t* CheckpointName(LONG checkpoint)
    {
        switch (checkpoint)
        {
        case Checkpoint_ProcessStart: return L"process_start";
        case Checkpoint_OpenSharedMemory: return L"open_shared_memory";
        case Checkpoint_LoadSdk: return L"load_private_plugin";
        case Checkpoint_ResolveExports: return L"resolve_plugin_exports";
        case Checkpoint_SdkInitialise: return L"plugin_initialise";
        case Checkpoint_SetKeycodeMode: return L"set_keycode_mode";
        case Checkpoint_WaitForPoll: return L"wait_for_snapshot_update";
        case Checkpoint_BeforeReadFullBuffer: return L"before_read_full_buffer";
        case Checkpoint_AfterReadFullBuffer: return L"after_read_full_buffer";
        case Checkpoint_ValidateSnapshot: return L"validate_snapshot";
        case Checkpoint_PublishSnapshot: return L"publish_snapshot";
        case Checkpoint_SdkUninitialise: return L"plugin_unload";
        case Checkpoint_ProcessExit: return L"process_exit";
        case Checkpoint_PluginReadEntry: return L"plugin_read_entry";
        case Checkpoint_PluginBeforeDeviceLock: return L"plugin_before_device_lock";
        case Checkpoint_PluginBeforeKeyboardUpdate: return L"plugin_before_keyboard_update";
        case Checkpoint_PluginAfterKeyboardUpdate: return L"plugin_after_keyboard_update";
        case Checkpoint_PluginReadReturn: return L"plugin_read_return";
        case Checkpoint_MadlionsEntry: return L"madlions_entry";
        case Checkpoint_MadlionsBeforeMutex: return L"madlions_before_mutex";
        case Checkpoint_MadlionsTransportBegin: return L"madlions_transport_begin";
        case Checkpoint_MadlionsReadArm: return L"madlions_read_arm";
        case Checkpoint_MadlionsStaleDiscarded: return L"madlions_stale_report_discarded";
        case Checkpoint_MadlionsReadPending: return L"madlions_read_pending";
        case Checkpoint_MadlionsWriteBegin: return L"madlions_write_begin";
        case Checkpoint_MadlionsWriteWait: return L"madlions_write_wait";
        case Checkpoint_MadlionsWriteComplete: return L"madlions_write_complete";
        case Checkpoint_MadlionsReadWait: return L"madlions_read_wait";
        case Checkpoint_MadlionsReadComplete: return L"madlions_read_complete";
        case Checkpoint_MadlionsCancelRead: return L"madlions_cancel_read";
        case Checkpoint_MadlionsCancelWrite: return L"madlions_cancel_write";
        case Checkpoint_MadlionsTransportReturn: return L"madlions_transport_return";
        case Checkpoint_MadlionsTransportFailed: return L"madlions_transport_failed";
        case Checkpoint_MadlionsAfterTransaction: return L"madlions_after_transaction";
        case Checkpoint_MadlionsParse: return L"madlions_parse";
        case Checkpoint_MadlionsReturn: return L"madlions_return";
        default: return L"none_or_unknown";
        }
    }

    void HostSetCheckpoint(SharedState* shared, Checkpoint checkpoint)
    {
        if (shared)
            InterlockedExchange(&shared->checkpoint, static_cast<LONG>(checkpoint));
    }

    std::uint64_t HostNowUs()
    {
        static LARGE_INTEGER frequency = []() {
            LARGE_INTEGER value{};
            QueryPerformanceFrequency(&value);
            return value;
        }();
        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        if (frequency.QuadPart <= 0)
            return static_cast<std::uint64_t>(GetTickCount64()) * 1000ull;
        const std::uint64_t ticks = static_cast<std::uint64_t>(counter.QuadPart);
        const std::uint64_t hz = static_cast<std::uint64_t>(frequency.QuadPart);
        return (ticks / hz) * 1000000ull + ((ticks % hz) * 1000000ull) / hz;
    }

    void WriteUtf8(HANDLE file, const wchar_t* line)
    {
        if (file == INVALID_HANDLE_VALUE || !line)
            return;
        const int chars = static_cast<int>(wcslen(line));
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, line, chars, nullptr, 0, nullptr, nullptr);
        if (bytes > 0)
        {
            std::string text(static_cast<size_t>(bytes), '\0');
            WideCharToMultiByte(CP_UTF8, 0, line, chars, text.data(), bytes, nullptr, nullptr);
            DWORD written = 0;
            WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        }
        static const char crlf[] = "\r\n";
        DWORD written = 0;
        WriteFile(file, crlf, 2, &written, nullptr);
    }

    void HostLog(const wchar_t* fmt, ...)
    {
#if !defined(HALLJOY_DIAGNOSTIC)
        // Production telemetry is kept in shared memory for the in-app debug
        // panels. The isolated host must not perform synchronous per-poll file
        // I/O or create HallJoyAnalogHost.log.
        (void)fmt;
        return;
#else
        wchar_t message[1600]{};
        va_list ap;
        va_start(ap, fmt);
        _vsnwprintf_s(message, _countof(message), _TRUNCATE, fmt, ap);
        va_end(ap);

        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t line[1900]{};
        _snwprintf_s(line, _countof(line), _TRUNCATE,
            L"[%02u:%02u:%02u.%03u][p%lu:t%lu] %s",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            GetCurrentProcessId(), GetCurrentThreadId(), message);

        HANDLE file = CreateFileW(BuildPathNearExe(L"HallJoyAnalogHost.log").c_str(),
            FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            WriteUtf8(file, line);
            FlushFileBuffers(file);
            CloseHandle(file);
        }
#endif
    }

    // Direct ABI1 plugin interface. HallJoy deliberately bypasses the Wooting
    // SDK dynamic-plugin wrapper on this path; that removes one FFI/function-
    // pointer layer from every poll. The plugin still runs only in the child.
    using PluginDeviceId = std::uint64_t;
    using PluginEventHandler = void(__cdecl*)(void*, int, void*);

    struct HostApi
    {
        HMODULE module = nullptr;
        const char* (__cdecl* name)() = nullptr;
        int (__cdecl* initialise)(void*, PluginEventHandler) = nullptr;
        bool (__cdecl* isInitialised)() = nullptr;
        void (__cdecl* unload)() = nullptr;
        int (__cdecl* readFullBuffer)(unsigned short*, float*, unsigned int, PluginDeviceId) = nullptr;
        std::uint32_t (__cdecl* getDeviceTelemetry)(HallJoyPluginTelemetry::DeviceV1*, std::uint32_t, std::uint32_t) = nullptr;
        std::uint32_t (__cdecl* getDenseSnapshots)(HallJoyDenseSnapshot::DeviceV1*, std::uint32_t, std::uint32_t) = nullptr;
        std::uint64_t (__cdecl* waitForSnapshotUpdate)(std::uint64_t, std::uint32_t) = nullptr;
        void (__cdecl* setDiagnosticCheckpoint)(volatile LONG*) = nullptr;
        void (__cdecl* setDiagnosticTransportError)(volatile LONG*) = nullptr;
        const std::uint32_t* abiVersion = nullptr;
    };

    template<typename T>
    bool Resolve(HMODULE module, const char* name, T& out)
    {
        out = reinterpret_cast<T>(GetProcAddress(module, name));
        return out != nullptr;
    }

    bool LoadHostApi(HostApi& api, const std::wstring& pluginPath)
    {
        HostLog(L"load direct ABI1 plugin path=%s", pluginPath.c_str());
        api.module = LoadLibraryW(pluginPath.c_str());
        if (!api.module)
        {
            HostLog(L"LoadLibrary direct plugin failed err=%lu", GetLastError());
            return false;
        }

        bool ok = true;
        ok = Resolve(api.module, "name", api.name) && ok;
        ok = Resolve(api.module, "initialise", api.initialise) && ok;
        ok = Resolve(api.module, "is_initialised", api.isInitialised) && ok;
        ok = Resolve(api.module, "unload", api.unload) && ok;
        ok = Resolve(api.module, "read_full_buffer", api.readFullBuffer) && ok;
        Resolve(api.module, "halljoy_get_device_telemetry", api.getDeviceTelemetry);
        Resolve(api.module, "halljoy_get_dense_snapshots", api.getDenseSnapshots);
        Resolve(api.module, "halljoy_wait_for_snapshot_update", api.waitForSnapshotUpdate);
        Resolve(api.module, "halljoy_set_diagnostic_checkpoint", api.setDiagnosticCheckpoint);
        Resolve(api.module, "halljoy_set_diagnostic_transport_error", api.setDiagnosticTransportError);
        api.abiVersion = reinterpret_cast<const std::uint32_t*>(
            GetProcAddress(api.module, "ANALOG_SDK_PLUGIN_ABI_VERSION"));
        ok = api.abiVersion != nullptr && *api.abiVersion == 1 && ok;
        if (!ok)
        {
            HostLog(L"mandatory direct plugin exports missing or ABI mismatch abi=%lu",
                api.abiVersion ? static_cast<unsigned long>(*api.abiVersion) : 0ul);
            FreeLibrary(api.module);
            api = HostApi{};
            return false;
        }

        const char* pluginName = api.name ? api.name() : nullptr;
        HostLog(L"direct plugin loaded base=%p abi=%lu name=%S sdk_layer=bypassed",
            api.module, static_cast<unsigned long>(*api.abiVersion),
            pluginName ? pluginName : "<null>");
        return true;
    }

    void PublishHostError(SharedState* shared, LONG error, LONG initResult)
    {
        if (!shared) return;
        InterlockedExchange(&shared->lastError, error);
        InterlockedExchange(&shared->initResult, initResult);
        InterlockedExchange(&shared->status, Status_Error);
        InterlockedExchange64(&shared->heartbeatTickMs, static_cast<LONG64>(GetTickCount64()));
    }

    void PublishSnapshot(SharedState* shared,
        const unsigned short* codes, const float* values, int count,
        const float* denseValues,
        const HallJoyDenseSnapshot::DeviceV1* denseDevices, int denseDeviceCount,
        std::uint64_t snapshotTimestampUs,
        int rawResult, unsigned long long polls, unsigned long long successful,
        const HallJoyPluginTelemetry::DeviceV1* deviceTelemetry, int deviceTelemetryCount,
        HANDLE snapshotEvent)
    {
        if (!shared) return;
        count = std::clamp(count, 0, static_cast<int>(kMaxKeys));
        denseDeviceCount = std::clamp(denseDeviceCount, 0, static_cast<int>(kMaxDevices));

        InterlockedIncrement(&shared->snapshotSequence); // odd
        MemoryBarrier();
        shared->keyCount = count;
        for (int i = 0; i < count; ++i)
        {
            shared->codes[i] = codes[i];
            shared->values[i] = values[i];
        }
        for (int i = count; i < static_cast<int>(kMaxKeys); ++i)
        {
            shared->codes[i] = 0;
            shared->values[i] = 0.0f;
        }

        LONG denseActive = 0;
        for (int code = 0; code < static_cast<int>(kMaxKeys); ++code)
        {
            const float value = denseValues ? std::clamp(denseValues[code], 0.0f, 1.0f) : 0.0f;
            shared->denseValues[code] = value;
            if (value > 0.0f)
                ++denseActive;
        }
        shared->denseActiveKeyCount = denseActive;
        shared->denseDeviceCount = denseDeviceCount;
        for (int i = 0; i < denseDeviceCount; ++i)
            shared->denseDevices[i] = denseDevices[i];
        for (int i = denseDeviceCount; i < static_cast<int>(kMaxDevices); ++i)
            shared->denseDevices[i] = HallJoyDenseSnapshot::DeviceV1{};

        shared->lastError = rawResult < 0 ? rawResult : 0;
        shared->pollCounterLow = static_cast<LONG>(polls & 0x7FFFFFFF);
        InterlockedExchange64(&shared->totalPolls, static_cast<LONG64>(polls));
        InterlockedExchange64(&shared->totalSuccessfulPolls, static_cast<LONG64>(successful));
        InterlockedIncrement64(&shared->snapshotGeneration);
        InterlockedExchange64(&shared->snapshotTimestampUs,
            static_cast<LONG64>(snapshotTimestampUs != 0 ? snapshotTimestampUs : HostNowUs()));
        const int telemetryCount = std::clamp(deviceTelemetryCount, 0, static_cast<int>(kMaxDevices));
        shared->deviceTelemetryCount = telemetryCount;
        for (int i = 0; i < telemetryCount; ++i)
            shared->deviceTelemetry[i] = deviceTelemetry[i];
        for (int i = telemetryCount; i < static_cast<int>(kMaxDevices); ++i)
            shared->deviceTelemetry[i] = HallJoyPluginTelemetry::DeviceV1{};
        const LONG64 now = static_cast<LONG64>(GetTickCount64());
        InterlockedExchange64(&shared->heartbeatTickMs, now);
        InterlockedExchange64(&shared->lastPublishTickMs, now);
        MemoryBarrier();
        InterlockedIncrement(&shared->snapshotSequence); // even
        if (snapshotEvent)
            SetEvent(snapshotEvent);
    }

    void InvalidateSnapshot(SharedState* shared, LONG status, LONG error, HANDLE snapshotEvent)
    {
        if (!shared) return;
        InterlockedIncrement(&shared->snapshotSequence); // odd
        MemoryBarrier();
        shared->keyCount = 0;
        shared->denseActiveKeyCount = 0;
        shared->denseDeviceCount = 0;
        shared->deviceTelemetryCount = 0;
        for (int i = 0; i < static_cast<int>(kMaxDevices); ++i)
        {
            shared->deviceTelemetry[i] = HallJoyPluginTelemetry::DeviceV1{};
            shared->denseDevices[i] = HallJoyDenseSnapshot::DeviceV1{};
        }
        for (int i = 0; i < static_cast<int>(kMaxKeys); ++i)
        {
            shared->codes[i] = 0;
            shared->values[i] = 0.0f;
            shared->denseValues[i] = 0.0f;
        }
        shared->lastError = error;
        InterlockedIncrement64(&shared->snapshotGeneration);
        InterlockedExchange64(&shared->snapshotTimestampUs, static_cast<LONG64>(HostNowUs()));
        const LONG64 now = static_cast<LONG64>(GetTickCount64());
        InterlockedExchange64(&shared->heartbeatTickMs, now);
        InterlockedExchange64(&shared->lastPublishTickMs, now);
        MemoryBarrier();
        InterlockedIncrement(&shared->snapshotSequence); // even
        InterlockedExchange(&shared->status, status);
        if (snapshotEvent)
            SetEvent(snapshotEvent);
    }

    bool ParseHostCommand(DWORD& ownerPid, unsigned long long& nonce,
        std::wstring& privatePluginPath)
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv)
            return false;
        bool found = false;
        for (int i = 1; i + 3 < argc; ++i)
        {
            if (_wcsicmp(argv[i], kHostArg) == 0)
            {
                wchar_t* endPid = nullptr;
                wchar_t* endNonce = nullptr;
                const unsigned long pidValue = wcstoul(argv[i + 1], &endPid, 10);
                const unsigned long long nonceValue = _wcstoui64(argv[i + 2], &endNonce, 16);
                if (endPid != argv[i + 1] && *endPid == L'\0' &&
                    endNonce != argv[i + 2] && *endNonce == L'\0' &&
                    pidValue != 0 && nonceValue != 0 && argv[i + 3][0] != L'\0')
                {
                    ownerPid = static_cast<DWORD>(pidValue);
                    nonce = nonceValue;
                    privatePluginPath = argv[i + 3];
                    found = true;
                }
                break;
            }
        }
        LocalFree(argv);
        return found;
    }

    int RunHostImpl(DWORD ownerPid, unsigned long long nonce, const std::wstring& privatePluginPath)
    {
        const std::wstring mappingName = BuildIpcName(L"Map", ownerPid, nonce);
        const std::wstring stopName = BuildIpcName(L"Stop", ownerPid, nonce);
        const std::wstring snapshotName = BuildIpcName(L"Snapshot", ownerPid, nonce);
        HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mappingName.c_str());
        if (!mapping)
            return 31;
        auto* shared = static_cast<SharedState*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState)));
        if (!shared)
        {
            CloseHandle(mapping);
            return 32;
        }
        g_hostFaultShared.store(shared, std::memory_order_release);
        HANDLE stopEvent = OpenEventW(SYNCHRONIZE, FALSE, stopName.c_str());
        if (!stopEvent)
        {
            g_hostFaultShared.store(nullptr, std::memory_order_release);
            UnmapViewOfFile(shared);
            CloseHandle(mapping);
            return 33;
        }
        HANDLE snapshotEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, snapshotName.c_str());
        if (!snapshotEvent)
        {
            g_hostFaultShared.store(nullptr, std::memory_order_release);
            CloseHandle(stopEvent);
            UnmapViewOfFile(shared);
            CloseHandle(mapping);
            return 37;
        }
        g_hostFaultSnapshotEvent.store(snapshotEvent, std::memory_order_release);
        HANDLE ownerProcess = OpenProcess(SYNCHRONIZE, FALSE, ownerPid);
        if (!ownerProcess)
        {
            g_hostFaultSnapshotEvent.store(nullptr, std::memory_order_release);
            g_hostFaultShared.store(nullptr, std::memory_order_release);
            CloseHandle(snapshotEvent);
            CloseHandle(stopEvent);
            UnmapViewOfFile(shared);
            CloseHandle(mapping);
            return 36;
        }

        HostSetCheckpoint(shared, Checkpoint_ProcessStart);
        InterlockedExchange(&shared->hostPid, static_cast<LONG>(GetCurrentProcessId()));
        InterlockedExchange(&shared->status, Status_Starting);
        InterlockedExchange64(&shared->heartbeatTickMs, static_cast<LONG64>(GetTickCount64()));
        HostLog(L"session start owner_pid=%lu nonce=%016llX shared=%p", ownerPid, nonce, shared);

        HostApi api;
        HostSetCheckpoint(shared, Checkpoint_LoadSdk);
        if (!LoadHostApi(api, privatePluginPath))
        {
            PublishHostError(shared, static_cast<LONG>(GetLastError()), WootingAnalogResult_DLLNotFound);
            g_hostFaultSnapshotEvent.store(nullptr, std::memory_order_release);
            g_hostFaultShared.store(nullptr, std::memory_order_release);
            CloseHandle(ownerProcess);
            CloseHandle(snapshotEvent);
            CloseHandle(stopEvent);
            UnmapViewOfFile(shared);
            CloseHandle(mapping);
            return 34;
        }

        if (api.setDiagnosticCheckpoint)
        {
            api.setDiagnosticCheckpoint(&shared->checkpoint);
            HostLog(L"plugin checkpoint bridge enabled target=%p", &shared->checkpoint);
        }
        else
        {
            HostLog(L"plugin checkpoint bridge unavailable");
        }
        if (api.setDiagnosticTransportError)
        {
            InterlockedExchange(&shared->transportError, 0);
            api.setDiagnosticTransportError(&shared->transportError);
            HostLog(L"plugin transport-error bridge enabled target=%p", &shared->transportError);
        }
        else
        {
            HostLog(L"plugin transport-error bridge unavailable");
        }

        HostSetCheckpoint(shared, Checkpoint_SdkInitialise);
        const int initResult = api.initialise(nullptr, nullptr);
        InterlockedExchange(&shared->initResult, initResult);
        HostLog(L"SDK initialise result=%d", initResult);
        if (initResult < 0)
        {
            PublishHostError(shared, initResult, initResult);
            FreeLibrary(api.module);
            g_hostFaultSnapshotEvent.store(nullptr, std::memory_order_release);
            g_hostFaultShared.store(nullptr, std::memory_order_release);
            CloseHandle(ownerProcess);
            CloseHandle(snapshotEvent);
            CloseHandle(stopEvent);
            UnmapViewOfFile(shared);
            CloseHandle(mapping);
            return 35;
        }

        // The direct Universal Analog Plugin ABI always publishes HID usage
        // codes. There is no SDK keycode-conversion layer in this process.
        HostSetCheckpoint(shared, Checkpoint_SetKeycodeMode);
        InterlockedExchange(&shared->requestedKeycodeMode, WootingAnalog_KeycodeType_HID);
        InterlockedExchange(&shared->appliedKeycodeMode, WootingAnalog_KeycodeType_HID);
        HostLog(L"keycode mode fixed to HID by direct ABI1 path");
        HostLog(L"snapshot scheduling=%s dense_device_api=%d dense_key_count=%u",
            api.waitForSnapshotUpdate ? L"event_driven" : L"1ms_fallback",
            api.getDenseSnapshots ? 1 : 0, static_cast<unsigned>(kMaxKeys));
        // Status remains Starting/Restarting until the first coherent snapshot
        // (including a valid all-zero snapshot) has been published below.

        unsigned long long polls = 0;
        unsigned long long successful = 0;
        unsigned int consecutivePluginErrors = 0;
        int hostExitCode = 0;
        ULONGLONG lastStats = GetTickCount64();
        std::uint64_t observedPluginGeneration = 0;
        std::array<unsigned short, kMaxKeys> rawCodes{};
        std::array<float, kMaxKeys> rawValues{};
        std::array<unsigned short, kMaxKeys> validCodes{};
        std::array<float, kMaxKeys> validValues{};
        std::array<float, kMaxKeys> mergedDense{};
        std::array<HallJoyDenseSnapshot::DeviceV1, kMaxDevices> rawDenseDevices{};
        std::array<HallJoyDenseSnapshot::DeviceV1, kMaxDevices> validDenseDevices{};
        int validDenseDeviceCount = 0;
        std::uint64_t snapshotTimestampUs = 0;
        std::array<HallJoyPluginTelemetry::DeviceV1, kMaxDevices> pluginTelemetry{};
        int pluginTelemetryCount = 0;
        ULONGLONG nextTelemetryRefresh = 0;

        for (;;)
        {
            if (WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0 ||
                WaitForSingleObject(ownerProcess, 0) == WAIT_OBJECT_0)
                break;

            HostSetCheckpoint(shared, Checkpoint_WaitForPoll);
            if (api.waitForSnapshotUpdate)
            {
                // Wake immediately for fresh data from any device worker. The
                // 50 ms timeout is a heartbeat only; it does not pace device I/O.
                observedPluginGeneration = api.waitForSnapshotUpdate(observedPluginGeneration, 50);
            }
            else if (WaitForSingleObject(stopEvent, 1) == WAIT_OBJECT_0)
            {
                break;
            }

            if (WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0 ||
                WaitForSingleObject(ownerProcess, 0) == WAIT_OBJECT_0)
                break;

            const ULONGLONG now = GetTickCount64();
            const LONG requestedMode = InterlockedCompareExchange(&shared->requestedKeycodeMode, 0, 0);
            if (requestedMode != WootingAnalog_KeycodeType_HID)
            {
                InterlockedExchange(&shared->lastError, WootingAnalogResult_NotAvailable);
                InterlockedExchange(&shared->requestedKeycodeMode, WootingAnalog_KeycodeType_HID);
                InterlockedExchange(&shared->appliedKeycodeMode, WootingAnalog_KeycodeType_HID);
                HostLog(L"non-HID keycode request=%ld rejected by direct ABI1 path", requestedMode);
            }

            if (api.getDeviceTelemetry && now >= nextTelemetryRefresh)
            {
                pluginTelemetryCount = static_cast<int>(api.getDeviceTelemetry(
                    pluginTelemetry.data(), static_cast<std::uint32_t>(pluginTelemetry.size()),
                    static_cast<std::uint32_t>(sizeof(pluginTelemetry[0]))));
                pluginTelemetryCount = std::clamp(pluginTelemetryCount, 0, static_cast<int>(pluginTelemetry.size()));
                nextTelemetryRefresh = now + 250;
            }

            ++polls;
            const LONG cppFaultAfter = InterlockedCompareExchange(
                &shared->diagnosticCppFaultAfterPolls, 0, 0);
            if (cppFaultAfter > 0 && polls >= static_cast<unsigned long long>(cppFaultAfter))
            {
                InterlockedExchange(&shared->diagnosticCppFaultAfterPolls, 0);
                HostSetCheckpoint(shared, Checkpoint_BeforeReadFullBuffer);
                throw std::runtime_error("simulated child-host C++ fault");
            }
            const LONG crashAfter = InterlockedCompareExchange(&shared->diagnosticCrashAfterPolls, 0, 0);
            if (crashAfter > 0 && polls >= static_cast<unsigned long long>(crashAfter))
            {
                InterlockedExchange(&shared->diagnosticCrashAfterPolls, 0);
                HostSetCheckpoint(shared, Checkpoint_BeforeReadFullBuffer);
                HostLog(L"intentional isolated crash self-test at poll=%llu", polls);
                using CrashFn = void(*)();
                reinterpret_cast<CrashFn>(static_cast<uintptr_t>(0x21))();
            }
            HostSetCheckpoint(shared, Checkpoint_BeforeReadFullBuffer);
            const int result = api.readFullBuffer(rawCodes.data(), rawValues.data(),
                static_cast<unsigned int>(rawCodes.size()), static_cast<PluginDeviceId>(0));
            HostSetCheckpoint(shared, Checkpoint_AfterReadFullBuffer);

            int validCount = 0;
            validDenseDeviceCount = 0;
            snapshotTimestampUs = HostNowUs();
            mergedDense.fill(0.0f);
            for (auto& item : validDenseDevices)
                item = HallJoyDenseSnapshot::DeviceV1{};

            if (result >= 0)
            {
                consecutivePluginErrors = 0;
                HostSetCheckpoint(shared, Checkpoint_ValidateSnapshot);
                const int n = std::min(result, static_cast<int>(kMaxKeys));
                for (int i = 0; i < n; ++i)
                {
                    const unsigned short code = rawCodes[static_cast<size_t>(i)];
                    const float value = rawValues[static_cast<size_t>(i)];
                    if (code >= kMaxKeys || !std::isfinite(value))
                    {
                        InterlockedIncrement(&shared->invalidSnapshotCount);
                        continue;
                    }
                    validCodes[static_cast<size_t>(validCount)] = code;
                    validValues[static_cast<size_t>(validCount)] = std::clamp(value, 0.0f, 1.0f);
                    ++validCount;
                }

                if (api.getDenseSnapshots)
                {
                    const std::uint32_t rawDenseCount = api.getDenseSnapshots(
                        rawDenseDevices.data(), static_cast<std::uint32_t>(rawDenseDevices.size()),
                        static_cast<std::uint32_t>(sizeof(rawDenseDevices[0])));
                    const int denseCount = std::clamp(static_cast<int>(rawDenseCount), 0, static_cast<int>(kMaxDevices));
                    for (int di = 0; di < denseCount; ++di)
                    {
                        const auto& input = rawDenseDevices[static_cast<std::size_t>(di)];
                        if (input.structSize != sizeof(HallJoyDenseSnapshot::DeviceV1) ||
                            input.version != HallJoyDenseSnapshot::kVersion)
                        {
                            InterlockedIncrement(&shared->invalidSnapshotCount);
                            continue;
                        }
                        auto& output = validDenseDevices[static_cast<std::size_t>(validDenseDeviceCount)];
                        output = input;
                        std::uint32_t active = 0;
                        for (std::size_t code = 0; code < kMaxKeys; ++code)
                        {
                            float value = input.values[code];
                            if (!std::isfinite(value))
                            {
                                value = 0.0f;
                                InterlockedIncrement(&shared->invalidSnapshotCount);
                            }
                            value = std::clamp(value, 0.0f, 1.0f);
                            output.values[code] = value;
                            mergedDense[code] = (std::max)(mergedDense[code], value);
                            if (value > 0.0f)
                                ++active;
                        }
                        output.activeKeyCount = active;
                        snapshotTimestampUs = (std::max)(snapshotTimestampUs, output.timestampUs);
                        ++validDenseDeviceCount;
                    }
                }
                else
                {
                    for (int i = 0; i < validCount; ++i)
                    {
                        const auto code = validCodes[static_cast<std::size_t>(i)];
                        mergedDense[code] = (std::max)(mergedDense[code], validValues[static_cast<std::size_t>(i)]);
                    }
                }

                // Rebuild the compatibility sparse view from the canonical dense
                // table. This deduplicates the same HID key across devices and
                // removes dependence on the plugin's sparse ordering.
                validCount = 0;
                for (std::size_t code = 0; code < mergedDense.size(); ++code)
                {
                    if (mergedDense[code] > 0.0f)
                    {
                        validCodes[static_cast<std::size_t>(validCount)] = static_cast<unsigned short>(code);
                        validValues[static_cast<std::size_t>(validCount)] = mergedDense[code];
                        ++validCount;
                    }
                }
                ++successful;
            }
            else
            {
                ++consecutivePluginErrors;
            }

            HostSetCheckpoint(shared, Checkpoint_PublishSnapshot);
            PublishSnapshot(shared, validCodes.data(), validValues.data(), result >= 0 ? validCount : 0,
                mergedDense.data(), validDenseDevices.data(), result >= 0 ? validDenseDeviceCount : 0, snapshotTimestampUs,
                result, polls, successful, pluginTelemetry.data(), pluginTelemetryCount, snapshotEvent);
            InterlockedExchange(&shared->status, Status_Ready);

            if (consecutivePluginErrors >= 4)
            {
                hostExitCode = 0xE0484944; // "HID" controlled transport restart
                HostLog(L"persistent plugin transport error result=%d transport_error=%ld count=%u; restarting isolated host",
                    result, InterlockedCompareExchange(&shared->transportError, 0, 0), consecutivePluginErrors);
                InvalidateSnapshot(shared, Status_Restarting, result, snapshotEvent);
                break;
            }

            if (now - lastStats >= kHostStatsIntervalMs)
            {
                float maxValue = 0.0f;
                for (int i = 0; i < validCount; ++i)
                    maxValue = std::max(maxValue, validValues[static_cast<size_t>(i)]);
                HostLog(L"stats polls=%llu successful=%llu last_result=%d transport_error=%ld count=%d dense_devices=%d dense_generation=%lld max=%d checkpoint=%s",
                    polls, successful, result,
                    InterlockedCompareExchange(&shared->transportError, 0, 0),
                    validCount, validDenseDeviceCount,
                    InterlockedCompareExchange64(&shared->snapshotGeneration, 0, 0),
                    static_cast<int>(std::lround(maxValue * 1000.0f)),
                    CheckpointName(InterlockedCompareExchange(&shared->checkpoint, 0, 0)));
                lastStats = now;
            }
        }

        HostSetCheckpoint(shared, Checkpoint_SdkUninitialise);
        if (api.unload)
            api.unload();
        if (api.setDiagnosticCheckpoint)
            api.setDiagnosticCheckpoint(nullptr);
        if (api.setDiagnosticTransportError)
            api.setDiagnosticTransportError(nullptr);
        HostSetCheckpoint(shared, Checkpoint_ProcessExit);
        InterlockedExchange(&shared->status, Status_Stopped);
        HostLog(L"session exit polls=%llu successful=%llu code=0x%08X", polls, successful, hostExitCode);
        FreeLibrary(api.module);
        g_hostFaultSnapshotEvent.store(nullptr, std::memory_order_release);
        g_hostFaultShared.store(nullptr, std::memory_order_release);
        CloseHandle(ownerProcess);
        CloseHandle(snapshotEvent);
        CloseHandle(stopEvent);
        UnmapViewOfFile(shared);
        CloseHandle(mapping);
        return hostExitCode;
    }

    void ChildHostOnCppFault(
        const halljoy::worker::WorkerExceptionRecord& record) noexcept
    {
        SharedState* shared = g_hostFaultShared.load(std::memory_order_acquire);
        const LONG error = record.kind == halljoy::worker::WorkerExceptionKind::StandardException
            ? static_cast<LONG>(0xE0484301u)
            : static_cast<LONG>(0xE0484302u);
        if (shared)
        {
            HostSetCheckpoint(shared, Checkpoint_ProcessExit);
            InvalidateSnapshot(shared, Status_Error, error,
                g_hostFaultSnapshotEvent.load(std::memory_order_acquire));
            InterlockedExchange(&shared->initResult, WootingAnalogResult_Failure);
        }
    }

    void ChildHostOnCppCompletion(
        const halljoy::worker::WorkerExceptionRecord&) noexcept
    {
    }

    int RunHostCpp(DWORD ownerPid, unsigned long long nonce,
        const std::wstring& privatePluginPath) noexcept
    {
        return static_cast<int>(halljoy::worker::RunWorkerEntryBarrier(
            [&] { return static_cast<std::uint32_t>(RunHostImpl(ownerPid, nonce, privatePluginPath)); },
            ChildHostOnCppFault,
            ChildHostOnCppCompletion,
            0xE0484303u));
    }

    void ChildHostOnStructuredFault(DWORD code) noexcept
    {
        SharedState* shared = g_hostFaultShared.load(std::memory_order_acquire);
        if (shared)
        {
            HostSetCheckpoint(shared, Checkpoint_ProcessExit);
            InvalidateSnapshot(shared, Status_Error, static_cast<LONG>(code),
                g_hostFaultSnapshotEvent.load(std::memory_order_acquire));
            InterlockedExchange(&shared->initResult, WootingAnalogResult_Failure);
        }
    }

    int RunHost(DWORD ownerPid, unsigned long long nonce,
        const std::wstring& privatePluginPath) noexcept
    {
        __try
        {
            return RunHostCpp(ownerPid, nonce, privatePluginPath);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            const DWORD code = GetExceptionCode();
            ChildHostOnStructuredFault(code);
            return static_cast<int>(code);
        }
    }

    struct ModuleRange
    {
        std::uintptr_t base = 0;
        std::uintptr_t end = 0;
        std::wstring name;
    };

    std::vector<ModuleRange> EnumerateModules(DWORD pid)
    {
        std::vector<ModuleRange> modules;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot == INVALID_HANDLE_VALUE)
            return modules;
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Module32FirstW(snapshot, &entry))
        {
            do
            {
                ModuleRange range;
                range.base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                range.end = range.base + entry.modBaseSize;
                range.name = entry.szModule;
                modules.emplace_back(std::move(range));
            } while (Module32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return modules;
    }

    std::wstring ResolveAddress(std::uintptr_t address, const std::vector<ModuleRange>& modules)
    {
        for (const auto& module : modules)
        {
            if (address >= module.base && address < module.end)
            {
                wchar_t text[512]{};
                _snwprintf_s(text, _countof(text), _TRUNCATE, L"%s+0x%llX",
                    module.name.c_str(), static_cast<unsigned long long>(address - module.base));
                return text;
            }
        }
        wchar_t text[128]{};
        _snwprintf_s(text, _countof(text), _TRUNCATE, L"0x%llX",
            static_cast<unsigned long long>(address));
        return text;
    }

    bool WriteMiniDump(HANDLE process, DWORD pid, DWORD threadId,
        const EXCEPTION_RECORD& sourceRecord, const CONTEXT* sourceContext,
        const std::wstring& path)
    {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        EXCEPTION_RECORD record = sourceRecord;
        // A chained-record pointer from the debug event belongs to the target
        // address space. Do not let DbgHelp follow it in the supervisor process.
        record.ExceptionRecord = nullptr;
        CONTEXT context{};
        EXCEPTION_POINTERS pointers{};
        MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
        MINIDUMP_EXCEPTION_INFORMATION* exceptionInfoPtr = nullptr;
        if (sourceContext)
        {
            context = *sourceContext;
            pointers.ExceptionRecord = &record;
            pointers.ContextRecord = &context;
            exceptionInfo.ThreadId = threadId;
            exceptionInfo.ExceptionPointers = &pointers;
            exceptionInfo.ClientPointers = FALSE;
            exceptionInfoPtr = &exceptionInfo;
        }

        const MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
            MiniDumpNormal |
            MiniDumpWithThreadInfo |
            MiniDumpWithUnloadedModules |
            MiniDumpWithIndirectlyReferencedMemory |
            MiniDumpWithDataSegs |
            MiniDumpWithFullMemoryInfo |
            MiniDumpIgnoreInaccessibleMemory);
        const BOOL ok = MiniDumpWriteDump(process, pid, file, type,
            exceptionInfoPtr, nullptr, nullptr);
        FlushFileBuffers(file);
        CloseHandle(file);
        return ok != FALSE;
    }

    void WriteHostCrashReport(HANDLE process, DWORD pid, DWORD threadId,
        const EXCEPTION_DEBUG_INFO& exceptionInfo, SharedState* shared, const wchar_t* reason)
    {
        const auto modules = EnumerateModules(pid);
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, threadId);
        CONTEXT context{};
        context.ContextFlags = CONTEXT_ALL;
        const bool gotContext = thread && GetThreadContext(thread, &context);

        const std::wstring latest = BuildPathNearExe(L"HallJoyAnalogHostCrashLatest.txt");
        HANDLE file = CreateFileW(latest.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            WriteUtf8(file, L"HallJoy isolated analog host crash report");
            SYSTEMTIME st{};
            GetLocalTime(&st);
            wchar_t line[1400]{};
            _snwprintf_s(line, _countof(line), _TRUNCATE,
                L"time=%04u-%02u-%02u %02u:%02u:%02u.%03u",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
            WriteUtf8(file, line);
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"reason=%s", reason ? reason : L"exception");
            WriteUtf8(file, line);
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"host_pid=%lu", pid);
            WriteUtf8(file, line);
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"thread=%lu", threadId);
            WriteUtf8(file, line);
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"first_chance=%lu", exceptionInfo.dwFirstChance);
            WriteUtf8(file, line);
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"exception=0x%08lX", exceptionInfo.ExceptionRecord.ExceptionCode);
            WriteUtf8(file, line);
            const auto exceptionAddress = reinterpret_cast<std::uintptr_t>(exceptionInfo.ExceptionRecord.ExceptionAddress);
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"exception_address=%s",
                ResolveAddress(exceptionAddress, modules).c_str());
            WriteUtf8(file, line);
            if (exceptionInfo.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
                exceptionInfo.ExceptionRecord.NumberParameters >= 2)
            {
                const ULONG_PTR operation = exceptionInfo.ExceptionRecord.ExceptionInformation[0];
                const ULONG_PTR target = exceptionInfo.ExceptionRecord.ExceptionInformation[1];
                const wchar_t* operationName = operation == 0 ? L"read" : operation == 1 ? L"write" : operation == 8 ? L"execute" : L"unknown";
                _snwprintf_s(line, _countof(line), _TRUNCATE, L"memory_operation=%s", operationName);
                WriteUtf8(file, line);
                _snwprintf_s(line, _countof(line), _TRUNCATE, L"memory_address=0x%llX", static_cast<unsigned long long>(target));
                WriteUtf8(file, line);
            }
            if (shared)
            {
                const LONG checkpoint = InterlockedCompareExchange(&shared->checkpoint, 0, 0);
                _snwprintf_s(line, _countof(line), _TRUNCATE, L"checkpoint=%ld (%s)", checkpoint, CheckpointName(checkpoint));
                WriteUtf8(file, line);
                _snwprintf_s(line, _countof(line), _TRUNCATE, L"host_status=%ld init_result=%ld last_error=%ld transport_error=%ld key_count=%ld restarts=%ld invalid_snapshots=%ld",
                    InterlockedCompareExchange(&shared->status, 0, 0),
                    InterlockedCompareExchange(&shared->initResult, 0, 0),
                    InterlockedCompareExchange(&shared->lastError, 0, 0),
                    InterlockedCompareExchange(&shared->transportError, 0, 0),
                    InterlockedCompareExchange(&shared->keyCount, 0, 0),
                    InterlockedCompareExchange(&shared->restartCount, 0, 0),
                    InterlockedCompareExchange(&shared->invalidSnapshotCount, 0, 0));
                WriteUtf8(file, line);
                _snwprintf_s(line, _countof(line), _TRUNCATE, L"total_polls=%lld successful_polls=%lld heartbeat_tick_ms=%lld",
                    InterlockedCompareExchange64(&shared->totalPolls, 0, 0),
                    InterlockedCompareExchange64(&shared->totalSuccessfulPolls, 0, 0),
                    InterlockedCompareExchange64(&shared->heartbeatTickMs, 0, 0));
                WriteUtf8(file, line);
            }

#if defined(_M_X64) || defined(__x86_64__)
            if (gotContext)
            {
                _snwprintf_s(line, _countof(line), _TRUNCATE, L"RIP=%s RSP=0x%016llX RBP=0x%016llX EFLAGS=0x%08lX",
                    ResolveAddress(static_cast<std::uintptr_t>(context.Rip), modules).c_str(),
                    static_cast<unsigned long long>(context.Rsp),
                    static_cast<unsigned long long>(context.Rbp), context.EFlags);
                WriteUtf8(file, line);
                _snwprintf_s(line, _countof(line), _TRUNCATE, L"RAX=%016llX RBX=%016llX RCX=%016llX RDX=%016llX RSI=%016llX RDI=%016llX",
                    context.Rax, context.Rbx, context.Rcx, context.Rdx, context.Rsi, context.Rdi);
                WriteUtf8(file, line);
                _snwprintf_s(line, _countof(line), _TRUNCATE, L"R8=%016llX R9=%016llX R10=%016llX R11=%016llX R12=%016llX R13=%016llX R14=%016llX R15=%016llX",
                    context.R8, context.R9, context.R10, context.R11, context.R12, context.R13, context.R14, context.R15);
                WriteUtf8(file, line);

                std::array<std::uintptr_t, 64> stack{};
                SIZE_T read = 0;
                if (ReadProcessMemory(process, reinterpret_cast<const void*>(context.Rsp), stack.data(), sizeof(stack), &read))
                {
                    WriteUtf8(file, L"stack_qwords:");
                    const size_t count = std::min(stack.size(), static_cast<size_t>(read / sizeof(stack[0])));
                    for (size_t i = 0; i < count; ++i)
                    {
                        const std::uintptr_t value = stack[i];
                        _snwprintf_s(line, _countof(line), _TRUNCATE,
                            L"  rsp+0x%03llX = 0x%016llX  %s",
                            static_cast<unsigned long long>(i * sizeof(std::uintptr_t)),
                            static_cast<unsigned long long>(value),
                            ResolveAddress(value, modules).c_str());
                        WriteUtf8(file, line);
                    }
                }
            }
#else
            WriteUtf8(file, L"register_capture=unsupported_non_x64_build");
#endif

            WriteUtf8(file, L"modules:");
            for (const auto& module : modules)
            {
                _snwprintf_s(line, _countof(line), _TRUNCATE, L"  %s base=0x%016llX size=0x%llX",
                    module.name.c_str(), static_cast<unsigned long long>(module.base),
                    static_cast<unsigned long long>(module.end - module.base));
                WriteUtf8(file, line);
            }
            WriteUtf8(file, L"dump=HallJoyAnalogHostCrashLatest.dmp");
            WriteUtf8(file, L"effect=only the isolated analog host terminates; HallJoy remains running and restarts it");
            FlushFileBuffers(file);
            CloseHandle(file);
        }

        const std::wstring latestDump = BuildPathNearExe(L"HallJoyAnalogHostCrashLatest.dmp");
        const bool dumpOk = WriteMiniDump(process, pid, threadId,
            exceptionInfo.ExceptionRecord, gotContext ? &context : nullptr, latestDump);

        // Preserve every captured failure instead of allowing a rapid restart
        // loop to overwrite the first and most useful report.
        SYSTEMTIME archiveTime{};
        GetLocalTime(&archiveTime);
        wchar_t archiveTextName[256]{};
        wchar_t archiveDumpName[256]{};
        _snwprintf_s(archiveTextName, _countof(archiveTextName), _TRUNCATE,
            L"HallJoyAnalogHostCrash_%04u%02u%02u-%02u%02u%02u-%03u_p%lu_t%lu.txt",
            archiveTime.wYear, archiveTime.wMonth, archiveTime.wDay,
            archiveTime.wHour, archiveTime.wMinute, archiveTime.wSecond,
            archiveTime.wMilliseconds, pid, threadId);
        _snwprintf_s(archiveDumpName, _countof(archiveDumpName), _TRUNCATE,
            L"HallJoyAnalogHostCrash_%04u%02u%02u-%02u%02u%02u-%03u_p%lu_t%lu.dmp",
            archiveTime.wYear, archiveTime.wMonth, archiveTime.wDay,
            archiveTime.wHour, archiveTime.wMinute, archiveTime.wSecond,
            archiveTime.wMilliseconds, pid, threadId);
        CopyFileW(latest.c_str(), BuildPathNearExe(archiveTextName).c_str(), TRUE);
        if (dumpOk)
            CopyFileW(latestDump.c_str(), BuildPathNearExe(archiveDumpName).c_str(), TRUE);

        DebugLog_Write(L"[analog.host.crash] report=%s dump=%d exception=0x%08lX thread=%lu checkpoint=%s transport_error=%ld archive=%s",
            latest.c_str(), dumpOk ? 1 : 0, exceptionInfo.ExceptionRecord.ExceptionCode,
            threadId, shared ? CheckpointName(InterlockedCompareExchange(&shared->checkpoint, 0, 0)) : L"unavailable",
            shared ? InterlockedCompareExchange(&shared->transportError, 0, 0) : 0,
            archiveTextName);
        if (thread)
            CloseHandle(thread);
    }

    DWORD FindAnyThreadId(DWORD pid)
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return 0;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        DWORD result = 0;
        if (Thread32First(snapshot, &entry))
        {
            do
            {
                if (entry.th32OwnerProcessID == pid)
                {
                    result = entry.th32ThreadID;
                    break;
                }
            } while (Thread32Next(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return result;
    }

    void WriteHostHangReport(HANDLE process, DWORD pid, SharedState* shared)
    {
        const DWORD threadId = FindAnyThreadId(pid);
        HANDLE thread = threadId ? OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
            FALSE, threadId) : nullptr;
        const bool suspended = thread && SuspendThread(thread) != static_cast<DWORD>(-1);

        EXCEPTION_DEBUG_INFO fake{};
        fake.ExceptionRecord.ExceptionCode = 0xE048414Eu; // synthetic HANG marker
        fake.ExceptionRecord.ExceptionAddress = nullptr;
        fake.dwFirstChance = 0;
        WriteHostCrashReport(process, pid, threadId, fake, shared, L"heartbeat_timeout");

        if (suspended)
            ResumeThread(thread);
        if (thread)
            CloseHandle(thread);
    }

    bool IsFatalExceptionCode(DWORD code)
    {
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
        case 0xC0000374u:
        case 0xC0000409u:
            return true;
        default:
            return false;
        }
    }

    bool CreateHostProcess(PROCESS_INFORMATION& pi)
    {
        wchar_t exePath[32768]{};
        const DWORD len = GetModuleFileNameW(nullptr, exePath, static_cast<DWORD>(_countof(exePath)));
        if (len == 0 || len >= _countof(exePath))
            return false;
        wchar_t identity[128]{};
        _snwprintf_s(identity, _countof(identity), _TRUNCATE,
            L" %s %lu %016llX ", kHostArg, g_client.ownerPid, g_client.nonce);
        std::wstring command = halljoy::windows_command_line::QuoteArgument(exePath);
        command += identity;
        command += halljoy::windows_command_line::QuoteArgument(g_client.privatePluginPath);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        const DWORD creationFlags = CREATE_NO_WINDOW |
            (kUseChildDebugger ? DEBUG_ONLY_THIS_PROCESS : 0u);
        const BOOL ok = CreateProcessW(exePath, command.data(), nullptr, nullptr, FALSE,
            creationFlags, nullptr, nullptr, &si, &pi);
        return ok != FALSE;
    }

    DWORD SupervisorThreadProcImpl()
    {
        if constexpr (kUseChildDebugger)
            DebugSetProcessKillOnExit(FALSE);
        HostLog(L"supervisor start mode=%s", kUseChildDebugger ? L"diagnostic_debugger" : L"production_process_wait");
        if (g_client.supervisorReadyEvent)
            SetEvent(g_client.supervisorReadyEvent);
#if defined(HALLJOY_ANALOG_SIMULATOR)
        if (g_client.injectSupervisorCppFault)
            throw std::runtime_error("simulated analog-host supervisor C++ fault");
#endif

        LONG restartCount = 0;
        while (!g_client.stopping.load(std::memory_order_acquire))
        {
            if (g_client.shared)
            {
                InvalidateSnapshot(g_client.shared,
                    restartCount == 0 ? Status_Starting : Status_Restarting,
                    0, g_client.snapshotEvent);
                InterlockedExchange(&g_client.shared->restartCount, restartCount);
                InterlockedExchange(&g_client.shared->transportError, 0);
            }

            PROCESS_INFORMATION pi{};
            if (!CreateHostProcess(pi))
            {
                const DWORD error = GetLastError();
                if (g_client.shared)
                    PublishHostError(g_client.shared, static_cast<LONG>(error), WootingAnalogResult_Failure);
                DebugLog_Write(L"[analog.host] CreateProcess failed err=%lu", error);
                if (WaitForSingleObject(g_client.stopEvent, kRestartDelayMs) == WAIT_OBJECT_0)
                    break;
                ++restartCount;
                continue;
            }

            bool restartAllowed = true;
            if (!AssignProcessToJobObject(g_client.job, pi.hProcess))
            {
                const DWORD error = GetLastError();
                restartAllowed = false;
                g_client.restartBlocked.store(true, std::memory_order_release);
                PublishHostError(g_client.shared, static_cast<LONG>(error), WootingAnalogResult_Failure);
                StabilityTrace_WriteCritical(L"ERROR", L"analog-host", L"child.job_assign_failed",
                    L"pid=%lu win32=%lu restart_blocked=1", pi.dwProcessId, error);
                DebugLog_Write(L"[analog.host] AssignProcessToJobObject failed err=%lu; restart blocked", error);
                TerminateProcess(pi.hProcess, 0xE0484A4Fu);
            }
            DebugLog_Write(L"[analog.host] started pid=%lu restart=%ld", pi.dwProcessId, restartCount);
            HostLog(L"supervisor child start pid=%lu restart=%ld", pi.dwProcessId, restartCount);
            StabilityTrace_Write(L"INFO", L"analog-host", L"child.start",
                L"pid=%lu restart=%ld", pi.dwProcessId, restartCount);
            bool processExited = false;
            bool crashCaptured = false;
            HANDLE debugProcessHandle = nullptr;
            HANDLE debugMainThreadHandle = nullptr;
            DWORD processExitCode = STILL_ACTIVE;
            ULONGLONG lastHeartbeatSeen = GetTickCount64();
            LONG64 lastHeartbeatValue = 0;

            ULONGLONG stopDeadline = 0;
            while (!processExited)
            {
                if (g_client.stopping.load(std::memory_order_acquire) && stopDeadline == 0)
                {
                    SetEvent(g_client.stopEvent);
                    stopDeadline = GetTickCount64() + 2500;
                }

                if constexpr (kUseChildDebugger)
                {
                    DEBUG_EVENT event{};
                    if (WaitForDebugEvent(&event, 100))
                    {
                        DWORD continueStatus = DBG_CONTINUE;
                        switch (event.dwDebugEventCode)
                        {
                        case CREATE_PROCESS_DEBUG_EVENT:
                            if (event.u.CreateProcessInfo.hFile)
                                CloseHandle(event.u.CreateProcessInfo.hFile);
                            debugProcessHandle = event.u.CreateProcessInfo.hProcess;
                            debugMainThreadHandle = event.u.CreateProcessInfo.hThread;
                            break;
                        case CREATE_THREAD_DEBUG_EVENT:
                            if (event.u.CreateThread.hThread)
                                CloseHandle(event.u.CreateThread.hThread);
                            break;
                        case LOAD_DLL_DEBUG_EVENT:
                            if (event.u.LoadDll.hFile)
                                CloseHandle(event.u.LoadDll.hFile);
                            break;
                        case EXCEPTION_DEBUG_EVENT:
                        {
                            const DWORD code = event.u.Exception.ExceptionRecord.ExceptionCode;
                            if (code == EXCEPTION_BREAKPOINT || code == 0x406D1388u)
                            {
                                continueStatus = DBG_CONTINUE;
                            }
                            else
                            {
                                continueStatus = DBG_EXCEPTION_NOT_HANDLED;
                                if ((IsFatalExceptionCode(code) || event.u.Exception.dwFirstChance == 0) && !crashCaptured)
                                {
                                    crashCaptured = true;
                                    InvalidateSnapshot(g_client.shared, Status_Restarting, static_cast<LONG>(code), g_client.snapshotEvent);
                                    WriteHostCrashReport(pi.hProcess, pi.dwProcessId, event.dwThreadId,
                                        event.u.Exception, g_client.shared,
                                        event.u.Exception.dwFirstChance ? L"fatal_first_chance_debug_exception" : L"unhandled_second_chance_debug_exception");
                                }
                            }
                            break;
                        }
                        case EXIT_PROCESS_DEBUG_EVENT:
                            processExitCode = event.u.ExitProcess.dwExitCode;
                            processExited = true;
                            break;
                        default:
                            break;
                        }
                        ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continueStatus);
                    }
                    else if (GetLastError() != ERROR_SEM_TIMEOUT)
                    {
                        DebugLog_Write(L"[analog.host] WaitForDebugEvent failed err=%lu", GetLastError());
                        break;
                    }
                }
                else
                {
                    const DWORD processWait = WaitForSingleObject(pi.hProcess, 100);
                    if (processWait == WAIT_OBJECT_0)
                    {
                        GetExitCodeProcess(pi.hProcess, &processExitCode);
                        processExited = true;
                    }
                    else if (processWait == WAIT_FAILED)
                    {
                        DebugLog_Write(L"[analog.host] process wait failed err=%lu", GetLastError());
                        break;
                    }
                }

                if (stopDeadline != 0 && GetTickCount64() >= stopDeadline && !processExited)
                {
                    DebugLog_Write(L"[analog.host] graceful stop timed out; terminating pid=%lu", pi.dwProcessId);
                    TerminateProcess(pi.hProcess, 0);
                    stopDeadline = GetTickCount64() + 1000;
                }

                if (g_client.shared)
                {
                    const LONG64 heartbeat = InterlockedCompareExchange64(&g_client.shared->heartbeatTickMs, 0, 0);
                    if (heartbeat != lastHeartbeatValue)
                    {
                        lastHeartbeatValue = heartbeat;
                        lastHeartbeatSeen = GetTickCount64();
                    }
                    else if (InterlockedCompareExchange(&g_client.shared->status, 0, 0) == Status_Ready &&
                        GetTickCount64() - lastHeartbeatSeen > kHeartbeatTimeoutMs)
                    {
                        if (!crashCaptured)
                        {
                            crashCaptured = true;
                            InvalidateSnapshot(g_client.shared, Status_Restarting, 0xE048414E, g_client.snapshotEvent);
                            WriteHostHangReport(pi.hProcess, pi.dwProcessId, g_client.shared);
                        }
                        DebugLog_Write(L"[analog.host] heartbeat timeout; terminating pid=%lu", pi.dwProcessId);
                        TerminateProcess(pi.hProcess, 0xE048414Eu);
                    }
                }
            }

            if (!processExited)
            {
                if constexpr (kUseChildDebugger)
                    DebugActiveProcessStop(pi.dwProcessId);
                if (g_client.stopping.load(std::memory_order_acquire))
                    SetEvent(g_client.stopEvent);
                if (WaitForSingleObject(pi.hProcess, 2000) != WAIT_OBJECT_0)
                    TerminateProcess(pi.hProcess, 0);
            }
            DWORD finalProcessWait = WaitForSingleObject(pi.hProcess, 3000);
#if defined(HALLJOY_ANALOG_SIMULATOR)
            if (g_client.injectChildReapTimeout)
            {
                g_client.injectChildReapTimeout = false;
                finalProcessWait = WAIT_TIMEOUT;
                StabilityTrace_Write(L"WARN", L"analog-host", L"test.child_reap_timeout.injected",
                    L"simulator_only=1 pid=%lu", pi.dwProcessId);
            }
#endif
            if (finalProcessWait != WAIT_OBJECT_0)
            {
                const DWORD waitError = finalProcessWait == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT;
                restartAllowed = false;
                g_client.restartBlocked.store(true, std::memory_order_release);
                InvalidateSnapshot(g_client.shared, Status_Error, static_cast<LONG>(waitError), g_client.snapshotEvent);
                StabilityTrace_WriteCritical(L"ERROR", L"analog-host", L"child.reap_timeout",
                    L"pid=%lu wait=%lu win32=%lu process_handle_retained=1 restart_blocked=1",
                    pi.dwProcessId, finalProcessWait, waitError);
                DebugLog_Write(L"[analog.host] child did not confirm exit; retaining process handle and blocking restart");

                do
                {
                    if (g_client.stopping.load(std::memory_order_acquire) && g_client.job)
                        TerminateJobObject(g_client.job, 0xE0485245u);
                    finalProcessWait = WaitForSingleObject(pi.hProcess, 250);
                    if (finalProcessWait == WAIT_FAILED)
                        Sleep(250);
                } while (finalProcessWait != WAIT_OBJECT_0);

                StabilityTrace_Write(L"INFO", L"analog-host", L"child.reaped_after_timeout",
                    L"pid=%lu restart_blocked=1", pi.dwProcessId);
            }
            GetExitCodeProcess(pi.hProcess, &processExitCode);
            if (debugMainThreadHandle && debugMainThreadHandle != pi.hThread)
                CloseHandle(debugMainThreadHandle);
            if (pi.hThread)
                CloseHandle(pi.hThread);
            if (debugProcessHandle && debugProcessHandle != pi.hProcess)
                CloseHandle(debugProcessHandle);
            CloseHandle(pi.hProcess);

            if (g_client.shared)
            {
                InvalidateSnapshot(g_client.shared,
                    g_client.stopping.load(std::memory_order_acquire) ? Status_Stopped : Status_Restarting,
                    static_cast<LONG>(processExitCode), g_client.snapshotEvent);
            }
            DebugLog_Write(L"[analog.host] exited code=0x%08lX captured=%d restart=%d",
                processExitCode, crashCaptured ? 1 : 0,
                g_client.stopping.load(std::memory_order_acquire) ? 0 : 1);
            HostLog(L"supervisor child exit pid=%lu code=0x%08lX captured=%d restart=%d",
                pi.dwProcessId, processExitCode, crashCaptured ? 1 : 0,
                g_client.stopping.load(std::memory_order_acquire) ? 0 : 1);
            StabilityTrace_Write(L"INFO", L"analog-host", L"child.exit",
                L"pid=%lu code=0x%08lX captured=%d restart=%d",
                pi.dwProcessId, processExitCode, crashCaptured ? 1 : 0,
                g_client.stopping.load(std::memory_order_acquire) ? 0 : 1);

            if (g_client.stopping.load(std::memory_order_acquire) || !restartAllowed ||
                g_client.restartBlocked.load(std::memory_order_acquire))
                break;
            ++restartCount;
            if (WaitForSingleObject(g_client.stopEvent, kRestartDelayMs) == WAIT_OBJECT_0)
                break;
        }

        if (g_client.shared)
        {
            const bool blocked = g_client.restartBlocked.load(std::memory_order_acquire);
            InvalidateSnapshot(g_client.shared, blocked ? Status_Error : Status_Stopped,
                blocked ? WootingAnalogResult_Failure : 0, g_client.snapshotEvent);
        }
        HostLog(L"supervisor exit");
        return 0;
    }

    void PublishParentWorkerFault(const wchar_t* worker, LONG error,
        const char* message) noexcept
    {
        g_client.restartBlocked.store(true, std::memory_order_release);
        InvalidateSnapshot(g_client.shared, Status_Error, error, g_client.snapshotEvent);
        if (g_client.stopEvent)
            SetEvent(g_client.stopEvent);
        if (g_client.snapshotEvent)
            SetEvent(g_client.snapshotEvent);
        StabilityTrace_WriteCritical(L"ERROR", L"analog-host", L"parent_worker.fault",
            L"worker=%s error=0x%08lX neutralized=1 restart_blocked=1", worker, error);
        OutputDebugStringA("[HallJoy] analog-host parent worker fault: ");
        OutputDebugStringA(message ? message : "unknown");
        OutputDebugStringA("\n");
    }

    void SupervisorWorkerOnCppFault(
        const halljoy::worker::WorkerExceptionRecord& record) noexcept
    {
        PublishParentWorkerFault(L"supervisor",
            record.kind == halljoy::worker::WorkerExceptionKind::StandardException
                ? static_cast<LONG>(0xE0485301u)
                : static_cast<LONG>(0xE0485302u),
            record.message);
    }

    void SupervisorWorkerOnCompletion(
        const halljoy::worker::WorkerExceptionRecord& record) noexcept
    {
        StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
            L"analog-host", L"worker.exit", L"worker=supervisor fault_kind=%u",
            static_cast<unsigned>(record.kind));
    }

    DWORD SupervisorThreadProcCpp() noexcept
    {
        return static_cast<DWORD>(halljoy::worker::RunWorkerEntryBarrier(
            [] { return static_cast<std::uint32_t>(SupervisorThreadProcImpl()); },
            SupervisorWorkerOnCppFault,
            SupervisorWorkerOnCompletion,
            0xE0485303u));
    }

    DWORD WINAPI SupervisorThreadProc(LPVOID) noexcept
    {
        StabilityTrace_Write(L"INFO", L"analog-host", L"worker.start", L"worker=supervisor");
        __try
        {
            return SupervisorThreadProcCpp();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            const DWORD code = GetExceptionCode();
            PublishParentWorkerFault(L"supervisor", static_cast<LONG>(code), "structured exception");
            StabilityTrace_WriteCritical(L"ERROR", L"analog-host", L"worker.exit",
                L"worker=supervisor fault_kind=seh code=0x%08lX", code);
            return code;
        }
    }

    bool DenseSnapshot(std::array<float, kMaxKeys>& values,
        int& activeCount, WootingAnalog_DeviceID deviceId = 0)
    {
        SharedState* shared = g_client.shared;
        if (!shared || shared->magic != kMagic || shared->version != kVersion || shared->structSize != sizeof(SharedState))
            return false;
        if (InterlockedCompareExchange(&shared->status, 0, 0) != Status_Ready)
            return false;

        for (int attempt = 0; attempt < 5; ++attempt)
        {
            const LONG before = InterlockedCompareExchange(&shared->snapshotSequence, 0, 0);
            if (before & 1)
            {
                YieldProcessor();
                continue;
            }
            MemoryBarrier();
            bool found = deviceId == 0;
            if (deviceId == 0)
            {
                std::copy_n(shared->denseValues, kMaxKeys, values.begin());
            }
            else
            {
                values.fill(0.0f);
                const int deviceCount = std::clamp(static_cast<int>(shared->denseDeviceCount), 0, static_cast<int>(kMaxDevices));
                for (int di = 0; di < deviceCount; ++di)
                {
                    const auto& device = shared->denseDevices[di];
                    if (device.deviceId == deviceId && device.structSize == sizeof(HallJoyDenseSnapshot::DeviceV1) &&
                        device.version == HallJoyDenseSnapshot::kVersion)
                    {
                        std::copy_n(device.values, kMaxKeys, values.begin());
                        found = true;
                        break;
                    }
                }
            }
            MemoryBarrier();
            const LONG after = InterlockedCompareExchange(&shared->snapshotSequence, 0, 0);
            if (before == after && !(after & 1))
            {
                if (!found)
                    return false;
                activeCount = static_cast<int>(std::count_if(values.begin(), values.end(), [](float value) { return value > 0.0f; }));
                return true;
            }
        }
        return false;
    }

    bool Snapshot(std::array<unsigned short, kMaxKeys>& codes,
        std::array<float, kMaxKeys>& values, int& count, WootingAnalog_DeviceID deviceId = 0)
    {
        std::array<float, kMaxKeys> dense{};
        int activeCount = 0;
        if (!DenseSnapshot(dense, activeCount, deviceId))
            return false;
        count = 0;
        for (std::size_t code = 0; code < dense.size(); ++code)
        {
            if (dense[code] > 0.0f)
            {
                codes[static_cast<std::size_t>(count)] = static_cast<unsigned short>(code);
                values[static_cast<std::size_t>(count)] = dense[code];
                ++count;
            }
        }
        return true;
    }

    DWORD SnapshotBridgeThreadProcImpl()
    {
#if defined(HALLJOY_ANALOG_SIMULATOR)
        const wchar_t* commandLine = GetCommandLineW();
        if (commandLine && wcsstr(commandLine, L"--halljoy-test-analog-host-bridge-stop-timeout"))
        {
            StabilityTrace_Write(L"WARN", L"analog-host", L"test.bridge_stop_timeout.injected",
                L"simulator_only=1");
            Sleep(INFINITE);
        }
#endif
        HANDLE waits[2] = { g_client.stopEvent, g_client.snapshotEvent };
        while (!g_client.stopping.load(std::memory_order_acquire))
        {
            const DWORD result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
            if (result == WAIT_OBJECT_0)
                break;
            if (result == WAIT_OBJECT_0 + 1)
                RealtimeLoop_NotifyInputChanged();
            else
                break;
        }
        return 0;
    }

    void SnapshotBridgeWorkerOnCppFault(
        const halljoy::worker::WorkerExceptionRecord& record) noexcept
    {
        PublishParentWorkerFault(L"snapshot_bridge",
            record.kind == halljoy::worker::WorkerExceptionKind::StandardException
                ? static_cast<LONG>(0xE0484201u)
                : static_cast<LONG>(0xE0484202u),
            record.message);
    }

    void SnapshotBridgeWorkerOnCompletion(
        const halljoy::worker::WorkerExceptionRecord& record) noexcept
    {
        StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
            L"analog-host", L"worker.exit", L"worker=snapshot_bridge fault_kind=%u",
            static_cast<unsigned>(record.kind));
    }

    DWORD SnapshotBridgeThreadProcCpp() noexcept
    {
        return static_cast<DWORD>(halljoy::worker::RunWorkerEntryBarrier(
            [] { return static_cast<std::uint32_t>(SnapshotBridgeThreadProcImpl()); },
            SnapshotBridgeWorkerOnCppFault,
            SnapshotBridgeWorkerOnCompletion,
            0xE0484203u));
    }

    DWORD WINAPI SnapshotBridgeThreadProc(LPVOID) noexcept
    {
        StabilityTrace_Write(L"INFO", L"analog-host", L"worker.start", L"worker=snapshot_bridge");
        __try
        {
            return SnapshotBridgeThreadProcCpp();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            const DWORD code = GetExceptionCode();
            PublishParentWorkerFault(L"snapshot_bridge", static_cast<LONG>(code), "structured exception");
            StabilityTrace_WriteCritical(L"ERROR", L"analog-host", L"worker.exit",
                L"worker=snapshot_bridge fault_kind=seh code=0x%08lX", code);
            return code;
        }
    }

    bool ClientOwnsResourcesLocked() noexcept
    {
        return g_client.mapping || g_client.shared || g_client.stopEvent ||
            g_client.snapshotEvent || g_client.snapshotBridgeThread ||
            g_client.supervisorThread || g_client.supervisorReadyEvent || g_client.job;
    }

    void CloseClientResourcesLocked() noexcept
    {
        if (g_client.snapshotBridgeThread) CloseHandle(g_client.snapshotBridgeThread);
        if (g_client.supervisorThread) CloseHandle(g_client.supervisorThread);
        if (g_client.shared) UnmapViewOfFile(g_client.shared);
        if (g_client.mapping) CloseHandle(g_client.mapping);
        if (g_client.stopEvent) CloseHandle(g_client.stopEvent);
        if (g_client.snapshotEvent) CloseHandle(g_client.snapshotEvent);
        if (g_client.supervisorReadyEvent) CloseHandle(g_client.supervisorReadyEvent);
        if (g_client.job) CloseHandle(g_client.job);
        g_client.snapshotBridgeThread = nullptr;
        g_client.supervisorThread = nullptr;
        g_client.job = nullptr;
        g_client.shared = nullptr;
        g_client.mapping = nullptr;
        g_client.stopEvent = nullptr;
        g_client.snapshotEvent = nullptr;
        g_client.supervisorReadyEvent = nullptr;
    }

    bool WaitForClientWorkers(HANDLE bridge, HANDLE supervisor, DWORD timeoutMs, DWORD* waitResult) noexcept
    {
        HANDLE workers[2]{};
        DWORD count = 0;
        if (bridge) workers[count++] = bridge;
        if (supervisor) workers[count++] = supervisor;
        if (count == 0)
        {
            if (waitResult) *waitResult = WAIT_OBJECT_0;
            return true;
        }
        const DWORD result = WaitForMultipleObjects(count, workers, TRUE, timeoutMs);
        if (waitResult) *waitResult = result;
        return result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + count;
    }

    bool EnsureClientResources()
    {
        AcquireSRWLockExclusive(&g_client.lock);
        if (g_client.restartBlocked.load(std::memory_order_acquire))
        {
            ReleaseSRWLockExclusive(&g_client.lock);
            StabilityTrace_WriteCritical(L"ERROR", L"analog-host", L"start.rejected",
                L"restart_blocked=1");
            return false;
        }
        if (g_client.lifecycle.State() == halljoy::lifecycle::WorkerState::Running &&
            g_client.mapping && g_client.shared && g_client.stopEvent && g_client.snapshotEvent &&
            g_client.snapshotBridgeThread && g_client.supervisorThread)
        {
            ReleaseSRWLockExclusive(&g_client.lock);
            return true;
        }
        if (ClientOwnsResourcesLocked() || !g_client.lifecycle.RestartSafe())
        {
            const auto state = g_client.lifecycle.State();
            ReleaseSRWLockExclusive(&g_client.lock);
            StabilityTrace_WriteCritical(L"ERROR", L"analog-host", L"start.rejected",
                L"state=%u resources_owned=1", static_cast<unsigned>(state));
            return false;
        }

        const auto start = g_client.lifecycle.BeginStart();
        if (start.status != halljoy::lifecycle::StartStatus::Starting)
        {
            ReleaseSRWLockExclusive(&g_client.lock);
            return false;
        }

        g_client.privatePluginPath = EmbeddedAnalogStack_PrivatePluginPath();
        if (g_client.privatePluginPath.empty())
        {
            const DWORD error = EmbeddedAnalogStack_LastError();
            (void)g_client.lifecycle.FailStartBeforeWorker(start.generation, error);
            ReleaseSRWLockExclusive(&g_client.lock);
            DebugLog_Write(L"[analog.host] private UAP path is unavailable err=%lu",
                error);
            return false;
        }

        g_client.ownerPid = GetCurrentProcessId();
        LARGE_INTEGER qpc{};
        QueryPerformanceCounter(&qpc);
        g_client.nonce = (static_cast<unsigned long long>(qpc.QuadPart) << 17) ^
            (static_cast<unsigned long long>(GetTickCount64()) << 7) ^ g_client.ownerPid;
        if (g_client.nonce == 0) g_client.nonce = 1;
        g_client.mappingName = BuildIpcName(L"Map", g_client.ownerPid, g_client.nonce);
        g_client.stopEventName = BuildIpcName(L"Stop", g_client.ownerPid, g_client.nonce);
        g_client.snapshotEventName = BuildIpcName(L"Snapshot", g_client.ownerPid, g_client.nonce);

        g_client.job = CreateJobObjectW(nullptr, nullptr);
        if (g_client.job)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!SetInformationJobObject(g_client.job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
            {
                CloseHandle(g_client.job);
                g_client.job = nullptr;
            }
        }

        g_client.mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            static_cast<DWORD>(sizeof(SharedState)), g_client.mappingName.c_str());
        if (g_client.mapping)
            g_client.shared = static_cast<SharedState*>(MapViewOfFile(g_client.mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState)));
        g_client.stopEvent = CreateEventW(nullptr, TRUE, FALSE, g_client.stopEventName.c_str());
        g_client.snapshotEvent = CreateEventW(nullptr, FALSE, FALSE, g_client.snapshotEventName.c_str());
        g_client.supervisorReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!g_client.job || !g_client.mapping || !g_client.shared || !g_client.stopEvent ||
            !g_client.snapshotEvent || !g_client.supervisorReadyEvent)
        {
            const DWORD error = GetLastError();
            CloseClientResourcesLocked();
            (void)g_client.lifecycle.FailStartBeforeWorker(start.generation, error);
            ReleaseSRWLockExclusive(&g_client.lock);
            DebugLog_Write(L"[analog.host] IPC creation failed err=%lu", error);
            return false;
        }

        ZeroMemory(g_client.shared, sizeof(SharedState));
        g_client.shared->magic = kMagic;
        g_client.shared->version = kVersion;
        g_client.shared->structSize = sizeof(SharedState);
        g_client.shared->status = Status_Stopped;
        g_client.shared->requestedKeycodeMode = WootingAnalog_KeycodeType_HID;
        g_client.shared->appliedKeycodeMode = -1;
#if defined(HALLJOY_ANALOG_SIMULATOR)
        const wchar_t* commandLine = GetCommandLineW();
        g_client.injectSupervisorCppFault = commandLine &&
            wcsstr(commandLine, L"--halljoy-test-analog-host-supervisor-cpp-fault") != nullptr;
        g_client.injectChildReapTimeout = commandLine &&
            wcsstr(commandLine, L"--halljoy-test-analog-host-child-reap-timeout") != nullptr;
        if (commandLine &&
            wcsstr(commandLine, L"--halljoy-test-analog-host-child-cpp-fault") != nullptr)
        {
            g_client.shared->diagnosticCppFaultAfterPolls = 3;
        }
#else
        g_client.injectSupervisorCppFault = false;
        g_client.injectChildReapTimeout = false;
#endif
#if defined(HALLJOY_DIAGNOSTIC)
        if (wcsstr(GetCommandLineW(), L"--diagnostic-analog-host-crash-test"))
            g_client.shared->diagnosticCrashAfterPolls = 250;
#endif
        g_client.stopping.store(false, std::memory_order_release);
        ResetEvent(g_client.stopEvent);
        ResetEvent(g_client.snapshotEvent);
        g_client.snapshotBridgeThread = CreateThread(nullptr, 0, SnapshotBridgeThreadProc, nullptr, 0, nullptr);
        if (!g_client.snapshotBridgeThread)
        {
            const DWORD error = GetLastError();
            CloseClientResourcesLocked();
            (void)g_client.lifecycle.FailStartBeforeWorker(start.generation, error);
            ReleaseSRWLockExclusive(&g_client.lock);
            DebugLog_Write(L"[analog.host] snapshot bridge thread creation failed err=%lu", error);
            return false;
        }
#if defined(HALLJOY_ANALOG_SIMULATOR)
        const bool injectSupervisorStartFailure =
            wcsstr(GetCommandLineW(), L"--halljoy-test-analog-host-supervisor-start-failure") != nullptr;
#else
        constexpr bool injectSupervisorStartFailure = false;
#endif
        g_client.supervisorThread = injectSupervisorStartFailure
            ? nullptr
            : CreateThread(nullptr, 0, SupervisorThreadProc, nullptr, 0, nullptr);
        if (!g_client.supervisorThread)
        {
            const DWORD error = injectSupervisorStartFailure ? ERROR_NOT_ENOUGH_MEMORY : GetLastError();
            const auto requested = g_client.lifecycle.RequestStop(start.generation);
            g_client.stopping.store(true, std::memory_order_release);
            SetEvent(g_client.stopEvent);
            SetEvent(g_client.snapshotEvent);
            const HANDLE bridge = g_client.snapshotBridgeThread;
            ReleaseSRWLockExclusive(&g_client.lock);

            DWORD waitResult = WAIT_FAILED;
            const bool joined = requested.status == halljoy::lifecycle::StopStatus::StopRequested &&
                WaitForClientWorkers(bridge, nullptr, 3000, &waitResult);
            const DWORD waitError = waitResult == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT;

            AcquireSRWLockExclusive(&g_client.lock);
            if (joined)
            {
                CloseClientResourcesLocked();
                (void)g_client.lifecycle.ConfirmJoined(start.generation);
                StabilityTrace_Write(L"INFO", L"analog-host", L"partial_start.rollback",
                    L"stage=supervisor_create joined=1 resources_released=1 injected=%d",
                    injectSupervisorStartFailure ? 1 : 0);
            }
            else
            {
                g_client.restartBlocked.store(true, std::memory_order_release);
                (void)g_client.lifecycle.MarkPoisoned(start.generation,
                    halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
                    waitResult == WAIT_TIMEOUT
                        ? halljoy::lifecycle::LifecycleErrorCode::StopTimedOut
                        : halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed,
                    waitError);
                StabilityTrace_WriteCritical(L"ERROR", L"analog-host", L"partial_start.poisoned",
                    L"stage=supervisor_create wait=%lu bridge_handle_retained=1 ipc_retained=1 restart_blocked=1",
                    static_cast<unsigned long>(waitResult));
            }
            ReleaseSRWLockExclusive(&g_client.lock);
            DebugLog_Write(L"[analog.host] supervisor thread creation failed err=%lu", error);
            return false;
        }
        const auto running = g_client.lifecycle.ConfirmRunning(start.generation);
        if (!running.IsRunning())
        {
            g_client.restartBlocked.store(true, std::memory_order_release);
            g_client.stopping.store(true, std::memory_order_release);
            SetEvent(g_client.stopEvent);
            SetEvent(g_client.snapshotEvent);
            (void)g_client.lifecycle.MarkPoisoned(start.generation,
                halljoy::lifecycle::LifecycleOperation::ConfirmRunning,
                halljoy::lifecycle::LifecycleErrorCode::InvalidTransition);
            ReleaseSRWLockExclusive(&g_client.lock);
            return false;
        }
        const HANDLE supervisorReadyEvent = g_client.supervisorReadyEvent;
        ReleaseSRWLockExclusive(&g_client.lock);

        WaitForSingleObject(supervisorReadyEvent, 2000);
        DebugLog_Write(L"[analog.host] isolation client started map=%s snapshot_event=%s", g_client.mappingName.c_str(), g_client.snapshotEventName.c_str());
        return true;
    }
}

void AnalogHostClient_ResetDiagnosticFiles()
{
#if defined(HALLJOY_DIAGNOSTIC)
    DeleteFileW(BuildPathNearExe(L"HallJoyAnalogHost.log").c_str());
    DeleteFileW(BuildPathNearExe(L"HallJoyAnalogHostCrashLatest.txt").c_str());
    DeleteFileW(BuildPathNearExe(L"HallJoyAnalogHostCrashLatest.dmp").c_str());
    DeleteFilesNearExe(L"HallJoyAnalogHostCrash_*.txt");
    DeleteFilesNearExe(L"HallJoyAnalogHostCrash_*.dmp");
#endif
}

bool AnalogHost_TryRunCommand(int& exitCode)
{
    DWORD ownerPid = 0;
    unsigned long long nonce = 0;
    std::wstring privatePluginPath;
    if (!ParseHostCommand(ownerPid, nonce, privatePluginPath))
        return false;
    exitCode = RunHost(ownerPid, nonce, privatePluginPath);
    return true;
}

int AnalogHostClient_Initialise()
{
    if (!EnsureClientResources())
        return WootingAnalogResult_Failure;

    const ULONGLONG deadline = GetTickCount64() + kHostReadyTimeoutMs;
    while (GetTickCount64() < deadline)
    {
        const LONG status = InterlockedCompareExchange(&g_client.shared->status, 0, 0);
        if (status == Status_Ready)
        {
            const int result = InterlockedCompareExchange(&g_client.shared->initResult, 0, 0);
            DebugLog_Write(L"[analog.host] ready init_result=%d pid=%ld restarts=%ld",
                result,
                InterlockedCompareExchange(&g_client.shared->hostPid, 0, 0),
                InterlockedCompareExchange(&g_client.shared->restartCount, 0, 0));
            return result >= 0 ? result : WootingAnalogResult_Failure;
        }
        if (status == Status_Error)
        {
            const int result = InterlockedCompareExchange(&g_client.shared->initResult, 0, 0);
            return result < 0 ? result : WootingAnalogResult_Failure;
        }
        Sleep(20);
    }
    DebugLog_Write(L"[analog.host] initialise timed out status=%ld checkpoint=%s",
        InterlockedCompareExchange(&g_client.shared->status, 0, 0),
        CheckpointName(InterlockedCompareExchange(&g_client.shared->checkpoint, 0, 0)));
    return WootingAnalogResult_Failure;
}

bool AnalogHostClient_IsInitialised()
{
    return g_client.shared &&
        InterlockedCompareExchange(&g_client.shared->status, 0, 0) == Status_Ready;
}

bool AnalogHostClient_GetTelemetry(AnalogHostTelemetry* out)
{
    if (!out)
        return false;

    AnalogHostTelemetry result{};
    SharedState* shared = g_client.shared;
    if (!shared || shared->magic != kMagic || shared->version != kVersion || shared->structSize != sizeof(SharedState))
    {
        *out = result;
        return false;
    }

    result.available = true;
    result.status = InterlockedCompareExchange(&shared->status, 0, 0);
    result.ready = result.status == Status_Ready;
    result.initResult = InterlockedCompareExchange(&shared->initResult, 0, 0);
    result.lastError = InterlockedCompareExchange(&shared->lastError, 0, 0);
    result.transportError = InterlockedCompareExchange(&shared->transportError, 0, 0);
    result.restartCount = InterlockedCompareExchange(&shared->restartCount, 0, 0);
    result.invalidSnapshotCount = InterlockedCompareExchange(&shared->invalidSnapshotCount, 0, 0);
    result.totalPolls = static_cast<std::uint64_t>(InterlockedCompareExchange64(&shared->totalPolls, 0, 0));
    result.totalSuccessfulPolls = static_cast<std::uint64_t>(InterlockedCompareExchange64(&shared->totalSuccessfulPolls, 0, 0));

    const ULONGLONG nowMs = GetTickCount64();
    const LONG64 lastPublish = InterlockedCompareExchange64(&shared->lastPublishTickMs, 0, 0);
    if (lastPublish > 0 && nowMs >= static_cast<ULONGLONG>(lastPublish))
    {
        result.lastPublishAgeMs = static_cast<std::uint32_t>(
            std::min<ULONGLONG>(nowMs - static_cast<ULONGLONG>(lastPublish), 0xffffffffull));
    }

    for (int attempt = 0; attempt < 5; ++attempt)
    {
        const LONG before = InterlockedCompareExchange(&shared->snapshotSequence, 0, 0);
        if (before & 1)
        {
            YieldProcessor();
            continue;
        }
        MemoryBarrier();
        result.activeKeyCount = std::clamp(static_cast<int>(shared->denseActiveKeyCount), 0, static_cast<int>(kMaxKeys));
        result.denseDeviceCount = std::clamp(static_cast<int>(shared->denseDeviceCount), 0, static_cast<int>(kMaxDevices));
        result.snapshotGeneration = static_cast<std::uint64_t>(InterlockedCompareExchange64(&shared->snapshotGeneration, 0, 0));
        result.snapshotTimestampUs = static_cast<std::uint64_t>(InterlockedCompareExchange64(&shared->snapshotTimestampUs, 0, 0));
        result.deviceCount = std::clamp(static_cast<int>(shared->deviceTelemetryCount), 0, static_cast<int>(kMaxDevices));
        for (int i = 0; i < result.deviceCount; ++i)
        {
            result.devices[static_cast<size_t>(i)] = shared->deviceTelemetry[i];
        }
        MemoryBarrier();
        const LONG after = InterlockedCompareExchange(&shared->snapshotSequence, 0, 0);
        if (before == after && !(after & 1))
            break;
        result.deviceCount = 0;
    }

    AcquireSRWLockExclusive(&g_telemetryRate.lock);
    if (g_telemetryRate.lastSampleMs == 0 ||
        result.totalPolls < g_telemetryRate.lastPolls ||
        result.totalSuccessfulPolls < g_telemetryRate.lastSuccessful)
    {
        g_telemetryRate.lastSampleMs = nowMs;
        g_telemetryRate.lastPolls = result.totalPolls;
        g_telemetryRate.lastSuccessful = result.totalSuccessfulPolls;
        g_telemetryRate.pollHz10 = 0;
        g_telemetryRate.successfulHz10 = 0;
    }
    else
    {
        const ULONGLONG elapsedMs = nowMs - g_telemetryRate.lastSampleMs;
        if (elapsedMs >= 250)
        {
            const std::uint64_t deltaPolls = result.totalPolls - g_telemetryRate.lastPolls;
            const std::uint64_t deltaSuccessful = result.totalSuccessfulPolls - g_telemetryRate.lastSuccessful;
            g_telemetryRate.pollHz10 = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                1000000ull, (deltaPolls * 10000ull + elapsedMs / 2ull) / elapsedMs));
            g_telemetryRate.successfulHz10 = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                1000000ull, (deltaSuccessful * 10000ull + elapsedMs / 2ull) / elapsedMs));
            g_telemetryRate.lastSampleMs = nowMs;
            g_telemetryRate.lastPolls = result.totalPolls;
            g_telemetryRate.lastSuccessful = result.totalSuccessfulPolls;
        }
    }
    result.hostPollHz10 = g_telemetryRate.pollHz10;
    result.hostSuccessfulPollHz10 = g_telemetryRate.successfulHz10;
    ReleaseSRWLockExclusive(&g_telemetryRate.lock);

    *out = result;
    return true;
}

WootingAnalogResult AnalogHostClient_Uninitialise()
{
    AcquireSRWLockExclusive(&g_client.lock);
    const auto lifecycleState = g_client.lifecycle.State();
    if (!ClientOwnsResourcesLocked() &&
        (lifecycleState == halljoy::lifecycle::WorkerState::Stopped ||
         lifecycleState == halljoy::lifecycle::WorkerState::Joined))
    {
        ReleaseSRWLockExclusive(&g_client.lock);
        return WootingAnalogResult_Ok;
    }
    if (lifecycleState == halljoy::lifecycle::WorkerState::Poisoned)
    {
        if (g_client.stopEvent) SetEvent(g_client.stopEvent);
        if (g_client.snapshotEvent) SetEvent(g_client.snapshotEvent);
        ReleaseSRWLockExclusive(&g_client.lock);
        return WootingAnalogResult_Failure;
    }

    const auto generation = g_client.lifecycle.Generation();
    const auto requested = g_client.lifecycle.RequestStop(generation);
    if (requested.status != halljoy::lifecycle::StopStatus::StopRequested)
    {
        g_client.restartBlocked.store(true, std::memory_order_release);
        ReleaseSRWLockExclusive(&g_client.lock);
        return WootingAnalogResult_Failure;
    }

    HostLog(L"client shutdown begin");
    g_client.stopping.store(true, std::memory_order_release);
    if (g_client.stopEvent) SetEvent(g_client.stopEvent);
    if (g_client.snapshotEvent) SetEvent(g_client.snapshotEvent);
    const HANDLE stopEvent = g_client.stopEvent;
    const HANDLE snapshotEvent = g_client.snapshotEvent;
    const HANDLE supervisor = g_client.supervisorThread;
    const HANDLE snapshotBridge = g_client.snapshotBridgeThread;
    const HANDLE job = g_client.job;
    ReleaseSRWLockExclusive(&g_client.lock);

    DWORD waitResult = WAIT_FAILED;
    bool cleanThreadShutdown = WaitForClientWorkers(snapshotBridge, supervisor, 6000, &waitResult);
    if (!cleanThreadShutdown)
    {
        const DWORD waitError = waitResult == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT;
        DebugLog_Write(L"[analog.host] worker group stop wait result=0x%08lX err=%lu; terminating child job and retrying",
            waitResult, waitError);
        if (stopEvent) SetEvent(stopEvent);
        if (snapshotEvent) SetEvent(snapshotEvent);
        if (job) TerminateJobObject(job, 0xE051484Fu);
        cleanThreadShutdown = WaitForClientWorkers(snapshotBridge, supervisor, 4000, &waitResult);
    }
    const DWORD waitError = waitResult == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT;

    AcquireSRWLockExclusive(&g_client.lock);
    if (!cleanThreadShutdown)
    {
        g_client.restartBlocked.store(true, std::memory_order_release);
        const auto poisoned = g_client.lifecycle.MarkPoisoned(generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            waitResult == WAIT_TIMEOUT
                ? halljoy::lifecycle::LifecycleErrorCode::StopTimedOut
                : halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed,
            waitError);
        ReleaseSRWLockExclusive(&g_client.lock);
        StabilityTrace_WriteCritical(L"ERROR", L"analog-host", L"stop.timeout",
            L"generation=%llu wait=%lu win32=%lu bridge_handle_retained=%d supervisor_handle_retained=%d ipc_retained=1 job_retained=%d restart_blocked=1",
            static_cast<unsigned long long>(poisoned.generation.Value()),
            static_cast<unsigned long>(waitResult), static_cast<unsigned long>(waitError),
            snapshotBridge ? 1 : 0, supervisor ? 1 : 0, job ? 1 : 0);
        DebugLog_Write(L"[analog.host] parent worker group did not join; resources retained and restart blocked");
        HostLog(L"client shutdown poisoned; deferred process cleanup");
        return WootingAnalogResult_Failure;
    }
    CloseClientResourcesLocked();
    const auto joined = g_client.lifecycle.ConfirmJoined(generation);
    ReleaseSRWLockExclusive(&g_client.lock);
    StabilityTrace_Write(L"INFO", L"analog-host", L"stop.joined",
        L"generation=%llu workers=2 resources_released=1",
        static_cast<unsigned long long>(joined.generation.Value()));
    DebugLog_Write(L"[analog.host] isolation client stopped");
    HostLog(L"client shutdown complete");
    return joined.RestartSafe() ? WootingAnalogResult_Ok : WootingAnalogResult_Failure;
}

WootingAnalogResult AnalogHostClient_SetKeycodeMode(WootingAnalog_KeycodeType mode)
{
    if (!g_client.shared || !AnalogHostClient_IsInitialised())
        return WootingAnalogResult_UnInitialized;
    if (mode < WootingAnalog_KeycodeType_HID || mode > WootingAnalog_KeycodeType_VirtualKeyTranslate)
        return WootingAnalogResult_InvalidArgument;
    InterlockedExchange(&g_client.shared->requestedKeycodeMode, static_cast<LONG>(mode));
    const ULONGLONG deadline = GetTickCount64() + 500;
    while (GetTickCount64() < deadline)
    {
        if (InterlockedCompareExchange(&g_client.shared->appliedKeycodeMode, 0, 0) == static_cast<LONG>(mode))
            return WootingAnalogResult_Ok;
        if (!AnalogHostClient_IsInitialised())
            return WootingAnalogResult_UnInitialized;
        Sleep(1);
    }
    return WootingAnalogResult_Failure;
}

int AnalogHostClient_GetConnectedDevicesInfo(WootingAnalog_DeviceInfo_FFI** buffer, unsigned int len)
{
    if (!buffer || len == 0)
        return len == 0 ? 0 : WootingAnalogResult_InvalidArgument;
    std::fill_n(buffer, len, nullptr);

    SharedState* shared = g_client.shared;
    if (!shared || !AnalogHostClient_IsInitialised())
        return WootingAnalogResult_UnInitialized;

    std::array<HallJoyPluginTelemetry::DeviceV1, kMaxDevices> devices{};
    int count = 0;
    bool copied = false;
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        const LONG before = InterlockedCompareExchange(&shared->snapshotSequence, 0, 0);
        if (before & 1)
        {
            YieldProcessor();
            continue;
        }
        MemoryBarrier();
        count = std::clamp(static_cast<int>(shared->deviceTelemetryCount), 0,
            static_cast<int>((std::min)(static_cast<unsigned int>(kMaxDevices), len)));
        for (int i = 0; i < count; ++i)
            devices[static_cast<std::size_t>(i)] = shared->deviceTelemetry[i];
        MemoryBarrier();
        const LONG after = InterlockedCompareExchange(&shared->snapshotSequence, 0, 0);
        if (before == after && !(after & 1))
        {
            copied = true;
            break;
        }
    }
    if (!copied)
        return WootingAnalogResult_Failure;
    if (count == 0)
        return WootingAnalogResult_NoDevices;

    AcquireSRWLockExclusive(&g_deviceInfoCache.lock);
    for (int i = 0; i < count; ++i)
    {
        const auto& source = devices[static_cast<std::size_t>(i)];
        auto& manufacturer = g_deviceInfoCache.manufacturers[static_cast<std::size_t>(i)];
        auto& name = g_deviceInfoCache.names[static_cast<std::size_t>(i)];
        manufacturer.fill('\0');
        name.fill('\0');
        std::copy_n(source.manufacturer, (std::min)(manufacturer.size() - 1, sizeof(source.manufacturer)), manufacturer.data());
        std::copy_n(source.name, (std::min)(name.size() - 1, sizeof(source.name)), name.data());

        auto& info = g_deviceInfoCache.info[static_cast<std::size_t>(i)];
        info.vendor_id = source.vendorId;
        info.product_id = source.productId;
        info.manufacturer_name = manufacturer.data();
        info.device_name = name.data();
        info.device_id = source.deviceId;
        info.device_type = WootingAnalog_DeviceType_Keyboard;
        buffer[i] = &info;
    }
    ReleaseSRWLockExclusive(&g_deviceInfoCache.lock);
    return count;
}

float AnalogHostClient_ReadAnalog(unsigned short code)
{
    if (code >= kMaxKeys)
        return static_cast<float>(WootingAnalogResult_InvalidArgument);
    std::array<float, kMaxKeys> values{};
    int activeCount = 0;
    if (!DenseSnapshot(values, activeCount))
        return static_cast<float>(WootingAnalogResult_UnInitialized);
    return values[code];
}

float AnalogHostClient_ReadAnalogDevice(unsigned short code, WootingAnalog_DeviceID deviceId)
{
    if (code >= kMaxKeys)
        return static_cast<float>(WootingAnalogResult_InvalidArgument);
    if (deviceId == 0)
        return AnalogHostClient_ReadAnalog(code);
    std::array<float, kMaxKeys> values{};
    int activeCount = 0;
    if (!DenseSnapshot(values, activeCount, deviceId))
        return static_cast<float>(WootingAnalogResult_NoDevices);
    return values[code];
}

int AnalogHostClient_ReadFullBuffer(unsigned short* codeBuffer, float* analogBuffer, unsigned int len)
{
    if (!codeBuffer || !analogBuffer || len == 0)
        return len == 0 ? 0 : WootingAnalogResult_InvalidArgument;
    std::array<unsigned short, kMaxKeys> codes{};
    std::array<float, kMaxKeys> values{};
    int count = 0;
    if (!Snapshot(codes, values, count))
        return WootingAnalogResult_UnInitialized;
    const int n = std::min(count, static_cast<int>(len));
    for (int i = 0; i < n; ++i)
    {
        codeBuffer[i] = codes[static_cast<size_t>(i)];
        analogBuffer[i] = values[static_cast<size_t>(i)];
    }
    return n;
}

int AnalogHostClient_ReadFullBufferDevice(unsigned short* codeBuffer, float* analogBuffer, unsigned int len, WootingAnalog_DeviceID deviceId)
{
    if (!codeBuffer || !analogBuffer || len == 0)
        return len == 0 ? 0 : WootingAnalogResult_InvalidArgument;
    if (deviceId == 0)
        return AnalogHostClient_ReadFullBuffer(codeBuffer, analogBuffer, len);
    std::array<unsigned short, kMaxKeys> codes{};
    std::array<float, kMaxKeys> values{};
    int count = 0;
    if (!Snapshot(codes, values, count, deviceId))
        return WootingAnalogResult_NoDevices;
    const int n = std::min(count, static_cast<int>(len));
    for (int i = 0; i < n; ++i)
    {
        codeBuffer[i] = codes[static_cast<size_t>(i)];
        analogBuffer[i] = values[static_cast<size_t>(i)];
    }
    return n;
}
