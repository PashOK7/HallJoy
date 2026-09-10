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
constexpr UINT WM_APP_BLOCK_KEYS_CAPTURED = WM_APP + 366;
constexpr UINT WM_APP_BLOCK_KEYS_CANCEL_CAPTURE = WM_APP + 367;
DWORD App_SetBlockKeysHotkey(UINT chord);
DWORD App_BlockKeysHotkeyError();
void App_SetBlockKeysHotkeyCapture(bool capturing);
