// backend.cpp
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <ViGEm/Client.h>

#include "backend.h"
#include "bindings.h"
#include "settings.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "mouse_bind_codes.h"
#include "backend_curve.h"
#include "analog_host_client.h"
#include "realtime_loop.h"
#include "hid_io_operation.h"
#include "addressed_analog_backend.h"
#include "mad68pr_backend.h"
#include "hex80_backend.h"
#include "native_analog_routing.h"
#include "native_analog_backend_registry.h"
#include "latest_value_mailbox.h"
#include "vigem_output_scheduler.h"
#include "worker_exception_barrier.h"
#include "worker_join_policy.h"
#if defined(HALLJOY_ANALOG_SIMULATOR)
#include "analog_simulator_backend.h"
#endif

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

static PVIGEM_CLIENT g_client = nullptr;
static constexpr int kMaxVirtualPads = 4;
static std::array<PVIGEM_TARGET, kMaxVirtualPads> g_pads{};
static std::atomic<int> g_virtualPadCount{ 1 };
static std::atomic<bool> g_virtualPadsEnabled{ true };
static int g_connectedPadCount = 0;

static std::array<XUSB_REPORT, kMaxVirtualPads> g_reports{};
static std::array<XUSB_REPORT, kMaxVirtualPads> g_lastSentReports{};
static std::array<LONGLONG, kMaxVirtualPads> g_lastSentQpc{};
static std::array<uint8_t, kMaxVirtualPads> g_lastSentValid{};
static std::array<VigemOutputScheduler, kMaxVirtualPads> g_outputSchedulers{};

struct VigemOutputBatch
{
    uint8_t count = 0;
    uint8_t validMask = 0;
    std::array<XUSB_REPORT, kMaxVirtualPads> reports{};
};

static halljoy::output::LatestValueMailbox<VigemOutputBatch> g_vigemOutputMailbox;
static std::mutex g_vigemOutputLifecycleMutex;
static halljoy::lifecycle::WorkerLifecycle g_vigemOutputLifecycle;
static HANDLE g_vigemOutputThread = nullptr;
static HANDLE g_vigemOutputWakeEvent = nullptr;
static std::atomic<bool> g_vigemOutputRun{ false };
static std::atomic<bool> g_vigemOutputThreadAlive{ false };
static std::atomic<bool> g_vigemEmergencyNeutralRequested{ false };
static std::atomic<bool> g_vigemResubmitRequested{ false };
static std::atomic<halljoy::worker::WorkerExceptionKind> g_vigemOutputFaultKind{
    halljoy::worker::WorkerExceptionKind::None };
static halljoy::worker::WorkerExceptionRecord g_vigemOutputFaultRecord{};
#if defined(HALLJOY_ANALOG_SIMULATOR)
static std::atomic<bool> g_vigemTestStallInjected{ false };
#endif

// Thread-safe last-report snapshot (writer: realtime thread, reader: UI thread).
// The realtime writer uses a non-blocking try-lock. If the UI is preempted while
// reading, only that diagnostic publication is skipped; ViGEm output never waits.
static SRWLOCK g_lastReportLock = SRWLOCK_INIT;
static std::array<XUSB_REPORT, kMaxVirtualPads> g_lastReport{};
static std::array<std::atomic<SHORT>, kMaxVirtualPads> g_lastRX{};

static void PublishLastReport(int padIndex, const XUSB_REPORT& report)
{
    const size_t index = static_cast<size_t>(padIndex);
    g_lastRX[index].store(report.sThumbRX, std::memory_order_release);
    if (TryAcquireSRWLockExclusive(&g_lastReportLock))
    {
        g_lastReport[index] = report;
        ReleaseSRWLockExclusive(&g_lastReportLock);
    }
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
static void TraceSimulatorPipelineReport(const XUSB_REPORT& report)
{
    using halljoy::analog_simulator::Phase;
    static std::array<bool, static_cast<std::size_t>(Phase::Complete) + 1> traced{};
    const Phase phase = AnalogSimulator_GetCurrentPhase();
    const std::size_t index = static_cast<std::size_t>(phase);
    if (index >= traced.size() || traced[index])
        return;

    const bool stickNeutral = report.sThumbLX == 0 && report.sThumbLY == 0;
    bool accepted = false;
    switch (phase)
    {
    case Phase::Neutral:
    case Phase::Disconnected:
    case Phase::Reconnected:
    case Phase::SourceFault:
    case Phase::Recovered:
    case Phase::Complete:
        accepted = stickNeutral;
        break;
    case Phase::WRamp:
    case Phase::WHold:
    case Phase::WRelease:
    case Phase::PostReconnectInput:
        accepted = report.sThumbLY != 0;
        break;
    case Phase::OpposingWS:
        accepted = report.sThumbLY == 0;
        break;
    case Phase::OpposingAD:
        accepted = report.sThumbLX == 0;
        break;
    case Phase::Diagonal:
        accepted = report.sThumbLX != 0 && report.sThumbLY != 0;
        break;
    }
    if (!accepted)
        return;

    traced[index] = true;
    StabilityTrace_Write(L"INFO", L"analog-simulator", L"pipeline-report.observed",
        L"phase=%s lx=%d ly=%d simulated=1 hardware=0",
        halljoy::analog_simulator::PhaseName(phase),
        static_cast<int>(report.sThumbLX), static_cast<int>(report.sThumbLY));
}

static void TraceAcceptedSimulatorVigemUpdate(const XUSB_REPORT& report)
{
    static bool nonNeutralAccepted = false;
    static bool neutralAfterInputAccepted = false;
    const bool neutral = report.wButtons == 0 && report.bLeftTrigger == 0 &&
        report.bRightTrigger == 0 && report.sThumbLX == 0 && report.sThumbLY == 0 &&
        report.sThumbRX == 0 && report.sThumbRY == 0;
    if (!neutral && !nonNeutralAccepted)
    {
        nonNeutralAccepted = true;
        StabilityTrace_Write(L"INFO", L"analog-simulator", L"vigem-report.accepted",
            L"state=non-neutral simulated=1 hardware=0");
    }
    else if (neutral && nonNeutralAccepted && !neutralAfterInputAccepted)
    {
        neutralAfterInputAccepted = true;
        StabilityTrace_Write(L"INFO", L"analog-simulator", L"vigem-report.accepted",
            L"state=neutral-after-input simulated=1 hardware=0");
    }
}
#endif

// ---- UI snapshot ----
static std::array<std::atomic<uint16_t>, 256> g_uiAnalogM{}; // filtered output (after curve)
static std::array<std::atomic<uint16_t>, 256> g_uiRawM{};    // NEW: raw input
static std::array<std::atomic<uint64_t>, 4>   g_uiDirty{};   // dirty for filtered only

// list of HID codes to track (provided by UI)
static std::array<uint16_t, 256> g_trackedList{};
static std::atomic<int>          g_trackedCount{ 0 };

// bind-capture state (layout editor)
static std::atomic<bool>         g_bindCaptureEnabled{ false };
static std::atomic<uint32_t>     g_bindCapturedPacked{ 0 }; // low16=hid, high16=rawMilli
static std::atomic<bool>         g_bindHadDown{ false };

// ---- status / reconnect ----
static std::atomic<bool>         g_vigemOk{ false };
static std::atomic<VIGEM_ERROR>  g_vigemLastErr{ VIGEM_ERROR_NONE };
static std::atomic<uint32_t>     g_lastInitIssues{ BackendInitIssue_None };
static std::atomic<bool>         g_reconnectRequested{ false }; // immediate reconnect (settings change)
static std::atomic<bool>         g_deviceChangeReconnectRequested{ false }; // throttled reconnect (WM_DEVICECHANGE)
static std::atomic<ULONGLONG>    g_ignoreDeviceChangeUntilMs{ 0 };
static int                       g_vigemUpdateFailStreak = 0;
static ULONGLONG                 g_lastReconnectAttemptMs = 0;
static std::atomic<int>          g_lastAnalogErrorCode{ 0 };
static std::atomic<ULONGLONG>    g_lastAnalogErrorLogMs{ 0 };
static std::atomic<ULONGLONG>    g_lastWootingStateLogMs{ 0 };
static std::atomic<ULONGLONG>    g_lastInputStateLogMs{ 0 };
static std::atomic<int>          g_keycodeMode{ (int)WootingAnalog_KeycodeType_HID };
static std::atomic<ULONGLONG>    g_lastKeycodeSwitchMs{ 0 };
static std::atomic<uint32_t>     g_keyboardEventSeq{ 0 };
static std::atomic<uint16_t>     g_keyboardEventHid{ 0 };
static std::atomic<uint16_t>     g_keyboardEventScan{ 0 };
static std::atomic<uint16_t>     g_keyboardEventVk{ 0 };
static std::array<std::atomic<uint16_t>, 256> g_hidToScan{};
static std::array<std::atomic<uint16_t>, 256> g_hidToVk{};
// Physical keyboard state is shared by generic HallJoy diagnostics and input routing.
// Keep it in the common backend rather than in a device-specific implementation.
static std::array<std::atomic<uint8_t>, 256> g_physicalDown{};
static std::atomic<ULONGLONG>    g_lastFullBufferLogMs{ 0 };
static std::atomic<int>          g_zeroProbeStreak{ 0 };
static std::atomic<bool>         g_autoRecoverTried{ false };
static std::array<WootingAnalog_DeviceID, 16> g_knownDeviceIds{};
static std::atomic<int>          g_knownDeviceCount{ 0 };
static std::atomic<uint16_t>     g_tmTrackedMaxRawMilli{ 0 };
static std::atomic<uint16_t>     g_tmTrackedMaxOutMilli{ 0 };
static std::atomic<int>          g_tmFullBufferRet{ 0 };
static std::atomic<uint16_t>     g_tmFullBufferMaxMilli{ 0 };
static std::atomic<int>          g_tmFullBufferDeviceBestRet{ 0 };
static std::atomic<uint16_t>     g_tmFullBufferDeviceBestMaxMilli{ 0 };
static std::atomic<bool>         g_digitalFallbackWarnPending{ false };
static std::atomic<bool>         g_keycodeModeLocked{ false };
static constexpr bool            kEnableAdaptiveKeycodeModeProbe = false;
static constexpr bool            kEnableFullBufferAssist = false;
static constexpr bool            kEnableDeviceInfoQuery = true; // V11 isolated dense snapshots provide stable process-local IDs
static constexpr bool            kEnableFullBufferTelemetry = false;
#if defined(HALLJOY_MADLIONS_DIAGNOSTIC)
// V9: the isolated host owns every blocking HID transaction. Reading its shared
// snapshot is non-blocking, so consume it on every realtime tick instead of
// imposing the old 8 ms / 125 Hz cache interval.
static constexpr bool            kPreferFullBufferSnapshot = true;
static constexpr UINT            kMadlionsSnapshotPeriodMs = 0;
#else
static constexpr bool            kPreferFullBufferSnapshot = false;
static constexpr UINT            kMadlionsSnapshotPeriodMs = 0;
#endif
static POINT                     g_mouseLastPos{};
static bool                      g_mouseHasLastPos = false;
static std::atomic<bool>         g_mouseSawRawInput{ false };
static float                     g_mouseFilteredX = 0.0f;
static float                     g_mouseFilteredY = 0.0f;
static double                    g_mouseTargetX = 0.0;   // integrated mouse displacement (virtual cursor)
static double                    g_mouseTargetY = 0.0;
static double                    g_mouseFollowerX = 0.0; // virtual "stick-controlled" anchor
static double                    g_mouseFollowerY = 0.0;
static ULONGLONG                 g_mouseLastTickMs = 0;
static std::atomic<int>          g_mouseRawAccumDx{ 0 };
static std::atomic<int>          g_mouseRawAccumDy{ 0 };
static std::atomic<uint8_t>      g_mouseBindButtons[5]{};
static std::atomic<ULONGLONG>    g_mouseWheelPulseUpUntilMs{ 0 };
static std::atomic<ULONGLONG>    g_mouseWheelPulseDownUntilMs{ 0 };
static std::atomic<uint8_t>      g_mouseDbgEnabled{ 0 };
static std::atomic<uint8_t>      g_mouseDbgUsingRaw{ 0 };
static std::atomic<int>          g_mouseDbgTargetX10{ 0 };
static std::atomic<int>          g_mouseDbgTargetY10{ 0 };
static std::atomic<int>          g_mouseDbgFollowerX10{ 0 };
static std::atomic<int>          g_mouseDbgFollowerY10{ 0 };
static std::atomic<int>          g_mouseDbgOutX1000{ 0 };
static std::atomic<int>          g_mouseDbgOutY1000{ 0 };
static std::atomic<int>          g_mouseDbgRadius1000{ 1000 };
static std::atomic<bool>         g_wootingReady{ false };
static std::atomic<bool>         g_wootingSdkFaulted{ false };
static std::atomic<uint32_t>     g_wootingOptionalFaultCount{ 0 };
// The SDK may be reached from both the realtime and UI threads.
// Keep the entire public SDK boundary single-threaded; several third-party
// plugins are not safe when queried concurrently.
static SRWLOCK                   g_wootingApiLock = SRWLOCK_INIT;
static std::atomic<uint64_t>     g_wootingReadAnalogCalls{ 0 };
static std::atomic<uint64_t>     g_wootingReadFullCalls{ 0 };
static std::atomic<uint64_t>     g_wootingOtherApiCalls{ 0 };
static std::atomic<ULONGLONG>    g_lastWootingApiStatsLogMs{ 0 };
// Realtime-thread-only snapshot cache. It prevents throttled full-buffer mode
// from falling back to tens of thousands of per-key SDK calls between polls.
static std::array<float, 256>    g_madlionsSnapshotRaw{};
static std::bitset<256>          g_madlionsSnapshotPresent{};
static bool                      g_madlionsSnapshotValid = false;
static constexpr bool            kLogPhysicalKeyTransitions = false;

static int WootingSdk_SehFilterCritical(const wchar_t* apiName, DWORD exceptionCode)
{
    g_wootingSdkFaulted.store(true, std::memory_order_release);
    g_wootingReady.store(false, std::memory_order_release);
    g_knownDeviceCount.store(0, std::memory_order_relaxed);
    g_lastInitIssues.fetch_or(BackendInitIssue_Unknown, std::memory_order_relaxed);
    DebugLog_Write(L"[backend.sdk] SEH fault api=%s code=0x%08lX; disabling Wooting path",
        apiName ? apiName : L"(null)",
        exceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

static int WootingSdk_SehFilterOptional(const wchar_t* apiName, DWORD exceptionCode)
{
    uint32_t n = g_wootingOptionalFaultCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n <= 5 || (n % 500) == 0)
    {
        DebugLog_Write(L"[backend.sdk] SEH fault api=%s code=0x%08lX; optional call skipped (count=%u)",
            apiName ? apiName : L"(null)",
            exceptionCode,
            (unsigned)n);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static int WootingSafe_Initialise()
{
    if (g_wootingSdkFaulted.load(std::memory_order_acquire))
        return (int)WootingAnalogResult_Failure;

    g_wootingOtherApiCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    int result = (int)WootingAnalogResult_Failure;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_Initialise();
        }
        __except (WootingSdk_SehFilterCritical(L"wooting_analog_initialise", GetExceptionCode()))
        {
            result = (int)WootingAnalogResult_Failure;
        }
    }
    ReleaseSRWLockExclusive(&g_wootingApiLock);
    return result;
}

static bool WootingSafe_IsInitialised()
{
    if (g_wootingSdkFaulted.load(std::memory_order_acquire))
        return false;

    g_wootingOtherApiCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    bool result = false;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_IsInitialised();
        }
        __except (WootingSdk_SehFilterCritical(L"wooting_analog_is_initialised", GetExceptionCode()))
        {
            result = false;
        }
    }
    ReleaseSRWLockExclusive(&g_wootingApiLock);
    return result;
}

static WootingAnalogResult WootingSafe_Uninitialise()
{
    if (g_wootingSdkFaulted.load(std::memory_order_acquire))
        return WootingAnalogResult_Failure;

    g_wootingOtherApiCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    WootingAnalogResult result = WootingAnalogResult_Failure;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_Uninitialise();
        }
        __except (WootingSdk_SehFilterCritical(L"wooting_analog_uninitialise", GetExceptionCode()))
        {
            result = WootingAnalogResult_Failure;
        }
    }
    ReleaseSRWLockExclusive(&g_wootingApiLock);
    return result;
}

static WootingAnalogResult WootingSafe_SetKeycodeMode(WootingAnalog_KeycodeType mode)
{
    if (g_wootingSdkFaulted.load(std::memory_order_acquire))
        return WootingAnalogResult_UnInitialized;

    g_wootingOtherApiCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    WootingAnalogResult result = WootingAnalogResult_UnInitialized;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_SetKeycodeMode(mode);
        }
        __except (WootingSdk_SehFilterCritical(L"wooting_analog_set_keycode_mode", GetExceptionCode()))
        {
            result = WootingAnalogResult_Failure;
        }
    }
    ReleaseSRWLockExclusive(&g_wootingApiLock);
    return result;
}

static int WootingSafe_GetConnectedDevicesInfo(WootingAnalog_DeviceInfo_FFI** buffer, unsigned int len)
{
    if (g_wootingSdkFaulted.load(std::memory_order_acquire))
        return (int)WootingAnalogResult_UnInitialized;

    g_wootingOtherApiCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    int result = (int)WootingAnalogResult_UnInitialized;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_GetConnectedDevicesInfo(buffer, len);
        }
        __except (WootingSdk_SehFilterOptional(L"wooting_analog_get_connected_devices_info", GetExceptionCode()))
        {
            result = (int)WootingAnalogResult_Failure;
        }
    }
    ReleaseSRWLockExclusive(&g_wootingApiLock);
    return result;
}

static float WootingSafe_ReadAnalog(unsigned short code)
{
    if (g_wootingSdkFaulted.load(std::memory_order_acquire))
        return (float)WootingAnalogResult_UnInitialized;

    g_wootingReadAnalogCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    float result = (float)WootingAnalogResult_UnInitialized;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_ReadAnalog(code);
        }
        __except (WootingSdk_SehFilterCritical(L"wooting_analog_read_analog", GetExceptionCode()))
        {
            result = (float)WootingAnalogResult_Failure;
        }
    }
    ReleaseSRWLockExclusive(&g_wootingApiLock);
    return result;
}

static float WootingSafe_ReadAnalogDevice(unsigned short code, WootingAnalog_DeviceID deviceId)
{
    if (g_wootingSdkFaulted.load(std::memory_order_acquire))
        return (float)WootingAnalogResult_UnInitialized;

    g_wootingReadAnalogCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    float result = (float)WootingAnalogResult_UnInitialized;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_ReadAnalogDevice(code, deviceId);
        }
        __except (WootingSdk_SehFilterOptional(L"wooting_analog_read_analog_device", GetExceptionCode()))
        {
            result = (float)WootingAnalogResult_Failure;
        }
    }
    ReleaseSRWLockExclusive(&g_wootingApiLock);
    return result;
}

static int WootingSafe_ReadFullBuffer(unsigned short* codeBuffer, float* analogBuffer, unsigned int len)
{
    if (g_wootingSdkFaulted.load(std::memory_order_acquire))
        return (int)WootingAnalogResult_UnInitialized;

    g_wootingReadFullCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    int result = (int)WootingAnalogResult_UnInitialized;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_ReadFullBuffer(codeBuffer, analogBuffer, len);
        }
        __except (WootingSdk_SehFilterOptional(L"wooting_analog_read_full_buffer", GetExceptionCode()))
        {
            result = (int)WootingAnalogResult_Failure;
        }
    }
    ReleaseSRWLockExclusive(&g_wootingApiLock);
    return result;
}

static int WootingSafe_ReadFullBufferDevice(unsigned short* codeBuffer, float* analogBuffer, unsigned int len, WootingAnalog_DeviceID deviceId)
{
    if (g_wootingSdkFaulted.load(std::memory_order_acquire))
        return (int)WootingAnalogResult_UnInitialized;

    g_wootingReadFullCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    int result = (int)WootingAnalogResult_UnInitialized;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_ReadFullBufferDevice(codeBuffer, analogBuffer, len, deviceId);
        }
        __except (WootingSdk_SehFilterOptional(L"wooting_analog_read_full_buffer_device", GetExceptionCode()))
        {
            result = (int)WootingAnalogResult_Failure;
        }
    }
    ReleaseSRWLockExclusive(&g_wootingApiLock);
    return result;
}

#define wooting_analog_initialise WootingSafe_Initialise
#define wooting_analog_is_initialised WootingSafe_IsInitialised
#define wooting_analog_uninitialise WootingSafe_Uninitialise
#define wooting_analog_set_keycode_mode WootingSafe_SetKeycodeMode
#define wooting_analog_get_connected_devices_info WootingSafe_GetConnectedDevicesInfo
#define wooting_analog_read_analog WootingSafe_ReadAnalog
#define wooting_analog_read_analog_device WootingSafe_ReadAnalogDevice
#define wooting_analog_read_full_buffer WootingSafe_ReadFullBuffer
#define wooting_analog_read_full_buffer_device WootingSafe_ReadFullBufferDevice

static float Clamp01(float v);

// ---- native HID paths ----
#include "backend_sparklink.inc"
#include "backend_sayo.inc"

static constexpr uint16_t kSparkMissedHidDepthThresholdMilli = 500;
static constexpr uint16_t kSparkMissedHidReleaseThresholdMilli = 250;
static constexpr ULONGLONG kSparkMissedHidDeadlineMs = 45;

struct SparkMissedHidWatch
{
    bool overThreshold = false;
    bool waiting = false;
    bool missedLogged = false;
    ULONGLONG startMs = 0;
    uint32_t startKeySeq = 0;
    uint16_t peakMilli = 0;
};

static std::array<SparkMissedHidWatch, 256> g_sparkMissedHidWatch{};
static bool g_sparkMissedHidWasEnabled = false;

static void SparkMissedHid_ResetWatch()
{
    for (auto& w : g_sparkMissedHidWatch)
        w = SparkMissedHidWatch{};
}

static void SparkMissedHid_Log(uint16_t hid, const SparkMissedHidWatch& w, uint16_t curMilli, ULONGLONG nowMs, bool late)
{
    const uint32_t keySeqNow = g_keyboardEventSeq.load(std::memory_order_acquire);
    const uint32_t routeSeq = g_sparkRouteQuerySeq.load(std::memory_order_relaxed);
    const uint8_t routeRow = g_sparkLastRouteRow.load(std::memory_order_relaxed);
    const uint8_t routeOk = g_sparkLastRouteOk.load(std::memory_order_relaxed);
    const ULONGLONG routeMs = g_sparkLastRouteMs.load(std::memory_order_relaxed);
    const ULONGLONG routeAge = (routeMs != 0 && nowMs >= routeMs) ? (nowMs - routeMs) : 0;
    const uint16_t scan = g_hidToScan[hid].load(std::memory_order_relaxed);
    const uint16_t vk = g_hidToVk[hid].load(std::memory_order_relaxed);
    const uint8_t phys = g_physicalDown[hid].load(std::memory_order_relaxed);
    const int rows = std::clamp(g_sparkRowCount.load(std::memory_order_relaxed), 0, kSparkMaxRows);

    DebugLog_Write(
        L"[backend.spark.%s] hid=%u scan=%u vk=%u peak=%u current=%u elapsed_ms=%llu phys=%u key_seq_start=%u key_seq_now=%u spark_route_seq=%u last_row=%u last_ok=%u last_route_age_ms=%llu rows=%d poll_ms=%u reason=%s",
        late ? L"late_hid" : L"missed_hid",
        (unsigned)hid,
        (unsigned)scan,
        (unsigned)vk,
        (unsigned)w.peakMilli,
        (unsigned)curMilli,
        (unsigned long long)(nowMs - w.startMs),
        (unsigned)phys,
        (unsigned)w.startKeySeq,
        (unsigned)keySeqNow,
        (unsigned)routeSeq,
        (unsigned)routeRow,
        (unsigned)routeOk,
        (unsigned long long)routeAge,
        rows,
        (unsigned)Settings_GetPollingMs(),
        late ? L"hid_down_arrived_after_missed_window" : L"analog_depth_over_50_without_hid_down");
}

