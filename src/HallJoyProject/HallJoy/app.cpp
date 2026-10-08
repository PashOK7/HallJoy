#include "perf_trace.h"
#include "keychron_onboard_backend.h"
#include "input_path_diagnostics.h"
// app.cpp
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "support_log.h"
#include "redsquare_code_probe.h"
#include "logitech_rapid_backend.h"
#include "irok_na87_diagnostic.h"
#include "aula_mini60_diagnostic.h"
#include "attackshark_pro_diagnostic.h"
#include "irok_na87_backend.h"
#include "native_layout_devices.h"
#include "window_placement_windows.h"
#include "main_keyboard_input.h"
#include "keyboard_canvas.h"
#include <dbt.h>
#include <commctrl.h>
#include <shellapi.h>
#include <hidusage.h>

#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <atomic>
#include <exception>
#include <mutex>
#include <unordered_map>
#include <cwctype>
#include <cwchar>

#include "game_profile_service.h"
#include "app.h"
#include "tray_window.h"
static halljoy::tray::Window g_tray;
#include "gamepad_latency.h"
#include "app_deps.h"
#include "Resource.h"
#include "backend.h"
#include "bindings.h"
#include "keyboard_layout.h"
#include "keyboard_profiles.h"
#include "keyboard_ui.h"
#include "settings.h"
#include "settings_ini.h"
#include "factory_reset.h"
#include "profile_ini.h"
#include "global_profiles.h"
#include "realtime_loop.h"
#include "engine_runtime_owner.h"
#include "engine_runtime_ui_bridge.h"
#include "runtime_supervisor.h"
#include "stability_trace.h"
#include "win_util.h"
#include "app_paths.h"
#include "ui_theme.h"
#include "debug_log.h"
#include "mouse_ipc.h"
#include "overlay_server.h"
#include "mouse_bind_codes.h"
#include "raw_input_packet_size.h"
#include "digital_keyboard_state.h"
#include "input_privilege_warning.h"
#include "input_privilege_windows.h"
#include "block_keys_policy.h"
#include "pause_hotkeys.h"
#include "input_shortcuts_runtime.h"
#include "keyboard_scan_hid.h"
#include "keyboard_hook_thread.h"
#include "keyboard_ui_state.h"
#include "addressed_analog_backend.h"
#include "mad68pr_backend.h"
#include "hex80_backend.h"
#include "native_analog_routing.h"
#include "native_analog_backend_registry.h"
#include "input_config_summary.h"
#include "input_trace.h"
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
#include "drunkdeer_backend.h"
#endif
#if defined(HALLJOY_MCHOSE_ACE68_DIAGNOSTIC)
#include "mchose_ace68_diagnostic_backend.h"
#endif
#if defined(HALLJOY_AULA_HERO84HE_DIAGNOSTIC)
#include "aula_hero84he_diagnostic_backend.h"
#endif
#if defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC)
#include "titan68_turbo_diagnostic_backend.h"
#endif

#pragma comment(lib, "Comctl32.lib")
static constexpr UINT WM_APP_REQUEST_SAVE = WM_APP + 1;
static constexpr UINT WM_APP_APPLY_TIMING = WM_APP + 2;
static constexpr UINT WM_APP_FACTORY_RESET_RESTART = WM_APP + 3;
static constexpr UINT WM_APP_KEYBOARD_LAYOUT_CHANGED = WM_APP + 260;
static constexpr UINT WM_APP_ENGINE_RUNTIME_UI_OPERATION = WM_APP + 261;
extern "C" UINT KeyboardPageMain_PerfCurveSweepMessage(); // perf profiling only
static constexpr UINT WM_APP_ENGINE_RUNTIME_TOGGLE = WM_APP + 262;
static constexpr UINT WM_APP_ENGINE_RUNTIME_STATE_CHANGED = WM_APP + 362;

// UI refresh timer
static const UINT_PTR UI_TIMER_ID = 2;

// Debounced settings save timer
static const UINT_PTR SETTINGS_SAVE_TIMER_ID = 3;
static const UINT SETTINGS_SAVE_TIMER_MS = 350;
static constexpr UINT_PTR WINDOW_SAVE_TIMER_ID = 14;
static constexpr UINT_PTR WINDOW_REFIT_TIMER_ID = 15;
// K4 HE with HallJoy onboard firmware that appeared after the engine started
// (or was busy then): one automatic Pause+Resume moves it to the onboard
// route. Never the UAP fallback (docs/current/K4_ONBOARD_ONLY_2026-10-03.md).
static constexpr UINT_PTR K4_TAKEOVER_TIMER_ID = 16;
// A failed Resume (startup or later) falls back to Paused. HallJoy retries it
// with a growing delay instead of staying paused until the user restarts.
static constexpr UINT_PTR ENGINE_RESUME_RETRY_TIMER_ID = 17;
static constexpr UINT kResumeRetryDelaysMs[] = { 1000, 2000, 4000, 8000, 15000, 30000 };
static std::atomic<bool> g_resumeWanted{ false };
static unsigned g_resumeRetryAttempts = 0;
static bool g_k4TakeoverPausing = false;
static unsigned g_k4TakeoverAttempts = 0;
static ULONGLONG g_k4TakeoverLastMs = 0;
static bool g_windowPlacementReady = false, g_windowMoving = false, g_windowApplying = false;
static bool g_windowPlacementDirty = false;

static HWND g_hPageMain = nullptr;
static HWND g_hMainWnd = nullptr;
static halljoy::block_keys::KeyboardHookThread g_keyboardHookThread;
static HHOOK g_hMouseHook = nullptr;
// Written by the serialized engine owner and observed by UI presentation only.
static std::atomic<bool> g_backendReady{ false };
static std::atomic<bool> g_shutdownStarted{ false };
static std::atomic<bool> g_relaunchAfterExit{ false };
static std::atomic<bool> g_immediateProcessExitRequired{ false };
static HANDLE g_shutdownWatchdogCancelEvent = nullptr;
static HANDLE g_shutdownWatchdogThread = nullptr;
// The process-wide deadline must cover one in-flight runtime-supervisor
// recovery plus the bounded output/UAP containment phases. Individual owners
// still retain/poison on an unconfirmed join; this is only the final process
// containment boundary, never permission for an unbounded wait.
static constexpr DWORD kShutdownWatchdogTimeoutMs = 30000;
static constexpr UINT kShutdownWatchdogExitCode = 4;
static bool g_cmdStartOverlay = false;
static bool g_cmdStartMinimized = false;
static bool g_cmdLatencyTrace = false;
static uint16_t g_cmdOverlayPort = 0;
static std::atomic<bool> g_mouseBlockPauseByRShift{ false };
// This is deliberately independent of the backend admission gate. It makes
// hooks pass through immediately while the owner waits for their UI-thread
// release acknowledgement; no old hook may swallow input during Pause.
static std::atomic<bool> g_engineUiInputPassThrough{ false };
// Auto-recovery is allowed only while the initial generation has never been
// explicitly paused by the user. A manual Resume remains explicit and does not
// re-enable device-change-driven opens later in the process lifetime.
static std::atomic<bool> g_enginePauseWasExplicit{ false };
static bool g_mouseCursorLocked = false;
static POINT g_mouseCursorLockPos{};
static std::atomic<uint32_t> g_uiTimerTickCount{ 0 };
// The first GetRawInputData call reports an externally supplied byte count.
// Bound it before growing the thread-local buffer; typed payload checks below
// still decide whether the received packet is a valid mouse/keyboard shape.
static constexpr UINT kMaxRawInputPacketBytes = 64u * 1024u;
#if defined(HALLJOY_MAD68PR_NATIVE)
static bool g_lastMad68PresenceForBackendRetry = false;
#endif
static std::atomic<bool> g_rawInputRegistered{ false };
static LRESULT CALLBACK KeyboardBlockHookProc(int nCode, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK MouseBlockHookProc(int nCode, WPARAM wParam, LPARAM lParam);

static DWORD WINAPI ShutdownWatchdogThreadProc(void* parameter) noexcept
{
    const HANDLE cancelEvent = static_cast<HANDLE>(parameter);
    if (WaitForSingleObject(cancelEvent, kShutdownWatchdogTimeoutMs) == WAIT_TIMEOUT)
    {
        // This is the final containment boundary. Do not call any logger here:
        // shutdown may be stuck while holding an arbitrary logger/CRT lock.
        OutputDebugStringW(L"[HallJoy] shutdown watchdog deadline exceeded; terminating process\n");
        TerminateProcess(GetCurrentProcess(), kShutdownWatchdogExitCode);
    }
    return 0;
}

static bool ArmShutdownWatchdog() noexcept
{
    g_shutdownWatchdogCancelEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_shutdownWatchdogCancelEvent)
        return false;

    g_shutdownWatchdogThread = CreateThread(
        nullptr, 0, ShutdownWatchdogThreadProc, g_shutdownWatchdogCancelEvent, 0, nullptr);
    if (!g_shutdownWatchdogThread)
    {
        const DWORD error = GetLastError();
        CloseHandle(g_shutdownWatchdogCancelEvent);
        g_shutdownWatchdogCancelEvent = nullptr;
        SetLastError(error);
        return false;
    }
    return true;
}

static void DisarmShutdownWatchdog() noexcept
{
    if (g_shutdownWatchdogCancelEvent)
        SetEvent(g_shutdownWatchdogCancelEvent);
    if (g_shutdownWatchdogThread)
    {
        WaitForSingleObject(g_shutdownWatchdogThread, INFINITE);
        CloseHandle(g_shutdownWatchdogThread);
        g_shutdownWatchdogThread = nullptr;
    }
    if (g_shutdownWatchdogCancelEvent)
    {
        CloseHandle(g_shutdownWatchdogCancelEvent);
        g_shutdownWatchdogCancelEvent = nullptr;
    }
}
#if defined(HALLJOY_MAD68PR_NATIVE)
static std::unordered_map<HANDLE, bool> g_mad68RawKeyboardCache;

static bool IsMad68RawKeyboard(HANDLE device)
{
    if (!device) return false;
    const auto cached = g_mad68RawKeyboardCache.find(device);
    if (cached != g_mad68RawKeyboardCache.end()) return cached->second;

    UINT chars = 0;
    GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &chars);
    std::vector<wchar_t> name(static_cast<std::size_t>(chars) + 2u, L'\0');
    bool match = false;
    if (chars != 0 && GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name.data(), &chars) != static_cast<UINT>(-1))
    {
        std::wstring lower(name.data());
        std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) {
            return static_cast<wchar_t>(towlower(c));
        });
        const std::size_t vidPos = lower.find(L"vid_373b");
        const std::size_t pidPos = lower.find(L"pid_");
        std::uint16_t pid = 0;
        if (pidPos != std::wstring::npos && pidPos + 8u <= lower.size())
        {
            wchar_t* end = nullptr;
            const unsigned long parsed = wcstoul(lower.c_str() + pidPos + 4u, &end, 16);
            if (end != lower.c_str() + pidPos + 4u && parsed <= 0xFFFFu)
                pid = static_cast<std::uint16_t>(parsed);
        }
        match = vidPos != std::wstring::npos && pid != 0 && Mad68ProR_IsRoutedProduct(pid);
        DebugLog_Write(L"[mad68pr.rawinput] keyboard device=%s target=%d", name.data(), match ? 1 : 0);
    }
    else
    {
        DebugLog_Write(L"[mad68pr.rawinput] device-name query failed handle=%p err=%lu", device, GetLastError());
    }
    g_mad68RawKeyboardCache.emplace(device, match);
    return match;
}
#endif

static void App_ParseCommandLine()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return;

    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--overlay-server")
        {
            g_cmdStartOverlay = true;
        }
        else if (arg == L"--minimized")
        {
            g_cmdStartMinimized = true;
        }
        else if (arg == L"--latency-trace")
        {
            g_cmdLatencyTrace = true;
        }
        else if (arg == L"--port" && i + 1 < argc)
        {
            wchar_t* end = nullptr;
            unsigned long port = wcstoul(argv[++i], &end, 10);
            if (end && *end == 0 && port >= 1 && port <= 65535)
                g_cmdOverlayPort = (uint16_t)port;
        }
    }

    LocalFree(argv);
}

static halljoy::block_keys::PressRoutes g_blockPressRoutes;
static std::array<std::atomic<ULONGLONG>, 256> g_hookDigital{};
static constexpr UINT WM_APP_BLOCK_TOGGLED = WM_APP + 368;
static DWORD g_keyboardHookError = ERROR_SUCCESS;
static void SeedBlockPressRoutes();

// Every command shortcut (Block toggle, Pause/Resume) is one shortcut system.
// A shortcut may not be shared by two commands of the active configuration.
static bool ShortcutUsedByOtherCommand(unsigned shortcut, int exceptAction)
{
    if (!shortcut) return false;
    const auto bindings = halljoy::shortcuts::CurrentBindings();
    for (unsigned i = 0; i < bindings.size(); ++i)
        if (static_cast<int>(i + 1) != exceptAction && bindings[i] == shortcut) return true;
    return false;
}

