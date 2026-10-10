#pragma once
#include <windows.h>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include "game_profiles.h"
#include "input_shortcuts_runtime.h"
#include "keychron_onboard_backend.h"
#include "keychron_onboard_client.h"
#include "keychron_onboard_host_profile.h"
#include "backend.h"
#include "settings.h"
#include "ui_theme.h"
#include "analog_key_codes.h"

// Application-wide game profile service (UI thread). It owns the game
// associations, the automatic switching policy and the profile shortcuts, and
// keeps working while the Profiles tab is closed or the window is hidden.
// Design: docs/current/GAME_PROFILES_V2_2026-10-03.md.
namespace halljoy::profiles::service {

enum class Reason { User, Automatic, Shortcut };
struct RecentApp { std::wstring exe, title; };

constexpr UINT MsgFocus = WM_APP + 30;
constexpr UINT MsgGameExit = WM_APP + 31;
constexpr UINT_PTR FocusTimer = 1;
constexpr std::size_t kRecentApps = 8;

inline Session session;
inline Policy policy;
inline HWND window = nullptr;                 // message-only window
inline HWND mainWindow = nullptr;
inline HWINEVENTHOOK focusHook = nullptr, titleHook = nullptr;
inline HANDLE gameProcess = nullptr, gameWait = nullptr;
inline std::uint32_t waitedPid = 0;
inline std::vector<RecentApp> recent;
inline std::wstring slotProfiles[shortcuts::kProfileSlots];
// UI refresh after any change; `runtime` is true when a profile was applied.
inline std::function<void(bool runtime)> onChanged;
inline std::wstring lastNotice;

inline std::wstring ProcessPath(DWORD pid) {
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return {};
    std::wstring b(32768, L'\0');
    DWORD n = static_cast<DWORD>(b.size());
    const bool ok = QueryFullProcessImageNameW(p, 0, b.data(), &n) != FALSE;
    CloseHandle(p);
    if (!ok) return {};
    b.resize(n);
    return b;
}
inline std::wstring WindowTitle(HWND w) {
    wchar_t t[256]{};
    GetWindowTextW(w, t, 256);
    return t;
}

// --- Notification popup -----------------------------------------------------
namespace notice {
inline HWND popup = nullptr;
inline std::wstring text;
constexpr UINT_PTR HideTimer = 1;
inline LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(h, &ps);
        RECT r{}; GetClientRect(h, &r);
        HBRUSH border = CreateSolidBrush(UiTheme::Color_Accent());
        FillRect(dc, &r, border); DeleteObject(border);
        RECT inner{ r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
        FillRect(dc, &inner, UiTheme::Brush_PanelBg());
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, UiTheme::Color_Text());
        HGDIOBJ old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        RECT tr = inner; tr.left += 14; tr.right -= 14;
        DrawTextW(dc, text.c_str(), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
        SelectObject(dc, old);
        EndPaint(h, &ps); return 0;
    }
    case WM_TIMER: if (w == HideTimer) { KillTimer(h, HideTimer); ShowWindow(h, SW_HIDE); } return 0;
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_DESTROY: popup = nullptr; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
// A non-activating topmost popup in the bottom-right corner of the monitor
// with the focused window. Never takes focus from the game.
inline void Show(const std::wstring& message) {
    text = message;
    if (!popup) {
        WNDCLASSW wc{}; wc.lpfnWndProc = Proc; wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"HallJoyProfileNotice"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        popup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
            wc.lpszClassName, L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, wc.hInstance, nullptr);
        if (!popup) return;
    }
    HMONITOR mon = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(mon, &mi);
    const UINT dpi = GetDpiForWindow(popup) ? GetDpiForWindow(popup) : 96;
    const int w = MulDiv(280, dpi, 96), h = MulDiv(40, dpi, 96), m = MulDiv(16, dpi, 96);
    SetWindowPos(popup, HWND_TOPMOST, mi.rcWork.right - w - m, mi.rcWork.bottom - h - m, w, h,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(popup, nullptr, TRUE);
    SetTimer(popup, HideTimer, 1800, nullptr);
}
inline void Destroy() { if (popup) DestroyWindow(popup); popup = nullptr; }
}

// --- Internal ------------------------------------------------------------------
inline void PublishShortcuts() {
    unsigned slots[shortcuts::kProfileSlots]{};
    for (auto& s : slotProfiles) s.clear();
    for (std::size_t i = 0; i < session.catalog.shortcuts.size() && i < shortcuts::kProfileSlots; ++i) {
        slots[i] = session.catalog.shortcuts[i].shortcut;
        slotProfiles[i] = session.catalog.shortcuts[i].profile;
    }
    shortcuts::SetProfileBindings(session.catalog.nextShortcut, session.catalog.autoShortcut, slots);
}

inline void CALLBACK FocusEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object, LONG, DWORD, DWORD) {
    if (!window) return;
    // Title hook: only the focused window's own title matters.
    if (event == EVENT_OBJECT_NAMECHANGE && (object != OBJID_WINDOW || hwnd != GetForegroundWindow())) return;
    PostMessageW(window, MsgFocus, 0, 0);
}
inline void ArmHooks() {
    if (!focusHook)
        focusHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, FocusEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    const bool wantTitles = session.catalog.automatic && AnyTitleRule(session.catalog);
    if (wantTitles && !titleHook)
        titleHook = SetWinEventHook(EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE, nullptr, FocusEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    else if (!wantTitles && titleHook) { UnhookWinEvent(titleHook); titleHook = nullptr; }
}
inline void ReleaseWait() {
    if (gameWait) { UnregisterWaitEx(gameWait, INVALID_HANDLE_VALUE); gameWait = nullptr; }
    if (gameProcess) { CloseHandle(gameProcess); gameProcess = nullptr; }
    waitedPid = 0;
}
inline void CALLBACK GameExited(PVOID context, BOOLEAN) {
    if (window) PostMessageW(window, MsgGameExit, reinterpret_cast<WPARAM>(context), 0);
}
// Keep exactly one exit registration, for the game the policy tracks.
inline void TrackGame() {
    const std::uint32_t pid = policy.GamePid();
    if (pid == waitedPid) return;
    ReleaseWait();
    if (!pid) return;
    gameProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!gameProcess) { policy.ForgetGame(); return; }
    if (!RegisterWaitForSingleObject(&gameWait, gameProcess, GameExited,
            reinterpret_cast<PVOID>(static_cast<UINT_PTR>(pid)), INFINITE, WT_EXECUTEONLYONCE)) {
        CloseHandle(gameProcess); gameProcess = nullptr; policy.ForgetGame(); return;
    }
    waitedPid = pid;
}

