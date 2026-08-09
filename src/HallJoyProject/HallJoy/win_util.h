#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

// DPI helpers (safe on older Windows)
UINT WinUtil_GetDpiForWindowCompat(HWND hwnd);
UINT WinUtil_GetSystemDpiCompat();

// Scale a pixel value using window DPI (96 -> unchanged)
int WinUtil_ScalePx(HWND hwnd, int px);

// Paths
std::wstring WinUtil_GetExeDir();
std::wstring WinUtil_BuildPathNearExe(const wchar_t* fileName);

// NEW: Get the highest refresh rate among all active monitors.
// Used to sync UI timer to V-Sync speed (approx).
int WinUtil_GetMaxRefreshRate();