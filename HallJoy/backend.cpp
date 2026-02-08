// backend.cpp
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include <ViGEm/Client.h>
#include "wooting-analog-wrapper.h"

#include "backend.h"
#include "bindings.h"
#include "settings.h"
#include "key_settings.h"

// shared curve math (single source of truth with UI)
#include "curve_math.h"

#pragma comment(lib, "setupapi.lib")

static PVIGEM_CLIENT g_client = nullptr;
static constexpr int kMaxVirtualPads = 4;
static std::array<PVIGEM_TARGET, kMaxVirtualPads> g_pads{};
static std::atomic<int> g_virtualPadCount{ 1 };
static std::atomic<bool> g_virtualPadsEnabled{ true };
static int g_connectedPadCount = 0;

static std::array<XUSB_REPORT, kMaxVirtualPads> g_reports{};
static std::array<XUSB_REPORT, kMaxVirtualPads> g_lastSentReports{};
static std::array<DWORD, kMaxVirtualPads> g_lastSentTicks{};
static std::array<uint8_t, kMaxVirtualPads> g_lastSentValid{};

// Thread-safe last-report snapshot (writer: realtime thread, reader: UI thread)
static std::array<std::atomic<uint32_t>, kMaxVirtualPads> g_lastSeq{};
static std::array<XUSB_REPORT, kMaxVirtualPads> g_lastReport{};
static std::array<std::atomic<SHORT>, kMaxVirtualPads> g_lastRX{};

// ---- UI snapshot ----
static std::array<std::atomic<uint16_t>, 256> g_uiAnalogM{}; // filtered output (after curve)
static std::array<std::atomic<uint16_t>, 256> g_uiRawM{};    // NEW: raw input
static std::array<std::atomic<uint64_t>, 4>   g_uiDirty{};   // dirty for filtered only

// list of HID codes to track (provided by UI)
static std::array<uint16_t, 256> g_trackedList{};
static std::atomic<int>          g_trackedCount{ 0 };

// bind-capture state (layout editor)
static std::atomic<bool>         g_bindCaptureEnabled{ false };
static std::atomic<uint32_t>     g_bindCapturedPacked{ 0 }; // low16=hid, high16=rawMilli
static std::atomic<bool>         g_bindHadDown{ false };

// ---- status / reconnect ----
static std::atomic<bool>         g_vigemOk{ false };
static std::atomic<VIGEM_ERROR>  g_vigemLastErr{ VIGEM_ERROR_NONE };
static std::atomic<uint32_t>     g_lastInitIssues{ BackendInitIssue_None };
static std::atomic<bool>         g_reconnectRequested{ false }; // immediate reconnect (settings change)
static std::atomic<bool>         g_deviceChangeReconnectRequested{ false }; // throttled reconnect (WM_DEVICECHANGE)
static std::atomic<ULONGLONG>    g_ignoreDeviceChangeUntilMs{ 0 };
static int                       g_vigemUpdateFailStreak = 0;
static ULONGLONG                 g_lastReconnectAttemptMs = 0;

// ------------------------------------------------------------
// Curve logic (shared with UI via CurveMath)
// ------------------------------------------------------------
struct CurveDef
{
    // Points in normalized space (0..1)
    float x0 = 0.0f, y0 = 0.0f; // Start
    float x1 = 0.0f, y1 = 0.0f; // CP1
    float x2 = 0.0f, y2 = 0.0f; // CP2
    float x3 = 1.0f, y3 = 1.0f; // End

    // CP weights in [0..1] (used only for Smooth mode)
    float w1 = 1.0f;
    float w2 = 1.0f;

    UINT mode = 0; // 0=Smooth (Rational Bezier), 1=Linear (Segments)
    bool invert = false;
};

static float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

static float ApplyCurve_LinearSegments(float x, const CurveDef& c)
{
    // piecewise interpolation between (x0,y0)->(x1,y1)->(x2,y2)->(x3,y3)
    float xa, ya, xb, yb;

    if (x <= c.x1) {
        xa = c.x0; ya = c.y0;
        xb = c.x1; yb = c.y1;
    }
    else if (x <= c.x2) {
        xa = c.x1; ya = c.y1;
        xb = c.x2; yb = c.y2;
    }
    else {
        xa = c.x2; ya = c.y2;
        xb = c.x3; yb = c.y3;
    }

    float denom = (xb - xa);
    if (std::fabs(denom) < 1e-6f) return Clamp01(yb);

    float t = (x - xa) / denom;
    t = std::clamp(t, 0.0f, 1.0f);
    float y = ya + (yb - ya) * t;
    return Clamp01(y);
}