DWORD App_SetBlockKeysHotkey(UINT shortcut)
{
    if (!halljoy::shortcuts::Valid(shortcut)) return ERROR_INVALID_PARAMETER;
    if (ShortcutUsedByOtherCommand(shortcut, static_cast<int>(halljoy::shortcuts::Action::BlockToggle)))
        return ERROR_ALREADY_ASSIGNED;
    Settings_SetBlockKeysHotkey(shortcut);
    return ERROR_SUCCESS;
}
DWORD App_ValidatePauseShortcut(unsigned slot, unsigned shortcut)
{
    if (slot > 2 || !halljoy::shortcuts::Valid(shortcut)) return ERROR_INVALID_PARAMETER;
    if (!shortcut) return ERROR_SUCCESS;
    if (shortcut == Settings_GetBlockKeysHotkey()) return ERROR_ALREADY_ASSIGNED;
    if (slot != 0 && Settings_GetPauseShortcut(slot == 1 ? 2 : 1) == shortcut) return ERROR_ALREADY_ASSIGNED;
    const auto bindings = halljoy::shortcuts::CurrentBindings();
    for (unsigned i = 4; i < bindings.size(); ++i) // game profile commands
        if (bindings[i] == shortcut) return ERROR_ALREADY_ASSIGNED;
    return ERROR_SUCCESS;
}
DWORD App_ValidateCommandShortcut(unsigned action, unsigned shortcut)
{
    if (!halljoy::shortcuts::Valid(shortcut)) return ERROR_INVALID_PARAMETER;
    if (!shortcut) return ERROR_SUCCESS;
    return ShortcutUsedByOtherCommand(shortcut, static_cast<int>(action)) ? ERROR_ALREADY_ASSIGNED : ERROR_SUCCESS;
}
DWORD App_BlockKeysHotkeyError() { return g_keyboardHookError; }

static std::atomic<HWND> g_shortcutCaptureOwner{nullptr};
void App_BeginShortcutCapture(HWND owner)
{
    // Only one capture at a time: a capture started elsewhere is cancelled.
    const HWND previous = g_shortcutCaptureOwner.exchange(owner);
    if (previous && previous != owner)
        PostMessageW(previous, halljoy::shortcuts::kCaptureMessage, 0,
            static_cast<LPARAM>(halljoy::shortcuts::kCaptureCancelled));
    halljoy::shortcuts::BeginCapture(owner);
}
void App_CancelShortcutCapture(bool notifyOwner)
{
    const HWND owner = g_shortcutCaptureOwner.exchange(nullptr);
    halljoy::shortcuts::CancelCapture();
    if (notifyOwner && owner)
        PostMessageW(owner, halljoy::shortcuts::kCaptureMessage, 0,
            static_cast<LPARAM>(halljoy::shortcuts::kCaptureCancelled));
}
void App_EndShortcutCapture(HWND owner)
{
    HWND expected = owner;
    if (g_shortcutCaptureOwner.compare_exchange_strong(expected, nullptr))
        halljoy::shortcuts::CancelCapture();
}

// Called on the hook thread or the realtime thread, outside the engine lock.
static bool ShortcutApplicable(halljoy::shortcuts::Action action)
{
    using halljoy::shortcuts::Action;
    const auto state = halljoy::pause_hotkey::state.load();
    switch (action)
    {
    case Action::BlockToggle:
        return !g_engineUiInputPassThrough.load(std::memory_order_acquire);
    case Action::PauseToggle:
        return state != halljoy::pause_hotkey::State::Transition;
    case Action::Pause:
        return state == halljoy::pause_hotkey::State::Active;
    case Action::Resume:
        return state == halljoy::pause_hotkey::State::Paused;
    case Action::NextProfile:
    case Action::AutoProfiles:
        return !g_engineUiInputPassThrough.load(std::memory_order_acquire);
    default:
        return halljoy::shortcuts::ProfileSlotIndex(action) >= 0 &&
            !g_engineUiInputPassThrough.load(std::memory_order_acquire);
    }
}

static void ShortcutDispatch(halljoy::shortcuts::Action action)
{
    using halljoy::shortcuts::Action;
    if (action == Action::BlockToggle)
    {
        // Applied immediately so the very next keyboard event sees it; UI
        // refresh, dirty marking and saving follow on the UI thread.
        Settings_SetBlockBoundKeys(!Settings_GetBlockBoundKeys());
        if (g_hMainWnd) PostMessageW(g_hMainWnd, WM_APP_BLOCK_TOGGLED, 0, 0);
        return;
    }
    if (action == Action::NextProfile || action == Action::AutoProfiles ||
        halljoy::shortcuts::ProfileSlotIndex(action) >= 0)
    {
        // Profile switching saves and loads files: the UI thread does it.
        if (g_hMainWnd) PostMessageW(g_hMainWnd, WM_APP_PROFILE_SHORTCUT, static_cast<WPARAM>(action), 0);
        return;
    }
    auto pauseAction = halljoy::pause_hotkey::Action::None;
    if (action == Action::Pause) pauseAction = halljoy::pause_hotkey::Action::Pause;
    else if (action == Action::Resume) pauseAction = halljoy::pause_hotkey::Action::Resume;
    else if (action == Action::PauseToggle)
        pauseAction = halljoy::pause_hotkey::state.load() == halljoy::pause_hotkey::State::Active
            ? halljoy::pause_hotkey::Action::Pause : halljoy::pause_hotkey::Action::Resume;
    const HWND target = halljoy::pause_hotkey::window.load();
    if (target && pauseAction != halljoy::pause_hotkey::Action::None)
        PostMessageW(target, halljoy::pause_hotkey::Message, static_cast<WPARAM>(pauseAction),
            static_cast<LPARAM>(halljoy::pause_hotkey::generation.load()));
}

static bool NeedMouseHookNow()
{
    // Mouse LL hook is expensive on some systems; enable only for mouse-to-stick path.
    return !g_engineUiInputPassThrough.load(std::memory_order_acquire) &&
           (Settings_GetMouseToStickEnabled() ||
            Settings_GetBlockMouseInput());
}

static void RefreshLowLevelHooks()
{
    // Keep press ownership through pause/settings changes. Installation and the
    // event pump belong to a dedicated thread, never the saving/rendering UI.
    const DWORD hookError = g_keyboardHookThread.Start(KeyboardBlockHookProc, SeedBlockPressRoutes);
    if (hookError != g_keyboardHookError) {
        g_keyboardHookError = hookError;
        DebugLog_Write(L"[app] keyboard hook thread error=%lu", hookError);
        if (g_hPageConfig) PostMessageW(g_hPageConfig, WM_APP_BLOCK_KEYS_CHANGED, 0, 0);
    }

    const bool wantMouse = NeedMouseHookNow();
    if (wantMouse && !g_hMouseHook)
    {
        g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseBlockHookProc, GetModuleHandleW(nullptr), 0);
        DebugLog_Write(L"[app] mouse hook install=%p", g_hMouseHook);
    }
    else if (!wantMouse && g_hMouseHook)
    {
        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = nullptr;
        DebugLog_Write(L"[app] mouse hook removed");
    }
}

// Startup failures and command-line self-tests must never persist partial state.
static bool g_profileReadyForAutosave = false;

static bool SaveSettingsByActiveGlobalProfile()
{
    if (!g_profileReadyForAutosave) return true;
    DebugLog_SetCheckpoint(L"ui: save settings begin");
    DebugLog_Write(L"[settings] save begin");
    const std::wstring& active = GlobalProfiles_GetActiveName();
    if (GlobalProfiles_IsDefault(active))
    {
        const bool saved = SettingsIni_Save(AppPaths_SettingsIni().c_str());
#if defined(HALLJOY_ANALOG_SIMULATOR)
        const wchar_t* commandLine = GetCommandLineW();
        if (commandLine && wcsstr(commandLine, L"--halljoy-test-persistence-failure-"))
        {
            const std::wstring bindingsProbe = AppPaths_BindingsIni() + L".transaction-probe";
            const std::wstring overlayProbe = AppPaths_SettingsIni() + L".overlay-transaction-probe";
            const std::wstring layoutProbe = AppPaths_SettingsIni() + L".layout-transaction-probe";
            const std::wstring curveProbe = AppPaths_SettingsIni() + L".curve-transaction-probe";
            const std::wstring curveStateProbe = AppPaths_SettingsIni() + L".curve-state-transaction-probe";
            Profile_SaveIni(bindingsProbe.c_str());
            SettingsIni_SaveOverlay(overlayProbe.c_str());
            KeyboardLayout_TestSaveActivePresetToPath(layoutProbe.c_str());
            KeyboardProfiles::SavePreset(curveProbe, KeyDeadzone{});
            KeyboardProfiles::TestSaveStateToPath(curveStateProbe, L"PersistenceProbe");
        }
#endif
        DebugLog_Write(L"[settings] save default profile done success=%d", saved ? 1 : 0);
        DebugLog_SetCheckpoint(L"ui: save settings done");
        return saved;
    }

    // IMPORTANT:
    // When non-default profile is active, do NOT overwrite base settings.ini with
    // runtime values from that profile, otherwise "Default" profile gets polluted.
    // Keep only active profile marker in base file.
    const bool activeMarkerSaved =
        GlobalProfiles_SaveActiveToSettingsIni(AppPaths_SettingsIni().c_str());
    const bool overlaySaved = SettingsIni_SaveOverlay(AppPaths_SettingsIni().c_str());

    // Active profile stores all runtime settings except layout/window; those
    // are global and go to the base file (the layout was not saved at all
    // while a named profile was active, so "Automatic layout" reverted).
    std::wstring profileSettingsPath = AppPaths_ActiveSettingsIni();
    const bool profileSaved = SettingsIni_SaveProfile(profileSettingsPath.c_str());
    const bool windowSaved = SettingsIni_SaveWindow(AppPaths_SettingsIni().c_str());
    const bool layoutSaved = SettingsIni_SaveLayout(AppPaths_SettingsIni().c_str());
    const bool saved = activeMarkerSaved && overlaySaved && profileSaved && windowSaved && layoutSaved;
    DebugLog_Write(L"[settings] save active profile done success=%d", saved ? 1 : 0);
    DebugLog_SetCheckpoint(L"ui: save settings done");
    return saved;
}

static bool CaptureMainWindowPlacement(HWND window)
{
    if (!g_windowPlacementReady || g_windowApplying) return false;
    halljoy::window_placement::Rect r{};
    bool maximized = false;
    if (!halljoy::window_placement::Capture(window, r, maximized)) return false;
    const int dpi = (int)WinUtil_GetSystemDpiCompat();
    const bool changed = r.x != Settings_GetMainWindowPosXPx() || r.y != Settings_GetMainWindowPosYPx() ||
        r.w != Settings_GetMainWindowWidthPx() || r.h != Settings_GetMainWindowHeightPx() ||
        maximized != Settings_GetMainWindowMaximized() || dpi != Settings_GetMainWindowDpi() ||
        Settings_GetMainWindowPlacementVersion() != 2;
    if (changed) {
        Settings_SetMainWindowWidthPx(r.w); Settings_SetMainWindowHeightPx(r.h);
        Settings_SetMainWindowPosXPx(r.x); Settings_SetMainWindowPosYPx(r.y);
        Settings_SetMainWindowPlacementMeta(2, dpi, maximized);
        g_windowPlacementDirty = true;
    }
    return changed;
}

static void SaveMainWindowPlacement(HWND window)
{
    KillTimer(window, WINDOW_SAVE_TIMER_ID);
    CaptureMainWindowPlacement(window);
    if (g_profileReadyForAutosave && g_windowPlacementDirty &&
        SettingsIni_SaveWindow(AppPaths_SettingsIni().c_str())) g_windowPlacementDirty = false;
}

static void RefitMainWindow(HWND window)
{
    if (!g_windowPlacementReady || g_windowApplying) return;
    halljoy::window_placement::Rect normal{};
    bool maximized = false;
    if (!halljoy::window_placement::Capture(window, normal, maximized)) return;
    const auto fit = halljoy::window_placement::FitToDesktop(normal);
    if (fit.x != normal.x || fit.y != normal.y || fit.w != normal.w || fit.h != normal.h) {
        g_windowApplying = true;
        halljoy::window_placement::Apply(window, fit,
            IsIconic(window) ? SW_SHOWMINIMIZED : maximized ? SW_SHOWMAXIMIZED : SW_SHOWNOACTIVATE, maximized);
        g_windowApplying = false;
    }
    SaveMainWindowPlacement(window);
}

