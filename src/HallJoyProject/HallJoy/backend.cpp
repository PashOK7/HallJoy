#include "profile_runtime_gate.h"
#include "input_privilege_warning.h"
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
#include <cwctype>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <ViGEm/Client.h>

#include "backend.h"
#include "analog_key_codes.h"
#include "bindings.h"
#include "settings.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "mouse_bind_codes.h"
#include "backend_curve.h"
#include "configured_xusb_builder.h"
#include "provider_v2_controller_shadow.h"
#include "provider_v2_qualification_model.h"
#include "xusb_output_adapter.h"
#include "analog_host_client.h"
#include "realtime_loop.h"
#include "hid_io_operation.h"
#include "addressed_analog_backend.h"
#include "aula_win60he_protocol.h"
#include "mad68pr_backend.h"
#include "hex80_backend.h"
#include "native_analog_routing.h"
#include "native_analog_backend_registry.h"
#include "native_hid_interface_claim_registry.h"
#include "monotonic_time.h"
#include "saturating_int.h"
#include "vigem_output_scheduler.h"
#include "vigem_output_runtime.h"
#if defined(HALLJOY_ANALOG_SIMULATOR)
#include "analog_simulator_backend.h"
#endif

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

static constexpr int kMaxVirtualPads = 4;
static std::atomic<int> g_virtualPadCount{ 1 };
static std::atomic<bool> g_virtualPadsEnabled{ true };
static std::atomic<bool> g_runtimeAdmission{ true };

static std::array<XUSB_REPORT, kMaxVirtualPads> g_reports{};
static std::array<XUSB_REPORT, kMaxVirtualPads> g_lastSentReports{};
static std::array<LONGLONG, kMaxVirtualPads> g_lastSentQpc{};
static std::array<uint8_t, kMaxVirtualPads> g_lastSentValid{};
static std::array<VigemOutputScheduler, kMaxVirtualPads> g_outputSchedulers{};

static halljoy::vigem_output::OutputRuntime g_vigemOutputRuntime;
static std::atomic<bool> g_vigemResubmitRequested{ false };
static std::atomic<std::uint64_t> g_vigemObservedGeneration{ 0 };
static std::atomic<bool> g_vigemOutputRecoveryBlocked{ false };
#if defined(HALLJOY_ANALOG_SIMULATOR)
static std::atomic<uint32_t> g_fileOnlyTestForbiddenInitAttempts{ 0 };
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

#endif

// ---- UI snapshot ----
static std::array<std::atomic<uint16_t>, halljoy::keycode::kCount> g_uiAnalogM{};
static std::array<std::atomic<uint16_t>, halljoy::keycode::kCount> g_uiRawM{};
static std::array<std::atomic<uint64_t>, halljoy::keycode::kMaskChunkCount> g_uiDirty{};

// list of HID codes to track (provided by UI)
struct TrackedKeys {
    std::array<uint16_t, halljoy::keycode::kCount> keys{};
    int count = 0;
};
static std::mutex g_trackingMutex;
static std::bitset<halljoy::keycode::kCount> g_mainTracking, g_overlayTracking;
static std::atomic<std::shared_ptr<const TrackedKeys>> g_trackingSnapshot{nullptr};

// bind-capture state (layout editor)
static std::atomic<bool>         g_bindCaptureEnabled{ false };
static std::atomic<uint32_t>     g_bindCapturedPacked{ 0 }; // low16=hid, high16=rawMilli
static std::atomic<bool>         g_bindHadDown{ false };

// ---- status / reconnect ----
static std::atomic<bool>         g_vigemOk{ false };
static std::atomic<VIGEM_ERROR>  g_vigemLastErr{ VIGEM_ERROR_NONE };
static std::atomic<uint32_t>     g_lastInitIssues{ BackendInitIssue_None };
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
static std::atomic<bool>         g_providerV2ShadowAvailable{ false };
static std::atomic<std::uint64_t> g_providerV2ShadowEligibleTicks{ 0 };
static std::atomic<std::uint64_t> g_providerV2ShadowMatchedReports{ 0 };
static std::atomic<std::uint64_t> g_providerV2ShadowMismatchedReports{ 0 };
static std::atomic<std::uint64_t> g_providerV2ShadowUnavailableTicks{ 0 };
static std::atomic<std::uint64_t> g_providerV2ShadowDigitalFallbackTicks{ 0 };
static std::atomic<std::uint64_t> g_providerV2ShadowCurveMutationTicks{ 0 };
static std::array<std::atomic<std::uint64_t>, 7>
    g_providerV2ShadowFieldMismatches{};
static std::atomic<std::uint64_t> g_providerV2ShadowBackendInitCount{ 0 };
static std::atomic<std::uint64_t> g_providerV2ShadowUniqueSampleGenerations{ 0 };
static std::atomic<std::uint64_t> g_providerV2ShadowFirstEligibleTickMs{ 0 };
static std::atomic<std::uint64_t> g_providerV2ShadowLastEligibleTickMs{ 0 };
static std::atomic<std::uint32_t> g_providerV2ShadowConfiguredFieldMask{ 0 };
static std::atomic<std::uint32_t> g_providerV2ShadowActivatedFieldMask{ 0 };
static std::atomic<std::uint32_t> g_providerV2ShadowReleasedFieldMask{ 0 };
static std::array<std::uint32_t, kMaxVirtualPads>
    g_providerV2ShadowPendingReleaseMasks{};
static std::atomic<std::uint32_t> g_providerV2ShadowLastMismatchMask{ 0 };
static std::atomic<std::uint32_t> g_providerV2ShadowLastMismatchPad{ 0 };
static std::atomic<std::uint64_t> g_providerV2ShadowLastSampleGeneration{ 0 };
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

