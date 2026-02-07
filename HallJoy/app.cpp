// app.cpp
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

#include "app.h"
#include "backend.h"
#include "keyboard_ui.h"
#include "settings.h"
#include "settings_ini.h"
#include "realtime_loop.h"
#include "win_util.h"
#include "app_paths.h"
#include "ui_theme.h"

#pragma comment(lib, "Comctl32.lib")

static constexpr UINT WM_APP_REQUEST_SAVE = WM_APP + 1;

// UI refresh timer
static const UINT_PTR UI_TIMER_ID = 2;

// Debounced settings save timer
static const UINT_PTR SETTINGS_SAVE_TIMER_ID = 3;
static const UINT SETTINGS_SAVE_TIMER_MS = 350;

static HWND g_hPageMain = nullptr;

static void RequestSettingsSave(HWND hMainWnd)
{
    SetTimer(hMainWnd, SETTINGS_SAVE_TIMER_ID, SETTINGS_SAVE_TIMER_MS, nullptr);
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

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
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
        UiTheme::ApplyToTopLevelWindow(hwnd);

        // Load app settings first (settings.ini).
        // NOTE: Curve presets are stored separately and are NOT auto-loaded here.
        if (!SettingsIni_Load(AppPaths_SettingsIni().c_str()))
            SettingsIni_Save(AppPaths_SettingsIni().c_str());

        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);

        // Create the main keyboard UI page directly (no top-level tabs anymore)
        g_hPageMain = KeyboardUI_CreatePage(hwnd, hInst);

        ResizeChildren(hwnd);
        ShowWindow(g_hPageMain, SW_SHOW);

        if (!Backend_Init())
        {
            MessageBoxW(hwnd, L"Failed to init backend (Wooting/ViGEm).", L"Error", MB_ICONERROR);
            PostQuitMessage(1);
            return 0;
        }

        RealtimeLoop_Start();
        RealtimeLoop_SetIntervalMs(Settings_GetPollingMs());

        // Auto-detect refresh rate for UI timer
        int hz = WinUtil_GetMaxRefreshRate();
        UINT interval = (UINT)(1000 / hz);
        if (interval < 1) interval = 1;
        SetTimer(hwnd, UI_TIMER_ID, interval, nullptr);

        return 0;
    }

    case WM_SIZE:
        ResizeChildren(hwnd);
        return 0;

    case WM_DEVICECHANGE:
        Backend_NotifyDeviceChange();
        return 0;

    case WM_TIMER:
        if (wParam == UI_TIMER_ID)
        {
            if (g_hPageMain)
                KeyboardUI_OnTimerTick(g_hPageMain);
            return 0;
        }
        if (wParam == SETTINGS_SAVE_TIMER_ID)
        {
            KillTimer(hwnd, SETTINGS_SAVE_TIMER_ID);
            SettingsIni_Save(AppPaths_SettingsIni().c_str());
            return 0;
        }
        return 0;

    case WM_APP_REQUEST_SAVE:
        RequestSettingsSave(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, UI_TIMER_ID);
        KillTimer(hwnd, SETTINGS_SAVE_TIMER_ID);

        SettingsIni_Save(AppPaths_SettingsIni().c_str());

        RealtimeLoop_Stop();
        Backend_Shutdown();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int App_Run(HINSTANCE hInst, int nCmdShow)
{
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

    if (!RegisterClassW(&wc))
        return 1;

    UINT dpi = WinUtil_GetSystemDpiCompat();

    // 820 x 750 size
    int w = MulDiv(820, (int)dpi, 96);
    int h = MulDiv(750, (int)dpi, 96);

    HWND hwnd = CreateWindowW(
        wc.lpszClassName,
        L"HallJoy",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        w, h,
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd) return 2;

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}