static bool RelaunchSelfImpl()
{
    wchar_t exePath[MAX_PATH]{};
    DWORD n = GetModuleFileNameW(nullptr, exePath, (DWORD)_countof(exePath));
    if (n == 0 || n >= _countof(exePath))
        return false;

    std::wstring workDir(exePath);
    size_t slash = workDir.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        workDir.resize(slash);

    std::wstring cmdLine = L"\"";
    cmdLine += exePath;
    cmdLine += L"\"";
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(
        exePath,
        mutableCmd.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        workDir.empty() ? nullptr : workDir.c_str(),
        &si,
        &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    HINSTANCE h = ShellExecuteW(nullptr, L"open", exePath, nullptr, workDir.empty() ? nullptr : workDir.c_str(), SW_SHOWNORMAL);
    return ((INT_PTR)h > 32);
}

static bool IsOwnForegroundWindow()
{
    HWND fg = GetForegroundWindow();
    if (!fg) return false;

    HWND root = GetAncestor(fg, GA_ROOT);
    if (!root) root = fg;

    wchar_t cls[128]{};
    GetClassNameW(root, cls, (int)_countof(cls));
    return (_wcsicmp(cls, L"WootingVigemGui") == 0 ||
        _wcsicmp(cls, L"KeyboardLayoutEditorHost") == 0);
}

static bool IsMouseBlockingActiveNow()
{
    if (g_engineUiInputPassThrough.load(std::memory_order_acquire)) return false;
    if (!Settings_GetBlockMouseInput()) return false;
    if (!Settings_GetMouseToStickEnabled()) return false;
    if (IsOwnForegroundWindow()) return false;
    if (g_mouseBlockPauseByRShift.load(std::memory_order_relaxed)) return false;
    return true;
}

static void PublishMouseIpcState()
{
    bool mts = Settings_GetMouseToStickEnabled();
    bool blockWanted = Settings_GetBlockMouseInput() && mts;
    bool active = IsMouseBlockingActiveNow();
    bool pause = g_mouseBlockPauseByRShift.load(std::memory_order_relaxed);
    MouseIpc_PublishState(blockWanted, active, mts, pause);
}

static void UpdateMouseCursorLockState(bool blockNow)
{
    if (!blockNow)
    {
        if (g_mouseCursorLocked)
            ClipCursor(nullptr);
        g_mouseCursorLocked = false;
        return;
    }

    if (!g_mouseCursorLocked)
    {
        if (!GetCursorPos(&g_mouseCursorLockPos))
            return;
        RECT clip{
            g_mouseCursorLockPos.x,
            g_mouseCursorLockPos.y,
            g_mouseCursorLockPos.x + 1,
            g_mouseCursorLockPos.y + 1
        };
        ClipCursor(&clip);
        SetCursorPos(g_mouseCursorLockPos.x, g_mouseCursorLockPos.y);
        g_mouseCursorLocked = true;
    }
}

static bool EngineRuntimeUiOperationHandler(
    halljoy::engine_runtime::ui_bridge::Operation operation,
    std::uint32_t& nativeError) noexcept
{
    switch (operation)
    {
    case halljoy::engine_runtime::ui_bridge::Operation::ReleaseInput:
        g_engineUiInputPassThrough.store(true, std::memory_order_release);
        g_mouseBlockPauseByRShift.store(false, std::memory_order_relaxed);
        UpdateMouseCursorLockState(false);
        RefreshLowLevelHooks();
        MouseIpc_ShutdownPublisher();
        nativeError = ERROR_SUCCESS;
        return true;

    case halljoy::engine_runtime::ui_bridge::Operation::RestoreInput:
    {
        halljoy::perf::Scope restoreScope("ui.bridge.restore_input");
        if (!MouseIpc_InitPublisher())
        {
            nativeError = GetLastError();
            return false;
        }
        g_engineUiInputPassThrough.store(false, std::memory_order_release);
        {
            halljoy::perf::Scope hooksScope("ui.bridge.refresh_hooks");
            RefreshLowLevelHooks();
        }
        PublishMouseIpcState();
        nativeError = ERROR_SUCCESS;
        return true;
    }

    case halljoy::engine_runtime::ui_bridge::Operation::DependencyGuidance:
    {
        const HINSTANCE instance = g_hMainWnd
            ? reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(g_hMainWnd, GWLP_HINSTANCE))
            : nullptr;
        const DependencyGuidanceResult result = AppDeps_ShowMissingDependencyGuidance(
            instance, g_hMainWnd, Backend_GetLastInitIssues());
        if (result == DependencyGuidanceResult::NoAction ||
            result == DependencyGuidanceResult::InstallCompleted)
        {
            nativeError = ERROR_SUCCESS;
            return true;
        }
        nativeError = ERROR_CANCELLED;
        return false;
    }
    }
    nativeError = ERROR_INVALID_PARAMETER;
    return false;
}

static void SeedBlockPressRoutes()
{
    g_blockPressRoutes.Reset();
    // Keys already down before hook installation were delivered to Windows.
    for (UINT vk = 8; vk < 255; ++vk) {
        if (!(GetAsyncKeyState(vk) & 0x8000)) continue;
        const UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX);
        const auto hid = HidFromKeyboardScanCode(scan & 255, (scan & 0xff00) == 0xe000, vk);
        halljoy::shortcuts::SeedDigital(hid);
        g_blockPressRoutes.SeedPassed(hid);
    }
}

static bool ReadProcessIntegrity(DWORD pid, DWORD& level, bool& uiAccess)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    HANDLE token = nullptr;
    const bool opened = OpenProcessToken(process, TOKEN_QUERY, &token) != FALSE;
    CloseHandle(process);
    if (!opened) return false;
    alignas(TOKEN_MANDATORY_LABEL) BYTE buffer[256]{};
    DWORD bytes = 0, access = 0;
    bool ok = GetTokenInformation(token, TokenIntegrityLevel, buffer, sizeof(buffer), &bytes) != FALSE;
    if (ok) {
        const auto label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer);
        ok = IsValidSid(label->Label.Sid) != FALSE;
        if (ok) {
            const BYTE count = *GetSidSubAuthorityCount(label->Label.Sid);
            ok = count != 0;
            if (ok) level = *GetSidSubAuthority(label->Label.Sid, count - 1);
        }
    }
    if (!GetTokenInformation(token, TokenUIAccess, &access, sizeof(access), &bytes)) ok = false;
    uiAccess = access != 0;
    CloseHandle(token);
    return ok;
}

static void UpdateInputPrivilegeWarning()
{
    using namespace halljoy::input_privilege;
    if (detector.warning) return; // Advisory stays until restart; no more probing needed.
    static ULONGLONG lastCheck = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - lastCheck < 100) return;
    lastCheck = now;
    static HWND previousWindow = nullptr;
    static DWORD previousPid = 0;
    static bool known = false, higher = false;
    static ULONGLONG sessionStarted = 0;
    const HWND foreground = GetForegroundWindow();
    DWORD pid = 0;
    if (foreground) GetWindowThreadProcessId(foreground, &pid);
    if (foreground != previousWindow || pid != previousPid) {
        previousWindow = foreground; previousPid = pid;
        DWORD ownLevel = 0, otherLevel = 0;
        bool ownUiAccess = false, otherUiAccess = false;
        const bool ownKnown = ReadProcessIntegrity(GetCurrentProcessId(), ownLevel, ownUiAccess);
        known = pid && ownKnown && ReadProcessIntegrity(pid, otherLevel, otherUiAccess);
        higher = known && !ownUiAccess && otherLevel > ownLevel;
        if (!known && pid && ownKnown && !ownUiAccess) {
            // Elevated apps can deny TOKEN_QUERY to a medium-integrity caller.
            // Confirm the actual UIPI barrier instead of treating query failure
            // itself as elevation, or disabling all input evidence processing.
            higher = IsWindowMessageAccessBlocked(foreground);
            known = higher;
        }
        detector.ResetSession(); // Never attribute a press spanning a focus change.
        sessionStarted = now;
    }
    const bool before = detector.warning;
    if (!g_backendReady || g_engineUiInputPassThrough.load(std::memory_order_acquire)) {
        detector.ResetSession();
        sessionStarted = now;
    } else if (known) {
        BackendAnalogTelemetry telemetry{};
        Backend_GetAnalogTelemetry(&telemetry);
        if (telemetry.sdkInitialised && telemetry.deviceCount > 0) {
            for (unsigned hid = 4; hid < 232; ++hid) {
                const auto press = analogPresses.Latest(hid);
                const auto hookTime = g_hookDigital[hid].load(std::memory_order_acquire);
                if (hookTime > sessionStarted) detector.Digital(hid, true, hookTime);
                detector.Sample(hid, press > sessionStarted ? press : 0, now,
                    higher, Settings_GetBlockBoundKeys() && pid != GetCurrentProcessId());
            }
        } else { detector.ResetSession(); sessionStarted = now; }
    }
    if (before != detector.warning && g_hPageConfig)
        PostMessageW(g_hPageConfig, kChangedMessage, 0, 0);
}

