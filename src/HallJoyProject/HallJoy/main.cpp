#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "support_log.h"
#include <objidl.h>
#include <gdiplus.h>
#include <strsafe.h>
#include <string>
#include <cwchar>
#include <exception>

#include "vigem_output_process_host.h"
#include "vigem_output_self_host_test.h"
#include "embedded_analog_stack.h"
#include "instance_guard.h"

#include "app.h"
#include "win_util.h"
#include "Resource.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "analog_host_client.h"
#include "app_deps.h"
#include "provider_v2_controller_shadow.h"
#include "provider_v2_qualification_report.h"

#pragma comment(lib, "gdiplus.lib")

static constexpr int kDebugLogSchemaVersion = 17;

static void InitDpiAwareness()
{
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (!u32)
        return;

    using SetCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setCtx = (SetCtxFn)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
    if (setCtx)
    {
        // system-aware: scales correctly at startup, without needing WM_DPICHANGED relayout
        setCtx(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
        return;
    }

    // fallback (older Windows)
    using SetAwareFn = BOOL(WINAPI*)();
    auto setAware = (SetAwareFn)GetProcAddress(u32, "SetProcessDPIAware");
    if (setAware) setAware();
}

static bool ShutdownDebugLogSafely()
{
    if (!SupportLog_Stop()) {
        TerminateProcess(GetCurrentProcess(), 3);
        return false;
    }
    const auto logStop = DebugLog_Shutdown();
    if (logStop.RestartSafe())
        return true;

    constexpr int logPoisonedExitCode = 3;
    StabilityTrace_WriteCritical(L"ERROR", L"main", L"process_exit.log_poisoned",
        L"state=%u generation=%llu error=%u native_error=%lu exit_code=%d crt_cleanup_skipped=1",
        static_cast<unsigned>(logStop.state),
        static_cast<unsigned long long>(logStop.generation.Value()),
        static_cast<unsigned>(logStop.error.code),
        static_cast<unsigned long>(logStop.error.native_error),
        logPoisonedExitCode);
    StabilityTrace_Shutdown(logPoisonedExitCode);
    TerminateProcess(GetCurrentProcess(), logPoisonedExitCode);
    return false;
}

static int RunUapProviderV2DualCaptureSelfTest()
{
    constexpr int kInitFailure = 61;
    constexpr int kCaptureFailure = 62;
    constexpr int kStopFailure = 63;

    const int initialised = AnalogHostClient_Initialise();
    bool captured = false;
    if (initialised >= 0)
    {
        const ULONGLONG deadline = GetTickCount64() + 5000;
        do
        {
            halljoy::uap_parent_snapshot::SnapshotV1 snapshot{};
            halljoy::analog_provider_v2::AnalogSnapshotHeaderV2
                providerPlaneHeader{};
            halljoy::provider_v2_shadow::RawInputMapV1 providerRaw{};
            AnalogHostTelemetry telemetry{};
            const bool denseCaptured =
                AnalogHostClient_CaptureTickSnapshot(&snapshot);
            auto providerLease =
                AnalogHostClient_AcquireProviderV2Snapshot();
            const bool brokerCaptured = denseCaptured && providerLease &&
                halljoy::provider_v2_shadow::MatchesCapturedPublication(
                    providerLease.Metadata(), snapshot) &&
                halljoy::provider_v2_shadow::ProjectCapturedSnapshot(
                    providerLease.Header(), providerLease.Devices(),
                    providerLease.DeviceCount(), providerLease.Samples(),
                    providerLease.SampleCount(), &providerRaw) ==
                    halljoy::provider_v2_shadow::ProjectionError::None;
            captured = brokerCaptured &&
                AnalogHostClient_CaptureProviderV2PlaneHeader(
                    &providerPlaneHeader) &&
                AnalogHostClient_GetTelemetry(&telemetry) &&
                telemetry.ready &&
                telemetry.providerV2PlaneAvailable &&
                telemetry.providerV2PlaneDualCoherent &&
                telemetry.providerV2PlaneParentReadOnly &&
                telemetry.providerV2PlaneGeneration != 0 &&
                telemetry.providerV2PlaneMappingBytes != 0 &&
                telemetry.providerV2PlaneTransactionToken != 0 &&
                telemetry.providerV2PlaneRequiredDeviceCount > 0 &&
                telemetry.providerV2PlaneRequiredSampleCount > 0 &&
                telemetry.providerV2PlaneDeviceCapacity >=
                    telemetry.providerV2PlaneRequiredDeviceCount &&
                telemetry.providerV2PlaneSampleCapacity >=
                    telemetry.providerV2PlaneRequiredSampleCount &&
                telemetry.restartCount >= 1 &&
                snapshot.providerV2PlaneDualCoherent &&
                snapshot.providerV2PlaneGeneration ==
                    providerLease.Metadata().planeGeneration &&
                snapshot.providerV2PlaneTransactionToken ==
                    providerLease.Metadata().transactionToken &&
                halljoy::analog_provider_v2::IsAuthoritative(
                    providerLease.Header()) &&
                halljoy::analog_provider_v2::IsAuthoritative(
                    providerPlaneHeader) &&
                providerLease.Header().sampleGeneration ==
                    providerPlaneHeader.sampleGeneration &&
                providerLease.DeviceCount() > 0 &&
                providerLease.SampleCount() > 0 &&
                providerRaw.mappedSampleCount + providerRaw.ignoredSampleCount ==
                    providerLease.SampleCount() &&
                providerPlaneHeader.deviceCount > 0 &&
                providerPlaneHeader.sampleCount > 0;
            if (captured)
                break;
            Sleep(5);
        } while (GetTickCount64() < deadline);
    }

    const WootingAnalogResult stopped = AnalogHostClient_Uninitialise();
    if (stopped != WootingAnalogResult_Ok)
        return kStopFailure;
    if (initialised < 0)
        return kInitFailure;
    return captured ? 0 : kCaptureFailure;
}

int WINAPI wWinMain(
    _In_ HINSTANCE hInst,
    _In_opt_ HINSTANCE,
    _In_ PWSTR,
    _In_ int nCmdShow)
{
    // The output service and its exact-image test must branch before every
    // installer, logger, UI, GDI+, analogue-provider and backend action.
    int vigemOutputHostExit = 0;
    if (VigemOutputHost_TryRunCommand(vigemOutputHostExit))
        return vigemOutputHostExit;

    int vigemOutputSelfTestExit = 0;
    if (VigemOutputSelfHostTest_TryRunCommand(vigemOutputSelfTestExit))
        return vigemOutputSelfTestExit;

    int embeddedVigemVerificationExit = 0;
    if (AppDeps_TryRunEmbeddedInstallerVerificationCommand(
            hInst, embeddedVigemVerificationExit))
        return embeddedVigemVerificationExit;

    int embeddedInstallerExit = 0;
    if (EmbeddedAnalogStack_TryRunInstallerCommand(hInst, embeddedInstallerExit))
        return embeddedInstallerExit;

    // The crash-isolated analog host uses the same executable. It must branch
    // before the main diagnostic logger/watchdog and before any UI startup.
    int analogHostExit = 0;
    if (AnalogHost_TryRunCommand(analogHostExit))
        return analogHostExit;

    // The diagnostic watchdog uses the same executable but must not initialise
    // the app or overwrite HallJoyDiagnostic.log.
    if (DebugLog_TryRunExitWatchdogCommand())
        return 0;

    // This is deliberately after every same-image child-role branch above,
    // but before a provider, data root, logger or qualification report can be
    // opened. The per-user global namespace also covers concurrent sessions
    // of one Windows account without blocking other users.
    halljoy::instance_guard::Guard instanceGuard;
    const auto instanceResult = instanceGuard.AcquireForCurrentUser();
    if (instanceResult != halljoy::instance_guard::AcquireResult::Acquired)
    {
        const bool conflict = instanceResult == halljoy::instance_guard::AcquireResult::Conflicted;
        wchar_t message[512]{};
        if (conflict)
        {
            StringCchCopyW(message, sizeof(message) / sizeof(message[0]),
                L"Another HallJoy window is already running for this Windows user.\n\n"
                L"Close it before starting another HallJoy instance.");
        }
        else
        {
            StringCchPrintfW(message, sizeof(message) / sizeof(message[0]),
                L"HallJoy could not safely reserve its per-user runtime ownership.\n\n"
                L"Windows error: %lu", static_cast<unsigned long>(instanceGuard.LastError()));
        }
        MessageBoxW(nullptr, message, L"HallJoy", MB_ICONWARNING | MB_OK);
        return conflict ? 0 : 66;
    }

    StabilityTrace_Init();
    StabilityTrace_Write(L"INFO", L"main", L"build", L"stage=S02V1 target=HallJoy");
    DebugLog_Init();
    SupportLog_Start();
    AnalogHostClient_ResetDiagnosticFiles();
    DebugLog_InstallCrashHandler();
    DebugLog_StartExitWatchdog();
    if (!ProviderV2QualificationReport_Begin())
    {
        constexpr int qualificationStartFailure = 64;
        DebugLog_Write(L"[provider_v2.qualification] failed to create the initial fail-closed report");
        if (!ShutdownDebugLogSafely())
            return 3;
        StabilityTrace_Shutdown(qualificationStartFailure);
        App_DisarmShutdownWatchdog();
        return qualificationStartFailure;
    }
    DebugLog_Write(L"[build] log_schema=%d compiled=%S %S", kDebugLogSchemaVersion, __DATE__, __TIME__);
    DebugLog_Write(L"[build] diagnostic=%d", DebugLog_IsDiagnosticBuild() ? 1 : 0);
    DebugLog_Write(L"[main] wWinMain start hInst=%p cmdShow=%d", hInst, nCmdShow);
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
    if (wcsstr(GetCommandLineW(), L"--halljoy-test-drunkdeer-log-growth"))
    {
        static constexpr wchar_t growthLine[] =
            L"[drunkdeer.log_growth_test] payload=0123456789abcdef0123456789abcdef"
            L"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
            L"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        for (unsigned index = 0; index < 16000; ++index)
            StabilityTrace_AppendPlain(growthLine);
        StabilityTrace_WriteCritical(L"INFO", L"trace",
            L"trace.growth_test.complete", L"lines=16000 hard_cap=0");
        if (!ShutdownDebugLogSafely()) return 3;
        StabilityTrace_Shutdown(0);
        return 0;
    }
#endif
#if defined(HALLJOY_DIAGNOSTIC)
    if (wcsstr(GetCommandLineW(), L"--diagnostic-crash-test"))
    {
        DebugLog_SetCheckpoint(L"diagnostic: intentional crash self-test");
        DebugLog_Write(L"[diagnostic] intentional crash self-test begin");
        RaiseException(0xE0424242u, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    }
#endif

    const bool embeddedAnalogReady = EmbeddedAnalogStack_Prepare(hInst);
    StabilityTrace_Write(embeddedAnalogReady ? L"INFO" : L"WARN", L"main", L"embedded_stack.prepare",
        L"ready=%d location=%s error=%lu system_sdk_required=0",
        embeddedAnalogReady ? 1 : 0, EmbeddedAnalogStack_RuntimeLocationName(),
        EmbeddedAnalogStack_LastError());
    if (!embeddedAnalogReady)
    {
        DebugLog_Write(L"[main] embedded analog stack preparation failed");
#if defined(HALLJOY_MAD68PR_NATIVE)
        // The universal native target can still discover and run independent HID
        // protocol backends (MAD68 A0, Hex80 0x96, Addressed 09/94/02, SparkLink
        // and Sayo) even when the optional crash-isolated UAP could not be prepared.
        // Capability classification in App_Run/Backend_Init remains authoritative;
        // unsupported devices are not claimed and no blind vendor probing is added.
        DebugLog_Write(L"[main] universal native continuation enabled; UAP unavailable for this run; capability classification will determine available native routes");
#else
        MessageBoxW(nullptr,
            L"Failed to prepare the private crash-isolated Universal Analog Plugin.",
            L"HallJoy",
            MB_ICONERROR | MB_OK);
        if (!ShutdownDebugLogSafely())
            return 3;
        StabilityTrace_Shutdown(1);
        return 1;
#endif
    }
    else
    {
        DebugLog_Write(L"[main] private ABI1 plugin ready; all plugin/Soup execution is isolated in child process");
        DebugLog_Write(L"[main] isolation=enabled sdk_layer=bypassed direct_abi1=enabled host=self --halljoy-analog-host external_debug_capture=enabled auto_restart=enabled");
    }

    // Local release gate: exercise the exact production image, real child
    // boundary and parent seqlock capture without starting UI or ViGEm output.
    // It intentionally requires a real UAP device and is therefore not a
    // universal build-machine gate.
    if (wcsstr(GetCommandLineW(), L"--halljoy-test-uap-provider-v2-dual-capture"))
    {
        const int selfTestResult = embeddedAnalogReady
            ? RunUapProviderV2DualCaptureSelfTest() : 60;
        if (!ShutdownDebugLogSafely())
            return 3;
        StabilityTrace_Shutdown(selfTestResult);
        App_DisarmShutdownWatchdog();
        return selfTestResult;
    }

    InitDpiAwareness();
    DebugLog_Write(L"[main] dpi awareness configured");

    // Init GDI+ once for the entire application lifetime
    Gdiplus::GdiplusStartupInput gdiInput;
    ULONG_PTR gdiToken = 0;
    Gdiplus::Status gdiStatus = Gdiplus::GdiplusStartup(&gdiToken, &gdiInput, nullptr);
    DebugLog_Write(L"[main] Gdiplus startup status=%d token=%p", (int)gdiStatus, (void*)gdiToken);

    int result = 1;
    try
    {
        result = App_Run(hInst, nCmdShow);
        StabilityTrace_Write(result == 0 ? L"INFO" : L"WARN", L"main", L"app.exit", L"result=%d", result);
        DebugLog_Write(L"[main] App_Run returned=%d", result);
    }
    catch (const std::exception& ex)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"main", L"app.exception", L"kind=std_exception");
        DebugLog_Write(L"[main] App_Run exception what=%S", ex.what());
    }
    catch (...)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"main", L"app.exception", L"kind=unknown");
        DebugLog_Write(L"[main] App_Run unknown exception");
    }
    StabilityTrace_Write(L"INFO", L"main", L"final_shutdown.begin");
    App_ForceFinalShutdown();
    StabilityTrace_Write(L"INFO", L"main", L"final_shutdown.end");
    const bool relaunchRequested = App_TakeRelaunchRequest();

    if (App_RequiresImmediateProcessExit())
    {
        constexpr int poisonedExitCode = 2;
        StabilityTrace_WriteCritical(L"ERROR", L"main", L"process_exit.poisoned",
            L"exit_code=%d crt_cleanup_skipped=1", poisonedExitCode);
        StabilityTrace_Shutdown(poisonedExitCode);
        TerminateProcess(GetCurrentProcess(), poisonedExitCode);
        return poisonedExitCode;
    }

    if (gdiStatus == Gdiplus::Ok && gdiToken != 0)
        Gdiplus::GdiplusShutdown(gdiToken);
    DebugLog_Write(L"[main] exit");
    if (!ShutdownDebugLogSafely())
        return 3;
    if (!ProviderV2QualificationReport_Finalize(result))
        result = 65;
    StabilityTrace_Shutdown(result);
    // Keep the watchdog armed through logger and stability-trace teardown. It
    // is safe to release only after every explicit shutdown stage completed.
    App_DisarmShutdownWatchdog();

    if (relaunchRequested && !App_RelaunchSelf())
    {
        MessageBoxW(nullptr,
            L"HallJoy closed safely, but Windows could not restart it automatically.\n\n"
            L"Start HallJoy again to finish the factory reset.",
            L"HallJoy factory reset",
            MB_ICONWARNING | MB_OK);
    }

    return result;
}
