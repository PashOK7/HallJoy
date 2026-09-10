#define NOMINMAX
#include "window_placement_policy.h"
#include <cassert>
#include <iostream>
#include <random>
#ifdef _WIN32
#include "window_placement_windows.h"
#endif
using namespace halljoy::window_placement;
static bool Inside(Rect r, Rect a) {
    return r.w > 0 && r.h > 0 && r.x >= a.x && r.y >= a.y &&
        r.x + r.w <= a.x + a.w && r.y + r.h <= a.y + a.h;
}
int main() {
    const std::vector<Rect> monitors{{0, 40, 1920, 1040}, {-1600, -900, 1600, 860}, {2400, 400, 1280, 984}};
    auto r = Fit({-1500, -800, 800, 700}, monitors);
    assert(r.x == -1500 && r.y == -800 && r.w == 800 && r.h == 700);
    r = Fit({2000, -500, 200, 200}, monitors); // In the bounding-box gap.
    assert(Inside(r, monitors[0]));
    r = Fit({1900, -600, 800, 700}, {monitors[0]}); // Tiny edge, hidden titlebar.
    assert(Inside(r, monitors[0]));
    r = Fit({3000, 400, 4000, 2200}, {monitors[0]}); // Removed monitor + oversize.
    assert(r.x == 0 && r.y == 40 && r.w == 1920 && r.h == 1040);
    r = Fit({0, 0, 821, 832}, {{80, 0, 560, 440}}); // Left taskbar, small display.
    assert(r.x == 80 && r.w == 560 && r.h == 440);
    assert(ScaleSize(800, 96, 144) == 1200);
    assert(ScaleSize(1200, 144, 96) == 800);
    assert(ScaleSize(800, 0, 144) == 800); // Legacy DPI unknown: no guessing.
    std::mt19937 rng(42);
    for (int i = 0; i < 20000; ++i) {
        Rect input{int(rng() % 200000) - 100000, int(rng() % 200000) - 100000,
            int(rng() % 10001), int(rng() % 10001)};
        r = Fit(input, monitors);
        bool inside = false;
        for (auto a : monitors) inside |= Inside(r, a);
        assert(inside);
        auto twice = Fit(r, monitors);
        assert(r.x == twice.x && r.y == twice.y && r.w == twice.w && r.h == twice.h);
    }
#ifdef _WIN32
    // Real Win32 states on a private desktop that is NEVER switched to the user.
    const HDESK originalDesktop = GetThreadDesktop(GetCurrentThreadId());
    wchar_t desktopName[64]{};
    swprintf_s(desktopName, L"HallJoyPlacementTest-%lu", GetCurrentProcessId());
    const HDESK testDesktop = CreateDesktopW(desktopName, nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    assert(testDesktop && SetThreadDesktop(testDesktop));
    HWND window = CreateWindowExW(0, L"STATIC", L"HallJoy placement test", WS_OVERLAPPEDWINDOW,
        0, 0, 700, 500, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(window);
    auto target = FitToDesktop({120, 120, 700, 500});
    for (int i = 0; i < 20; ++i) {
        assert(Apply(window, target, SW_HIDE));
        bool maximized = true;
        Rect actual{};
        assert(Capture(window, actual, maximized) && !maximized);
        assert(actual.x == target.x && actual.y == target.y && actual.w == target.w && actual.h == target.h);
        assert(!IsWindowVisible(window));
        target = actual;
    }
    assert(Apply(window, target, SW_SHOWMAXIMIZED));
    Rect actual{};
    bool maximized = false;
    assert(Capture(window, actual, maximized) && maximized);
    assert(actual.x == target.x && actual.y == target.y && actual.w == target.w && actual.h == target.h);
    assert(Apply(window, target, SW_SHOWMINIMIZED, true));
    assert(IsIconic(window) && Capture(window, actual, maximized) && maximized);
    assert(Apply(window, target, SW_SHOWNORMAL));
    assert(Capture(window, actual, maximized) && !maximized && !IsIconic(window));
    assert(actual.x == target.x && actual.y == target.y && actual.w == target.w && actual.h == target.h);
    DestroyWindow(window);
    assert(SetThreadDesktop(originalDesktop));
    CloseDesktop(testDesktop);
#endif
    std::cout << "WINDOW_PLACEMENT_TEST=PASS cases=20000 win32_roundtrips=20 maximized_minimized_restore=PASS\n";
}