static void SparkMissedHid_Tick(ULONGLONG nowMs)
{
    const bool enabled = false;
    if (!enabled || !g_sparkConnected.load(std::memory_order_acquire))
    {
        if (g_sparkMissedHidWasEnabled || enabled)
            SparkMissedHid_ResetWatch();
        g_sparkMissedHidWasEnabled = enabled;
        return;
    }
    if (!g_sparkMissedHidWasEnabled)
    {
        SparkMissedHid_ResetWatch();
        DebugLog_Write(L"[backend.spark.missed_hid] diagnostic enabled threshold=%u deadline_ms=%llu",
            (unsigned)kSparkMissedHidDepthThresholdMilli,
            (unsigned long long)kSparkMissedHidDeadlineMs);
    }
    g_sparkMissedHidWasEnabled = true;

    const uint32_t keySeqNow = g_keyboardEventSeq.load(std::memory_order_acquire);
    for (uint16_t hid = 1; hid < 256; ++hid)
    {
        SparkMissedHidWatch& w = g_sparkMissedHidWatch[hid];
        const uint16_t cur = g_sparkAnalogMilli[hid].load(std::memory_order_relaxed);
        const bool physDown = g_physicalDown[hid].load(std::memory_order_relaxed) != 0;

        if (cur < kSparkMissedHidReleaseThresholdMilli)
        {
            if (w.overThreshold || w.waiting || w.missedLogged)
                w = SparkMissedHidWatch{};
            continue;
        }

        if (cur >= kSparkMissedHidDepthThresholdMilli && !w.overThreshold)
        {
            w.overThreshold = true;
            w.waiting = !physDown;
            w.missedLogged = false;
            w.startMs = nowMs;
            w.startKeySeq = keySeqNow;
            w.peakMilli = cur;
        }
        else if (w.overThreshold && cur > w.peakMilli)
        {
            w.peakMilli = cur;
        }

        if (!w.waiting)
            continue;

        if (physDown)
        {
            if (w.missedLogged)
                SparkMissedHid_Log(hid, w, cur, nowMs, true);
            w.waiting = false;
            continue;
        }

        if (!w.missedLogged && nowMs - w.startMs >= kSparkMissedHidDeadlineMs)
        {
            SparkMissedHid_Log(hid, w, cur, nowMs, false);
            w.missedLogged = true;
        }
    }
}

static const wchar_t* KeycodeModeName(int mode)
{
    switch ((WootingAnalog_KeycodeType)mode)
    {
    case WootingAnalog_KeycodeType_HID: return L"HID";
    case WootingAnalog_KeycodeType_ScanCode1: return L"ScanCode1";
    case WootingAnalog_KeycodeType_VirtualKey: return L"VirtualKey";
    case WootingAnalog_KeycodeType_VirtualKeyTranslate: return L"VirtualKeyTranslate";
    default: return L"Unknown";
    }
}

static WootingAnalog_KeycodeType NextKeycodeMode(WootingAnalog_KeycodeType mode)
{
    switch (mode)
    {
    case WootingAnalog_KeycodeType_HID: return WootingAnalog_KeycodeType_ScanCode1;
    case WootingAnalog_KeycodeType_ScanCode1: return WootingAnalog_KeycodeType_VirtualKey;
    case WootingAnalog_KeycodeType_VirtualKey: return WootingAnalog_KeycodeType_VirtualKeyTranslate;
    case WootingAnalog_KeycodeType_VirtualKeyTranslate: return WootingAnalog_KeycodeType_HID;
    default: return WootingAnalog_KeycodeType_HID;
    }
}

static bool SetKeycodeModeWithLog(WootingAnalog_KeycodeType mode, const wchar_t* reason, uint16_t hidHint)
{
    WootingAnalogResult r = wooting_analog_set_keycode_mode(mode);
    DebugLog_Write(
        L"[backend.mode] set mode=%s(%d) reason=%s hid_hint=%u ret=%d",
        KeycodeModeName((int)mode), (int)mode,
        reason ? reason : L"-",
        (unsigned)hidHint,
        (int)r);
    if (r >= 0)
    {
        g_keycodeMode.store((int)mode, std::memory_order_relaxed);
        g_lastKeycodeSwitchMs.store(GetTickCount64(), std::memory_order_relaxed);
        return true;
    }
    return false;
}

static void LogConnectedDevicesDetailed(const wchar_t* stage)
{
    if (!kEnableDeviceInfoQuery)
    {
        g_knownDeviceCount.store(0, std::memory_order_relaxed);
        DebugLog_Write(L"[backend.devices] %s skipped (device query disabled for stability)",
            stage ? stage : L"-");
        return;
    }

    WootingAnalog_DeviceInfo_FFI* devs[16]{};
    int n = wooting_analog_get_connected_devices_info(devs, (unsigned)_countof(devs));
    if (n < 0)
    {
        DebugLog_Write(L"[backend.devices] %s get_devices_ret=%d", stage ? stage : L"-", n);
        return;
    }

    std::array<WootingAnalog_DeviceID, 16> newIds{};
    int uniqueCount = 0;
    for (int i = 0; i < n && i < (int)_countof(devs); ++i)
    {
        const WootingAnalog_DeviceInfo_FFI* d = devs[i];
        if (!d) continue;
        WootingAnalog_DeviceID id = 0;
        __try
        {
            id = d->device_id;
        }
        __except (WootingSdk_SehFilterOptional(L"wooting_device_info.device_id", GetExceptionCode()))
        {
            continue;
        }

        bool dup = false;
        for (int k = 0; k < uniqueCount; ++k)
        {
            if (newIds[(size_t)k] == id)
            {
                dup = true;
                break;
            }
        }
        if (!dup && uniqueCount < (int)newIds.size())
            newIds[(size_t)uniqueCount++] = id;
    }

    for (int i = 0; i < uniqueCount; ++i)
        g_knownDeviceIds[(size_t)i] = newIds[(size_t)i];
    g_knownDeviceCount.store(uniqueCount, std::memory_order_relaxed);
    DebugLog_Write(L"[backend.devices] %s count=%d unique_ids=%d", stage ? stage : L"-", n, uniqueCount);
}

static uint16_t HidFallbackToVk(uint16_t hid)
{
    if (hid >= 4 && hid <= 29)  return (uint16_t)('A' + (hid - 4)); // A..Z
    if (hid >= 30 && hid <= 38) return (uint16_t)('1' + (hid - 30)); // 1..9
    if (hid == 39) return (uint16_t)'0';
    switch (hid)
    {
    case 40: return VK_RETURN;
    case 41: return VK_ESCAPE;
    case 42: return VK_BACK;
    case 43: return VK_TAB;
    case 44: return VK_SPACE;
    case 45: return VK_OEM_MINUS;
    case 46: return VK_OEM_PLUS;
    case 47: return VK_OEM_4;
    case 48: return VK_OEM_6;
    case 49: return VK_OEM_5;
    case 51: return VK_OEM_1;
    case 52: return VK_OEM_7;
    case 54: return VK_OEM_COMMA;
    case 55: return VK_OEM_PERIOD;
    case 56: return VK_OEM_2;
    case 57: return VK_CAPITAL;
    case 58: return VK_F1;
    case 59: return VK_F2;
    case 60: return VK_F3;
    case 61: return VK_F4;
    case 62: return VK_F5;
    case 63: return VK_F6;
    case 64: return VK_F7;
    case 65: return VK_F8;
    case 66: return VK_F9;
    case 67: return VK_F10;
    case 68: return VK_F11;
    case 69: return VK_F12;
    case 73: return VK_INSERT;
    case 74: return VK_HOME;
    case 75: return VK_PRIOR;
    case 76: return VK_DELETE;
    case 77: return VK_END;
    case 78: return VK_NEXT;
    case 79: return VK_RIGHT;
    case 80: return VK_LEFT;
    case 81: return VK_DOWN;
    case 82: return VK_UP;
    case 83: return VK_NUMLOCK;
    case 84: return VK_DIVIDE;
    case 85: return VK_MULTIPLY;
    case 86: return VK_SUBTRACT;
    case 87: return VK_ADD;
    case 89: return VK_NUMPAD1;
    case 90: return VK_NUMPAD2;
    case 91: return VK_NUMPAD3;
    case 92: return VK_NUMPAD4;
    case 93: return VK_NUMPAD5;
    case 94: return VK_NUMPAD6;
    case 95: return VK_NUMPAD7;
    case 96: return VK_NUMPAD8;
    case 97: return VK_NUMPAD9;
    case 98: return VK_NUMPAD0;
    case 99: return VK_DECIMAL;
    case 224: return VK_LCONTROL;
    case 225: return VK_LSHIFT;
    case 226: return VK_LMENU;
    case 227: return VK_LWIN;
    case 228: return VK_RCONTROL;
    case 229: return VK_RSHIFT;
    case 230: return VK_RMENU;
    case 231: return VK_RWIN;
    default: return 0;
    }
}

static uint16_t HidToModeCode(uint16_t hid, WootingAnalog_KeycodeType mode)
{
    if (hid == 0) return 0;
    if (mode == WootingAnalog_KeycodeType_HID)
        return hid;

    if (hid < 256)
    {
        if (mode == WootingAnalog_KeycodeType_ScanCode1)
        {
            uint16_t sc = g_hidToScan[hid].load(std::memory_order_relaxed);
            return sc;
        }
        if (mode == WootingAnalog_KeycodeType_VirtualKey || mode == WootingAnalog_KeycodeType_VirtualKeyTranslate)
        {
            uint16_t vk = g_hidToVk[hid].load(std::memory_order_relaxed);
            if (vk != 0) return vk;
            return HidFallbackToVk(hid);
        }
    }
    return 0;
}

static float SafeReadAnalogByCode(uint16_t code)
{
    if (code == 0) return 0.0f;
    float v = wooting_analog_read_analog(code);
    if (!std::isfinite(v) || v < 0.0f) return 0.0f;
    return std::clamp(v, 0.0f, 1.0f);
}

static float ReadAnalogByCodeWithDeviceFallback(uint16_t code, uint16_t hidForLog)
{
    if (code == 0) return 0.0f;
    float base = wooting_analog_read_analog(code);
    float best = (std::isfinite(base) ? base : 0.0f);

    int n = std::clamp(g_knownDeviceCount.load(std::memory_order_relaxed), 0, (int)g_knownDeviceIds.size());
    for (int i = 0; i < n; ++i)
    {
        WootingAnalog_DeviceID id = g_knownDeviceIds[i];
        float dv = wooting_analog_read_analog_device(code, id);
        if (!std::isfinite(dv)) continue;
        if (dv > best)
            best = dv;
    }

    if (best > base + 0.0005f)
    {
        DebugLog_Write(
            L"[backend.analog] device_fallback improved hid=%u code=%u base=%.3f best=%.3f",
            (unsigned)hidForLog,
            (unsigned)code,
            base,
            best);
    }

    return best;
}

static bool ProbeKeycodeModeByFullBufferActivity(uint16_t hidHint)
{
    struct ModeProbe
    {
        WootingAnalog_KeycodeType mode;
        int setRet = -9999;
        int readRet = -9999;
        float maxV = 0.0f;
    };

    ModeProbe probes[] = {
        { WootingAnalog_KeycodeType_HID },
        { WootingAnalog_KeycodeType_ScanCode1 },
        { WootingAnalog_KeycodeType_VirtualKey },
        { WootingAnalog_KeycodeType_VirtualKeyTranslate },
    };

    int currentMode = g_keycodeMode.load(std::memory_order_relaxed);
    int bestIdx = -1;
    int bestReadRet = -1;
    float bestMax = 0.0f;

    for (int i = 0; i < (int)_countof(probes); ++i)
    {
        WootingAnalogResult sr = wooting_analog_set_keycode_mode(probes[i].mode);
        probes[i].setRet = (int)sr;
        if (sr < 0)
            continue;

        unsigned short codes[64]{};
        float vals[64]{};
        int rr = wooting_analog_read_full_buffer(codes, vals, (unsigned)_countof(codes));
        probes[i].readRet = rr;
        if (rr < 0)
            continue;

        int n = std::min(rr, (int)_countof(vals));
        float maxV = 0.0f;
        for (int j = 0; j < n; ++j)
        {
            float v = vals[j];
            if (std::isfinite(v) && v > maxV)
                maxV = v;
        }
        probes[i].maxV = maxV;

        // Prefer mode with highest max analog first, then by more reported keys.
        if (maxV > bestMax + 0.005f || (std::abs(maxV - bestMax) <= 0.005f && rr > bestReadRet))
        {
            bestMax = maxV;
            bestReadRet = rr;
            bestIdx = i;
        }
    }

    DebugLog_Write(
        L"[backend.mode] full_probe hid=%u HID(ret=%d,max=%.3f) SC(ret=%d,max=%.3f) VK(ret=%d,max=%.3f) VKT(ret=%d,max=%.3f)",
        (unsigned)hidHint,
        probes[0].readRet, probes[0].maxV,
        probes[1].readRet, probes[1].maxV,
        probes[2].readRet, probes[2].maxV,
        probes[3].readRet, probes[3].maxV);

    if (bestIdx >= 0 && (bestReadRet > 0 || bestMax >= 0.02f))
    {
        WootingAnalog_KeycodeType target = probes[bestIdx].mode;
        if ((int)target != currentMode)
            SetKeycodeModeWithLog(target, L"full_probe", hidHint);
        else
            wooting_analog_set_keycode_mode((WootingAnalog_KeycodeType)currentMode);
        return true;
    }

    // Restore current mode after temporary probing.
    wooting_analog_set_keycode_mode((WootingAnalog_KeycodeType)currentMode);
    return false;
}

static bool AutoProbeKeycodeModeFromEvent(uint16_t hidHint, uint16_t scanCode, uint16_t vkCode)
{
    if (hidHint == 0) return false;

    struct ProbeItem { WootingAnalog_KeycodeType mode; uint16_t code; float value; int setRet; };
    ProbeItem items[] = {
        { WootingAnalog_KeycodeType_HID, hidHint, 0.0f, 0 },
        { WootingAnalog_KeycodeType_ScanCode1, scanCode, 0.0f, 0 },
        { WootingAnalog_KeycodeType_VirtualKey, vkCode, 0.0f, 0 },
        { WootingAnalog_KeycodeType_VirtualKeyTranslate, vkCode, 0.0f, 0 },
    };

    int currentMode = g_keycodeMode.load(std::memory_order_relaxed);
    int bestIdx = -1;
    float bestVal = 0.0f;
    float currentVal = 0.0f;

    for (int i = 0; i < (int)_countof(items); ++i)
    {
        if (items[i].code == 0)
            continue;
        WootingAnalogResult sr = wooting_analog_set_keycode_mode(items[i].mode);
        items[i].setRet = (int)sr;
        if (sr < 0)
            continue;
        items[i].value = SafeReadAnalogByCode(items[i].code);
        if ((int)items[i].mode == currentMode)
            currentVal = items[i].value;
        if (items[i].value > bestVal)
        {
            bestVal = items[i].value;
            bestIdx = i;
        }
    }

    DebugLog_Write(
        L"[backend.mode] probe hid=%u scan=%u vk=%u values: HID=%.3f SC=%.3f VK=%.3f VKT=%.3f",
        (unsigned)hidHint, (unsigned)scanCode, (unsigned)vkCode,
        items[0].value, items[1].value, items[2].value, items[3].value);

    bool foundWorkingMode = (bestIdx >= 0 && bestVal >= 0.015f);
    if (foundWorkingMode)
    {
        WootingAnalog_KeycodeType targetMode = items[bestIdx].mode;
        // Switch only when the alternative mode is meaningfully better.
        if ((int)targetMode != currentMode && bestVal > currentVal + 0.01f)
            SetKeycodeModeWithLog(targetMode, L"auto_probe", hidHint);
        else
            wooting_analog_set_keycode_mode((WootingAnalog_KeycodeType)currentMode);
        return true;
    }

    // Restore current mode after temporary probe switching.
    wooting_analog_set_keycode_mode((WootingAnalog_KeycodeType)currentMode);
    return false;
}

static void LogFullBufferSnapshot(const wchar_t* stage)
{
    unsigned short codes[64]{};
    float vals[64]{};
    int ret = wooting_analog_read_full_buffer(codes, vals, (unsigned)_countof(codes));
    if (ret < 0)
    {
        g_tmFullBufferRet.store(ret, std::memory_order_relaxed);
        g_tmFullBufferMaxMilli.store(0, std::memory_order_relaxed);
        g_tmFullBufferDeviceBestRet.store(ret, std::memory_order_relaxed);
        g_tmFullBufferDeviceBestMaxMilli.store(0, std::memory_order_relaxed);
        DebugLog_Write(
            L"[backend.full] %s ret=%d mode=%s",
            stage ? stage : L"-",
            ret,
            KeycodeModeName(g_keycodeMode.load(std::memory_order_relaxed)));
        return;
    }

    int n = std::min(ret, (int)_countof(codes));
    float maxV = 0.0f;
    unsigned short maxCode = 0;
    for (int i = 0; i < n; ++i)
    {
        float v = vals[i];
        if (std::isfinite(v) && v > maxV)
        {
            maxV = v;
            maxCode = codes[i];
        }
    }

    DebugLog_Write(
        L"[backend.full] %s ret=%d max=%.3f code=%u mode=%s",
        stage ? stage : L"-",
        ret,
        maxV,
        (unsigned)maxCode,
        KeycodeModeName(g_keycodeMode.load(std::memory_order_relaxed)));
    g_tmFullBufferRet.store(ret, std::memory_order_relaxed);
    g_tmFullBufferMaxMilli.store((uint16_t)std::clamp((int)std::lround(maxV * 1000.0f), 0, 1000), std::memory_order_relaxed);

    int ndev = std::clamp(g_knownDeviceCount.load(std::memory_order_relaxed), 0, (int)g_knownDeviceIds.size());
    int bestDevRet = ret;
    uint16_t bestDevMilli = (uint16_t)std::clamp((int)std::lround(maxV * 1000.0f), 0, 1000);
    for (int di = 0; di < ndev; ++di)
    {
        WootingAnalog_DeviceID id = g_knownDeviceIds[di];
        unsigned short dcodes[64]{};
        float dvals[64]{};
        int dret = wooting_analog_read_full_buffer_device(dcodes, dvals, (unsigned)_countof(dcodes), id);
        if (dret < 0)
        {
            DebugLog_Write(
                L"[backend.full.dev] %s dev#%d id=%llu ret=%d",
                stage ? stage : L"-",
                di,
                (unsigned long long)id,
                dret);
            continue;
        }
        int dn = std::min(dret, (int)_countof(dcodes));
        float dmax = 0.0f;
        unsigned short dcode = 0;
        for (int i = 0; i < dn; ++i)
        {
            float v = dvals[i];
            if (std::isfinite(v) && v > dmax)
            {
                dmax = v;
                dcode = dcodes[i];
            }
        }
        DebugLog_Write(
            L"[backend.full.dev] %s dev#%d id=%llu ret=%d max=%.3f code=%u",
            stage ? stage : L"-",
            di,
            (unsigned long long)id,
            dret,
            dmax,
            (unsigned)dcode);

        uint16_t dm = (uint16_t)std::clamp((int)std::lround(dmax * 1000.0f), 0, 1000);
        if (dm > bestDevMilli)
        {
            bestDevMilli = dm;
            bestDevRet = dret;
        }
    }
    g_tmFullBufferDeviceBestRet.store(bestDevRet, std::memory_order_relaxed);
    g_tmFullBufferDeviceBestMaxMilli.store(bestDevMilli, std::memory_order_relaxed);
}

static void LogWootingStateSnapshot(const wchar_t* stage)
{
    bool inited = wooting_analog_is_initialised();
    int devRet = (int)WootingAnalogResult_NotAvailable;
    if (kEnableDeviceInfoQuery)
    {
        WootingAnalog_DeviceInfo_FFI* devs[16]{};
        devRet = wooting_analog_get_connected_devices_info(devs, (unsigned)_countof(devs));
    }
    DebugLog_Write(
        L"[backend.wooting] %s init=%d get_devices_ret=%d keycode_mode=%d",
        stage ? stage : L"(null)",
        inited ? 1 : 0,
        devRet,
        g_keycodeMode.load(std::memory_order_relaxed));
}

static float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

static void Vigem_Destroy() noexcept
{
    if (g_client)
    {
        for (int i = 0; i < g_connectedPadCount; ++i)
        {
            if (g_pads[(size_t)i])
            {
                DebugLog_SetCheckpoint(L"vigem-owner: target remove pad=%d", i);
                vigem_target_remove(g_client, g_pads[(size_t)i]);
                DebugLog_SetCheckpoint(L"vigem-owner: target free pad=%d", i);
                vigem_target_free(g_pads[(size_t)i]);
                g_pads[(size_t)i] = nullptr;
            }
        }
    }

    g_connectedPadCount = 0;
    if (g_client)
    {
        DebugLog_SetCheckpoint(L"vigem-owner: disconnect");
        vigem_disconnect(g_client);
        DebugLog_SetCheckpoint(L"vigem-owner: client free");
        vigem_free(g_client);
        g_client = nullptr;
    }
}

static bool Vigem_Create(int padCount, VIGEM_ERROR* outErr)
{
    padCount = std::clamp(padCount, 1, kMaxVirtualPads);
    if (outErr) *outErr = VIGEM_ERROR_NONE;
    DebugLog_SetCheckpoint(L"vigem-owner: alloc");
    g_client = vigem_alloc();
    if (!g_client) { if (outErr) *outErr = VIGEM_ERROR_BUS_NOT_FOUND; return false; }
    DebugLog_SetCheckpoint(L"vigem-owner: connect");
    VIGEM_ERROR err = vigem_connect(g_client);
    if (!VIGEM_SUCCESS(err)) { if (outErr) *outErr = err; vigem_free(g_client); g_client = nullptr; return false; }

    g_connectedPadCount = 0;
    for (int i = 0; i < padCount; ++i)
    {
        DebugLog_SetCheckpoint(L"vigem-owner: target alloc pad=%d", i);
        PVIGEM_TARGET pad = vigem_target_x360_alloc();
        if (!pad)
        {
            if (outErr) *outErr = VIGEM_ERROR_INVALID_TARGET;
            Vigem_Destroy();
            return false;
        }

        DebugLog_SetCheckpoint(L"vigem-owner: target add pad=%d", i);
        err = vigem_target_add(g_client, pad);
        if (!VIGEM_SUCCESS(err))
        {
            if (outErr) *outErr = err;
            vigem_target_free(pad);
            Vigem_Destroy();
            return false;
        }

        g_pads[(size_t)i] = pad;
        g_connectedPadCount = i + 1;
    }

    if (outErr) *outErr = VIGEM_ERROR_NONE;
    return true;
}