static LRESULT CALLBACK KeyboardBlockHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && lParam)
    {
        if (wParam == WM_KEYDOWN || wParam == WM_KEYUP || wParam == WM_SYSKEYDOWN || wParam == WM_SYSKEYUP)
        {
            const KBDLLHOOKSTRUCT* k = (const KBDLLHOOKSTRUCT*)lParam;
            const bool ext = (k->flags & LLKHF_EXTENDED) != 0;
            if (IsSyntheticNumpadShift(k->scanCode, ext))
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            const bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            const bool paused = g_engineUiInputPassThrough.load(std::memory_order_acquire);
            const bool injected = (k->flags & LLKHF_INJECTED) != 0;
            uint16_t hid = HidFromHookKey(k->scanCode, ext, k->vkCode);
            if (isDown && !injected)
                if (hid < g_hookDigital.size()) g_hookDigital[hid].store(GetTickCount64(), std::memory_order_release);
            Backend_NotifyKeyboardEvent(
                hid,
                (uint16_t)(k->scanCode & 0xFFFFu),
                (uint16_t)(k->vkCode & 0xFFFFu),
                isDown,
                (k->flags & LLKHF_INJECTED) != 0);

            if (hid == 229)
            {
                g_mouseBlockPauseByRShift.store(isDown, std::memory_order_relaxed);
                // The UI timer publishes IPC; no IPC or disk work in the hook.
            }

            if (!paused && isDown && !(k->flags & LLKHF_INJECTED) && k->vkCode == VK_DELETE)
            {
                const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                const bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                if (ctrlDown && altDown && Settings_GetMouseToStickEnabled())
                {
                    Settings_SetMouseToStickEnabled(false);
                    if (g_hMainWnd && IsWindow(g_hMainWnd))
                        PostMessageW(g_hMainWnd, WM_APP_REQUEST_SAVE, 0, 0);
                }
            }

            // Shortcuts see every keyboard: physical ones, and injected input from
            // remote-desktop/automation tools (HallJoy itself never injects keys).
            // On the realtime side they also see analog depth of the same keys.
            // The press that issued a command (Block toggle, Pause/Resume) never
            // reaches other applications.
            if (halljoy::shortcuts::Digital(hid, isDown))
                return 1;
            // Block Bound Keys applies only to physical keyboard input.
            if (!injected)
            {
                const bool rescueShift = hid == 229 && Settings_GetBlockMouseInput() && Settings_GetMouseToStickEnabled();
                const bool reserved =
                    Settings_GetBlockKeysAllowAltTab() && halljoy::block_keys::IsAltOrTab(hid);
                const bool enabled = Settings_GetBlockBoundKeys();
                const bool bound = halljoy::block_keys::IsWindowsKeyBound(hid, Bindings_IsHidBound);
                const bool block = halljoy::block_keys::ShouldBlock(paused, enabled,
                    IsOwnForegroundWindow(), rescueShift, reserved, bound);
                const bool suppressed = g_blockPressRoutes.Filter(hid, isDown, block);
                if (bound) halljoy::input_path::Add(enabled, suppressed ?
                    halljoy::input_path::BoundBlocked : halljoy::input_path::BoundPassed);
                if (suppressed) return 1;
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static LRESULT CALLBACK MouseBlockHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (g_engineUiInputPassThrough.load(std::memory_order_acquire))
        return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
    if (nCode == HC_ACTION && lParam)
    {
        const MSLLHOOKSTRUCT* m = (const MSLLHOOKSTRUCT*)lParam;
        if ((m->flags & LLMHF_INJECTED) == 0)
        {
            switch (wParam)
            {
            case WM_LBUTTONDOWN: Backend_SetMouseBindButtonState(kMouseBindHidLButton, true); break;
            case WM_LBUTTONUP: Backend_SetMouseBindButtonState(kMouseBindHidLButton, false); break;
            case WM_RBUTTONDOWN: Backend_SetMouseBindButtonState(kMouseBindHidRButton, true); break;
            case WM_RBUTTONUP: Backend_SetMouseBindButtonState(kMouseBindHidRButton, false); break;
            case WM_MBUTTONDOWN: Backend_SetMouseBindButtonState(kMouseBindHidMButton, true); break;
            case WM_MBUTTONUP: Backend_SetMouseBindButtonState(kMouseBindHidMButton, false); break;
            case WM_XBUTTONDOWN:
            {
                WORD xb = HIWORD(m->mouseData);
                if (xb == XBUTTON1) Backend_SetMouseBindButtonState(kMouseBindHidX1, true);
                else if (xb == XBUTTON2) Backend_SetMouseBindButtonState(kMouseBindHidX2, true);
                break;
            }
            case WM_XBUTTONUP:
            {
                WORD xb = HIWORD(m->mouseData);
                if (xb == XBUTTON1) Backend_SetMouseBindButtonState(kMouseBindHidX1, false);
                else if (xb == XBUTTON2) Backend_SetMouseBindButtonState(kMouseBindHidX2, false);
                break;
            }
            case WM_MOUSEWHEEL:
            {
                short d = GET_WHEEL_DELTA_WPARAM(m->mouseData);
                if (d > 0) Backend_PulseMouseBindWheel(kMouseBindHidWheelUp);
                else if (d < 0) Backend_PulseMouseBindWheel(kMouseBindHidWheelDown);
                break;
            }
            default:
                break;
            }
        }

        bool blockNow = IsMouseBlockingActiveNow();
        UpdateMouseCursorLockState(blockNow);

        if ((m->flags & LLMHF_INJECTED) == 0 && blockNow)
        {
            switch (wParam)
            {
            case WM_MOUSEMOVE:
                return 1;
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                return 1;
            default:
                break;
            }
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

static void RequestSettingsSave(HWND hMainWnd)
{
    SetTimer(hMainWnd, SETTINGS_SAVE_TIMER_ID, SETTINGS_SAVE_TIMER_MS, nullptr);
}

static void ApplyTimingSettings(HWND hMainWnd)
{
    UINT pollMs = std::clamp(Settings_GetPollingMs(), 1u, 20u);
    RealtimeLoop_SetIntervalMs(pollMs);

    // Keep maintenance (hooks, routing, recovery) alive while suspended.
    // This does not change the independent realtime input loop.
    UINT uiMs = IsWindowVisible(hMainWnd) && !IsIconic(hMainWnd) ? Settings_GetUIRefreshMs() : 100u;
    SetTimer(hMainWnd, UI_TIMER_ID, uiMs, nullptr);
}

static void UpdateUiVisibility(HWND hwnd, bool visible)
{
    BackendUI_SetPreviewWindow(visible ? hwnd : nullptr);
    KeychronOnboard_MonitorVisible(visible);
    SetTimer(hwnd, UI_TIMER_ID, visible ? Settings_GetUIRefreshMs() : 100u, nullptr);
    if (visible && g_hPageMain) {
        PostMessageW(g_hPageMain, WM_APP_PROFILES_RESUME_VISUAL, 0, 0);
        KeyboardUI_OnTimerTick(g_hPageMain);
        RedrawWindow(g_hPageMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

static void ResizeChildren(HWND hwnd)
{
    if (!g_hPageMain) return;

    RECT rc{};
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    SetWindowPos(g_hPageMain, nullptr, 0, 0, w, h, SWP_NOZORDER);
}

struct AppBackendStartupProgress
{
    bool backend = true;
    bool realtime = false;
    bool runtimeSupervisor = false;
#if defined(HALLJOY_MAD68PR_NATIVE)
    bool afterRealtime = false;
    bool afterRawInput = false;
#endif
};

static bool AppRollbackBackendStartup(
    const AppBackendStartupProgress& progress,
    const wchar_t* failedStage) noexcept
{
    StabilityTrace_Write(L"WARN", L"app", L"startup.rollback.begin",
        L"failed_stage=%s", failedStage ? failedStage : L"unknown");

    auto poison = [&](const wchar_t* component) noexcept {
        g_immediateProcessExitRequired.store(true, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"app", L"startup.rollback.poisoned",
            L"failed_stage=%s component=%s dependent_cleanup_skipped=1",
            failedStage ? failedStage : L"unknown", component);
        return false;
    };

    // It may stop or recreate realtime/output state, so it must relinquish
    // ownership before any dependency is stopped during rollback.
    if (progress.runtimeSupervisor)
    {
        const auto stopped = RuntimeSupervisor_Stop();
        if (!stopped.RestartSafe()) return poison(L"runtime-supervisor");
    }

#if defined(HALLJOY_MAD68PR_NATIVE)
    if (progress.afterRawInput)
    {
        bool stopped = false;
        try { stopped = NativeAnalogBackends_StopPhase(NativeAnalogStartPhase::AfterRawInput); }
        catch (...) { return poison(L"native-after-raw-input-exception"); }
        StabilityTrace_Write(stopped ? L"INFO" : L"ERROR", L"app", L"startup.rollback.step",
            L"component=native-after-raw-input joined=%d", stopped ? 1 : 0);
        if (!stopped) return poison(L"native-after-raw-input");
    }
    if (progress.afterRealtime)
    {
        bool stopped = false;
        try { stopped = NativeAnalogBackends_StopPhase(NativeAnalogStartPhase::AfterRealtime); }
        catch (...) { return poison(L"native-after-realtime-exception"); }
        StabilityTrace_Write(stopped ? L"INFO" : L"ERROR", L"app", L"startup.rollback.step",
            L"component=native-after-realtime joined=%d", stopped ? 1 : 0);
        if (!stopped) return poison(L"native-after-realtime");
    }
#endif

    if (progress.realtime)
    {
        halljoy::lifecycle::StopResult stopped{};
        try { stopped = RealtimeLoop_Stop(); }
        catch (...) { return poison(L"realtime-exception"); }
        StabilityTrace_Write(stopped.RestartSafe() ? L"INFO" : L"ERROR", L"app", L"startup.rollback.step",
            L"component=realtime joined=%d", stopped.RestartSafe() ? 1 : 0);
        if (!stopped.RestartSafe()) return poison(L"realtime");
    }

    if (progress.backend)
    {
        bool stopped = false;
        try { stopped = Backend_Shutdown(); }
        catch (...) { return poison(L"backend-exception"); }
        StabilityTrace_Write(stopped ? L"INFO" : L"ERROR", L"app", L"startup.rollback.step",
            L"component=backend joined=%d", stopped ? 1 : 0);
        if (!stopped) return poison(L"backend");
    }

    StabilityTrace_Write(L"INFO", L"app", L"startup.rollback.end",
        L"failed_stage=%s restart_safe=1", failedStage ? failedStage : L"unknown");
    return true;
}

static bool AppStartBackendDependents(bool rawInputRegistered, const wchar_t* origin) noexcept
{
    AppBackendStartupProgress progress{};
    StabilityTrace_Write(L"INFO", L"app", L"startup.transaction.begin",
        L"origin=%s", origin ? origin : L"unknown");

    try
    {
        // Acquire cleanup responsibility before Start(): a failed start may still
        // own a partially-created worker that RealtimeLoop_Stop must reap.
        progress.realtime = true;
        if (!RealtimeLoop_Start())
        {
            (void)AppRollbackBackendStartup(progress, L"realtime");
            return false;
        }

#if defined(HALLJOY_MAD68PR_NATIVE)
        progress.afterRealtime = true;
        const NativeAnalogPhaseStartResult afterRealtime =
            NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRealtime);
        StabilityTrace_Write(afterRealtime.TransactionSafe() ? L"INFO" : L"ERROR",
            L"app", L"startup.native_phase",
            L"phase=after_realtime running=%u unavailable=%u required_failures=%u rejected=%u transaction_safe=%d",
            static_cast<unsigned>(afterRealtime.running),
            static_cast<unsigned>(afterRealtime.unavailable),
            static_cast<unsigned>(afterRealtime.requiredFailures),
            static_cast<unsigned>(afterRealtime.rejected),
            afterRealtime.TransactionSafe() ? 1 : 0);

        bool injectedAfterRealtimeFailure = false;
#if defined(HALLJOY_ANALOG_SIMULATOR)
        const wchar_t* commandLine = GetCommandLineW();
        injectedAfterRealtimeFailure = commandLine &&
            wcsstr(commandLine, L"--halljoy-test-native-phase-start-failure") != nullptr;
        if (injectedAfterRealtimeFailure)
        {
            StabilityTrace_Write(L"WARN", L"app", L"test.native_phase_start_failure.injected",
                L"phase=after_realtime simulator_only=1");
        }
#endif
        if (!afterRealtime.TransactionSafe() || injectedAfterRealtimeFailure)
        {
            (void)AppRollbackBackendStartup(progress, L"native-after-realtime");
            return false;
        }

        if (!rawInputRegistered)
        {
            (void)AppRollbackBackendStartup(progress, L"raw-input-registration");
            return false;
        }

        progress.afterRawInput = true;
        const NativeAnalogPhaseStartResult afterRawInput =
            NativeAnalogBackends_StartPhase(NativeAnalogStartPhase::AfterRawInput);
        StabilityTrace_Write(afterRawInput.TransactionSafe() ? L"INFO" : L"ERROR",
            L"app", L"startup.native_phase",
            L"phase=after_raw_input running=%u unavailable=%u required_failures=%u rejected=%u transaction_safe=%d",
            static_cast<unsigned>(afterRawInput.running),
            static_cast<unsigned>(afterRawInput.unavailable),
            static_cast<unsigned>(afterRawInput.requiredFailures),
            static_cast<unsigned>(afterRawInput.rejected),
            afterRawInput.TransactionSafe() ? 1 : 0);
        if (!afterRawInput.TransactionSafe())
        {
            (void)AppRollbackBackendStartup(progress, L"native-after-raw-input");
            return false;
        }
#else
        (void)rawInputRegistered;
#endif

        // The UI timer is not a lifecycle owner. Once all input/output
        // dependents exist, a dedicated bounded worker owns their recovery.
        progress.runtimeSupervisor = true;
        if (!RuntimeSupervisor_Start())
        {
            (void)AppRollbackBackendStartup(progress, L"runtime-supervisor");
            return false;
        }
    }
    catch (...)
    {
        (void)AppRollbackBackendStartup(progress, L"exception");
        return false;
    }

    StabilityTrace_Write(L"INFO", L"app", L"startup.transaction.commit",
        L"origin=%s", origin ? origin : L"unknown");
    return true;
}

static bool EngineRuntimeCloseAdmission(void*, std::uint32_t& nativeError) noexcept
{
    Backend_SetRuntimeAdmission(false);
    g_backendReady.store(false, std::memory_order_release);
    nativeError = ERROR_SUCCESS;
    return true;
}

static bool EngineRuntimeStopSupervisor(void*, std::uint32_t& nativeError) noexcept
{
    const auto stopped = RuntimeSupervisor_Stop();
    if (!stopped.RestartSafe())
    {
        nativeError = stopped.error.native_error ? stopped.error.native_error : ERROR_TIMEOUT;
        return false;
    }
    nativeError = ERROR_SUCCESS;
    return true;
}

static bool EngineRuntimePublishNeutral(void*, std::uint32_t& nativeError) noexcept
{
    Backend_ResetPublishedStateAfterRealtimeFault();
    nativeError = ERROR_SUCCESS;
    return true;
}

static bool EngineRuntimeStopRealtime(void*, std::uint32_t& nativeError) noexcept
{
    const auto stopped = RealtimeLoop_Stop();
    if (!stopped.RestartSafe())
    {
        nativeError = stopped.error.native_error ? stopped.error.native_error : ERROR_TIMEOUT;
        return false;
    }
    nativeError = ERROR_SUCCESS;
    return true;
}

static bool EngineRuntimeReleaseUiInput(void*, std::uint32_t& nativeError) noexcept
{
    // Window shutdown runs on the UI thread. It has already forced pass-through
    // and removed hooks, so posting a request back to that blocked thread would
    // deadlock the owner join.
    if (g_shutdownStarted.load(std::memory_order_acquire))
    {
        nativeError = ERROR_SUCCESS;
        return true;
    }
    return halljoy::engine_runtime::ui_bridge::Execute(
        halljoy::engine_runtime::ui_bridge::Operation::ReleaseInput, nativeError);
}

static bool EngineRuntimeStopNativeProviders(void*, std::uint32_t& nativeError) noexcept
{
    if (!NativeAnalogBackends_StopAll())
    {
        nativeError = ERROR_TIMEOUT;
        return false;
    }
    nativeError = ERROR_SUCCESS;
    return true;
}

static bool EngineRuntimeReleaseBackendLeases(void*, std::uint32_t& nativeError) noexcept
{
    if (!Backend_Shutdown())
    {
        nativeError = ERROR_BUSY;
        return false;
    }
    nativeError = ERROR_SUCCESS;
    return true;
}

// Support log evidence for a Resume that fell back to Paused. Steps:
// 1 reset providers, 2 catalog, 3 backend init, 4 dependency guidance,
// 5 start generation, 6 restore UI input.
static void LogResumeStepFailure(unsigned step, std::uint32_t error) noexcept
{
    SupportLog_Event("engine.resume_step_failed", step, SupportLog_Win32(error));
    DebugLog_Write(L"[engine-runtime] resume step %u failed err=%lu", step, static_cast<unsigned long>(error));
}

static bool EngineRuntimeEnumerateFresh(void*, std::uint32_t& nativeError) noexcept
{
    const bool reset = NativeAnalogBackends_Reset();
    if (!reset || !NativeAnalogBackends_CatalogIsValid())
    {
        nativeError = ERROR_INVALID_DATA;
        LogResumeStepFailure(reset ? 2u : 1u, nativeError);
        return false;
    }
    // A false result means no native protocol is currently present. It is not
    // an error: UAP/Soup may still own a valid fresh universal session.
    {
        halljoy::perf::Scope prepare("native.prepare_routing.all");
        (void)NativeAnalogBackends_PrepareRouting();
    }
    nativeError = ERROR_SUCCESS;
    return true;
}

static bool EngineRuntimeProveCapabilities(void*, std::uint32_t& nativeError) noexcept
{
    if (Backend_Init())
    {
        nativeError = ERROR_SUCCESS;
        return true;
    }
    LogResumeStepFailure(3u, static_cast<std::uint32_t>(Backend_GetLastInitIssues()));
    if (!halljoy::engine_runtime::ui_bridge::Execute(
            halljoy::engine_runtime::ui_bridge::Operation::DependencyGuidance, nativeError))
    {
        LogResumeStepFailure(4u, nativeError);
        return false;
    }
    if (!Backend_Init())
    {
        nativeError = ERROR_DEVICE_NOT_AVAILABLE;
        LogResumeStepFailure(3u, static_cast<std::uint32_t>(Backend_GetLastInitIssues()));
        return false;
    }
    nativeError = ERROR_SUCCESS;
    return true;
}

static bool EngineRuntimeStartFreshGeneration(void*, std::uint32_t& nativeError) noexcept
{
    if (!AppStartBackendDependents(g_rawInputRegistered.load(std::memory_order_acquire),
            L"engine-runtime-owner"))
    {
        nativeError = ERROR_GEN_FAILURE;
        LogResumeStepFailure(5u, nativeError);
        return false;
    }
    nativeError = ERROR_SUCCESS;
    return true;
}

static bool EngineRuntimeRestoreUiInput(void*, std::uint32_t& nativeError) noexcept
{
    const bool ok = halljoy::engine_runtime::ui_bridge::Execute(
        halljoy::engine_runtime::ui_bridge::Operation::RestoreInput, nativeError);
    if (!ok) LogResumeStepFailure(6u, nativeError);
    return ok;
}

static bool EngineRuntimeOpenAdmission(void*, std::uint32_t& nativeError) noexcept
{
    Backend_SetRuntimeAdmission(true);
    g_backendReady.store(true, std::memory_order_release);
    nativeError = ERROR_SUCCESS;
    return true;
}

static bool EngineRuntimeReleaseFailedResume(void*, std::uint32_t& nativeError) noexcept
{
    Backend_SetRuntimeAdmission(false);
    g_backendReady.store(false, std::memory_order_release);
    if (!EngineRuntimeStopSupervisor(nullptr, nativeError) ||
        !EngineRuntimeStopRealtime(nullptr, nativeError))
        return false;
    // Failed Resume can already own a realtime worker; join before resetting
    // state shared with Backend_Tick, just as in the normal Pause transaction.
    Backend_ResetPublishedStateAfterRealtimeFault();
    if (!EngineRuntimeStopNativeProviders(nullptr, nativeError) ||
        !EngineRuntimeReleaseBackendLeases(nullptr, nativeError))
        return false;
    nativeError = ERROR_SUCCESS;
    return true;
}

// Resume requests can be refused while the owner is busy (it finishes a
// transaction under its state lock). A refused request is retried shortly
// and the pause card is refreshed, so its Resume button never stays stuck.
static void RequestEngineResume(HWND hwnd) noexcept
{
    using halljoy::engine_runtime::SubmitStatus;
    const auto status = halljoy::engine_runtime::EngineRuntimeOwner_RequestResume();
    if (status == SubmitStatus::Queued) return;
    if (status == SubmitStatus::Rejected && !g_shutdownStarted.load(std::memory_order_acquire))
    {
        g_resumeWanted.store(true, std::memory_order_release);
        SetTimer(hwnd, ENGINE_RESUME_RETRY_TIMER_ID, 250, nullptr);
    }
    KeyboardUI_OnEngineStateChanged();
}

int App_EngineStartFailure() noexcept
{
    if (!g_resumeWanted.load(std::memory_order_acquire)) return 0;
    return g_resumeRetryAttempts < std::size(kResumeRetryDelaysMs) ? 1 : 2;
}

static void EngineRuntimeStateChanged(void* context) noexcept
{
    const auto state = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot();
    // Paused straight after a Resume phase = the Resume failed and rolled back.
    static std::atomic<std::uint8_t> previous{ static_cast<std::uint8_t>(halljoy::runtime_command::State::Paused) };
    const auto before = static_cast<halljoy::runtime_command::State>(
        previous.exchange(static_cast<std::uint8_t>(state.state)));
    if (state.state == halljoy::runtime_command::State::Paused &&
        (before == halljoy::runtime_command::State::ResumeRequested ||
         before == halljoy::runtime_command::State::Enumerating ||
         before == halljoy::runtime_command::State::ProvingCapabilities ||
         before == halljoy::runtime_command::State::PublishingNeutralGeneration))
    {
        g_resumeWanted.store(true, std::memory_order_release);
        SupportLog_ReportFailure("engine.resume_failed", state.lastNativeError);
    }
    halljoy::pause_hotkey::window.store(static_cast<HWND>(context));
    halljoy::pause_hotkey::generation.store(state.commandGeneration);
    halljoy::pause_hotkey::state.store(state.state==halljoy::runtime_command::State::Active ? halljoy::pause_hotkey::State::Active :
        state.state==halljoy::runtime_command::State::Paused ? halljoy::pause_hotkey::State::Paused : halljoy::pause_hotkey::State::Transition);
    // Analog readers are stopped while paused. A last "held" sample must not
    // hide the next ordinary press, e.g. the Resume key.
    if (state.state == halljoy::runtime_command::State::Paused)
        halljoy::shortcuts::ResetAnalog();
    SupportLog_Event("engine.state", static_cast<unsigned>(state.state),SupportLog_Win32(state.lastNativeError));
    if (state.state == halljoy::runtime_command::State::PauseFaulted)
        SupportLog_ReportFailure("engine.fault", state.lastNativeError);
    SupportLog_SetWindow(static_cast<HWND>(context));
    PostMessageW(static_cast<HWND>(context), WM_APP_ENGINE_RUNTIME_STATE_CHANGED, 0, 0);
}

static halljoy::engine_runtime::OperationsV1 BuildEngineRuntimeOperations(HWND hwnd) noexcept
{
    return {
        hwnd,
        EngineRuntimeCloseAdmission,
        EngineRuntimeStopSupervisor,
        EngineRuntimePublishNeutral,
        EngineRuntimeStopRealtime,
        EngineRuntimeReleaseUiInput,
        EngineRuntimeStopNativeProviders,
        EngineRuntimeReleaseBackendLeases,
        EngineRuntimeEnumerateFresh,
        EngineRuntimeProveCapabilities,
        EngineRuntimeStartFreshGeneration,
        EngineRuntimePublishNeutral,
        EngineRuntimeRestoreUiInput,
        EngineRuntimeOpenAdmission,
        EngineRuntimeReleaseFailedResume,
        EngineRuntimeStateChanged,
    };
}

static void AppShutdownNoThrow(HWND hwnd) noexcept
{
    if (g_shutdownStarted.exchange(true, std::memory_order_acq_rel))
        return;
    // Arm before the first cleanup/logging call. Even a broken HID driver or a
    // poisoned dependency lock cannot leave HallJoy requiring Task Manager.
    if (ArmShutdownWatchdog())
    {
        StabilityTrace_WriteCritical(L"INFO", L"app", L"shutdown.watchdog.armed",
            L"deadline_ms=%lu exit_code=%u",
            static_cast<unsigned long>(kShutdownWatchdogTimeoutMs),
            static_cast<unsigned>(kShutdownWatchdogExitCode));
    }
    else
    {
        StabilityTrace_WriteCritical(L"ERROR", L"app", L"shutdown.watchdog.arm_failed",
            L"native_error=%lu", static_cast<unsigned long>(GetLastError()));
    }
    StabilityTrace_WriteCritical(L"INFO", L"app", L"shutdown.begin",
        L"hwnd_present=%d", hwnd ? 1 : 0);
    DebugLog_Write(L"[app.shutdown] begin hwnd=%p", hwnd);
    g_mouseBlockPauseByRShift.store(false, std::memory_order_relaxed);
    g_engineUiInputPassThrough.store(true, std::memory_order_release);
    UpdateMouseCursorLockState(false);
    halljoy::engine_runtime::ui_bridge::CancelPending();

    BackendUI_SetPreviewWindow(nullptr);
    if (hwnd)
    {
        KillTimer(hwnd, UI_TIMER_ID);
        KillTimer(hwnd, SETTINGS_SAVE_TIMER_ID);
        KillTimer(hwnd, WINDOW_SAVE_TIMER_ID);
        KillTimer(hwnd, WINDOW_REFIT_TIMER_ID);
    }
    g_keyboardHookThread.Stop();
    if (g_hMouseHook)
    {
        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = nullptr;
    }
    const auto ownerStopStart = halljoy::perf::Now();
    const auto ownerStop = halljoy::engine_runtime::EngineRuntimeOwner_Stop();
    halljoy::perf::Span("shutdown.engine_owner_stop", 0, ownerStopStart);
    if (!ownerStop.RestartSafe())
    {
        g_immediateProcessExitRequired.store(true, std::memory_order_release);
        StabilityTrace_WriteCritical(L"ERROR", L"app", L"shutdown.poisoned",
            L"component=engine-runtime-owner state=%u generation=%llu error=%u native_error=%lu",
            static_cast<unsigned>(ownerStop.state),
            static_cast<unsigned long long>(ownerStop.generation.Value()),
            static_cast<unsigned>(ownerStop.error.code),
            static_cast<unsigned long>(ownerStop.error.native_error));
        return;
    }
    {
        // The stop parked the K4 onboard session (or an earlier pause did).
        // One STOP from PARKED returns the keyboard to ordinary mode without
        // waiting for USB re-enumeration, unlike closing an active session.
        halljoy::perf::Scope releaseScope("shutdown.k4_release_parked");
        KeychronOnboard_ReleaseParked();
    }
    const auto overlayStopStart = halljoy::perf::Now();
    const auto overlayStop = OverlayServer_Stop();
    halljoy::perf::Span("shutdown.overlay_stop", 0, overlayStopStart);
    if (!overlayStop.RestartSafe())
    {
        g_immediateProcessExitRequired.store(true, std::memory_order_release);
        return;
    }
    MouseIpc_ShutdownPublisher();
    halljoy::perf::Scope saveScope("shutdown.settings_save");
    if (!SaveSettingsByActiveGlobalProfile())
    {
        StabilityTrace_WriteCritical(L"ERROR", L"app", L"shutdown.settings_save_failed",
            L"active_profile=%s", GlobalProfiles_GetActiveName().c_str());
    }
    if (!halljoy::engine_runtime::ui_bridge::Stop())
    {
        g_immediateProcessExitRequired.store(true, std::memory_order_release);
        return;
    }
    g_backendReady.store(false, std::memory_order_release);
    StabilityTrace_WriteCritical(L"INFO", L"app", L"shutdown.complete");
    DebugLog_Write(L"[app.shutdown] complete");
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (halljoy::perf::Enabled()) {
        switch (msg) {
        case WM_INPUT: halljoy::perf::Mark("msg.input", GET_RAWINPUT_CODE_WPARAM(wParam)); break;
        case WM_NCLBUTTONDOWN: halljoy::perf::Mark("msg.nclbuttondown", wParam); break;
        case WM_ENTERSIZEMOVE: halljoy::perf::Mark("msg.entersizemove"); break;
        case WM_EXITSIZEMOVE: halljoy::perf::Mark("msg.exitsizemove"); break;
        case WM_MOVE: halljoy::perf::Mark("msg.move", static_cast<std::uint64_t>(static_cast<std::int16_t>(LOWORD(lParam)) + 100000)); break;
        default: break;
        }
    }
    if (msg == halljoy::tray::ShowMessage()) {
        g_tray.Restore();
        return halljoy::tray::ShowAcknowledged;
    }
    if (msg == halljoy::tray::TaskbarMessage()) { g_tray.ShellRestarted(); return 0; }
    if (halljoy::perf::Enabled()) {
        if (msg == KeyboardPageMain_PerfCurveSweepMessage()) {
            if (g_hPageMain) PostMessageW(g_hPageMain, msg, wParam, lParam);
            return 0;
        }
    }
    if (msg == halljoy::tray::ExitMessage()) {
        if (KeyboardUI_CloseLayoutEditor()) DestroyWindow(hwnd);
        return 0;
    }
    switch (msg)
    {
    case halljoy::tray::Callback:
        if (lParam == WM_LBUTTONUP || lParam == NIN_SELECT || lParam == NIN_KEYSELECT) g_tray.Restore();
        else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            using halljoy::runtime_command::State;
            using halljoy::tray::Action;
            const auto state = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot().state;
            const bool active = state == State::Active, paused = state == State::Paused;
            const wchar_t* text = active ? L"Pause HallJoy" : paused ? L"Resume HallJoy" :
                state == State::PauseFaulted ? L"Restart required" : L"HallJoy is changing state...";
            const auto action = g_tray.ContextMenu({ text, active || paused, paused, OverlayServer_IsRunning() });
            // A menu pumps messages: honor the selected action, never invert a newer state.
            const auto current = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot().state;
            if ((action == Action::Pause && current == State::Active) ||
                (action == Action::Resume && current == State::Paused))
                SendMessageW(hwnd, WM_APP_ENGINE_RUNTIME_TOGGLE, 0, 0);
            else if (action == Action::StartOverlay || action == Action::StopOverlay) {
                g_cmdStartOverlay = false; // An explicit stop overrides startup/watchdog intent.
                bool ok = true;
                if (action == Action::StopOverlay) {
                    OverlayServer_SetAutoStart(false);
                    ok = OverlayServer_Stop().RestartSafe();
                } else {
                    ok = OverlayServer_IsRunning() || OverlayServer_Start(OverlayServer_GetConfiguredPort());
                    OverlayServer_SetAutoStart(ok);
                }
                RequestSettingsSave(hwnd);
                if (g_hPageInputOverlay) PostMessageW(g_hPageInputOverlay, WM_APP_OVERLAY_STATE_CHANGED, 0, 0);
                if (!ok) {
                    g_tray.Restore();
                    const std::wstring error = L"Input Overlay could not complete the action.\n\n" + OverlayServer_GetLastError();
                    MessageBoxW(hwnd, error.c_str(), L"HallJoy", MB_OK | MB_ICONWARNING);
                }
            }
            else if (action == Action::Exit && KeyboardUI_CloseLayoutEditor()) DestroyWindow(hwnd);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE && Settings_GetMinimizeToTray()) {
            if (!KeyboardUI_CloseLayoutEditor()) return 0;
            SaveMainWindowPlacement(hwnd);
            if (g_tray.Hide()) return 0;
            // No tray available: retain ordinary taskbar minimization.
        }
        break;
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, UiTheme::Brush_WindowBg());
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CREATE:
    {
        DebugLog_Write(L"[app] WM_CREATE");
        halljoy::shortcuts::applicable.store(&ShortcutApplicable);
        halljoy::shortcuts::dispatch.store(&ShortcutDispatch);
        g_tray.Attach(hwnd, reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICON)));
        SetPropW(hwnd, L"HallJoy.TrayLifecycle.v1", reinterpret_cast<HANDLE>(1));
        ChangeWindowMessageFilterEx(hwnd, halljoy::tray::ShowMessage(), MSGFLT_ALLOW, nullptr);
        ChangeWindowMessageFilterEx(hwnd, halljoy::tray::TaskbarMessage(), MSGFLT_ALLOW, nullptr);
        // The build publisher runs unelevated: let it request a normal exit
        // even when HallJoy itself runs as administrator.
        ChangeWindowMessageFilterEx(hwnd, halljoy::tray::ExitMessage(), MSGFLT_ALLOW, nullptr);
        // Perf profiling only (--halljoy-perf-log): the unelevated profiler
        // drives pause/resume cycles. Normal runs keep the default filter.
        if (halljoy::perf::Enabled())
        {
            ChangeWindowMessageFilterEx(hwnd, WM_APP_ENGINE_RUNTIME_TOGGLE, MSGFLT_ALLOW, nullptr);
            ChangeWindowMessageFilterEx(hwnd, WM_SYSCOMMAND, MSGFLT_ALLOW, nullptr);
            ChangeWindowMessageFilterEx(hwnd, KeyboardPageMain_PerfCurveSweepMessage(), MSGFLT_ALLOW, nullptr);
        }
        g_mouseBlockPauseByRShift.store(false, std::memory_order_relaxed);
        g_engineUiInputPassThrough.store(false, std::memory_order_release);
        g_mouseCursorLocked = false;
        if (!halljoy::engine_runtime::ui_bridge::Start(
                hwnd, WM_APP_ENGINE_RUNTIME_UI_OPERATION, EngineRuntimeUiOperationHandler))
        {
            DebugLog_Write(L"[engine-runtime] UI bridge initialization failed err=%lu", GetLastError());
            return -1;
        }
        UiTheme::ApplyToTopLevelWindow(hwnd);

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);

        // Start the engine before building the keyboard page: provider
        // discovery, UAP and ViGEm initialisation (owner thread) then overlap
        // page creation instead of waiting for it. Everything the engine needs
        // (timing, admission gate, raw input) is set up first, as before.
        ApplyTimingSettings(hwnd);
        Backend_SetRuntimeAdmission(false);
        g_backendReady.store(false, std::memory_order_release);

        // Receive digital keyboard state independently of game-input hooks.
        bool rawInputRegistered = false;
        RAWINPUTDEVICE rid[2]{};
        rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        rid[0].dwFlags = RIDEV_INPUTSINK;
        rid[0].hwndTarget = hwnd;
        rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
        rid[1].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
        rid[1].hwndTarget = hwnd;
        if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)))
            DebugLog_Write(L"[app] RegisterRawInputDevices(mouse+keyboard) failed err=%lu", GetLastError());
        else
        {
            rawInputRegistered = true;
#if defined(HALLJOY_AULA_HERO84HE_DIAGNOSTIC) || defined(HALLJOY_DRUNKDEER_DIAGNOSTIC) || defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC)
            DebugLog_Write(L"[app] raw mouse and diagnostic keyboard input registered");
#else
            DebugLog_Write(L"[app] raw mouse and keyboard preview input registered");
#endif
        }

        g_rawInputRegistered.store(rawInputRegistered, std::memory_order_release);
        halljoy::redsquare_probe::SetRawInputAvailable(rawInputRegistered);
#if defined(HALLJOY_AULA_HERO84HE_DIAGNOSTIC)
        AulaHero84HeDiagnostic_NotifyRawInputReady(rawInputRegistered);
#endif
#if defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC)
        Titan68TurboDiagnostic_NotifyRawInputReady(rawInputRegistered);
#endif
        halljoy::perf::Mark("engine.owner_start");
        if (!halljoy::engine_runtime::EngineRuntimeOwner_Start(BuildEngineRuntimeOperations(hwnd)))
        {
            DebugLog_Write(L"[engine-runtime] owner start failed err=%lu", GetLastError());
            return -1;
        }
        if (halljoy::engine_runtime::EngineRuntimeOwner_RequestResume() !=
            halljoy::engine_runtime::SubmitStatus::Queued)
        {
            DebugLog_Write(L"[engine-runtime] initial resume request rejected");
            return -1;
        }

        // Create the main keyboard UI page directly (no top-level tabs anymore)
        const auto createPageStart = halljoy::perf::Now();
        g_hPageMain = KeyboardUI_CreatePage(hwnd, hInst);
        halljoy::perf::Span("ui.create_page", 0, createPageStart);
        if (!g_hPageMain)
        {
            DebugLog_Write(L"[app] KeyboardUI_CreatePage failed");
            MessageBoxW(hwnd, L"Failed to create main UI page.", L"Error", MB_ICONERROR);
            return -1; // abort window creation
        }

        ResizeChildren(hwnd);
        ShowWindow(g_hPageMain, SW_SHOW);


        DebugLog_Write(L"[app] init complete");

        if (OverlayServer_GetAutoStart() || g_cmdStartOverlay)
        {
            uint16_t overlayPort = g_cmdOverlayPort ? g_cmdOverlayPort : OverlayServer_GetConfiguredPort();
            if (!OverlayServer_Start(overlayPort))
                DebugLog_Write(L"[overlay] autostart failed: %s", OverlayServer_GetLastError().c_str());
        }

        return 0;
    }

    case WM_ENTERSIZEMOVE:
        g_windowMoving = true;
        KillTimer(hwnd, WINDOW_SAVE_TIMER_ID);
        return 0;
    case WM_EXITSIZEMOVE:
        g_windowMoving = false;
        SaveMainWindowPlacement(hwnd);
        return 0;
    case WM_DISPLAYCHANGE:
        if (g_windowPlacementReady) SetTimer(hwnd, WINDOW_REFIT_TIMER_ID, 350, nullptr);
        break;
    case WM_SETTINGCHANGE:
        if (g_windowPlacementReady && wParam == SPI_SETWORKAREA)
            SetTimer(hwnd, WINDOW_REFIT_TIMER_ID, 350, nullptr);
        break;
    case WM_QUERYENDSESSION:
        if (!KeyboardUI_CloseLayoutEditor(true)) return FALSE;
        SaveMainWindowPlacement(hwnd);
        return TRUE;
    case WM_ENDSESSION:
        if (wParam) DestroyWindow(hwnd); // OS shutdown must never become hide-to-tray.
        return 0;
    case WM_MOVE:
        if (g_windowPlacementReady && !g_windowMoving && !g_windowApplying)
            SetTimer(hwnd, WINDOW_SAVE_TIMER_ID, 350, nullptr);
        break;
    case WM_SIZE:
        UpdateUiVisibility(hwnd, wParam != SIZE_MINIMIZED && IsWindowVisible(hwnd));
        if (wParam != SIZE_MINIMIZED) ResizeChildren(hwnd);
        if (g_windowPlacementReady && !g_windowMoving && !g_windowApplying)
            SetTimer(hwnd, WINDOW_SAVE_TIMER_ID, 350, nullptr);
        return 0;

    case WM_SHOWWINDOW:
        UpdateUiVisibility(hwnd, wParam != 0 && !IsIconic(hwnd));
        break;

    case WM_INPUT:
    {
        Na87Diagnostic_ObserveRawInput((HRAWINPUT)lParam);
        Mini60Diagnostic_ObserveRawInput((HRAWINPUT)lParam);
        SharkDiagnostic_ObserveRawInput((HRAWINPUT)lParam);
        if (g_engineUiInputPassThrough.load(std::memory_order_acquire) ||
            !Backend_IsRuntimeAdmissionOpen())
            return 0;
        UINT sz = 0;
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &sz, sizeof(RAWINPUTHEADER)) != 0 ||
            sz == 0 || sz > kMaxRawInputPacketBytes)
            return 0;

        static thread_local std::vector<BYTE> s_rawInputBuf;
        if (s_rawInputBuf.size() < sz)
            s_rawInputBuf.resize(sz);
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, s_rawInputBuf.data(), &sz, sizeof(RAWINPUTHEADER)) == (UINT)-1)
            return 0;

        if (sz < sizeof(RAWINPUTHEADER)) return 0;
        RAWINPUT* ri = (RAWINPUT*)s_rawInputBuf.data();
        if (ri->header.dwType == RIM_TYPEMOUSE)
        {
            if (!halljoy::raw_input::ContainsTypedPayload(sz,
                    ri->header.dwSize, offsetof(RAWINPUT, data),
                    sizeof(RAWMOUSE)))
                return 0;
            const RAWMOUSE& rm = ri->data.mouse;
            LONG dx = 0;
            LONG dy = 0;
            if ((rm.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
            {
                dx = rm.lLastX;
                dy = rm.lLastY;
            }
            if (dx != 0 || dy != 0)
                Backend_AddMouseDelta((int)dx, (int)dy);
        }
        else if (ri->header.dwType == RIM_TYPEKEYBOARD)
        {
            if (!halljoy::raw_input::ContainsTypedPayload(sz,
                    ri->header.dwSize, offsetof(RAWINPUT, data),
                    sizeof(RAWKEYBOARD)))
                return 0;
            const RAWKEYBOARD& rk = ri->data.keyboard;
            if (rk.VKey != 0xFFu &&
                !IsSyntheticNumpadShift(rk.MakeCode, (rk.Flags & RI_KEY_E0) != 0))
            {
                const bool extended = (rk.Flags & (RI_KEY_E0 | RI_KEY_E1)) != 0;
                const bool isDown = (rk.Flags & RI_KEY_BREAK) == 0;
                const uint16_t hid = HidFromKeyboardScanCode(rk.MakeCode, extended, rk.VKey);
                halljoy::redsquare_probe::ObserveRawKeyboard(ri->header.hDevice, hid, isDown);
                LogitechRapid_ObserveRawKey(ri->header.hDevice, hid, isDown);
                InputTrace_OsKey(reinterpret_cast<std::uintptr_t>(ri->header.hDevice), hid, isDown);
                if (isDown)
                    halljoy::input_privilege::detector.Digital(hid, false, GetTickCount64());
                // Preview observes physical identity before Num Lock/VK aliases
                // and independently of gameplay admission or optional hooks.
                try
                {
                    const auto device = reinterpret_cast<std::uintptr_t>(ri->header.hDevice);
                    // Pause has no reliable break event in the Windows stream.
                    if (hid == 72)
                    {
                        if (isDown)
                            halljoy::digital_keyboard::state.ObservePulse(device, hid, GetTickCount64() + 150);
                    }
                    else
                        halljoy::digital_keyboard::state.Observe(device, hid, isDown);
                }
                catch (...)
                {
                    halljoy::digital_keyboard::state.Reset();
                }
#if defined(HALLJOY_AULA_HERO84HE_DIAGNOSTIC)
                (void)isDown;
                AulaHero84HeDiagnostic_RecordRawKeyboardEvent(
                    reinterpret_cast<std::uintptr_t>(ri->header.hDevice), hid,
                    rk.MakeCode, rk.Flags, rk.VKey);
#elif defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC)
                (void)isDown;
                Titan68TurboDiagnostic_RecordRawKeyboardEvent(
                    reinterpret_cast<std::uintptr_t>(ri->header.hDevice), hid,
                    rk.MakeCode, rk.Flags, rk.VKey);
#elif defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
                DrunkDeerDiagnostic_RecordRawKeyboardEvent(
                    reinterpret_cast<std::uintptr_t>(ri->header.hDevice), hid,
                    rk.MakeCode, rk.Flags, rk.VKey);
                if (hid != 0 && IsMad68RawKeyboard(ri->header.hDevice))
                    Mad68ProR_NotifyKeyboardEvent(hid, isDown, false);
#elif defined(HALLJOY_MCHOSE_ACE68_DIAGNOSTIC)
                (void)isDown;
                MchoseAce68Diagnostic_RecordRawKeyboardEvent(
                    reinterpret_cast<std::uintptr_t>(ri->header.hDevice), hid,
                    rk.MakeCode, rk.Flags, rk.VKey);
#elif defined(HALLJOY_MAD68PR_NATIVE)
                if (hid != 0)
                    Mad68ProR_NotifyKeyboardEvent(hid, isDown, false);
#endif
            }
        }
        return 0;
    }

    case WM_INPUT_DEVICE_CHANGE:
    {
        const HANDLE changed = reinterpret_cast<HANDLE>(lParam);
        if (wParam == GIDC_REMOVAL)
            halljoy::redsquare_probe::RawKeyboardRemoved(changed);
        Mini60Diagnostic_DeviceChanged(changed);
        SharkDiagnostic_DeviceChanged(changed);
        if (wParam == GIDC_REMOVAL)
            halljoy::digital_keyboard::state.Remove(reinterpret_cast<std::uintptr_t>(changed));
#if defined(HALLJOY_MAD68PR_NATIVE)
#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
        DrunkDeerDiagnostic_RecordRawDeviceChange(
            reinterpret_cast<std::uintptr_t>(changed),
            wParam == GIDC_ARRIVAL);
#endif
        bool target = false;
        const auto cached = g_mad68RawKeyboardCache.find(changed);
        if (cached != g_mad68RawKeyboardCache.end())
            target = cached->second;
        else if (wParam == GIDC_ARRIVAL)
            target = IsMad68RawKeyboard(changed);

        g_mad68RawKeyboardCache.erase(changed);
        if (target)
        {
            DebugLog_Write(L"[mad68pr.rawinput] target keyboard device change kind=%s",
                wParam == GIDC_REMOVAL ? L"removal" : L"arrival");
            Mad68ProR_NotifyKeyboardDeviceReset();
        }
        Mad68ProR_NotifyDeviceChange();
#endif
        return 0;
    }

    case WM_DEVICECHANGE:
        // Debounced: Windows sends several notifications per re-enumeration.
        SetTimer(hwnd, K4_TAKEOVER_TIMER_ID, 700, nullptr);
        NativeLayoutDevices_Invalidate();
        SupportLog_InventoryChanged();
        SupportLog_Event("device.change", wParam);
        if (wParam == DBT_DEVNODES_CHANGED ||
            wParam == DBT_DEVICEARRIVAL ||
            wParam == DBT_DEVICEREMOVECOMPLETE)
        {
            if (g_engineUiInputPassThrough.load(std::memory_order_acquire) ||
                !Backend_IsRuntimeAdmissionOpen())
                return 0;
#if defined(HALLJOY_MAD68PR_NATIVE) || defined(HALLJOY_AULA_HERO84HE_DIAGNOSTIC) || defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC) || defined(HALLJOY_ROG_AZOTH96HE_DIAGNOSTIC)
            // Generic WM_DEVICECHANGE is broadcast for unrelated USB devices too.
            // Protocol modules receive it through the common catalog; the MAD68
            // keyboard-state reset remains tied to WM_INPUT_DEVICE_CHANGE above.
            NativeAnalogBackends_NotifyDeviceChange();
#else
            AddressedAnalog_NotifyDeviceChange();
#endif
            Backend_NotifyDeviceChange();
        }
        return 0;

    case WM_TIMER:
        if (wParam == ENGINE_RESUME_RETRY_TIMER_ID) {
            KillTimer(hwnd, ENGINE_RESUME_RETRY_TIMER_ID);
            if (!g_shutdownStarted.load(std::memory_order_acquire) &&
                g_resumeWanted.load(std::memory_order_acquire) &&
                halljoy::engine_runtime::EngineRuntimeOwner_Snapshot().state == halljoy::runtime_command::State::Paused)
            {
                SupportLog_Event("engine.resume_retry", g_resumeRetryAttempts);
                RequestEngineResume(hwnd);
            }
            KeyboardUI_OnEngineStateChanged();
            return 0;
        }
        if (wParam == K4_TAKEOVER_TIMER_ID) {
            KillTimer(hwnd, K4_TAKEOVER_TIMER_ID);
            const auto engine = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot().state;
            const bool needed = KeychronOnboard_NeedsTakeover();
            if (!needed) g_k4TakeoverAttempts = 0; // resolved or the K4 left: allow a new cycle later
            else if (!g_k4TakeoverPausing && !g_shutdownStarted.load(std::memory_order_acquire) &&
                engine == halljoy::runtime_command::State::Active && g_k4TakeoverAttempts < 2 &&
                GetTickCount64() - g_k4TakeoverLastMs >= 5000) {
                ++g_k4TakeoverAttempts;
                g_k4TakeoverLastMs = GetTickCount64();
                g_k4TakeoverPausing = true;
                SupportLog_Event("k4.onboard_late_takeover", g_k4TakeoverAttempts);
                (void)halljoy::engine_runtime::EngineRuntimeOwner_RequestPause();
            }
            return 0;
        }
        if (wParam == WINDOW_SAVE_TIMER_ID) {
            if (!g_windowMoving) SaveMainWindowPlacement(hwnd);
            else KillTimer(hwnd, WINDOW_SAVE_TIMER_ID);
            return 0;
        }
        if (wParam == WINDOW_REFIT_TIMER_ID) {
            if (g_windowMoving) return 0;
            KillTimer(hwnd, WINDOW_REFIT_TIMER_ID);
            RefitMainWindow(hwnd);
            return 0;
        }
        if (wParam == UI_TIMER_ID)
        {
            const bool uiVisible=IsWindowVisible(hwnd) && !IsIconic(hwnd);
            if (uiVisible) {
            Na87Diagnostic_UpdateWindow(hwnd);
            IrokNa87_UpdateWindow(hwnd);
            }
            KeychronOnboard_MonitorVisible(uiVisible);
#if defined(HALLJOY_AJAZZ_DIAGNOSTIC)
            static const auto diagnosticUiStart=GetTickCount64();
            static bool smokeClosePosted=false;
            if(!smokeClosePosted && wcsstr(GetCommandLineW(),L"--halljoy-ajazz-startup-smoke") &&
                GetTickCount64()-diagnosticUiStart>=5000) {
                smokeClosePosted=true;
                StabilityTrace_Write(L"INFO",L"ajazz-startup",L"smoke.close",L"automatic_test=1");
                PostMessageW(hwnd,WM_CLOSE,0,0);
            }
#endif

            if (uiVisible) {
                Mini60Diagnostic_UpdateWindow(hwnd);
                SharkDiagnostic_UpdateWindow(hwnd);
            }
            uint32_t tick = g_uiTimerTickCount.fetch_add(1u, std::memory_order_relaxed) + 1u;
            if (tick <= 8 || (tick % 120u) == 0u)
                DebugLog_Write(L"[app.timer] ui tick=%u", tick);

#if defined(HALLJOY_MAD68PR_NATIVE)
            // Before any user-issued Pause exists, a late native device may
            // trigger one fresh owner-owned generation. The timer never calls
            // Backend_Init or starts dependents itself.
            const bool mad68PresentNow = Mad68ProR_IsDevicePresent();
            if (!g_enginePauseWasExplicit.load(std::memory_order_acquire) &&
                !g_backendReady && mad68PresentNow && !g_lastMad68PresenceForBackendRetry)
            {
                const auto request = halljoy::engine_runtime::EngineRuntimeOwner_RequestResume();
                DebugLog_Write(L"[mad68pr] late-device owner resume request=%u",
                    static_cast<unsigned>(request));
            }
            g_lastMad68PresenceForBackendRetry = mad68PresentNow;
#endif

            if ((tick % 30u) == 0u &&
                (g_cmdStartOverlay || OverlayServer_GetAutoStart()) &&
                !OverlayServer_IsRunning())
            {
                if (!OverlayServer_Start(OverlayServer_GetConfiguredPort()))
                    DebugLog_Write(L"[app.overlay.watchdog] recovery failed: %s", OverlayServer_GetLastError().c_str());
                else
                    DebugLog_Write(L"[app.overlay.watchdog] recovery succeeded");
            }
            bool traceTick = (tick <= 20u) || ((tick % 120u) == 0u);
            if (traceTick) DebugLog_Write(L"[app.timer] step hooks begin");
            RefreshLowLevelHooks();
            UpdateInputPrivilegeWarning();
            if (traceTick) DebugLog_Write(L"[app.timer] step hooks done");
            if (traceTick) DebugLog_Write(L"[app.timer] step ipc begin");
            PublishMouseIpcState();
            if (traceTick) DebugLog_Write(L"[app.timer] step ipc done");
            if (g_hPageMain)
            {
                if (traceTick) DebugLog_Write(L"[app.timer] step ui begin");
                KeyboardUI_OnTimerTick(g_hPageMain);
                if (traceTick) DebugLog_Write(L"[app.timer] step ui done");
            }
            return 0;
        }
        if (wParam == SETTINGS_SAVE_TIMER_ID)
        {
            KillTimer(hwnd, SETTINGS_SAVE_TIMER_ID);
            if (!SaveSettingsByActiveGlobalProfile())
            {
                StabilityTrace_Write(L"WARN", L"app", L"settings.debounced_save_failed",
                    L"active_profile=%s", GlobalProfiles_GetActiveName().c_str());
            }
            return 0;
        }
        return 0;

    case WM_APP_ANALOG_PREVIEW_READY:
        // Acknowledge first: a producer racing this dispatch may queue one more
        // hint. Read current values, never replay historical frames.
        BackendUI_AcknowledgePreview();
        if(g_hPageMain && IsWindowVisible(hwnd) && !IsIconic(hwnd))
            KeyboardUI_OnAnalogPreview();
        return 0;

    case WM_APP_REQUEST_SAVE:
        RequestSettingsSave(hwnd);
        return 0;

    case WM_APP_BLOCK_TOGGLED:
        {
            GlobalProfiles_SetDirty(true);
            if (g_hPageConfig) PostMessageW(g_hPageConfig, WM_APP_BLOCK_KEYS_CHANGED, 0, 0);
            if (g_hPageGlobal) PostMessageW(g_hPageGlobal, WM_APP + 122, 0, 0);
            RefreshLowLevelHooks();
            RequestSettingsSave(hwnd);
        }
        return 0;

    case WM_APP_PROFILE_SHORTCUT:
        halljoy::profiles::service::OnShortcut(static_cast<halljoy::shortcuts::Action>(wParam));
        return 0;

    case WM_APP_PROFILE_RUNTIME_APPLIED:
        RefreshLowLevelHooks();
        ApplyTimingSettings(hwnd);
        return 0;

    case WM_APP_APPLY_TIMING:
        ApplyTimingSettings(hwnd);
        return 0;

    case WM_ACTIVATEAPP:
        // Shortcut capture belongs to the focused HallJoy window only.
        if (!wParam) App_CancelShortcutCapture(true);
        break;

    case WM_APP_FACTORY_RESET_RESTART:
        g_relaunchAfterExit.store(true, std::memory_order_release);
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;

    case WM_APP_KEYBOARD_LAYOUT_CHANGED:
        if (g_hPageMain && IsWindow(g_hPageMain))
            PostMessageW(g_hPageMain, WM_APP_KEYBOARD_LAYOUT_CHANGED, 0, 0);
        return 0;

    case WM_APP_ENGINE_RUNTIME_UI_OPERATION:
        (void)halljoy::engine_runtime::ui_bridge::Dispatch(static_cast<std::uintptr_t>(wParam));
        return 0;

    case WM_APP_ENGINE_RUNTIME_STATE_CHANGED:
    {
        const auto state = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot().state;
        if (g_k4TakeoverPausing &&
            (state == halljoy::runtime_command::State::Paused || state == halljoy::runtime_command::State::PauseFaulted))
        {
            // Our own pause: resume at once, the user never asked for Paused.
            g_k4TakeoverPausing = false;
            if (state == halljoy::runtime_command::State::Paused)
            {
                (void)halljoy::engine_runtime::EngineRuntimeOwner_RequestResume();
                return 0;
            }
        }
        if (state == halljoy::runtime_command::State::Paused) g_tray.SetPaused(true);
        else if (state == halljoy::runtime_command::State::Active) g_tray.SetPaused(false);
        if (state == halljoy::runtime_command::State::Active)
        {
            g_resumeWanted.store(false, std::memory_order_release);
            g_resumeRetryAttempts = 0;
            KillTimer(hwnd, ENGINE_RESUME_RETRY_TIMER_ID);
        }
        else if (state == halljoy::runtime_command::State::Paused &&
            g_resumeWanted.load(std::memory_order_acquire) &&
            !g_shutdownStarted.load(std::memory_order_acquire) &&
            g_resumeRetryAttempts < std::size(kResumeRetryDelaysMs))
        {
            SetTimer(hwnd, ENGINE_RESUME_RETRY_TIMER_ID, kResumeRetryDelaysMs[g_resumeRetryAttempts++], nullptr);
        }
        KeyboardUI_OnEngineStateChanged();
        if (state == halljoy::runtime_command::State::Active && KeychronOnboard_NeedsTakeover())
            SetTimer(hwnd, K4_TAKEOVER_TIMER_ID, 700, nullptr);
        return 0;
    }

    case halljoy::redsquare_probe::StatusMessage:
        if(wParam<=static_cast<WPARAM>(halljoy::redsquare_probe::Status::Fault))
            SetWindowTextW(hwnd,halljoy::redsquare_probe::StatusTitle(
                static_cast<halljoy::redsquare_probe::Status>(wParam)));
        return 0;

    case WM_APP + 363: // Preview Resume is one-way: delayed/double clicks cannot pause again.
        RequestEngineResume(hwnd);
        return 0;

    case halljoy::pause_hotkey::Message:
    {
        using halljoy::pause_hotkey::Action;
        if(g_shutdownStarted.load(std::memory_order_acquire)) return 0;
        const auto snapshot=halljoy::engine_runtime::EngineRuntimeOwner_Snapshot();
        if(snapshot.commandGeneration!=static_cast<std::uint64_t>(lParam) || halljoy::shortcuts::Capturing()) return 0;
        const auto current=snapshot.state;
        if(wParam==static_cast<WPARAM>(Action::Pause) && current==halljoy::runtime_command::State::Active) {
            g_enginePauseWasExplicit.store(true,std::memory_order_release);
            (void)halljoy::engine_runtime::EngineRuntimeOwner_RequestPause();
        } else if(wParam==static_cast<WPARAM>(Action::Resume) && current==halljoy::runtime_command::State::Paused) {
            RequestEngineResume(hwnd);
        }
        return 0;
    }

    case WM_APP_ENGINE_RUNTIME_TOGGLE:
    {
        const auto snapshot = halljoy::engine_runtime::EngineRuntimeOwner_Snapshot();
        if (snapshot.state == halljoy::runtime_command::State::Active)
        {
            g_enginePauseWasExplicit.store(true, std::memory_order_release);
            (void)halljoy::engine_runtime::EngineRuntimeOwner_RequestPause();
        }
        else if (snapshot.state == halljoy::runtime_command::State::Paused)
        {
            RequestEngineResume(hwnd);
        }
        return 0;
    }


    case WM_CLOSE:
        if (!KeyboardUI_CloseLayoutEditor()) return 0;
        if (Settings_GetCloseToTray() && !g_relaunchAfterExit.load(std::memory_order_acquire)) {
            SaveMainWindowPlacement(hwnd);
            if (!g_tray.Hide()) MessageBoxW(hwnd,
                L"The system tray is unavailable. HallJoy will stay open.", L"HallJoy", MB_OK | MB_ICONWARNING);
            return 0;
        }
        StabilityTrace_WriteCritical(L"INFO", L"app", L"window.close",
            L"source=WM_CLOSE");
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        halljoy::redsquare_probe::BindWindow(nullptr);
        RemovePropW(hwnd, L"HallJoy.TrayLifecycle.v1");
        g_tray.Remove();
        GamepadLatency_Stop();
        App_CancelShortcutCapture(false);
        StabilityTrace_WriteCritical(L"INFO", L"app", L"window.destroy",
            L"source=WM_DESTROY");
        DebugLog_Write(L"[app] WM_DESTROY");
        // Start/stop actions persist the preference at the time of the action.
        // Do not erase an enabled autostart preference merely because the
        // accept worker faulted before application shutdown.
        if (!g_cmdStartOverlay && OverlayServer_IsRunning())
            OverlayServer_SetAutoStart(true);

        CaptureMainWindowPlacement(hwnd);
        g_windowPlacementReady = false;

        AppShutdownNoThrow(hwnd);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int App_Run(HINSTANCE hInst, int nCmdShow)
{
    App_ParseCommandLine();

    if (!AppPaths_Initialize())
    {
        StabilityTrace_WriteCritical(L"ERROR", L"storage", L"root.failed",
            L"mode=%ls root=%ls legacy=%ls",
            AppPaths_ModeName(), AppPaths_DataRoot().c_str(), AppPaths_LegacyDataRoot().c_str());
#if defined(HALLJOY_ANALOG_SIMULATOR)
        // The storage migration runner deliberately creates failing temporary
        // roots.  Its result is read from the process code and stability trace;
        // a modal dialog would make that non-interactive test block the user.
        if (wcsstr(GetCommandLineW(), L"--halljoy-test-data-root"))
            return 1;
#endif
        MessageBoxW(nullptr,
            L"HallJoy could not prepare its writable data folder or safely migrate existing settings.\n\n"
            L"No legacy files were deleted. Check folder permissions and try again.",
            L"HallJoy storage error",
            MB_ICONERROR | MB_OK);
        return 1;
    }

#if defined(HALLJOY_ANALOG_SIMULATOR)
    if (wcsstr(GetCommandLineW(), L"--halljoy-test-storage-initialize-only"))
        return 0; // Exercise real migration without UI, profiles or devices.
    if (wcsstr(GetCommandLineW(), L"--halljoy-test-profile-transactions")) {
        extern bool HallJoy_RunProfileTransactionTests();
        const bool passed = HallJoy_RunProfileTransactionTests();
        const uint32_t forbiddenBackendInits = Backend_FileOnlyTestForbiddenInitAttempts();
        if (forbiddenBackendInits != 0)
        {
            StabilityTrace_WriteCritical(L"ERROR", L"profile-test", L"forbidden_backend_init",
                L"attempts=%u", static_cast<unsigned>(forbiddenBackendInits));
        }
        return passed && forbiddenBackendInits == 0 ? 0 : 1;
    }
#endif
    const FactoryResetApplyResult factoryReset = FactoryReset_ApplyPending();
    if (factoryReset.status == FactoryResetApplyStatus::Failed)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"factory-reset", L"startup.failed",
            L"native_error=%lu rollback_complete=%d backup=%ls",
            static_cast<unsigned long>(factoryReset.nativeError),
            factoryReset.rollbackComplete ? 1 : 0, factoryReset.backupRoot.c_str());
#if defined(HALLJOY_ANALOG_SIMULATOR)
        if (wcsstr(GetCommandLineW(), L"--halljoy-test-factory-reset-apply"))
            return 1;
#endif
        if (factoryReset.rollbackComplete)
        {
            MessageBoxW(nullptr,
                L"HallJoy could not safely reset its settings. No partial reset was accepted.\n\n"
                L"Your original files were restored. Check folder permissions and try again.",
                L"HallJoy factory reset",
                MB_ICONERROR | MB_OK);
        }
        else
        {
            std::wstring message =
                L"HallJoy stopped the reset, but Windows also prevented a complete automatic rollback.\n\n"
                L"Do not remove any files. Check this recovery folder and the HallJoy logs:\n";
            message += factoryReset.backupRoot.empty() ? AppPaths_DataRoot() : factoryReset.backupRoot;
            MessageBoxW(nullptr, message.c_str(), L"HallJoy factory reset recovery required",
                MB_ICONERROR | MB_OK);
        }
        return 1;
    }