static float ApplyCurve_SmoothRationalBezier(float x, const CurveDef& c)
{
    CurveMath::Curve01 cc{};
    cc.x0 = c.x0; cc.y0 = c.y0;
    cc.x1 = c.x1; cc.y1 = c.y1;
    cc.x2 = c.x2; cc.y2 = c.y2;
    cc.x3 = c.x3; cc.y3 = c.y3;
    cc.w1 = Clamp01(c.w1);
    cc.w2 = Clamp01(c.w2);

    // Solve x(t)=x, return y(t)
    return CurveMath::EvalRationalYForX(cc, x, 18);
}

static CurveDef BuildCurveForHid(uint16_t hid)
{
    CurveDef c{};
    KeyDeadzone ks = KeySettings_Get(hid);

    if (ks.useUnique)
    {
        c.invert = ks.invert;
        c.mode = (UINT)(ks.curveMode == 0 ? 0 : 1);

        c.x0 = ks.low;          c.y0 = ks.antiDeadzone;
        c.x1 = ks.cp1_x;        c.y1 = ks.cp1_y;
        c.x2 = ks.cp2_x;        c.y2 = ks.cp2_y;
        c.x3 = ks.high;         c.y3 = ks.outputCap;

        c.w1 = ks.cp1_w;
        c.w2 = ks.cp2_w;
    }
    else
    {
        c.invert = Settings_GetInputInvert();
        c.mode = Settings_GetInputCurveMode();

        c.x0 = Settings_GetInputDeadzoneLow();
        c.x3 = Settings_GetInputDeadzoneHigh();
        c.y0 = Settings_GetInputAntiDeadzone();
        c.y3 = Settings_GetInputOutputCap();

        c.x1 = Settings_GetInputBezierCp1X();
        c.y1 = Settings_GetInputBezierCp1Y();
        c.x2 = Settings_GetInputBezierCp2X();
        c.y2 = Settings_GetInputBezierCp2Y();

        c.w1 = Settings_GetInputBezierCp1W();
        c.w2 = Settings_GetInputBezierCp2W();
    }

    c.w1 = Clamp01(c.w1);
    c.w2 = Clamp01(c.w2);

    // Clamp Y to [0..1]
    c.y0 = Clamp01(c.y0); c.y1 = Clamp01(c.y1);
    c.y2 = Clamp01(c.y2); c.y3 = Clamp01(c.y3);

    // Enforce X range + monotonic constraints (needed for x(t) solve)
    c.x0 = Clamp01(c.x0);
    c.x3 = Clamp01(c.x3);
    if (c.x3 < c.x0 + 0.01f) c.x3 = std::clamp(c.x0 + 0.01f, 0.01f, 1.0f);

    float minGap = 0.001f;

    c.x1 = std::clamp(c.x1, c.x0, c.x3);
    c.x2 = std::clamp(c.x2, c.x0, c.x3);

    // enforce order slightly for safety
    c.x1 = std::clamp(c.x1, c.x0, c.x3 - minGap);
    c.x2 = std::clamp(c.x2, c.x1, c.x3);

    return c;
}

static float ApplyCurveByHid(uint16_t hid, float x01Raw)
{
    float x01 = Clamp01(x01Raw);

    CurveDef c = BuildCurveForHid(hid);

    if (c.invert) x01 = 1.0f - x01;

    // Range check against endpoints
    if (x01 < c.x0) return 0.0f;
    if (x01 > c.x3) return Clamp01(c.y3);

    if (c.mode == 1) return ApplyCurve_LinearSegments(x01, c);
    return ApplyCurve_SmoothRationalBezier(x01, c);
}

// ------------------------------------------------------------

static void Vigem_Destroy()
{
    if (g_client)
    {
        for (int i = 0; i < g_connectedPadCount; ++i)
        {
            if (g_pads[(size_t)i])
            {
                vigem_target_remove(g_client, g_pads[(size_t)i]);
                vigem_target_free(g_pads[(size_t)i]);
                g_pads[(size_t)i] = nullptr;
            }
        }
    }

    g_connectedPadCount = 0;
    for (int i = 0; i < kMaxVirtualPads; ++i)
        g_lastSentValid[(size_t)i] = 0;

    if (g_client)
    {
        vigem_disconnect(g_client);
        vigem_free(g_client);
        g_client = nullptr;
    }
}