static bool Vigem_ReconnectThrottled(bool force = false)
{
    ULONGLONG now = GetTickCount64();
    if (!force && now - g_lastReconnectAttemptMs < 1000) return false;
    g_lastReconnectAttemptMs = now;
    g_vigemUpdateFailStreak = 0;

    // Reconnect itself emits device-change broadcasts. Suppress them briefly so
    // WM_DEVICECHANGE does not trigger reconnect loops.
    g_ignoreDeviceChangeUntilMs.store(now + 1500, std::memory_order_release);
    Vigem_Destroy();

    if (!g_virtualPadsEnabled.load(std::memory_order_acquire))
    {
        g_vigemOk.store(true, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
        return true;
    }

    VIGEM_ERROR err = VIGEM_ERROR_NONE;
    int wantedPads = std::clamp(g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads);
    bool ok = Vigem_Create(wantedPads, &err);
    g_vigemOk.store(ok, std::memory_order_release);
    g_vigemLastErr.store(ok ? VIGEM_ERROR_NONE : err, std::memory_order_release);
    StabilityTrace_Write(ok ? L"INFO" : L"ERROR", L"vigem", L"reconnect",
        L"ok=%d pads=%d error=%d forced=%d", ok ? 1 : 0, wantedPads, (int)err, force ? 1 : 0);
    if (ok)
    {
        g_vigemResubmitRequested.store(true, std::memory_order_release);
        RealtimeLoop_NotifyInputChanged();
    }
    return ok;
}

static void VigemOutput_Wake() noexcept
{
    HANDLE wakeEvent = g_vigemOutputWakeEvent;
    if (wakeEvent)
        SetEvent(wakeEvent);
}

static bool VigemOutput_SendBatch(const VigemOutputBatch& batch)
{
    if (!g_client || g_connectedPadCount <= 0)
    {
        g_vigemOk.store(false, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_BUS_NOT_FOUND, std::memory_order_release);
        g_vigemResubmitRequested.store(true, std::memory_order_release);
        RealtimeLoop_NotifyInputChanged();
        return false;
    }

    VIGEM_ERROR error = VIGEM_ERROR_NONE;
    bool allOk = true;
    const int count = std::min<int>(batch.count, g_connectedPadCount);
    for (int i = 0; i < count; ++i)
    {
        if ((batch.validMask & (1u << i)) == 0)
            continue;
        PVIGEM_TARGET pad = g_pads[static_cast<size_t>(i)];
        if (!pad)
            continue;

#if defined(HALLJOY_ANALOG_SIMULATOR)
        const wchar_t* commandLine = GetCommandLineW();
        if (commandLine && wcsstr(commandLine, L"--halljoy-test-vigem-update-stall") &&
            !g_vigemTestStallInjected.exchange(true, std::memory_order_acq_rel))
        {
            StabilityTrace_Write(L"WARN", L"vigem-output", L"test.update_stall.injected",
                L"simulator_only=1 pad=%d sleep_ms=60000", i);
            Sleep(60000);
        }
#endif

        DebugLog_SetCheckpoint(L"vigem-output: update pad=%d", i);
        error = vigem_target_x360_update(g_client, pad, batch.reports[static_cast<size_t>(i)]);
        DebugLog_SetCheckpoint(L"vigem-output: update returned pad=%d", i);
        if (!VIGEM_SUCCESS(error))
        {
            allOk = false;
            break;
        }
#if defined(HALLJOY_ANALOG_SIMULATOR)
        if (i == 0)
            TraceAcceptedSimulatorVigemUpdate(batch.reports[0]);
#endif
    }

    if (!allOk)
    {
        ++g_vigemUpdateFailStreak;
        if (g_vigemUpdateFailStreak == 1)
        {
            StabilityTrace_Write(L"WARN", L"vigem", L"update.failed",
                L"error=%d streak=1 owner=output-worker", static_cast<int>(error));
        }
        g_vigemOk.store(false, std::memory_order_release);
        g_vigemLastErr.store(error, std::memory_order_release);
        g_vigemResubmitRequested.store(true, std::memory_order_release);
        RealtimeLoop_NotifyInputChanged();
        if (g_vigemUpdateFailStreak >= 3)
        {
            g_vigemUpdateFailStreak = 0;
            (void)Vigem_ReconnectThrottled();
        }
        return false;
    }

    g_vigemUpdateFailStreak = 0;
    g_vigemOk.store(true, std::memory_order_release);
    g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
    return true;
}

static DWORD VigemOutputThreadBody()
{
    uint64_t consumedGeneration = g_vigemOutputMailbox.PublishedGeneration();
    while (g_vigemOutputRun.load(std::memory_order_acquire))
    {
        bool didWork = false;
        const bool forceReconnect = g_reconnectRequested.exchange(false, std::memory_order_acq_rel);
        const bool deviceReconnect = !forceReconnect &&
            g_deviceChangeReconnectRequested.exchange(false, std::memory_order_acq_rel);
        const bool emergencyNeutral =
            g_vigemEmergencyNeutralRequested.exchange(false, std::memory_order_acq_rel);
        const bool enabled = g_virtualPadsEnabled.load(std::memory_order_acquire);
        const int wantedPads = std::clamp(
            g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads);

        if (forceReconnect || deviceReconnect ||
            (enabled && (!g_client || g_connectedPadCount != wantedPads)))
        {
            if (forceReconnect)
                g_deviceChangeReconnectRequested.store(false, std::memory_order_release);
            (void)Vigem_ReconnectThrottled(forceReconnect);
            didWork = true;
        }
        else if (!enabled && (g_client || g_connectedPadCount > 0))
        {
            Vigem_Destroy();
            g_vigemOk.store(true, std::memory_order_release);
            g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
            didWork = true;
        }

        for (;;)
        {
            VigemOutputBatch batch{};
            uint64_t generation = consumedGeneration;
            const auto read = g_vigemOutputMailbox.TryReadAfter(
                consumedGeneration, &batch, &generation);
            if (read == halljoy::output::LatestValueMailbox<VigemOutputBatch>::ReadResult::Busy)
            {
                SwitchToThread();
                continue;
            }
            if (read == halljoy::output::LatestValueMailbox<VigemOutputBatch>::ReadResult::Unchanged)
                break;
            consumedGeneration = generation;
            // A realtime fault invalidates every previously queued report. Drain
            // those generations without submitting them; neutral must be the
            // final driver write, never followed by stale pre-fault input.
            if (enabled && !emergencyNeutral)
                (void)VigemOutput_SendBatch(batch);
            didWork = true;
        }

        if (emergencyNeutral)
        {
            VigemOutputBatch neutral{};
            neutral.count = static_cast<uint8_t>(std::clamp(g_connectedPadCount, 0, kMaxVirtualPads));
            neutral.validMask = neutral.count == 0
                ? 0
                : static_cast<uint8_t>((1u << neutral.count) - 1u);
            (void)VigemOutput_SendBatch(neutral);
            didWork = true;
        }

        if (!didWork)
            WaitForSingleObject(g_vigemOutputWakeEvent, 100);
    }
    return 0;
}

static void VigemOutputThreadOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_vigemOutputFaultRecord = record;
    g_vigemOutputFaultKind.store(record.kind, std::memory_order_release);
    g_vigemOutputRun.store(false, std::memory_order_release);
    g_vigemOk.store(false, std::memory_order_release);
    StabilityTrace_WriteCritical(L"ERROR", L"vigem-output", L"worker.fault",
        L"kind=%u restart_blocked=1", static_cast<unsigned>(record.kind));
}

static void VigemOutputThreadOnCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_vigemOutputThreadAlive.store(false, std::memory_order_release);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"vigem-output", L"worker.exit", L"fault_kind=%u", static_cast<unsigned>(record.kind));
}

static DWORD WINAPI VigemOutputThreadProc(LPVOID) noexcept
{
    g_vigemOutputThreadAlive.store(true, std::memory_order_release);
    StabilityTrace_Write(L"INFO", L"vigem-output", L"worker.start");
    const DWORD result = static_cast<DWORD>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return VigemOutputThreadBody(); },
        VigemOutputThreadOnFault,
        [](const halljoy::worker::WorkerExceptionRecord&) noexcept {},
        0xE0564947u));
    Vigem_Destroy();
    VigemOutputThreadOnCompletion(g_vigemOutputFaultRecord);
    return result;
}

static bool VigemOutput_Start()
{
    std::lock_guard<std::mutex> lock(g_vigemOutputLifecycleMutex);
    if (g_vigemOutputLifecycle.State() == halljoy::lifecycle::WorkerState::Running)
    {
        return g_vigemOutputThreadAlive.load(std::memory_order_acquire) &&
            g_vigemOutputFaultKind.load(std::memory_order_acquire) ==
                halljoy::worker::WorkerExceptionKind::None;
    }

    const auto start = g_vigemOutputLifecycle.BeginStart();
    if (start.status != halljoy::lifecycle::StartStatus::Starting)
        return false;

    g_vigemOutputWakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_vigemOutputWakeEvent)
    {
        (void)g_vigemOutputLifecycle.FailStartBeforeWorker(start.generation, GetLastError());
        return false;
    }

    g_vigemOutputFaultRecord = {};
    g_vigemOutputFaultKind.store(halljoy::worker::WorkerExceptionKind::None, std::memory_order_release);
    g_vigemEmergencyNeutralRequested.store(false, std::memory_order_release);
    g_vigemResubmitRequested.store(false, std::memory_order_release);
    g_vigemOutputMailbox.DiscardPending();
#if defined(HALLJOY_ANALOG_SIMULATOR)
    g_vigemTestStallInjected.store(false, std::memory_order_release);
#endif
    g_vigemOutputRun.store(true, std::memory_order_release);
    g_vigemOutputThread = CreateThread(nullptr, 0, VigemOutputThreadProc, nullptr, 0, nullptr);
    if (!g_vigemOutputThread)
    {
        const DWORD error = GetLastError();
        g_vigemOutputRun.store(false, std::memory_order_release);
        CloseHandle(g_vigemOutputWakeEvent);
        g_vigemOutputWakeEvent = nullptr;
        (void)g_vigemOutputLifecycle.FailStartBeforeWorker(start.generation, error);
        return false;
    }

    const auto running = g_vigemOutputLifecycle.ConfirmRunning(start.generation);
    if (!running.IsRunning())
        return false;
    StabilityTrace_Write(L"INFO", L"vigem-output", L"start.ok",
        L"generation=%llu", static_cast<unsigned long long>(start.generation.Value()));
    return true;
}

static halljoy::lifecycle::StopResult VigemOutput_Stop()
{
    std::lock_guard<std::mutex> lock(g_vigemOutputLifecycleMutex);
    const auto state = g_vigemOutputLifecycle.State();
    if (state == halljoy::lifecycle::WorkerState::Stopped ||
        state == halljoy::lifecycle::WorkerState::Joined)
        return g_vigemOutputLifecycle.RequestStop();
    if (state == halljoy::lifecycle::WorkerState::Poisoned)
        return g_vigemOutputLifecycle.RequestStop(g_vigemOutputLifecycle.Generation());

    const auto requested = g_vigemOutputLifecycle.RequestStop(g_vigemOutputLifecycle.Generation());
    if (requested.status != halljoy::lifecycle::StopStatus::StopRequested)
        return requested;
    if (!g_vigemOutputThread)
    {
        return g_vigemOutputLifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed,
            ERROR_INVALID_HANDLE);
    }

    StabilityTrace_Write(L"INFO", L"vigem-output", L"stop.begin",
        L"generation=%llu", static_cast<unsigned long long>(requested.generation.Value()));
    g_vigemOutputRun.store(false, std::memory_order_release);
    VigemOutput_Wake();
    const DWORD waitResult = WaitForSingleObject(g_vigemOutputThread, 3000);
    const DWORD waitError = waitResult == WAIT_OBJECT_0
        ? ERROR_SUCCESS
        : (waitResult == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT);
    const auto observed = halljoy::lifecycle::ObserveWorkerJoin(
        requested.generation,
        waitResult == WAIT_OBJECT_0
            ? halljoy::lifecycle::JoinWaitStatus::Joined
            : (waitResult == WAIT_FAILED
                ? halljoy::lifecycle::JoinWaitStatus::Failed
                : halljoy::lifecycle::JoinWaitStatus::TimedOut),
        waitError);
    if (!observed.Completed())
    {
        StabilityTrace_WriteCritical(L"ERROR", L"vigem-output", L"stop.timeout",
            L"generation=%llu wait=%lu win32=%lu handles_retained=1 restart_blocked=1",
            static_cast<unsigned long long>(requested.generation.Value()), waitResult, waitError);
        return g_vigemOutputLifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            observed.error.code, observed.error.native_error);
    }

    CloseHandle(g_vigemOutputThread);
    g_vigemOutputThread = nullptr;
    CloseHandle(g_vigemOutputWakeEvent);
    g_vigemOutputWakeEvent = nullptr;
    g_vigemOutputThreadAlive.store(false, std::memory_order_release);
    const auto joined = g_vigemOutputLifecycle.ConfirmJoined(requested.generation);
    StabilityTrace_Write(L"INFO", L"vigem-output", L"stop.end",
        L"generation=%llu", static_cast<unsigned long long>(requested.generation.Value()));
    return joined;
}

// Cache: for HID <= 255 read once per tick
struct HidCache
{
    std::array<float, 256> raw{};
    std::array<float, 256> filtered{};
    std::array<float, 256> fullRaw{};
    std::bitset<256> fullPresent{};
    std::bitset<256> hasRaw{};
    std::bitset<256> hasFiltered{};
    bool sparkConnected = false;
    bool sayoConnected = false;
    bool addressedConnected = false;
    bool mad68Connected = false;
    bool hex80Connected = false;
    bool allowFallback = false;
    bool wootingReady = false;
    WootingAnalog_KeycodeType mode = WootingAnalog_KeycodeType_HID;
    bool hasFullBuffer = false;
};

struct SimulatedKeyState
{
    bool down = false;
    float value = 0.0f;
    ULONGLONG lastUpdateMs = 0;
};

static std::array<SimulatedKeyState, 256> g_simulatedKeys{};

// Persistent curve cache shared across realtime ticks. The old tick-local cache
// avoided duplicate work only inside one report build; a MAD68 A0 wake therefore
// recalculated every bound key even though only one HID had changed. Raw values
// and curve generation now form the cache key, so unchanged keys are only read
// and their previous filtered result is reused.
struct PersistentFilteredValue
{
    float raw = 0.0f;
    float filtered = 0.0f;
    uint64_t curveGeneration = 0;
    bool valid = false;
};
static std::array<PersistentFilteredValue, 256> g_persistentFiltered{};
static uint64_t g_persistentCurveCacheHits = 0;
static uint64_t g_persistentCurveCacheMisses = 0;

static void ResetPersistentFilteredCache()
{
    for (auto& value : g_persistentFiltered)
        value = {};
    g_persistentCurveCacheHits = 0;
    g_persistentCurveCacheMisses = 0;
}

static bool IsHidDownViaAsyncState(uint16_t hidKeycode)
{
    if (hidKeycode == 0 || hidKeycode >= 256) return false;

    uint16_t vk = g_hidToVk[hidKeycode].load(std::memory_order_relaxed);
    if (vk == 0)
        vk = HidFallbackToVk(hidKeycode);
    if (vk == 0)
        return false;

    return (GetAsyncKeyState((int)vk) & 0x8000) != 0;
}

static float ReadDigitalFallback01(uint16_t hidKeycode)
{
    if (hidKeycode == 0 || hidKeycode >= 256)
        return 0.0f;

    SimulatedKeyState& s = g_simulatedKeys[hidKeycode];

    ULONGLONG now = GetTickCount64();
    ULONGLONG prev = s.lastUpdateMs;
    float dtMs = 1.0f;
    if (prev != 0 && now > prev)
    {
        dtMs = (float)(now - prev);
        dtMs = std::clamp(dtMs, 0.5f, 40.0f);
    }
    s.lastUpdateMs = now;

    const bool down = IsHidDownViaAsyncState(hidKeycode);
    s.down = down;

    // Two-stage press curve:
    // 0.00 -> 0.70 in ~50 ms, then 0.70 -> 1.00 in ~50 ms.
    // Release is slightly smoother to avoid harsh jitter on quick taps.
    if (down)
    {
        if (s.value < 0.70f)
            s.value += (0.70f / 50.0f) * dtMs;
        else
            s.value += (0.30f / 50.0f) * dtMs;
    }
    else
    {
        s.value -= (1.00f / 80.0f) * dtMs;
    }

    s.value = std::clamp(s.value, 0.0f, 1.0f);
    return s.value;
}

static float ReadMouseBindRaw01(uint16_t hidKeycode)
{
    switch (hidKeycode)
    {
    case kMouseBindHidLButton: return g_mouseBindButtons[0].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidRButton: return g_mouseBindButtons[1].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidMButton: return g_mouseBindButtons[2].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidX1: return g_mouseBindButtons[3].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidX2: return g_mouseBindButtons[4].load(std::memory_order_relaxed) ? 1.0f : 0.0f;
    case kMouseBindHidWheelUp:
    {
        ULONGLONG now = GetTickCount64();
        ULONGLONG until = g_mouseWheelPulseUpUntilMs.load(std::memory_order_relaxed);
        return (now < until) ? 1.0f : 0.0f;
    }
    case kMouseBindHidWheelDown:
    {
        ULONGLONG now = GetTickCount64();
        ULONGLONG until = g_mouseWheelPulseDownUntilMs.load(std::memory_order_relaxed);
        return (now < until) ? 1.0f : 0.0f;
    }
    default:
        return 0.0f;
    }
}

static float ReadRaw01Cached(uint16_t hidKeycode, HidCache& cache)
{
    if (hidKeycode == 0) return 0.0f;
    if (MouseBind_IsPseudoHid(hidKeycode))
        return ReadMouseBindRaw01(hidKeycode);

    if (hidKeycode < 256)
    {
        if (cache.hasRaw.test(hidKeycode))
            return cache.raw[hidKeycode];

        uint16_t modeCode = cache.wootingReady ? HidToModeCode(hidKeycode, cache.mode) : 0;
        float v = 0.0f;
        // Every native protocol participates through the same descriptor catalog.
        // The exact physical device is excluded from UAP only after capability
        // proof, so max aggregation remains safe for multiple analogue keyboards.
        const NativeAnalogReadResult native = NativeAnalogBackends_ReadMilli(hidKeycode);
        if (native.owned)
            v = std::max(v, std::clamp((float)native.milli / 1000.0f, 0.0f, 1.0f));

        if (cache.wootingReady && modeCode != 0)
        {
            float wsdk = 0.0f;
            const bool snapshotMode = kPreferFullBufferSnapshot &&
                cache.mode == WootingAnalog_KeycodeType_HID;

            if (snapshotMode)
            {
                // Never fall back to per-key SDK calls in the Madlions build.
                // A missing/failed snapshot is treated as zero until the next
                // realtime-tick snapshot, avoiding blocking per-key SDK calls entirely.
                if (cache.hasFullBuffer && cache.fullPresent.test(hidKeycode))
                    wsdk = cache.fullRaw[hidKeycode];
            }
            else
            {
                // Normal builds retain the established per-key path.
                wsdk = ReadAnalogByCodeWithDeviceFallback(modeCode, hidKeycode);
                if (wsdk < 0.0f)
                {
                    int err = (int)std::lround(wsdk);
                    ULONGLONG now = GetTickCount64();
                    int prev = g_lastAnalogErrorCode.load(std::memory_order_relaxed);
                    ULONGLONG prevMs = g_lastAnalogErrorLogMs.load(std::memory_order_relaxed);
                    if (err != prev || now - prevMs >= 5000)
                    {
                        DebugLog_Write(
                            L"[backend.analog] read_analog hid=%u code=%u mode=%s err=%d",
                            (unsigned)hidKeycode,
                            (unsigned)modeCode,
                            KeycodeModeName((int)cache.mode),
                            err);
                        g_lastAnalogErrorCode.store(err, std::memory_order_relaxed);
                        g_lastAnalogErrorLogMs.store(now, std::memory_order_relaxed);
                    }
                    wsdk = 0.0f;
                }

                // Generic builds may optionally use a snapshot as a conservative
                // assist without changing their primary read behavior.
                if (kEnableFullBufferAssist &&
                    cache.hasFullBuffer &&
                    cache.mode == WootingAnalog_KeycodeType_HID &&
                    cache.fullPresent.test(hidKeycode))
                {
                    float vf = cache.fullRaw[hidKeycode];
                    if (std::isfinite(vf))
                    {
                        vf = Clamp01(vf);
                        if (vf > wsdk + 0.02f || (wsdk <= 0.001f && vf >= 0.01f))
                            wsdk = vf;
                    }
                }
            }

            if (!std::isfinite(wsdk)) wsdk = 0.0f;
            wsdk = Clamp01(wsdk);
            v = std::max(v, wsdk);
        }

        if (!std::isfinite(v)) v = 0.0f;
        v = Clamp01(v);

        // If no confirmed native analogue source owns this HID and the SDK path
        // provides only zeros, retain HallJoy's existing generic digital fallback.
        // Native protocol modules never derive depth from this fallback.
        if (cache.allowFallback && !native.owned && v <= 0.001f)
        {
            float sim = ReadDigitalFallback01(hidKeycode);
            if (sim > v)
            {
                v = sim;
                if (v >= 0.05f)
                    g_digitalFallbackWarnPending.store(true, std::memory_order_release);
            }
        }

        cache.raw[hidKeycode] = v;
        cache.hasRaw.set(hidKeycode);
        return v;
    }

    // HID>=256: no caching needed in this project (UI tracks <256 anyway).
    // Native addressed sources use byte-sized HID usages; HID>=256 stays SDK-only.
    uint16_t modeCode = cache.wootingReady ? HidToModeCode(hidKeycode, cache.mode) : 0;
    if (!cache.wootingReady || modeCode == 0)
        return 0.0f;

    float v = ReadAnalogByCodeWithDeviceFallback(modeCode, hidKeycode);
    if (v < 0.0f)
    {
        int err = (int)std::lround(v);
        ULONGLONG now = GetTickCount64();
        int prev = g_lastAnalogErrorCode.load(std::memory_order_relaxed);
        ULONGLONG prevMs = g_lastAnalogErrorLogMs.load(std::memory_order_relaxed);
        if (err != prev || now - prevMs >= 5000)
        {
            DebugLog_Write(
                L"[backend.analog] read_analog hid=%u code=%u mode=%s err=%d",
                (unsigned)hidKeycode,
                (unsigned)modeCode,
                KeycodeModeName((int)cache.mode),
                err);
            g_lastAnalogErrorCode.store(err, std::memory_order_relaxed);
            g_lastAnalogErrorLogMs.store(now, std::memory_order_relaxed);
        }
    }
    if (!std::isfinite(v)) v = 0.0f;
    v = Clamp01(v);
    if (cache.allowFallback && v <= 0.001f)
    {
        float sim = ReadDigitalFallback01(hidKeycode);
        if (sim > v)
        {
            v = sim;
            if (v >= 0.05f)
                g_digitalFallbackWarnPending.store(true, std::memory_order_release);
        }
    }
    return v;
}

static float ReadFiltered01Cached(uint16_t hidKeycode, HidCache& cache)
{
    if (hidKeycode == 0) return 0.0f;
    if (MouseBind_IsPseudoHid(hidKeycode))
        return ReadRaw01Cached(hidKeycode, cache);

    if (hidKeycode < 256)
    {
        if (cache.hasFiltered.test(hidKeycode))
            return cache.filtered[hidKeycode];

        float raw = ReadRaw01Cached(hidKeycode, cache);
        const uint64_t curveGeneration = BackendCurve_GetGeneration();
        PersistentFilteredValue& persistent = g_persistentFiltered[hidKeycode];
        float filtered = 0.0f;
        if (persistent.valid &&
            persistent.curveGeneration == curveGeneration &&
            persistent.raw == raw)
        {
            filtered = persistent.filtered;
            ++g_persistentCurveCacheHits;
        }
        else
        {
            filtered = BackendCurve_ApplyByHid(hidKeycode, raw);
            persistent.raw = raw;
            persistent.filtered = filtered;
            persistent.curveGeneration = curveGeneration;
            persistent.valid = true;
            ++g_persistentCurveCacheMisses;
        }

        cache.filtered[hidKeycode] = filtered;
        cache.hasFiltered.set(hidKeycode);
        return filtered;
    }

    float raw = ReadRaw01Cached(hidKeycode, cache);
    return BackendCurve_ApplyByHid(hidKeycode, raw);
}

