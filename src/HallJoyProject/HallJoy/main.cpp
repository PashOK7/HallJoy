#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <cwchar>
#include <exception>

#include "embedded_analog_stack.h"

#include "app.h"
#include "win_util.h"
#include "Resource.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "analog_host_client.h"

#pragma comment(lib, "gdiplus.lib")

static constexpr int kDebugLogSchemaVersion = 17;

static void InitDpiAwareness()
{
    HMODULE u32 = GetModuleHandleW(L"user32.dll");

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

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
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

    StabilityTrace_Init();
    StabilityTrace_Write(L"INFO", L"main", L"build", L"stage=S02V1 target=HallJoy");
    DebugLog_Init();
    AnalogHostClient_ResetDiagnosticFiles();
    DebugLog_InstallCrashHandler();
    DebugLog_StartExitWatchdog();
    DebugLog_Write(L"[build] log_schema=%d compiled=%S %S", kDebugLogSchemaVersion, __DATE__, __TIME__);
    DebugLog_Write(L"[build] diagnostic=%d", DebugLog_IsDiagnosticBuild() ? 1 : 0);
    DebugLog_Write(L"[main] wWinMain start hInst=%p cmdShow=%d", hInst, nCmdShow);
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
    StabilityTrace_Shutdown(result);

    return result;
}
