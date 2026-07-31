#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include "custom_page_controls.h"
#include "ui_theme.h"
#include "win_util.h"

using namespace Gdiplus;

static int CpcScale(HWND hwnd, int px)
{
    return WinUtil_ScalePx(hwnd, px);
}

static Color CpcColor(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void CpcAddRoundRectPath(GraphicsPath& path, const RectF& r, float rad)
{
    float d = rad * 2.0f;
    if (d <= 0.0f)
    {
        path.AddRectangle(r);
        return;
    }
    path.AddArc(r.X, r.Y, d, d, 180.0f, 90.0f);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270.0f, 90.0f);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

void CustomPage_DrawText(HDC hdc, const std::wstring& text, RECT rc, COLORREF color, UINT fmt)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text.c_str(), -1, &rc, fmt);
}

void CustomPage_DrawRoundRect(Graphics& g, const RECT& rc, COLORREF fill, COLORREF border, float radius, BYTE alpha)
{
    RectF r((REAL)rc.left, (REAL)rc.top, (REAL)(rc.right - rc.left), (REAL)(rc.bottom - rc.top));
    r.Inflate(-0.5f, -0.5f);
    GraphicsPath p;
    CpcAddRoundRectPath(p, r, radius);
    SolidBrush br(CpcColor(fill, alpha));
    g.FillPath(&br, &p);
    Pen pen(CpcColor(border, alpha), 1.0f);
    g.DrawPath(&pen, &p);
}

void CustomPage_DrawButton(Graphics& g, HDC hdc, const RECT& rc, const std::wstring& text, bool hot, bool pressed, bool enabled)
{
    COLORREF fill = pressed ? RGB(42, 42, 44) : (hot ? RGB(40, 40, 42) : UiTheme::Color_ControlBg());
    CustomPage_DrawRoundRect(g, rc, fill, UiTheme::Color_Border(), 3.0f, enabled ? 255 : 150);
    CustomPage_DrawText(hdc, text, rc, enabled ? UiTheme::Color_Text() : UiTheme::Color_TextMuted(),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void CustomPage_DrawChip(Graphics& g, HDC hdc, const RECT& rc, const std::wstring& text, bool enabled)
{
    CustomPage_DrawRoundRect(g, rc, UiTheme::Color_ControlBg(), UiTheme::Color_Border(), 10.0f, enabled ? 255 : 150);
    CustomPage_DrawText(hdc, text, rc, enabled ? UiTheme::Color_Text() : UiTheme::Color_TextMuted(),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void CustomPage_DrawCheckbox(Graphics& g, HDC hdc, HWND hWnd, const RECT& rc, const std::wstring& text, bool checked, bool enabled)
{
    int box = std::min((int)(rc.bottom - rc.top - 6), CpcScale(hWnd, 18));
    RECT brc{ rc.left + 1, rc.top + ((rc.bottom - rc.top) - box) / 2, rc.left + 1 + box, rc.top + ((rc.bottom - rc.top) + box) / 2 };
    CustomPage_DrawRoundRect(g, brc,
        checked ? UiTheme::Color_Accent() : UiTheme::Color_ControlBg(),
        checked ? UiTheme::Color_Accent() : UiTheme::Color_Border(),
        2.0f,
        enabled ? 255 : 150);
    if (checked)
    {
        Pen check(CpcColor(RGB(255, 255, 255)), 2.0f);
        g.DrawLine(&check, (INT)(brc.left + box / 4), (INT)(brc.top + box / 2), (INT)(brc.left + box / 2 - 1), (INT)(brc.bottom - box / 4));
        g.DrawLine(&check, (INT)(brc.left + box / 2 - 1), (INT)(brc.bottom - box / 4), (INT)(brc.right - box / 5), (INT)(brc.top + box / 4));
    }
    RECT trc{ brc.right + CpcScale(hWnd, 8), rc.top, rc.right, rc.bottom };
    CustomPage_DrawText(hdc, text, trc, enabled ? UiTheme::Color_Text() : UiTheme::Color_TextMuted(),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

CustomSliderMetrics CustomPage_GetSliderMetrics(HWND hWnd, const RECT& rc)
{
    int h = rc.bottom - rc.top;
    CustomSliderMetrics m{};
    m.pad = std::clamp(h / 3, 8, 14);
    m.trackLeft = rc.left + m.pad;
    m.trackRight = rc.right - m.pad;
    if (m.trackRight <= m.trackLeft)
        m.trackRight = m.trackLeft + 1;
    (void)hWnd;
    return m;
}

void CustomPage_DrawSlider(Graphics& g, HWND hWnd, const RECT& rc, int minV, int maxV, int value)
{
    int h = rc.bottom - rc.top;
    int trackH = std::clamp(h / 5, 6, 10);
    int cy = (rc.top + rc.bottom) / 2;
    CustomSliderMetrics m = CustomPage_GetSliderMetrics(hWnd, rc);
    int trackW = std::max(1, m.trackRight - m.trackLeft);
    RectF track((REAL)m.trackLeft, (REAL)(cy - trackH / 2), (REAL)trackW, (REAL)trackH);
    float t = (float)(std::clamp(value, minV, maxV) - minV) / (float)std::max(1, maxV - minV);

    GraphicsPath tp;
    CpcAddRoundRectPath(tp, track, track.Height * 0.5f);
    SolidBrush trackBr(CpcColor(RGB(55, 55, 55)));
    g.FillPath(&trackBr, &tp);
    Pen border(CpcColor(UiTheme::Color_Border()), 1.0f);
    g.DrawPath(&border, &tp);

    if (t > 0.0f)
    {
        RectF fill = track;
        fill.Width = std::max(0.0f, track.Width * t);
        GraphicsPath fp;
        CpcAddRoundRectPath(fp, fill, fill.Height * 0.5f);
        SolidBrush fillBr(CpcColor(UiTheme::Color_Accent()));
        g.FillPath(&fillBr, &fp);
    }

    float knobX = track.X + track.Width * t;
    float knobR = std::clamp((float)h * 0.22f, 7.0f, 12.0f);
    SolidBrush knobFill(CpcColor(RGB(235, 235, 235)));
    Pen knobBorder(CpcColor(RGB(15, 15, 15), 220), 1.5f);
    g.FillEllipse(&knobFill, RectF(knobX - knobR, (REAL)cy - knobR, knobR * 2.0f, knobR * 2.0f));
    g.DrawEllipse(&knobBorder, RectF(knobX - knobR, (REAL)cy - knobR, knobR * 2.0f, knobR * 2.0f));
}

int CustomPage_SliderValueFromPoint(HWND hWnd, const RECT& rc, int minV, int maxV, int x)
{
    CustomSliderMetrics m = CustomPage_GetSliderMetrics(hWnd, rc);
    double t = (double)(x - m.trackLeft) / (double)std::max(1, m.trackRight - m.trackLeft);
    return minV + (int)std::lround(std::clamp(t, 0.0, 1.0) * (double)(maxV - minV));
}