static SHORT StickFromMinus1Plus1(float x)
{
    x = std::clamp(x, -1.0f, 1.0f);
    return (SHORT)std::lround(x * 32767.0f);
}

static uint8_t TriggerByte01(float v01)
{
    v01 = std::clamp(v01, 0.0f, 1.0f);
    return (uint8_t)std::lround(v01 * 255.0f);
}

static bool Pressed(float v01)
{
    // Simple logic threshold after curve applied
    return v01 >= 0.10f;
}

// ---- Snappy Joystick (SOCD-like) state (backend thread) ----
// One state per axis (LX,LY,RX,RY)
static std::array<std::array<uint8_t, 4>, kMaxVirtualPads> g_snappyPrevMinusDown{};
static std::array<std::array<uint8_t, 4>, kMaxVirtualPads> g_snappyPrevPlusDown{};
static std::array<std::array<int8_t, 4>, kMaxVirtualPads>  g_snappyLastDir{}; // -1 = minus, +1 = plus, 0 = unknown
static std::array<std::array<float, 4>, kMaxVirtualPads>   g_snappyMinusValley{};
static std::array<std::array<float, 4>, kMaxVirtualPads>   g_snappyPlusValley{};

static int AxisIndexSafe(Axis a)
{
    switch (a)
    {
    case Axis::LX: return 0;
    case Axis::LY: return 1;
    case Axis::RX: return 2;
    case Axis::RY: return 3;
    default:       return -1;
    }
}

static float AxisValue_WithConflictModes(int padIndex, Axis a, float minusV, float plusV)
{
    const bool snapStick = Settings_GetSnappyJoystick();
    const bool lastKeyPriority = Settings_GetLastKeyPriority();
    if (!snapStick && !lastKeyPriority)
        return plusV - minusV;

    int idx = AxisIndexSafe(a);
    if (idx < 0 || idx >= 4)
        return plusV - minusV;

    // detect "press" edges using the same semantics as buttons (stable threshold)
    bool minusDown = Pressed(minusV);
    bool plusDown = Pressed(plusV);

    int p = std::clamp(padIndex, 0, kMaxVirtualPads - 1);
    bool prevMinus = (g_snappyPrevMinusDown[(size_t)p][idx] != 0);
    bool prevPlus = (g_snappyPrevPlusDown[(size_t)p][idx] != 0);

    if (minusDown && !prevMinus) g_snappyLastDir[(size_t)p][idx] = -1;
    if (plusDown && !prevPlus)  g_snappyLastDir[(size_t)p][idx] = +1;

    if (lastKeyPriority)
    {
        // Re-trigger threshold for analog "re-press" while key is still logically down.
        // Example: user slightly releases key and presses again without crossing Pressed() threshold.
        const float repDelta = std::clamp(Settings_GetLastKeyPrioritySensitivity(), 0.02f, 0.95f);

        if (!minusDown)
        {
            g_snappyMinusValley[(size_t)p][idx] = 1.0f;
        }
        else if (!prevMinus)
        {
            g_snappyMinusValley[(size_t)p][idx] = minusV;
        }
        else
        {
            float& valley = g_snappyMinusValley[(size_t)p][idx];
            valley = std::min(valley, minusV);
            if ((minusV - valley) >= repDelta)
            {
                g_snappyLastDir[(size_t)p][idx] = -1;
                valley = minusV;
            }
        }

        if (!plusDown)
        {
            g_snappyPlusValley[(size_t)p][idx] = 1.0f;
        }
        else if (!prevPlus)
        {
            g_snappyPlusValley[(size_t)p][idx] = plusV;
        }
        else
        {
            float& valley = g_snappyPlusValley[(size_t)p][idx];
            valley = std::min(valley, plusV);
            if ((plusV - valley) >= repDelta)
            {
                g_snappyLastDir[(size_t)p][idx] = +1;
                valley = plusV;
            }
        }
    }

    g_snappyPrevMinusDown[(size_t)p][idx] = minusDown ? 1u : 0u;
    g_snappyPrevPlusDown[(size_t)p][idx] = plusDown ? 1u : 0u;

    float maxV = std::max(minusV, plusV);
    if (maxV <= 0.0001f)
        return 0.0f;

    if (lastKeyPriority)
    {
        // While only one side is logically pressed, keep output fully bound to that side.
        // This prevents partial cancellation when the opposite side starts moving but
        // has not crossed the press threshold yet.
        if (minusDown && !plusDown) return -minusV;
        if (plusDown && !minusDown) return +plusV;
    }

    // Last Key Priority: when both directions are down, most recent press wins.
    if (lastKeyPriority && minusDown && plusDown)
    {
        int8_t dir = g_snappyLastDir[(size_t)p][idx];
        if (dir == 0)
            dir = (plusV >= minusV) ? +1 : -1;

        float mag = 0.0f;
        if (snapStick)
        {
            // Keep "snap" punch while still honoring last pressed direction.
            mag = maxV;
        }
        else
        {
            mag = (dir > 0) ? plusV : minusV;
        }
        return (dir > 0) ? +mag : -mag;
    }

    // Snap Stick behavior: stronger side wins; if equal, last direction wins.
    if (snapStick)
    {
        constexpr float EQ_EPS = 0.002f; // tolerant equality (float noise)
        float d = plusV - minusV;

        if (std::fabs(d) > EQ_EPS)
            return (d > 0.0f) ? +maxV : -maxV;

        if (g_snappyLastDir[(size_t)p][idx] > 0) return +maxV;
        if (g_snappyLastDir[(size_t)p][idx] < 0) return -maxV;
        return 0.0f;
    }

    return plusV - minusV;
}

static void SetBtn(XUSB_REPORT& report, WORD mask, bool down)
{
    if (down) report.wButtons |= mask;
    else      report.wButtons &= ~mask;
}

static float MouseErrorToAxis(double err, float radius, float aggressiveness)
{
    if (radius <= 0.0001f) return 0.0f;
    double n = err / (double)radius;
    n *= (double)std::clamp(aggressiveness, 0.2f, 3.0f);
    // tanh gives smooth response around center with soft saturation on large offsets.
    float out = (float)std::tanh(n);
    if (std::fabs(out) < 0.0025f)
        out = 0.0f;
    return std::clamp(out, -1.0f, 1.0f);
}

static void ApplyMouseCardinalAssist(float& x, float& y)
{
    float ax = std::fabs(x);
    float ay = std::fabs(y);
    float major = std::max(ax, ay);
    float minor = std::min(ax, ay);
    if (major < 0.22f || minor <= 0.0001f)
        return;

    // Keep diagonals available near center, but on strong flicks prefer
    // cardinal directions (X/Y) and suppress the weak orthogonal axis.
    float edge = std::clamp((major - 0.35f) / 0.65f, 0.0f, 1.0f);
    float dominance = std::clamp((major - minor) / (major + 0.0001f), 0.0f, 1.0f);
    float strength = edge * dominance;

    // Near full tilt with clearly dominant axis: aggressively kill minor axis.
    if (major > 0.90f && minor < 0.24f)
        strength = std::max(strength, 0.95f);

    float minorScale = std::clamp(1.0f - 0.92f * strength, 0.04f, 1.0f);
    if (ax >= ay)
        y *= minorScale;
    else
        x *= minorScale;
}

static void AlignMouseOutputDirection(float targetX, float targetY, float& outX, float& outY)
{
    float tMajor = std::max(std::fabs(targetX), std::fabs(targetY));
    float oMajor = std::max(std::fabs(outX), std::fabs(outY));
    if (tMajor < 0.0001f || oMajor < 0.0001f)
        return;

    float tx = targetX / tMajor;
    float ty = targetY / tMajor;
    float ox = outX / oMajor;
    float oy = outY / oMajor;

    float edge = std::clamp((tMajor - 0.30f) / 0.70f, 0.0f, 1.0f);
    if (edge <= 0.0f)
        return;

    // Bias filtered output direction toward live target direction on strong motion.
    float mix = 0.18f + 0.62f * edge;
    float nx = ox + (tx - ox) * mix;
    float ny = oy + (ty - oy) * mix;
    float nMajor = std::max(std::fabs(nx), std::fabs(ny));
    if (nMajor <= 0.0001f)
        return;

    outX = (nx / nMajor) * oMajor;
    outY = (ny / nMajor) * oMajor;
}

static bool ReadMouseStickSample(float& outX, float& outY)
{
    outX = 0.0f;
    outY = 0.0f;

    if (!Settings_GetMouseToStickEnabled())
    {
        g_mouseHasLastPos = false;
        g_mouseSawRawInput.store(false, std::memory_order_relaxed);
        g_mouseFilteredX = 0.0f;
        g_mouseFilteredY = 0.0f;
        g_mouseTargetX = 0.0;
        g_mouseTargetY = 0.0;
        g_mouseFollowerX = 0.0;
        g_mouseFollowerY = 0.0;
        g_mouseLastTickMs = 0;
        g_mouseRawAccumDx.store(0, std::memory_order_relaxed);
        g_mouseRawAccumDy.store(0, std::memory_order_relaxed);
        g_mouseDbgEnabled.store(0, std::memory_order_relaxed);
        g_mouseDbgUsingRaw.store(0, std::memory_order_relaxed);
        g_mouseDbgTargetX10.store(0, std::memory_order_relaxed);
        g_mouseDbgTargetY10.store(0, std::memory_order_relaxed);
        g_mouseDbgFollowerX10.store(0, std::memory_order_relaxed);
        g_mouseDbgFollowerY10.store(0, std::memory_order_relaxed);
        g_mouseDbgOutX1000.store(0, std::memory_order_relaxed);
        g_mouseDbgOutY1000.store(0, std::memory_order_relaxed);
        g_mouseDbgRadius1000.store(1000, std::memory_order_relaxed);
        return false;
    }

    ULONGLONG nowMs = GetTickCount64();

    if (g_mouseLastTickMs == 0)
        g_mouseLastTickMs = nowMs;

    ULONGLONG dtRaw = (g_mouseLastTickMs > 0 && nowMs > g_mouseLastTickMs) ? (nowMs - g_mouseLastTickMs) : 1ull;
    g_mouseLastTickMs = nowMs;
    float dtMs = (float)std::clamp<ULONGLONG>(dtRaw, 1, 25);

    int rawDx = g_mouseRawAccumDx.exchange(0, std::memory_order_acq_rel);
    int rawDy = g_mouseRawAccumDy.exchange(0, std::memory_order_acq_rel);
    if (rawDx != 0 || rawDy != 0)
        g_mouseSawRawInput.store(true, std::memory_order_relaxed);

    LONG dx = (LONG)rawDx;
    LONG dy = (LONG)rawDy;

    // Fallback to cursor delta only when raw input has not been seen yet.
    if (!g_mouseSawRawInput.load(std::memory_order_relaxed))
    {
        POINT pt{};
        if (GetCursorPos(&pt))
        {
            if (!g_mouseHasLastPos)
            {
                g_mouseLastPos = pt;
                g_mouseHasLastPos = true;
            }
            else
            {
                dx = (LONG)(pt.x - g_mouseLastPos.x);
                dy = (LONG)(pt.y - g_mouseLastPos.y);
                g_mouseLastPos = pt;
            }
        }
    }

    const float sens = std::clamp(Settings_GetMouseToStickSensitivity(), 0.1f, 8.0f);
    const float aggressiveness = std::clamp(Settings_GetMouseToStickAggressiveness(), 0.2f, 3.0f);
    const float maxOffsetMul = std::clamp(Settings_GetMouseToStickMaxOffset(), 0.0f, 6.0f);
    const float followSpeedMul = std::clamp(Settings_GetMouseToStickFollowSpeed(), 0.2f, 3.0f);

    // Base mouse-space unit: how many raw counts are needed for "1.0" of virtual range.
    const float baseRange = std::clamp(92.0f / sens, 10.0f, 260.0f);
    // Slider-controlled max allowed virtual displacement from center.
    const double offsetLimit = (double)baseRange * (double)maxOffsetMul;

    auto smoothAxis = [dtMs](float current, float target) -> float
    {
        // Output smoothing is independent from offset and follower speed.
        const float tauMs = 5.0f;
        float alpha = 1.0f - std::exp(-dtMs / std::max(0.5f, tauMs));
        float v = current + (target - current) * alpha;
        if (std::fabs(v) < 0.0006f && std::fabs(target) < 0.0006f) v = 0.0f;
        return std::clamp(v, -1.0f, 1.0f);
    };

    if (offsetLimit <= 0.0001)
    {
        // Max offset = 0 means disabled movement envelope.
        g_mouseTargetX = 0.0;
        g_mouseTargetY = 0.0;
        g_mouseFollowerX = 0.0;
        g_mouseFollowerY = 0.0;
        g_mouseFilteredX = 0.0f;
        g_mouseFilteredY = 0.0f;
        outX = 0.0f;
        outY = 0.0f;
        g_mouseDbgEnabled.store(1, std::memory_order_relaxed);
        g_mouseDbgUsingRaw.store(g_mouseSawRawInput.load(std::memory_order_relaxed) ? 1u : 0u, std::memory_order_relaxed);
        g_mouseDbgTargetX10.store(0, std::memory_order_relaxed);
        g_mouseDbgTargetY10.store(0, std::memory_order_relaxed);
        g_mouseDbgFollowerX10.store(0, std::memory_order_relaxed);
        g_mouseDbgFollowerY10.store(0, std::memory_order_relaxed);
        g_mouseDbgOutX1000.store(0, std::memory_order_relaxed);
        g_mouseDbgOutY1000.store(0, std::memory_order_relaxed);
        g_mouseDbgRadius1000.store((int)std::lround((double)baseRange * 1000.0), std::memory_order_relaxed);
        return false;
    }

    // In this model g_mouseTarget is the live offset between virtual cursor and
    // virtual center ("zero point"), not an absolute world position.
    // This prevents hard one-way lock at map borders.
    // First, let center catch up (offset decays toward zero)...
    const double invOffset = 1.0 / offsetLimit;
    double errNormX = std::clamp(g_mouseTargetX * invOffset, -1.0, 1.0);
    double errNormY = std::clamp(g_mouseTargetY * invOffset, -1.0, 1.0);

    // Follow speed in normalized units per millisecond.
    const float followNormPerMs = std::clamp(0.018f * followSpeedMul, 0.0015f, 0.12f);
    auto moveTowardNorm2D = [dtMs, followNormPerMs](double& cx, double& cy, double tx, double ty)
    {
        double dxv = tx - cx;
        double dyv = ty - cy;
        double dist = std::sqrt(dxv * dxv + dyv * dyv);
        double maxStep = (double)followNormPerMs * (double)dtMs;
        if (dist <= 0.000001 || maxStep <= 0.0)
            return;
        if (dist <= maxStep)
        {
            cx = tx;
            cy = ty;
            return;
        }
        double s = maxStep / dist;
        cx += dxv * s;
        cy += dyv * s;
    };
    moveTowardNorm2D(errNormX, errNormY, 0.0, 0.0);

    // ...then apply this tick mouse movement.
    errNormX += (double)dx * invOffset;
    errNormY += (double)(-dy) * invOffset; // Y up like stick

    // Limit only the offset (difference), not absolute motion space.
    errNormX = std::clamp(errNormX, -1.0, 1.0);
    errNormY = std::clamp(errNormY, -1.0, 1.0);
    g_mouseTargetX = errNormX * offsetLimit;
    g_mouseTargetY = errNormY * offsetLimit;

    // Follower stays at center in this representation; offset itself is the error.
    g_mouseFollowerX = 0.0;
    g_mouseFollowerY = 0.0;

    // Snap tiny residuals to zero when fully idle.
    if (rawDx == 0 && rawDy == 0 && std::fabs(errNormX) < 0.003 && std::fabs(errNormY) < 0.003)
    {
        g_mouseTargetX = 0.0;
        g_mouseTargetY = 0.0;
        errNormX = 0.0;
        errNormY = 0.0;
    }

    float targetX = MouseErrorToAxis(errNormX, 1.0f, aggressiveness);
    float targetY = MouseErrorToAxis(errNormY, 1.0f, aggressiveness);
    ApplyMouseCardinalAssist(targetX, targetY);

    // 4) Smooth output and align its direction toward the live target direction.
    g_mouseFilteredX = smoothAxis(g_mouseFilteredX, targetX);
    g_mouseFilteredY = smoothAxis(g_mouseFilteredY, targetY);
    ApplyMouseCardinalAssist(g_mouseFilteredX, g_mouseFilteredY);
    AlignMouseOutputDirection(targetX, targetY, g_mouseFilteredX, g_mouseFilteredY);

    // Square stick space (independent axes): do NOT renormalize to a circle.
    // This keeps full X/Y output even when the other axis is slightly non-zero.
    g_mouseFilteredX = std::clamp(g_mouseFilteredX, -1.0f, 1.0f);
    g_mouseFilteredY = std::clamp(g_mouseFilteredY, -1.0f, 1.0f);

    outX = g_mouseFilteredX;
    outY = g_mouseFilteredY;
    g_mouseDbgEnabled.store(1, std::memory_order_relaxed);
    g_mouseDbgUsingRaw.store(g_mouseSawRawInput.load(std::memory_order_relaxed) ? 1u : 0u, std::memory_order_relaxed);
    g_mouseDbgTargetX10.store((int)std::lround(std::clamp(g_mouseTargetX, -200000.0, 200000.0) * 10.0), std::memory_order_relaxed);
    g_mouseDbgTargetY10.store((int)std::lround(std::clamp(g_mouseTargetY, -200000.0, 200000.0) * 10.0), std::memory_order_relaxed);
    g_mouseDbgFollowerX10.store((int)std::lround(std::clamp(g_mouseFollowerX, -200000.0, 200000.0) * 10.0), std::memory_order_relaxed);
    g_mouseDbgFollowerY10.store((int)std::lround(std::clamp(g_mouseFollowerY, -200000.0, 200000.0) * 10.0), std::memory_order_relaxed);
    g_mouseDbgOutX1000.store((int)std::lround(std::clamp((double)outX, -1.0, 1.0) * 1000.0), std::memory_order_relaxed);
    g_mouseDbgOutY1000.store((int)std::lround(std::clamp((double)outY, -1.0, 1.0) * 1000.0), std::memory_order_relaxed);
    g_mouseDbgRadius1000.store((int)std::lround((double)offsetLimit * 1000.0), std::memory_order_relaxed);
    return (std::fabs(outX) > 0.0001f || std::fabs(outY) > 0.0001f);
}

static SHORT MergeStickAxis(SHORT baseAxis, float mouseAxis01)
{
    SHORT mouse = StickFromMinus1Plus1(mouseAxis01);
    if (mouse == 0)
        return baseAxis;
    // Mouse should feel immediate, but keep keyboard if stronger on this tick.
    return (std::abs((int)mouse) >= std::abs((int)baseAxis)) ? mouse : baseAxis;
}

static bool BtnPressedFromMask(int padIndex, GameButton b, HidCache& cache)
{
    for (int chunk = 0; chunk < 4; ++chunk)
    {
        uint64_t bits = Bindings_GetButtonMaskChunkForPad(padIndex, b, chunk);
        if (!bits) continue;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        while (bits) {
            unsigned long idx = 0;
            _BitScanForward64(&idx, bits);
            bits &= (bits - 1);
            uint16_t hid = (uint16_t)(chunk * 64 + (int)idx);
            float v01 = ReadFiltered01Cached(hid, cache);
            if (Pressed(v01)) return true;
        }
#else
        for (int bit = 0; bit < 64; ++bit) {
            if (bits & (1ULL << bit)) {
                uint16_t hid = (uint16_t)(chunk * 64 + bit);
                float v01 = ReadFiltered01Cached(hid, cache);
                if (Pressed(v01)) return true;
            }
        }
#endif
    }
    return false;
}

