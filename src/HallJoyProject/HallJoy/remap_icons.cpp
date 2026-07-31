#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "remap_icons.h"
#include "remap_abxy.h"
#include "remap_startselect.h"
#include "remap_bumpers.h"
#include "remap_triggers.h"
#include "remap_dpad.h"
#include "remap_sticks.h"
#include "remap_guide.h" // NEW: Guide/Home icon moved to its own module

using namespace Gdiplus;

static const RemapIconDef g_icons[] =
{
    // Row 1 (13):  Y X dU dL Lpress LU LL RU RL LT RT start home
    {L"Y",     BindAction::Btn_Y,        RGB(240, 200, 60),  true},
    {L"X",     BindAction::Btn_X,        RGB(80, 140, 255),  true},

    {L"DU",    BindAction::Btn_DU,       RGB(180, 180, 180), false},
    {L"DL",    BindAction::Btn_DL,       RGB(180, 180, 180), false},

    {L"LS",    BindAction::Btn_LS,       RGB(180, 180, 180), false}, // Lpress

    {L"LY+",   BindAction::Axis_LY_Plus,  RGB(180, 180, 180), false}, // LU
    {L"LX-",   BindAction::Axis_LX_Minus, RGB(180, 180, 180), false}, // LL

    {L"RY+",   BindAction::Axis_RY_Plus,  RGB(180, 180, 180), false}, // RU
    {L"RX-",   BindAction::Axis_RX_Minus, RGB(180, 180, 180), false}, // RL

    {L"LT",    BindAction::Trigger_LT,    RGB(180, 180, 180), false},
    {L"RT",    BindAction::Trigger_RT,    RGB(180, 180, 180), false},

    {L"Start", BindAction::Btn_Start,     RGB(180, 180, 180), false},
    {L"Guide", BindAction::Btn_Guide,     RGB(255, 170, 0),   true},  // home


    // Row 2 (12):  A B dD dR Rpress LD LR RD RR LB RB select
    {L"A",     BindAction::Btn_A,         RGB(70, 200, 80),   true},
    {L"B",     BindAction::Btn_B,         RGB(230, 80, 80),   true},

    {L"DD",    BindAction::Btn_DD,        RGB(180, 180, 180), false},
    {L"DR",    BindAction::Btn_DR,        RGB(180, 180, 180), false},

    {L"RS",    BindAction::Btn_RS,        RGB(180, 180, 180), false}, // Rpress

    {L"LY-",   BindAction::Axis_LY_Minus, RGB(180, 180, 180), false}, // LD
    {L"LX+",   BindAction::Axis_LX_Plus,  RGB(180, 180, 180), false}, // LR

    {L"RY-",   BindAction::Axis_RY_Minus, RGB(180, 180, 180), false}, // RD
    {L"RX+",   BindAction::Axis_RX_Plus,  RGB(180, 180, 180), false}, // RR

    {L"LB",    BindAction::Btn_LB,        RGB(180, 180, 180), false},
    {L"RB",    BindAction::Btn_RB,        RGB(180, 180, 180), false},

    {L"Select",BindAction::Btn_Back,      RGB(180, 180, 180), false}, // select


    // (������ ������ � �� ��������� ������ ������ "�����������", ��� ������ ������ �������)
};

struct RemapIconStyle
{
    COLORREF accent = RGB(255, 212, 92); // default legacy accent (single gamepad)
    bool extendedAccent = false;          // extra accent outline for ABXY/LT/RT/LB/RB/Start/Select
};

static RemapIconStyle ResolveStyleVariant(int styleVariant)
{
    switch (styleVariant)
    {
    case 1: return { RGB(255, 212, 92),  true  }; // gamepad #1 in multi-pad mode (yellow + extended outlines)
    case 2: return { RGB(96, 178, 255),  true  }; // gamepad #2 (sapphire)
    case 3: return { RGB(90, 255, 144), true  }; // gamepad #3 (emerald)
    case 4: return { RGB(255, 111, 135), true  }; // gamepad #4 (ruby)
    default: return { RGB(255, 212, 92), false }; // single gamepad mode: legacy default look
    }
}

int RemapIcons_Count()
{
    return (int)(sizeof(g_icons) / sizeof(g_icons[0]));
}

const RemapIconDef& RemapIcons_Get(int idx)
{
    int n = RemapIcons_Count();
    if (n <= 0) idx = 0;
    idx = std::clamp(idx, 0, n - 1);
    return g_icons[idx];
}

