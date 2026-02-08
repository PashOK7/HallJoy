#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "remap_dpad.h"

using namespace Gdiplus;

// Hard-fixed styling (your tuned values)
static constexpr COLORREF kFillBase = RGB(124, 132, 141);
static constexpr COLORREF kFillActive = RGB(255, 212, 92);
static constexpr COLORREF kBorder = RGB(140, 140, 140);

static constexpr float kActiveFillDepthRatio = 0.55f;

static constexpr float kBorderWidthRatio = 0.055f;
static constexpr float kBorderWidthPxMin = 1.0f;

static constexpr float kArmThicknessRatio = 0.3f;
static constexpr float kOuterCornerRadiusRatio = 0.20f;
static constexpr float kInnerCornerRadiusRatio = 0.08f;

static constexpr bool  kUseIncomingPadRatio = true;
static constexpr float kFixedOuterPadRatio = 0.12f;
static constexpr float kOuterPadMin = 0.04f;
static constexpr float kOuterPadMax = 0.30f;

static int Clamp255(int v) { return (v < 0) ? 0 : (v > 255) ? 255 : v; }

static COLORREF Brighten(COLORREF c, int add)
{
    return RGB(
        Clamp255((int)GetRValue(c) + add),
        Clamp255((int)GetGValue(c) + add),
        Clamp255((int)GetBValue(c) + add));
}

