#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>

// Simple dark theme module (palette + cached GDI objects + optional dark title bar).
// No dependency on MFC/ATL.

namespace UiTheme
{
    // Call once for each top-level window (enables dark title bar when supported).
    void ApplyToTopLevelWindow(HWND hwnd);

    // Call for common controls (Tab/Trackbar/etc). Best-effort: does nothing harmful on old Windows.
    void ApplyToControl(HWND hwnd);

    // --- Palette ---
    COLORREF Color_WindowBg();
    COLORREF Color_PanelBg();
    COLORREF Color_ControlBg();
    COLORREF Color_Text();
    COLORREF Color_TextMuted();
    COLORREF Color_Border();
    COLORREF Color_Accent();

    // --- Brushes ---
    HBRUSH Brush_WindowBg();
    HBRUSH Brush_PanelBg();
    HBRUSH Brush_ControlBg();
}