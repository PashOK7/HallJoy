#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "remap_startselect.h"

using namespace Gdiplus;

// Hard-fixed styling (your tuned values)
static constexpr bool     kUseFixedColors = true;

static constexpr COLORREF kBgNormal = RGB(220, 220, 220);
static constexpr COLORREF kBgPressed = RGB(220, 220, 220);

static constexpr COLORREF kFrameNormal = RGB(160, 160, 160);
static constexpr COLORREF kFramePressed = RGB(160, 160, 160);

static constexpr COLORREF kGlyphNormal = RGB(45, 45, 45);
static constexpr COLORREF kGlyphPressed = RGB(45, 45, 45);

static constexpr int      kDeriveBgAddNormal = 22;
static constexpr int      kDeriveBgAddPressed = 22;

static constexpr float    kFrameWidthRatio = 0.055f;
static constexpr float    kFrameWidthPxMin = 1.0f;

static constexpr float    kGlyphWidthRatio = 0.055f;
static constexpr float    kGlyphWidthPxMin = 1.0f;

static constexpr bool     kUseIncomingPadRatio = true;
static constexpr float    kFixedOuterPadRatio = 0.12f;
static constexpr float    kOuterPadMin = 0.04f;
static constexpr float    kOuterPadMax = 0.30f;

static constexpr float    kButtonWidthRatio = 0.95f;
static constexpr float    kButtonHeightRatio = 0.6f;
static constexpr float    kCornerRadiusRatio = 0.10f;

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

static void AddRoundRectPath(GraphicsPath& path, const RectF& r, float radius)
{
    float d = radius * 2.0f;
    if (d > r.Width)  d = r.Width;
    if (d > r.Height) d = r.Height;

    RectF arc(r.X, r.Y, d, d);

    path.AddArc(arc, 180, 90);
    arc.X = r.GetRight() - d;
    path.AddArc(arc, 270, 90);
    arc.Y = r.GetBottom() - d;
    path.AddArc(arc, 0, 90);
    arc.X = r.X;
    path.AddArc(arc, 90, 90);
    path.CloseFigure();
}

static RectF MakeCenteredRect(const RectF& bounds, float w, float h)
{
    RectF r;
    r.Width = w;
    r.Height = h;
    r.X = bounds.X + (bounds.Width - w) * 0.5f;
    r.Y = bounds.Y + (bounds.Height - h) * 0.5f;
    return r;
}

static void DrawButtonTile(Graphics& g, const RectF& buttonRect,
    COLORREF fillRef, COLORREF frameRef, float frameW)
{
    float radius = std::round(std::clamp(buttonRect.Height * kCornerRadiusRatio, 2.0f, 12.0f));

    GraphicsPath p;
    AddRoundRectPath(p, buttonRect, radius);

    SolidBrush fill(Gp(fillRef, 255));
    Pen frame(Gp(frameRef, 255), frameW);
    frame.SetLineJoin(LineJoinRound);

    g.FillPath(&fill, &p);
    g.DrawPath(&frame, &p);
}

static void DrawStartGlyph(Graphics& g, const RectF& buttonRect, COLORREF glyphRef, float strokeW)
{
    Pen pen(Gp(glyphRef, 255), strokeW);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);

    float x0 = buttonRect.X + buttonRect.Width * 0.22f;
    float x1 = buttonRect.X + buttonRect.Width * 0.78f;

    float yC = buttonRect.Y + buttonRect.Height * 0.50f;
    float dy = buttonRect.Height * 0.22f;

    g.DrawLine(&pen, PointF(x0, yC - dy), PointF(x1, yC - dy));
    g.DrawLine(&pen, PointF(x0, yC), PointF(x1, yC));
    g.DrawLine(&pen, PointF(x0, yC + dy), PointF(x1, yC + dy));
}

static void DrawBackGlyph(Graphics& g, const RectF& buttonRect, COLORREF glyphRef, float strokeW)
{
    Pen pen(Gp(glyphRef, 255), strokeW);
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);

    float w = buttonRect.Width * 0.42f;
    float h = buttonRect.Height * 0.60f;

    RectF a;
    a.Width = w;
    a.Height = h;
    a.X = buttonRect.X + buttonRect.Width * 0.42f;
    a.Y = buttonRect.Y + buttonRect.Height * 0.22f;

    RectF b = a;
    b.X -= buttonRect.Width * 0.14f;
    b.Y -= buttonRect.Height * 0.12f;

    float rr = std::round(std::clamp(buttonRect.Height * 0.18f, 2.0f, 8.0f));

    GraphicsPath p1; AddRoundRectPath(p1, b, rr);
    GraphicsPath p2; AddRoundRectPath(p2, a, rr);

    g.DrawPath(&pen, &p1);
    g.DrawPath(&pen, &p2);
}

void RemapStartSelect_DrawGlyphAA(HDC hdc, const RECT& rc,
    BindAction action,
    COLORREF baseColor,
    bool brightFill,
    float padRatioIncoming,
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

    float s = (float)std::min(w0, h0);
    float x = (float)rc.left + (w0 - s) * 0.5f;
    float y = (float)rc.top + (h0 - s) * 0.5f;
    RectF tile(x, y, s, s);

    float padRatio = kUseIncomingPadRatio ? padRatioIncoming : kFixedOuterPadRatio;
    padRatio = ClampF(padRatio, kOuterPadMin, kOuterPadMax);
    float pad = std::max(1.0f, std::round(s * padRatio));

    RectF content = tile;
    content.Inflate(-pad, -pad);

    float frameW = std::max(kFrameWidthPxMin, (float)std::lround(s * kFrameWidthRatio));
    float glyphW = std::max(kGlyphWidthPxMin, (float)std::lround(s * kGlyphWidthRatio));

    float buttonW = content.Width * kButtonWidthRatio;
    float buttonH = content.Height * kButtonHeightRatio;
    RectF buttonRect = MakeCenteredRect(content, buttonW, buttonH);

    COLORREF bgRef{}, frameRef{}, glyphRef{};

    if (kUseFixedColors)
    {
        bgRef = brightFill ? kBgPressed : kBgNormal;
        frameRef = brightFill ? kFramePressed : kFrameNormal;
        glyphRef = brightFill ? kGlyphPressed : kGlyphNormal;
    }
    else
    {
        bgRef = Brighten(baseColor, kDeriveBgAddNormal);
        if (brightFill) bgRef = Brighten(bgRef, kDeriveBgAddPressed);

        frameRef = brightFill ? kFramePressed : kFrameNormal;
        glyphRef = brightFill ? kGlyphPressed : kGlyphNormal;
    }

    if (borderOverride != CLR_INVALID) frameRef = borderOverride;

    DrawButtonTile(g, buttonRect, bgRef, frameRef, frameW);

    if (action == BindAction::Btn_Start)
        DrawStartGlyph(g, buttonRect, glyphRef, glyphW);
    else
        DrawBackGlyph(g, buttonRect, glyphRef, glyphW);
}