#if defined(HALLJOY_ANALOG_SIMULATOR)
    if (wcsstr(GetCommandLineW(), L"--halljoy-test-factory-reset-request"))
    {
        DWORD error = ERROR_SUCCESS;
        const bool passed = factoryReset.status == FactoryResetApplyStatus::None &&
            FactoryReset_Request(&error);
        StabilityTrace_Write(passed ? L"INFO" : L"ERROR", L"factory-reset", L"test.request",
            L"passed=%d error=%lu", passed ? 1 : 0, static_cast<unsigned long>(error));
        return passed ? 0 : 1;
    }
    if (wcsstr(GetCommandLineW(), L"--halljoy-test-factory-reset-apply"))
    {
        const bool passed = factoryReset.status == FactoryResetApplyStatus::Applied;
        StabilityTrace_Write(passed ? L"INFO" : L"ERROR", L"factory-reset", L"test.apply",
            L"passed=%d backup=%ls", passed ? 1 : 0, factoryReset.backupRoot.c_str());
        return passed ? 0 : 1;
    }
#endif

#if defined(HALLJOY_ANALOG_SIMULATOR)
    if (wcsstr(GetCommandLineW(), L"--halljoy-test-storage-policy"))
    {
        const bool passed = AppPaths_RunStoragePolicySelfTest();
        StabilityTrace_Write(passed ? L"INFO" : L"ERROR", L"storage", L"policy.self_test",
            L"passed=%d mode=%ls", passed ? 1 : 0, AppPaths_ModeName());
        if (!passed) return 1;
    }
    if (wcsstr(GetCommandLineW(), L"--halljoy-test-mouse-ipc-policy"))
    {
        const bool passed = MouseIpc_RunPolicySelfTest();
        StabilityTrace_Write(passed ? L"INFO" : L"ERROR", L"mouse-ipc", L"policy.self_test",
            L"passed=%d existing_preserved=%d invalid_rejected=%d atomic_reads=1",
            passed ? 1 : 0, passed ? 1 : 0, passed ? 1 : 0);
        if (!passed) return 1;
    }
