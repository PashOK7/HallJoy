// settings.cpp
#define NOMINMAX
#include "settings.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

static uint32_t PackDz(int lowM, int highM)
{
    lowM = std::clamp(lowM, 0, 1000);
    highM = std::clamp(highM, 0, 1000);
    return (uint32_t)(lowM & 0xFFFF) | ((uint32_t)(highM & 0xFFFF) << 16);
}

static void UnpackDz(uint32_t p, int& lowM, int& highM)
{
    lowM = (int)(p & 0xFFFFu);
    highM = (int)((p >> 16) & 0xFFFFu);
}

static int ClampM01(int m) { return std::clamp(m, 0, 1000); }

// Input deadzones X (packed low/high to read consistently)
static std::atomic<uint32_t> g_inDzPacked{ PackDz(80, 980) };

// Polling/UI
static std::atomic<UINT> g_pollMs{ 1 };
static std::atomic<UINT> g_uiRefreshMs{ 1 };
static std::atomic<int> g_virtualGamepadCount{ 1 };
static std::atomic<bool> g_virtualGamepadsEnabled{ true };
static std::atomic<int> g_mainWinW{ 0 };
static std::atomic<int> g_mainWinH{ 0 };

// Global curve endpoints (Y)
static std::atomic<int> g_globalAntiDzM{ 0 };
static std::atomic<int> g_globalOutCapM{ 1000 };

// Global Bezier CPs
// Keep defaults consistent with KeyDeadzone{} (key_settings.h):
// cp1_x=0.38, cp1_y=0.33, cp2_x=0.68, cp2_y=0.66
static std::atomic<int> g_globalC1xM{ 380 };
static std::atomic<int> g_globalC1yM{ 330 };
static std::atomic<int> g_globalC2xM{ 680 };
static std::atomic<int> g_globalC2yM{ 660 };

// Global Bezier CP weights (0..1000)
static std::atomic<int> g_globalC1wM{ 1000 };
static std::atomic<int> g_globalC2wM{ 1000 };

// Global curve mode
static std::atomic<UINT> g_globalCurveMode{ 0 };

// Global invert
static std::atomic<bool> g_globalInvert{ false };

// ---------------- NEW: Snappy Joystick ----------------
static std::atomic<bool> g_snappyJoystick{ false };
static std::atomic<bool> g_lastKeyPriority{ false };
static std::atomic<int> g_lastKeyPrioritySensitivityM{ 120 }; // 0.120 default
static std::atomic<bool> g_blockBoundKeys{ true };

// UI sizes (fixed)
static constexpr UINT kRemapButtonSizePx = 43;
static constexpr UINT kDragIconSizePx = 46;
static constexpr UINT kBoundKeyIconPx = 37;
static constexpr bool kBoundIconBacking = false;

// ---------------- Deadzone X ----------------
void Settings_SetInputDeadzoneLow(float v01)
{
    int lowM = (int)lroundf(std::clamp(v01, 0.0f, 0.99f) * 1000.0f);

    uint32_t old = g_inDzPacked.load(std::memory_order_relaxed);
    for (;;)
    {
        int curLowM, curHighM;
        UnpackDz(old, curLowM, curHighM);

        int newLowM = std::clamp(lowM, 0, 990);
        int newHighM = curHighM;
        if (newHighM < newLowM + 10) newHighM = std::clamp(newLowM + 10, 10, 1000);

        uint32_t nw = PackDz(newLowM, newHighM);
        if (g_inDzPacked.compare_exchange_weak(old, nw, std::memory_order_release, std::memory_order_relaxed))
            return;
    }
}

float Settings_GetInputDeadzoneLow()
{
    uint32_t p = g_inDzPacked.load(std::memory_order_acquire);
    int lowM, highM;
    UnpackDz(p, lowM, highM);
    (void)highM;
    return (float)lowM / 1000.0f;
}

void Settings_SetInputDeadzoneHigh(float v01)
{
    int highM = (int)lroundf(std::clamp(v01, 0.01f, 1.0f) * 1000.0f);

    uint32_t old = g_inDzPacked.load(std::memory_order_relaxed);
    for (;;)
    {
        int curLowM, curHighM;
        UnpackDz(old, curLowM, curHighM);

        int newHighM = std::clamp(highM, 10, 1000);
        int newLowM = curLowM;
        if (newHighM < newLowM + 10) newLowM = std::clamp(newHighM - 10, 0, 990);

        uint32_t nw = PackDz(newLowM, newHighM);
        if (g_inDzPacked.compare_exchange_weak(old, nw, std::memory_order_release, std::memory_order_relaxed))
            return;
    }
}

