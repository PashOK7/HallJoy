// keyboard_keysettings_panel_graph.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cwchar>
#include <string>

#include <objidl.h>
#include <gdiplus.h>

#include "keyboard_keysettings_panel_internal.h"
#include "keyboard_keysettings_panel.h"
#include "win_util.h"
#include "ui_theme.h"
#include "backend.h"

#include "curve_math.h"

using namespace Gdiplus;

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }
static constexpr const wchar_t* CONFIG_SCROLLY_PROP = L"DD_ConfigScrollY";

static int ParentScrollY(HWND parent)
{
    if (!parent) return 0;
    return (int)(INT_PTR)GetPropW(parent, CONFIG_SCROLLY_PROP);
}

// ----------------------------------------------------------------------------
// Drag state
// ----------------------------------------------------------------------------
enum class DragTarget { None, Start, End, C1, C2 };
static DragTarget g_drag = DragTarget::None;
static bool g_hoverHandle = false;

// ----------------------------------------------------------------------------

KeySettingsPanel_DragHint KeySettingsPanel_GetDragHint(float* outWeight01)
{
    if (outWeight01) *outWeight01 = 0.0f;

    KeyDeadzone ks = Ksp_GetVisualCurve();

    if (g_drag == DragTarget::C1)
    {
        if (outWeight01) *outWeight01 = std::clamp(ks.cp1_w, 0.0f, 1.0f);
        return KeySettingsPanel_DragHint::Cp1;
    }
    if (g_drag == DragTarget::C2)
    {
        if (outWeight01) *outWeight01 = std::clamp(ks.cp2_w, 0.0f, 1.0f);
        return KeySettingsPanel_DragHint::Cp2;
    }
    return KeySettingsPanel_DragHint::None;
}

static RectF GraphRectF(HWND hParent)
{
    int x = S(hParent, 12);
    int y = S(hParent, 86);
    int w = S(hParent, 520);
    int h = S(hParent, 160);
    return RectF((float)x, (float)y, (float)w, (float)h);
}

static RECT GraphRectR(HWND hParent)
{
    RectF r = GraphRectF(hParent);
    RECT rc{};
    rc.left = (int)std::floor(r.X);
    rc.top = (int)std::floor(r.Y);
    rc.right = (int)std::ceil(r.X + r.Width);
    rc.bottom = (int)std::ceil(r.Y + r.Height);
    return rc;
}

bool KeySettingsPanel_GetGraphRect(HWND parent, RECT* outRc)
{
    if (!outRc) return false;
    *outRc = RECT{};
    if (!parent) return false;
    RECT r = GraphRectR(parent);
    int scrollY = ParentScrollY(parent);
    if (scrollY != 0) OffsetRect(&r, 0, -scrollY);
    InflateRect(&r, S(parent, 3), S(parent, 3));
    *outRc = r;
    return true;
}

// NEW: exact rect for the CP weight hint (drawn under the graph on Config page).
// Must match DrawCpWeightHintIfNeeded() in keyboard_subpages.cpp.
bool KeySettingsPanel_GetCpWeightHintRect(HWND parent, RECT* outRc)
{
    if (!outRc) return false;
    *outRc = RECT{};
    if (!parent) return false;

    RECT rcClient{};
    GetClientRect(parent, &rcClient);

    int x = S(parent, 12);
    int y = S(parent, 286);
    y -= ParentScrollY(parent);

    int h = S(parent, 20);
    int w = (rcClient.right - rcClient.left) - x - S(parent, 12);
    if (w < S(parent, 60)) w = S(parent, 60);

    RECT r{ x, y, x + w, y + h };
    InflateRect(&r, S(parent, 4), S(parent, 4)); // cover AA edges
    *outRc = r;
    return true;
}

static void InvalidateCpHint(HWND parent)
{
    RECT r{};
    if (KeySettingsPanel_GetCpWeightHintRect(parent, &r))
        InvalidateRect(parent, &r, FALSE);
}

static float GraphPadPxFromSize(float w, float h)
{
    float s = std::min(w, h);
    return std::clamp(s * 0.045f, 5.0f, 11.0f);
}

static RectF GraphInnerFromBounds(const RectF& outer)
{
    float pad = GraphPadPxFromSize(outer.Width, outer.Height);
    RectF inner = outer;
    inner.Inflate(-pad, -pad);
    if (inner.Width < 10.0f) inner.Width = 10.0f;
    if (inner.Height < 10.0f) inner.Height = 10.0f;
    return inner;
}

// ---------------- Info label ----------------
static std::wstring g_lastInfoText;