#endif

    // Real UI run from here on (test modes returned above): the first D3D
    // device and DirectWrite fonts load in parallel with settings, profiles
    // and window creation instead of blocking the first UI frame.
    halljoy::keyboard_canvas::WarmUpAsync();
    const auto profileStart = halljoy::perf::Now();
    const auto startupProfile = GlobalProfiles_InitializeStartup();
    halljoy::perf::Span("app.profiles_startup", 0, profileStart);
    if (startupProfile.firstRun) KeyboardLayout_ArmFirstRunSelection();
    g_profileReadyForAutosave = startupProfile.writable;
    SupportLog_SetInputConfigProvider(InputConfig_Summary);
#if defined(HALLJOY_ANALOG_SIMULATOR)
    if (wcsstr(GetCommandLineW(), L"--halljoy-test-profile-startup-only")) {
        g_profileReadyForAutosave = false;
        const uint32_t forbiddenBackendInits = Backend_FileOnlyTestForbiddenInitAttempts();
        if (forbiddenBackendInits != 0)
        {
            StabilityTrace_WriteCritical(L"ERROR", L"profile-test", L"forbidden_backend_init",
                L"attempts=%u", static_cast<unsigned>(forbiddenBackendInits));
            return 1;
        }
        return 0; // File-only startup probe: never create a window or backend.
    }