static bool Vigem_Create(int padCount, VIGEM_ERROR* outErr)
{
    padCount = std::clamp(padCount, 1, kMaxVirtualPads);
    if (outErr) *outErr = VIGEM_ERROR_NONE;
    g_client = vigem_alloc();
    if (!g_client) { if (outErr) *outErr = VIGEM_ERROR_BUS_NOT_FOUND; return false; }
    VIGEM_ERROR err = vigem_connect(g_client);
    if (!VIGEM_SUCCESS(err)) { if (outErr) *outErr = err; vigem_free(g_client); g_client = nullptr; return false; }

    g_connectedPadCount = 0;
    for (int i = 0; i < padCount; ++i)
    {
        PVIGEM_TARGET pad = vigem_target_x360_alloc();
        if (!pad)
        {
            if (outErr) *outErr = VIGEM_ERROR_INVALID_TARGET;
            Vigem_Destroy();
            return false;
        }

        err = vigem_target_add(g_client, pad);
        if (!VIGEM_SUCCESS(err))
        {
            if (outErr) *outErr = err;
            vigem_target_free(pad);
            Vigem_Destroy();
            return false;
        }

        g_pads[(size_t)i] = pad;
        g_connectedPadCount = i + 1;
    }

    if (outErr) *outErr = VIGEM_ERROR_NONE;
    return true;
}

