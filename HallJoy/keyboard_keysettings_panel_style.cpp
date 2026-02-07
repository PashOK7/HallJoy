// keyboard_keysettings_panel_style.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <cmath>
#include <vector>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <objidl.h>
#include <gdiplus.h>

#include "keyboard_keysettings_panel_internal.h"
#include "ui_theme.h"
#include "win_util.h"

using namespace Gdiplus;

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }

static Color Gp(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }

static BYTE LerpB(BYTE a, BYTE b, float t)
{
    int v = (int)std::lround((float)a + ((float)b - (float)a) * t);
    return (BYTE)std::clamp(v, 0, 255);
}

static Color LerpColor(const Color& a, const Color& b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return Color(
        LerpB(a.GetA(), b.GetA(), t),
        LerpB(a.GetR(), b.GetR(), t),
        LerpB(a.GetG(), b.GetG(), t),
        LerpB(a.GetB(), b.GetB(), t));
}

static void AddRoundRect(GraphicsPath& path, const RectF& r, float radius)
{
    float rr = std::clamp(radius, 0.0f, std::min(r.Width, r.Height) * 0.5f);
    float d = rr * 2.0f;
    RectF arc(r.X, r.Y, d, d);

    path.StartFigure();
    path.AddArc(arc, 180, 90);
    arc.X = r.GetRight() - d; path.AddArc(arc, 270, 90);
    arc.Y = r.GetBottom() - d; path.AddArc(arc, 0, 90);
    arc.X = r.X; path.AddArc(arc, 90, 90);
    path.CloseFigure();
}

static void FillRound(Graphics& g, const RectF& r, float rad, const Color& c)
{
    SolidBrush br(c);
    GraphicsPath p;
    AddRoundRect(p, r, rad);
    g.FillPath(&br, &p);
}

static void StrokeRound(Graphics& g, const RectF& r, float rad, const Color& c, float w)
{
    Pen pen(c, w);
    pen.SetLineJoin(LineJoinRound);
    GraphicsPath p;
    AddRoundRect(p, r, rad);
    g.DrawPath(&pen, &p);
}

static void DrawTextGp(Graphics& g, const RectF& r, const wchar_t* txt, const Color& c, bool center)
{
    if (!txt) txt = L"";

    FontFamily ff(L"Segoe UI");
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetAlignment(center ? StringAlignmentCenter : StringAlignmentNear);
    fmt.SetLineAlignment(StringAlignmentCenter);

    float em = std::clamp(r.Height * 0.46f, 11.0f, 16.0f);
    Font font(&ff, em, FontStyleRegular, UnitPixel);

    SolidBrush br(c);
    RectF rr = r;
    if (!center) rr.X += 2.0f;
    g.DrawString(txt, -1, &font, rr, &fmt, &br);
}

// ---------------- Double-buffer helpers ----------------
static bool BeginBuffered(HDC outDC, int w, int h, HDC& memDC, HBITMAP& bmp, HGDIOBJ& oldBmp)
{
    memDC = CreateCompatibleDC(outDC);
    if (!memDC) return false;

    bmp = CreateCompatibleBitmap(outDC, w, h);
    if (!bmp)
    {
        DeleteDC(memDC);
        memDC = nullptr;
        return false;
    }

    oldBmp = SelectObject(memDC, bmp);
    return true;
}