inline void Changed(bool runtime) { if (onChanged) onChanged(runtime); }

inline void Applied(Reason reason) {
    Backend_SetVirtualGamepadCount(Settings_GetVirtualGamepadCount());
    Backend_SetVirtualGamepadsEnabled(Settings_GetVirtualGamepadsEnabled());
    if (mainWindow) {
        SetWindowTextW(mainWindow, (L"HallJoy - " + GlobalProfiles_GetActiveName()).c_str());
        constexpr UINT runtimeApplied = WM_APP + 483, requestSave = WM_APP + 1;
        SendMessageW(mainWindow, runtimeApplied, 0, 0);
        PostMessageW(mainWindow, requestSave, 0, 0);
    }
    if (session.catalog.notify && reason != Reason::User) {
        lastNotice = L"HallJoy profile: " + GlobalProfiles_GetActiveName();
        notice::Show(lastNotice);
    }
    Changed(true);
}

inline bool Switch(const std::wstring& name, Reason reason) {
    if (name.empty()) return false;
    const bool byUser = reason != Reason::Automatic;
    if (Same(name, GlobalProfiles_GetActiveName())) {
        if (byUser && !session.manual) { session.manual = true; Changed(false); }
        return true;
    }
    if (!session.Activate(name, byUser)) { Changed(false); return false; }
    Applied(reason);
    return true;
}

inline Focus CurrentFocus() {
    Focus f;
    HWND fg = GetForegroundWindow();
    if (!fg) { f.ownWindow = true; return f; }
    DWORD pid = 0; GetWindowThreadProcessId(fg, &pid);
    f.pid = pid;
    f.ownWindow = pid == GetCurrentProcessId(); // settings, editor, dialogs, tray menu
    if (f.ownWindow) return f;
    f.exe = ProcessPath(pid);
    f.title = WindowTitle(fg);
    return f;
}