static void SetStaticText_NoFlicker(HWND hStatic, const wchar_t* text)
{
    if (!hStatic) return;
    SendMessageW(hStatic, WM_SETREDRAW, FALSE, 0);
    SetWindowTextW(hStatic, text ? text : L"");
    SendMessageW(hStatic, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(hStatic, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE | RDW_UPDATENOW);
}

static float EvalLinear(float x, const KeyDeadzone& ks)
{
    float x0 = ks.low;
    float x3 = ks.high;
    float x1 = std::clamp(ks.cp1_x, x0, x3);
    float x2 = std::clamp(ks.cp2_x, x1, x3);

    float y0 = ks.antiDeadzone;
    float y1 = ks.cp1_y;
    float y2 = ks.cp2_y;
    float y3 = ks.outputCap;

    if (x <= x0) return y0;
    if (x >= x3) return y3;

    if (x <= x1)
    {
        if (x1 <= x0 + 1e-5f) return y0;
        float t = (x - x0) / (x1 - x0);
        return y0 + (y1 - y0) * t;
    }
    else if (x <= x2)
    {
        if (x2 <= x1 + 1e-5f) return y1;
        float t = (x - x1) / (x2 - x1);
        return y1 + (y2 - y1) * t;
    }
    else
    {
        if (x3 <= x2 + 1e-5f) return y2;
        float t = (x - x2) / (x3 - x2);
        return y2 + (y3 - y2) * t;
    }
}

static void ComputeYMinMax(const KeyDeadzone& ks, float& outYMin, float& outYMax)
{
    if (ks.curveMode == 1) // Linear
    {
        outYMin = std::min({ ks.antiDeadzone, ks.outputCap, ks.cp1_y, ks.cp2_y });
        outYMax = std::max({ ks.antiDeadzone, ks.outputCap, ks.cp1_y, ks.cp2_y });
    }
    else
    {
        CurveMath::Curve01 c = CurveMath::FromKeyDeadzone(ks);
        float yMin = 1.0f;
        float yMax = 0.0f;
        constexpr int N = 160;
        for (int i = 0; i <= N; ++i)
        {
            float t = (float)i / (float)N;
            CurveMath::Vec2 p = CurveMath::EvalRationalBezier(c, t);
            yMin = std::min(yMin, p.y);
            yMax = std::max(yMax, p.y);
        }
        outYMin = yMin;
        outYMax = yMax;
    }
    outYMin = std::clamp(outYMin, 0.0f, 1.0f);
    outYMax = std::clamp(outYMax, 0.0f, 1.0f);
    if (outYMax < outYMin) std::swap(outYMin, outYMax);
}

static void BuildInfoText(const KeyDeadzone& ks, wchar_t out[256])
{
    auto pct = [](float f) { return f * 100.0f; };
    float shownAnti = 0.0f, shownCap = 1.0f;
    ComputeYMinMax(ks, shownAnti, shownCap);

    if (!Ksp_IsKeySelected())
        swprintf_s(out, 256, L"Global | DZ: %.1f%% | Act: %.1f%% | Min: %.1f%% | Max: %.1f%%",
            pct(ks.low), pct(ks.high), pct(shownAnti), pct(shownCap));
    else if (!Ksp_EditingUniqueKey())
        swprintf_s(out, 256, L"Editing Global via Key 0x%02X | Min: %.1f%% | Max: %.1f%%",
            g_kspSelectedHid, pct(shownAnti), pct(shownCap));
    else
        swprintf_s(out, 256, L"Key 0x%02X | DZ: %.1f%% | Act: %.1f%% | Min: %.1f%% | Max: %.1f%%",
            g_kspSelectedHid, pct(ks.low), pct(ks.high), pct(shownAnti), pct(shownCap));
}

static void UpdateInfoLabelIfNeeded(bool force)
{
    if (!g_kspTxtInfo) return;
    KeyDeadzone ks = Ksp_GetActiveSettings();
    wchar_t buf[256]{};
    BuildInfoText(ks, buf);
    if (!force && g_lastInfoText == buf) return;
    g_lastInfoText = buf;
    SetStaticText_NoFlicker(g_kspTxtInfo, buf);
}

// ---------------- Stable evaluation for marker (y(x)) ----------------
static float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

static float SampleSmoothY_Stable(const KeyDeadzone& ks, float x01)
{
    x01 = Clamp01(x01);

    CurveMath::Curve01 c = CurveMath::FromKeyDeadzone(ks);

    // Dense sampling in t
    constexpr int M = 700;
    float xs[M + 1];
    float ys[M + 1];

    for (int i = 0; i <= M; ++i)
    {
        float t = (float)i / (float)M;
        CurveMath::Vec2 p = CurveMath::EvalRationalBezier(c, t);
        xs[i] = Clamp01(p.x);
        ys[i] = Clamp01(p.y);
    }

    // Enforce non-decreasing x to make inverse stable
    for (int i = 1; i <= M; ++i)
        if (xs[i] < xs[i - 1]) xs[i] = xs[i - 1];

    // Binary search for x01
    int lo = 0, hi = M;
    while (lo < hi)
    {
        int mid = (lo + hi) / 2;
        if (xs[mid] < x01) lo = mid + 1;
        else hi = mid;
    }
    int j = lo;

    if (j <= 0) return ys[0];
    if (j >= M) return ys[M];

    float x0 = xs[j - 1], x1 = xs[j];
    float y0 = ys[j - 1], y1 = ys[j];

    float denom = (x1 - x0);
    if (std::fabs(denom) < 1e-7f)
        return y1;

    float t = (x01 - x0) / denom;
    t = Clamp01(t);
    return y0 + (y1 - y0) * t;
}

// ---------------- Graph drawing helpers ----------------
static Color GpFromColorRef(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void DrawHandle(Graphics& g, PointF p, float r, bool filled, Color fill, Color outline)
{
    Pen pen(outline, 1.0f);
    if (filled) {
        SolidBrush br(fill);
        g.FillEllipse(&br, p.X - r, p.Y - r, r * 2, r * 2);
    }
    else {
        SolidBrush bg(Color(255, 30, 30, 30));
        g.FillEllipse(&bg, p.X - r, p.Y - r, r * 2, r * 2);
    }
    g.DrawEllipse(&pen, p.X - r, p.Y - r, r * 2, r * 2);
}

// ---------------- Morph easing helper ----------------
static float MorphEase01()
{
    if (!g_kspMorph.running) return 1.0f;

    DWORD now = GetTickCount();
    DWORD dt = now - g_kspMorph.startTick;
    float t = (g_kspMorph.durationMs > 0) ? (float)dt / (float)g_kspMorph.durationMs : 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    // ease-in-out
    float e = (t < 0.5f) ? (4.0f * t * t * t) : (1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f);
    return std::clamp(e, 0.0f, 1.0f);
}

// ---------------- Morph evaluation as parametric curve (no inverse y(x)) ----------------
static CurveMath::Vec2 EvalLinearPolylinePointT(const KeyDeadzone& ks, float t)
{
    t = Clamp01(t);

    CurveMath::Vec2 p0{ ks.low, ks.antiDeadzone };
    CurveMath::Vec2 p1{ ks.cp1_x, ks.cp1_y };
    CurveMath::Vec2 p2{ ks.cp2_x, ks.cp2_y };
    CurveMath::Vec2 p3{ ks.high, ks.outputCap };

    auto dist = [](const CurveMath::Vec2& a, const CurveMath::Vec2& b) -> float {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
        };
    auto lerp = [](const CurveMath::Vec2& a, const CurveMath::Vec2& b, float u) -> CurveMath::Vec2 {
        return CurveMath::Vec2{ a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u };
        };

    float d01 = dist(p0, p1);
    float d12 = dist(p1, p2);
    float d23 = dist(p2, p3);
    float total = d01 + d12 + d23;
    if (total < 1e-6f) return p0;

    float s = t * total;

    if (s <= d01 && d01 > 1e-6f) return lerp(p0, p1, s / d01);
    s -= d01;
    if (s <= d12 && d12 > 1e-6f) return lerp(p1, p2, s / d12);
    s -= d12;
    if (d23 > 1e-6f) return lerp(p2, p3, s / d23);
    return p3;
}

static CurveMath::Vec2 EvalCurvePointT(const KeyDeadzone& ks, float t)
{
    t = Clamp01(t);
    if (ks.curveMode == 1)
        return EvalLinearPolylinePointT(ks, t);

    CurveMath::Curve01 c = CurveMath::FromKeyDeadzone(ks);
    return CurveMath::EvalRationalBezier(c, t);
}

static void RenderGraphContent(Graphics& g, const RectF& inner, const KeyDeadzone& ks, bool isMorphing)
{
    GraphicsPath curvePath;

    float yMin = 0.0f;
    float yMax = 1.0f;

    if (isMorphing)
    {
        float e = MorphEase01();

        // Parametric morph sampling eliminates staircase artifacts at extreme weights
        constexpr int N = 260;
        std::vector<PointF> pts;
        pts.reserve(N);

        yMin = 1.0f;
        yMax = 0.0f;

        auto toScr = [&](float x, float y) -> PointF {
            x = std::clamp(x, 0.0f, 1.0f);
            y = std::clamp(y, 0.0f, 1.0f);
            return PointF(inner.X + x * inner.Width, inner.GetBottom() - y * inner.Height);
            };

        for (int i = 0; i < N; ++i)
        {
            float t = (float)i / (float)(N - 1);

            CurveMath::Vec2 a = EvalCurvePointT(g_kspMorph.fromKs, t);
            CurveMath::Vec2 b = EvalCurvePointT(g_kspMorph.toKs, t);

            CurveMath::Vec2 p{
                a.x + (b.x - a.x) * e,
                a.y + (b.y - a.y) * e
            };

            p.x = std::clamp(p.x, 0.0f, 1.0f);
            p.y = std::clamp(p.y, 0.0f, 1.0f);

            yMin = std::min(yMin, p.y);
            yMax = std::max(yMax, p.y);

            pts.push_back(toScr(p.x, p.y));
        }

        if ((int)pts.size() >= 2)
            curvePath.AddLines(pts.data(), (INT)pts.size());
    }
    else
    {
        ComputeYMinMax(ks, yMin, yMax);

        auto toScr = [&](float x, float y) -> PointF {
            x = std::clamp(x, 0.0f, 1.0f);
            y = std::clamp(y, 0.0f, 1.0f);
            return PointF(inner.X + x * inner.Width, inner.GetBottom() - y * inner.Height);
            };

        if (ks.curveMode == 1) // Linear
        {
            PointF p0 = toScr(ks.low, ks.antiDeadzone);
            PointF p3 = toScr(ks.high, ks.outputCap);
            PointF p1 = toScr(ks.cp1_x, ks.cp1_y);
            PointF p2 = toScr(ks.cp2_x, ks.cp2_y);
            std::vector<PointF> pts = { p0, p1, p2, p3 };
            curvePath.AddLines(pts.data(), (INT)pts.size());
        }
        else // Smooth
        {
            CurveMath::Curve01 c = CurveMath::FromKeyDeadzone(ks);
            constexpr int N = 220;
            std::vector<PointF> pts;
            pts.reserve(N + 1);
            for (int i = 0; i <= N; ++i)
            {
                float t = (float)i / (float)N;
                CurveMath::Vec2 p = CurveMath::EvalRationalBezier(c, t);
                pts.push_back(toScr(p.x, p.y));
            }
            if ((int)pts.size() >= 2)
                curvePath.AddLines(pts.data(), (INT)pts.size());
        }
    }

    yMin = std::clamp(yMin, 0.0f, 1.0f);
    yMax = std::clamp(yMax, 0.0f, 1.0f);
    if (yMax < yMin) std::swap(yMin, yMax);

    // Deadzones (gray areas)
    float xLow = inner.X + inner.Width * std::clamp(ks.low, 0.0f, 1.0f);
    float xHigh = inner.X + inner.Width * std::clamp(ks.high, 0.0f, 1.0f);

    float yMinScr = inner.GetBottom() - inner.Height * yMin;
    float yMaxScr = inner.GetBottom() - inner.Height * yMax;

    {
        SolidBrush zoneBrush(Color(40, 200, 200, 200));

        if (xLow > inner.X) g.FillRectangle(&zoneBrush, inner.X, inner.Y, xLow - inner.X, inner.Height);
        if (xHigh < inner.GetRight()) g.FillRectangle(&zoneBrush, xHigh, inner.Y, inner.GetRight() - xHigh, inner.Height);

        if (yMin > 0.0f) g.FillRectangle(&zoneBrush, inner.X, yMinScr, inner.Width, inner.GetBottom() - yMinScr);
        if (yMax < 1.0f) g.FillRectangle(&zoneBrush, inner.X, inner.Y, inner.Width, yMaxScr - inner.Y);
    }

    // Clip curve+fill to ACTIVE area so it doesn't render inside gray zones
    RectF activeRect;
    {
        float left = std::min(xLow, xHigh);
        float right = std::max(xLow, xHigh);
        float top = std::min(yMaxScr, yMinScr);
        float bottom = std::max(yMaxScr, yMinScr);

        activeRect = RectF(left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top));
        if (activeRect.Width < 1.0f || activeRect.Height < 1.0f)
            activeRect = inner;
    }

    GraphicsState st = g.Save();
    g.SetClip(activeRect, CombineModeIntersect);

    // Fill under curve down to baseline = yMin (not to bottom gray zone)
    float baselineY = yMinScr;

    GraphicsPath fillPath;
    fillPath.AddPath(&curvePath, FALSE);
    fillPath.AddLine(activeRect.GetRight(), baselineY, activeRect.X, baselineY);
    fillPath.CloseFigure();

    Color accent(255, 255, 170, 0);
    Color topC(200, accent.GetR(), accent.GetG(), accent.GetB());
    Color botC(25, accent.GetR(), accent.GetG(), accent.GetB());
    LinearGradientBrush grad(inner, topC, botC, LinearGradientModeVertical);
    g.FillPath(&grad, &fillPath);

    Pen linePen(topC, 3.0f);
    g.DrawPath(&linePen, &curvePath);

    g.Restore(st);
}

// ----------------------------------------------------------------------------
// Graph Cache (struct + global + ensure/free)
// ----------------------------------------------------------------------------
struct GraphCache
{
    HDC memDC = nullptr;
    HBITMAP bmp = nullptr;
    HBITMAP oldBmp = nullptr;

    int w = 0;
    int h = 0;
    bool valid = false;

    KeyDeadzone lastKs{};
    KspGraphMode lastMode{};
    bool lastMorphing = false; // avoid keeping last morph frame into static
};

static GraphCache g_cache;

static void GraphCache_Free()
{
    if (g_cache.memDC)
    {
        if (g_cache.oldBmp)
        {
            SelectObject(g_cache.memDC, g_cache.oldBmp);
            g_cache.oldBmp = nullptr;
        }
        if (g_cache.bmp)
        {
            DeleteObject(g_cache.bmp);
            g_cache.bmp = nullptr;
        }
        DeleteDC(g_cache.memDC);
        g_cache.memDC = nullptr;
    }

    g_cache.w = 0;
    g_cache.h = 0;
    g_cache.valid = false;

    g_cache.lastKs = KeyDeadzone{};
    g_cache.lastMode = KspGraphMode::SmoothBezier;
    g_cache.lastMorphing = false;
}

static bool GraphCache_Ensure(int w, int h)
{
    if (w <= 1 || h <= 1) return false;

    if (!g_cache.memDC)
    {
        g_cache.memDC = CreateCompatibleDC(nullptr);
        if (!g_cache.memDC) return false;
    }

    if (!g_cache.bmp || g_cache.w != w || g_cache.h != h)
    {
        if (g_cache.oldBmp)
        {
            SelectObject(g_cache.memDC, g_cache.oldBmp);
            g_cache.oldBmp = nullptr;
        }
        if (g_cache.bmp)
        {
            DeleteObject(g_cache.bmp);
            g_cache.bmp = nullptr;
        }

        HDC screen = GetDC(nullptr);
        if (!screen) return false;

        HBITMAP newBmp = CreateCompatibleBitmap(screen, w, h);
        ReleaseDC(nullptr, screen);

        if (!newBmp) return false;

        g_cache.bmp = newBmp;
        g_cache.oldBmp = (HBITMAP)SelectObject(g_cache.memDC, g_cache.bmp);

        g_cache.w = w;
        g_cache.h = h;
        g_cache.valid = false;
    }

    return (g_cache.memDC && g_cache.bmp);
}

// ---------------- Comparison Helper ----------------
static bool NearlyEq2(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }

static bool SameKs2(const KeyDeadzone& a, const KeyDeadzone& b)
{
    return a.useUnique == b.useUnique &&
        a.invert == b.invert &&
        a.curveMode == b.curveMode &&
        NearlyEq2(a.low, b.low) &&
        NearlyEq2(a.high, b.high) &&
        NearlyEq2(a.antiDeadzone, b.antiDeadzone) &&
        NearlyEq2(a.outputCap, b.outputCap) &&
        NearlyEq2(a.cp1_x, b.cp1_x) &&
        NearlyEq2(a.cp1_y, b.cp1_y) &&
        NearlyEq2(a.cp2_x, b.cp2_x) &&
        NearlyEq2(a.cp2_y, b.cp2_y) &&
        NearlyEq2(a.cp1_w, b.cp1_w) &&
        NearlyEq2(a.cp2_w, b.cp2_w);
}

// ----------------------------------------------------------------------------
// RenderGraphToDC
// ----------------------------------------------------------------------------
static void RenderGraphToDC(HDC dc, int w, int h, const KeyDeadzone& ks, bool morphing)
{
    if (!dc || w <= 1 || h <= 1) return;

    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    RectF bounds(0.0f, 0.0f, (float)w, (float)h);
    RectF inner = GraphInnerFromBounds(bounds);

    // Outer background
    {
        SolidBrush bg(Color(255, 16, 16, 16));
        g.FillRectangle(&bg, bounds);
    }

    // Inner plot background
    {
        SolidBrush plotBg(Color(255, 28, 28, 28));
        g.FillRectangle(&plotBg, inner);
    }

    // Frame
    {
        Pen frame(Color(90, 255, 255, 255), 1.0f);
        g.DrawRectangle(&frame, inner);
    }

    // Grid
    {
        Pen grid(Color(22, 255, 255, 255), 1.0f);
        grid.SetDashStyle(DashStyleDot);

        for (int i = 1; i < 4; ++i)
        {
            float x = inner.X + inner.Width * (float)i / 4.0f;
            g.DrawLine(&grid, PointF(x, inner.Y), PointF(x, inner.GetBottom()));
        }
        for (int i = 1; i < 4; ++i)
        {
            float y = inner.Y + inner.Height * (float)i / 4.0f;
            g.DrawLine(&grid, PointF(inner.X, y), PointF(inner.GetRight(), y));
        }
    }

    RenderGraphContent(g, inner, ks, morphing);
}

// ----------------------------------------------------------------------------
// Live Marker
// ----------------------------------------------------------------------------
static void DrawLiveMarkerOverlay(Graphics& g, const RectF& inner, const KeyDeadzone& ks, bool morphing)
{
    if (!Ksp_IsKeySelected()) return;
    uint16_t hid = g_kspSelectedHid;
    if (hid == 0 || hid >= 256) return;

    float raw = (float)BackendUI_GetRawMilli(hid) / 1000.0f;
    float out = (float)BackendUI_GetAnalogMilli(hid) / 1000.0f;
    raw = std::clamp(raw, 0.0f, 1.0f);
    out = std::clamp(out, 0.0f, 1.0f);

    float x = ks.invert ? (1.0f - raw) : raw;
    x = std::clamp(x, 0.0f, 1.0f);

    auto evalExactY = [&](const KeyDeadzone& one) -> float
    {
        if (one.curveMode == 1)
            return EvalLinear(x, one);
        CurveMath::Curve01 c = CurveMath::FromKeyDeadzone(one);
        float y = CurveMath::EvalRationalYForX(c, x, 24);
        if (!std::isfinite(y))
            y = SampleSmoothY_Stable(one, x);
        return y;
    };

    float yCurve = 0.0f;
    if (morphing)
    {
        float e = MorphEase01();
        float y1 = evalExactY(g_kspMorph.fromKs);
        float y2 = evalExactY(g_kspMorph.toKs);
        yCurve = y1 + (y2 - y1) * e;
    }
    else
    {
        yCurve = evalExactY(ks);
    }

    // Backend output is authoritative, but it's quantized to milli-units.
    // Near the top edge this can snap to 100% one step early, so keep the
    // marker on the exact curve until x reaches the high endpoint.
    float y = out;
    if (!std::isfinite(y))
    {
        y = yCurve;
    }
    else
    {
        float xHigh = std::clamp(ks.high, 0.0f, 1.0f);
        if (y >= 0.9995f && x < (xHigh - 0.0005f))
            y = std::min(y, yCurve);
    }
    y = std::clamp(y, 0.0f, 1.0f);

    PointF p(inner.X + x * inner.Width, inner.GetBottom() - y * inner.Height);

    {
        Pen vx(Color(70, 255, 255, 255), 1.0f);
        vx.SetDashStyle(DashStyleDot);
        g.DrawLine(&vx, PointF(p.X, inner.Y), PointF(p.X, inner.GetBottom()));
        Pen hy(Color(60, 255, 255, 255), 1.0f);
        hy.SetDashStyle(DashStyleDot);
        g.DrawLine(&hy, PointF(inner.X, p.Y), PointF(inner.GetRight(), p.Y));
    }

    {
        Color fill = GpFromColorRef(UiTheme::Color_Accent(), 255);
        Color ring = Color(255, 20, 20, 20);
        float rOuter = 5.6f;
        float rInner = 3.2f;
        SolidBrush brFill(fill);
        SolidBrush brInner(Color(255, 255, 255, 255));
        Pen penRing(ring, 1.5f);
        g.FillEllipse(&brFill, p.X - rOuter, p.Y - rOuter, rOuter * 2, rOuter * 2);
        g.DrawEllipse(&penRing, p.X - rOuter, p.Y - rOuter, rOuter * 2, rOuter * 2);
        g.FillEllipse(&brInner, p.X - rInner, p.Y - rInner, rInner * 2, rInner * 2);
    }
}

// ---------------- Mouse interaction ----------------
static float DistSq2(PointF a, POINT b)
{
    float dx = a.X - (float)b.x;
    float dy = a.Y - (float)b.y;
    return dx * dx + dy * dy;
}

bool Ksp_GraphHandleMouse(HWND parent, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RectF grOuter = GraphRectF(parent);
    RectF gr = GraphInnerFromBounds(grOuter);

    POINT pt{};
    if (msg == WM_MOUSEWHEEL)
    {
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
        ScreenToClient(parent, &pt);
    }
    else
    {
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
    }

    float margin = (float)S(parent, 16);
    bool inArea =
        (pt.x >= grOuter.X - margin && pt.x <= grOuter.GetRight() + margin &&
            pt.y >= grOuter.Y - margin && pt.y <= grOuter.GetBottom() + margin);

    if (!inArea && msg == WM_LBUTTONDOWN) return false;

    KeyDeadzone ks = Ksp_GetActiveSettings();

    auto toScr = [&](float x, float y) -> PointF {
        return PointF(gr.X + x * gr.Width, gr.GetBottom() - y * gr.Height);
        };

    PointF p0 = toScr(ks.low, ks.antiDeadzone);
    PointF p3 = toScr(ks.high, ks.outputCap);
    PointF p1 = toScr(ks.cp1_x, ks.cp1_y);
    PointF p2 = toScr(ks.cp2_x, ks.cp2_y);

    float hitR = (float)S(parent, 10);
    float hitRSq = hitR * hitR;

    auto setCursor = [&](bool hover) {
        SetCursor(LoadCursorW(nullptr, hover ? IDC_HAND : IDC_ARROW));
        };

    switch (msg)
    {
    case WM_LBUTTONDOWN:
    {
        DragTarget old = g_drag;
        g_drag = DragTarget::None;

        if (DistSq2(p0, pt) <= hitRSq) g_drag = DragTarget::Start;
        else if (DistSq2(p3, pt) <= hitRSq) g_drag = DragTarget::End;
        else if (DistSq2(p1, pt) <= hitRSq) g_drag = DragTarget::C1;
        else if (DistSq2(p2, pt) <= hitRSq) g_drag = DragTarget::C2;

        if (g_drag != DragTarget::None)
        {
            SetFocus(parent);
            SetCapture(parent);

            // FIX #2: update hint immediately when a CP is grabbed
            if (g_drag == DragTarget::C1 || g_drag == DragTarget::C2)
                InvalidateCpHint(parent);

            if (old != g_drag) InvalidateRect(parent, nullptr, FALSE);
            UpdateInfoLabelIfNeeded(true);
            return true;
        }
        return false;
    }

    case WM_MOUSEMOVE:
    {
        if (g_drag == DragTarget::None)
        {
            bool hover =
                (DistSq2(p0, pt) <= hitRSq) || (DistSq2(p3, pt) <= hitRSq) ||
                (DistSq2(p1, pt) <= hitRSq) || (DistSq2(p2, pt) <= hitRSq);

            if (hover != g_hoverHandle)
            {
                g_hoverHandle = hover;
                setCursor(hover);
            }
            return false;
        }

        float nx = (float)(pt.x - gr.X) / gr.Width;
        float ny = (float)(gr.GetBottom() - pt.y) / gr.Height;
        nx = std::clamp(nx, 0.0f, 1.0f);
        ny = std::clamp(ny, 0.0f, 1.0f);

        float minGap = 0.01f;

        if (g_drag == DragTarget::Start)
        {
            if (nx > ks.high - minGap) nx = ks.high - minGap;
            ks.low = nx;
            ks.antiDeadzone = ny;
            ks.cp1_x = std::clamp(ks.cp1_x, ks.low + minGap, ks.high - minGap);
            ks.cp2_x = std::clamp(ks.cp2_x, ks.cp1_x + minGap, ks.high - minGap);
            Ksp_SaveActiveSettings(ks);
        }
        else if (g_drag == DragTarget::End)
        {
            if (nx < ks.low + minGap) nx = ks.low + minGap;
            ks.high = nx;
            ks.outputCap = ny;
            ks.cp1_x = std::clamp(ks.cp1_x, ks.low + minGap, ks.high - minGap);
            ks.cp2_x = std::clamp(ks.cp2_x, ks.cp1_x + minGap, ks.high - minGap);
            Ksp_SaveActiveSettings(ks);
        }
        else if (g_drag == DragTarget::C1)
        {
            ks.cp1_x = std::clamp(nx, ks.low + minGap, ks.cp2_x - minGap);
            ks.cp1_y = ny;
            Ksp_SaveActiveSettings(ks);
        }
        else if (g_drag == DragTarget::C2)
        {
            ks.cp2_x = std::clamp(nx, ks.cp1_x + minGap, ks.high - minGap);
            ks.cp2_y = ny;
            Ksp_SaveActiveSettings(ks);
        }

        UpdateInfoLabelIfNeeded(false);
        RECT rcInv = GraphRectR(parent);
        InvalidateRect(parent, &rcInv, FALSE);
        return true;
    }

    case WM_MOUSEWHEEL:
    {
        if (g_drag != DragTarget::C1 && g_drag != DragTarget::C2) return false;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta == 0) return true;

        float step = 0.05f;
        float d = step * (float)delta / 120.0f;

        if (g_drag == DragTarget::C1)
            ks.cp1_w = std::clamp(ks.cp1_w + d, 0.0f, 1.0f);
        else
            ks.cp2_w = std::clamp(ks.cp2_w + d, 0.0f, 1.0f);

        Ksp_SaveActiveSettings(ks);

        RECT rcInv = GraphRectR(parent);
        InvalidateRect(parent, &rcInv, FALSE);

        // FIX #2: invalidate hint so percent updates live
        InvalidateCpHint(parent);

        UpdateInfoLabelIfNeeded(false);
        Ksp_RequestSave(parent);
        return true;
    }

    case WM_LBUTTONUP:
        if (g_drag != DragTarget::None)
        {
            DragTarget old = g_drag;
            g_drag = DragTarget::None;
            ReleaseCapture();

            // FIX #2: hide/update hint immediately on release
            if (old == DragTarget::C1 || old == DragTarget::C2)
                InvalidateCpHint(parent);

            if (old != DragTarget::None) InvalidateRect(parent, nullptr, FALSE);
            UpdateInfoLabelIfNeeded(true);
            Ksp_SyncUI();
            InvalidateRect(parent, nullptr, FALSE);
            Ksp_RequestSave(parent);
            return true;
        }
        return false;
    }

    return false;
}

