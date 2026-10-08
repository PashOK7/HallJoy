#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int App_Run(HINSTANCE hInst, int nCmdShow);
void App_ForceFinalShutdown() noexcept;
void App_DisarmShutdownWatchdog() noexcept;
bool App_RequiresImmediateProcessExit() noexcept;
bool App_TakeRelaunchRequest() noexcept;
bool App_RelaunchSelf() noexcept;
constexpr UINT WM_APP_BLOCK_KEYS_CHANGED = WM_APP + 365;
// Shortcuts use the packed halljoy::shortcuts format (HID key + modifiers).
// ERROR_ALREADY_ASSIGNED: another command already uses the same shortcut.
DWORD App_SetBlockKeysHotkey(UINT shortcut);
DWORD App_ValidatePauseShortcut(unsigned slot, unsigned shortcut);
// Game profile shortcuts: `action` is the halljoy::shortcuts::Action value the
// shortcut is for (its current owner is excluded from the conflict check).
DWORD App_ValidateCommandShortcut(unsigned action, unsigned shortcut);
// Posted to the main window when a game profile shortcut fires (wParam = Action).
constexpr UINT WM_APP_PROFILE_SHORTCUT = WM_APP + 485;
DWORD App_BlockKeysHotkeyError();
// One capture at a time. The owner receives halljoy::shortcuts::kCaptureMessage
// with lParam = packed shortcut or kCaptureCancelled (Esc / focus loss).
void App_BeginShortcutCapture(HWND owner);
void App_CancelShortcutCapture(bool notifyOwner);
void App_EndShortcutCapture(HWND owner);
// Start/Resume of input processing failed (not a user pause):
// 0 none, 1 retrying automatically, 2 automatic retries exhausted.
int App_EngineStartFailure() noexcept;