float Settings_GetInputDeadzoneHigh()
{
    uint32_t p = g_inDzPacked.load(std::memory_order_acquire);
    int lowM, highM;
    UnpackDz(p, lowM, highM);
    (void)lowM;
    return (float)highM / 1000.0f;
}

float Settings_ApplyInputDeadzones(float v01)
{
    v01 = std::clamp(v01, 0.0f, 1.0f);

    uint32_t p = g_inDzPacked.load(std::memory_order_acquire);
    int lowM, highM;
    UnpackDz(p, lowM, highM);

    float low = std::clamp((float)lowM / 1000.0f, 0.0f, 0.99f);
    float high = std::clamp((float)highM / 1000.0f, 0.01f, 1.0f);
    if (high <= low + 0.0001f) high = low + 0.0001f;

    if (v01 <= low)  return 0.0f;
    if (v01 >= high) return 1.0f;

    return (v01 - low) / (high - low);
}

// ---------------- Global curve endpoints (Y) ----------------
void Settings_SetInputAntiDeadzone(float v01)
{
    int m = (int)lroundf(std::clamp(v01, 0.0f, 0.99f) * 1000.0f);

    int cap = g_globalOutCapM.load(std::memory_order_relaxed);
    if (m > cap - 10) m = std::max(0, cap - 10);

    g_globalAntiDzM.store(std::clamp(m, 0, 990), std::memory_order_release);
}

float Settings_GetInputAntiDeadzone()
{
    int m = g_globalAntiDzM.load(std::memory_order_acquire);
    m = std::clamp(m, 0, 990);
    return (float)m / 1000.0f;
}

void Settings_SetInputOutputCap(float v01)
{
    int m = (int)lroundf(std::clamp(v01, 0.01f, 1.0f) * 1000.0f);

    int adz = g_globalAntiDzM.load(std::memory_order_relaxed);
    if (m < adz + 10) m = std::min(1000, adz + 10);

    g_globalOutCapM.store(std::clamp(m, 10, 1000), std::memory_order_release);
}

float Settings_GetInputOutputCap()
{
    int m = g_globalOutCapM.load(std::memory_order_acquire);
    m = std::clamp(m, 10, 1000);
    return (float)m / 1000.0f;
}

// ---------------- Global Bezier CPs ----------------
void Settings_SetInputBezierCp1X(float v01)
{
    int m = (int)lroundf(std::clamp(v01, 0.0f, 1.0f) * 1000.0f);
    g_globalC1xM.store(ClampM01(m), std::memory_order_release);
}
float Settings_GetInputBezierCp1X()
{
    return (float)ClampM01(g_globalC1xM.load(std::memory_order_acquire)) / 1000.0f;
}

void Settings_SetInputBezierCp1Y(float v01)
{
    int m = (int)lroundf(std::clamp(v01, 0.0f, 1.0f) * 1000.0f);
    g_globalC1yM.store(ClampM01(m), std::memory_order_release);
}
float Settings_GetInputBezierCp1Y()
{
    return (float)ClampM01(g_globalC1yM.load(std::memory_order_acquire)) / 1000.0f;
}

void Settings_SetInputBezierCp2X(float v01)
{
    int m = (int)lroundf(std::clamp(v01, 0.0f, 1.0f) * 1000.0f);
    g_globalC2xM.store(ClampM01(m), std::memory_order_release);
}
float Settings_GetInputBezierCp2X()
{
    return (float)ClampM01(g_globalC2xM.load(std::memory_order_acquire)) / 1000.0f;
}

void Settings_SetInputBezierCp2Y(float v01)
{
    int m = (int)lroundf(std::clamp(v01, 0.0f, 1.0f) * 1000.0f);
    g_globalC2yM.store(ClampM01(m), std::memory_order_release);
}
float Settings_GetInputBezierCp2Y()
{
    return (float)ClampM01(g_globalC2yM.load(std::memory_order_acquire)) / 1000.0f;
}

// ---------------- Global Bezier CP weights ----------------
void Settings_SetInputBezierCp1W(float v01)
{
    int m = (int)lroundf(std::clamp(v01, 0.0f, 1.0f) * 1000.0f);
    g_globalC1wM.store(ClampM01(m), std::memory_order_release);
}
float Settings_GetInputBezierCp1W()
{
    return (float)ClampM01(g_globalC1wM.load(std::memory_order_acquire)) / 1000.0f;
}

void Settings_SetInputBezierCp2W(float v01)
{
    int m = (int)lroundf(std::clamp(v01, 0.0f, 1.0f) * 1000.0f);
    g_globalC2wM.store(ClampM01(m), std::memory_order_release);
}
float Settings_GetInputBezierCp2W()
{
    return (float)ClampM01(g_globalC2wM.load(std::memory_order_acquire)) / 1000.0f;
}