#endif

    // The one-command latency test must not depend on a previously saved UI value.
    // Force the highest supported HallJoy realtime cadence for this temporary trace run.
    if (g_cmdLatencyTrace)
    {
        Settings_SetPollingMs(1);
        DebugLog_Write(L"[latency.config] trace=1 forced_polling_ms=1");
    }

    // IMPORTANT:
    // Ensure common controls are registered before we create any TabControl/Trackbar/etc.
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSW wc{};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"WootingVigemGui";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_HALLJOY), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);

    if (!RegisterClassW(&wc))
    {
        DebugLog_Write(L"[app] RegisterClass failed err=%lu", GetLastError());
        return 1;
    }

    UINT dpi = WinUtil_GetSystemDpiCompat();

    int defaultW = MulDiv(821, (int)dpi, 96);
    int defaultH = MulDiv(832, (int)dpi, 96);

    int w = Settings_GetMainWindowWidthPx();
    int h = Settings_GetMainWindowHeightPx();
    if (w <= 0) w = defaultW;
    if (h <= 0) h = defaultH;
    w = halljoy::window_placement::ScaleSize(w, Settings_GetMainWindowDpi(), (int)dpi);
    h = halljoy::window_placement::ScaleSize(h, Settings_GetMainWindowDpi(), (int)dpi);

    int x = Settings_GetMainWindowPosXPx();
    int y = Settings_GetMainWindowPosYPx();
    bool hasSavedPos = (x != std::numeric_limits<int>::min() &&
                        y != std::numeric_limits<int>::min());
    if (!hasSavedPos)
    {
        w = defaultW; h = defaultH;
        POINT cursor{};
        GetCursorPos(&cursor);
        x = cursor.x; y = cursor.y;
    }
    halljoy::window_placement::Rect initial{x, y, w, h};
    if (hasSavedPos && Settings_GetMainWindowPlacementVersion() != 2) {
        RECT legacy = halljoy::window_placement::Native(initial);
        initial = halljoy::window_placement::ConvertWorkspace(initial,
            MonitorFromRect(&legacy, MONITOR_DEFAULTTONEAREST), true);
    }
    initial = halljoy::window_placement::FitToDesktop(initial);
    x = initial.x; y = initial.y; w = initial.w; h = initial.h;

    halljoy::perf::Mark("app.create_window.begin");
    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
