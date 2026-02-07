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
static PVIGEM_TARGET g_pad = nullptr;

static XUSB_REPORT   g_report{};

// Thread-safe last-report snapshot (writer: realtime thread, reader: UI thread)
static std::atomic<uint32_t> g_lastSeq{ 0 };
static XUSB_REPORT           g_lastReport{};
static std::atomic<SHORT>    g_lastRX{ 0 };

// ---- UI snapshot ----
static std::array<std::atomic<uint16_t>, 256> g_uiAnalogM{}; // filtered output (after curve)
static std::array<std::atomic<uint16_t>, 256> g_uiRawM{};    // NEW: raw input
static std::array<std::atomic<uint64_t>, 4>   g_uiDirty{};   // dirty for filtered only

// list of HID codes to track (provided by UI)
static std::array<uint16_t, 256> g_trackedList{};
static std::atomic<int>          g_trackedCount{ 0 };

// ---- status / reconnect ----
static std::atomic<bool>         g_vigemOk{ false };
static std::atomic<VIGEM_ERROR>  g_vigemLastErr{ VIGEM_ERROR_NONE };
static std::atomic<bool>         g_reconnectRequested{ false };
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
    if (g_client && g_pad) { vigem_target_remove(g_client, g_pad); vigem_target_free(g_pad); g_pad = nullptr; }
    if (g_client) { vigem_disconnect(g_client); vigem_free(g_client); g_client = nullptr; }
}

static bool Vigem_Create(VIGEM_ERROR* outErr)
{
    if (outErr) *outErr = VIGEM_ERROR_NONE;
    g_client = vigem_alloc();
    if (!g_client) { if (outErr) *outErr = VIGEM_ERROR_BUS_NOT_FOUND; return false; }
    VIGEM_ERROR err = vigem_connect(g_client);
    if (!VIGEM_SUCCESS(err)) { if (outErr) *outErr = err; vigem_free(g_client); g_client = nullptr; return false; }
    g_pad = vigem_target_x360_alloc();
    if (!g_pad) { if (outErr) *outErr = VIGEM_ERROR_INVALID_TARGET; vigem_disconnect(g_client); vigem_free(g_client); g_client = nullptr; return false; }
    err = vigem_target_add(g_client, g_pad);
    if (!VIGEM_SUCCESS(err)) { if (outErr) *outErr = err; vigem_target_free(g_pad); g_pad = nullptr; vigem_disconnect(g_client); vigem_free(g_client); g_client = nullptr; return false; }
    if (outErr) *outErr = VIGEM_ERROR_NONE;
    return true;
}