static bool Vigem_ReconnectThrottled(bool force = false)
{
    ULONGLONG now = GetTickCount64();
    if (!force && now - g_lastReconnectAttemptMs < 1000) return false;
    g_lastReconnectAttemptMs = now;
    g_vigemUpdateFailStreak = 0;

    // Reconnect itself emits device-change broadcasts. Suppress them briefly so
    // WM_DEVICECHANGE does not trigger reconnect loops.
    g_ignoreDeviceChangeUntilMs.store(now + 1500, std::memory_order_release);
    Vigem_Destroy();

    if (!g_virtualPadsEnabled.load(std::memory_order_acquire))
    {
        g_vigemOk.store(true, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
        return true;
    }

    VIGEM_ERROR err = VIGEM_ERROR_NONE;
    int wantedPads = std::clamp(g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads);
    bool ok = Vigem_Create(wantedPads, &err);
    g_vigemOk.store(ok, std::memory_order_release);
    g_vigemLastErr.store(ok ? VIGEM_ERROR_NONE : err, std::memory_order_release);
    return ok;
}

// Cache: for HID <= 255 read once per tick
struct HidCache
{
    std::array<float, 256> raw{};
    std::array<float, 256> filtered{};
    std::bitset<256> hasRaw{};
    std::bitset<256> hasFiltered{};
};

static float ReadRaw01Cached(uint16_t hidKeycode, HidCache& cache)
{
    if (hidKeycode == 0) return 0.0f;

    if (hidKeycode < 256)
    {
        if (cache.hasRaw.test(hidKeycode))
            return cache.raw[hidKeycode];

        float v = wooting_analog_read_analog(hidKeycode);
        if (!std::isfinite(v)) v = 0.0f;
        v = Clamp01(v);

        cache.raw[hidKeycode] = v;
        cache.hasRaw.set(hidKeycode);
        return v;
    }

    // HID>=256: no caching needed in this project (UI tracks <256 anyway)
    float v = wooting_analog_read_analog(hidKeycode);
    if (!std::isfinite(v)) v = 0.0f;
    return Clamp01(v);
}

static float ReadFiltered01Cached(uint16_t hidKeycode, HidCache& cache)
{
    if (hidKeycode == 0) return 0.0f;

    if (hidKeycode < 256)
    {
        if (cache.hasFiltered.test(hidKeycode))
            return cache.filtered[hidKeycode];

        float raw = ReadRaw01Cached(hidKeycode, cache);
        float filtered = ApplyCurveByHid(hidKeycode, raw);

        cache.filtered[hidKeycode] = filtered;
        cache.hasFiltered.set(hidKeycode);
        return filtered;
    }

    float raw = ReadRaw01Cached(hidKeycode, cache);
    return ApplyCurveByHid(hidKeycode, raw);
}

static SHORT StickFromMinus1Plus1(float x)
{
    x = std::clamp(x, -1.0f, 1.0f);
    return (SHORT)std::lround(x * 32767.0f);
}

static uint8_t TriggerByte01(float v01)
{
    v01 = std::clamp(v01, 0.0f, 1.0f);
    return (uint8_t)std::lround(v01 * 255.0f);
}

static bool Pressed(float v01)
{
    // Simple logic threshold after curve applied
    return v01 >= 0.10f;
}

// ---- Snappy Joystick (SOCD-like) state (backend thread) ----
// One state per axis (LX,LY,RX,RY)
static std::array<std::array<uint8_t, 4>, kMaxVirtualPads> g_snappyPrevMinusDown{};
static std::array<std::array<uint8_t, 4>, kMaxVirtualPads> g_snappyPrevPlusDown{};
static std::array<std::array<int8_t, 4>, kMaxVirtualPads>  g_snappyLastDir{}; // -1 = minus, +1 = plus, 0 = unknown
static std::array<std::array<float, 4>, kMaxVirtualPads>   g_snappyMinusValley{};
static std::array<std::array<float, 4>, kMaxVirtualPads>   g_snappyPlusValley{};

static int AxisIndexSafe(Axis a)
{
    switch (a)
    {
    case Axis::LX: return 0;
    case Axis::LY: return 1;
    case Axis::RX: return 2;
    case Axis::RY: return 3;
    default:       return -1;
    }
}

static float AxisValue_WithConflictModes(int padIndex, Axis a, float minusV, float plusV)
{
    const bool snapStick = Settings_GetSnappyJoystick();
    const bool lastKeyPriority = Settings_GetLastKeyPriority();
    if (!snapStick && !lastKeyPriority)
        return plusV - minusV;

    int idx = AxisIndexSafe(a);
    if (idx < 0 || idx >= 4)
        return plusV - minusV;

    // detect "press" edges using the same semantics as buttons (stable threshold)
    bool minusDown = Pressed(minusV);
    bool plusDown = Pressed(plusV);

    int p = std::clamp(padIndex, 0, kMaxVirtualPads - 1);
    bool prevMinus = (g_snappyPrevMinusDown[(size_t)p][idx] != 0);
    bool prevPlus = (g_snappyPrevPlusDown[(size_t)p][idx] != 0);

    if (minusDown && !prevMinus) g_snappyLastDir[(size_t)p][idx] = -1;
    if (plusDown && !prevPlus)  g_snappyLastDir[(size_t)p][idx] = +1;

    if (lastKeyPriority)
    {
        // Re-trigger threshold for analog "re-press" while key is still logically down.
        // Example: user slightly releases key and presses again without crossing Pressed() threshold.
        const float repDelta = std::clamp(Settings_GetLastKeyPrioritySensitivity(), 0.02f, 0.95f);

        if (!minusDown)
        {
            g_snappyMinusValley[(size_t)p][idx] = 1.0f;
        }
        else if (!prevMinus)
        {
            g_snappyMinusValley[(size_t)p][idx] = minusV;
        }
        else
        {
            float& valley = g_snappyMinusValley[(size_t)p][idx];
            valley = std::min(valley, minusV);
            if ((minusV - valley) >= repDelta)
            {
                g_snappyLastDir[(size_t)p][idx] = -1;
                valley = minusV;
            }
        }

        if (!plusDown)
        {
            g_snappyPlusValley[(size_t)p][idx] = 1.0f;
        }
        else if (!prevPlus)
        {
            g_snappyPlusValley[(size_t)p][idx] = plusV;
        }
        else
        {
            float& valley = g_snappyPlusValley[(size_t)p][idx];
            valley = std::min(valley, plusV);
            if ((plusV - valley) >= repDelta)
            {
                g_snappyLastDir[(size_t)p][idx] = +1;
                valley = plusV;
            }
        }
    }

    g_snappyPrevMinusDown[(size_t)p][idx] = minusDown ? 1u : 0u;
    g_snappyPrevPlusDown[(size_t)p][idx] = plusDown ? 1u : 0u;

    float maxV = std::max(minusV, plusV);
    if (maxV <= 0.0001f)
        return 0.0f;

    if (lastKeyPriority)
    {
        // While only one side is logically pressed, keep output fully bound to that side.
        // This prevents partial cancellation when the opposite side starts moving but
        // has not crossed the press threshold yet.
        if (minusDown && !plusDown) return -minusV;
        if (plusDown && !minusDown) return +plusV;
    }

    // Last Key Priority: when both directions are down, most recent press wins.
    if (lastKeyPriority && minusDown && plusDown)
    {
        int8_t dir = g_snappyLastDir[(size_t)p][idx];
        if (dir == 0)
            dir = (plusV >= minusV) ? +1 : -1;

        float mag = 0.0f;
        if (snapStick)
        {
            // Keep "snap" punch while still honoring last pressed direction.
            mag = maxV;
        }
        else
        {
            mag = (dir > 0) ? plusV : minusV;
        }
        return (dir > 0) ? +mag : -mag;
    }

    // Snap Stick behavior: stronger side wins; if equal, last direction wins.
    if (snapStick)
    {
        constexpr float EQ_EPS = 0.002f; // tolerant equality (float noise)
        float d = plusV - minusV;

        if (std::fabs(d) > EQ_EPS)
            return (d > 0.0f) ? +maxV : -maxV;

        if (g_snappyLastDir[(size_t)p][idx] > 0) return +maxV;
        if (g_snappyLastDir[(size_t)p][idx] < 0) return -maxV;
        return 0.0f;
    }

    return plusV - minusV;
}

static void SetBtn(XUSB_REPORT& report, WORD mask, bool down)
{
    if (down) report.wButtons |= mask;
    else      report.wButtons &= ~mask;
}

static bool BtnPressedFromMask(int padIndex, GameButton b, HidCache& cache)
{
    for (int chunk = 0; chunk < 4; ++chunk)
    {
        uint64_t bits = Bindings_GetButtonMaskChunkForPad(padIndex, b, chunk);
        if (!bits) continue;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        while (bits) {
            unsigned long idx = 0;
            _BitScanForward64(&idx, bits);
            bits &= (bits - 1);
            uint16_t hid = (uint16_t)(chunk * 64 + (int)idx);
            float v01 = ReadFiltered01Cached(hid, cache);
            if (Pressed(v01)) return true;
        }
#else
        for (int bit = 0; bit < 64; ++bit) {
            if (bits & (1ULL << bit)) {
                uint16_t hid = (uint16_t)(chunk * 64 + bit);
                float v01 = ReadFiltered01Cached(hid, cache);
                if (Pressed(v01)) return true;
            }
        }
#endif
    }
    return false;
}

static XUSB_REPORT BuildReportForPad(int padIndex, HidCache& cache)
{
    XUSB_REPORT report{};
    report.wButtons = 0;

    auto applyAxis = [&](Axis a, SHORT& out) {
        AxisBinding b = Bindings_GetAxisForPad(padIndex, a);
        float minusV = ReadFiltered01Cached(b.minusHid, cache);
        float plusV = ReadFiltered01Cached(b.plusHid, cache);
        out = StickFromMinus1Plus1(AxisValue_WithConflictModes(padIndex, a, minusV, plusV));
        };

    applyAxis(Axis::LX, report.sThumbLX);
    applyAxis(Axis::LY, report.sThumbLY);
    applyAxis(Axis::RX, report.sThumbRX);
    applyAxis(Axis::RY, report.sThumbRY);

    report.bLeftTrigger = TriggerByte01(ReadFiltered01Cached(Bindings_GetTriggerForPad(padIndex, Trigger::LT), cache));
    report.bRightTrigger = TriggerByte01(ReadFiltered01Cached(Bindings_GetTriggerForPad(padIndex, Trigger::RT), cache));

    SetBtn(report, XUSB_GAMEPAD_A, BtnPressedFromMask(padIndex, GameButton::A, cache));
    SetBtn(report, XUSB_GAMEPAD_B, BtnPressedFromMask(padIndex, GameButton::B, cache));
    SetBtn(report, XUSB_GAMEPAD_X, BtnPressedFromMask(padIndex, GameButton::X, cache));
    SetBtn(report, XUSB_GAMEPAD_Y, BtnPressedFromMask(padIndex, GameButton::Y, cache));
    SetBtn(report, XUSB_GAMEPAD_LEFT_SHOULDER, BtnPressedFromMask(padIndex, GameButton::LB, cache));
    SetBtn(report, XUSB_GAMEPAD_RIGHT_SHOULDER, BtnPressedFromMask(padIndex, GameButton::RB, cache));
    SetBtn(report, XUSB_GAMEPAD_BACK, BtnPressedFromMask(padIndex, GameButton::Back, cache));
    SetBtn(report, XUSB_GAMEPAD_START, BtnPressedFromMask(padIndex, GameButton::Start, cache));
    SetBtn(report, XUSB_GAMEPAD_GUIDE, BtnPressedFromMask(padIndex, GameButton::Guide, cache));
    SetBtn(report, XUSB_GAMEPAD_LEFT_THUMB, BtnPressedFromMask(padIndex, GameButton::LS, cache));
    SetBtn(report, XUSB_GAMEPAD_RIGHT_THUMB, BtnPressedFromMask(padIndex, GameButton::RS, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_UP, BtnPressedFromMask(padIndex, GameButton::DpadUp, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_DOWN, BtnPressedFromMask(padIndex, GameButton::DpadDown, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_LEFT, BtnPressedFromMask(padIndex, GameButton::DpadLeft, cache));
    SetBtn(report, XUSB_GAMEPAD_DPAD_RIGHT, BtnPressedFromMask(padIndex, GameButton::DpadRight, cache));

    return report;
}

static bool IsReportSignificantlyDifferent(const XUSB_REPORT& a, const XUSB_REPORT& b)
{
    if (a.wButtons != b.wButtons) return true;
    if (std::abs((int)a.bLeftTrigger - (int)b.bLeftTrigger) >= 2) return true;
    if (std::abs((int)a.bRightTrigger - (int)b.bRightTrigger) >= 2) return true;

    if (std::abs((int)a.sThumbLX - (int)b.sThumbLX) >= 256) return true;
    if (std::abs((int)a.sThumbLY - (int)b.sThumbLY) >= 256) return true;
    if (std::abs((int)a.sThumbRX - (int)b.sThumbRX) >= 256) return true;
    if (std::abs((int)a.sThumbRY - (int)b.sThumbRY) >= 256) return true;

    return false;
}

bool Backend_Init()
{
    g_virtualPadCount.store(std::clamp(Settings_GetVirtualGamepadCount(), 1, kMaxVirtualPads), std::memory_order_release);
    g_virtualPadsEnabled.store(Settings_GetVirtualGamepadsEnabled(), std::memory_order_release);
    g_lastInitIssues.store(BackendInitIssue_None, std::memory_order_release);
    g_reconnectRequested.store(false, std::memory_order_release);
    g_deviceChangeReconnectRequested.store(false, std::memory_order_release);
    g_ignoreDeviceChangeUntilMs.store(0, std::memory_order_release);
    g_vigemUpdateFailStreak = 0;

    uint32_t initIssues = BackendInitIssue_None;
    int wootingInit = wooting_analog_initialise();
    if (wootingInit < 0)
    {
        switch ((WootingAnalogResult)wootingInit)
        {
        case WootingAnalogResult_DLLNotFound:
        case WootingAnalogResult_FunctionNotFound:
            initIssues |= BackendInitIssue_WootingSdkMissing;
            break;
        case WootingAnalogResult_NoPlugins:
            initIssues |= BackendInitIssue_WootingNoPlugins;
            break;
        case WootingAnalogResult_IncompatibleVersion:
            initIssues |= BackendInitIssue_WootingIncompatible;
            break;
        default:
            initIssues |= BackendInitIssue_Unknown;
            break;
        }
    }

    if (g_virtualPadsEnabled.load(std::memory_order_acquire))
    {
        // Initial virtual pad creation may also broadcast device changes.
        g_ignoreDeviceChangeUntilMs.store(GetTickCount64() + 1500, std::memory_order_release);
        VIGEM_ERROR err = VIGEM_ERROR_NONE;
        if (!Vigem_Create(g_virtualPadCount.load(std::memory_order_acquire), &err)) {
            g_vigemOk.store(false, std::memory_order_release);
            g_vigemLastErr.store(err, std::memory_order_release);
            if (err == VIGEM_ERROR_BUS_NOT_FOUND)
                initIssues |= BackendInitIssue_VigemBusMissing;
            else
                initIssues |= BackendInitIssue_Unknown;
        }
        else
        {
            g_vigemOk.store(true, std::memory_order_release);
            g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
        }
    }
    else
    {
        g_vigemOk.store(true, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
    }

    if (initIssues != BackendInitIssue_None)
    {
        g_lastInitIssues.store(initIssues, std::memory_order_release);
        Vigem_Destroy();
        wooting_analog_uninitialise();
        return false;
    }

    for (auto& a : g_uiAnalogM) a.store(0, std::memory_order_relaxed);
    for (auto& a : g_uiRawM)    a.store(0, std::memory_order_relaxed);
    for (auto& d : g_uiDirty)   d.store(0, std::memory_order_relaxed);
    for (int i = 0; i < kMaxVirtualPads; ++i)
    {
        g_lastSentValid[(size_t)i] = 0;
        g_lastSentTicks[(size_t)i] = 0;
        g_lastSentReports[(size_t)i] = XUSB_REPORT{};
    }

    return true;
}

void Backend_Shutdown()
{
    g_reconnectRequested.store(false, std::memory_order_release);
    g_deviceChangeReconnectRequested.store(false, std::memory_order_release);
    g_vigemUpdateFailStreak = 0;
    Vigem_Destroy();
    wooting_analog_uninitialise();
}

void Backend_Tick()
{
    if (g_reconnectRequested.exchange(false, std::memory_order_acq_rel))
    {
        g_deviceChangeReconnectRequested.store(false, std::memory_order_release);
        Vigem_ReconnectThrottled(true);
    }
    else if (g_deviceChangeReconnectRequested.exchange(false, std::memory_order_acq_rel))
    {
        Vigem_ReconnectThrottled(false);
    }

    HidCache cache;

    int cnt = g_trackedCount.load(std::memory_order_acquire);
    cnt = std::clamp(cnt, 0, 256);

    // UI snapshot update
    for (int i = 0; i < cnt; ++i)
    {
        uint16_t hid = g_trackedList[i];
        if (hid == 0 || hid >= 256) continue;

        float raw = ReadRaw01Cached(hid, cache);
        float filtered = ReadFiltered01Cached(hid, cache);

        int rawM = (int)std::lround(raw * 1000.0f);
        rawM = std::clamp(rawM, 0, 1000);
        g_uiRawM[hid].store((uint16_t)rawM, std::memory_order_relaxed);

        int outM = (int)std::lround(filtered * 1000.0f);
        outM = std::clamp(outM, 0, 1000);

        uint16_t newV = (uint16_t)outM;
        uint16_t oldV = g_uiAnalogM[hid].load(std::memory_order_relaxed);
        if (oldV != newV) {
            int diff = std::abs((int)newV - (int)oldV);
            bool edge = (oldV == 0 || newV == 0 || oldV == 1000 || newV == 1000);
            if (diff >= 2 || edge)
            {
                g_uiAnalogM[hid].store(newV, std::memory_order_relaxed);
                int chunk = hid / 64;
                int bit = hid % 64;
                g_uiDirty[chunk].fetch_or(1ULL << bit, std::memory_order_relaxed);
            }
        }
    }

    // Bind capture: scan all HID 1..255 and capture first edge above threshold.
    if (g_bindCaptureEnabled.load(std::memory_order_acquire))
    {
        uint16_t bestHid = 0;
        int bestRawM = 0;
        for (uint16_t hid = 1; hid < 256; ++hid)
        {
            float raw = ReadRaw01Cached(hid, cache);
            int rawM = (int)std::lround(raw * 1000.0f);
            if (rawM > bestRawM)
            {
                bestRawM = rawM;
                bestHid = hid;
            }
        }

        bool down = (bestRawM >= 120);
        bool hadDown = g_bindHadDown.load(std::memory_order_relaxed);
        if (down && !hadDown && bestHid != 0)
        {
            uint32_t packed = (uint32_t)(bestHid & 0xFFFFu) | ((uint32_t)(bestRawM & 0xFFFFu) << 16);
            g_bindCapturedPacked.store(packed, std::memory_order_release);
        }
        g_bindHadDown.store(down, std::memory_order_relaxed);
    }
    else
    {
        g_bindHadDown.store(false, std::memory_order_relaxed);
    }

    int logicalPads = std::clamp(g_virtualPadCount.load(std::memory_order_acquire), 1, kMaxVirtualPads);
    for (int pad = 0; pad < logicalPads; ++pad)
    {
        XUSB_REPORT report = BuildReportForPad(pad, cache);
        g_reports[(size_t)pad] = report;

        g_lastRX[(size_t)pad].store(report.sThumbRX, std::memory_order_release);
        g_lastSeq[(size_t)pad].fetch_add(1, std::memory_order_acq_rel);
        g_lastReport[(size_t)pad] = report;
        g_lastSeq[(size_t)pad].fetch_add(1, std::memory_order_release);
    }
    for (int pad = logicalPads; pad < kMaxVirtualPads; ++pad)
    {
        XUSB_REPORT report{};
        g_reports[(size_t)pad] = report;

        g_lastRX[(size_t)pad].store(0, std::memory_order_release);
        g_lastSeq[(size_t)pad].fetch_add(1, std::memory_order_acq_rel);
        g_lastReport[(size_t)pad] = report;
        g_lastSeq[(size_t)pad].fetch_add(1, std::memory_order_release);
    }

    if (g_virtualPadsEnabled.load(std::memory_order_acquire))
    {
        if (g_client && g_connectedPadCount > 0) {
            VIGEM_ERROR err = VIGEM_ERROR_NONE;
            bool allOk = true;
            DWORD now = GetTickCount();
            constexpr DWORD kMinSendIntervalMs = 4;
            constexpr DWORD kKeepAliveMs = 250;

            for (int i = 0; i < g_connectedPadCount; ++i)
            {
                PVIGEM_TARGET pad = g_pads[(size_t)i];
                if (!pad) continue;

                int idx = std::clamp(i, 0, kMaxVirtualPads - 1);
                const XUSB_REPORT& report = g_reports[(size_t)idx];

                bool valid = g_lastSentValid[(size_t)idx] != 0;
                bool changed = !valid || IsReportSignificantlyDifferent(report, g_lastSentReports[(size_t)idx]);
                DWORD elapsed = now - g_lastSentTicks[(size_t)idx];

                if (!changed && elapsed < kKeepAliveMs)
                    continue;
                if (changed && elapsed < kMinSendIntervalMs)
                    continue;

                err = vigem_target_x360_update(g_client, pad, report);
                if (!VIGEM_SUCCESS(err))
                {
                    allOk = false;
                    break;
                }

                g_lastSentReports[(size_t)idx] = report;
                g_lastSentTicks[(size_t)idx] = now;
                g_lastSentValid[(size_t)idx] = 1;
            }

            if (!allOk) {
                g_vigemOk.store(false, std::memory_order_release);
                g_vigemLastErr.store(err, std::memory_order_release);
                ++g_vigemUpdateFailStreak;
                if (g_vigemUpdateFailStreak >= 3)
                {
                    g_vigemUpdateFailStreak = 0;
                    Vigem_ReconnectThrottled();
                }
            }
            else {
                g_vigemUpdateFailStreak = 0;
                g_vigemOk.store(true, std::memory_order_release);
                g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
            }
        }
        else {
            g_vigemUpdateFailStreak = 0;
            g_vigemOk.store(false, std::memory_order_release);
            g_vigemLastErr.store(VIGEM_ERROR_BUS_NOT_FOUND, std::memory_order_release);
            Vigem_ReconnectThrottled();
        }
    }
    else {
        g_vigemUpdateFailStreak = 0;
        if (g_client || g_connectedPadCount > 0)
            Vigem_Destroy();
        g_vigemOk.store(true, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
    }
}

SHORT Backend_GetLastRX() { return g_lastRX[0].load(std::memory_order_acquire); }

XUSB_REPORT Backend_GetLastReport()
{
    return Backend_GetLastReportForPad(0);
}

XUSB_REPORT Backend_GetLastReportForPad(int padIndex)
{
    int p = std::clamp(padIndex, 0, kMaxVirtualPads - 1);
    XUSB_REPORT r{};
    for (;;) {
        uint32_t s1 = g_lastSeq[(size_t)p].load(std::memory_order_acquire);
        if (s1 & 1u) continue;
        r = g_lastReport[(size_t)p];
        uint32_t s2 = g_lastSeq[(size_t)p].load(std::memory_order_acquire);
        if (s1 == s2) return r;
    }
}

void BackendUI_SetTrackedHids(const uint16_t* hids, int count)
{
    if (!hids || count <= 0) { BackendUI_ClearTrackedHids(); return; }
    count = std::clamp(count, 0, 256);

    g_trackedCount.store(0, std::memory_order_release);

    int outN = 0;
    for (int i = 0; i < count && outN < 256; ++i) {
        uint16_t hid = hids[i];
        if (hid == 0 || hid >= 256) continue;
        g_trackedList[outN++] = hid;
    }

    g_trackedCount.store(outN, std::memory_order_release);
}

void BackendUI_ClearTrackedHids() { g_trackedCount.store(0, std::memory_order_release); }

uint16_t BackendUI_GetAnalogMilli(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return 0;
    return g_uiAnalogM[hid].load(std::memory_order_relaxed);
}

uint16_t BackendUI_GetRawMilli(uint16_t hid)
{
    if (hid == 0 || hid >= 256) return 0;
    return g_uiRawM[hid].load(std::memory_order_relaxed);
}

void BackendUI_SetBindCapture(bool enable)
{
    g_bindCaptureEnabled.store(enable, std::memory_order_release);
    if (!enable)
    {
        g_bindCapturedPacked.store(0, std::memory_order_release);
        g_bindHadDown.store(false, std::memory_order_relaxed);
    }
}

bool BackendUI_ConsumeBindCapture(uint16_t* outHid, uint16_t* outRawMilli)
{
    uint32_t p = g_bindCapturedPacked.exchange(0, std::memory_order_acq_rel);
    if (!p) return false;
    if (outHid) *outHid = (uint16_t)(p & 0xFFFFu);
    if (outRawMilli) *outRawMilli = (uint16_t)((p >> 16) & 0xFFFFu);
    return true;
}

uint64_t BackendUI_ConsumeDirtyChunk(int chunk)
{
    if (chunk < 0 || chunk >= 4) return 0;
    return g_uiDirty[chunk].exchange(0, std::memory_order_acq_rel);
}

BackendStatus Backend_GetStatus()
{
    BackendStatus s;
    s.vigemOk = g_vigemOk.load(std::memory_order_acquire);
    s.lastVigemError = g_vigemLastErr.load(std::memory_order_acquire);
    return s;
}

void Backend_NotifyDeviceChange()
{
    if (!g_virtualPadsEnabled.load(std::memory_order_acquire))
        return;

    // Ignore generic device-change noise while ViGEm is healthy.
    // Realtime tick already detects real update failures and reconnects.
    if (g_vigemOk.load(std::memory_order_acquire))
        return;

    ULONGLONG now = GetTickCount64();
    ULONGLONG ignoreUntil = g_ignoreDeviceChangeUntilMs.load(std::memory_order_acquire);
    if (now < ignoreUntil)
        return;

    g_deviceChangeReconnectRequested.store(true, std::memory_order_release);
}

void Backend_SetVirtualGamepadCount(int count)
{
    count = std::clamp(count, 1, kMaxVirtualPads);
    int old = g_virtualPadCount.exchange(count, std::memory_order_acq_rel);
    if (old != count)
        g_reconnectRequested.store(true, std::memory_order_release);
}

int Backend_GetVirtualGamepadCount()
{
    return g_virtualPadCount.load(std::memory_order_acquire);
}

void Backend_SetVirtualGamepadsEnabled(bool on)
{
    bool old = g_virtualPadsEnabled.exchange(on, std::memory_order_acq_rel);
    if (old != on)
        g_reconnectRequested.store(true, std::memory_order_release);
}

bool Backend_GetVirtualGamepadsEnabled()
{
    return g_virtualPadsEnabled.load(std::memory_order_acquire);
}

uint32_t Backend_GetLastInitIssues()
{
    return g_lastInitIssues.load(std::memory_order_acquire);
}
