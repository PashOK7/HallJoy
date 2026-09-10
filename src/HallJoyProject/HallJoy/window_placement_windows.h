#pragma once
#include <windows.h>
#include "window_placement_policy.h"

namespace halljoy::window_placement {
inline RECT Native(Rect r) { return {r.x, r.y, r.x + r.w, r.y + r.h}; }
inline Rect FromNative(RECT r) { return {r.left, r.top, r.right - r.left, r.bottom - r.top}; }
inline BOOL CALLBACK CollectWorkArea(HMONITOR monitor, HDC, LPRECT, LPARAM context)
{
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info))
        reinterpret_cast<std::vector<Rect>*>(context)->push_back(FromNative(info.rcWork));
    return TRUE;
}
inline Rect FitToDesktop(Rect r)
{
    std::vector<Rect> areas;
    EnumDisplayMonitors(nullptr, nullptr, CollectWorkArea, reinterpret_cast<LPARAM>(&areas));
    return Fit(r, areas);
}
inline Rect ConvertWorkspace(Rect r, HMONITOR monitor, bool toScreen)
{
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        const int sign = toScreen ? 1 : -1;
        r.x += sign * (info.rcWork.left - info.rcMonitor.left);
        r.y += sign * (info.rcWork.top - info.rcMonitor.top);
    }
    return r;
}
inline bool Capture(HWND window, Rect& normal, bool& maximized)
{
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(window, &wp)) return false;
    normal = ConvertWorkspace(FromNative(wp.rcNormalPosition),
        MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), true);
    maximized = IsZoomed(window) || (IsIconic(window) && (wp.flags & WPF_RESTORETOMAXIMIZED));
    return normal.w > 0 && normal.h > 0;
}
inline bool Apply(HWND window, Rect normal, UINT show, bool restoreMaximized = false)
{
    RECT screen = Native(normal);
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    wp.showCmd = show;
    if (restoreMaximized && show == SW_SHOWMINIMIZED) wp.flags = WPF_RESTORETOMAXIMIZED;
    wp.rcNormalPosition = Native(ConvertWorkspace(normal,
        MonitorFromRect(&screen, MONITOR_DEFAULTTONEAREST), false));
    return SetWindowPlacement(window, &wp) != FALSE;
}
}