static void EndBuffered(HDC outDC, int dstX, int dstY, int w, int h, HDC memDC, HBITMAP bmp, HGDIOBJ oldBmp)
{
    BitBlt(outDC, dstX, dstY, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

// ---- Toggles (animated) ----
static bool ToggleCheckedById(int ctlId)
{
    if (ctlId == KSP_ID_UNIQUE)
    {
        if (!Ksp_IsKeySelected()) return false;
        return Ksp_IsOverrideOnForKey();
    }
    if (ctlId == KSP_ID_INVERT)
    {
        return Ksp_GetActiveSettings().invert;
    }
    return false;
}

static bool ToggleDisabledById(int ctlId, const DRAWITEMSTRUCT* dis)
{
    if (dis && (dis->itemState & ODS_DISABLED)) return true;
    if (ctlId == KSP_ID_UNIQUE && !Ksp_IsKeySelected()) return true;
    return false;
}

static float ToggleAnimTFromWindow(HWND hBtn, bool checkedFallback)
{
    auto* st = (KspToggleAnimState*)GetPropW(hBtn, KSP_TOGGLE_ANIM_PROP);
    if (!st || !st->initialized)
        return checkedFallback ? 1.0f : 0.0f;
    return std::clamp(st->t, 0.0f, 1.0f);
}

static void DrawToggleOwnerDraw_Impl(const DRAWITEMSTRUCT* dis, const wchar_t* label)
{
    Graphics g(dis->hDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    bool checked = ToggleCheckedById((int)dis->CtlID);

    // FIX:
    // Do NOT show any focus/accent outline for these toggles.
    // Users reported a "blue outline" that stays after interaction (focus remains on control).
    // Since this UI is mouse-first and already has hover cues, we remove focus visuals entirely.
    // bool focused = (dis->itemState & ODS_FOCUS) != 0;

    bool disabled = ToggleDisabledById((int)dis->CtlID, dis);

    float t = ToggleAnimTFromWindow(dis->hwndItem, checked);

    RectF bounds((float)dis->rcItem.left, (float)dis->rcItem.top,
        (float)(dis->rcItem.right - dis->rcItem.left),
        (float)(dis->rcItem.bottom - dis->rcItem.top));

    SolidBrush brBg(Gp(UiTheme::Color_PanelBg()));
    g.FillRectangle(&brBg, bounds);

    float h = bounds.Height;
    float sw = std::clamp(h * 1.55f, 36.0f, 54.0f);
    float sh = std::clamp(h * 0.78f, 18.0f, 28.0f);
    float sx = bounds.X;
    float sy = bounds.Y + (bounds.Height - sh) * 0.5f;

    RectF track(sx, sy, sw, sh);
    float rr = sh * 0.5f;

    Color onC = disabled ? Gp(UiTheme::Color_Border()) : Gp(UiTheme::Color_Accent());
    Color offC = Gp(RGB(70, 70, 70));
    Color trackC = LerpColor(offC, onC, t);

    FillRound(g, track, rr, trackC);

    float thumbD = sh - 4.0f;
    float thumbX0 = track.X + 2.0f;
    float thumbX1 = track.GetRight() - 2.0f - thumbD;
    float thumbX = thumbX0 + (thumbX1 - thumbX0) * t;

    RectF thumb(thumbX, track.Y + 2.0f, thumbD, thumbD);
    SolidBrush brThumb(disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(RGB(240, 240, 240)));
    g.FillEllipse(&brThumb, thumb);

    // REMOVED: focus accent outline
    // if (focused)
    //     StrokeRound(g, track, rr, Gp(UiTheme::Color_Accent()), 2.0f);

    RectF textR(track.GetRight() + 10.0f, bounds.Y, bounds.GetRight() - (track.GetRight() + 10.0f), bounds.Height);
    DrawTextGp(g, textR, label, disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(UiTheme::Color_Text()), false);
}

static void DrawToggleOwnerDraw_Buffered(const DRAWITEMSTRUCT* dis, const wchar_t* label)
{
    RECT rc = dis->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2)
    {
        DrawToggleOwnerDraw_Impl(dis, label);
        return;
    }

    HDC memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;

    if (!BeginBuffered(dis->hDC, w, h, memDC, bmp, oldBmp))
    {
        DrawToggleOwnerDraw_Impl(dis, label);
        return;
    }

    DRAWITEMSTRUCT di = *dis;
    di.hDC = memDC;
    di.rcItem = RECT{ 0,0,w,h };

    DrawToggleOwnerDraw_Impl(&di, label);

    EndBuffered(dis->hDC, rc.left, rc.top, w, h, memDC, bmp, oldBmp);
}

// ============================================================================
// Combo styling (legacy / kept for compatibility)
// (Your project now uses PremiumCombo for Mode/Preset, so this code is not used
// for those controls, but we keep it to avoid breaking other potential users.)
// ============================================================================

static void Combo_GetSelectedText(HWND hCombo, wchar_t* out, int outCch)
{
    if (!out || outCch <= 0) return;
    out[0] = 0;

    int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    int textLen = (int)SendMessageW(hCombo, CB_GETLBTEXTLEN, (WPARAM)sel, 0);
    if (textLen < 0) return;
    std::vector<wchar_t> tmp((size_t)textLen + 1u, 0);
    if (tmp.empty()) return;

    SendMessageW(hCombo, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)tmp.data());
    wcsncpy_s(out, (size_t)outCch, tmp.data(), _TRUNCATE);
}

static void Combo_PaintClosed_Impl(HWND hCombo, HDC hdc)
{
    RECT rc{};
    GetClientRect(hCombo, &rc);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    bool disabled = !IsWindowEnabled(hCombo);
    bool focused = (GetFocus() == hCombo);
    bool dropped = (SendMessageW(hCombo, CB_GETDROPPEDSTATE, 0, 0) != 0);

    RectF r((float)rc.left, (float)rc.top, (float)(rc.right - rc.left), (float)(rc.bottom - rc.top));
    float rad = std::clamp(r.Height * 0.22f, 4.0f, 8.0f);

    Color fill = Gp(UiTheme::Color_ControlBg());
    Color border = focused ? Gp(UiTheme::Color_Accent()) : Gp(UiTheme::Color_Border());
    Color textC = disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(UiTheme::Color_Text());

    SolidBrush brOuter(Gp(UiTheme::Color_PanelBg()));
    g.FillRectangle(&brOuter, r);

    RectF inner = r;
    inner.Inflate(-1.0f, -1.0f);

    FillRound(g, inner, rad, fill);
    StrokeRound(g, inner, rad, border, focused ? 2.0f : 1.0f);

    float arrowW = std::clamp(inner.Height * 0.95f, 18.0f, 30.0f);
    RectF arrowR(inner.GetRight() - arrowW, inner.Y, arrowW, inner.Height);

    Pen sep(Gp(UiTheme::Color_Border()), 1.0f);
    g.DrawLine(&sep, PointF(arrowR.X, arrowR.Y + 2.0f), PointF(arrowR.X, arrowR.GetBottom() - 2.0f));

    wchar_t text[256]{};
    Combo_GetSelectedText(hCombo, text, 256);

    RectF textR = inner;
    textR.Width -= arrowW;
    textR.Inflate(-8.0f, 0.0f);
    DrawTextGp(g, textR, text, textC, false);

    float cx = arrowR.X + arrowR.Width * 0.5f;
    float cy = arrowR.Y + arrowR.Height * 0.52f;

    float s = std::clamp(arrowR.Height * 0.18f, 3.0f, 6.0f);

    PointF p1(cx - s, cy - (dropped ? -s * 0.6f : s * 0.6f));
    PointF p2(cx, cy + (dropped ? -s * 0.6f : s * 0.6f));
    PointF p3(cx + s, cy - (dropped ? -s * 0.6f : s * 0.6f));

    Pen pen(disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(UiTheme::Color_TextMuted()), 2.0f);
    pen.SetLineJoin(LineJoinRound);
    g.DrawLine(&pen, p1, p2);
    g.DrawLine(&pen, p2, p3);
}

static void Combo_PaintClosed(HWND hCombo, HDC hdc)
{
    RECT rc{};
    GetClientRect(hCombo, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2)
    {
        Combo_PaintClosed_Impl(hCombo, hdc);
        return;
    }

    HDC memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;

    if (!BeginBuffered(hdc, w, h, memDC, bmp, oldBmp))
    {
        Combo_PaintClosed_Impl(hCombo, hdc);
        return;
    }

    Combo_PaintClosed_Impl(hCombo, memDC);
    EndBuffered(hdc, 0, 0, w, h, memDC, bmp, oldBmp);
}

static LRESULT CALLBACK ComboSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_NCPAINT:
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        Combo_PaintClosed(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_PRINTCLIENT:
    {
        HDC hdc = (HDC)wParam;
        if (hdc)
        {
            Combo_PaintClosed(hWnd, hdc);
            return 0;
        }
        break;
    }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, ComboSubclassProc, 1);
        break;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

void Ksp_StyleInstallCombo(HWND hCombo)
{
    if (!hCombo) return;

    LONG_PTR ex = GetWindowLongPtrW(hCombo, GWL_EXSTYLE);
    ex &= ~WS_EX_CLIENTEDGE;
    SetWindowLongPtrW(hCombo, GWL_EXSTYLE, ex);

    LONG_PTR st = GetWindowLongPtrW(hCombo, GWL_STYLE);
    st &= ~WS_BORDER;
    SetWindowLongPtrW(hCombo, GWL_STYLE, st);

    SetWindowPos(hCombo, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    SetWindowSubclass(hCombo, ComboSubclassProc, 1, 0);
    InvalidateRect(hCombo, nullptr, TRUE);
}

static void DrawComboListItem_Impl(const DRAWITEMSTRUCT* dis)
{
    Graphics g(dis->hDC);
    g.SetSmoothingMode(SmoothingModeNone);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    RectF r((float)dis->rcItem.left, (float)dis->rcItem.top,
        (float)(dis->rcItem.right - dis->rcItem.left),
        (float)(dis->rcItem.bottom - dis->rcItem.top));

    bool selected = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    Color fill = selected ? Gp(UiTheme::Color_Accent()) : Gp(UiTheme::Color_ControlBg());
    Color textC = selected ? Gp(RGB(12, 12, 12)) : (disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(UiTheme::Color_Text()));

    SolidBrush br(fill);
    g.FillRectangle(&br, r);

    wchar_t text[256]{};
    if (dis->itemID != (UINT)-1)
    {
        int textLen = (int)SendMessageW(dis->hwndItem, CB_GETLBTEXTLEN, (WPARAM)dis->itemID, 0);
        if (textLen > 0)
        {
            std::vector<wchar_t> tmp((size_t)textLen + 1u, 0);
            if (!tmp.empty())
            {
                SendMessageW(dis->hwndItem, CB_GETLBTEXT, (WPARAM)dis->itemID, (LPARAM)tmp.data());
                wcsncpy_s(text, tmp.data(), _TRUNCATE);
            }
        }
    }

    RectF tr = r;
    tr.Inflate(-8.0f, 0.0f);
    DrawTextGp(g, tr, text, textC, false);

    if (dis->itemState & ODS_FOCUS)
    {
        Pen pen(Gp(UiTheme::Color_TextMuted()), 1.0f);
        g.DrawRectangle(&pen, r);
    }
}

static void DrawComboListItem_Buffered(const DRAWITEMSTRUCT* dis)
{
    RECT rc = dis->rcItem;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2)
    {
        DrawComboListItem_Impl(dis);
        return;
    }

    HDC memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;

    if (!BeginBuffered(dis->hDC, w, h, memDC, bmp, oldBmp))
    {
        DrawComboListItem_Impl(dis);
        return;
    }

    DRAWITEMSTRUCT di = *dis;
    di.hDC = memDC;
    di.rcItem = RECT{ 0,0,w,h };

    DrawComboListItem_Impl(&di);

    EndBuffered(dis->hDC, rc.left, rc.top, w, h, memDC, bmp, oldBmp);
}

bool Ksp_StyleHandleMeasureItem(MEASUREITEMSTRUCT* mis)
{
    if (!mis) return false;

    if (mis->CtlType == ODT_COMBOBOX && (mis->CtlID == KSP_ID_MODE || mis->CtlID == KSP_ID_CURVE))
    {
        HWND ref = g_kspParent ? g_kspParent : GetActiveWindow();
        UINT h = (UINT)std::clamp(S(ref, 28), 20, 44);
        mis->itemHeight = h;
        return true;
    }
    return false;
}

bool Ksp_StyleHandleDrawItem(const DRAWITEMSTRUCT* dis)
{
    if (!dis) return false;

    if (dis->CtlType == ODT_BUTTON && (dis->CtlID == KSP_ID_UNIQUE || dis->CtlID == KSP_ID_INVERT))
    {
        const wchar_t* label = (dis->CtlID == KSP_ID_UNIQUE)
            ? L"Override global settings for this key"
            : L"Invert axis (press to release)";

        DrawToggleOwnerDraw_Buffered(dis, label);
        return true;
    }

    if (dis->CtlType == ODT_COMBOBOX && (dis->CtlID == KSP_ID_MODE || dis->CtlID == KSP_ID_CURVE))
    {
        if (dis->itemID == (UINT)-1)
            return true;

        DrawComboListItem_Buffered(dis);
        return true;
    }

    return false;
}
