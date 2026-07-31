#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "win_util.h"

#include <string>
#include <vector>
#include <algorithm>

UINT WinUtil_GetDpiForWindowCompat(HWND hwnd)
{
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    using Fn = UINT(WINAPI*)(HWND);
    auto p = (Fn)GetProcAddress(u32, "GetDpiForWindow");
    if (p) return p(hwnd);

    HDC dc = GetDC(hwnd);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(hwnd, dc);

    return (dpi > 0) ? (UINT)dpi : 96;
}

UINT WinUtil_GetSystemDpiCompat()
{
    HDC dc = GetDC(nullptr);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);

    return (dpi > 0) ? (UINT)dpi : 96;
}

int WinUtil_ScalePx(HWND hwnd, int px)
{
    return MulDiv(px, (int)WinUtil_GetDpiForWindowCompat(hwnd), 96);
}

std::wstring WinUtil_GetExeDir()
{
    std::vector<wchar_t> buf(1024);
    for (;;)
    {
        DWORD len = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (len == 0) return L"";

        if (len < buf.size())
        {
            std::wstring p(buf.data(), len);
            size_t slash = p.find_last_of(L"\\/");
            if (slash != std::wstring::npos) p.erase(slash + 1);
            else p.clear();
            return p;
        }
        if (buf.size() > 65536) return L"";
        buf.resize(buf.size() * 2);
    }
}

std::wstring WinUtil_BuildPathNearExe(const wchar_t* fileName)
{
    std::wstring dir = WinUtil_GetExeDir();
    if (!fileName) fileName = L"";
    dir += fileName;
    return dir;
}

// ------------------------------------------------------------
// Refresh Rate helper
// ------------------------------------------------------------
static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
    int* pMaxRate = (int*)dwData;
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMonitor, &mi))
    {
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
        {
            if ((int)dm.dmDisplayFrequency > *pMaxRate)
                *pMaxRate = (int)dm.dmDisplayFrequency;
        }
    }
    return TRUE;
}

int WinUtil_GetMaxRefreshRate()
{
    int maxRate = 60;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&maxRate);
    if (maxRate < 30) maxRate = 30; // Sanity check
    if (maxRate > 1000) maxRate = 1000;
    return maxRate;
}