// ----------------------------------------------------------------------------
// Ksp_GraphDraw
// ----------------------------------------------------------------------------
void Ksp_GraphDraw(HDC hdc, const RECT&)
{
    HWND ref = g_kspParent ? g_kspParent : GetActiveWindow();
    if (!ref) ref = GetDesktopWindow();

    RectF bounds = GraphRectF(ref);
    int w = (int)std::lround(bounds.Width);
    int h = (int)std::lround(bounds.Height);
    if (w <= 1 || h <= 1) return;

    KeyDeadzone ks = Ksp_GetVisualCurve();
    bool morphing = g_kspMorph.running;

    // Always redraw cache while morphing (animation frames)
    if (morphing)
        g_cache.valid = false;

    if (!GraphCache_Ensure(w, h))
    {
        SaveDC(hdc);
        SetViewportOrgEx(hdc, (int)bounds.X, (int)bounds.Y, nullptr);
        RenderGraphToDC(hdc, w, h, ks, morphing);
        RestoreDC(hdc, -1);
    }
    else
    {
        KspGraphMode mode = Ksp_GetActiveMode();

        bool need =
            !g_cache.valid ||
            g_cache.w != w || g_cache.h != h ||
            mode != g_cache.lastMode ||
            !SameKs2(ks, g_cache.lastKs) ||
            (morphing != g_cache.lastMorphing); // IMPORTANT: redraw on morph end

        if (need)
        {
            RenderGraphToDC(g_cache.memDC, w, h, ks, morphing);
            g_cache.lastKs = ks;
            g_cache.lastMode = mode;
            g_cache.lastMorphing = morphing;
            g_cache.valid = true;
        }

        BitBlt(hdc,
            (int)std::lround(bounds.X), (int)std::lround(bounds.Y),
            w, h,
            g_cache.memDC, 0, 0, SRCCOPY);
    }

    // Overlays: Live marker
    {
        Graphics gg(hdc);
        gg.SetSmoothingMode(SmoothingModeAntiAlias);
        gg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        gg.SetCompositingQuality(CompositingQualityHighQuality);
        gg.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        gg.SetClip(bounds);
        RectF innerAbs = GraphInnerFromBounds(bounds);

        DrawLiveMarkerOverlay(gg, innerAbs, ks, morphing);
    }

    // Handles (on top)
    {
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetClip(bounds);

        RectF inner = GraphInnerFromBounds(bounds);
        auto toScr = [&](float x, float y) -> PointF {
            return PointF(inner.X + x * inner.Width, inner.GetBottom() - y * inner.Height);
            };

        PointF p0 = toScr(ks.low, ks.antiDeadzone);
        PointF p3 = toScr(ks.high, ks.outputCap);
        PointF p1 = toScr(ks.cp1_x, ks.cp1_y);
        PointF p2 = toScr(ks.cp2_x, ks.cp2_y);

        Color hFill(255, 255, 255, 255), hOutline(255, 0, 0, 0);
        DrawHandle(g, p0, 5.0f, true, hFill, hOutline);
        DrawHandle(g, p3, 5.0f, true, hFill, hOutline);
        DrawHandle(g, p1, 4.0f, false, hFill, hOutline);
        DrawHandle(g, p2, 4.0f, false, hFill, hOutline);

        float w1 = std::clamp(ks.cp1_w, 0.0f, 1.0f);
        float w2 = std::clamp(ks.cp2_w, 0.0f, 1.0f);
        BYTE a1 = (BYTE)std::clamp((int)std::lround(20.0f + 140.0f * w1), 0, 255);
        BYTE a2 = (BYTE)std::clamp((int)std::lround(20.0f + 140.0f * w2), 0, 255);

        Pen g1(Color(a1, 255, 255, 255), 1.0f);
        Pen g2(Color(a2, 255, 255, 255), 1.0f);
        g1.SetDashStyle(DashStyleDot);
        g2.SetDashStyle(DashStyleDot);

        g.DrawLine(&g1, p0, p1);
        g.DrawLine(&g2, p3, p2);
    }
}

// -----------------------------------------------------------------------------
// Shutdown/cleanup
// -----------------------------------------------------------------------------
void KeySettingsPanel_Shutdown()
{
    if (g_kspParent && GetCapture() == g_kspParent)
        ReleaseCapture();

    g_drag = DragTarget::None;
    g_hoverHandle = false;
    g_lastInfoText.clear();

    GraphCache_Free();
}