#if defined(HALLJOY_INPUT_PATH_DIAGNOSTIC)
        L"HallJoy - Input path diagnostic (automatic logging)",
#else
        L"HallJoy",
#endif
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y,
        w, h,
        nullptr, nullptr, hInst, nullptr);

    halljoy::perf::Mark("app.create_window.end");
    if (!hwnd) { DebugLog_Write(L"[app] CreateWindowEx failed err=%lu", GetLastError()); return 2; }
    g_hMainWnd = hwnd;
    halljoy::redsquare_probe::BindWindow(hwnd);
    BackendUI_SetPreviewWindow(hwnd);
    Na87Diagnostic_Start();
#if !defined(HALLJOY_ATTACKSHARK_NATIVE)
    SharkDiagnostic_Start();
#endif
#if !defined(HALLJOY_AULA_MINI60_NATIVE)
    Mini60Diagnostic_Start();
#endif
    DebugLog_Write(L"[app] main window created hwnd=%p pos=(%d,%d) size=(%d,%d)", hwnd, x, y, w, h);

    if (wc.hIcon)
    {
        DebugLog_Write(L"[app] set big icon begin");
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
        DebugLog_Write(L"[app] set big icon done");
    }
    HICON hSmall = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_SMALL), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    if (hSmall)
    {
        DebugLog_Write(L"[app] set small icon begin");
        SendMessageW(hwnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hSmall);
        DebugLog_Write(L"[app] set small icon done");
    }

    DebugLog_Write(L"[app] ShowWindow begin");
    const bool startMinimized = g_cmdStartMinimized || nCmdShow == SW_SHOWMINIMIZED || nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINNOACTIVE;
    const bool startMaximized = Settings_GetMainWindowMaximized() || nCmdShow == SW_SHOWMAXIMIZED;
    const UINT showCmd = startMinimized ? SW_SHOWMINIMIZED : startMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    if (!startMinimized) {
        halljoy::perf::Scope waitScope("app.wait_canvas_warmup");
        // The engine's UI bridge (posted to this window) keeps being served,
        // so engine readiness does not wait for the first visible frame.
        halljoy::keyboard_canvas::WaitForWarmUp(500, hwnd, WM_APP_ENGINE_RUNTIME_UI_OPERATION);
    }
    if (!halljoy::window_placement::Apply(hwnd, initial, showCmd, startMaximized)) ShowWindow(hwnd, showCmd);
    g_windowPlacementReady = true;
    halljoy::perf::Mark("app.window_shown");
    DebugLog_Write(L"[app] ShowWindow done");
#if !defined(HALLJOY_ANALOG_SIMULATOR)
    if (startupProfile.recovered || !startupProfile.writable) {
        std::wstring message = startupProfile.writable
            ? L"HallJoy recovered the settings it could read and reset any unavailable data.\n\n"
              L"Please check your bindings before playing. Other saved profiles and layouts were not removed."
            : L"HallJoy is running, but Windows prevented safe recovery or saving.\n\n"
              L"Your existing files were not replaced without a backup. Changes in this session will not be saved.";
        if (!startupProfile.backupPath.empty()) message += L"\n\nRecovery files:\n" + startupProfile.backupPath;
        MessageBoxW(hwnd, message.c_str(), L"HallJoy settings recovery", MB_OK | MB_ICONINFORMATION);
    }
#endif

    if (factoryReset.status == FactoryResetApplyStatus::Applied)
    {
        std::wstring message =
            L"All HallJoy settings were reset to defaults.\n\nYour previous settings were backed up to:\n";
        message += factoryReset.backupRoot;
        MessageBoxW(hwnd, message.c_str(), L"HallJoy factory reset", MB_ICONINFORMATION | MB_OK);
    }

    DebugLog_Write(L"[app] RefreshLowLevelHooks begin");
    RefreshLowLevelHooks();
    DebugLog_Write(L"[app] RefreshLowLevelHooks done");

    halljoy::perf::Mark("app.message_loop.enter");
    DebugLog_Write(L"[app] message loop enter");
    MSG msg{};
    uint32_t msgCount = 0;
    while (true)
    {
        BOOL gm = GetMessageW(&msg, nullptr, 0, 0);
        if (gm == -1)
        {
            DebugLog_Write(L"[app] GetMessage failed");
            return 3;
        }
        if (gm == 0)
            break;
        ++msgCount;
        if (msgCount <= 8)
            DebugLog_Write(L"[app] msg[%u] id=0x%04X", msgCount, (unsigned)msg.message);
        DebugLog_SetCheckpoint(
            L"ui: dispatch message 0x%04X wparam=0x%llX",
            (unsigned)msg.message,
            (unsigned long long)msg.wParam);
        // UI-only gate: low-level digital input and user-defined bindings have
        // already been processed independently. No keyboard message reaches a
        // main-window button, slider, tab, combo or implicit profile shortcut.
        if (!halljoy::main_input::Allow(msg,hwnd)) continue;
        TranslateMessage(&msg);
        const auto dispatchStart = halljoy::perf::Enabled() ? halljoy::perf::Now() : 0;
        DispatchMessageW(&msg);
        if (dispatchStart && halljoy::perf::Now() - dispatchStart > 0) {
            static LARGE_INTEGER frequency{};
            if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
            if ((halljoy::perf::Now() - dispatchStart) * 1000 > frequency.QuadPart) { // > 1 ms
                char cls[64]{};
                GetClassNameA(msg.hwnd, cls, sizeof(cls));
                halljoy::perf::Span("ui.dispatch", msg.message, dispatchStart, halljoy::perf::Intern(cls));
            }
        }
        DebugLog_SetCheckpoint(L"ui: message loop idle");
    }

    g_keyboardHookThread.Stop();
    if (g_hMouseHook)
    {
        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = nullptr;
    }
    g_hMainWnd = nullptr;
    halljoy::perf::Mark("app.message_loop.exit");
    DebugLog_Write(L"[app] message loop exit code=%d", (int)msg.wParam);

    return (int)msg.wParam;
}


void App_ForceFinalShutdown() noexcept
{
    AppShutdownNoThrow(g_hMainWnd);
}

void App_DisarmShutdownWatchdog() noexcept
{
    DisarmShutdownWatchdog();
}

bool App_RequiresImmediateProcessExit() noexcept
{
    return g_immediateProcessExitRequired.load(std::memory_order_acquire);
}

bool App_TakeRelaunchRequest() noexcept
{
    return g_relaunchAfterExit.exchange(false, std::memory_order_acq_rel);
}

bool App_RelaunchSelf() noexcept
{
    try
    {
        return RelaunchSelfImpl();
    }
    catch (...)
    {
        return false;
    }
}
