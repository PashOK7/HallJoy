#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "remap_sticks.h"

using namespace Gdiplus;

// Hard-fixed styling (your tuned values)
static constexpr COLORREF kFillBase = RGB(60, 60, 60);
static constexpr COLORREF kFillCap = RGB(124, 132, 141);
static constexpr COLORREF kFillActive = RGB(255, 212, 92);
static constexpr COLORREF kBorder = RGB(140, 140, 140);
static constexpr COLORREF kTextColor = RGB(255, 255, 255);

static constexpr float kCapSizeRatio = 0.62f;

static constexpr float kBorderWidthRatio = 0.015f;
static constexpr float kBorderWidthPxMin = 1.0f;

static constexpr float kAxisArcAngle = 100.0f;

static constexpr float kTextEmRatio = 0.80f;

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

static void AddCirclePath(GraphicsPath& path, float x, float y, float d)
{
    path.AddEllipse(x, y, d, d);
}

static void AddAnnularSectorPath(GraphicsPath& path, float cx, float cy,
    float rOuter, float rInner,
    float midAngle, float sweepAngle)
{
    float startAngle = midAngle - (sweepAngle * 0.5f);

    path.StartFigure();
    path.AddArc(cx - rOuter, cy - rOuter, rOuter * 2, rOuter * 2, startAngle, sweepAngle);
    path.AddArc(cx - rInner, cy - rInner, rInner * 2, rInner * 2, startAngle + sweepAngle, -sweepAngle);
    path.CloseFigure();
}

static void DrawLetterCentered(Graphics& g, const WCHAR* txt, float cx, float cy, float capSize, COLORREF color)
{
    FontFamily ff(L"Segoe UI");
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    RectF r(cx - capSize * 0.5f, cy - capSize * 0.5f, capSize, capSize);

    float em = capSize * kTextEmRatio;

    GraphicsPath path;
    path.AddString(txt, -1, &ff, FontStyleBold, em, r, &fmt);

    SolidBrush br(Gp(color, 255));
    g.FillPath(&br, &path);
}

enum class StickHighlight { None, Full, Sector };

struct StickInfo {
    WCHAR letter;
    StickHighlight hl;
    float angle; // screen-space: 0=R, 90=D, 180=L, 270=U
};

static StickInfo GetStickInfo(BindAction act)
{
    StickInfo info = { L'?', StickHighlight::None, 0.0f };

    if (act == BindAction::Btn_LS) return { L'L', StickHighlight::Full,   0.0f };
    if (act == BindAction::Axis_LX_Plus) return { L'L', StickHighlight::Sector, 0.0f };
    if (act == BindAction::Axis_LX_Minus) return { L'L', StickHighlight::Sector, 180.0f };
    if (act == BindAction::Axis_LY_Plus) return { L'L', StickHighlight::Sector, 270.0f };
    if (act == BindAction::Axis_LY_Minus) return { L'L', StickHighlight::Sector, 90.0f };

    if (act == BindAction::Btn_RS) return { L'R', StickHighlight::Full,   0.0f };
    if (act == BindAction::Axis_RX_Plus) return { L'R', StickHighlight::Sector, 0.0f };
    if (act == BindAction::Axis_RX_Minus) return { L'R', StickHighlight::Sector, 180.0f };
    if (act == BindAction::Axis_RY_Plus) return { L'R', StickHighlight::Sector, 270.0f };
    if (act == BindAction::Axis_RY_Minus) return { L'R', StickHighlight::Sector, 90.0f };

    return info;
}

void RemapSticks_DrawGlyphAA(HDC hdc, const RECT& rc,
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
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    float s = (float)std::min(w0, h0);
    float x = (float)rc.left + (w0 - s) * 0.5f;
    float y = (float)rc.top + (h0 - s) * 0.5f;

    float padRatio = kUseIncomingPadRatio ? padRatioIncoming : kFixedOuterPadRatio;
    padRatio = ClampF(padRatio, kOuterPadMin, kOuterPadMax);
    float pad = std::max(1.0f, std::round(s * padRatio));

    float stickSize = s - 2.0f * pad;
    float cx = x + s * 0.5f;
    float cy = y + s * 0.5f;
    float rOuter = stickSize * 0.5f;

    float capSize = stickSize * kCapSizeRatio;
    float rInner = capSize * 0.5f;

    float borderW = std::max(kBorderWidthPxMin, (float)std::lround(s * kBorderWidthRatio));

    StickInfo info = GetStickInfo(action);

    {
        GraphicsPath pathBase;
        AddCirclePath(pathBase, cx - rOuter, cy - rOuter, rOuter * 2.0f);
        SolidBrush brBase(Gp(kFillBase, 255));
        g.FillPath(&brBase, &pathBase);
    }

    if (info.hl != StickHighlight::None)
    {
        GraphicsPath pathHl;
        if (info.hl == StickHighlight::Full)
        {
            pathHl.AddEllipse(cx - rOuter, cy - rOuter, rOuter * 2, rOuter * 2);
            pathHl.AddEllipse(cx - rInner, cy - rInner, rInner * 2, rInner * 2);
        }
        else
        {
            AddAnnularSectorPath(pathHl, cx, cy, rOuter, rInner, info.angle, kAxisArcAngle);
        }

        SolidBrush brActive(Gp(activeFill, 255));
        g.FillPath(&brActive, &pathHl);
    }

    {
        GraphicsPath pathCap;
        AddCirclePath(pathCap, cx - rInner, cy - rInner, rInner * 2.0f);

        SolidBrush brCap(Gp(kFillCap, 255));
        g.FillPath(&brCap, &pathCap);

        Pen penBorder(Gp(kBorder, 255), borderW);
        g.DrawPath(&penBorder, &pathCap);
    }

    {
        GraphicsPath pathOuter;
        AddCirclePath(pathOuter, cx - rOuter, cy - rOuter, rOuter * 2.0f);
        Pen penBorder(Gp(kBorder, 255), borderW);
        g.DrawPath(&penBorder, &pathOuter);
    }

    WCHAR buf[2] = { info.letter, 0 };
    DrawLetterCentered(g, buf, cx, cy, capSize, kTextColor);
}