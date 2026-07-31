#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "remap_triggers.h"

using namespace Gdiplus;

// Hard-fixed styling (your tuned values)
static constexpr bool  kIgnoreBrightFill = true;
static constexpr int   kBrightenAddIfPressed = 10;

static constexpr bool     kUseFixedColors = true;
static constexpr COLORREF kFill = RGB(124, 132, 141);
static constexpr COLORREF kBorder = RGB(140, 140, 140);
static constexpr COLORREF kText = RGB(255, 255, 255);

static constexpr int      kDeriveFillAdd = 22;
static constexpr float    kDeriveBorderMul = 0.78f;

static constexpr float    kBorderWidthRatio = 0.055f;
static constexpr float    kBorderWidthPxMin = 1.0f;

static constexpr bool     kUseIncomingPadRatio = true;
static constexpr float    kFixedOuterPadRatio = 0.12f;
static constexpr float    kOuterPadMin = 0.04f;
static constexpr float    kOuterPadMax = 0.30f;

static constexpr float    kTriggerWidthRatio = 1.00f;
static constexpr float    kTriggerHeightRatio = 0.9f;

static constexpr float    kTopCornerRxOfWidth = 0.50f;
static constexpr float    kTopCornerRyOfHeight = 0.85f;
static constexpr float    kBottomCornerRRatio = 0.12f;

static constexpr bool     kDrawText = true;
static constexpr float    kTextEmHeightRatio = 0.75f;
static constexpr float    kTextEmMinPx = 9.0f;
static constexpr float    kTextEmMaxPx = 16.0f;

static int Clamp255(int v) { return (v < 0) ? 0 : (v > 255) ? 255 : v; }

static COLORREF Brighten(COLORREF c, int add)
{
    return RGB(
        Clamp255((int)GetRValue(c) + add),
        Clamp255((int)GetGValue(c) + add),
        Clamp255((int)GetBValue(c) + add));
}

static COLORREF DarkenMul(COLORREF c, float mul)
{
    mul = std::clamp(mul, 0.0f, 1.0f);
    return RGB(
        Clamp255((int)std::lround(GetRValue(c) * mul)),
        Clamp255((int)std::lround(GetGValue(c) * mul)),
        Clamp255((int)std::lround(GetBValue(c) * mul)));
}

