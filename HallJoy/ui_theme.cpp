#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui_theme.h"

#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

namespace UiTheme
{
    // ---- Palette (tweak here) ----
    static constexpr COLORREF C_WINDOW_BG = RGB(18, 18, 18);
    static constexpr COLORREF C_PANEL_BG = RGB(26, 26, 26);
    static constexpr COLORREF C_CONTROL_BG = RGB(34, 34, 34);
    static constexpr COLORREF C_TEXT = RGB(235, 235, 235);
    static constexpr COLORREF C_TEXT_MUTED = RGB(160, 160, 160);
    static constexpr COLORREF C_BORDER = RGB(70, 70, 70);
    static constexpr COLORREF C_ACCENT = RGB(80, 160, 255);

    COLORREF Color_WindowBg() { return C_WINDOW_BG; }
    COLORREF Color_PanelBg() { return C_PANEL_BG; }
    COLORREF Color_ControlBg() { return C_CONTROL_BG; }
    COLORREF Color_Text() { return C_TEXT; }
    COLORREF Color_TextMuted() { return C_TEXT_MUTED; }
    COLORREF Color_Border() { return C_BORDER; }
    COLORREF Color_Accent() { return C_ACCENT; }

    HBRUSH Brush_WindowBg()
    {
        static HBRUSH b = CreateSolidBrush(C_WINDOW_BG);
        return b;
    }
    HBRUSH Brush_PanelBg()
    {
        static HBRUSH b = CreateSolidBrush(C_PANEL_BG);
        return b;
    }
    HBRUSH Brush_ControlBg()
    {
        static HBRUSH b = CreateSolidBrush(C_CONTROL_BG);
        return b;
    }

    // Best-effort dark title bar + border/caption/text colors (Win10/11).
    void ApplyToTopLevelWindow(HWND hwnd)
    {
        if (!hwnd) return;

        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        if (!dwm) return;

        using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
        auto p = (DwmSetWindowAttributeFn)GetProcAddress(dwm, "DwmSetWindowAttribute");
        if (!p)
        {
            FreeLibrary(dwm);
            return;
        }

        // --- Dark title bar ---
        // DWMWA_USE_IMMERSIVE_DARK_MODE:
        // - some builds use 19, newer use 20
        const BOOL on = TRUE;
        p(hwnd, 20, &on, sizeof(on));
        p(hwnd, 19, &on, sizeof(on));

        // --- Non-client colors (Win11, partly Win10 1809+) ---
        // These constants are not always present in older SDK headers.
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

        const COLORREF border = Color_Border();
        const COLORREF caption = Color_WindowBg();
        const COLORREF text = Color_Text();

        // If unsupported by OS, DwmSetWindowAttribute returns an error; we ignore it.
        p(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
        p(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
        p(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));

        FreeLibrary(dwm);
    }

    // Best-effort for common controls (Win10/11).
    void ApplyToControl(HWND hwnd)
    {
        if (!hwnd) return;
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    }
}