// Returns true when the list changed (a new application, or a new order).
inline bool Remember(const Focus& f) {
    if (f.ownWindow || f.exe.empty()) return false;
    if (!recent.empty() && Same(recent.front().exe, f.exe)) { recent.front().title = f.title; return false; }
    for (auto it = recent.begin(); it != recent.end(); ++it)
        if (Same(it->exe, f.exe)) { recent.erase(it); break; }
    recent.insert(recent.begin(), { f.exe, f.title });
    if (recent.size() > kRecentApps) recent.resize(kRecentApps);
    return true;
}

// A rule without an icon path (added by name, imported, from v1) learns the
// full path the first time its game is in focus.
inline void LearnIconPath(const Focus& f) {
    if (f.ownWindow || f.exe.empty() || session.readOnly) return;
    const int rule = Match(session.catalog, f.exe, f.title);
    if (rule < 0) return;
    const auto& known = session.catalog.games[static_cast<std::size_t>(rule)].path;
    if (Same(known, f.exe)) return;
    auto next = session.catalog;
    next.games[static_cast<std::size_t>(rule)].path = f.exe;
    if (session.Store(std::move(next))) Changed(false);
}

inline void EvaluateFocus() {
    const Focus f = CurrentFocus();
    LearnIconPath(f);
    const bool recentChanged = Remember(f);
    const auto target = policy.OnFocus(session.catalog, f, session.manual);
    TrackGame();
    if (!target.empty()) Switch(target, Reason::Automatic);
    // The Profiles tab suggests recently focused programs: show new ones.
    if (recentChanged) Changed(false);
}