static XUSB_REPORT BuildReportForPad(int padIndex, HidCache& cache)
{
    XUSB_REPORT report{};
    report.wButtons = 0;

    auto applyAxis = [&](Axis a, SHORT& out) {
        AxisBinding b = Bindings_GetAxisForPad(padIndex, a);
        float minusV = ReadFiltered01Cached(b.minusHid, cache);
        float plusV = ReadFiltered01Cached(b.plusHid, cache);
        out = StickFromMinus1Plus1(AxisValue_WithConflictModes(padIndex, a, minusV, plusV));
        };

    applyAxis(Axis::LX, report.sThumbLX);
    applyAxis(Axis::LY, report.sThumbLY);
    applyAxis(Axis::RX, report.sThumbRX);
    applyAxis(Axis::RY, report.sThumbRY);

    if (padIndex == 0 && Settings_GetMouseToStickEnabled())
    {
        float mx = 0.0f, my = 0.0f;
        if (ReadMouseStickSample(mx, my))
        {
            if (Settings_GetMouseToStickTarget() == 0)
            {
                report.sThumbLX = MergeStickAxis(report.sThumbLX, mx);
                report.sThumbLY = MergeStickAxis(report.sThumbLY, my);
            }
            else
            {
                report.sThumbRX = MergeStickAxis(report.sThumbRX, mx);
                report.sThumbRY = MergeStickAxis(report.sThumbRY, my);
            }
        }
    }

    report.bLeftTrigger = TriggerByte01(ReadFiltered01Cached(Bindings_GetTriggerForPad(padIndex, Trigger::LT), cache));
    report.bRightTrigger = TriggerByte01(ReadFiltered01Cached(Bindings_GetTriggerForPad(padIndex, Trigger::RT), cache));

    SetBtn(report, XUSB_GAMEPAD_A, BtnPressedFromMask(padIndex, GameButton::A, cache));
    SetBtn(report, XUSB_GAMEPAD_B, BtnPressedFromMask(padIndex, GameButton::B, cache));
    SetBtn(report, XUSB_GAMEPAD_X, BtnPressedFromMask(padIndex, GameButton::X, cache));
    SetBtn(report, XUSB_GAMEPAD_Y, BtnPressedFromMask(padIndex, GameButton::Y, cache));
    SetBtn(report, XUSB_GAMEPAD_LEFT_SHOULDER, BtnPressedFromMask(padIndex, GameButton::LB, cache));
    SetBtn(report, XUSB_GAMEPAD_RIGHT_SHOULDER, BtnPressedFromMask(padIndex, GameButton::RB, cache));
    SetBtn(report, XUSB_GAMEPAD_BACK, BtnPressedFromMask(padIndex, GameButton::Back, cache));
    SetBtn(report, XUSB_GAMEPAD_START, BtnPressedFromMask(padIndex, GameButton::Start, cache));
    SetBtn(report, XUSB_GAMEPAD_GUIDE, BtnPressedFromMask(padIndex, GameButton::Guide, cache));
    SetBtn(report, XUSB_GAMEPAD_LEFT_THUMB, BtnPressedFromMask(padIndex, GameButton::LS, cache));
    SetBtn(report, XUSB_GAMEPAD_RIGHT_THUMB, BtnPressedFromMask(padIndex, GameButton::RS, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_UP, BtnPressedFromMask(padIndex, GameButton::DpadUp, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_DOWN, BtnPressedFromMask(padIndex, GameButton::DpadDown, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_LEFT, BtnPressedFromMask(padIndex, GameButton::DpadLeft, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_RIGHT, BtnPressedFromMask(padIndex, GameButton::DpadRight, cache));

    return report;
}

static LONGLONG BackendQpcFrequency()
{
    static const LONGLONG frequency = []() -> LONGLONG {
        LARGE_INTEGER value{};
        return QueryPerformanceFrequency(&value) && value.QuadPart > 0 ? value.QuadPart : 1000;
    }();
    return frequency;
}

static LONGLONG BackendQpcNow()
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

static uint64_t BackendQpcElapsedUs(LONGLONG start, LONGLONG now)
{
    if (start <= 0)
        return UINT64_MAX;
    if (now <= start)
        return 0;
    const uint64_t delta = static_cast<uint64_t>(now - start);
    const uint64_t frequency = static_cast<uint64_t>(BackendQpcFrequency());
    return (delta / frequency) * 1000000ull + ((delta % frequency) * 1000000ull) / frequency;
}

static uint64_t BackendQpcIntervalTicks(uint64_t intervalUs)
{
    const uint64_t frequency = static_cast<uint64_t>(BackendQpcFrequency());
    const uint64_t whole = (intervalUs / 1000000ull) * frequency;
    const uint64_t remainder = intervalUs % 1000000ull;
    const uint64_t fractional = (remainder * frequency + 999999ull) / 1000000ull;
    return std::max<uint64_t>(1, whole + fractional);
}

LONGLONG Backend_GetNextOutputDeadlineQpc()
{
    uint64_t earliest = 0;
    for (const auto& scheduler : g_outputSchedulers)
    {
        const uint64_t due = scheduler.DueTick();
        if (due != 0 && (earliest == 0 || due < earliest))
            earliest = due;
    }
    return static_cast<LONGLONG>(earliest);
}

static bool IsReportDifferent(const XUSB_REPORT& a, const XUSB_REPORT& b)
{
    // Do not discard small analogue movements. If HallJoy calculated a new
    // XInput value, the low-latency output path must eventually deliver it.
    return a.wButtons != b.wButtons ||
        a.bLeftTrigger != b.bLeftTrigger ||
        a.bRightTrigger != b.bRightTrigger ||
        a.sThumbLX != b.sThumbLX ||
        a.sThumbLY != b.sThumbLY ||
        a.sThumbRX != b.sThumbRX ||
        a.sThumbRY != b.sThumbRY;
}


struct BackendTraceSamples
{
    static constexpr size_t kCapacity = 4096;
    std::array<uint32_t, kCapacity> values{};
    size_t count = 0;
    uint64_t sum = 0;
    uint32_t minimum = 0;
    uint32_t maximum = 0;
    uint64_t dropped = 0;

    void Add(uint64_t value)
    {
        const uint32_t v = static_cast<uint32_t>(std::min<uint64_t>(value, 0xffffffffull));
        if (count < values.size())
            values[count++] = v;
        else
            ++dropped;
        sum += v;
        if (minimum == 0 || v < minimum) minimum = v;
        if (v > maximum) maximum = v;
    }

    void Sort()
    {
        std::sort(values.begin(), values.begin() + count);
    }

    uint32_t Percentile(unsigned percentile) const
    {
        if (count == 0) return 0;
        const size_t index = ((count - 1) * std::min(percentile, 100u)) / 100u;
        return values[index];
    }

    uint64_t Average() const
    {
        return count != 0 ? sum / count : 0;
    }

    void Reset()
    {
        count = 0;
        sum = 0;
        minimum = 0;
        maximum = 0;
        dropped = 0;
    }
};

struct Mad68PipelineTraceWindow
{
    uint64_t batches = 0;
    uint64_t samples = 0;
    uint64_t coalescedSamples = 0;
    uint64_t dirtyHids = 0;
    uint64_t reportsReady = 0;
    uint64_t changedSends = 0;
    uint64_t noOutputChange = 0;
    uint64_t rateLimitedBatches = 0;
    BackendTraceSamples receiveToPublishUs{};
    BackendTraceSamples publishToReportReadyUs{};
    BackendTraceSamples receiveToReportReadyUs{};
    BackendTraceSamples reportReadyToSendUs{};
    BackendTraceSamples vigemCallUs{};
    BackendTraceSamples receiveToVigemEndUs{};
    uint64_t curveHitsStart = 0;
    uint64_t curveMissesStart = 0;

    void Reset()
    {
        batches = 0;
        samples = 0;
        coalescedSamples = 0;
        dirtyHids = 0;
        reportsReady = 0;
        changedSends = 0;
        noOutputChange = 0;
        rateLimitedBatches = 0;
        receiveToPublishUs.Reset();
        publishToReportReadyUs.Reset();
        receiveToReportReadyUs.Reset();
        reportReadyToSendUs.Reset();
        vigemCallUs.Reset();
        receiveToVigemEndUs.Reset();
        curveHitsStart = g_persistentCurveCacheHits;
        curveMissesStart = g_persistentCurveCacheMisses;
    }
};

static Mad68PipelineTraceWindow g_mad68PipelineTrace{};

static uint32_t CountMad68DirtyHids(const Mad68ProRChangeBatch& batch)
{
    uint32_t count = 0;
    for (uint64_t bits : batch.dirtyHids)
    {
        while (bits)
        {
            bits &= bits - 1;
            ++count;
        }
    }
    return count;
}

struct BackendTracePadWindow
{
    uint64_t decisions = 0;
    uint64_t changedCandidates = 0;
    uint64_t unchangedCandidates = 0;
    uint64_t rateLimited = 0;
    uint64_t unchangedSkipped = 0;
    uint64_t sends = 0;
    uint64_t changedSends = 0;
    uint64_t failures = 0;
    uint64_t intervalsBelow4ms = 0;
    uint64_t intervalsBelow2ms = 0;
    BackendTraceSamples sendIntervalUs{};
    BackendTraceSamples vigemCallUs{};
    BackendTraceSamples signalToSendUs{};

    void Reset()
    {
        decisions = 0;
        changedCandidates = 0;
        unchangedCandidates = 0;
        rateLimited = 0;
        unchangedSkipped = 0;
        sends = 0;
        changedSends = 0;
        failures = 0;
        intervalsBelow4ms = 0;
        intervalsBelow2ms = 0;
        sendIntervalUs.Reset();
        vigemCallUs.Reset();
        signalToSendUs.Reset();
    }
};

static std::array<BackendTracePadWindow, kMaxVirtualPads> g_backendTracePads{};
static std::array<uint64_t, kMaxVirtualPads> g_backendTraceLastInputSequence{};
static LONGLONG g_backendTraceWindowStartQpc = 0;
static bool g_backendTraceInitialised = false;
struct BackendTraceDetailedSample
{
    uint64_t sequence = 0;
    int pad = 0;
    uint64_t inputSequence = 0;
    uint64_t intervalUs = 0;
    uint64_t signalToSendUs = 0;
    uint64_t vigemCallUs = 0;
    XUSB_REPORT report{};
};

static std::array<BackendTraceDetailedSample, 32> g_backendTraceDetailedSamples{};
static size_t g_backendTraceDetailedCount = 0;
static uint64_t g_backendTraceSendSequence = 0;
static uint32_t g_backendTraceSparkLastChangedRows = 0;
static uint32_t g_backendTraceSparkLastInputNotifies = 0;

static void BackendLatencyTraceFlushDetailedSamples()
{
    if (!RealtimeLoop_IsLatencyTraceEnabled() || g_backendTraceDetailedCount == 0)
        return;

    DebugLog_WriteBuffered(L"[latency.samples.begin] count=%u written_at_shutdown=1", static_cast<unsigned>(g_backendTraceDetailedCount));
    for (size_t i = 0; i < g_backendTraceDetailedCount; ++i)
    {
        const auto& sample = g_backendTraceDetailedSamples[i];
        const auto& report = sample.report;
        DebugLog_WriteBuffered(
            L"[latency.sample] seq=%llu pad=%d input_seq=%llu interval_us=%llu signal_to_send_us=%llu vigem_call_us=%llu buttons=0x%04X lt=%u rt=%u lx=%d ly=%d rx=%d ry=%d",
            static_cast<unsigned long long>(sample.sequence),
            sample.pad,
            static_cast<unsigned long long>(sample.inputSequence),
            static_cast<unsigned long long>(sample.intervalUs),
            static_cast<unsigned long long>(sample.signalToSendUs),
            static_cast<unsigned long long>(sample.vigemCallUs),
            static_cast<unsigned>(report.wButtons),
            static_cast<unsigned>(report.bLeftTrigger),
            static_cast<unsigned>(report.bRightTrigger),
            static_cast<int>(report.sThumbLX),
            static_cast<int>(report.sThumbLY),
            static_cast<int>(report.sThumbRX),
            static_cast<int>(report.sThumbRY));
    }
    DebugLog_WriteBuffered(L"[latency.samples.end]");
    g_backendTraceDetailedCount = 0;
}

static void BackendLatencyTraceReset(LONGLONG nowQpc)
{
    g_backendTraceWindowStartQpc = nowQpc;
    for (auto& pad : g_backendTracePads)
        pad.Reset();
    g_mad68PipelineTrace.Reset();
}

static void BackendLatencyTraceLogInputSources(double windowSeconds)
{
    BackendAnalogTelemetry t{};
    Backend_GetAnalogTelemetry(&t);

    const uint32_t sparkChangedRows = g_sparkChangedRowSeq.load(std::memory_order_relaxed);
    const uint32_t sparkInputNotifies = g_sparkInputNotifySeq.load(std::memory_order_relaxed);
    const uint32_t changedRowsDelta = sparkChangedRows >= g_backendTraceSparkLastChangedRows
        ? sparkChangedRows - g_backendTraceSparkLastChangedRows
        : sparkChangedRows;
    const uint32_t inputNotifiesDelta = sparkInputNotifies >= g_backendTraceSparkLastInputNotifies
        ? sparkInputNotifies - g_backendTraceSparkLastInputNotifies
        : sparkInputNotifies;
    g_backendTraceSparkLastChangedRows = sparkChangedRows;
    g_backendTraceSparkLastInputNotifies = sparkInputNotifies;
    const double changedRowHz = windowSeconds > 0.0 ? static_cast<double>(changedRowsDelta) / windowSeconds : 0.0;
    const double sparkNotifyHz = windowSeconds > 0.0 ? static_cast<double>(inputNotifiesDelta) / windowSeconds : 0.0;
    const double wakeReduction = sparkNotifyHz > 0.0 ? (t.sparkRouteHz10 / 10.0) / sparkNotifyHz : 0.0;
    DebugLog_WriteBuffered(
        L"[latency.input] host_snapshot_hz=%.1f host_success_hz=%.1f host_publish_age_ms=%u host_total=%llu host_success_total=%llu host_dense_devices=%d host_active_keys=%d host_generation=%llu spark_route_hz=%.1f spark_matrix_hz=%.1f spark_changed_row_hz=%.1f spark_notify_hz=%.1f spark_wake_reduction=%.2fx spark_route_age_ms=%u sayo_depth_hz=%.1f sayo_age_ms=%u halljoy_timer_target_hz=%.1f",
        t.pluginHostPollHz10 / 10.0,
        t.pluginHostSuccessfulPollHz10 / 10.0,
        t.pluginHostLastPublishAgeMs,
        static_cast<unsigned long long>(t.pluginHostTotalPolls),
        static_cast<unsigned long long>(t.pluginHostSuccessfulPolls),
        t.pluginHostDenseDeviceCount,
        t.pluginHostActiveKeys,
        static_cast<unsigned long long>(t.pluginHostSnapshotGeneration),
        t.sparkRouteHz10 / 10.0,
        t.sparkMatrixHz10 / 10.0,
        changedRowHz,
        sparkNotifyHz,
        wakeReduction,
        t.sparkLastRouteAgeMs,
        t.sayoDepthHz10 / 10.0,
        t.sayoLastDepthAgeMs,
        t.sdkPollHz10 / 10.0);

    for (int i = 0; i < t.pluginDeviceCount; ++i)
    {
        const auto& d = t.pluginDevices[i];
        DebugLog_WriteBuffered(
            L"[latency.device] index=%d name=%S manufacturer=%S vid=%04X pid=%04X flags=0x%08X active=%u update_hz=%.1f interval_us_avg=%u interval_us_max=%u age_ms=%u updates=%llu",
            i,
            d.name[0] ? d.name : "-",
            d.manufacturer[0] ? d.manufacturer : "-",
            static_cast<unsigned>(d.vendorId),
            static_cast<unsigned>(d.productId),
            static_cast<unsigned>(d.flags),
            static_cast<unsigned>(d.activeKeys),
            d.updateHz10 / 10.0,
            d.averageUpdateIntervalUs,
            d.maximumUpdateIntervalUs,
            d.lastUpdateAgeMs,
            static_cast<unsigned long long>(d.updateCount));
    }
}

static void BackendLatencyTraceMaybeLog(LONGLONG nowQpc)
{
    if (!RealtimeLoop_IsLatencyTraceEnabled())
        return;

    if (!g_backendTraceInitialised)
    {
        g_backendTraceInitialised = true;
        g_backendTraceDetailedCount = 0;
        g_backendTraceSendSequence = 0;
        g_backendTraceLastInputSequence.fill(0);
        g_backendTraceSparkLastChangedRows = g_sparkChangedRowSeq.load(std::memory_order_relaxed);
        g_backendTraceSparkLastInputNotifies = g_sparkInputNotifySeq.load(std::memory_order_relaxed);
        BackendLatencyTraceReset(nowQpc);
        DebugLog_WriteBuffered(L"[latency.vigem.start] limiter_us=1000 duplicate_keepalive=disabled exact_report_compare=1 detailed_samples=32_buffered_until_shutdown");
        return;
    }

    const uint64_t windowUs = BackendQpcElapsedUs(g_backendTraceWindowStartQpc, nowQpc);
    if (windowUs < 1000000ull)
        return;

    const double windowSeconds = static_cast<double>(windowUs) / 1000000.0;
    const int padsToLog = std::max(1, std::clamp(
        g_virtualPadCount.load(std::memory_order_acquire), 0, kMaxVirtualPads));
    for (int i = 0; i < padsToLog; ++i)
    {
        auto& p = g_backendTracePads[static_cast<size_t>(i)];
        p.sendIntervalUs.Sort();
        p.vigemCallUs.Sort();
        p.signalToSendUs.Sort();
        DebugLog_WriteBuffered(
            L"[latency.vigem] pad=%d window_ms=%.1f decisions=%llu changed_candidates=%llu unchanged_candidates=%llu rate_limited_1ms=%llu unchanged_skipped=%llu sends=%llu send_hz=%.1f changed_sends=%llu failures=%llu interval_below_4ms=%llu interval_below_2ms=%llu send_interval_us[min/avg/p50/p95/p99/max]=%u/%llu/%u/%u/%u/%u vigem_call_us[min/avg/p50/p95/p99/max]=%u/%llu/%u/%u/%u/%u signal_to_send_us[min/avg/p50/p95/p99/max]=%u/%llu/%u/%u/%u/%u dropped_samples=%llu",
            i,
            static_cast<double>(windowUs) / 1000.0,
            static_cast<unsigned long long>(p.decisions),
            static_cast<unsigned long long>(p.changedCandidates),
            static_cast<unsigned long long>(p.unchangedCandidates),
            static_cast<unsigned long long>(p.rateLimited),
            static_cast<unsigned long long>(p.unchangedSkipped),
            static_cast<unsigned long long>(p.sends),
            windowSeconds > 0.0 ? static_cast<double>(p.sends) / windowSeconds : 0.0,
            static_cast<unsigned long long>(p.changedSends),
            static_cast<unsigned long long>(p.failures),
            static_cast<unsigned long long>(p.intervalsBelow4ms),
            static_cast<unsigned long long>(p.intervalsBelow2ms),
            p.sendIntervalUs.minimum,
            static_cast<unsigned long long>(p.sendIntervalUs.Average()),
            p.sendIntervalUs.Percentile(50),
            p.sendIntervalUs.Percentile(95),
            p.sendIntervalUs.Percentile(99),
            p.sendIntervalUs.maximum,
            p.vigemCallUs.minimum,
            static_cast<unsigned long long>(p.vigemCallUs.Average()),
            p.vigemCallUs.Percentile(50),
            p.vigemCallUs.Percentile(95),
            p.vigemCallUs.Percentile(99),
            p.vigemCallUs.maximum,
            p.signalToSendUs.minimum,
            static_cast<unsigned long long>(p.signalToSendUs.Average()),
            p.signalToSendUs.Percentile(50),
            p.signalToSendUs.Percentile(95),
            p.signalToSendUs.Percentile(99),
            p.signalToSendUs.maximum,
            static_cast<unsigned long long>(p.sendIntervalUs.dropped + p.vigemCallUs.dropped + p.signalToSendUs.dropped));
    }

    auto& m = g_mad68PipelineTrace;
    m.receiveToPublishUs.Sort();
    m.publishToReportReadyUs.Sort();
    m.receiveToReportReadyUs.Sort();
    m.reportReadyToSendUs.Sort();
    m.vigemCallUs.Sort();
    m.receiveToVigemEndUs.Sort();
    const uint64_t curveHits = g_persistentCurveCacheHits - m.curveHitsStart;
    const uint64_t curveMisses = g_persistentCurveCacheMisses - m.curveMissesStart;
    DebugLog_WriteBuffered(
        L"[latency.mad68] window_ms=%.1f batches=%llu samples=%llu coalesced=%llu dirty_hids=%llu reports_ready=%llu changed_sends=%llu no_output_change=%llu rate_limited_batches=%llu curve_cache_hits=%llu curve_recalculations=%llu receive_to_publish_us[min/avg/p50/p95/p99/max]=%u/%llu/%u/%u/%u/%u publish_to_report_ready_us[min/avg/p50/p95/p99/max]=%u/%llu/%u/%u/%u/%u receive_to_report_ready_us[min/avg/p50/p95/p99/max]=%u/%llu/%u/%u/%u/%u report_ready_to_send_us[min/avg/p50/p95/p99/max]=%u/%llu/%u/%u/%u/%u vigem_call_us[min/avg/p50/p95/p99/max]=%u/%llu/%u/%u/%u/%u receive_to_vigem_end_us[min/avg/p50/p95/p99/max]=%u/%llu/%u/%u/%u/%u",
        static_cast<double>(windowUs) / 1000.0,
        static_cast<unsigned long long>(m.batches),
        static_cast<unsigned long long>(m.samples),
        static_cast<unsigned long long>(m.coalescedSamples),
        static_cast<unsigned long long>(m.dirtyHids),
        static_cast<unsigned long long>(m.reportsReady),
        static_cast<unsigned long long>(m.changedSends),
        static_cast<unsigned long long>(m.noOutputChange),
        static_cast<unsigned long long>(m.rateLimitedBatches),
        static_cast<unsigned long long>(curveHits),
        static_cast<unsigned long long>(curveMisses),
        m.receiveToPublishUs.minimum, static_cast<unsigned long long>(m.receiveToPublishUs.Average()), m.receiveToPublishUs.Percentile(50), m.receiveToPublishUs.Percentile(95), m.receiveToPublishUs.Percentile(99), m.receiveToPublishUs.maximum,
        m.publishToReportReadyUs.minimum, static_cast<unsigned long long>(m.publishToReportReadyUs.Average()), m.publishToReportReadyUs.Percentile(50), m.publishToReportReadyUs.Percentile(95), m.publishToReportReadyUs.Percentile(99), m.publishToReportReadyUs.maximum,
        m.receiveToReportReadyUs.minimum, static_cast<unsigned long long>(m.receiveToReportReadyUs.Average()), m.receiveToReportReadyUs.Percentile(50), m.receiveToReportReadyUs.Percentile(95), m.receiveToReportReadyUs.Percentile(99), m.receiveToReportReadyUs.maximum,
        m.reportReadyToSendUs.minimum, static_cast<unsigned long long>(m.reportReadyToSendUs.Average()), m.reportReadyToSendUs.Percentile(50), m.reportReadyToSendUs.Percentile(95), m.reportReadyToSendUs.Percentile(99), m.reportReadyToSendUs.maximum,
        m.vigemCallUs.minimum, static_cast<unsigned long long>(m.vigemCallUs.Average()), m.vigemCallUs.Percentile(50), m.vigemCallUs.Percentile(95), m.vigemCallUs.Percentile(99), m.vigemCallUs.maximum,
        m.receiveToVigemEndUs.minimum, static_cast<unsigned long long>(m.receiveToVigemEndUs.Average()), m.receiveToVigemEndUs.Percentile(50), m.receiveToVigemEndUs.Percentile(95), m.receiveToVigemEndUs.Percentile(99), m.receiveToVigemEndUs.maximum);

    BackendLatencyTraceLogInputSources(windowSeconds);
    BackendLatencyTraceReset(nowQpc);
}

bool Backend_Init()
{
    StabilityTrace_Write(L"INFO", L"backend", L"init.begin");
    DebugLog_Write(L"[backend.init] begin");
    const auto previousOutput = VigemOutput_Stop();
    if (!previousOutput.RestartSafe())
    {
        g_lastInitIssues.store(BackendInitIssue_Unknown, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"backend", L"init.failed",
            L"stage=previous_vigem_output_stop");
        return false;
    }
    ResetPersistentFilteredCache();
    g_wootingSdkFaulted.store(false, std::memory_order_release);
    g_wootingOptionalFaultCount.store(0, std::memory_order_relaxed);
    g_wootingReadAnalogCalls.store(0, std::memory_order_relaxed);
    g_wootingReadFullCalls.store(0, std::memory_order_relaxed);
    g_wootingOtherApiCalls.store(0, std::memory_order_relaxed);
    g_madlionsSnapshotRaw.fill(0.0f);
    g_madlionsSnapshotPresent.reset();
    g_madlionsSnapshotValid = false;
    g_lastWootingApiStatsLogMs.store(0, std::memory_order_relaxed);
    g_wootingReady.store(false, std::memory_order_release);
    // Before-UAP protocol workers are lifecycle-managed by the common native
    // backend catalog. Stop any previous init attempt before resetting state.
    if (!NativeAnalogBackends_StopPhase(NativeAnalogStartPhase::BeforeUap))
    {
        g_lastInitIssues.store(BackendInitIssue_Unknown, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"backend", L"init.failed", L"stage=previous_native_stop");
        DebugLog_Write(L"[backend.init] previous pre-UAP native worker join failed");
        return false;
    }
    Spark_ResetKeyState();
    Sayo_ResetKeyState();
    g_sparkLastReconnectTryMs = 0;
    g_sayoLastReconnectTryMs = 0;
    g_tmTrackedMaxRawMilli.store(0, std::memory_order_relaxed);
    g_tmTrackedMaxOutMilli.store(0, std::memory_order_relaxed);
    g_tmFullBufferRet.store(0, std::memory_order_relaxed);
    g_tmFullBufferMaxMilli.store(0, std::memory_order_relaxed);
    g_tmFullBufferDeviceBestRet.store(0, std::memory_order_relaxed);
    g_tmFullBufferDeviceBestMaxMilli.store(0, std::memory_order_relaxed);
    g_virtualPadCount.store(std::clamp(Settings_GetVirtualGamepadCount(), 1, kMaxVirtualPads), std::memory_order_release);
    g_virtualPadsEnabled.store(Settings_GetVirtualGamepadsEnabled(), std::memory_order_release);
    g_lastInitIssues.store(BackendInitIssue_None, std::memory_order_release);
    g_reconnectRequested.store(false, std::memory_order_release);
    g_deviceChangeReconnectRequested.store(false, std::memory_order_release);
    g_ignoreDeviceChangeUntilMs.store(0, std::memory_order_release);
    g_vigemUpdateFailStreak = 0;
    g_zeroProbeStreak.store(0, std::memory_order_relaxed);
    g_autoRecoverTried.store(false, std::memory_order_relaxed);
    g_keycodeModeLocked.store(false, std::memory_order_relaxed);
    g_mouseHasLastPos = false;
    g_mouseSawRawInput.store(false, std::memory_order_relaxed);
    g_mouseFilteredX = 0.0f;
    g_mouseFilteredY = 0.0f;
    g_mouseTargetX = 0.0;
    g_mouseTargetY = 0.0;
    g_mouseFollowerX = 0.0;
    g_mouseFollowerY = 0.0;
    g_mouseLastTickMs = 0;
    g_mouseRawAccumDx.store(0, std::memory_order_relaxed);
    g_mouseRawAccumDy.store(0, std::memory_order_relaxed);
    g_mouseDbgEnabled.store(0, std::memory_order_relaxed);
    g_mouseDbgUsingRaw.store(0, std::memory_order_relaxed);
    g_mouseDbgTargetX10.store(0, std::memory_order_relaxed);
    g_mouseDbgTargetY10.store(0, std::memory_order_relaxed);
    g_mouseDbgFollowerX10.store(0, std::memory_order_relaxed);
    g_mouseDbgFollowerY10.store(0, std::memory_order_relaxed);
    g_mouseDbgOutX1000.store(0, std::memory_order_relaxed);
    g_mouseDbgOutY1000.store(0, std::memory_order_relaxed);
    g_mouseDbgRadius1000.store(1000, std::memory_order_relaxed);
    for (auto& b : g_mouseBindButtons) b.store(0, std::memory_order_relaxed);
    for (auto& s : g_physicalDown) s.store(0, std::memory_order_relaxed);
    g_mouseWheelPulseUpUntilMs.store(0, std::memory_order_relaxed);
    g_mouseWheelPulseDownUntilMs.store(0, std::memory_order_relaxed);

    uint32_t initIssues = BackendInitIssue_None;
    int wootingInit = WootingAnalogResult_NoPlugins;

    DebugLog_Write(L"[backend.init] native BeforeUap phase begin");
    const bool preUapReady = NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::BeforeUap);
    DebugLog_Write(L"[backend.init] native BeforeUap phase ready=%d", preUapReady ? 1 : 0);
    StabilityTrace_Write(preUapReady ? L"INFO" : L"WARN", L"backend", L"native_phase.start",
        L"phase=before_uap ready=%d", preUapReady ? 1 : 0);
    // All protocol modules report capability presence through the common catalog.
    // A validated native route keeps HallJoy usable when optional UAP is absent.
    const bool nativeReady = NativeAnalogBackends_AnyProtocolDevicePresent();

    DebugLog_Write(L"[backend.init] wooting_analog_initialise begin");
    wootingInit = wooting_analog_initialise();
    DebugLog_Write(L"[backend.init] wooting_analog_initialise ret=%d", wootingInit);
    // The SDK/plugin now run in an isolated child process. Reinstalling the
    // handler here remains a cheap integrity check for the HallJoy process.
    DebugLog_InstallCrashHandler();
    if (wootingInit >= 0)
    {
        g_wootingReady.store(true, std::memory_order_release);
        SetKeycodeModeWithLog(WootingAnalog_KeycodeType_HID, L"init", 0);
        DebugLog_Write(L"[backend.init] wooting snapshot begin");
        LogWootingStateSnapshot(L"after_init_call");
        if (kEnableDeviceInfoQuery)
        {
            DebugLog_Write(L"[backend.init] device snapshot begin");
            LogConnectedDevicesDetailed(L"after_init_call");
            DebugLog_Write(L"[backend.init] device snapshot done known_ids=%d",
                g_knownDeviceCount.load(std::memory_order_relaxed));
        }
        else
        {
            DebugLog_Write(L"[backend.init] device snapshot skipped by config");
        }
    }
    if (wootingInit < 0)
    {
        switch ((WootingAnalogResult)wootingInit)
        {
        case WootingAnalogResult_DLLNotFound:
        case WootingAnalogResult_FunctionNotFound:
            initIssues |= BackendInitIssue_PrivateUapUnavailable;
            break;
        case WootingAnalogResult_NoPlugins:
            initIssues |= BackendInitIssue_PrivateUapNoDevices;
            break;
        case WootingAnalogResult_IncompatibleVersion:
            initIssues |= BackendInitIssue_PrivateUapIncompatible;
            break;
        default:
            initIssues |= BackendInitIssue_Unknown;
            break;
        }
    }

    if (nativeReady)
    {
        // If a native HID path is up, Wooting SDK is optional.
        initIssues &= ~(BackendInitIssue_PrivateUapUnavailable |
            BackendInitIssue_PrivateUapNoDevices |
            BackendInitIssue_PrivateUapIncompatible);
        if (wootingInit < 0)
            initIssues &= ~BackendInitIssue_Unknown;
    }

    if (g_virtualPadsEnabled.load(std::memory_order_acquire))
    {
        // Initial virtual pad creation may also broadcast device changes.
        g_ignoreDeviceChangeUntilMs.store(GetTickCount64() + 1500, std::memory_order_release);
        VIGEM_ERROR err = VIGEM_ERROR_NONE;
        if (!Vigem_Create(g_virtualPadCount.load(std::memory_order_acquire), &err)) {
            DebugLog_Write(L"[backend.init] Vigem_Create failed err=%d", (int)err);
            g_vigemOk.store(false, std::memory_order_release);
            g_vigemLastErr.store(err, std::memory_order_release);
            StabilityTrace_WriteCritical(L"ERROR", L"vigem", L"init.failed",
                L"error=%d pads=%d", (int)err, g_virtualPadCount.load(std::memory_order_acquire));
            if (err == VIGEM_ERROR_BUS_NOT_FOUND)
                initIssues |= BackendInitIssue_VigemBusMissing;
            else
                initIssues |= BackendInitIssue_Unknown;
        }
        else
        {
            DebugLog_Write(L"[backend.init] Vigem_Create ok pads=%d", g_virtualPadCount.load(std::memory_order_acquire));
            g_vigemOk.store(true, std::memory_order_release);
            g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
            StabilityTrace_Write(L"INFO", L"vigem", L"init.ok",
                L"pads=%d", g_virtualPadCount.load(std::memory_order_acquire));
        }
    }
    else
    {
        g_vigemOk.store(true, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
    }

    if (initIssues == BackendInitIssue_None && !VigemOutput_Start())
    {
        initIssues |= BackendInitIssue_Unknown;
        StabilityTrace_WriteCritical(L"ERROR", L"vigem-output", L"start.failed");
    }

    if (initIssues != BackendInitIssue_None)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"backend", L"init.failed",
            L"issues=0x%08X native_ready=%d uap_result=%d", initIssues, nativeReady ? 1 : 0, wootingInit);
        DebugLog_Write(L"[backend.init] fail issues=0x%08X", initIssues);
        g_lastInitIssues.store(initIssues, std::memory_order_release);
        NativeAnalogBackends_StopPhase(NativeAnalogStartPhase::BeforeUap);
        Spark_ResetKeyState();
        Sayo_ResetKeyState();
        g_wootingReady.store(false, std::memory_order_release);
        const auto outputStopped = VigemOutput_Stop();
        if (outputStopped.RestartSafe())
            Vigem_Destroy();
        else
            StabilityTrace_WriteCritical(L"ERROR", L"backend", L"init.rollback_incomplete",
                L"component=vigem-output dependent_cleanup_skipped=1");
        const WootingAnalogResult analogStop = wooting_analog_uninitialise();
        if (analogStop != WootingAnalogResult_Ok)
        {
            StabilityTrace_WriteCritical(L"ERROR", L"backend", L"init.rollback_incomplete",
                L"component=analog-host result=%d", static_cast<int>(analogStop));
        }
        return false;
    }

    for (auto& a : g_uiAnalogM) a.store(0, std::memory_order_relaxed);
    for (auto& a : g_uiRawM)    a.store(0, std::memory_order_relaxed);
    for (auto& d : g_uiDirty)   d.store(0, std::memory_order_relaxed);
    const uint64_t outputIntervalTicks = BackendQpcIntervalTicks(1000);
    for (int i = 0; i < kMaxVirtualPads; ++i)
    {
        g_lastSentValid[(size_t)i] = 0;
        g_lastSentQpc[(size_t)i] = 0;
        g_lastSentReports[(size_t)i] = XUSB_REPORT{};
        g_outputSchedulers[(size_t)i].Configure(outputIntervalTicks);
    }

    StabilityTrace_Write(L"INFO", L"backend", L"init.ok",
        L"native_ready=%d uap_result=%d vigem_ok=%d pads=%d", nativeReady ? 1 : 0, wootingInit,
        g_vigemOk.load(std::memory_order_acquire) ? 1 : 0, g_virtualPadCount.load(std::memory_order_acquire));
    DebugLog_Write(L"[backend.init] success");
    return true;
}

bool Backend_Shutdown()
{
    StabilityTrace_Write(L"INFO", L"backend", L"shutdown.begin");
    DebugLog_Write(L"[backend] shutdown");
    const auto outputStopped = VigemOutput_Stop();
    if (!outputStopped.RestartSafe())
    {
        StabilityTrace_WriteCritical(L"ERROR", L"backend", L"shutdown.blocked",
            L"component=vigem-output dependent_cleanup_skipped=1");
        return false;
    }
    BackendLatencyTraceFlushDetailedSamples();
    g_wootingReady.store(false, std::memory_order_release);
    g_knownDeviceCount.store(0, std::memory_order_relaxed);
    g_mouseHasLastPos = false;
    g_mouseSawRawInput.store(false, std::memory_order_relaxed);
    g_mouseFilteredX = 0.0f;
    g_mouseFilteredY = 0.0f;
    g_mouseTargetX = 0.0;
    g_mouseTargetY = 0.0;
    g_mouseFollowerX = 0.0;
    g_mouseFollowerY = 0.0;
    g_mouseLastTickMs = 0;
    g_mouseRawAccumDx.store(0, std::memory_order_relaxed);
    g_mouseRawAccumDy.store(0, std::memory_order_relaxed);
    g_mouseDbgEnabled.store(0, std::memory_order_relaxed);
    g_mouseDbgUsingRaw.store(0, std::memory_order_relaxed);
    g_mouseDbgTargetX10.store(0, std::memory_order_relaxed);
    g_mouseDbgTargetY10.store(0, std::memory_order_relaxed);
    g_mouseDbgFollowerX10.store(0, std::memory_order_relaxed);
    g_mouseDbgFollowerY10.store(0, std::memory_order_relaxed);
    g_mouseDbgOutX1000.store(0, std::memory_order_relaxed);
    g_mouseDbgOutY1000.store(0, std::memory_order_relaxed);
    g_mouseDbgRadius1000.store(1000, std::memory_order_relaxed);
    for (auto& b : g_mouseBindButtons) b.store(0, std::memory_order_relaxed);
    for (auto& s : g_physicalDown) s.store(0, std::memory_order_relaxed);
    g_mouseWheelPulseUpUntilMs.store(0, std::memory_order_relaxed);
    g_mouseWheelPulseDownUntilMs.store(0, std::memory_order_relaxed);
    g_reconnectRequested.store(false, std::memory_order_release);
    g_deviceChangeReconnectRequested.store(false, std::memory_order_release);
    g_keycodeModeLocked.store(false, std::memory_order_relaxed);
    g_vigemUpdateFailStreak = 0;
    DebugLog_Write(L"[backend.shutdown] pre-UAP native stop begin");
    const bool nativeStopped = NativeAnalogBackends_StopPhase(NativeAnalogStartPhase::BeforeUap);
    Spark_ResetKeyState();
    Sayo_ResetKeyState();
    DebugLog_Write(L"[backend.shutdown] pre-UAP native stop joined=%d", nativeStopped ? 1 : 0);
    DebugLog_Write(L"[backend.shutdown] analog host stop begin");
    const WootingAnalogResult analogStop = wooting_analog_uninitialise();
    const bool analogHostStopped = analogStop == WootingAnalogResult_Ok;
    DebugLog_Write(L"[backend.shutdown] analog host stop done joined=%d result=%d",
        analogHostStopped ? 1 : 0, static_cast<int>(analogStop));
    StabilityTrace_Write(nativeStopped && analogHostStopped ? L"INFO" : L"ERROR",
        L"backend", L"shutdown.end", L"native_joined=%d analog_host_joined=%d vigem_output_joined=1",
        nativeStopped ? 1 : 0, analogHostStopped ? 1 : 0);
    DebugLog_Write(L"[backend.shutdown] complete");
    return nativeStopped && analogHostStopped;
}


void Backend_ResetPublishedStateAfterRealtimeFault() noexcept
{
    // Do not leave UI snapshots or the virtual controller's last published
    // state stuck after an unexpected worker exception. This path performs no
    // allocation and is never used for normal control flow.
    for (auto& value : g_uiAnalogM)
        value.store(0, std::memory_order_release);
    for (auto& value : g_uiRawM)
        value.store(0, std::memory_order_release);
    for (auto& dirty : g_uiDirty)
        dirty.store(UINT64_MAX, std::memory_order_release);

    g_bindCapturedPacked.store(0, std::memory_order_release);
    g_bindHadDown.store(false, std::memory_order_release);
    for (auto& state : g_physicalDown)
        state.store(0, std::memory_order_release);
    for (auto& button : g_mouseBindButtons)
        button.store(0, std::memory_order_release);
    g_mouseWheelPulseUpUntilMs.store(0, std::memory_order_release);
    g_mouseWheelPulseDownUntilMs.store(0, std::memory_order_release);
    g_mouseRawAccumDx.store(0, std::memory_order_release);
    g_mouseRawAccumDy.store(0, std::memory_order_release);

    const XUSB_REPORT neutral{};
    for (int i = 0; i < kMaxVirtualPads; ++i)
    {
        const size_t index = static_cast<size_t>(i);
        g_reports[index] = neutral;
        g_lastSentReports[index] = neutral;
        g_lastSentQpc[index] = 0;
        g_lastSentValid[index] = 0;
        g_outputSchedulers[index].Reset();
        PublishLastReport(i, neutral);
    }

    // The faulting realtime worker must never enter the driver. The independent
    // output owner observes this request and submits neutral state if possible.
    g_vigemEmergencyNeutralRequested.store(true, std::memory_order_release);
    VigemOutput_Wake();
    g_vigemOk.store(false, std::memory_order_release);
}

void Backend_Tick()
{
    ULONGLONG nowMs = GetTickCount64();
    const bool latencyTrace = RealtimeLoop_IsLatencyTraceEnabled();
    const LONGLONG backendTickQpc = latencyTrace ? BackendQpcNow() : 0;
    if (latencyTrace && !g_backendTraceInitialised)
        BackendLatencyTraceMaybeLog(backendTickQpc);

    Mad68ProRChangeBatch mad68Batch{};
    const bool mad68BatchPresent = Mad68ProR_ConsumeChangeBatch(&mad68Batch);
    const uint32_t mad68DirtyHidCount = mad68BatchPresent
        ? CountMad68DirtyHids(mad68Batch)
        : 0;
    if (latencyTrace && mad68BatchPresent)
    {
        auto& trace = g_mad68PipelineTrace;
        ++trace.batches;
        trace.samples += std::max<uint64_t>(1u, mad68Batch.sampleCount);
        trace.dirtyHids += mad68DirtyHidCount;
        if (mad68Batch.sampleCount > mad68DirtyHidCount)
            trace.coalescedSamples += mad68Batch.sampleCount - mad68DirtyHidCount;
        if (mad68Batch.latestA0ReceivedQpc > 0 &&
            mad68Batch.latestSnapshotPublishedQpc >= mad68Batch.latestA0ReceivedQpc)
        {
            trace.receiveToPublishUs.Add(BackendQpcElapsedUs(
                mad68Batch.latestA0ReceivedQpc,
                mad68Batch.latestSnapshotPublishedQpc));
        }
    }

    BackendCurve_BeginTick();
    ULONGLONG lastStateLog = g_lastWootingStateLogMs.load(std::memory_order_relaxed);
    if (g_wootingReady.load(std::memory_order_acquire) && nowMs - lastStateLog >= 10000)
    {
        g_lastWootingStateLogMs.store(nowMs, std::memory_order_relaxed);
        LogWootingStateSnapshot(L"tick_heartbeat");
    }
    SparkTickHotplug(nowMs);
    SayoTickHotplug(nowMs);

    HidCache cache;
    cache.sparkConnected = g_sparkConnected.load(std::memory_order_acquire);
    cache.sayoConnected = g_sayoConnected.load(std::memory_order_acquire);
    cache.addressedConnected = AddressedAnalog_IsConnected();
    cache.mad68Connected = Mad68ProR_IsConnected();
    cache.hex80Connected = Hex80_IsConnected();
    cache.allowFallback = Settings_GetDigitalFallbackInput() &&
        !cache.sparkConnected &&
        !cache.sayoConnected &&
        !cache.hex80Connected &&
        !cache.addressedConnected &&
        !cache.mad68Connected;
    // The plugin runs in the crash-isolated child, so it can safely coexist with
    // native SparkLink/Sayo/addressed backends. This also allows simultaneous rate
    // comparison instead of silently disabling Wooting/Madlions when IROK is present.
    cache.wootingReady = g_wootingReady.load(std::memory_order_acquire);
    cache.mode = (WootingAnalog_KeycodeType)g_keycodeMode.load(std::memory_order_relaxed);
    static uint32_t s_lastHandledKeyEventSeq = 0;
    static ULONGLONG s_lastFullAssistTickMs = 0;

    // Build the raw map from the isolated host's shared-memory snapshot. V9
    // reads it on every realtime tick; no blocking device I/O occurs here.
    if ((kEnableFullBufferAssist || kPreferFullBufferSnapshot) && cache.wootingReady)
    {
        const UINT assistMinPeriodMs = kPreferFullBufferSnapshot
            ? kMadlionsSnapshotPeriodMs
            : std::max<UINT>(4u, Settings_GetPollingMs());
        const bool assistDue = (s_lastFullAssistTickMs == 0) ||
            (nowMs - s_lastFullAssistTickMs >= assistMinPeriodMs);
        if (assistDue)
        {
            s_lastFullAssistTickMs = nowMs;
            if (cache.mode == WootingAnalog_KeycodeType_HID)
            {
                unsigned short codes[256]{};
                float vals[256]{};
                const int ret = wooting_analog_read_full_buffer(codes, vals, (unsigned)_countof(codes));
                g_tmFullBufferRet.store(ret, std::memory_order_relaxed);
                g_tmFullBufferMaxMilli.store(0, std::memory_order_relaxed);

                std::array<float, 256> nextRaw{};
                std::bitset<256> nextPresent{};
                uint16_t fullMaxMilli = 0;
                if (ret >= 0)
                {
                    const int n = std::min(ret, (int)_countof(codes));
                    for (int i = 0; i < n; ++i)
                    {
                        const unsigned short code = codes[i];
                        if (code >= 256) continue;
                        float v = vals[i];
                        if (!std::isfinite(v)) continue;
                        v = Clamp01(v);
                        const uint16_t milli = (uint16_t)std::clamp((int)std::lround(v * 1000.0f), 0, 1000);
                        fullMaxMilli = std::max(fullMaxMilli, milli);
                        nextPresent.set(code);
                        if (v > nextRaw[code])
                            nextRaw[code] = v;
                    }

                    // Merge per-device buffers if device enumeration is enabled.
                    const int ndev = std::clamp(g_knownDeviceCount.load(std::memory_order_relaxed), 0, (int)g_knownDeviceIds.size());
                    for (int di = 0; di < ndev; ++di)
                    {
                        unsigned short dcodes[128]{};
                        float dvals[128]{};
                        const int dret = wooting_analog_read_full_buffer_device(
                            dcodes, dvals, (unsigned)_countof(dcodes), g_knownDeviceIds[di]);
                        if (dret < 0) continue;
                        const int dn = std::min(dret, (int)_countof(dcodes));
                        for (int i = 0; i < dn; ++i)
                        {
                            const unsigned short code = dcodes[i];
                            if (code >= 256) continue;
                            float v = dvals[i];
                            if (!std::isfinite(v)) continue;
                            v = Clamp01(v);
                            nextPresent.set(code);
                            if (v > nextRaw[code])
                                nextRaw[code] = v;
                        }
                    }
                    g_madlionsSnapshotRaw = nextRaw;
                    g_madlionsSnapshotPresent = nextPresent;
                    g_madlionsSnapshotValid = true;
                }
                else
                {
                    // A failed snapshot must not trigger the old per-key fallback.
                    g_madlionsSnapshotRaw.fill(0.0f);
                    g_madlionsSnapshotPresent.reset();
                    g_madlionsSnapshotValid = false;
                }
                g_tmFullBufferMaxMilli.store(fullMaxMilli, std::memory_order_relaxed);
            }
        }

        if (kPreferFullBufferSnapshot && cache.mode == WootingAnalog_KeycodeType_HID)
        {
            cache.fullRaw = g_madlionsSnapshotRaw;
            cache.fullPresent = g_madlionsSnapshotPresent;
            cache.hasFullBuffer = g_madlionsSnapshotValid;
        }
    }

    int cnt = g_trackedCount.load(std::memory_order_acquire);
    cnt = std::clamp(cnt, 0, 256);
    uint16_t maxRawM = 0;
    uint16_t maxOutM = 0;
    uint16_t maxRawHid = 0;
    uint16_t maxOutHid = 0;

    // UI snapshot update
    for (int i = 0; i < cnt; ++i)
    {
        uint16_t hid = g_trackedList[i];
        if (hid == 0 || hid >= 256) continue;

        float raw = ReadRaw01Cached(hid, cache);
        float filtered = ReadFiltered01Cached(hid, cache);

        int rawM = (int)std::lround(raw * 1000.0f);
        rawM = std::clamp(rawM, 0, 1000);
        g_uiRawM[hid].store((uint16_t)rawM, std::memory_order_relaxed);
        if ((uint16_t)rawM >= maxRawM)
        {
            maxRawM = (uint16_t)rawM;
            maxRawHid = hid;
        }

        int outM = (int)std::lround(filtered * 1000.0f);
        outM = std::clamp(outM, 0, 1000);
        if ((uint16_t)outM >= maxOutM)
        {
            maxOutM = (uint16_t)outM;
            maxOutHid = hid;
        }

        uint16_t newV = (uint16_t)outM;
        uint16_t oldV = g_uiAnalogM[hid].load(std::memory_order_relaxed);
        if (oldV != newV) {
            int diff = std::abs((int)newV - (int)oldV);
            bool edge = (oldV == 0 || newV == 0 || oldV == 1000 || newV == 1000);
            if (diff >= 2 || edge)
            {
                g_uiAnalogM[hid].store(newV, std::memory_order_relaxed);
                int chunk = hid / 64;
                int bit = hid % 64;
                g_uiDirty[chunk].fetch_or(1ULL << bit, std::memory_order_relaxed);
            }
        }
    }
    g_tmTrackedMaxRawMilli.store(maxRawM, std::memory_order_relaxed);
    g_tmTrackedMaxOutMilli.store(maxOutM, std::memory_order_relaxed);
    ULONGLONG lastInputLog = g_lastInputStateLogMs.load(std::memory_order_relaxed);
    if (nowMs - lastInputLog >= 2000)
    {
        g_lastInputStateLogMs.store(nowMs, std::memory_order_relaxed);
        DebugLog_Write(
            L"[backend.input] tracked=%d max_raw=%u(hid=%u) max_out=%u(hid=%u)",
            cnt,
            (unsigned)maxRawM, (unsigned)maxRawHid,
            (unsigned)maxOutM, (unsigned)maxOutHid);
    }
    ULONGLONG lastSdkStats = g_lastWootingApiStatsLogMs.load(std::memory_order_relaxed);
    if (cache.wootingReady && nowMs - lastSdkStats >= 2000)
    {
        g_lastWootingApiStatsLogMs.store(nowMs, std::memory_order_relaxed);
        const uint64_t keyCalls = g_wootingReadAnalogCalls.exchange(0, std::memory_order_relaxed);
        const uint64_t fullCalls = g_wootingReadFullCalls.exchange(0, std::memory_order_relaxed);
        const uint64_t otherCalls = g_wootingOtherApiCalls.exchange(0, std::memory_order_relaxed);
        DebugLog_Write(
            L"[backend.sdk.stats] window_ms=2000 read_key=%llu read_full=%llu other=%llu serialized=1 snapshot_primary=%d snapshot_period_ms=%u full_ret=%d full_max=%u",
            (unsigned long long)keyCalls,
            (unsigned long long)fullCalls,
            (unsigned long long)otherCalls,
            kPreferFullBufferSnapshot ? 1 : 0,
            (unsigned)kMadlionsSnapshotPeriodMs,
            g_tmFullBufferRet.load(std::memory_order_relaxed),
            (unsigned)g_tmFullBufferMaxMilli.load(std::memory_order_relaxed));
    }
    if (kEnableFullBufferTelemetry)
    {
        ULONGLONG lastFullLog = g_lastFullBufferLogMs.load(std::memory_order_relaxed);
        if (g_wootingReady.load(std::memory_order_acquire) && nowMs - lastFullLog >= 2000)
        {
            g_lastFullBufferLogMs.store(nowMs, std::memory_order_relaxed);
            LogFullBufferSnapshot(L"periodic");
        }
    }

    // Probe keycode mode only via per-key event reads; full-buffer based probing
    // is intentionally avoided because some SDK/plugin versions expose noisy,
    // non-key-specific full-buffer activity.
    uint32_t keySeq = g_keyboardEventSeq.load(std::memory_order_acquire);
    if (kEnableAdaptiveKeycodeModeProbe && keySeq != s_lastHandledKeyEventSeq)
    {
        s_lastHandledKeyEventSeq = keySeq;
        uint16_t hidHint = g_keyboardEventHid.load(std::memory_order_relaxed);
        uint16_t scanHint = g_keyboardEventScan.load(std::memory_order_relaxed);
        uint16_t vkHint = g_keyboardEventVk.load(std::memory_order_relaxed);
        float probe = 0.0f;
        if (hidHint != 0)
            probe = ReadRaw01Cached(hidHint, cache);

        DebugLog_Write(
            L"[backend.mode] key_event seq=%u hid=%u scan=%u vk=%u probe=%.3f mode=%s",
            (unsigned)keySeq,
            (unsigned)hidHint,
            (unsigned)scanHint,
            (unsigned)vkHint,
            probe,
            KeycodeModeName(g_keycodeMode.load(std::memory_order_relaxed)));

        ULONGLONG now = GetTickCount64();
        ULONGLONG lastSwitch = g_lastKeycodeSwitchMs.load(std::memory_order_relaxed);
        if (hidHint != 0 && probe > 0.02f)
        {
            g_zeroProbeStreak.store(0, std::memory_order_relaxed);
            g_keycodeModeLocked.store(false, std::memory_order_relaxed);
        }
        else if (hidHint != 0 && probe <= 0.001f && now - lastSwitch >= 120)
        {
            g_zeroProbeStreak.fetch_add(1, std::memory_order_relaxed);
            bool found = AutoProbeKeycodeModeFromEvent(hidHint, scanHint, vkHint);
            if (found)
                LogFullBufferSnapshot(L"after_probe_found");
        }
    }

    // Bind capture: scan all HID 1..255 and capture first edge above threshold.
    if (g_bindCaptureEnabled.load(std::memory_order_acquire))
    {
        uint16_t bestHid = 0;
        int bestRawM = 0;
        for (uint16_t hid = 1; hid < 256; ++hid)
        {
            float raw = ReadRaw01Cached(hid, cache);
            int rawM = (int)std::lround(raw * 1000.0f);
            if (rawM > bestRawM)
            {
                bestRawM = rawM;
                bestHid = hid;
            }
        }

        bool down = (bestRawM >= 120);
        bool hadDown = g_bindHadDown.load(std::memory_order_relaxed);
        if (down && !hadDown && bestHid != 0)
        {
            uint32_t packed = (uint32_t)(bestHid & 0xFFFFu) | ((uint32_t)(bestRawM & 0xFFFFu) << 16);
            g_bindCapturedPacked.store(packed, std::memory_order_release);
        }
        g_bindHadDown.store(down, std::memory_order_relaxed);
    }
    else
    {
        g_bindHadDown.store(false, std::memory_order_relaxed);
    }

    int logicalPads = std::clamp(g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads);
    for (int pad = 0; pad < logicalPads; ++pad)
    {
        XUSB_REPORT report = BuildReportForPad(pad, cache);
        g_reports[(size_t)pad] = report;

        PublishLastReport(pad, report);
#if defined(HALLJOY_ANALOG_SIMULATOR)
        if (pad == 0)
            TraceSimulatorPipelineReport(report);
#endif
    }
    for (int pad = logicalPads; pad < kMaxVirtualPads; ++pad)
    {
        XUSB_REPORT report{};
        g_reports[(size_t)pad] = report;

        PublishLastReport(pad, report);
    }

    const LONGLONG mad68ReportReadyQpc =
        latencyTrace && mad68BatchPresent ? BackendQpcNow() : 0;
    bool mad68SendRecorded = false;
    bool mad68RateLimited = false;
    if (latencyTrace && mad68BatchPresent && mad68ReportReadyQpc > 0)
    {
        auto& trace = g_mad68PipelineTrace;
        ++trace.reportsReady;
        if (mad68Batch.latestSnapshotPublishedQpc > 0)
            trace.publishToReportReadyUs.Add(BackendQpcElapsedUs(
                mad68Batch.latestSnapshotPublishedQpc, mad68ReportReadyQpc));
        if (mad68Batch.latestA0ReceivedQpc > 0)
            trace.receiveToReportReadyUs.Add(BackendQpcElapsedUs(
                mad68Batch.latestA0ReceivedQpc, mad68ReportReadyQpc));
    }

    if (g_vigemResubmitRequested.exchange(false, std::memory_order_acq_rel))
    {
        for (int i = 0; i < kMaxVirtualPads; ++i)
        {
            g_lastSentValid[static_cast<size_t>(i)] = 0;
            g_outputSchedulers[static_cast<size_t>(i)].Reset();
        }
    }

    if (g_virtualPadsEnabled.load(std::memory_order_acquire))
    {
        VigemOutputBatch batch{};
        batch.count = static_cast<uint8_t>(std::clamp(
            g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads));
        const LONGLONG publishQpc = BackendQpcNow();
        bool hasChangedCandidate = false;
        for (int i = 0; i < batch.count; ++i)
        {
            const size_t index = static_cast<size_t>(i);
            const XUSB_REPORT& report = g_reports[index];
            batch.reports[index] = report;
            const bool valid = g_lastSentValid[index] != 0;
            const bool changed = !valid || IsReportDifferent(report, g_lastSentReports[index]);
            hasChangedCandidate = hasChangedCandidate || changed;
            BackendTracePadWindow* tracePad = latencyTrace ? &g_backendTracePads[index] : nullptr;
            if (tracePad)
            {
                ++tracePad->decisions;
                if (changed) ++tracePad->changedCandidates;
                else ++tracePad->unchangedCandidates;
            }

            auto& scheduler = g_outputSchedulers[index];
            // All analogue backends share the same scheduler. It governs
            // realtime-to-mailbox publication; driver latency is isolated
            // behind the output worker and cannot extend Backend_Tick.
            const auto decision = scheduler.Evaluate(changed, static_cast<uint64_t>(publishQpc));
            if (!changed)
            {
                // Periodic duplicate keepalives add no information. The output
                // owner retains the last submitted XUSB state until a changed
                // complete snapshot or target removal arrives.
                if (tracePad) ++tracePad->unchangedSkipped;
                continue;
            }
            if (decision == VigemOutputScheduler::Decision::DeferUntilDeadline)
            {
                if (tracePad) ++tracePad->rateLimited;
                if (mad68BatchPresent) mad68RateLimited = true;
                continue;
            }

            batch.validMask |= static_cast<uint8_t>(1u << i);
        }

        // Even when every changed pad is still rate-limited, refresh an
        // already-pending batch with the newest complete snapshot. Its due-pad
        // mask is preserved by the merge, so the output worker never emits an
        // older intermediate state merely because it was queued first.
        if (batch.validMask != 0 || hasChangedCandidate)
        {
            if (g_vigemOutputMailbox.TryPublishMerged(batch,
                [](const VigemOutputBatch& pending, VigemOutputBatch& next) noexcept {
                    const uint8_t validPads = next.count == 0
                        ? 0
                        : static_cast<uint8_t>((1u << next.count) - 1u);
                    next.validMask = static_cast<uint8_t>(
                        next.validMask | (pending.validMask & validPads));
                }))
            {
                VigemOutput_Wake();
                const uint64_t inputSequence = latencyTrace
                    ? RealtimeLoop_GetInputNotifySequence()
                    : 0;
                const LONGLONG inputNotifyQpc = latencyTrace
                    ? RealtimeLoop_GetLastInputNotifyQpc()
                    : 0;
                for (int i = 0; i < batch.count; ++i)
                {
                    if ((batch.validMask & (1u << i)) == 0)
                        continue;
                    const size_t index = static_cast<size_t>(i);
                    const bool valid = g_lastSentValid[index] != 0;
                    const uint64_t intervalUs = valid
                        ? BackendQpcElapsedUs(g_lastSentQpc[index], publishQpc)
                        : 0;
                    const uint64_t signalToPublishUs = latencyTrace && inputNotifyQpc > 0
                        ? BackendQpcElapsedUs(inputNotifyQpc, publishQpc)
                        : 0;
                    BackendTracePadWindow* tracePad = latencyTrace
                        ? &g_backendTracePads[index]
                        : nullptr;
                    if (tracePad)
                    {
                        ++tracePad->sends;
                        ++tracePad->changedSends;
                        if (valid)
                        {
                            tracePad->sendIntervalUs.Add(intervalUs);
                            if (intervalUs < 4000) ++tracePad->intervalsBelow4ms;
                            if (intervalUs < 2000) ++tracePad->intervalsBelow2ms;
                        }
                        if (signalToPublishUs != 0)
                            tracePad->signalToSendUs.Add(signalToPublishUs);
                        g_backendTraceLastInputSequence[index] = inputSequence;
                    }
                    g_lastSentReports[index] = batch.reports[index];
                    g_lastSentQpc[index] = publishQpc;
                    g_lastSentValid[index] = 1;
                    g_outputSchedulers[index].MarkSent(static_cast<uint64_t>(publishQpc));
                }

                if (latencyTrace && mad68BatchPresent)
                {
                    ++g_mad68PipelineTrace.changedSends;
                    if (mad68ReportReadyQpc > 0)
                        g_mad68PipelineTrace.reportReadyToSendUs.Add(
                            BackendQpcElapsedUs(mad68ReportReadyQpc, publishQpc));
                    mad68SendRecorded = true;
                }
            }
            else if (mad68BatchPresent)
            {
                mad68RateLimited = true;
            }
        }
    }

    if (latencyTrace && mad68BatchPresent && !mad68SendRecorded)
    {
        if (mad68RateLimited)
            ++g_mad68PipelineTrace.rateLimitedBatches;
        else
            ++g_mad68PipelineTrace.noOutputChange;
    }

    if (latencyTrace)
        BackendLatencyTraceMaybeLog(BackendQpcNow());
}

SHORT Backend_GetLastRX() { return g_lastRX[0].load(std::memory_order_acquire); }

XUSB_REPORT Backend_GetLastReport()
{
    return Backend_GetLastReportForPad(0);
}

XUSB_REPORT Backend_GetLastReportForPad(int padIndex)
{
    const int p = std::clamp(padIndex, 0, kMaxVirtualPads - 1);
    XUSB_REPORT report{};
    AcquireSRWLockShared(&g_lastReportLock);
    report = g_lastReport[static_cast<size_t>(p)];
    ReleaseSRWLockShared(&g_lastReportLock);
    return report;
}

void BackendUI_SetTrackedHids(const uint16_t* hids, int count)
{
    if (!hids || count <= 0) { BackendUI_ClearTrackedHids(); return; }
    count = std::clamp(count, 0, 256);

    g_trackedCount.store(0, std::memory_order_release);

    int outN = 0;
    for (int i = 0; i < count && outN < 256; ++i) {
        uint16_t hid = hids[i];
        if (hid == 0 || hid >= 256) continue;
        g_trackedList[outN++] = hid;
    }

    g_trackedCount.store(outN, std::memory_order_release);
    wchar_t sample[256]{};
    size_t used = 0;
    int sampleN = std::min(outN, 12);
    for (int i = 0; i < sampleN; ++i)
    {
        wchar_t t[16]{};
        _snwprintf_s(t, _countof(t), _TRUNCATE, (i == 0) ? L"%u" : L",%u", (unsigned)g_trackedList[i]);
        size_t left = _countof(sample) - 1 - used;
        if (left == 0) break;
        wcsncat_s(sample, _countof(sample), t, _TRUNCATE);
        used = wcslen(sample);
    }
    DebugLog_Write(L"[backend.ui] tracked set count=%d sample=%s", outN, sample[0] ? sample : L"-");
}

void BackendUI_ClearTrackedHids()
{
    g_trackedCount.store(0, std::memory_order_release);
    DebugLog_Write(L"[backend.ui] tracked cleared");
}

uint16_t BackendUI_GetAnalogMilli(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return 0;
    return g_uiAnalogM[hid].load(std::memory_order_relaxed);
}

uint16_t BackendUI_GetRawMilli(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return 0;
    return g_uiRawM[hid].load(std::memory_order_relaxed);
}

void BackendUI_SetBindCapture(bool enable)
{
    g_bindCaptureEnabled.store(enable, std::memory_order_release);
    if (!enable)
    {
        g_bindCapturedPacked.store(0, std::memory_order_release);
        g_bindHadDown.store(false, std::memory_order_relaxed);
    }
}

bool BackendUI_ConsumeBindCapture(uint16_t* outHid, uint16_t* outRawMilli)
{
    uint32_t p = g_bindCapturedPacked.exchange(0, std::memory_order_acq_rel);
    if (!p) return false;
    if (outHid) *outHid = (uint16_t)(p & 0xFFFFu);
    if (outRawMilli) *outRawMilli = (uint16_t)((p >> 16) & 0xFFFFu);
    return true;
}

uint64_t BackendUI_ConsumeDirtyChunk(int chunk)
{
    if (chunk < 0 || chunk >= 4) return 0;
    return g_uiDirty[chunk].exchange(0, std::memory_order_acq_rel);
}

BackendStatus Backend_GetStatus()
{
    BackendStatus s;
    s.vigemOk = g_vigemOk.load(std::memory_order_acquire);
    s.lastVigemError = g_vigemLastErr.load(std::memory_order_acquire);
    return s;
}

void Backend_GetAnalogTelemetry(BackendAnalogTelemetry* out)
{
    if (!out) return;
    BackendAnalogTelemetry t{};
    int nativeConnectedCount = 0;
    const std::size_t nativeCount = std::min<std::size_t>(
        NativeAnalogBackends_Count(), kBackendMaxNativeProtocols);
    for (std::size_t i = 0; i < nativeCount; ++i)
    {
        const NativeAnalogBackendDescriptor* descriptor = NativeAnalogBackends_Descriptor(i);
        NativeAnalogBackendTelemetry native{};
        if (!descriptor || !NativeAnalogBackends_GetTelemetry(i, &native))
            continue;
        if (!native.present && !native.connected)
            continue;
        auto& dst = t.nativeProtocols[t.nativeProtocolCount++];
        dst.present = native.present;
        dst.connected = native.connected;
        dst.protocol = static_cast<std::uint16_t>(descriptor->protocol);
        dst.vendorId = native.vendorId;
        dst.productId = native.productId;
        dst.usagePage = native.usagePage;
        dst.usage = native.usage;
        dst.flags = descriptor->flags;
        dst.mappedKeys = native.mappedKeys;
        dst.activeKeys = native.activeKeys;
        dst.nominalRawLevels = native.nominalRawLevels;
        dst.inputReportBytes = native.inputReportBytes;
        dst.outputReportBytes = native.outputReportBytes;
        dst.updateHz10 = native.updateHz10;
        dst.averageIntervalUs = native.averageIntervalUs;
        dst.maximumIntervalUs = native.maximumIntervalUs;
        dst.lastUpdateAgeMs = native.lastUpdateAgeMs;
        dst.successfulUpdates = native.successfulUpdates;
        dst.failedUpdates = native.failedUpdates;
        strncpy_s(dst.id, descriptor->id, _TRUNCATE);
        wcsncpy_s(dst.name, descriptor->displayName, _TRUNCATE);
        wcsncpy_s(dst.status, native.status, _TRUNCATE);
        if (native.connected) ++nativeConnectedCount;
    }
    const ULONGLONG nowMs = GetTickCount64();
    const bool sparkConnected = g_sparkConnected.load(std::memory_order_acquire);
    const bool sayoConnected = g_sayoConnected.load(std::memory_order_acquire);
    const bool addressedConnected = AddressedAnalog_IsConnected();
    const bool mad68Present = Mad68ProR_IsDevicePresent();
    const bool mad68Connected = Mad68ProR_IsConnected();
    Hex80Telemetry hex80{};
    Hex80_GetTelemetry(&hex80);
    AddressedAnalogTelemetry addressed{};
    AddressedAnalog_GetTelemetry(&addressed);
    // Do not call into the third-party SDK from the UI thread. The realtime
    // thread owns live reads and publishes readiness atomically.
    const bool sdkInited = g_wootingReady.load(std::memory_order_acquire);
    t.sdkInitialised = sdkInited || nativeConnectedCount != 0;
    int sdkDevCount = std::clamp(g_knownDeviceCount.load(std::memory_order_relaxed), 0, (int)g_knownDeviceIds.size());
    t.deviceCount = sdkDevCount + nativeConnectedCount;
    t.mad68Present = mad68Present;
    t.mad68Connected = mad68Connected;
    t.mad68Full = Mad68ProR_IsFullConnected();
    t.mad68EmergencyWasd = Mad68ProR_IsEmergencyWasd();
    t.mad68ProductId = Mad68ProR_GetProductId();
    t.mad68FirmwareVersion = Mad68ProR_GetFirmwareVersion();
    t.mad68Coverage = Mad68ProR_GetCoverage();
    t.mad68PublishedKeys = t.mad68Full ? 67u : (t.mad68EmergencyWasd ? 4u : 0u);
    t.hex80Present = hex80.present;
    t.hex80Connected = hex80.connected;
    t.hex80VendorId = hex80.vendorId;
    t.hex80ProductId = hex80.productId;
    t.hex80FirmwareVersion = hex80.firmwareVersion;
    t.hex80TravelMax = hex80.travelMax;
    t.hex80MappedKeys = hex80.mappedKeys;
    t.hex80ActiveKeys = hex80.activeKeys;
    t.hex80ObservedKeys = hex80.observedKeys;
    t.hex80InputReportBytes = hex80.inputReportBytes;
    t.hex80OutputReportBytes = hex80.outputReportBytes;
    t.hex80ChunkHz10 = hex80.chunkHz10;
    t.hex80MatrixHz10 = hex80.matrixHz10;
    t.hex80AvgTransactionUs = hex80.averageTransactionUs;
    t.hex80MaxTransactionUs = hex80.maximumTransactionUs;
    t.hex80AvgMatrixIntervalUs = hex80.averageMatrixIntervalUs;
    t.hex80MaxMatrixIntervalUs = hex80.maximumMatrixIntervalUs;
    t.hex80LastPacketAgeMs = hex80.lastPacketAgeMs;
    t.hex80PollAttempts = hex80.pollAttempts;
    t.hex80PollSuccess = hex80.pollSuccess;
    t.hex80PollFail = hex80.pollFail;
    t.hex80MatrixCycles = hex80.matrixCycles;
    t.addressedPresent = addressed.present;
    t.addressedConnected = addressed.connected;
    t.addressedVendorId = addressed.vendorId;
    t.addressedProductId = addressed.productId;
    t.addressedMappedKeys = addressed.mappedKeys;
    t.addressedActiveKeys = addressed.activeKeys;
    t.addressedInputReportBytes = addressed.inputReportBytes;
    t.addressedOutputReportBytes = addressed.outputReportBytes;
    t.addressedLastResponseAgeMs = addressed.lastResponseAgeMs;
    t.addressedPollAttempts = addressed.pollAttempts;
    t.addressedPollSuccess = addressed.pollSuccess;
    t.addressedPollFail = addressed.pollFail;
    t.sayoConnected = sayoConnected;
    t.sayoVendorId = g_sayoConnectedVid.load(std::memory_order_relaxed);
    t.sayoProductId = g_sayoConnectedPid.load(std::memory_order_relaxed);
    t.sayoReaders = std::clamp(g_sayoReaderCount.load(std::memory_order_relaxed), 0, (int)kSayoMaxDevices);
    t.sayoAvgDepthIntervalUs = g_sayoAvgDepthIntervalUs.load(std::memory_order_relaxed);
    t.sayoMaxDepthIntervalUs = g_sayoMaxDepthIntervalUs.load(std::memory_order_relaxed);
    if (t.sayoAvgDepthIntervalUs != 0)
        t.sayoDepthHz10 = (uint32_t)std::clamp<uint64_t>((10000000ull + (t.sayoAvgDepthIntervalUs / 2u)) / t.sayoAvgDepthIntervalUs, 0ull, 1000000ull);
    t.sayoDepthRawLevels = (uint32_t)kSayoRawFullScale + 1u;
    const ULONGLONG sayoLastDepthMs = g_sayoLastDepthMs.load(std::memory_order_relaxed);
    if (sayoLastDepthMs != 0 && nowMs >= sayoLastDepthMs)
        t.sayoLastDepthAgeMs = (uint32_t)std::min<ULONGLONG>(nowMs - sayoLastDepthMs, 0xffffffffull);
    const ULONGLONG sayoLastPacketMs = g_sayoLastPacketMs.load(std::memory_order_relaxed);
    if (sayoLastPacketMs != 0 && nowMs >= sayoLastPacketMs)
        t.sayoLastPacketAgeMs = (uint32_t)std::min<ULONGLONG>(nowMs - sayoLastPacketMs, 0xffffffffull);
    t.sayoPollAttempts = g_sayoPollAttempts.load(std::memory_order_relaxed);
    t.sayoPollSuccess = g_sayoPollSuccess.load(std::memory_order_relaxed);
    t.sayoPollFail = g_sayoPollFail.load(std::memory_order_relaxed);
    t.sayoDepthPackets = g_sayoDepthPackets.load(std::memory_order_relaxed);
    for (const auto& mapped : g_sayoIndexToHid)
        if (mapped.load(std::memory_order_relaxed) != 0) ++t.sayoMappedKeys;
    t.sayoInputReportBytes = g_sayoMaxInputReportBytes.load(std::memory_order_relaxed);
    t.sayoOutputReportBytes = g_sayoMaxOutputReportBytes.load(std::memory_order_relaxed);
    t.sayoWriteCapableReaders = g_sayoWriteCapableReaders.load(std::memory_order_relaxed);
    {
        uint32_t observedMin = UINT32_MAX;
        uint32_t observedMax = 0;
        uint64_t observedSum = 0;
        for (const auto& countAtomic : g_sayoObservedDepthCounts)
        {
            const uint32_t count = countAtomic.load(std::memory_order_relaxed);
            if (count == 0) continue;
            ++t.sayoObservedKeys;
            observedMin = std::min(observedMin, count);
            observedMax = std::max(observedMax, count);
            observedSum += count;
        }
        t.sayoObservedPositionsMin = t.sayoObservedKeys != 0 ? observedMin : 0;
        t.sayoObservedPositionsMax = observedMax;
        if (t.sayoObservedKeys != 0)
            t.sayoObservedPositionsAverage10 = (uint32_t)((observedSum * 10ull + t.sayoObservedKeys / 2u) / t.sayoObservedKeys);
    }
    t.sparkConnected = sparkConnected;
    t.sparkVendorId = g_sparkConnectedVid.load(std::memory_order_relaxed);
    t.sparkProductId = g_sparkConnectedPid.load(std::memory_order_relaxed);
    t.sparkRows = std::clamp(g_sparkRowCount.load(std::memory_order_relaxed), 0, kSparkMaxRows);
    t.sparkActiveRows = SparkActiveRowCount();
    t.sparkRouteQueries = g_sparkRouteQuerySeq.load(std::memory_order_relaxed);
    t.sparkRouteOk = g_sparkRouteOkSeq.load(std::memory_order_relaxed);
    t.sparkRouteFail = g_sparkRouteFailSeq.load(std::memory_order_relaxed);
    t.sparkAvgIntervalUs = g_sparkAvgRouteIntervalUs.load(std::memory_order_relaxed);
    t.sparkMaxIntervalUs = g_sparkMaxRouteIntervalUs.load(std::memory_order_relaxed);
    t.sparkAvgRouteTxUs = g_sparkAvgRouteTxUs.load(std::memory_order_relaxed);
    t.sparkMaxRouteTxUs = g_sparkMaxRouteTxUs.load(std::memory_order_relaxed);
    t.analogOutputLevels = 1001;
    if (t.sparkAvgIntervalUs != 0)
    {
        t.sparkRouteHz10 = (uint32_t)std::clamp<uint64_t>((10000000ull + (t.sparkAvgIntervalUs / 2u)) / t.sparkAvgIntervalUs, 0, 1000000);
        int activeRows = std::max(1, t.sparkActiveRows);
        t.sparkMatrixHz10 = t.sparkRouteHz10 / (uint32_t)activeRows;
    }
    t.sparkLastRouteRow = g_sparkLastRouteRow.load(std::memory_order_relaxed);
    t.sparkLastRouteOk = g_sparkLastRouteOk.load(std::memory_order_relaxed) != 0;
    t.sparkPollMode = Settings_GetSparkPollMode();
    t.sparkRowLimit = Settings_GetSparkRowLimit();
    UINT pollMs = std::max<UINT>(1u, Settings_GetPollingMs());
    t.sdkPollHz10 = std::min<uint32_t>(1000000u, (uint32_t)(10000u / pollMs));

    bool sparkSeen[256]{};
    uint32_t sparkMapped = 0;
    uint32_t rawMin = UINT32_MAX;
    uint32_t rawMax = 0;
    int sparkRowsForStats = t.sparkRowLimit > 0
        ? std::min(t.sparkRows, std::clamp((int)t.sparkRowLimit, 1, kSparkMaxRows))
        : t.sparkRows;
    for (int row = 0; row < sparkRowsForStats; ++row)
    {
        if (g_sparkRowActive[(size_t)row].load(std::memory_order_relaxed) == 0)
            continue;
        for (int col = 0; col < kSparkColsPerRow; ++col)
        {
            uint8_t hid = g_sparkRowColToHid[(size_t)row * kSparkColsPerRow + (size_t)col].load(std::memory_order_relaxed);
            if (hid == 0 || sparkSeen[hid])
                continue;
            sparkSeen[hid] = true;
            ++sparkMapped;
            uint32_t observed = g_sparkObservedRouteRaw[hid].load(std::memory_order_relaxed);
            rawMin = std::min(rawMin, observed);
            rawMax = std::max(rawMax, observed);
        }
    }
    t.sparkMappedAnalogKeys = sparkMapped;
    t.sparkObservedRawMin = (rawMin == UINT32_MAX) ? 0 : rawMin;
    t.sparkObservedRawMax = rawMax;
    ULONGLONG lastRouteMs = g_sparkLastRouteMs.load(std::memory_order_relaxed);
    if (lastRouteMs != 0 && nowMs >= lastRouteMs)
        t.sparkLastRouteAgeMs = (uint32_t)std::min<ULONGLONG>(nowMs - lastRouteMs, 0xffffffffull);
    ULONGLONG maxRowAgeMs = 0;
    int effectiveSparkRows = t.sparkRowLimit > 0
        ? std::min(t.sparkRows, std::clamp((int)t.sparkRowLimit, 1, kSparkMaxRows))
        : t.sparkRows;
    for (int row = 0; row < effectiveSparkRows; ++row)
    {
        if (g_sparkRowActive[(size_t)row].load(std::memory_order_relaxed) == 0)
            continue;
        ULONGLONG rowMs = g_sparkRowLastOkMs[(size_t)row].load(std::memory_order_relaxed);
        if (rowMs != 0 && nowMs >= rowMs)
            maxRowAgeMs = std::max(maxRowAgeMs, nowMs - rowMs);
    }
    t.sparkMaxRowAgeMs = (uint32_t)std::min<ULONGLONG>(maxRowAgeMs, 0xffffffffull);

    AnalogHostTelemetry host{};
    if (AnalogHostClient_GetTelemetry(&host))
    {
        t.pluginHostAvailable = host.available;
        t.pluginHostReady = host.ready;
        t.pluginHostStatus = host.status;
        t.pluginHostLastError = host.lastError;
        t.pluginHostTransportError = host.transportError;
        t.pluginHostRestartCount = host.restartCount;
        t.pluginHostInvalidSnapshots = host.invalidSnapshotCount;
        t.pluginHostActiveKeys = host.activeKeyCount;
        t.pluginHostDenseDeviceCount = host.denseDeviceCount;
        t.pluginHostSnapshotGeneration = host.snapshotGeneration;
        t.pluginHostSnapshotTimestampUs = host.snapshotTimestampUs;
        t.pluginHostPollHz10 = host.hostPollHz10;
        t.pluginHostSuccessfulPollHz10 = host.hostSuccessfulPollHz10;
        t.pluginHostLastPublishAgeMs = host.lastPublishAgeMs;
        t.pluginHostTotalPolls = host.totalPolls;
        t.pluginHostSuccessfulPolls = host.totalSuccessfulPolls;
        t.pluginDeviceCount = std::clamp(host.deviceCount, 0, kBackendMaxAnalogDevices);
        for (int i = 0; i < t.pluginDeviceCount; ++i)
        {
            const auto& src = host.devices[(size_t)i];
            auto& dst = t.pluginDevices[i];
            dst.present = true;
            dst.deviceId = src.deviceId;
            dst.vendorId = src.vendorId;
            dst.productId = src.productId;
            dst.usagePage = src.usagePage;
            dst.usage = src.usage;
            dst.flags = src.flags;
            dst.rows = src.rows;
            dst.columns = src.columns;
            dst.layoutKeySlots = src.layoutKeySlots;
            dst.nominalRawLevels = src.nominalRawLevels;
            dst.inputReportBytes = src.inputReportBytes;
            dst.outputReportBytes = src.outputReportBytes;
            dst.featureReportBytes = src.featureReportBytes;
            dst.bluetooth = src.bluetooth != 0;
            dst.observedDistinctLevels = src.observedDistinctLevels;
            dst.observedKeys = src.observedKeys;
            dst.observedLevelsPerKeyMin = src.observedLevelsPerKeyMin;
            dst.observedLevelsPerKeyMax = src.observedLevelsPerKeyMax;
            dst.observedLevelsPerKeyAverage10 = src.observedLevelsPerKeyAverage10;
            dst.activeKeys = src.activeKeys;
            dst.updateHz10 = src.updateHz10;
            dst.averageUpdateIntervalUs = src.averageUpdateIntervalUs;
            dst.maximumUpdateIntervalUs = src.maximumUpdateIntervalUs;
            dst.lastUpdateAgeMs = src.lastUpdateAgeMs;
            dst.updateCount = src.updateCount;
            memcpy(dst.manufacturer, src.manufacturer, sizeof(dst.manufacturer));
            dst.manufacturer[sizeof(dst.manufacturer) - 1] = '\0';
            memcpy(dst.name, src.name, sizeof(dst.name));
            dst.name[sizeof(dst.name) - 1] = '\0';
        }
        if (t.pluginDeviceCount > sdkDevCount)
        {
            t.deviceCount += t.pluginDeviceCount - sdkDevCount;
        }
    }

    t.keycodeMode = g_keycodeMode.load(std::memory_order_relaxed);
    t.keyboardEventSeq = g_keyboardEventSeq.load(std::memory_order_acquire);
    t.trackedMaxRawMilli = g_tmTrackedMaxRawMilli.load(std::memory_order_relaxed);
    t.trackedMaxOutMilli = g_tmTrackedMaxOutMilli.load(std::memory_order_relaxed);
    t.fullBufferRet = g_tmFullBufferRet.load(std::memory_order_relaxed);
    t.fullBufferMaxMilli = g_tmFullBufferMaxMilli.load(std::memory_order_relaxed);
    t.fullBufferDeviceBestRet = g_tmFullBufferDeviceBestRet.load(std::memory_order_relaxed);
    t.fullBufferDeviceBestMaxMilli = g_tmFullBufferDeviceBestMaxMilli.load(std::memory_order_relaxed);
    t.lastAnalogError = g_lastAnalogErrorCode.load(std::memory_order_relaxed);
    *out = t;
}

void Backend_GetMouseStickDebug(BackendMouseStickDebug* out)
{
    if (!out) return;
    BackendMouseStickDebug d{};
    d.enabled = (g_mouseDbgEnabled.load(std::memory_order_relaxed) != 0);
    d.usingRawInput = (g_mouseDbgUsingRaw.load(std::memory_order_relaxed) != 0);
    d.targetX = (float)g_mouseDbgTargetX10.load(std::memory_order_relaxed) / 10.0f;
    d.targetY = (float)g_mouseDbgTargetY10.load(std::memory_order_relaxed) / 10.0f;
    d.followerX = (float)g_mouseDbgFollowerX10.load(std::memory_order_relaxed) / 10.0f;
    d.followerY = (float)g_mouseDbgFollowerY10.load(std::memory_order_relaxed) / 10.0f;
    d.outputX = (float)g_mouseDbgOutX1000.load(std::memory_order_relaxed) / 1000.0f;
    d.outputY = (float)g_mouseDbgOutY1000.load(std::memory_order_relaxed) / 1000.0f;
    d.radius = std::max(0.001f, (float)g_mouseDbgRadius1000.load(std::memory_order_relaxed) / 1000.0f);
    *out = d;
}

bool Backend_ConsumeDigitalFallbackWarning()
{
    if (!Settings_GetDigitalFallbackInput())
    {
        g_digitalFallbackWarnPending.store(false, std::memory_order_release);
        return false;
    }
    return g_digitalFallbackWarnPending.exchange(false, std::memory_order_acq_rel);
}



void Backend_NotifyDeviceChange()
{
    if (!g_virtualPadsEnabled.load(std::memory_order_acquire))
        return;

    // Ignore generic device-change noise while ViGEm is healthy.
    // Realtime tick already detects real update failures and reconnects.
    if (g_vigemOk.load(std::memory_order_acquire))
        return;

    ULONGLONG now = GetTickCount64();
    ULONGLONG ignoreUntil = g_ignoreDeviceChangeUntilMs.load(std::memory_order_acquire);
    if (now < ignoreUntil)
        return;

    g_deviceChangeReconnectRequested.store(true, std::memory_order_release);
    VigemOutput_Wake();
}

void Backend_NotifyKeyboardEvent(
    uint16_t hidHint,
    uint16_t scanCode,
    uint16_t vkCode,
    bool isKeyDown,
    bool isInjected)
{
    if (hidHint == 0 || isInjected) return;

    if (hidHint < 256)
    {
        if (scanCode != 0) g_hidToScan[hidHint].store(scanCode, std::memory_order_relaxed);
        if (vkCode != 0) g_hidToVk[hidHint].store(vkCode, std::memory_order_relaxed);

        uint8_t prev = g_physicalDown[hidHint].load(std::memory_order_relaxed);
        uint8_t now = isKeyDown ? 1u : 0u;
        if (prev != now)
        {
            g_physicalDown[hidHint].store(now, std::memory_order_relaxed);
            if (kLogPhysicalKeyTransitions)
            {
                DebugLog_Write(
                    L"[backend.phys] %s hid=%u sc=%u vk=%u",
                    isKeyDown ? L"down" : L"up",
                    (unsigned)hidHint,
                    (unsigned)scanCode,
                    (unsigned)vkCode);
            }
        }

    }

    if (!isKeyDown) return;

    g_keyboardEventHid.store(hidHint, std::memory_order_relaxed);
    g_keyboardEventScan.store(scanCode, std::memory_order_relaxed);
    g_keyboardEventVk.store(vkCode, std::memory_order_relaxed);
    g_keyboardEventSeq.fetch_add(1u, std::memory_order_release);
}

void Backend_AddMouseDelta(int dx, int dy)
{
    if (dx != 0)
    {
        int old = g_mouseRawAccumDx.load(std::memory_order_relaxed);
        for (;;)
        {
            int nxt = std::clamp(old + dx, -32768, 32767);
            if (g_mouseRawAccumDx.compare_exchange_weak(old, nxt, std::memory_order_release, std::memory_order_relaxed))
                break;
        }
    }
    if (dy != 0)
    {
        int old = g_mouseRawAccumDy.load(std::memory_order_relaxed);
        for (;;)
        {
            int nxt = std::clamp(old + dy, -32768, 32767);
            if (g_mouseRawAccumDy.compare_exchange_weak(old, nxt, std::memory_order_release, std::memory_order_relaxed))
                break;
        }
    }
}

void Backend_SetMouseBindButtonState(uint16_t mouseBindHid, bool down)
{
    switch (mouseBindHid)
    {
    case kMouseBindHidLButton: g_mouseBindButtons[0].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    case kMouseBindHidRButton: g_mouseBindButtons[1].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    case kMouseBindHidMButton: g_mouseBindButtons[2].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    case kMouseBindHidX1: g_mouseBindButtons[3].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    case kMouseBindHidX2: g_mouseBindButtons[4].store(down ? 1u : 0u, std::memory_order_relaxed); break;
    default: break;
    }
}

void Backend_PulseMouseBindWheel(uint16_t mouseBindHid)
{
    // Keep wheel as a short digital pulse.
    constexpr ULONGLONG kPulseMs = 42;
    ULONGLONG until = GetTickCount64() + kPulseMs;
    if (mouseBindHid == kMouseBindHidWheelUp)
        g_mouseWheelPulseUpUntilMs.store(until, std::memory_order_relaxed);
    else if (mouseBindHid == kMouseBindHidWheelDown)
        g_mouseWheelPulseDownUntilMs.store(until, std::memory_order_relaxed);
}

void Backend_SetVirtualGamepadCount(int count)
{
    count = std::clamp(count, 1, kMaxVirtualPads);
    int old = g_virtualPadCount.exchange(count, std::memory_order_acq_rel);
    if (old != count)
    {
        g_reconnectRequested.store(true, std::memory_order_release);
        VigemOutput_Wake();
    }
}

int Backend_GetVirtualGamepadCount()
{
    return g_virtualPadCount.load(std::memory_order_acquire);
}

void Backend_SetVirtualGamepadsEnabled(bool on)
{
    bool old = g_virtualPadsEnabled.exchange(on, std::memory_order_acq_rel);
    if (old != on)
    {
        g_reconnectRequested.store(true, std::memory_order_release);
        VigemOutput_Wake();
    }
}

bool Backend_GetVirtualGamepadsEnabled()
{
    return g_virtualPadsEnabled.load(std::memory_order_acquire);
}

uint32_t Backend_GetLastInitIssues()
{
    return g_lastInitIssues.load(std::memory_order_acquire);
}



// ---- standard native-backend descriptors for integrated SparkLink/Sayo code ----
namespace
{
bool BackendNative_SparkPresent()
{
    return g_sparkConnected.load(std::memory_order_acquire);
}

bool BackendNative_SparkOwnsHid(std::uint16_t hidUsage)
{
    if (hidUsage == 0 || hidUsage >= 256 || !BackendNative_SparkPresent())
        return false;
    for (const auto& mapped : g_sparkRowColToHid)
    {
        if (mapped.load(std::memory_order_relaxed) == hidUsage)
            return true;
    }
    return false;
}

std::uint16_t BackendNative_SparkGetMilli(std::uint16_t hidUsage)
{
    return hidUsage < g_sparkAnalogMilli.size()
        ? g_sparkAnalogMilli[hidUsage].load(std::memory_order_relaxed)
        : 0;
}

void BackendNative_SparkTelemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return;
    *out = NativeAnalogBackendTelemetry{};
    out->present = BackendNative_SparkPresent();
    out->connected = out->present;
    out->vendorId = g_sparkConnectedVid.load(std::memory_order_relaxed);
    out->productId = g_sparkConnectedPid.load(std::memory_order_relaxed);
    out->usagePage = g_sparkUsagePage.load(std::memory_order_relaxed);
    out->usage = kSparkKnownUsage;
    out->mappedKeys = static_cast<std::uint32_t>(SparkActiveRowCount() * kSparkColsPerRow);
    std::uint32_t active = 0;
    for (const auto& value : g_sparkAnalogMilli)
        if (value.load(std::memory_order_relaxed) != 0) ++active;
    out->activeKeys = active;
    out->inputReportBytes = g_sparkInputReportLen;
    out->outputReportBytes = g_sparkOutputReportLen;
    const std::uint32_t ok = g_sparkRouteOkSeq.load(std::memory_order_relaxed);
    const std::uint32_t fail = g_sparkRouteFailSeq.load(std::memory_order_relaxed);
    out->successfulUpdates = ok;
    out->failedUpdates = fail;
    out->averageIntervalUs = g_sparkAvgRouteIntervalUs.load(std::memory_order_relaxed);
    out->maximumIntervalUs = g_sparkMaxRouteIntervalUs.load(std::memory_order_relaxed);
    const ULONGLONG last = g_sparkLastRouteMs.load(std::memory_order_relaxed);
    out->lastUpdateAgeMs = last == 0 ? 0u : static_cast<std::uint32_t>(
        std::min<ULONGLONG>(GetTickCount64() - last, 0xFFFFFFFFull));
    if (out->averageIntervalUs != 0)
        out->updateHz10 = static_cast<std::uint32_t>(10000000ull / out->averageIntervalUs);
    _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
        L"row polling, %d active rows, route ok/fail %u/%u",
        SparkActiveRowCount(), static_cast<unsigned>(ok), static_cast<unsigned>(fail));
}

bool BackendNative_SayoPresent()
{
    return g_sayoConnected.load(std::memory_order_acquire);
}

bool BackendNative_SayoOwnsHid(std::uint16_t hidUsage)
{
    if (hidUsage == 0 || hidUsage >= 256 || !BackendNative_SayoPresent())
        return false;
    for (const auto& mapped : g_sayoIndexToHid)
    {
        if (mapped.load(std::memory_order_relaxed) == hidUsage)
            return true;
    }
    return false;
}

std::uint16_t BackendNative_SayoGetMilli(std::uint16_t hidUsage)
{
    return hidUsage < g_sayoAnalogMilli.size()
        ? g_sayoAnalogMilli[hidUsage].load(std::memory_order_relaxed)
        : 0;
}

void BackendNative_SayoTelemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out) return;
    *out = NativeAnalogBackendTelemetry{};
    out->present = BackendNative_SayoPresent();
    out->connected = out->present;
    out->vendorId = g_sayoConnectedVid.load(std::memory_order_relaxed);
    out->productId = g_sayoConnectedPid.load(std::memory_order_relaxed);
    out->mappedKeys = 0;
    for (const auto& mapped : g_sayoIndexToHid)
        if (mapped.load(std::memory_order_relaxed) != 0) ++out->mappedKeys;
    for (const auto& value : g_sayoAnalogMilli)
        if (value.load(std::memory_order_relaxed) != 0) ++out->activeKeys;
    out->nominalRawLevels = kSayoRawFullScale + 1u;
    out->inputReportBytes = g_sayoMaxInputReportBytes.load(std::memory_order_relaxed);
    out->outputReportBytes = g_sayoMaxOutputReportBytes.load(std::memory_order_relaxed);
    out->successfulUpdates = g_sayoPollSuccess.load(std::memory_order_relaxed);
    out->failedUpdates = g_sayoPollFail.load(std::memory_order_relaxed);
    out->averageIntervalUs = g_sayoAvgDepthIntervalUs.load(std::memory_order_relaxed);
    out->maximumIntervalUs = g_sayoMaxDepthIntervalUs.load(std::memory_order_relaxed);
    const ULONGLONG last = g_sayoLastDepthMs.load(std::memory_order_relaxed);
    out->lastUpdateAgeMs = last == 0 ? 0u : static_cast<std::uint32_t>(
        std::min<ULONGLONG>(GetTickCount64() - last, 0xFFFFFFFFull));
    if (out->averageIntervalUs != 0)
        out->updateHz10 = static_cast<std::uint32_t>(10000000ull / out->averageIntervalUs);
    _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
        L"depth polling, %d readers, %u mapped keys",
        g_sayoReaderCount.load(std::memory_order_relaxed),
        static_cast<unsigned>(out->mappedKeys));
}
}

