#define NOMINMAX
#include "backend_curve.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

#include "curve_math.h"
#include "key_settings.h"
#include "settings.h"

namespace
{
struct CurveDef
{
    float x0 = 0.0f, y0 = 0.0f;
    float x1 = 0.0f, y1 = 0.0f;
    float x2 = 0.0f, y2 = 0.0f;
    float x3 = 1.0f, y3 = 1.0f;
    float w1 = 1.0f;
    float w2 = 1.0f;
    UINT mode = 0;
    bool invert = false;
};

static std::atomic<uint64_t> g_curveCacheStamp{ 1 };

static float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

static float ApplyCurve_LinearSegments(float x, const CurveDef& c)
{
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
    return Clamp01(ya + (yb - ya) * t);
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
    return CurveMath::EvalRationalYForX(cc, x, 18);
}

static CurveDef NormalizeCurveDef(CurveDef c)
{
    c.w1 = Clamp01(c.w1);
    c.w2 = Clamp01(c.w2);

    c.y0 = Clamp01(c.y0); c.y1 = Clamp01(c.y1);
    c.y2 = Clamp01(c.y2); c.y3 = Clamp01(c.y3);

    c.x0 = Clamp01(c.x0);
    c.x3 = Clamp01(c.x3);
    if (c.x3 < c.x0 + 0.01f) c.x3 = std::clamp(c.x0 + 0.01f, 0.01f, 1.0f);

    const float minGap = 0.001f;
    c.x1 = std::clamp(c.x1, c.x0, c.x3);
    c.x2 = std::clamp(c.x2, c.x0, c.x3);
    c.x1 = std::clamp(c.x1, c.x0, c.x3 - minGap);
    c.x2 = std::clamp(c.x2, c.x1, c.x3);

    return c;
}

struct CurveThreadCache
{
    uint64_t stamp = 0;
    bool globalReady = false;
    CurveDef globalCurve{};
    std::array<uint8_t, 256> hasCurve{};
    std::array<CurveDef, 256> curves{};
};

static CurveThreadCache& GetCurveThreadCache()
{
    static thread_local CurveThreadCache c;
    uint64_t stamp = g_curveCacheStamp.load(std::memory_order_relaxed);
    if (c.stamp != stamp)
    {
        c.stamp = stamp;
        c.globalReady = false;
        c.hasCurve.fill(0u);
    }
    return c;
}

static CurveDef BuildGlobalCurveSnapshot()
{
    CurveDef c{};
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
    return NormalizeCurveDef(c);
}

static CurveDef BuildCurveForHid(uint16_t hid)
{
    CurveThreadCache& cache = GetCurveThreadCache();
    if (hid < 256 && cache.hasCurve[hid] != 0)
        return cache.curves[hid];

    CurveDef c{};
    if (KeySettings_GetUseUnique(hid))
    {
        KeyDeadzone ks = KeySettings_Get(hid);
        c.invert = ks.invert;
        c.mode = (UINT)(ks.curveMode == 0 ? 0 : 1);
        c.x0 = ks.low;   c.y0 = ks.antiDeadzone;
        c.x1 = ks.cp1_x; c.y1 = ks.cp1_y;
        c.x2 = ks.cp2_x; c.y2 = ks.cp2_y;
        c.x3 = ks.high;  c.y3 = ks.outputCap;
        c.w1 = ks.cp1_w;
        c.w2 = ks.cp2_w;
        c = NormalizeCurveDef(c);
    }
    else
    {
        if (!cache.globalReady)
        {
            cache.globalCurve = BuildGlobalCurveSnapshot();
            cache.globalReady = true;
        }
        c = cache.globalCurve;
    }

    if (hid < 256)
    {
        cache.curves[hid] = c;
        cache.hasCurve[hid] = 1u;
    }
    return c;
}
}

void BackendCurve_BeginTick()
{
    g_curveCacheStamp.fetch_add(1u, std::memory_order_relaxed);
}

float BackendCurve_ApplyByHid(uint16_t hid, float x01Raw)
{
    float x01 = Clamp01(x01Raw);
    CurveDef c = BuildCurveForHid(hid);

    if (c.invert) x01 = 1.0f - x01;
    if (x01 < c.x0) return 0.0f;
    if (x01 > c.x3) return Clamp01(c.y3);

    if (c.mode == 1) return ApplyCurve_LinearSegments(x01, c);
    return ApplyCurve_SmoothRationalBezier(x01, c);
}