static Color Gp(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static float ClampF(float v, float a, float b) { return (v < a) ? a : (v > b) ? b : v; }

static RectF MakeCenteredRect(const RectF& bounds, float w, float h)
{
    RectF r;
    r.Width = w;
    r.Height = h;
    r.X = bounds.X + (bounds.Width - w) * 0.5f;
    r.Y = bounds.Y + (bounds.Height - h) * 0.5f;
    return r;
}

static void AddRoundRectPathAsymXY(GraphicsPath& path, const RectF& r,
    float rtlx, float rtly,
    float rtrx, float rtry,
    float rbrx, float rbry,
    float rblx, float rbly)
{
    float w = r.Width;
    float h = r.Height;

    auto clampPos = [](float v) { return (v < 0.0f) ? 0.0f : v; };
    rtlx = clampPos(rtlx); rtly = clampPos(rtly);
    rtrx = clampPos(rtrx); rtry = clampPos(rtry);
    rbrx = clampPos(rbrx); rbry = clampPos(rbry);
    rblx = clampPos(rblx); rbly = clampPos(rbly);

    rtlx = std::min(rtlx, w);  rtrx = std::min(rtrx, w);
    rblx = std::min(rblx, w);  rbrx = std::min(rbrx, w);

    rtly = std::min(rtly, h);  rtry = std::min(rtry, h);
    rbly = std::min(rbly, h);  rbry = std::min(rbry, h);

    auto scalePairToFit = [](float& a, float& b, float maxSum)
        {
            float sum = a + b;
            if (sum > maxSum && sum > 0.0f)
            {
                float k = maxSum / sum;
                a *= k; b *= k;
            }
        };

    scalePairToFit(rtlx, rtrx, w);
    scalePairToFit(rblx, rbrx, w);
    scalePairToFit(rtly, rbly, h);
    scalePairToFit(rtry, rbry, h);

    const float left = r.X;
    const float top = r.Y;
    const float right = r.GetRight();
    const float bottom = r.GetBottom();

    path.StartFigure();

    path.AddLine(PointF(left + rtlx, top), PointF(right - rtrx, top));

    if (rtrx > 0.0f && rtry > 0.0f)
        path.AddArc(right - 2.0f * rtrx, top, 2.0f * rtrx, 2.0f * rtry, 270.0f, 90.0f);

    path.AddLine(PointF(right, top + rtry), PointF(right, bottom - rbry));

    if (rbrx > 0.0f && rbry > 0.0f)
        path.AddArc(right - 2.0f * rbrx, bottom - 2.0f * rbry, 2.0f * rbrx, 2.0f * rbry, 0.0f, 90.0f);

    path.AddLine(PointF(right - rbrx, bottom), PointF(left + rblx, bottom));

    if (rblx > 0.0f && rbly > 0.0f)
        path.AddArc(left, bottom - 2.0f * rbly, 2.0f * rblx, 2.0f * rbly, 90.0f, 90.0f);

    path.AddLine(PointF(left, bottom - rbly), PointF(left, top + rtly));

    if (rtlx > 0.0f && rtly > 0.0f)
        path.AddArc(left, top, 2.0f * rtlx, 2.0f * rtly, 180.0f, 90.0f);

    path.CloseFigure();
}

static void DrawTextCenteredNoOutline(Graphics& g, const WCHAR* txt, const RectF& r, float emPx, COLORREF color)
{
    FontFamily ff(L"Segoe UI");
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    GraphicsPath path;
    path.AddString(txt, -1, &ff, FontStyleBold, emPx, r, &fmt);

    SolidBrush br(Gp(color, 255));
    g.FillPath(&br, &path);
}

void RemapTriggers_DrawGlyphAA(HDC hdc, const RECT& rc,
    BindAction action, COLORREF baseColor,
    bool brightFill, float padRatioIncoming,
    COLORREF borderOverride)
{
    int w0 = rc.right - rc.left;
    int h0 = rc.bottom - rc.top;
    if (w0 <= 2 || h0 <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    float s = (float)std::min(w0, h0);
    float x = (float)rc.left + (w0 - s) * 0.5f;
    float y = (float)rc.top + (h0 - s) * 0.5f;
    RectF tile(x, y, s, s);

    float padRatio = kUseIncomingPadRatio ? padRatioIncoming : kFixedOuterPadRatio;
    padRatio = ClampF(padRatio, kOuterPadMin, kOuterPadMax);
    float pad = std::max(1.0f, std::round(s * padRatio));

    RectF content = tile;
    content.Inflate(-pad, -pad);

    RectF trig = MakeCenteredRect(content,
        content.Width * kTriggerWidthRatio,
        content.Height * kTriggerHeightRatio);

    COLORREF fillRef{}, borderRef{}, textRef{};
    if (kUseFixedColors)
    {
        fillRef = kFill;
        borderRef = kBorder;
        textRef = kText;
    }
    else
    {
        fillRef = Brighten(baseColor, kDeriveFillAdd);
        borderRef = DarkenMul(fillRef, kDeriveBorderMul);
        textRef = kText;
    }

    if (!kIgnoreBrightFill && brightFill)
        fillRef = Brighten(fillRef, kBrightenAddIfPressed);

    if (borderOverride != CLR_INVALID) borderRef = borderOverride;

    float borderW = std::max(kBorderWidthPxMin, (float)std::lround(s * kBorderWidthRatio));

    float topRx = trig.Width * kTopCornerRxOfWidth;
    float topRy = trig.Height * kTopCornerRyOfHeight;
    float botR = trig.Height * kBottomCornerRRatio;

    GraphicsPath p;
    AddRoundRectPathAsymXY(p, trig,
        topRx, topRy,
        topRx, topRy,
        botR, botR,
        botR, botR);

    SolidBrush fill(Gp(fillRef, 255));
    Pen border(Gp(borderRef, 255), borderW);
    border.SetLineJoin(LineJoinRound);

    g.FillPath(&fill, &p);
    g.DrawPath(&border, &p);

    if (kDrawText)
    {
        const WCHAR* txt = (action == BindAction::Trigger_LT) ? L"LT" : L"RT";
        float em = std::clamp(trig.Height * kTextEmHeightRatio, kTextEmMinPx, kTextEmMaxPx);
        DrawTextCenteredNoOutline(g, txt, trig, em, textRef);
    }
}