static bool WootingSafe_CaptureTickSnapshot(
    halljoy::uap_parent_snapshot::SnapshotV1* out)
{
    if (!out || g_wootingSdkFaulted.load(std::memory_order_acquire))
        return false;

    g_wootingOtherApiCalls.fetch_add(1, std::memory_order_relaxed);
    AcquireSRWLockExclusive(&g_wootingApiLock);
    bool result = false;
    if (!g_wootingSdkFaulted.load(std::memory_order_acquire))
    {
        __try
        {
            result = AnalogHostClient_CaptureTickSnapshot(out);
        }
        __except (WootingSdk_SehFilterCritical(
            L"AnalogHostClient_CaptureTickSnapshot", GetExceptionCode()))
        {
            result = false;
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
        if (g_sparkMissedHidWasEnabled)
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

static halljoy::vigem_output::XusbReportV1 ToOutputReport(
    const XUSB_REPORT& report) noexcept
{
    halljoy::vigem_output::XusbReportV1 converted{};
    converted.buttons = report.wButtons;
    converted.leftTrigger = report.bLeftTrigger;
    converted.rightTrigger = report.bRightTrigger;
    converted.thumbLX = report.sThumbLX;
    converted.thumbLY = report.sThumbLY;
    converted.thumbRX = report.sThumbRX;
    converted.thumbRY = report.sThumbRY;
    return converted;
}

static VIGEM_ERROR OutputStatusError(
    const halljoy::vigem_output::OutputRuntimeStatus& status) noexcept
{
    return status.lastError == 0u
        ? VIGEM_ERROR_NONE
        : static_cast<VIGEM_ERROR>(status.lastError);
}

static void RefreshVigemOutputStatus(bool requestNewestOnGeneration) noexcept
{
    const auto status = g_vigemOutputRuntime.GetStatus();
    const bool ready = status.ready || !status.desiredEnabled;
    g_vigemOk.store(ready, std::memory_order_release);
    g_vigemLastErr.store(ready ? VIGEM_ERROR_NONE : OutputStatusError(status),
        std::memory_order_release);

    const std::uint64_t previous = g_vigemObservedGeneration.exchange(
        status.activeGeneration, std::memory_order_acq_rel);
    if (requestNewestOnGeneration && status.ready &&
        status.activeGeneration != 0u && status.activeGeneration != previous)
    {
        g_vigemResubmitRequested.store(true, std::memory_order_release);
        RealtimeLoop_NotifyInputChanged();
        StabilityTrace_Write(L"INFO", L"vigem-output", L"generation.route_ready",
            L"generation=%llu previous=%llu newest_snapshot_requested=1",
            static_cast<unsigned long long>(status.activeGeneration),
            static_cast<unsigned long long>(previous));
    }
}

static bool VigemOutput_Start()
{
    std::uint32_t error = ERROR_SUCCESS;
    const bool enabled = g_virtualPadsEnabled.load(std::memory_order_acquire);
    const std::uint32_t pads = static_cast<std::uint32_t>(std::clamp(
        g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads));
    const bool started = g_vigemOutputRuntime.Start(enabled, pads, error);
    RefreshVigemOutputStatus(false);
    if (!started)
    {
        g_vigemOk.store(false, std::memory_order_release);
        g_vigemLastErr.store(static_cast<VIGEM_ERROR>(error),
            std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"vigem-output", L"start.failed",
            L"process_isolated=1 pads=%lu enabled=%d error=%lu",
            static_cast<unsigned long>(pads), enabled ? 1 : 0,
            static_cast<unsigned long>(error));
        return false;
    }

    const auto status = g_vigemOutputRuntime.GetStatus();
    g_vigemOutputRecoveryBlocked.store(false, std::memory_order_release);
    g_vigemObservedGeneration.store(status.activeGeneration,
        std::memory_order_release);
    StabilityTrace_Write(L"INFO", L"vigem-output", L"start.ok",
        L"process_isolated=1 generation=%llu child_pid=%lu pads=%lu enabled=%d",
        static_cast<unsigned long long>(status.activeGeneration),
        static_cast<unsigned long>(status.childPid),
        static_cast<unsigned long>(pads), enabled ? 1 : 0);
    return true;
}

static bool VigemOutput_Stop()
{
    std::uint32_t error = ERROR_SUCCESS;
    const bool stopped = g_vigemOutputRuntime.Stop(error);
    g_vigemObservedGeneration.store(0u, std::memory_order_release);
    g_vigemOk.store(false, std::memory_order_release);
    if (!stopped)
    {
        g_vigemLastErr.store(static_cast<VIGEM_ERROR>(error),
            std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"vigem-output", L"stop.failed",
            L"owner_joined=0 error=%lu", static_cast<unsigned long>(error));
    }
    return stopped;
}

bool Backend_EnsureOutputRuntimeHealthy()
{
    if (g_vigemOutputRecoveryBlocked.load(std::memory_order_acquire))
        return false;

    auto status = g_vigemOutputRuntime.GetStatus();
    if (status.state == halljoy::vigem_output::OutputRuntimeState::Faulted ||
        !status.restartSafe)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"vigem-output",
            L"watchdog.session_rebuild.begin",
            L"state=%u error=%lu completed_generation=%llu",
            static_cast<unsigned>(status.state),
            static_cast<unsigned long>(status.lastError),
            static_cast<unsigned long long>(status.completedGeneration));
        std::uint32_t stopError = ERROR_SUCCESS;
        if (!g_vigemOutputRuntime.Stop(stopError))
        {
            g_vigemOk.store(false, std::memory_order_release);
            g_vigemLastErr.store(static_cast<VIGEM_ERROR>(stopError),
                std::memory_order_release);
            g_vigemOutputRecoveryBlocked.store(true, std::memory_order_release);
            const auto blocked = g_vigemOutputRuntime.GetStatus();
            StabilityTrace_WriteCritical(L"ERROR", L"vigem-output",
                L"watchdog.recovery_blocked",
                L"stage=stop error=%lu state=%u last_error=%lu last_outcome=%u restart_safe=%d completed_generation=%llu unsafe_generation=%llu unsafe_flags=%lu action=restart_halljoy",
                static_cast<unsigned long>(stopError),
                static_cast<unsigned>(blocked.state),
                static_cast<unsigned long>(blocked.lastError),
                static_cast<unsigned>(blocked.lastOutcome),
                blocked.restartSafe ? 1 : 0,
                static_cast<unsigned long long>(blocked.completedGeneration),
                static_cast<unsigned long long>(blocked.lastUnsafeGeneration),
                static_cast<unsigned long>(blocked.lastUnsafeFlags));
            return false;
        }
        std::uint32_t startError = ERROR_SUCCESS;
        const bool restarted = g_vigemOutputRuntime.Start(
            g_virtualPadsEnabled.load(std::memory_order_acquire),
            static_cast<std::uint32_t>(std::clamp(
                g_virtualPadCount.load(std::memory_order_acquire),
                1, kMaxVirtualPads)),
            startError);
        if (!restarted)
        {
            g_vigemOk.store(false, std::memory_order_release);
            g_vigemLastErr.store(static_cast<VIGEM_ERROR>(startError),
                std::memory_order_release);
            g_vigemOutputRecoveryBlocked.store(true, std::memory_order_release);
            const auto blocked = g_vigemOutputRuntime.GetStatus();
            StabilityTrace_WriteCritical(L"ERROR", L"vigem-output",
                L"watchdog.recovery_blocked",
                L"stage=start error=%lu state=%u last_error=%lu last_outcome=%u restart_safe=%d completed_generation=%llu unsafe_generation=%llu unsafe_flags=%lu action=restart_halljoy",
                static_cast<unsigned long>(startError),
                static_cast<unsigned>(blocked.state),
                static_cast<unsigned long>(blocked.lastError),
                static_cast<unsigned>(blocked.lastOutcome),
                blocked.restartSafe ? 1 : 0,
                static_cast<unsigned long long>(blocked.completedGeneration),
                static_cast<unsigned long long>(blocked.lastUnsafeGeneration),
                static_cast<unsigned long>(blocked.lastUnsafeFlags));
            return false;
        }
        g_vigemOutputRecoveryBlocked.store(false, std::memory_order_release);
        status = g_vigemOutputRuntime.GetStatus();
        StabilityTrace_Write(L"INFO", L"vigem-output",
            L"watchdog.session_rebuild.end",
            L"restart_safe=1 generation=%llu child_pid=%lu",
            static_cast<unsigned long long>(status.activeGeneration),
            static_cast<unsigned long>(status.childPid));
    }

    RefreshVigemOutputStatus(true);
    status = g_vigemOutputRuntime.GetStatus();
    // Starting and Recovering are bounded states owned by the supervisor. The
    // watchdog must not create a competing owner while that recovery is active.
    return status.ready || !status.desiredEnabled ||
        status.state == halljoy::vigem_output::OutputRuntimeState::Starting ||
        status.state == halljoy::vigem_output::OutputRuntimeState::Recovering;
}
// Cache every supported ordinary or extended key code once per tick.
struct HidCache
{
    std::array<float, halljoy::keycode::kCount> raw{};
    std::array<float, halljoy::keycode::kCount> filtered{};
    std::array<NativeAnalogReadResult, halljoy::keycode::kCount> native{};
    std::array<float, 256> fullRaw{};
    std::bitset<256> fullPresent{};
    halljoy::provider_v2_shadow::RawInputMapV1 providerV2Raw{};
    std::bitset<halljoy::keycode::kCount> hasRaw{};
    std::bitset<halljoy::keycode::kCount> hasFiltered{};
    std::bitset<halljoy::keycode::kCount> hasNative{};
#if defined(HALLJOY_ANALOG_SIMULATOR)
    std::bitset<halljoy::keycode::kCount> isolatedSynthetic{};
#endif
    bool sparkConnected = false;
    bool sayoConnected = false;
    bool addressedConnected = false;
    bool mad68Connected = false;
    bool hex80Connected = false;
    bool allowFallback = false;
    bool wootingReady = false;
    WootingAnalog_KeycodeType mode = WootingAnalog_KeycodeType_HID;
    bool hasFullBuffer = false;
    bool hasAuthoritativeUapDense = false;
    bool hasAuthoritativeProviderV2 = false;
    const halljoy::uap_parent_snapshot::SnapshotV1* uapTickSnapshot = nullptr;
};

struct ProviderV2ShadowTick
{
    halljoy::provider_v2_shadow::RawInputMapV1 providerRaw{};
    std::array<float, halljoy::keycode::kCount> raw{};
    std::array<float, halljoy::keycode::kCount> filtered{};
    std::bitset<halljoy::keycode::kCount> hasRaw{};
    std::bitset<halljoy::keycode::kCount> hasFiltered{};
    bool eligible = false;
    bool curveCoherent = true;
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
static std::array<PersistentFilteredValue, halljoy::keycode::kCount> g_persistentFiltered{};
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

static const NativeAnalogReadResult& ReadNativeCached(
    uint16_t hidKeycode, HidCache& cache)
{
    static const NativeAnalogReadResult empty{};
    if (!halljoy::keycode::IsSupported(hidKeycode))
        return empty;
    if (!cache.hasNative.test(hidKeycode))
    {
#if defined(HALLJOY_ANALOG_SIMULATOR)
        if (AnalogSimulator_ReadIsolated(hidKeycode, cache.native[hidKeycode]))
            cache.isolatedSynthetic.set(hidKeycode);
        else
#endif
        cache.native[hidKeycode] =
            NativeAnalogBackends_ReadMilli(hidKeycode);
        cache.hasNative.set(hidKeycode);
    }
    return cache.native[hidKeycode];
}

static float ReadRaw01Cached(uint16_t hidKeycode, HidCache& cache)
{
    if (hidKeycode == 0) return 0.0f;
    if (MouseBind_IsPseudoHid(hidKeycode))
        return ReadMouseBindRaw01(hidKeycode);
    if (!halljoy::keycode::IsSupported(hidKeycode))
        return 0.0f;

    if (cache.hasRaw.test(hidKeycode))
        return cache.raw[hidKeycode];

    const NativeAnalogReadResult& native =
        ReadNativeCached(hidKeycode, cache);
    float providerValue = 0.0f;
    bool providerAvailable = false;
    bool providerOwned = false;

#if defined(HALLJOY_ANALOG_SIMULATOR)
    if (cache.isolatedSynthetic.test(hidKeycode)) {
        cache.raw[hidKeycode] = native.owned
            ? std::clamp(static_cast<float>(native.milli) / 1000.0f, 0.0f, 1.0f)
            : 0.0f;
        cache.hasRaw.set(hidKeycode);
        return cache.raw[hidKeycode];
    }
#endif
    const uint16_t modeCode = cache.wootingReady
        ? HidToModeCode(hidKeycode, cache.mode) : 0;
    // Ordinary HID usages retain max aggregation across multiple physical
    // keyboards. Extended native Fn/OEM codes have no second USB HID source;
    // avoid a serialized SDK call after the DrunkDeer backend owns them.
    if (cache.wootingReady && (modeCode != 0 ||
        cache.hasAuthoritativeProviderV2) &&
        (!native.owned || halljoy::keycode::IsStandardHid(hidKeycode)))
    {
        float sdk = 0.0f;
        providerAvailable = true;
        const bool standard = halljoy::keycode::IsStandardHid(hidKeycode);
        const bool snapshotMode = !cache.hasAuthoritativeProviderV2 &&
            cache.mode == WootingAnalog_KeycodeType_HID &&
            cache.hasFullBuffer &&
            (cache.hasAuthoritativeUapDense || kPreferFullBufferSnapshot);
        if (cache.hasAuthoritativeProviderV2)
        {
            // The production route consumes the immutable, variable-capacity
            // V2 broker snapshot.  Missing ownership is a released/unbound
            // source, not permission to fall back to dense compatibility IPC.
            if (cache.providerV2Raw.owned.test(hidKeycode))
            {
                sdk = cache.providerV2Raw.values[hidKeycode];
                providerOwned = true;
            }
        }
        else if (snapshotMode)
        {
            if (standard && cache.hasFullBuffer &&
                cache.fullPresent.test(hidKeycode))
            {
                sdk = cache.fullRaw[hidKeycode];
                providerOwned = true;
            }
        }
        else
        {
            sdk = ReadAnalogByCodeWithDeviceFallback(modeCode, hidKeycode);
            if (sdk < 0.0f)
            {
                providerAvailable = false;
                const int error = static_cast<int>(std::lround(sdk));
                const ULONGLONG now = GetTickCount64();
                const int previous =
                    g_lastAnalogErrorCode.load(std::memory_order_relaxed);
                const ULONGLONG previousMs =
                    g_lastAnalogErrorLogMs.load(std::memory_order_relaxed);
                if (error != previous || now - previousMs >= 5000)
                {
                    DebugLog_Write(
                        L"[backend.analog] read_analog key_code=%u mode_code=%u mode=%s err=%d",
                        static_cast<unsigned>(hidKeycode),
                        static_cast<unsigned>(modeCode),
                        KeycodeModeName(static_cast<int>(cache.mode)), error);
                    g_lastAnalogErrorCode.store(error,
                        std::memory_order_relaxed);
                    g_lastAnalogErrorLogMs.store(now,
                        std::memory_order_relaxed);
                }
                sdk = 0.0f;
            }
            else
            {
                // The legacy SDK does not expose per-key ownership. A successful
                // read is its historical available/owned-zero equivalence.
                providerOwned = true;
            }

            if (standard && kEnableFullBufferAssist &&
                cache.hasFullBuffer &&
                cache.mode == WootingAnalog_KeycodeType_HID &&
                cache.fullPresent.test(hidKeycode))
            {
                float snapshot = cache.fullRaw[hidKeycode];
                if (std::isfinite(snapshot))
                {
                    snapshot = Clamp01(snapshot);
                    if (snapshot > sdk + 0.02f ||
                        (sdk <= 0.001f && snapshot >= 0.01f))
                        sdk = snapshot;
                }
            }
        }
        if (!std::isfinite(sdk)) sdk = 0.0f;
        providerValue = Clamp01(sdk);
    }

    const auto nativeSource = halljoy::provider_v2_shadow::AnalogSourceStateV1{
        native.connected, native.owned, native.owned,
        static_cast<float>(native.milli) / 1000.0f };
    const auto providerSource = halljoy::provider_v2_shadow::AnalogSourceStateV1{
        providerAvailable, providerOwned, providerAvailable, providerValue };
    auto arbitration = halljoy::provider_v2_shadow::Arbitrate({ hidKeycode,
        nativeSource, providerSource, {}, false });
    float v = arbitration.value;

    // Extended Fn/OEM keys never have a Windows digital fallback. Native
    // matrix ownership therefore preserves their travel from the first sample.
    if (halljoy::keycode::IsStandardHid(hidKeycode) &&
        cache.allowFallback && !native.owned && v <= 0.001f)
    {
        const float sim = ReadDigitalFallback01(hidKeycode);
        arbitration = halljoy::provider_v2_shadow::Arbitrate({ hidKeycode,
            nativeSource, providerSource,
            { true, true, true, sim }, true });
        if ((arbitration.sourceMask &
                halljoy::provider_v2_shadow::ArbitrationSource_DigitalFallback) != 0 &&
            arbitration.value > v)
        {
            v = arbitration.value;
            if (v >= 0.05f)
            g_digitalFallbackWarnPending.store(true, std::memory_order_release);
        }
    }

    cache.raw[hidKeycode] = v;
    cache.hasRaw.set(hidKeycode);
    return v;
}

static float ReadFiltered01Cached(uint16_t hidKeycode, HidCache& cache)
{
    if (hidKeycode == 0) return 0.0f;
    if (MouseBind_IsPseudoHid(hidKeycode))
        return ReadRaw01Cached(hidKeycode, cache);

    if (halljoy::keycode::IsSupported(hidKeycode))
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

    return 0.0f;
}

static float ReadProviderV2ShadowRaw01Cached(uint16_t hidKeycode,
    HidCache& qualifiedCache, ProviderV2ShadowTick& shadow)
{
    if (!halljoy::keycode::IsSupported(hidKeycode) ||
        MouseBind_IsPseudoHid(hidKeycode))
        return 0.0f;
    if (shadow.hasRaw.test(hidKeycode))
        return shadow.raw[hidKeycode];

    const NativeAnalogReadResult& native =
        ReadNativeCached(hidKeycode, qualifiedCache);
#if defined(HALLJOY_ANALOG_SIMULATOR)
    if (qualifiedCache.isolatedSynthetic.test(hidKeycode)) {
        shadow.raw[hidKeycode] = static_cast<float>(native.milli) / 1000.0f;
        shadow.hasRaw.set(hidKeycode);
        return shadow.raw[hidKeycode];
    }
#endif
    // The shadow deliberately remains on the legacy compatibility plane while
    // qualified input uses Provider V2. Standard keys come from the one dense
    // parent capture; extended keys retain the legacy per-key reader because
    // the historical dense table has no extended-key slots.
    const bool standard = halljoy::keycode::IsStandardHid(hidKeycode);
    bool compatibilityOwned = standard &&
        qualifiedCache.hasAuthoritativeUapDense &&
        qualifiedCache.fullPresent.test(hidKeycode);
    float compatibilityValue = compatibilityOwned
        ? qualifiedCache.fullRaw[hidKeycode] : 0.0f;
    if (!standard)
    {
        const uint16_t modeCode = HidToModeCode(hidKeycode,
            qualifiedCache.mode);
        if (modeCode != 0)
        {
            compatibilityValue = ReadAnalogByCodeWithDeviceFallback(modeCode,
                hidKeycode);
            compatibilityOwned = compatibilityValue >= 0.0f;
            if (!compatibilityOwned)
                compatibilityValue = 0.0f;
        }
    }
    // Match the ordinary ownership policy: standard USB HID keys aggregate
    // across native and UAP sources; native extended keys remain authoritative.
    const float value = halljoy::provider_v2_shadow::MergeWithNative(
        hidKeycode, native.owned,
        static_cast<float>(native.milli) / 1000.0f,
        compatibilityOwned, compatibilityValue);
    shadow.raw[hidKeycode] = value;
    shadow.hasRaw.set(hidKeycode);
    return value;
}

static void ReadFilteredPair(uint16_t hidKeycode, HidCache& qualifiedCache,
    ProviderV2ShadowTick& shadow, float* qualified, float* shadowValue)
{
    if (!qualified || !shadowValue ||
        !halljoy::keycode::IsSupported(hidKeycode))
        return;

    const float qualifiedFiltered =
        ReadFiltered01Cached(hidKeycode, qualifiedCache);
    *qualified = qualifiedFiltered;

    // Mouse pseudo-bindings are sampled once through the qualified cache and
    // copied. Provider V2 USB usages in this reserved UI range must not alias
    // HallJoy's mouse controls.
    if (!shadow.eligible || MouseBind_IsPseudoHid(hidKeycode))
    {
        *shadowValue = qualifiedFiltered;
        return;
    }
    if (shadow.hasFiltered.test(hidKeycode))
    {
        *shadowValue = shadow.filtered[hidKeycode];
        return;
    }

    const float qualifiedRaw = ReadRaw01Cached(hidKeycode, qualifiedCache);
    const float providerV2Raw = ReadProviderV2ShadowRaw01Cached(
        hidKeycode, qualifiedCache, shadow);
    float expectedQualified = qualifiedFiltered;
    float providerV2Filtered = qualifiedFiltered;
    if (qualifiedRaw != providerV2Raw)
    {
        BackendCurve_ApplyPairByHid(hidKeycode, qualifiedRaw, providerV2Raw,
            &expectedQualified, &providerV2Filtered);
        if (std::fabs(expectedQualified - qualifiedFiltered) > 0.000001f)
            shadow.curveCoherent = false;
    }
    shadow.filtered[hidKeycode] = providerV2Filtered;
    shadow.hasFiltered.set(hidKeycode);
    *shadowValue = providerV2Filtered;
}

// Separate explicit states ensure that evaluating V2 can never change SOCD or
// Last Key Priority behavior sent to the game. On an ineligible tick the shadow
// state is advanced with qualified input, keeping its history synchronized
// without treating unavailable V2 data as proof.
static std::array<halljoy::configured_xusb::BuilderState,
    kMaxVirtualPads> g_qualifiedReportBuilderState{};
static std::array<halljoy::configured_xusb::BuilderState,
    kMaxVirtualPads> g_providerV2ShadowReportBuilderState{};

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

struct PadFramePair
{
    halljoy::controller::VirtualControllerFrameV1 qualified{};
    halljoy::controller::VirtualControllerFrameV1 shadow{};
    std::uint32_t providerConfiguredFieldMask = 0;
    std::uint32_t providerDeepTravelFieldMask = 0;
    std::uint32_t providerNonNeutralFieldMask = 0;
    std::uint32_t providerActiveFieldMask = 0;
    std::uint32_t providerNeutralFieldMask = 0;
    std::uint32_t mismatchMask = 0;
};

static PadFramePair BuildReportFramesForPad(int padIndex, HidCache& cache,
    ProviderV2ShadowTick& shadowTick, bool snappyJoystick,
    bool lastKeyPriority, float lastKeyPrioritySensitivity,
    bool mouseToStickEnabled, std::uint8_t mouseTarget)
{
    using namespace halljoy::configured_xusb;
    const int boundedPad = std::clamp(padIndex, 0, kMaxVirtualPads - 1);
    PadConfiguration configuration{};
    InputValues qualifiedInput{};
    InputValues shadowInput{};
    PadFramePair pair{};
    const auto recordProviderKey = [&](std::uint16_t hid,
        std::uint32_t fieldMask) {
        if (!halljoy::keycode::IsSupported(hid) ||
            MouseBind_IsPseudoHid(hid) ||
            !shadowTick.providerRaw.owned.test(hid))
        {
            return;
        }
        pair.providerConfiguredFieldMask |= fieldMask;
        const float value = shadowTick.providerRaw.values[hid];
        if (value > 0.001f)
            pair.providerNonNeutralFieldMask |= fieldMask;
        if (value >= 0.5f)
            pair.providerDeepTravelFieldMask |= fieldMask;
    };
    const auto readIntoInput = [&](std::uint16_t hid) {
        if (halljoy::keycode::IsSupported(hid))
        {
            ReadFilteredPair(hid, cache, shadowTick,
                &qualifiedInput.filtered[hid], &shadowInput.filtered[hid]);
        }
    };

    for (std::size_t axis = 0; axis < kAxisCount; ++axis)
    {
        configuration.axes[axis] = Bindings_GetAxisForPad(
            boundedPad, static_cast<Axis>(axis));
        readIntoInput(configuration.axes[axis].minusHid);
        readIntoInput(configuration.axes[axis].plusHid);
    }
    for (std::size_t trigger = 0; trigger < kTriggerCount; ++trigger)
    {
        configuration.triggers[trigger] = Bindings_GetTriggerForPad(
            boundedPad, static_cast<::Trigger>(trigger));
        readIntoInput(configuration.triggers[trigger]);
    }
    for (std::size_t button = 0; button < kButtonCount; ++button)
    {
        for (std::size_t chunk = 0; chunk < halljoy::keycode::kMaskChunkCount;
            ++chunk)
        {
            const std::uint64_t mask = Bindings_GetButtonMaskChunkForPad(
                boundedPad, static_cast<GameButton>(button),
                static_cast<int>(chunk));
            configuration.buttonMasks[button][chunk] = mask;
            std::uint64_t remaining = mask;
            while (remaining != 0)
            {
                unsigned long bit = 0;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
                _BitScanForward64(&bit, remaining);
#else
                while ((remaining & (std::uint64_t{ 1 } << bit)) == 0)
                    ++bit;
#endif
                remaining &= remaining - 1u;
                const std::size_t hid = chunk * 64u + bit;
                if (hid < halljoy::keycode::kCount)
                    readIntoInput(static_cast<std::uint16_t>(hid));
            }
        }
    }
    configuration.snappyJoystick = snappyJoystick;
    configuration.lastKeyPriority = lastKeyPriority;
    configuration.lastKeyPrioritySensitivity = lastKeyPrioritySensitivity;

    for (std::size_t axis = 0; axis < kAxisCount; ++axis)
    {
        const auto& binding = configuration.axes[axis];
        const std::uint32_t fieldMask = 1u << (3u + axis);
        recordProviderKey(binding.minusHid, fieldMask);
        recordProviderKey(binding.plusHid, fieldMask);
    }
    for (std::size_t trigger = 0; trigger < kTriggerCount; ++trigger)
    {
        recordProviderKey(configuration.triggers[trigger],
            1u << (1u + trigger));
    }
    for (const auto& buttonMask : configuration.buttonMasks)
    {
        for (std::size_t chunk = 0; chunk < buttonMask.size(); ++chunk)
        {
            std::uint64_t remaining = buttonMask[chunk];
            while (remaining != 0)
            {
                unsigned long bit = 0;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
                _BitScanForward64(&bit, remaining);
#else
                while ((remaining & (std::uint64_t{ 1 } << bit)) == 0)
                    ++bit;
#endif
                remaining &= remaining - 1u;
                const std::size_t hid = chunk * 64u + bit;
                if (hid < halljoy::keycode::kCount)
                {
                    recordProviderKey(static_cast<std::uint16_t>(hid),
                        1u << 0);
                }
            }
        }
    }

    if (boundedPad == 0 && mouseToStickEnabled)
    {
        qualifiedInput.mouseEnabled = ReadMouseStickSample(
            qualifiedInput.mouseX, qualifiedInput.mouseY);
        qualifiedInput.mouseTarget = mouseTarget;
        shadowInput.mouseEnabled = qualifiedInput.mouseEnabled;
        shadowInput.mouseTarget = qualifiedInput.mouseTarget;
        shadowInput.mouseX = qualifiedInput.mouseX;
        shadowInput.mouseY = qualifiedInput.mouseY;
    }

    const std::size_t stateIndex = static_cast<std::size_t>(boundedPad);
    if (!shadowTick.eligible)
    {
        g_providerV2ShadowReportBuilderState[stateIndex] =
            g_qualifiedReportBuilderState[stateIndex];
    }
    pair.qualified = halljoy::configured_xusb::BuildReport(configuration,
        qualifiedInput, g_qualifiedReportBuilderState[stateIndex]);
    pair.shadow = halljoy::configured_xusb::BuildReport(configuration,
        shadowInput, g_providerV2ShadowReportBuilderState[stateIndex]);
    if (shadowTick.eligible)
    {
        const std::uint32_t shadowActive =
            halljoy::provider_v2_qualification::ActiveFieldMask(pair.shadow);
        pair.providerActiveFieldMask =
            pair.providerDeepTravelFieldMask & shadowActive;
        pair.providerNeutralFieldMask = pair.providerConfiguredFieldMask &
            ~pair.providerNonNeutralFieldMask & ~shadowActive;
        pair.mismatchMask = halljoy::provider_v2_shadow::CompareFrames(
            pair.qualified, pair.shadow);
    }
    return pair;
}

static XUSB_REPORT ToLegacyXusbReport(
    const halljoy::controller::VirtualControllerFrameV1& frame)
{
    const auto built = halljoy::xusb_output::ToReport(frame);
    XUSB_REPORT report{};
    report.wButtons = built.buttons;
    report.bLeftTrigger = built.leftTrigger;
    report.bRightTrigger = built.rightTrigger;
    report.sThumbLX = built.thumbLX;
    report.sThumbLY = built.thumbLY;
    report.sThumbRX = built.thumbRX;
    report.sThumbRY = built.thumbRY;
    return report;
}

static void RecordProviderV2ShadowComparison(
    const std::array<std::uint32_t, kMaxVirtualPads>& mismatchMasks,
    const std::array<std::uint32_t, kMaxVirtualPads>&
        providerConfiguredFieldMasks,
    const std::array<std::uint32_t, kMaxVirtualPads>&
        providerActiveFieldMasks,
    const std::array<std::uint32_t, kMaxVirtualPads>&
        providerNeutralFieldMasks,
    int padCount, std::uint64_t sampleGeneration, std::uint64_t nowMs)
{
    g_providerV2ShadowEligibleTicks.fetch_add(1, std::memory_order_relaxed);
    std::uint64_t firstTick = 0;
    (void)g_providerV2ShadowFirstEligibleTickMs.compare_exchange_strong(
        firstTick, nowMs, std::memory_order_relaxed);
    g_providerV2ShadowLastEligibleTickMs.store(nowMs, std::memory_order_relaxed);
    const std::uint64_t previousSample =
        g_providerV2ShadowLastSampleGeneration.exchange(sampleGeneration,
            std::memory_order_relaxed);
    if (previousSample != sampleGeneration)
    {
        g_providerV2ShadowUniqueSampleGenerations.fetch_add(1,
            std::memory_order_relaxed);
    }
    for (int pad = 0; pad < padCount; ++pad)
    {
        const std::size_t index = static_cast<std::size_t>(pad);
        const std::uint32_t configuredFields =
            providerConfiguredFieldMasks[index];
        const std::uint32_t activeFields = providerActiveFieldMasks[index];
        const auto releaseUpdate =
            halljoy::provider_v2_qualification::UpdateReleaseTracker(
                g_providerV2ShadowPendingReleaseMasks[index], activeFields,
                providerNeutralFieldMasks[index]);
        g_providerV2ShadowPendingReleaseMasks[index] =
            releaseUpdate.pendingMask;
        const std::uint32_t releasedFields = releaseUpdate.releasedNowMask;
        constexpr std::uint32_t fieldsPerPad = static_cast<std::uint32_t>(
            halljoy::provider_v2_qualification::kFieldCount);
        const std::uint32_t coverageShift =
            static_cast<std::uint32_t>(index) * fieldsPerPad;
        const std::uint32_t configuredCoverage =
            configuredFields << coverageShift;
        const std::uint32_t activeCoverage = activeFields << coverageShift;
        const std::uint32_t releasedCoverage = releasedFields << coverageShift;
        g_providerV2ShadowConfiguredFieldMask.fetch_or(configuredCoverage,
            std::memory_order_relaxed);
        g_providerV2ShadowActivatedFieldMask.fetch_or(activeCoverage,
            std::memory_order_relaxed);
        g_providerV2ShadowReleasedFieldMask.fetch_or(releasedCoverage,
            std::memory_order_relaxed);

        const std::uint32_t mismatch = mismatchMasks[index];
        if (mismatch == 0)
        {
            g_providerV2ShadowMatchedReports.fetch_add(1,
                std::memory_order_relaxed);
            continue;
        }

        g_providerV2ShadowMismatchedReports.fetch_add(1,
            std::memory_order_relaxed);
        g_providerV2ShadowLastMismatchMask.store(mismatch,
            std::memory_order_relaxed);
        g_providerV2ShadowLastMismatchPad.store(
            static_cast<std::uint32_t>(pad), std::memory_order_relaxed);
        for (std::size_t field = 0;
            field < g_providerV2ShadowFieldMismatches.size(); ++field)
        {
            if ((mismatch & (1u << field)) != 0)
            {
                g_providerV2ShadowFieldMismatches[field].fetch_add(1,
                    std::memory_order_relaxed);
            }
        }
    }
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

static uint64_t BackendQpcTimestampUs(LONGLONG now)
{
    if (now <= 0)
        return 0;
    const uint64_t ticks = static_cast<uint64_t>(now);
    const uint64_t frequency = static_cast<uint64_t>(BackendQpcFrequency());
    return (ticks / frequency) * 1000000ull +
        ((ticks % frequency) * 1000000ull) / frequency;
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
    // File-only profile tests must fail closed if a future startup refactor
    // reaches the backend.  Keep this check before output stop, HID discovery,
    // native worker lifecycle, or any virtual-controller activity.
    const wchar_t* const commandLine = GetCommandLineW();
    if (commandLine && wcsstr(commandLine, L"--halljoy-test-forbid-backend-init"))
    {
#if defined(HALLJOY_ANALOG_SIMULATOR)
        g_fileOnlyTestForbiddenInitAttempts.fetch_add(1, std::memory_order_relaxed);
#endif
        g_lastInitIssues.store(BackendInitIssue_Unknown, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"backend", L"test.forbidden_backend_init");
        DebugLog_Write(L"[backend.init] denied by file-only test guard");
        return false;
    }
    StabilityTrace_Write(L"INFO", L"backend", L"init.begin");
    DebugLog_Write(L"[backend.init] begin");
    const bool previousOutputStopped = VigemOutput_Stop();
    if (!previousOutputStopped)
    {
        g_lastInitIssues.store(BackendInitIssue_Unknown, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"backend", L"init.failed",
            L"stage=previous_vigem_output_stop");
        return false;
    }
    ResetPersistentFilteredCache();
    g_qualifiedReportBuilderState = {};
    g_providerV2ShadowReportBuilderState = {};
    g_providerV2ShadowAvailable.store(false, std::memory_order_relaxed);
    g_providerV2ShadowBackendInitCount.fetch_add(1,
        std::memory_order_relaxed);
    g_providerV2ShadowPendingReleaseMasks.fill(0);
    g_providerV2ShadowLastSampleGeneration.store(0,
        std::memory_order_relaxed);
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
    g_vigemResubmitRequested.store(false, std::memory_order_release);
    g_vigemObservedGeneration.store(0u, std::memory_order_release);
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

#if defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC) || defined(HALLJOY_ROG_AZOTH96HE_DIAGNOSTIC)
    // These images are bounded HID transport probes, not HallJoy gameplay.
    // Starting the UAP host or ViGEm can fail independently and must never
    // prevent the documented diagnostic transaction from reaching the keyboard.
    StabilityTrace_Write(L"INFO", L"backend", L"diagnostic.transport_only",
        L"native_ready=%d uap=disabled vigem=disabled", nativeReady ? 1 : 0);
    DebugLog_Write(L"[backend.init] transport-only diagnostic; UAP and ViGEm disabled");
    return preUapReady;
#endif

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

    if (initIssues == BackendInitIssue_None && !VigemOutput_Start())
    {
        const VIGEM_ERROR error = g_vigemLastErr.load(std::memory_order_acquire);
        initIssues |= error == VIGEM_ERROR_BUS_NOT_FOUND
            ? BackendInitIssue_VigemBusMissing
            : BackendInitIssue_Unknown;
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
        if (!VigemOutput_Stop())
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

#if defined(HALLJOY_ANALOG_SIMULATOR)
uint32_t Backend_FileOnlyTestForbiddenInitAttempts() noexcept
{
    return g_fileOnlyTestForbiddenInitAttempts.load(std::memory_order_acquire);
}
#endif

void Backend_SetRuntimeAdmission(bool admitted) noexcept
{
    g_runtimeAdmission.store(admitted, std::memory_order_release);
}

bool Backend_IsRuntimeAdmissionOpen() noexcept
{
    return g_runtimeAdmission.load(std::memory_order_acquire);
}

bool Backend_Shutdown()
{
    StabilityTrace_Write(L"INFO", L"backend", L"shutdown.begin");
    DebugLog_Write(L"[backend] shutdown");
#if defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC) || defined(HALLJOY_ROG_AZOTH96HE_DIAGNOSTIC)
    const bool nativeStopped = NativeAnalogBackends_StopPhase(NativeAnalogStartPhase::BeforeUap);
    StabilityTrace_Write(nativeStopped ? L"INFO" : L"ERROR", L"backend", L"shutdown.end",
        L"native_joined=%d uap=disabled vigem=disabled", nativeStopped ? 1 : 0);
    return nativeStopped;
#else
    if (!VigemOutput_Stop())
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
    g_keycodeModeLocked.store(false, std::memory_order_relaxed);
    g_vigemResubmitRequested.store(false, std::memory_order_release);
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
#endif
}


void Backend_ResetPublishedStateAfterRealtimeFault() noexcept
{
#if defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC) || defined(HALLJOY_ROG_AZOTH96HE_DIAGNOSTIC)
    // The diagnostic is deliberately transport-only.  In particular, a
    // realtime fault must not cause its recovery path to create a ViGEm
    // generation after Backend_Init deliberately skipped that subsystem.
    return;
#endif
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

    // The faulting realtime worker never enters the driver. It publishes one
    // complete neutral snapshot through the same bounded process channel.
    std::array<halljoy::vigem_output::XusbReportV1, kMaxVirtualPads> output{};
    const std::uint32_t padCount = static_cast<std::uint32_t>(std::clamp(
        g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads));
    const std::uint32_t validMask = (1u << padCount) - 1u;
    (void)g_vigemOutputRuntime.PublishProducerProgress(GetTickCount64());
    const auto published = g_vigemOutputRuntime.TryPublish(output.data(),
        padCount, validMask, GetTickCount64() * 1000u, nullptr);
    g_vigemResubmitRequested.store(
        published != halljoy::vigem_output::OutputPublishResult::Published,
        std::memory_order_release);
    g_vigemOk.store(false, std::memory_order_release);
}

void Backend_Tick()
{
    if (!g_runtimeAdmission.load(std::memory_order_acquire))
        return;
    halljoy::profile_runtime::ReadLease profileLease;
    if (!profileLease) return; // No partial profile is allowed into an output report.
#if defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC)
    // No normal input/output processing belongs in a transport-only HID probe.
    // Its dedicated backend owns the bounded control/stream exchange.
    return;
#endif
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
    const std::uint64_t tickCurveGeneration = BackendCurve_GetGeneration();
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

    // One parent transaction captures the complete legacy dense plane and its
    // optional same-generation Provider V2 view. Standard HID reads below use
    // this immutable dense table for the whole tick. If capture is unavailable,
    // the qualified legacy per-key path remains the fail-safe fallback.
    static halljoy::uap_parent_snapshot::SnapshotV1 s_uapTickSnapshot{};
    const bool uapTickCaptured = cache.wootingReady &&
        cache.mode == WootingAnalog_KeycodeType_HID &&
        WootingSafe_CaptureTickSnapshot(&s_uapTickSnapshot);
    if (uapTickCaptured)
    {
        cache.fullRaw = s_uapTickSnapshot.denseValues;
        cache.fullPresent.set();
        cache.hasFullBuffer = true;
        cache.hasAuthoritativeUapDense = true;
        cache.uapTickSnapshot = &s_uapTickSnapshot;
        g_tmFullBufferRet.store(
            static_cast<int>(s_uapTickSnapshot.denseActiveKeyCount),
            std::memory_order_relaxed);
        std::uint16_t maximumMilli = 0;
        for (const float value : s_uapTickSnapshot.denseValues)
        {
            maximumMilli = (std::max)(maximumMilli,
                static_cast<std::uint16_t>(std::clamp(
                    static_cast<int>(std::lround(value * 1000.0f)), 0, 1000)));
        }
        g_tmFullBufferMaxMilli.store(maximumMilli, std::memory_order_relaxed);
    }

    ProviderV2ShadowTick providerV2Shadow{};
    bool providerV2Projected = false;
    std::uint64_t providerV2SampleGeneration = 0;
    auto providerV2Lease = AnalogHostClient_AcquireProviderV2Snapshot();
    const bool providerV2LeaseMatchesDense = uapTickCaptured &&
        providerV2Lease &&
        halljoy::provider_v2_shadow::MatchesCapturedPublication(
            providerV2Lease.Metadata(), s_uapTickSnapshot);
    if (providerV2LeaseMatchesDense)
    {
        providerV2Projected =
            halljoy::provider_v2_shadow::ProjectCapturedSnapshot(
                providerV2Lease.Header(), providerV2Lease.Devices(),
                providerV2Lease.DeviceCount(), providerV2Lease.Samples(),
                providerV2Lease.SampleCount(),
                &providerV2Shadow.providerRaw) ==
            halljoy::provider_v2_shadow::ProjectionError::None;
        providerV2SampleGeneration =
            providerV2Lease.Header().sampleGeneration;
        if (providerV2Projected)
        {
            cache.providerV2Raw = providerV2Shadow.providerRaw;
            cache.hasAuthoritativeProviderV2 = true;
        }
    }
    if (uapTickCaptured && !providerV2Projected)
    {
        g_providerV2ShadowUnavailableTicks.fetch_add(1,
            std::memory_order_relaxed);
    }
    g_providerV2ShadowAvailable.store(providerV2Projected,
        std::memory_order_relaxed);
    if (providerV2Projected && cache.allowFallback)
    {
        // A Windows key edge is neither Provider V2 input nor an analogue
        // ownership signal. Never let it qualify or train the shadow route.
        g_providerV2ShadowDigitalFallbackTicks.fetch_add(1,
            std::memory_order_relaxed);
    }
    providerV2Shadow.eligible = providerV2Projected && !cache.allowFallback;

    // Build the raw map from the isolated host's shared-memory snapshot. V9
    // reads it on every realtime tick; no blocking device I/O occurs here.
    if ((kEnableFullBufferAssist || kPreferFullBufferSnapshot) &&
        cache.wootingReady && !cache.hasAuthoritativeUapDense)
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

    // One immutable, deduplicated subscription for both independent consumers.
    const auto tracked = g_trackingSnapshot.load(std::memory_order_acquire);
    const int cnt = tracked ? tracked->count : 0;
    uint16_t maxRawM = 0;
    uint16_t maxOutM = 0;
    uint16_t maxRawHid = 0;
    uint16_t maxOutHid = 0;

    const auto inputEvidenceTime = GetTickCount64();
    // UI snapshot update
    for (int i = 0; i < cnt; ++i)
    {
        uint16_t hid = tracked->keys[i];
        if (!halljoy::keycode::IsSupported(hid)) continue;

        float raw = ReadRaw01Cached(hid, cache);
        float filtered = ReadFiltered01Cached(hid, cache);

        int rawM = (int)std::lround(raw * 1000.0f);
        rawM = std::clamp(rawM, 0, 1000);
        g_uiRawM[hid].store((uint16_t)rawM, std::memory_order_relaxed);
        halljoy::input_privilege::analogPresses.Observe(hid, static_cast<unsigned>(rawM), inputEvidenceTime);
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

    // Bind capture includes the two stable Soup/UAP extended layer-key codes.
    if (g_bindCaptureEnabled.load(std::memory_order_acquire))
    {
        uint16_t bestHid = 0;
        int bestRawM = 0;
        const auto consider = [&](uint16_t hid) {
            const float raw = ReadRaw01Cached(hid, cache);
            const int rawM = static_cast<int>(std::lround(raw * 1000.0f));
            if (rawM > bestRawM)
            {
                bestRawM = rawM;
                bestHid = hid;
            }
        };
        for (uint16_t hid = 1; hid < 256; ++hid)
            consider(hid);
        consider(halljoy::keycode::kOem1);
        consider(halljoy::keycode::kFn);

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

    const bool snappyJoystick = Settings_GetSnappyJoystick();
    const bool lastKeyPriority = Settings_GetLastKeyPriority();
    const float lastKeyPrioritySensitivity =
        Settings_GetLastKeyPrioritySensitivity();
    const bool mouseToStickEnabled = Settings_GetMouseToStickEnabled();
    const std::uint8_t mouseTarget =
        Settings_GetMouseToStickTarget() == 0 ? 0u : 1u;
    std::array<std::uint32_t, kMaxVirtualPads> shadowMismatchMasks{};
    std::array<std::uint32_t, kMaxVirtualPads>
        shadowProviderConfiguredFieldMasks{};
    std::array<std::uint32_t, kMaxVirtualPads>
        shadowProviderActiveFieldMasks{};
    std::array<std::uint32_t, kMaxVirtualPads>
        shadowProviderNeutralFieldMasks{};

    int logicalPads = std::clamp(g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads);
    for (int pad = 0; pad < logicalPads; ++pad)
    {
        const PadFramePair frames = BuildReportFramesForPad(pad, cache,
            providerV2Shadow, snappyJoystick, lastKeyPriority,
            lastKeyPrioritySensitivity, mouseToStickEnabled, mouseTarget);
        shadowMismatchMasks[static_cast<std::size_t>(pad)] =
            frames.mismatchMask;
        shadowProviderConfiguredFieldMasks[static_cast<std::size_t>(pad)] =
            frames.providerConfiguredFieldMask;
        shadowProviderActiveFieldMasks[static_cast<std::size_t>(pad)] =
            frames.providerActiveFieldMask;
        shadowProviderNeutralFieldMasks[static_cast<std::size_t>(pad)] =
            frames.providerNeutralFieldMask;
        const XUSB_REPORT report = ToLegacyXusbReport(frames.qualified);
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

    if (providerV2Shadow.eligible)
    {
        const bool curveGenerationStable =
            tickCurveGeneration == BackendCurve_GetGeneration();
        if (curveGenerationStable && providerV2Shadow.curveCoherent)
        {
            RecordProviderV2ShadowComparison(shadowMismatchMasks,
                shadowProviderConfiguredFieldMasks,
                shadowProviderActiveFieldMasks,
                shadowProviderNeutralFieldMasks, logicalPads,
                providerV2SampleGeneration, nowMs);
        }
        else
        {
            g_providerV2ShadowCurveMutationTicks.fetch_add(1,
                std::memory_order_relaxed);
            g_providerV2ShadowReportBuilderState =
                g_qualifiedReportBuilderState;
        }
    }
    else
    {
        g_providerV2ShadowReportBuilderState = g_qualifiedReportBuilderState;
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
        const std::uint32_t outputCount = static_cast<std::uint32_t>(std::clamp(
            g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads));
        std::array<halljoy::vigem_output::XusbReportV1, kMaxVirtualPads>
            outputReports{};
        std::array<bool, kMaxVirtualPads> changedPads{};
        const LONGLONG publishQpc = BackendQpcNow();
        bool publishDue = false;
        bool hasChangedCandidate = false;
        for (std::uint32_t i = 0; i < outputCount; ++i)
        {
            const size_t index = static_cast<size_t>(i);
            const XUSB_REPORT& report = g_reports[index];
            outputReports[index] = ToOutputReport(report);
            const bool valid = g_lastSentValid[index] != 0;
            const bool changed = !valid ||
                IsReportDifferent(report, g_lastSentReports[index]);
            changedPads[index] = changed;
            hasChangedCandidate = hasChangedCandidate || changed;
            BackendTracePadWindow* tracePad =
                latencyTrace ? &g_backendTracePads[index] : nullptr;
            if (tracePad)
            {
                ++tracePad->decisions;
                if (changed) ++tracePad->changedCandidates;
                else ++tracePad->unchangedCandidates;
            }

            auto& scheduler = g_outputSchedulers[index];
            // The scheduler remains in realtime, but the complete snapshot and
            // every ViGEm call are owned by the isolated output process.
            const auto decision = scheduler.Evaluate(
                changed, static_cast<uint64_t>(publishQpc));
            if (!changed)
            {
                if (tracePad) ++tracePad->unchangedSkipped;
                continue;
            }
            if (decision == VigemOutputScheduler::Decision::DeferUntilDeadline)
            {
                if (tracePad) ++tracePad->rateLimited;
                if (mad68BatchPresent) mad68RateLimited = true;
                continue;
            }
            publishDue = true;
        }

        // A calculation that produces the same held XUSB value is still proof
        // that the realtime producer is alive. Keep it independent from the
        // deduplicated snapshot path and its bounded slot contention.
        (void)g_vigemOutputRuntime.PublishProducerProgress(nowMs);

        if (publishDue)
        {
            // A newest-value slot may replace an older ready slot. Marking the
            // complete configured set valid guarantees that no due pad is lost
            // during that coalescing.
            const std::uint32_t validMask = (1u << outputCount) - 1u;
            std::uint64_t publicationSequence = 0u;
            const auto publishResult = g_vigemOutputRuntime.TryPublish(
                outputReports.data(), outputCount, validMask,
                BackendQpcTimestampUs(publishQpc), &publicationSequence);
            if (publishResult ==
                halljoy::vigem_output::OutputPublishResult::Published)
            {
                const uint64_t inputSequence = latencyTrace
                    ? RealtimeLoop_GetInputNotifySequence()
                    : 0;
                const LONGLONG inputNotifyQpc = latencyTrace
                    ? RealtimeLoop_GetLastInputNotifyQpc()
                    : 0;
                for (std::uint32_t i = 0; i < outputCount; ++i)
                {
                    const size_t index = static_cast<size_t>(i);
                    const bool valid = g_lastSentValid[index] != 0;
                    const uint64_t intervalUs = valid
                        ? BackendQpcElapsedUs(g_lastSentQpc[index], publishQpc)
                        : 0;
                    const uint64_t signalToPublishUs =
                        latencyTrace && inputNotifyQpc > 0 && changedPads[index]
                        ? BackendQpcElapsedUs(inputNotifyQpc, publishQpc)
                        : 0;
                    BackendTracePadWindow* tracePad =
                        latencyTrace ? &g_backendTracePads[index] : nullptr;
                    if (tracePad)
                    {
                        ++tracePad->sends;
                        if (changedPads[index])
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
                    g_lastSentReports[index] = g_reports[index];
                    g_lastSentQpc[index] = publishQpc;
                    g_lastSentValid[index] = 1;
                    g_outputSchedulers[index].MarkSent(
                        static_cast<uint64_t>(publishQpc));
                }

                if (latencyTrace && mad68BatchPresent)
                {
                    ++g_mad68PipelineTrace.changedSends;
                    if (mad68ReportReadyQpc > 0)
                        g_mad68PipelineTrace.reportReadyToSendUs.Add(
                            BackendQpcElapsedUs(
                                mad68ReportReadyQpc, publishQpc));
                    mad68SendRecorded = true;
                }
            }
            else
            {
                // No state is marked sent until the process channel accepts the
                // complete value. The next realtime pass retries immediately.
                g_vigemResubmitRequested.store(true, std::memory_order_release);
                if (latencyTrace)
                {
                    for (std::uint32_t i = 0; i < outputCount; ++i)
                    {
                        if (changedPads[static_cast<size_t>(i)])
                            ++g_backendTracePads[static_cast<size_t>(i)].failures;
                    }
                }
                if (mad68BatchPresent)
                    mad68RateLimited = true;
            }
        }
        else if (hasChangedCandidate && mad68BatchPresent)
        {
            mad68RateLimited = true;
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

static void SetTrackedSubscription(bool overlay, const uint16_t* hids, int count)
{
    std::bitset<halljoy::keycode::kCount> requested;
    if (hids) for (int i = 0; i < count; ++i)
        if (halljoy::keycode::IsSupported(hids[i])) requested.set(hids[i]);
    std::lock_guard<std::mutex> lock(g_trackingMutex);
    auto& consumer = overlay ? g_overlayTracking : g_mainTracking;
    if (consumer == requested) return;
    const auto combined = requested | (overlay ? g_mainTracking : g_overlayTracking);
    auto snapshot = std::make_shared<TrackedKeys>();
    for (size_t hid = 0; hid < combined.size(); ++hid)
        if (combined.test(hid)) snapshot->keys[snapshot->count++] = static_cast<uint16_t>(hid);
    consumer = requested;
    g_trackingSnapshot.store(std::move(snapshot), std::memory_order_release);
}

void BackendUI_SetTrackedHids(const uint16_t* hids, int count) { SetTrackedSubscription(false, hids, count); }
void BackendUI_SetOverlayTrackedHids(const uint16_t* hids, int count) { SetTrackedSubscription(true, hids, count); }
void BackendUI_ClearTrackedHids()
{
    SetTrackedSubscription(false, nullptr, 0);
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
bool BackendUI_TestIsTracked(uint16_t hid) {
    const auto snapshot = g_trackingSnapshot.load(std::memory_order_acquire);
    return snapshot && std::binary_search(snapshot->keys.begin(), snapshot->keys.begin() + snapshot->count, hid);
}
bool BackendUI_TestTrackedUnion() {
    const auto oldMain = g_mainTracking, oldOverlay = g_overlayTracking;
    const uint16_t main[] = {0x1A, 0x1A}; // W only, duplicated intentionally
    const uint16_t overlay[] = {0x1A, 0x59, 0x58, 0xFFFF}; // keypad 1, Enter
    BackendUI_SetTrackedHids(main, 2); BackendUI_SetOverlayTrackedHids(overlay, 4);
    const auto held = g_trackingSnapshot.load();
    bool ok = held && held->count == 3 && BackendUI_TestIsTracked(0x59) && BackendUI_TestIsTracked(0x58);
    BackendUI_ClearTrackedHids();
    ok &= BackendUI_TestIsTracked(0x59); // main rebuild cannot remove overlay keys
    BackendUI_SetOverlayTrackedHids(nullptr, 0);
    ok &= !BackendUI_TestIsTracked(0x59) && held->count == 3; // old reader remains valid
    BackendUI_SetTrackedHids(main, 2);
    ok &= BackendUI_TestIsTracked(0x1A) && !BackendUI_TestIsTracked(0x59);
    std::vector<uint16_t> restoreMain, restoreOverlay;
    for (size_t i = 0; i < oldMain.size(); ++i) {
        if (oldMain.test(i)) restoreMain.push_back(static_cast<uint16_t>(i));
        if (oldOverlay.test(i)) restoreOverlay.push_back(static_cast<uint16_t>(i));
    }
    BackendUI_SetTrackedHids(restoreMain.data(), (int)restoreMain.size());
    BackendUI_SetOverlayTrackedHids(restoreOverlay.data(), (int)restoreOverlay.size());
    return ok;
}
#endif

uint16_t BackendUI_GetAnalogMilli(uint16_t hid)
{
    if (!halljoy::keycode::IsSupported(hid)) return 0;
    return g_uiAnalogM[hid].load(std::memory_order_relaxed);
}

uint16_t BackendUI_GetRawMilli(uint16_t hid)
{
    if (!halljoy::keycode::IsSupported(hid)) return 0;
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
    if (chunk < 0 || static_cast<std::size_t>(chunk) >=
        halljoy::keycode::kMaskChunkCount) return 0;
    return g_uiDirty[chunk].exchange(0, std::memory_order_acq_rel);
}

BackendStatus Backend_GetStatus()
{
    RefreshVigemOutputStatus(false);
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
        dst.verifiedLayoutToken = native.verifiedLayoutToken;
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
    t.providerV2ShadowAvailable =
        g_providerV2ShadowAvailable.load(std::memory_order_relaxed);
    t.providerV2ShadowEligibleTicks =
        g_providerV2ShadowEligibleTicks.load(std::memory_order_relaxed);
    t.providerV2ShadowMatchedReports =
        g_providerV2ShadowMatchedReports.load(std::memory_order_relaxed);
    t.providerV2ShadowMismatchedReports =
        g_providerV2ShadowMismatchedReports.load(std::memory_order_relaxed);
    t.providerV2ShadowUnavailableTicks =
        g_providerV2ShadowUnavailableTicks.load(std::memory_order_relaxed);
    t.providerV2ShadowDigitalFallbackTicks =
        g_providerV2ShadowDigitalFallbackTicks.load(std::memory_order_relaxed);
    t.providerV2ShadowCurveMutationTicks =
        g_providerV2ShadowCurveMutationTicks.load(std::memory_order_relaxed);
    for (std::size_t field = 0;
        field < g_providerV2ShadowFieldMismatches.size(); ++field)
    {
        t.providerV2ShadowFieldMismatches[field] =
            g_providerV2ShadowFieldMismatches[field].load(
                std::memory_order_relaxed);
    }
    t.providerV2ShadowBackendInitCount =
        g_providerV2ShadowBackendInitCount.load(std::memory_order_relaxed);
    t.providerV2ShadowUniqueSampleGenerations =
        g_providerV2ShadowUniqueSampleGenerations.load(
            std::memory_order_relaxed);
    t.providerV2ShadowFirstEligibleTickMs =
        g_providerV2ShadowFirstEligibleTickMs.load(std::memory_order_relaxed);
    t.providerV2ShadowLastEligibleTickMs =
        g_providerV2ShadowLastEligibleTickMs.load(std::memory_order_relaxed);
    t.providerV2ShadowConfiguredFieldMask =
        g_providerV2ShadowConfiguredFieldMask.load(std::memory_order_relaxed);
    t.providerV2ShadowActivatedFieldMask =
        g_providerV2ShadowActivatedFieldMask.load(std::memory_order_relaxed);
    t.providerV2ShadowReleasedFieldMask =
        g_providerV2ShadowReleasedFieldMask.load(std::memory_order_relaxed);
    t.providerV2ShadowLastMismatchMask =
        g_providerV2ShadowLastMismatchMask.load(std::memory_order_relaxed);
    t.providerV2ShadowLastMismatchPad =
        g_providerV2ShadowLastMismatchPad.load(std::memory_order_relaxed);
    t.providerV2ShadowLastSampleGeneration =
        g_providerV2ShadowLastSampleGeneration.load(std::memory_order_relaxed);
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
    if (!g_runtimeAdmission.load(std::memory_order_acquire))
        return;
    // The UAP child deliberately has no periodic discovery thread.  Forward
    // genuine Windows topology changes to its supervisor; this only queues a
    // bounded replacement and never enumerates HID from the UI thread.
    (void)AnalogHostClient_RequestDeviceRefresh();

    if (!g_virtualPadsEnabled.load(std::memory_order_acquire))
        return;

    // Ignore generic device-change noise while the child reports fresh
    // progress. An unhealthy generation is replaced by its sole owner.
    if (g_vigemOk.load(std::memory_order_acquire))
        return;
    g_vigemOutputRuntime.RequestRestart();
}

void Backend_NotifyKeyboardEvent(
    uint16_t hidHint,
    uint16_t scanCode,
    uint16_t vkCode,
    bool isKeyDown,
    bool isInjected)
{
    if (!g_runtimeAdmission.load(std::memory_order_acquire) || hidHint == 0 || isInjected) return;

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
    if (!g_runtimeAdmission.load(std::memory_order_acquire)) return;
    if (dx != 0)
    {
        int old = g_mouseRawAccumDx.load(std::memory_order_relaxed);
        for (;;)
        {
            const int nxt = halljoy::arithmetic::SaturatingAddInt(old, dx, -32768, 32767);
            if (g_mouseRawAccumDx.compare_exchange_weak(old, nxt, std::memory_order_release, std::memory_order_relaxed))
                break;
        }
    }
    if (dy != 0)
    {
        int old = g_mouseRawAccumDy.load(std::memory_order_relaxed);
        for (;;)
        {
            const int nxt = halljoy::arithmetic::SaturatingAddInt(old, dy, -32768, 32767);
            if (g_mouseRawAccumDy.compare_exchange_weak(old, nxt, std::memory_order_release, std::memory_order_relaxed))
                break;
        }
    }
}

void Backend_SetMouseBindButtonState(uint16_t mouseBindHid, bool down)
{
    if (!g_runtimeAdmission.load(std::memory_order_acquire)) return;
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
    if (!g_runtimeAdmission.load(std::memory_order_acquire)) return;
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
        g_vigemResubmitRequested.store(true, std::memory_order_release);
        g_vigemOutputRuntime.Configure(
            g_virtualPadsEnabled.load(std::memory_order_acquire),
            static_cast<std::uint32_t>(count));
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
        g_vigemResubmitRequested.store(true, std::memory_order_release);
        g_vigemOutputRuntime.Configure(on,
            static_cast<std::uint32_t>(std::clamp(
                g_virtualPadCount.load(std::memory_order_acquire),
                1, kMaxVirtualPads)));
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

bool BackendNative_SparkGetSnapshotV2(halljoy::native_analog_snapshot::OutputV1 output)
{
    using namespace halljoy::native_analog_snapshot;
    const std::uint64_t providerId = StableProviderId("native.sparklink.v2");
    std::array<std::uint8_t, 256> owned{};
    std::array<std::uint16_t, 256> milli{};
    LegacyMilliSourceV1 source{};
    std::uint64_t publication = 0;

    // Do not block Backend_Tick: retry a concurrent row publication a bounded
    // number of times, then report no snapshot rather than inventing coherence.
    for (unsigned attempt = 0; attempt < 3u; ++attempt)
    {
        const std::uint64_t before = g_sparkPublicationSequence.load(std::memory_order_acquire);
        if ((before & 1u) != 0)
            continue;
        const bool connected = g_sparkConnected.load(std::memory_order_acquire);
        source.exactInterfaceId = g_sparkExactInterfaceId.load(std::memory_order_relaxed);
        source.vendorId = g_sparkConnectedVid.load(std::memory_order_relaxed);
        source.productId = g_sparkConnectedPid.load(std::memory_order_relaxed);
        source.usagePage = g_sparkUsagePage.load(std::memory_order_relaxed);
        source.usage = kSparkKnownUsage;
        source.protocolId = static_cast<std::uint32_t>(NativeAnalogProtocol::SparkLink);
        source.connected = connected;
        source.protocolProven = connected;
        source.layoutProven = connected;
        // Row polling offers independent fresh samples, but not one atomic
        // whole-keyboard acquisition; leave V2 Complete clear intentionally.
        source.topologyComplete = false;
        source.ownedHid = owned.data();
        source.milli = milli.data();
        if (connected)
        {
            int effectiveRows = std::clamp(g_sparkRowCount.load(std::memory_order_relaxed),
                0, kSparkMaxRows);
            const UINT rowLimit = Settings_GetSparkRowLimit();
            if (rowLimit > 0)
                effectiveRows = std::min(effectiveRows,
                    std::clamp(static_cast<int>(rowLimit), 1, kSparkMaxRows));
            const ULONGLONG snapshotMs = GetTickCount64();
            for (int row = 0; row < effectiveRows; ++row)
            {
                if (!SparkRowFresh(static_cast<uint8_t>(row), snapshotMs, effectiveRows))
                    continue;
                for (int col = 0; col < kSparkColsPerRow; ++col)
                {
                    const auto index = static_cast<std::size_t>(row) *
                        kSparkColsPerRow + static_cast<std::size_t>(col);
                    const std::uint8_t hid = g_sparkRowColToHid[index]
                        .load(std::memory_order_relaxed);
                    if (hid != 0)
                        owned[hid] = 1;
                }
            }
            for (std::size_t hid = 1; hid < milli.size(); ++hid)
                milli[hid] = g_sparkAnalogMilli[hid].load(std::memory_order_relaxed);
        }
        const std::uint64_t after = g_sparkPublicationSequence.load(std::memory_order_acquire);
        if (before == after && (after & 1u) == 0)
        {
            publication = after / 2u + 1u;
            break;
        }
    }
    if (publication == 0)
        return false;
    return PublishLegacyMilli(providerId, 1, publication, SparkNowUs(),
        source.connected ? &source : nullptr, source.connected ? 1u : 0u, output);
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
    out->lastUpdateAgeMs = static_cast<std::uint32_t>(std::min<ULONGLONG>(
        halljoy::monotonic_time::SaturatingAgeMs(GetTickCount64(), last),
        0xFFFFFFFFull));
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
    out->lastUpdateAgeMs = static_cast<std::uint32_t>(std::min<ULONGLONG>(
        halljoy::monotonic_time::SaturatingAgeMs(GetTickCount64(), last),
        0xFFFFFFFFull));
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
        &BackendNative_SparkGetSnapshotV2,
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