inline LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case MsgFocus: SetTimer(h, FocusTimer, 80, nullptr); return 0;
    case WM_TIMER:
        if (w == FocusTimer) { KillTimer(h, FocusTimer); EvaluateFocus(); return 0; }
        break;
    case MsgGameExit: {
        const auto pid = static_cast<std::uint32_t>(w);
        if (pid == waitedPid) ReleaseWait();
        const auto target = policy.OnGameExit(session.catalog, pid, session.manual);
        TrackGame();
        if (!target.empty()) Switch(target, Reason::Automatic);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

// A profile that needs inputs the onboard (K4) route cannot provide is still
// activated (refusing it could lock the user out of Default and of fixing the
// profile); the onboard controller stays off until the bindings fit, and this
// notice says why.
inline std::wstring OnboardNotice(const std::wstring& name) {
    if (!KeychronOnboard_OwnsOutput()) return {};
    const auto path = GlobalProfiles_GetSettingsPath(name);
    if (!GetPrivateProfileIntW(L"Main", L"VirtualGamepadsEnabled", 1, path.c_str())) return {};
    bool supported = GetPrivateProfileIntW(L"Main", L"VirtualGamepads", 1, path.c_str()) == 1 &&
        GetPrivateProfileIntW(L"Main", L"MouseToStickEnabled", 0, path.c_str()) == 0;
    BindingsSnapshot bindings;
    if (!Profile_PrepareIni(GlobalProfiles_GetBindingsPath(name).c_str(), bindings)) return {};
    // The K4 onboard mapping holds one key per axis direction: several keys on
    // one direction cannot be represented, so the controller stays off.
    // Several keys on one stick direction need K4 firmware r9 (HJP2).
    bool multiKey = false;
    for (const auto& axis : bindings.axes[0]) {
        for (unsigned k = 0; k < HJO_AXIS_KEYS; ++k) {
            supported &= k4_onboard::SlotForHid(axis.minusHids[k]) >= 0 && k4_onboard::SlotForHid(axis.plusHids[k]) >= 0;
            multiKey |= k > 0 && (axis.minusHids[k] != 0 || axis.plusHids[k] != 0);
        }
    }
    for (auto hid : bindings.triggers[0]) supported &= k4_onboard::SlotForHid(hid) >= 0;
    for (const auto& button : bindings.buttons[0])
        for (unsigned hid = 1; hid < keycode::kCount; ++hid)
            if (button[hid / 64] & (uint64_t{ 1 } << (hid % 64))) supported &= k4_onboard::SlotForHid(static_cast<uint16_t>(hid)) >= 0;
    if (multiKey && k4_onboard::K4FirmwareMultiKey().load() != 1)
        return L"Profile activated. This K4 firmware has one key per stick direction. Several keys on one stick "
               L"direction need K4 firmware r9; the controller stays off until the keyboard is updated.";
    if (supported) return {};
    return L"Profile activated. In K4 HE onboard mode the controller stays off until this profile uses "
           L"one gamepad, only K4 keys and no mouse-to-stick; change those bindings to turn it on.";
}

// --- Public API (UI thread) --------------------------------------------------
inline void Start(HWND main) {
    if (window) return;
    mainWindow = main;
    session.Initialize();
    session.activationNotice = OnboardNotice;
    WNDCLASSW wc{}; wc.lpfnWndProc = Proc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"HallJoyGameProfileService";
    RegisterClassW(&wc);
    window = CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    PublishShortcuts();
    ArmHooks();
    if (!focusHook && session.error.empty())
        session.error = L"Foreground window tracking is unavailable. Manual profile selection still works.";
    PostMessageW(window, MsgFocus, 0, 0);
}
inline void Stop() {
    if (focusHook) UnhookWinEvent(focusHook);
    if (titleHook) UnhookWinEvent(titleHook);
    focusHook = titleHook = nullptr;
    ReleaseWait();
    notice::Destroy();
    if (window) DestroyWindow(window);
    window = nullptr;
    unsigned none[shortcuts::kProfileSlots]{};
    shortcuts::SetProfileBindings(0, 0, none);
}

inline bool Automatic() { return session.catalog.automatic; }
inline bool Manual() { return session.manual; }

// The user picked a profile in the window: manual until "Return to automatic".
inline bool UserActivate(const std::wstring& name) { return Switch(name, Reason::User); }

inline void ReturnToAutomatic(Reason reason = Reason::User) {
    session.manual = false;
    const auto target = policy.Reevaluate(session.catalog, false);
    TrackGame();
    if (target.empty() || !Switch(target, reason)) {
        if (reason != Reason::User && session.catalog.notify) {
            lastNotice = L"HallJoy profiles: automatic";
            notice::Show(lastNotice);
        }
        Changed(false);
    }
}

// Every catalog change goes through here: hooks, shortcuts and the current
// choice follow the new rules immediately.
inline bool Store(Catalog next) {
    const bool wasAutomatic = session.catalog.automatic;
    if (!session.Store(std::move(next))) { Changed(false); return false; }
    ArmHooks();
    PublishShortcuts();
    if (session.catalog.automatic && !wasAutomatic) session.manual = false;
    const auto target = policy.Reevaluate(session.catalog, session.manual);
    TrackGame();
    if (!target.empty()) Switch(target, Reason::Automatic);
    Changed(false);
    return true;
}

inline void OnShortcut(shortcuts::Action action) {
    using shortcuts::Action;
    if (action == Action::AutoProfiles) { ReturnToAutomatic(Reason::Shortcut); return; }
    if (action == Action::NextProfile) {
        std::vector<std::wstring> names; GlobalProfiles_List(names);
        if (names.empty()) return;
        std::size_t next = 0;
        for (std::size_t i = 0; i < names.size(); ++i)
            if (Same(names[i], GlobalProfiles_GetActiveName())) next = (i + 1) % names.size();
        Switch(names[next], Reason::Shortcut);
        return;
    }
    const int slot = shortcuts::ProfileSlotIndex(action);
    if (slot >= 0 && !slotProfiles[slot].empty()) Switch(slotProfiles[slot], Reason::Shortcut);
}

// Profile management wrappers keep the catalog, shortcuts and UI in step.
inline bool Rename(const std::wstring& from, const std::wstring& to) {
    const bool wasActive = Same(from, GlobalProfiles_GetActiveName());
    const bool ok = session.Rename(from, to);
    PublishShortcuts();
    if (ok && wasActive) Applied(Reason::User); else Changed(false);
    return ok;
}
inline bool Remove(const std::wstring& name) {
    const bool ok = session.Remove(name);
    PublishShortcuts();
    Changed(false);
    return ok;
}
inline bool Undo() {
    const bool ok = session.Undo();
    if (ok) Applied(Reason::User); else Changed(false);
    return ok;
}

} // namespace halljoy::profiles::service
