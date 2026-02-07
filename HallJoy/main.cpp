#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "app.h"

#pragma comment(lib, "gdiplus.lib")

static void InitDpiAwareness()
{
    HMODULE u32 = GetModuleHandleW(L"user32.dll");

    using SetCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setCtx = (SetCtxFn)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
    if (setCtx)
    {
        // system-aware: scales correctly at startup, without needing WM_DPICHANGED relayout
        setCtx(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
        return;
    }

    // fallback (older Windows)
    using SetAwareFn = BOOL(WINAPI*)();
    auto setAware = (SetAwareFn)GetProcAddress(u32, "SetProcessDPIAware");
    if (setAware) setAware();
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
    InitDpiAwareness();

    // Init GDI+ once for the entire application lifetime
    Gdiplus::GdiplusStartupInput gdiInput;
    ULONG_PTR gdiToken = 0;
    Gdiplus::GdiplusStartup(&gdiToken, &gdiInput, nullptr);

    int result = App_Run(hInst, nCmdShow);

    Gdiplus::GdiplusShutdown(gdiToken);

    return result;
}