static bool Vigem_ReconnectThrottled()
{
    ULONGLONG now = GetTickCount64();
    if (now - g_lastReconnectAttemptMs < 1000) return false;
    g_lastReconnectAttemptMs = now;
    Vigem_Destroy();
    VIGEM_ERROR err = VIGEM_ERROR_NONE;
    bool ok = Vigem_Create(&err);
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
static std::array<uint8_t, 4> g_snappyPrevMinusDown{};
static std::array<uint8_t, 4> g_snappyPrevPlusDown{};
static std::array<int8_t, 4>  g_snappyLastDir{}; // -1 = minus, +1 = plus, 0 = unknown

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

static float AxisValue_Snappy(Axis a, float minusV, float plusV)
{
    // old behavior
    if (!Settings_GetSnappyJoystick())
        return plusV - minusV;

    int idx = AxisIndexSafe(a);
    if (idx < 0 || idx >= 4)
        return plusV - minusV;

    // detect "press" edges using the same semantics as buttons (stable threshold)
    bool minusDown = Pressed(minusV);
    bool plusDown = Pressed(plusV);

    bool prevMinus = (g_snappyPrevMinusDown[idx] != 0);
    bool prevPlus = (g_snappyPrevPlusDown[idx] != 0);

    if (minusDown && !prevMinus) g_snappyLastDir[idx] = -1;
    if (plusDown && !prevPlus)  g_snappyLastDir[idx] = +1;

    g_snappyPrevMinusDown[idx] = minusDown ? 1u : 0u;
    g_snappyPrevPlusDown[idx] = plusDown ? 1u : 0u;

    float maxV = std::max(minusV, plusV);
    if (maxV <= 0.0001f)
        return 0.0f;

    // If one is bigger -> choose that direction with full magnitude=maxV
    // If equal -> choose last pressed direction (if known)
    constexpr float EQ_EPS = 0.002f; // tolerant equality (float noise)
    float d = plusV - minusV;

    if (std::fabs(d) > EQ_EPS)
        return (d > 0.0f) ? +maxV : -maxV;

    // equal values
    if (g_snappyLastDir[idx] > 0) return +maxV;
    if (g_snappyLastDir[idx] < 0) return -maxV;

    // unknown history: neutral
    return 0.0f;
}

static void SetBtn(WORD mask, bool down)
{
    if (down) g_report.wButtons |= mask;
    else      g_report.wButtons &= ~mask;
}

static bool BtnPressedFromMask(GameButton b, HidCache& cache)
{
    for (int chunk = 0; chunk < 4; ++chunk)
    {
        uint64_t bits = Bindings_GetButtonMaskChunk(b, chunk);
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

bool Backend_Init()
{
    wooting_analog_initialise();
    VIGEM_ERROR err = VIGEM_ERROR_NONE;
    if (!Vigem_Create(&err)) {
        g_vigemOk.store(false, std::memory_order_release);
        g_vigemLastErr.store(err, std::memory_order_release);
        wooting_analog_uninitialise();
        return false;
    }
    g_vigemOk.store(true, std::memory_order_release);
    g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);

    for (auto& a : g_uiAnalogM) a.store(0, std::memory_order_relaxed);
    for (auto& a : g_uiRawM)    a.store(0, std::memory_order_relaxed);
    for (auto& d : g_uiDirty)   d.store(0, std::memory_order_relaxed);

    return true;
}

void Backend_Shutdown()
{
    Vigem_Destroy();
    wooting_analog_uninitialise();
}

void Backend_Tick()
{
    if (g_reconnectRequested.exchange(false, std::memory_order_acq_rel))
        Vigem_ReconnectThrottled();

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
            g_uiAnalogM[hid].store(newV, std::memory_order_relaxed);
            int chunk = hid / 64;
            int bit = hid % 64;
            g_uiDirty[chunk].fetch_or(1ULL << bit, std::memory_order_relaxed);
        }
    }

    g_report.wButtons = 0;

    auto applyAxis = [&](Axis a, SHORT& out) {
        AxisBinding b = Bindings_GetAxis(a);
        float minusV = ReadFiltered01Cached(b.minusHid, cache);
        float plusV = ReadFiltered01Cached(b.plusHid, cache);
        out = StickFromMinus1Plus1(AxisValue_Snappy(a, minusV, plusV));
        };

    applyAxis(Axis::LX, g_report.sThumbLX);
    applyAxis(Axis::LY, g_report.sThumbLY);
    applyAxis(Axis::RX, g_report.sThumbRX);
    applyAxis(Axis::RY, g_report.sThumbRY);

    g_report.bLeftTrigger = TriggerByte01(ReadFiltered01Cached(Bindings_GetTrigger(Trigger::LT), cache));
    g_report.bRightTrigger = TriggerByte01(ReadFiltered01Cached(Bindings_GetTrigger(Trigger::RT), cache));

    SetBtn(XUSB_GAMEPAD_A, BtnPressedFromMask(GameButton::A, cache));
    SetBtn(XUSB_GAMEPAD_B, BtnPressedFromMask(GameButton::B, cache));
    SetBtn(XUSB_GAMEPAD_X, BtnPressedFromMask(GameButton::X, cache));
    SetBtn(XUSB_GAMEPAD_Y, BtnPressedFromMask(GameButton::Y, cache));
    SetBtn(XUSB_GAMEPAD_LEFT_SHOULDER, BtnPressedFromMask(GameButton::LB, cache));
    SetBtn(XUSB_GAMEPAD_RIGHT_SHOULDER, BtnPressedFromMask(GameButton::RB, cache));
    SetBtn(XUSB_GAMEPAD_BACK, BtnPressedFromMask(GameButton::Back, cache));
    SetBtn(XUSB_GAMEPAD_START, BtnPressedFromMask(GameButton::Start, cache));
    SetBtn(XUSB_GAMEPAD_GUIDE, BtnPressedFromMask(GameButton::Guide, cache));
    SetBtn(XUSB_GAMEPAD_LEFT_THUMB, BtnPressedFromMask(GameButton::LS, cache));
    SetBtn(XUSB_GAMEPAD_RIGHT_THUMB, BtnPressedFromMask(GameButton::RS, cache));
    SetBtn(XUSB_GAMEPAD_DPAD_UP, BtnPressedFromMask(GameButton::DpadUp, cache));
    SetBtn(XUSB_GAMEPAD_DPAD_DOWN, BtnPressedFromMask(GameButton::DpadDown, cache));
    SetBtn(XUSB_GAMEPAD_DPAD_LEFT, BtnPressedFromMask(GameButton::DpadLeft, cache));
    SetBtn(XUSB_GAMEPAD_DPAD_RIGHT, BtnPressedFromMask(GameButton::DpadRight, cache));

    g_lastRX.store(g_report.sThumbRX, std::memory_order_release);
    // Mark sequence as "write in progress" (odd) before touching payload.
    g_lastSeq.fetch_add(1, std::memory_order_acq_rel);
    g_lastReport = g_report;
    g_lastSeq.fetch_add(1, std::memory_order_release);

    if (g_client && g_pad) {
        VIGEM_ERROR err = vigem_target_x360_update(g_client, g_pad, g_report);
        if (!VIGEM_SUCCESS(err)) {
            g_vigemOk.store(false, std::memory_order_release);
            g_vigemLastErr.store(err, std::memory_order_release);
            Vigem_ReconnectThrottled();
        }
        else {
            g_vigemOk.store(true, std::memory_order_release);
            g_vigemLastErr.store(VIGEM_ERROR_NONE, std::memory_order_release);
        }
    }
    else {
        g_vigemOk.store(false, std::memory_order_release);
        g_vigemLastErr.store(VIGEM_ERROR_BUS_NOT_FOUND, std::memory_order_release);
        Vigem_ReconnectThrottled();
    }
}

SHORT Backend_GetLastRX() { return g_lastRX.load(std::memory_order_acquire); }

XUSB_REPORT Backend_GetLastReport()
{
    XUSB_REPORT r{};
    for (;;) {
        uint32_t s1 = g_lastSeq.load(std::memory_order_acquire);
        if (s1 & 1u) continue;
        r = g_lastReport;
        uint32_t s2 = g_lastSeq.load(std::memory_order_acquire);
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

void Backend_NotifyDeviceChange() { g_reconnectRequested.store(true, std::memory_order_release); }