// ---------------- Global curve mode ----------------
void Settings_SetInputCurveMode(UINT mode)
{
    mode = std::clamp(mode, 0u, 1u);
    g_globalCurveMode.store(mode, std::memory_order_release);
}

UINT Settings_GetInputCurveMode()
{
    return g_globalCurveMode.load(std::memory_order_acquire);
}

// ---------------- Global invert ----------------
void Settings_SetInputInvert(bool on)
{
    g_globalInvert.store(on, std::memory_order_release);
}

bool Settings_GetInputInvert()
{
    return g_globalInvert.load(std::memory_order_acquire);
}

// ---------------- NEW: Snappy Joystick ----------------
void Settings_SetSnappyJoystick(bool on)
{
    g_snappyJoystick.store(on, std::memory_order_release);
}

bool Settings_GetSnappyJoystick()
{
    return g_snappyJoystick.load(std::memory_order_acquire);
}

void Settings_SetLastKeyPriority(bool on)
{
    g_lastKeyPriority.store(on, std::memory_order_release);
}

bool Settings_GetLastKeyPriority()
{
    return g_lastKeyPriority.load(std::memory_order_acquire);
}

void Settings_SetLastKeyPrioritySensitivity(float v01)
{
    int m = (int)lroundf(std::clamp(v01, 0.02f, 0.30f) * 1000.0f);
    g_lastKeyPrioritySensitivityM.store(std::clamp(m, 20, 300), std::memory_order_release);
}

float Settings_GetLastKeyPrioritySensitivity()
{
    int m = g_lastKeyPrioritySensitivityM.load(std::memory_order_acquire);
    m = std::clamp(m, 20, 300);
    return (float)m / 1000.0f;
}

void Settings_SetBlockBoundKeys(bool on)
{
    g_blockBoundKeys.store(on, std::memory_order_release);
}

bool Settings_GetBlockBoundKeys()
{
    return g_blockBoundKeys.load(std::memory_order_acquire);
}

// ---------------- Polling / UI refresh ----------------
void Settings_SetPollingMs(UINT ms)
{
    ms = std::clamp(ms, 1u, 20u);
    g_pollMs.store(ms, std::memory_order_release);
}

UINT Settings_GetPollingMs()
{
    return g_pollMs.load(std::memory_order_acquire);
}

void Settings_SetUIRefreshMs(UINT ms)
{
    ms = std::clamp(ms, 1u, 200u);
    g_uiRefreshMs.store(ms, std::memory_order_release);
}

UINT Settings_GetUIRefreshMs()
{
    return g_uiRefreshMs.load(std::memory_order_acquire);
}

void Settings_SetVirtualGamepadCount(int count)
{
    count = std::clamp(count, 1, 4);
    g_virtualGamepadCount.store(count, std::memory_order_release);
}

int Settings_GetVirtualGamepadCount()
{
    return g_virtualGamepadCount.load(std::memory_order_acquire);
}

void Settings_SetVirtualGamepadsEnabled(bool on)
{
    g_virtualGamepadsEnabled.store(on, std::memory_order_release);
}

bool Settings_GetVirtualGamepadsEnabled()
{
    return g_virtualGamepadsEnabled.load(std::memory_order_acquire);
}

// ---------------- Fixed UI sizes ----------------
void Settings_SetRemapButtonSizePx(UINT) {}
UINT Settings_GetRemapButtonSizePx() { return kRemapButtonSizePx; }

void Settings_SetDragIconSizePx(UINT) {}
UINT Settings_GetDragIconSizePx() { return kDragIconSizePx; }

void Settings_SetBoundKeyIconSizePx(UINT) {}
UINT Settings_GetBoundKeyIconSizePx() { return kBoundKeyIconPx; }

void Settings_SetBoundKeyIconBacking(bool) {}
bool Settings_GetBoundKeyIconBacking() { return kBoundIconBacking; }

void Settings_SetMainWindowWidthPx(int px)
{
    px = std::clamp(px, 0, 10000);
    g_mainWinW.store(px, std::memory_order_release);
}

int Settings_GetMainWindowWidthPx()
{
    return g_mainWinW.load(std::memory_order_acquire);
}

void Settings_SetMainWindowHeightPx(int px)
{
    px = std::clamp(px, 0, 10000);
    g_mainWinH.store(px, std::memory_order_release);
}

int Settings_GetMainWindowHeightPx()
{
    return g_mainWinH.load(std::memory_order_acquire);
}
