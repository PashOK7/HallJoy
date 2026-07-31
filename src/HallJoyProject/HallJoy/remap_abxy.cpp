#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "remap_abxy.h"

using namespace Gdiplus;

// Hard-fixed styling (your tuned values)
static constexpr int   kBrightenAdd = 8;

static constexpr bool  kUsePerButtonBorderColors = true;
static constexpr float kBorderDarkMul = 0.78f;
static constexpr float kBorderWidthRatio = 0.065f;

static constexpr COLORREF kFillA = RGB(50, 200, 60);
static constexpr COLORREF kFillB = RGB(255, 70, 70);
static constexpr COLORREF kFillX = RGB(65, 120, 255);
static constexpr COLORREF kFillY = RGB(216, 164, 8);

static constexpr COLORREF kBorderA = RGB(40, 165, 40);
static constexpr COLORREF kBorderB = RGB(190, 50, 50);
static constexpr COLORREF kBorderX = RGB(30, 75, 210);
static constexpr COLORREF kBorderY = RGB(161, 98, 7);

static constexpr COLORREF kLetterColor = RGB(255, 255, 255);

static constexpr float kPadMin = 0.06f;
static constexpr float kPadMax = 0.20f;

static constexpr float kLetterEmRatio = 0.62f;
static constexpr float kLetterEmMinPx = 11.0f;
static constexpr float kLetterEmMaxPx = 20.0f;

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
    mul = (mul < 0.0f) ? 0.0f : (mul > 1.0f ? 1.0f : mul);
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

static void DrawLetterNoOutline(Graphics& g, const WCHAR* text, const RectF& r, float emPx)
{
    FontFamily ff(L"Segoe UI");
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    GraphicsPath path;
    path.AddString(text, -1, &ff, FontStyleBold, emPx, r, &fmt);

    SolidBrush br(Gp(kLetterColor, 255));
    g.FillPath(&br, &path);
}

static COLORREF GetFillForLetter(const wchar_t* letter, COLORREF fallback)
{
    if (!letter || !letter[0]) return fallback;
    switch (letter[0])
    {
    case L'A': return kFillA;
    case L'B': return kFillB;
    case L'X': return kFillX;
    case L'Y': return kFillY;
    default:   return fallback;
    }
}

static COLORREF GetBorderForLetter(const wchar_t* letter, COLORREF derivedBorder)
{
    if (!kUsePerButtonBorderColors) return derivedBorder;

    if (!letter || !letter[0]) return derivedBorder;
    switch (letter[0])
    {
    case L'A': return kBorderA;
    case L'B': return kBorderB;
    case L'X': return kBorderX;
    case L'Y': return kBorderY;
    default:   return derivedBorder;
    }
}

void RemapABXY_DrawGlyphAA(HDC hdc, const RECT& rc,
    const wchar_t* letter, COLORREF color,
    bool brightFill, float padRatio,
    COLORREF borderOverride)
{
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    float s = (float)std::min(w, h);
    float x = (float)rc.left + (w - s) * 0.5f;
    float y = (float)rc.top + (h - s) * 0.5f;
    RectF tile(x, y, s, s);

    COLORREF baseFill = GetFillForLetter(letter, color);

    padRatio = ClampF(padRatio, kPadMin, kPadMax);
    float pad = std::max(1.0f, std::round(s * padRatio));

    RectF circle = tile;
    circle.Inflate(-pad, -pad);

    COLORREF fillRef = brightFill ? Brighten(baseFill, kBrightenAdd) : baseFill;

    COLORREF derivedBorder = DarkenMul(fillRef, kBorderDarkMul);
    COLORREF borderRef = (borderOverride != CLR_INVALID) ? borderOverride : GetBorderForLetter(letter, derivedBorder);

    SolidBrush fill(Gp(fillRef, 255));

    float bw = std::max(1.0f, std::round(s * kBorderWidthRatio));
    Pen border(Gp(borderRef, 255), bw);
    border.SetLineJoin(LineJoinRound);

    g.FillEllipse(&fill, circle);
    g.DrawEllipse(&border, circle);

    float em = ClampF(circle.Height * kLetterEmRatio, kLetterEmMinPx, kLetterEmMaxPx);
    DrawLetterNoOutline(g, letter, tile, em);
}