static Color Gp(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static float ClampF(float v, float a, float b) { return (v < a) ? a : (v > b) ? b : v; }

// Full D-pad cross path with outer/inner rounding.
static void AddDpadPath(GraphicsPath& path, float x, float y, float size,
    float armW, float outerR, float innerR)
{
    float cx = x + size * 0.5f;
    float cy = y + size * 0.5f;

    float halfW = armW * 0.5f;
    float halfS = size * 0.5f;

    outerR = std::min(outerR, halfW);
    innerR = std::min(innerR, (halfS - halfW));
    if (innerR < 0) innerR = 0;

    path.StartFigure();

    // Top arm
    path.AddArc(cx - halfW, y, outerR * 2, outerR * 2, 180.0f, 90.0f);
    path.AddLine(cx - halfW + outerR, y, cx + halfW - outerR, y);
    path.AddArc(cx + halfW - outerR * 2, y, outerR * 2, outerR * 2, 270.0f, 90.0f);
    path.AddLine(cx + halfW, y + outerR, cx + halfW, cy - halfW - innerR);

    if (innerR > 0.01f)
        path.AddArc(cx + halfW, cy - halfW - innerR * 2, innerR * 2, innerR * 2, 180.0f, -90.0f);
    else
        path.AddLine(cx + halfW, cy - halfW, cx + halfW, cy - halfW);

    // Right arm
    path.AddLine(cx + halfW + innerR, cy - halfW, x + size - outerR, cy - halfW);
    path.AddArc(x + size - outerR * 2, cy - halfW, outerR * 2, outerR * 2, 270.0f, 90.0f);
    path.AddLine(x + size, cy - halfW + outerR, x + size, cy + halfW - outerR);
    path.AddArc(x + size - outerR * 2, cy + halfW - outerR * 2, outerR * 2, outerR * 2, 0.0f, 90.0f);
    path.AddLine(x + size - outerR, cy + halfW, cx + halfW + innerR, cy + halfW);

    if (innerR > 0.01f)
        path.AddArc(cx + halfW, cy + halfW, innerR * 2, innerR * 2, 270.0f, -90.0f);
    else
        path.AddLine(cx + halfW, cy + halfW, cx + halfW, cy + halfW);

    // Bottom arm
    path.AddLine(cx + halfW, cy + halfW + innerR, cx + halfW, y + size - outerR);
    path.AddArc(cx + halfW - outerR * 2, y + size - outerR * 2, outerR * 2, outerR * 2, 0.0f, 90.0f);
    path.AddLine(cx + halfW - outerR, y + size, cx - halfW + outerR, y + size);
    path.AddArc(cx - halfW, y + size - outerR * 2, outerR * 2, outerR * 2, 90.0f, 90.0f);
    path.AddLine(cx - halfW, y + size - outerR, cx - halfW, cy + halfW + innerR);

    if (innerR > 0.01f)
        path.AddArc(cx - halfW - innerR * 2, cy + halfW, innerR * 2, innerR * 2, 0.0f, -90.0f);
    else
        path.AddLine(cx - halfW, cy + halfW, cx - halfW, cy + halfW);

    // Left arm
    path.AddLine(cx - halfW - innerR, cy + halfW, x + outerR, cy + halfW);
    path.AddArc(x, cy + halfW - outerR * 2, outerR * 2, outerR * 2, 90.0f, 90.0f);
    path.AddLine(x, cy + halfW - outerR, x, cy - halfW + outerR);
    path.AddArc(x, cy - halfW, outerR * 2, outerR * 2, 180.0f, 90.0f);
    path.AddLine(x + outerR, cy - halfW, cx - halfW - innerR, cy - halfW);

    if (innerR > 0.01f)
        path.AddArc(cx - halfW - innerR * 2, cy - halfW - innerR * 2, innerR * 2, innerR * 2, 90.0f, -90.0f);
    else
        path.AddLine(cx - halfW, cy - halfW, cx - halfW, cy - halfW);

    path.CloseFigure();
}

static RectF GetArmRect(float x, float y, float size, float fillDepth, BindAction act)
{
    switch (act)
    {
    case BindAction::Btn_DU: return RectF(x, y, size, fillDepth);
    case BindAction::Btn_DD: return RectF(x, y + size - fillDepth, size, fillDepth);
    case BindAction::Btn_DL: return RectF(x, y, fillDepth, size);
    case BindAction::Btn_DR: return RectF(x + size - fillDepth, y, fillDepth, size);
    default: return RectF(0, 0, 0, 0);
    }
}

void RemapDpad_DrawGlyphAA(HDC hdc, const RECT& rc,
    BindAction action,
    COLORREF baseColor,
    bool brightFill,
    float padRatioIncoming)
{
    COLORREF activeFill = (baseColor == CLR_INVALID) ? kFillActive : baseColor;
    if (brightFill) activeFill = Brighten(activeFill, 8);

    int w0 = rc.right - rc.left;
    int h0 = rc.bottom - rc.top;
    if (w0 <= 2 || h0 <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    float s = (float)std::min(w0, h0);
    float x = (float)rc.left + (w0 - s) * 0.5f;
    float y = (float)rc.top + (h0 - s) * 0.5f;

    float padRatio = kUseIncomingPadRatio ? padRatioIncoming : kFixedOuterPadRatio;
    padRatio = ClampF(padRatio, kOuterPadMin, kOuterPadMax);
    float pad = std::max(1.0f, std::round(s * padRatio));

    float dpadSize = s - 2.0f * pad;
    float dpadX = x + pad;
    float dpadY = y + pad;

    float armW = dpadSize * kArmThicknessRatio;
    float outerR = armW * kOuterCornerRadiusRatio;
    float innerR = dpadSize * kInnerCornerRadiusRatio;
    float borderW = std::max(kBorderWidthPxMin, (float)std::lround(s * kBorderWidthRatio));

    float fillDepth = (dpadSize * 0.5f) * kActiveFillDepthRatio;

    GraphicsPath pathFull;
    AddDpadPath(pathFull, dpadX, dpadY, dpadSize, armW, outerR, innerR);

    SolidBrush brushBase(Gp(kFillBase, 255));
    g.FillPath(&brushBase, &pathFull);

    {
        Region region(&pathFull);
        RectF armRect = GetArmRect(dpadX, dpadY, dpadSize, fillDepth, action);
        region.Intersect(armRect);

        SolidBrush brushActive(Gp(activeFill, 255));
        g.FillRegion(&brushActive, &region);
    }

    Pen penBorder(Gp(kBorder, 255), borderW);
    penBorder.SetLineJoin(LineJoinRound);
    g.DrawPath(&penBorder, &pathFull);
}