const NativeAnalogBackendDescriptor& BackendNative_GetSparkDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "sparklink",
        L"SparkLink / XD row protocol",
        NativeAnalogProtocol::SparkLink,
        NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_PolledTransport |
            NativeAnalogBackendFlag_ReversibleControlProbe |
            NativeAnalogBackendFlag_DynamicVidPid,
        nullptr,
        &SparkStartService,
        [](halljoy::lifecycle::GenerationId generation) {
            const auto stopped = SparkStopService();
            if (stopped.RestartSafe())
                return NativeAnalogBackendStopJoined(generation);
            const auto reason = stopped.error.code == halljoy::lifecycle::LifecycleErrorCode::None
                ? halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed
                : stopped.error.code;
            return NativeAnalogBackendStopFailed(
                generation, reason, stopped.error.native_error);
        },
        nullptr,
        &BackendNative_SparkPresent,
        &BackendNative_SparkPresent,
        &BackendNative_SparkOwnsHid,
        &BackendNative_SparkGetMilli,
        &BackendNative_SparkTelemetry,
    };
    return descriptor;
}

const NativeAnalogBackendDescriptor& BackendNative_GetSayoDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "sayo-depth",
        L"SayoDevice depth protocol",
        NativeAnalogProtocol::SayoDepth,
        NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_PolledTransport |
            NativeAnalogBackendFlag_ReversibleControlProbe |
            NativeAnalogBackendFlag_DynamicVidPid,
        nullptr,
        &SayoStartService,
        [](halljoy::lifecycle::GenerationId generation) {
            const auto stopped = SayoStop();
            if (stopped.RestartSafe())
                return NativeAnalogBackendStopJoined(generation);
            const auto reason = stopped.error.code == halljoy::lifecycle::LifecycleErrorCode::None
                ? halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed
                : stopped.error.code;
            return NativeAnalogBackendStopFailed(
                generation, reason, stopped.error.native_error);
        },
        nullptr,
        &BackendNative_SayoPresent,
        &BackendNative_SayoPresent,
        &BackendNative_SayoOwnsHid,
        &BackendNative_SayoGetMilli,
        &BackendNative_SayoTelemetry,
    };
    return descriptor;
}