static int Clamp255(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

COLORREF RemapIcons_Brighten(COLORREF c, int add)
{
    return RGB(
        Clamp255((int)GetRValue(c) + add),
        Clamp255((int)GetGValue(c) + add),
        Clamp255((int)GetBValue(c) + add));
}

static Color GpColorFromColorRef(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

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

void RemapIcons_DrawGlyphAA(HDC hdc, const RECT& rc, int iconIdx, bool brightFill, float padRatio, int styleVariant)
{
    const RemapIconDef& def = RemapIcons_Get(iconIdx);
    const RemapIconStyle style = ResolveStyleVariant(styleVariant);
    COLORREF borderAccent = style.extendedAccent ? style.accent : CLR_INVALID;

    if (def.action == BindAction::Btn_A ||
        def.action == BindAction::Btn_B ||
        def.action == BindAction::Btn_X ||
        def.action == BindAction::Btn_Y)
    {
        RemapABXY_DrawGlyphAA(hdc, rc, def.label, def.color, brightFill, padRatio, borderAccent);
        return;
    }

    if (def.action == BindAction::Btn_Start || def.action == BindAction::Btn_Back)
    {
        RemapStartSelect_DrawGlyphAA(hdc, rc, def.action, def.color, brightFill, padRatio, borderAccent);
        return;
    }

    // Guide/Home icon (moved to remap_guide.*)
    if (def.action == BindAction::Btn_Guide)
    {
        RemapGuide_DrawGlyphAA(hdc, rc, brightFill, padRatio, style.accent);
        return;
    }

    if (def.action == BindAction::Btn_LB || def.action == BindAction::Btn_RB)
    {
        RemapBumpers_DrawGlyphAA(hdc, rc, def.action, def.color, brightFill, padRatio, borderAccent);
        return;
    }

    if (def.action == BindAction::Trigger_LT || def.action == BindAction::Trigger_RT)
    {
        RemapTriggers_DrawGlyphAA(hdc, rc, def.action, def.color, brightFill, padRatio, borderAccent);
        return;
    }

    if (def.action == BindAction::Btn_DU ||
        def.action == BindAction::Btn_DD ||
        def.action == BindAction::Btn_DL ||
        def.action == BindAction::Btn_DR)
    {
        RemapDpad_DrawGlyphAA(hdc, rc, def.action, style.accent, brightFill, padRatio);
        return;
    }

    bool isStick = (def.action == BindAction::Btn_LS || def.action == BindAction::Btn_RS);
    if (!isStick)
    {
        switch (def.action)
        {
        case BindAction::Axis_LX_Minus: case BindAction::Axis_LX_Plus:
        case BindAction::Axis_LY_Minus: case BindAction::Axis_LY_Plus:
        case BindAction::Axis_RX_Minus: case BindAction::Axis_RX_Plus:
        case BindAction::Axis_RY_Minus: case BindAction::Axis_RY_Plus:
            isStick = true;
            break;
        }
    }

    if (isStick)
    {
        RemapSticks_DrawGlyphAA(hdc, rc, def.action, style.accent, brightFill, padRatio);
        return;
    }

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    RectF r((float)rc.left, (float)rc.top, (float)w, (float)h);

    padRatio = std::clamp(padRatio, 0.0f, 0.40f);
    float pad = std::max(1.0f, std::round(w * padRatio));

    RectF inner = r;
    inner.Inflate(-pad, -pad);

    COLORREF fillC = brightFill ? RemapIcons_Brighten(def.color, 15) : def.color;

    SolidBrush fill(GpColorFromColorRef(fillC, 255));
    Pen outline(GpColorFromColorRef(RGB(70, 70, 70), 255), 2.0f);

    if (def.round)
    {
        g.FillEllipse(&fill, inner);
        g.DrawEllipse(&outline, inner);
    }
    else
    {
        GraphicsPath path;
        AddRoundRectPath(path, inner, 10.0f);
        g.FillPath(&fill, &path);
        g.DrawPath(&outline, &path);
    }

    SolidBrush txt(Color(255, 20, 20, 20));
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    float fs = std::clamp(w * 0.38f, 10.0f, 16.0f);
    Font font(L"Segoe UI", fs, FontStyleBold, UnitPixel);
    g.DrawString(def.label, -1, &font, r, &fmt, &txt);
}