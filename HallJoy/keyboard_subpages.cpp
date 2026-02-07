// keyboard_subpages.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <filesystem>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <objidl.h>
#include <gdiplus.h>

#include "keyboard_ui_internal.h"
#include "keyboard_keysettings_panel.h"
#include "keyboard_keysettings_panel_internal.h"

#include "backend.h"
#include "gamepad_render.h"
#include "ui_theme.h"
#include "settings.h"
#include "realtime_loop.h"
#include "win_util.h"
#include "keyboard_profiles.h"
#include "premium_combo.h"

using namespace Gdiplus;
namespace fs = std::filesystem;

static constexpr UINT WM_APP_REQUEST_SAVE = WM_APP + 1;
static constexpr UINT WM_APP_PROFILE_BEGIN_CREATE = WM_APP + 120;

static constexpr UINT_PTR TOAST_TIMER_ID = 8811;
static constexpr DWORD    TOAST_SHOW_MS = 1600;

static constexpr int ID_SNAPPY = 7003;

static int S(HWND hwnd, int px) { return WinUtil_ScalePx(hwnd, px); }

// ---------------- Double-buffer helpers ----------------
static void BeginDoubleBufferPaint(HWND hWnd, PAINTSTRUCT& ps, HDC& outMemDC, HBITMAP& outBmp, HGDIOBJ& outOldBmp)
{
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT rc{};
    GetClientRect(hWnd, &rc);
    outMemDC = CreateCompatibleDC(hdc);
    outBmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    outOldBmp = SelectObject(outMemDC, outBmp);
    FillRect(outMemDC, &rc, UiTheme::Brush_PanelBg());
}

static void EndDoubleBufferPaint(HWND hWnd, PAINTSTRUCT& ps, HDC memDC, HBITMAP bmp, HGDIOBJ oldBmp)
{
    HDC hdc = ps.hdc;
    RECT rc{};
    GetClientRect(hWnd, &rc);
    BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
    EndPaint(hWnd, &ps);
}

// ============================================================================
// Gamepad Tester page (DPI-scaled)
// ============================================================================
LRESULT CALLBACK KeyboardSubpages_TesterPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND: return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);

        XUSB_REPORT r = Backend_GetLastReport();
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HGDIOBJ oldFont = SelectObject(memDC, font);
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, UiTheme::Color_Text());

        const int x0 = S(hWnd, 12);
        int y = S(hWnd, 12);

        const int lineH = S(hWnd, 18);
        const int gapAfterText = S(hWnd, 10);

        auto textLine = [&](const std::wstring& t)
            {
                TextOutW(memDC, x0, y, t.c_str(), (int)t.size());
                y += lineH;
            };

        wchar_t buf[256]{};

        swprintf_s(buf, L"LX: %6d   LY: %6d", (int)r.sThumbLX, (int)r.sThumbLY);
        textLine(buf);

        const int barW = S(hWnd, 360);
        const int barH = S(hWnd, 20);
        const int barGapX = S(hWnd, 20);
        RECT barLX{ x0, y, x0 + barW, y + barH };
        RECT barLY{ x0 + barW + barGapX, y, x0 + barW + barGapX + barW, y + barH };
        GamepadRender_DrawAxisBarCentered(memDC, barLX, r.sThumbLX);
        GamepadRender_DrawAxisBarCentered(memDC, barLY, r.sThumbLY);
        y += barH + S(hWnd, 8);

        swprintf_s(buf, L"RX: %6d   RY: %6d", (int)r.sThumbRX, (int)r.sThumbRY);
        textLine(buf);

        RECT barRX{ x0, y, x0 + barW, y + barH };
        RECT barRY{ x0 + barW + barGapX, y, x0 + barW + barGapX + barW, y + barH };
        GamepadRender_DrawAxisBarCentered(memDC, barRX, r.sThumbRX);
        GamepadRender_DrawAxisBarCentered(memDC, barRY, r.sThumbRY);
        y += barH + S(hWnd, 8);

        swprintf_s(buf, L"LT: %3u   RT: %3u", (unsigned)r.bLeftTrigger, (unsigned)r.bRightTrigger);
        textLine(buf);

        const int trigH = S(hWnd, 18);
        RECT barLT{ x0, y, x0 + barW, y + trigH };
        RECT barRT{ x0 + barW + barGapX, y, x0 + barW + barGapX + barW, y + trigH };
        GamepadRender_DrawTriggerBar01(memDC, barLT, r.bLeftTrigger);
        GamepadRender_DrawTriggerBar01(memDC, barRT, r.bRightTrigger);
        y += trigH + S(hWnd, 8);

        y += gapAfterText;
        textLine(L"Buttons: " + GamepadRender_ButtonsToString(r.wButtons));

        SelectObject(memDC, oldFont);
        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// Premium slider + value chip
// ============================================================================
static Color Gp(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }

struct PremiumSliderState
{
    int minV = 1;
    int maxV = 20;
    int posV = 5;
    bool dragging = false;
};

static int PremiumSlider_Clamp(const PremiumSliderState* st, int v)
{
    if (!st) return v;
    return std::clamp(v, st->minV, st->maxV);
}

static float PremiumSlider_ValueToT(const PremiumSliderState* st)
{
    if (!st) return 0.0f;
    int den = (st->maxV - st->minV);
    if (den <= 0) return 0.0f;
    return (float)(st->posV - st->minV) / (float)den;
}

static int PremiumSlider_XToValue(const PremiumSliderState* st, int x, int w, int pad)
{
    if (!st) return 0;
    int usable = w - pad * 2;
    if (usable <= 1) return st->minV;

    float t = (float)(x - pad) / (float)usable;
    t = std::clamp(t, 0.0f, 1.0f);

    float v = (float)st->minV + t * (float)(st->maxV - st->minV);
    int iv = (int)lroundf(v);
    return PremiumSlider_Clamp(st, iv);
}

static void PremiumSlider_Notify(HWND hWnd, int code)
{
    HWND parent = GetParent(hWnd);
    if (!parent) return;
    PostMessageW(parent, WM_HSCROLL, (WPARAM)code, (LPARAM)hWnd);
}

static void AddRoundRectPath(GraphicsPath& path, const RectF& r, float rad)
{
    float rr = std::clamp(rad, 0.0f, std::min(r.Width, r.Height) * 0.5f);
    float d = rr * 2.0f;
    RectF arc(r.X, r.Y, d, d);

    path.StartFigure();
    path.AddArc(arc, 180, 90);
    arc.X = r.GetRight() - d; path.AddArc(arc, 270, 90);
    arc.Y = r.GetBottom() - d; path.AddArc(arc, 0, 90);
    arc.X = r.X; path.AddArc(arc, 90, 90);
    path.CloseFigure();
}

static void PremiumSlider_Paint(HWND hWnd, HDC hdc)
{
    PremiumSliderState* st = (PremiumSliderState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    RECT rc{};
    GetClientRect(hWnd, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
    g.FillRectangle(&bg, (REAL)0, (REAL)0, (REAL)w, (REAL)h);

    int pad = std::clamp(h / 3, 8, 14);
    int trackH = std::clamp(h / 5, 6, 10);
    int cy = h / 2;

    RectF track((REAL)pad, (REAL)(cy - trackH / 2), (REAL)(w - pad * 2), (REAL)trackH);
    float rr = track.Height * 0.5f;

    {
        SolidBrush br(Gp(RGB(55, 55, 55)));
        GraphicsPath p;
        AddRoundRectPath(p, track, rr);
        g.FillPath(&br, &p);

        Pen border(Gp(UiTheme::Color_Border()), 1.0f);
        g.DrawPath(&border, &p);
    }

    float t = PremiumSlider_ValueToT(st);

    RectF fill = track;
    fill.Width = std::max(0.0f, track.Width * t);

    if (fill.Width > 0.5f)
    {
        Color accent = Gp(UiTheme::Color_Accent());
        Color accent2(
            255,
            (BYTE)std::min(255, (int)accent.GetR() + 18),
            (BYTE)std::min(255, (int)accent.GetG() + 18),
            (BYTE)std::min(255, (int)accent.GetB() + 18));

        LinearGradientBrush grad(fill, accent2, accent, LinearGradientModeVertical);

        GraphicsPath p;
        AddRoundRectPath(p, fill, rr);
        g.FillPath(&grad, &p);
    }

    float knobX = track.X + track.Width * t;
    float knobR = std::clamp((float)h * 0.22f, 7.0f, 12.0f);

    SolidBrush knobFill(Gp(RGB(235, 235, 235)));
    Pen knobBorder(Gp(RGB(15, 15, 15), 220), 1.5f);

    RectF knob(knobX - knobR, (REAL)cy - knobR, knobR * 2.0f, knobR * 2.0f);
    g.FillEllipse(&knobFill, knob);
    g.DrawEllipse(&knobBorder, knob);

    if (st && st->dragging)
    {
        Pen ring(Gp(UiTheme::Color_Accent(), 230), 2.5f);
        g.DrawEllipse(&ring, RectF(knob.X - 2.0f, knob.Y - 2.0f, knob.Width + 4.0f, knob.Height + 4.0f));
    }
}

static LRESULT CALLBACK PremiumSliderProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PremiumSliderState* st = (PremiumSliderState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE:
        return TRUE;

    case WM_CREATE:
    {
        st = new PremiumSliderState();
        st->posV = (int)Settings_GetPollingMs();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        return 0;
    }

    case WM_NCDESTROY:
        if (st)
        {
            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        PremiumSlider_Paint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (!st) break;
        SetFocus(hWnd);
        SetCapture(hWnd);
        st->dragging = true;

        RECT rc{};
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int pad = std::clamp(h / 3, 8, 14);

        int x = (short)LOWORD(lParam);
        int nv = PremiumSlider_XToValue(st, x, w, pad);
        if (nv != st->posV)
        {
            st->posV = nv;
            InvalidateRect(hWnd, nullptr, FALSE);
            PremiumSlider_Notify(hWnd, SB_THUMBTRACK);
        }
        else
        {
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (!st || !st->dragging) break;

        RECT rc{};
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int pad = std::clamp(h / 3, 8, 14);

        int x = (short)LOWORD(lParam);
        int nv = PremiumSlider_XToValue(st, x, w, pad);
        if (nv != st->posV)
        {
            st->posV = nv;
            InvalidateRect(hWnd, nullptr, FALSE);
            PremiumSlider_Notify(hWnd, SB_THUMBTRACK);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (!st) break;
        if (st->dragging)
        {
            st->dragging = false;
            ReleaseCapture();
            InvalidateRect(hWnd, nullptr, FALSE);
            PremiumSlider_Notify(hWnd, SB_ENDSCROLL);
            PremiumSlider_Notify(hWnd, SB_THUMBPOSITION);
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        if (!st) break;

        if (wParam == VK_LEFT || wParam == VK_DOWN) st->posV -= 1;
        else if (wParam == VK_RIGHT || wParam == VK_UP) st->posV += 1;
        else break;

        st->posV = PremiumSlider_Clamp(st, st->posV);
        InvalidateRect(hWnd, nullptr, FALSE);
        PremiumSlider_Notify(hWnd, SB_THUMBPOSITION);
        return 0;
    }

    case TBM_SETRANGE:
    {
        if (!st) break;
        int minV = (int)LOWORD(lParam);
        int maxV = (int)HIWORD(lParam);
        if (minV > maxV) std::swap(minV, maxV);
        st->minV = minV;
        st->maxV = maxV;
        st->posV = PremiumSlider_Clamp(st, st->posV);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case TBM_SETPOS:
    {
        if (!st) break;
        st->posV = PremiumSlider_Clamp(st, (int)lParam);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case TBM_GETPOS:
    {
        if (!st) break;
        return (LRESULT)st->posV;
    }

    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static HWND PremiumSlider_Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int id)
{
    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PremiumSliderProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"PremiumSlider";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    return CreateWindowW(L"PremiumSlider", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h,
        parent, (HMENU)(INT_PTR)id, hInst, nullptr);
}

// ---------------- Premium value chip ----------------
struct PremiumChipState
{
    wchar_t text[64]{};
};

static void PremiumChip_Paint(HWND hWnd, HDC hdc)
{
    PremiumChipState* st = (PremiumChipState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    RECT rc{};
    GetClientRect(hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
    g.FillRectangle(&bg, 0, 0, w, h);

    RectF r(0.0f, 0.0f, (REAL)w, (REAL)h);
    r.Inflate(-1.0f, -1.0f);
    float rad = std::clamp(r.Height * 0.40f, 6.0f, 14.0f);

    GraphicsPath p;
    AddRoundRectPath(p, r, rad);

    SolidBrush fill(Gp(UiTheme::Color_ControlBg()));
    g.FillPath(&fill, &p);

    Pen border(Gp(UiTheme::Color_Border()), 1.0f);
    g.DrawPath(&border, &p);

    const wchar_t* txt = (st && st->text[0]) ? st->text : L"";
    FontFamily ff(L"Segoe UI");
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    float em = std::clamp(r.Height * 0.52f, 11.0f, 16.0f);
    Font font(&ff, em, FontStyleBold, UnitPixel);

    SolidBrush tbr(Gp(UiTheme::Color_Text()));
    g.DrawString(txt, -1, &font, r, &fmt, &tbr);
}

static LRESULT CALLBACK PremiumChipProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PremiumChipState* st = (PremiumChipState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_NCCREATE: return TRUE;

    case WM_CREATE:
        st = new PremiumChipState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        return 0;

    case WM_NCDESTROY:
        if (st) { delete st; SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0); }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SETTEXT:
        if (st)
        {
            const wchar_t* s = (const wchar_t*)lParam;
            if (!s) s = L"";
            wcsncpy_s(st->text, s, _TRUNCATE);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        PremiumChip_Paint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static HWND PremiumChip_Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int id)
{
    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PremiumChipProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"PremiumValueChip";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    return CreateWindowW(L"PremiumValueChip", L"",
        WS_CHILD | WS_VISIBLE,
        x, y, w, h,
        parent, (HMENU)(INT_PTR)id, hInst, nullptr);
}

// ============================================================================
// Snappy Joystick Toggle (premium owner-draw)
// ============================================================================
static constexpr const wchar_t* SNAPPY_TOGGLE_ANIM_PROP = L"DD_SnappyToggleAnimPtr";

static KspToggleAnimState* SnappyToggle_Get(HWND hBtn)
{
    return (KspToggleAnimState*)GetPropW(hBtn, SNAPPY_TOGGLE_ANIM_PROP);
}

static void SnappyToggle_Free(HWND hBtn)
{
    if (auto* st = SnappyToggle_Get(hBtn))
    {
        RemovePropW(hBtn, SNAPPY_TOGGLE_ANIM_PROP);
        delete st;
    }
}

static float SnappyClamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

static void SnappyToggle_StartAnim(HWND hBtn, bool checked, bool animate)
{
    auto* st = SnappyToggle_Get(hBtn);
    if (!st)
    {
        st = new KspToggleAnimState();
        SetPropW(hBtn, SNAPPY_TOGGLE_ANIM_PROP, (HANDLE)st);
    }

    float target = checked ? 1.0f : 0.0f;

    if (!st->initialized || !animate)
    {
        st->initialized = true;
        st->checked = checked;
        st->t = target;
        st->from = target;
        st->to = target;
        st->running = false;
        st->startTick = GetTickCount();
        InvalidateRect(hBtn, nullptr, FALSE);
        return;
    }

    st->checked = checked;
    st->from = st->t;
    st->to = target;
    st->startTick = GetTickCount();
    st->durationMs = 140;
    st->running = true;

    SetTimer(hBtn, 1, 15, nullptr);
    InvalidateRect(hBtn, nullptr, FALSE);
}

static void SnappyToggle_Tick(HWND hBtn)
{
    auto* st = SnappyToggle_Get(hBtn);
    if (!st || !st->running) { KillTimer(hBtn, 1); return; }

    DWORD now = GetTickCount();
    DWORD dt = now - st->startTick;
    float x = (st->durationMs > 0) ? (float)dt / (float)st->durationMs : 1.0f;
    x = SnappyClamp01(x);

    // smoothstep
    float s = x * x * (3.0f - 2.0f * x);
    st->t = st->from + (st->to - st->from) * s;

    if (x >= 1.0f - 1e-4f)
    {
        st->t = st->to;
        st->running = false;
        KillTimer(hBtn, 1);
    }

    InvalidateRect(hBtn, nullptr, FALSE);
}

static LRESULT CALLBACK SnappyToggle_SubclassProc(HWND hBtn, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR, DWORD_PTR)
{
    switch (msg)
    {
    case WM_TIMER:
        if (wParam == 1) { SnappyToggle_Tick(hBtn); return 0; }
        break;

    case WM_NCDESTROY:
        KillTimer(hBtn, 1);
        SnappyToggle_Free(hBtn);
        RemoveWindowSubclass(hBtn, SnappyToggle_SubclassProc, 1);
        break;
    }
    return DefSubclassProc(hBtn, msg, wParam, lParam);
}

static void DrawSnappyToggleOwnerDraw(const DRAWITEMSTRUCT* dis)
{
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    bool checked = (SendMessageW(dis->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED);

    float t = checked ? 1.0f : 0.0f;
    if (auto* st = SnappyToggle_Get(dis->hwndItem))
        if (st->initialized) t = std::clamp(st->t, 0.0f, 1.0f);

    Gdiplus::Graphics g(dis->hDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    Gdiplus::RectF bounds(
        (float)dis->rcItem.left, (float)dis->rcItem.top,
        (float)(dis->rcItem.right - dis->rcItem.left),
        (float)(dis->rcItem.bottom - dis->rcItem.top));

    // background
    {
        Gdiplus::SolidBrush bg(Gp(UiTheme::Color_PanelBg()));
        g.FillRectangle(&bg, bounds);
    }

    float h = bounds.Height;
    float sw = std::clamp(h * 1.55f, 36.0f, 54.0f);
    float sh = std::clamp(h * 0.78f, 18.0f, 28.0f);
    float sx = bounds.X;
    float sy = bounds.Y + (bounds.Height - sh) * 0.5f;

    Gdiplus::RectF track(sx, sy, sw, sh);
    float rr = sh * 0.5f;

    Gdiplus::Color onC = disabled ? Gp(UiTheme::Color_Border()) : Gp(UiTheme::Color_Accent());
    Gdiplus::Color offC = Gp(RGB(70, 70, 70));
    auto lerpC = [&](const Gdiplus::Color& a, const Gdiplus::Color& b, float tt)
        {
            tt = std::clamp(tt, 0.0f, 1.0f);
            auto L = [&](BYTE aa, BYTE bb) -> BYTE { return (BYTE)std::clamp((int)lroundf(aa + (bb - aa) * tt), 0, 255); };
            return Gdiplus::Color(L(a.GetA(), b.GetA()), L(a.GetR(), b.GetR()), L(a.GetG(), b.GetG()), L(a.GetB(), b.GetB()));
        };

    {
        Gdiplus::SolidBrush br(lerpC(offC, onC, t));
        Gdiplus::GraphicsPath p;
        AddRoundRectPath(p, track, rr);
        g.FillPath(&br, &p);
    }

    float thumbD = sh - 4.0f;
    float thumbX0 = track.X + 2.0f;
    float thumbX1 = track.GetRight() - 2.0f - thumbD;
    float thumbX = thumbX0 + (thumbX1 - thumbX0) * t;

    {
        Gdiplus::RectF thumb(thumbX, track.Y + 2.0f, thumbD, thumbD);
        Gdiplus::SolidBrush brThumb(disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(RGB(240, 240, 240)));
        g.FillEllipse(&brThumb, thumb);
    }

    // label
    {
        const wchar_t* label = L"Snappy Joystick";
        Gdiplus::RectF textR(track.GetRight() + 10.0f, bounds.Y,
            bounds.GetRight() - (track.GetRight() + 10.0f), bounds.Height);

        FontFamily ff(L"Segoe UI");
        Gdiplus::StringFormat fmt;
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        fmt.SetAlignment(Gdiplus::StringAlignmentNear);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        float em = std::clamp(bounds.Height * 0.46f, 11.0f, 16.0f);
        Gdiplus::Font font(&ff, em, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        Gdiplus::SolidBrush br(disabled ? Gp(UiTheme::Color_TextMuted()) : Gp(UiTheme::Color_Text()));
        g.DrawString(label, -1, &font, textR, &fmt, &br);
    }
}

// ============================================================================
// Config page
// ============================================================================
struct ConfigPageState
{
    HWND lblPoll = nullptr;
    HWND chipPoll = nullptr;
    HWND sldPoll = nullptr;

    HWND chkSnappy = nullptr;

    // status label for presets
    HWND lblProfileStatus = nullptr;

    // --- Delete confirmation (two-click) ---
    int   pendingDeleteIdx = -1;
    DWORD pendingDeleteTick = 0;

    // --- Premium toast (popup hint) ---
    HWND hToast = nullptr;
    std::wstring toastText;
    DWORD toastHideAt = 0;
};

static void RequestSave(HWND hWnd)
{
    HWND root = GetAncestor(hWnd, GA_ROOT);
    if (root) PostMessageW(root, WM_APP_REQUEST_SAVE, 0, 0);
}

static void SetProfileStatus(ConfigPageState* st, const wchar_t* text)
{
    if (!st || !st->lblProfileStatus) return;
    SetWindowTextW(st->lblProfileStatus, text ? text : L"");
}

static void UpdatePollingUi(ConfigPageState* st, UINT pollingMs)
{
    if (!st) return;

    if (st->chipPoll)
    {
        wchar_t b[32]{};
        swprintf_s(b, L"%u ms", (unsigned)pollingMs);
        SetWindowTextW(st->chipPoll, b);
    }

    if (st->lblPoll)
        SetWindowTextW(st->lblPoll, L"Polling rate");
}

static void LayoutConfigControls(HWND hWnd, ConfigPageState* st)
{
    if (!st) return;

    int margin = S(hWnd, 12);

    int sliderW = S(hWnd, 320);
    int sliderH = S(hWnd, 34);
    int chipW = S(hWnd, 86);
    int chipH = sliderH;
    int gap = S(hWnd, 10);

    int totalW = sliderW + gap + chipW;

    int x = margin;
    int y = S(hWnd, 310);

    if (st->lblPoll)
        SetWindowPos(st->lblPoll, nullptr, x, y, totalW, S(hWnd, 18), SWP_NOZORDER);

    if (st->sldPoll)
        SetWindowPos(st->sldPoll, nullptr, x, y + S(hWnd, 22), sliderW, sliderH, SWP_NOZORDER);

    if (st->chipPoll)
        SetWindowPos(st->chipPoll, nullptr, x + sliderW + gap, y + S(hWnd, 22), chipW, chipH, SWP_NOZORDER);

    int yAfterSlider = y + S(hWnd, 22) + sliderH + S(hWnd, 10);

    // Snappy toggle
    if (st->chkSnappy)
    {
        int toggleH = S(hWnd, 26);
        SetWindowPos(st->chkSnappy, nullptr, x, yAfterSlider,
            totalW, toggleH, SWP_NOZORDER);

        yAfterSlider += toggleH + S(hWnd, 10);
    }

    if (st->lblProfileStatus)
    {
        SetWindowPos(st->lblProfileStatus, nullptr, x, yAfterSlider,
            (int)std::max(10, (int)totalW), S(hWnd, 18), SWP_NOZORDER);
    }
}

static std::wstring SanitizePresetNameForFile(const std::wstring& in)
{
    std::wstring s = in;

    while (!s.empty() && s.back() == L' ') s.pop_back();
    while (!s.empty() && s.front() == L' ') s.erase(s.begin());

    if (s.size() >= 4)
    {
        const wchar_t* tail = s.c_str() + (s.size() - 4);
        if (_wcsicmp(tail, L".ini") == 0)
            s.resize(s.size() - 4);
    }

    const wchar_t* bad = L"<>:\"/\\|?*";
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (wcschr(bad, s[i]) || s[i] < 32)
            s[i] = L'_';
    }

    while (!s.empty() && (s.back() == L' ' || s.back() == L'.')) s.pop_back();

    return s;
}

// ============================================================================
// Premium toast (small popup hint) for delete confirmation
// ============================================================================
static void Toast_EnsureWindow(HWND hPage, ConfigPageState* st)
{
    if (!st || st->hToast) return;

    static bool reg = false;
    if (!reg)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
            {
                auto* stLocal = (ConfigPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

                switch (msg)
                {
                case WM_NCCREATE:
                    return TRUE;

                case WM_CREATE:
                {
                    auto* cs = (CREATESTRUCTW*)lParam;
                    stLocal = (ConfigPageState*)cs->lpCreateParams;
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)stLocal);

                    // layered alpha
                    SetLayeredWindowAttributes(hWnd, 0, 235, LWA_ALPHA);
                    return 0;
                }

                case WM_ERASEBKGND:
                    return 1;

                case WM_PAINT:
                {
                    PAINTSTRUCT ps{};
                    HDC hdc = BeginPaint(hWnd, &ps);

                    RECT rc{};
                    GetClientRect(hWnd, &rc);
                    int w = rc.right - rc.left;
                    int h = rc.bottom - rc.top;

                    Graphics g(hdc);
                    g.SetSmoothingMode(SmoothingModeAntiAlias);
                    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
                    g.SetCompositingQuality(CompositingQualityHighQuality);
                    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

                    RectF r(0.0f, 0.0f, (REAL)w, (REAL)h);
                    r.Inflate(-1.0f, -1.0f);

                    float rad = std::clamp(r.Height * 0.35f, 8.0f, 14.0f);

                    GraphicsPath p;
                    AddRoundRectPath(p, r, rad);

                    // Fill
                    Color fill(245, 34, 34, 34); // slightly translucent
                    SolidBrush brFill(fill);
                    g.FillPath(&brFill, &p);

                    // Border (soft red)
                    Color border(255, 255, 90, 90);
                    Pen pen(border, 2.0f);
                    pen.SetLineJoin(LineJoinRound);
                    g.DrawPath(&pen, &p);

                    // Text
                    std::wstring text = (stLocal ? stLocal->toastText : L"");
                    if (!text.empty())
                    {
                        FontFamily ff(L"Segoe UI");
                        float em = std::clamp(r.Height * 0.36f, 11.0f, 14.0f);
                        Font font(&ff, em, FontStyleRegular, UnitPixel);

                        StringFormat fmt;
                        fmt.SetAlignment(StringAlignmentNear);
                        fmt.SetLineAlignment(StringAlignmentCenter);
                        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
                        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

                        RectF tr = r;
                        tr.Inflate(-10.0f, 0.0f);

                        SolidBrush brTxt(Gp(UiTheme::Color_Text(), 255));
                        g.DrawString(text.c_str(), -1, &font, tr, &fmt, &brTxt);
                    }

                    EndPaint(hWnd, &ps);
                    return 0;
                }

                case WM_NCDESTROY:
                    SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
                    return 0;
                }

                return DefWindowProcW(hWnd, msg, wParam, lParam);
            };

        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE);
        wc.lpszClassName = L"DD_PresetToast";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }

    HWND ownerTop = GetAncestor(hPage, GA_ROOT);

    st->hToast = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        L"DD_PresetToast",
        L"",
        WS_POPUP,
        0, 0, 10, 10,
        ownerTop, nullptr, (HINSTANCE)GetWindowLongPtrW(hPage, GWLP_HINSTANCE),
        st);

    if (st->hToast)
        ShowWindow(st->hToast, SW_HIDE);
}

static void Toast_Hide(HWND hPage, ConfigPageState* st)
{
    if (!st) return;
    st->toastHideAt = 0;
    if (hPage) KillTimer(hPage, TOAST_TIMER_ID);
    if (st->hToast) ShowWindow(st->hToast, SW_HIDE);
}

static void Toast_ShowNearCursor(HWND hPage, ConfigPageState* st, const wchar_t* text)
{
    if (!st || !hPage) return;

    Toast_EnsureWindow(hPage, st);
    if (!st->hToast) return;

    st->toastText = (text ? text : L"");

    // Measure text using Win32 DrawText for good sizing
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HDC hdc = GetDC(hPage);
    HGDIOBJ oldF = SelectObject(hdc, font);

    RECT calc{ 0,0,0,0 };
    DrawTextW(hdc, st->toastText.c_str(), (int)st->toastText.size(), &calc,
        DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, oldF);
    ReleaseDC(hPage, hdc);

    int padX = S(hPage, 16);
    int padY = S(hPage, 10);

    // FIX: RECT uses LONG; std::max needs same types
    int textW = (int)(calc.right - calc.left);
    int textH = (int)(calc.bottom - calc.top);

    int w = textW + padX * 2;
    int h = std::max(S(hPage, 34), textH + padY * 2);

    w = std::clamp(w, S(hPage, 220), S(hPage, 520));

    POINT pt{};
    GetCursorPos(&pt);

    int x = pt.x + S(hPage, 14);
    int y = pt.y + S(hPage, 18);

    // Clamp to monitor work area
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi))
    {
        RECT wa = mi.rcWork;
        if (x + w > wa.right) x = wa.right - w;
        if (y + h > wa.bottom) y = wa.bottom - h;
        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
    }

    SetWindowPos(st->hToast, HWND_TOPMOST, x, y, w, h,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    InvalidateRect(st->hToast, nullptr, TRUE);

    st->toastHideAt = GetTickCount() + TOAST_SHOW_MS;
    SetTimer(hPage, TOAST_TIMER_ID, 30, nullptr);
}

static void DeleteConfirm_Clear(HWND hPage, ConfigPageState* st)
{
    if (!st) return;
    st->pendingDeleteIdx = -1;
    st->pendingDeleteTick = 0;
    Toast_Hide(hPage, st);
}

// Draw hint for CP weights
static void DrawCpWeightHintIfNeeded(HWND hWnd, HDC hdc)
{
    float w01 = 0.0f;
    KeySettingsPanel_DragHint hint = KeySettingsPanel_GetDragHint(&w01);
    if (hint == KeySettingsPanel_DragHint::None)
        return;

    RECT rcClient{};
    GetClientRect(hWnd, &rcClient);

    int x = S(hWnd, 12);
    int y = S(hWnd, 286);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    const Color orange(255, 255, 170, 0);
    const Color orangeBorder(255, 210, 135, 0);

    float iconD = (float)S(hWnd, 16);
    RectF icon((float)x, (float)y, iconD, iconD);

    {
        SolidBrush br(orange);
        g.FillEllipse(&br, icon);
        Pen pen(orangeBorder, 1.0f);
        g.DrawEllipse(&pen, icon);
    }

    {
        FontFamily ff(L"Segoe UI");
        float em = std::clamp(iconD * 0.78f, 10.0f, 14.0f);
        Font font(&ff, em, FontStyleBold, UnitPixel);

        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentCenter);
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

        SolidBrush txt(Color(255, 20, 20, 20));
        g.DrawString(L"!", -1, &font, icon, &fmt, &txt);
    }

    int pct = (int)std::lround(std::clamp(w01, 0.0f, 1.0f) * 100.0f);

    wchar_t msg2[256]{};
    swprintf_s(msg2, L"Use mouse wheel to change weight (%d%%).", pct);

    {
        FontFamily ff(L"Segoe UI");
        float em = (float)S(hWnd, 13);
        em = std::clamp(em, 11.0f, 14.0f);
        Font font(&ff, em, FontStyleRegular, UnitPixel);

        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentNear);
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

        RectF tr(icon.GetRight() + 8.0f, (REAL)y - 1.0f,
            (REAL)(rcClient.right - rcClient.left) - (icon.GetRight() + 8.0f) - (REAL)S(hWnd, 12),
            iconD + 2.0f);

        SolidBrush txt(Gp(UiTheme::Color_TextMuted(), 255));
        g.DrawString(msg2, -1, &font, tr, &fmt, &txt);
    }

    (void)hint;
}

static HWND GetPresetCombo(HWND hWnd)
{
    return GetDlgItem(hWnd, KSP_ID_PROFILE);
}

static void SelectActivePresetInCombo(HWND hWnd)
{
    HWND hCombo = GetPresetCombo(hWnd);
    if (!hCombo) return;

    std::vector<KeyboardProfiles::ProfileInfo> list;
    int activeIdx = KeyboardProfiles::RefreshList(list); // active preset index among presets

    if (activeIdx >= 0)
    {
        // Indices match: KeySettingsPanel adds "+ Create..." after preset items.
        PremiumCombo::SetCurSel(hCombo, activeIdx, false);
        PremiumCombo::SetExtraIcon(hCombo, PremiumCombo::ExtraIconKind::None);
    }
}

static void DoBeginInlineCreate(HWND hWnd, ConfigPageState* st)
{
    HWND hCombo = GetPresetCombo(hWnd);
    if (!hCombo) return;

    int count = PremiumCombo::GetCount(hCombo);
    if (count <= 0) return;

    int idx = count - 1;

    PremiumCombo::ShowDropDown(hCombo, true);
    PremiumCombo::SetCurSel(hCombo, idx, false);
    PremiumCombo::BeginInlineEdit(hCombo, idx, false);

    SetProfileStatus(st, L"Type a name and press Enter to create a new preset.");
}

static bool DeletePreset_NoPopup_ConfigPage(HWND hWnd, ConfigPageState* st, int idx, bool requireShift)
{
    std::vector<KeyboardProfiles::ProfileInfo> list;
    KeyboardProfiles::RefreshList(list);

    if (idx < 0 || idx >= (int)list.size())
        return false;

    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (requireShift && !shift)
    {
        SetProfileStatus(st, L"Hold Shift to delete.");
        return false;
    }

    if (KeyboardProfiles::DeletePreset(list[idx].path))
    {
        SetProfileStatus(st, L"Preset deleted.");
        KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
        RequestSave(hWnd);
        return true;
    }

    SetProfileStatus(st, L"ERROR: Failed to delete preset.");
    KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
    return false;
}

LRESULT CALLBACK KeyboardSubpages_ConfigPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = (ConfigPageState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    if (msg == WM_TIMER)
    {
        // 1) toast auto-hide
        if (wParam == TOAST_TIMER_ID && st)
        {
            DWORD now = GetTickCount();
            if (st->toastHideAt != 0 && now >= st->toastHideAt)
                Toast_Hide(hWnd, st);
            return 0;
        }

        // 2) forward other timers to KeySettings panel (morph etc.)
        KeySettingsPanel_HandleTimer(hWnd, wParam);
        return 0;
    }

    if (msg == WM_APP_PROFILE_BEGIN_CREATE)
    {
        DoBeginInlineCreate(hWnd, st);
        return 0;
    }

    // Inline text commit from PremiumCombo (Rename + Create New)
    if (msg == PremiumCombo::MsgItemTextCommit())
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);
        HWND hCombo = (HWND)lParam;

        if (kind != PremiumCombo::ItemButtonKind::Rename || !hCombo)
            return 0;

        wchar_t newNameBuf[260]{};
        PremiumCombo::ConsumeCommittedText(hCombo, newNameBuf, 260);

        std::wstring raw = newNameBuf;
        std::wstring safe = SanitizePresetNameForFile(raw);

        std::vector<KeyboardProfiles::ProfileInfo> list;
        KeyboardProfiles::RefreshList(list);

        // Current curve shown on screen (the whole point of presets)
        KeyDeadzone curCurve = Ksp_GetVisualCurve();

        // Case A: commit came from the last row => create new preset
        if (idx == (int)list.size())
        {
            if (safe.empty())
            {
                SetProfileStatus(st, L"");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                return 0;
            }

            if (KeyboardProfiles::CreatePreset(safe, curCurve))
            {
                SetProfileStatus(st, L"Preset created.");

                // refresh list in KeySettingsPanel
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);

                // force UI selection to the now-active preset (newly created)
                SelectActivePresetInCombo(hWnd);

                // optional: close dropdown after creation (feels premium)
                HWND hCombo2 = GetPresetCombo(hWnd);
                if (hCombo2) PremiumCombo::ShowDropDown(hCombo2, false);

                RequestSave(hWnd);
            }
            else
            {
                SetProfileStatus(st, L"ERROR: Failed to create preset.");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            }
            return 0;
        }

        // Case B: rename existing preset
        if (idx < 0 || idx >= (int)list.size())
        {
            SetProfileStatus(st, L"Rename failed.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        const auto& p = list[idx];

        if (safe.empty())
        {
            SetProfileStatus(st, L"");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        if (_wcsicmp(safe.c_str(), p.name.c_str()) == 0)
        {
            SetProfileStatus(st, L"");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        fs::path oldPath = fs::path(p.path);
        fs::path dir = oldPath.parent_path();
        fs::path newPath = dir / (safe + L".ini");

        if (GetFileAttributesW(newPath.wstring().c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            SetProfileStatus(st, L"Rename failed: name already exists.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        std::wstring active = KeyboardProfiles::GetActiveProfileName();

        // If renaming active preset:
        // Save current curve into new file (this also updates "active" state inside module),
        // then delete the old file.
        if (!active.empty() && _wcsicmp(active.c_str(), p.name.c_str()) == 0)
        {
            if (!KeyboardProfiles::SavePreset(newPath.wstring(), curCurve))
            {
                SetProfileStatus(st, L"Rename failed: could not save new preset.");
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                return 0;
            }

            DeleteFileW(oldPath.wstring().c_str());

            SetProfileStatus(st, L"Preset renamed.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            RequestSave(hWnd);
            return 0;
        }

        // Non-active preset: simple file rename
        BOOL ok = MoveFileExW(oldPath.wstring().c_str(), newPath.wstring().c_str(),
            MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);

        if (!ok)
        {
            SetProfileStatus(st, L"Rename failed: file rename error.");
            KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
            return 0;
        }

        SetProfileStatus(st, L"Preset renamed.");
        KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
        return 0;
    }

    // Item button clicks (Delete gets delivered here; Rename starts inline edit internally)
    if (msg == PremiumCombo::MsgItemButton())
    {
        int idx = (int)LOWORD(wParam);
        PremiumCombo::ItemButtonKind kind = (PremiumCombo::ItemButtonKind)(int)HIWORD(wParam);

        if (kind == PremiumCombo::ItemButtonKind::Delete)
        {
            if (!st) return 0;

            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            // Shift => instant delete (no confirmation)
            if (shift)
            {
                DeleteConfirm_Clear(hWnd, st);
                DeletePreset_NoPopup_ConfigPage(hWnd, st, idx, false);
                return 0;
            }

            DWORD now = GetTickCount();

            // Second click within window => delete
            if (st->pendingDeleteIdx == idx && (now - st->pendingDeleteTick) <= TOAST_SHOW_MS)
            {
                DeleteConfirm_Clear(hWnd, st);
                DeletePreset_NoPopup_ConfigPage(hWnd, st, idx, false);
                return 0;
            }

            // First click => arm confirmation + show premium toast
            st->pendingDeleteIdx = idx;
            st->pendingDeleteTick = now;

            Toast_ShowNearCursor(hWnd, st, L"Click again to confirm delete");
            SetProfileStatus(st, L""); // don't spam status bar
            return 0;
        }

        // any other button cancels pending delete
        if (st) DeleteConfirm_Clear(hWnd, st);
        return 0;
    }

    // Extra icon click (save dirty preset)
    if (msg == PremiumCombo::MsgExtraIcon())
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

        HWND hCombo = (HWND)lParam;

        std::vector<KeyboardProfiles::ProfileInfo> list;
        KeyboardProfiles::RefreshList(list);

        int sel = -1;
        int count = 0;
        if (hCombo)
        {
            sel = PremiumCombo::GetCurSel(hCombo);
            count = PremiumCombo::GetCount(hCombo);
        }

        auto refreshUi = [&]()
            {
                KeySettingsPanel_HandleCommand(hWnd, 9999, 0);
                RequestSave(hWnd);
            };

        bool selIsCreateNew = (count > 0 && sel == (count - 1));

        if (list.empty() || selIsCreateNew || sel < 0)
        {
            PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
            return 0;
        }

        if (sel >= 0 && sel < (int)list.size())
        {
            const auto& p = list[sel];

            // Save CURRENT curve (visual state) into selected preset
            KeyDeadzone curCurve = Ksp_GetVisualCurve();

            if (KeyboardProfiles::SavePreset(p.path, curCurve))
            {
                std::wstring ok = L"Preset saved: " + p.name;
                SetProfileStatus(st, ok.c_str());
                refreshUi();
            }
            else
            {
                std::wstring err = L"ERROR: Failed to save preset: " + p.name;
                SetProfileStatus(st, err.c_str());
                refreshUi();
            }
            return 0;
        }

        PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
        return 0;
    }

    switch (msg)
    {
    case WM_ERASEBKGND: return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC memDC = nullptr;
        HBITMAP bmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        BeginDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);

        RECT rc{};
        GetClientRect(hWnd, &rc);

        KeySettingsPanel_DrawGraph(memDC, rc);
        DrawCpWeightHintIfNeeded(hWnd, memDC);

        EndDoubleBufferPaint(hWnd, ps, memDC, bmp, oldBmp);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);

        HWND hCtl = (HWND)lParam;
        if (st && hCtl)
        {
            if ((st->lblPoll && hCtl == st->lblPoll) ||
                (st->lblProfileStatus && hCtl == st->lblProfileStatus))
            {
                SetTextColor(hdc, UiTheme::Color_TextMuted());
            }
            else
            {
                SetTextColor(hdc, UiTheme::Color_Text());
            }
        }
        else
        {
            SetTextColor(hdc, UiTheme::Color_Text());
        }

        return (LRESULT)UiTheme::Brush_PanelBg();
    }

    case WM_CREATE:
    {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        st = new ConfigPageState();
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);

        KeySettingsPanel_Create(hWnd, hInst);
        KeySettingsPanel_SetSelectedHid(KeyboardUI_Internal_GetSelectedHid());

        st->lblPoll = CreateWindowW(L"STATIC", L"Polling rate",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10,
            hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblPoll, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->sldPoll = PremiumSlider_Create(hWnd, hInst, 0, 0, 10, 10, 7001);
        SendMessageW(st->sldPoll, TBM_SETRANGE, TRUE, MAKELONG(1, 20));
        SendMessageW(st->sldPoll, TBM_SETPOS, TRUE, (LPARAM)Settings_GetPollingMs());

        st->chipPoll = PremiumChip_Create(hWnd, hInst, 0, 0, 10, 10, 7002);
        SendMessageW(st->chipPoll, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->lblProfileStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10,
            hWnd, nullptr, hInst, nullptr);
        SendMessageW(st->lblProfileStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

        st->chkSnappy = CreateWindowW(L"BUTTON", L"Snappy Joystick",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_OWNERDRAW,
            0, 0, 10, 10,
            hWnd, (HMENU)(INT_PTR)ID_SNAPPY, hInst, nullptr);
        SendMessageW(st->chkSnappy, WM_SETFONT, (WPARAM)hFont, TRUE);

        // initial state
        SendMessageW(st->chkSnappy, BM_SETCHECK, Settings_GetSnappyJoystick() ? BST_CHECKED : BST_UNCHECKED, 0);

        // anim init (snap, no boot-animation)
        SetWindowSubclass(st->chkSnappy, SnappyToggle_SubclassProc, 1, 0);
        SnappyToggle_StartAnim(st->chkSnappy, Settings_GetSnappyJoystick(), false);

        UpdatePollingUi(st, Settings_GetPollingMs());
        LayoutConfigControls(hWnd, st);

        SetProfileStatus(st, L"");
        return 0;
    }

    case WM_SIZE:
        LayoutConfigControls(hWnd, st);
        break;

    case WM_MEASUREITEM:
    {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        if (KeySettingsPanel_HandleMeasureItem(mis))
            return TRUE;
        break;
    }

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;

        // 1) KeySettings panel controls
        if (KeySettingsPanel_HandleDrawItem(dis))
            return TRUE;

        // 2) Snappy toggle
        if (st && dis && dis->CtlType == ODT_BUTTON && dis->CtlID == ID_SNAPPY && st->chkSnappy == dis->hwndItem)
        {
            DrawSnappyToggleOwnerDraw(dis);
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
        if (KeySettingsPanel_HandleMouse(hWnd, msg, wParam, lParam))
            return 0;
        break;

    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        HWND hCombo = GetPresetCombo(hWnd);

        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        bool comboOpen = (hCombo && PremiumCombo::GetDroppedState(hCombo));
        bool comboEditing = (hCombo && PremiumCombo::IsEditingItem(hCombo));
        bool allowNonCtrl = comboOpen || comboEditing;

        // Ctrl+S = save preset
        if (ctrl && (wParam == 'S' || wParam == 's'))
        {
            if (hCombo)
            {
                WPARAM wp = MAKEWPARAM((UINT)PremiumCombo::ExtraIconKind::Save, (UINT)KSP_ID_PROFILE);
                PostMessageW(hWnd, PremiumCombo::MsgExtraIcon(), wp, (LPARAM)hCombo);
            }
            return 0;
        }

        // Undo/redo shortcuts
        if (KeySettingsPanel_HandleKey(hWnd, msg, wParam, lParam))
            return 0;

        // Non-ctrl keys: do nothing unless dropdown is open
        if (!ctrl && !allowNonCtrl)
            return 0;

        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        // F2 = rename selected (inline) [only when dropdown open]
        if (wParam == VK_F2)
        {
            if (hCombo)
            {
                int sel = PremiumCombo::GetCurSel(hCombo);
                int cnt = PremiumCombo::GetCount(hCombo);

                if (cnt > 0 && sel == cnt - 1)
                {
                    PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
                }
                else
                {
                    PremiumCombo::BeginInlineEditSelected(hCombo, true);
                    SetProfileStatus(st, L"Type a new name and press Enter.");
                }
            }
            return 0;
        }

        // Delete = delete selected (require Shift) [only when dropdown open]
        if (wParam == VK_DELETE)
        {
            if (hCombo)
            {
                int sel = PremiumCombo::GetCurSel(hCombo);
                int cnt = PremiumCombo::GetCount(hCombo);
                if (cnt > 0 && sel == cnt - 1)
                    return 0;

                if (!shift)
                {
                    SetProfileStatus(st, L"Hold Shift and press Delete to delete.");
                    return 0;
                }

                DeletePreset_NoPopup_ConfigPage(hWnd, st, sel, true);
            }
            return 0;
        }

        return 0;
    }

    case WM_HSCROLL:
    {
        if (st && st->sldPoll && (HWND)lParam == st->sldPoll)
        {
            int v = (int)SendMessageW(st->sldPoll, TBM_GETPOS, 0, 0);
            v = std::clamp(v, 1, 20);

            Settings_SetPollingMs((UINT)v);
            RealtimeLoop_SetIntervalMs(Settings_GetPollingMs());

            UpdatePollingUi(st, Settings_GetPollingMs());
            RequestSave(hWnd);
            return 0;
        }
        break;
    }

    case WM_COMMAND:
    {
        if (st) DeleteConfirm_Clear(hWnd, st);

        // Snappy toggle
        if (LOWORD(wParam) == (UINT)ID_SNAPPY && HIWORD(wParam) == BN_CLICKED && st && st->chkSnappy)
        {
            bool on = (SendMessageW(st->chkSnappy, BM_GETCHECK, 0, 0) == BST_CHECKED);

            Settings_SetSnappyJoystick(on);
            SnappyToggle_StartAnim(st->chkSnappy, on, true);

            RequestSave(hWnd);
            return 0;
        }

        // Preset selection:
        // If user selected "+ Create New..." row, start inline create (deferred).
        if (LOWORD(wParam) == (UINT)KSP_ID_PROFILE && HIWORD(wParam) == CBN_SELCHANGE)
        {
            HWND hCombo = (HWND)lParam;
            if (hCombo)
            {
                int sel = PremiumCombo::GetCurSel(hCombo);
                int cnt = PremiumCombo::GetCount(hCombo);
                if (cnt > 0 && sel == cnt - 1)
                {
                    PostMessageW(hWnd, WM_APP_PROFILE_BEGIN_CREATE, 0, 0);
                    return 0;
                }
            }
            // else: fallthrough to KeySettingsPanel_HandleCommand to apply preset
        }

        if (KeySettingsPanel_HandleCommand(hWnd, wParam, lParam))
            return 0;

        return 0;
    }

    case WM_NCDESTROY:
        // FIX: free cached GDI resources used by graph renderer
        KeySettingsPanel_Shutdown();

        if (st)
        {
            Toast_Hide(hWnd, st);
            if (st->hToast)
            {
                DestroyWindow(st->hToast);
                st->hToast = nullptr;
            }

            if (st->chkSnappy && IsWindow(st->chkSnappy))
            {
                // subclass will free state on WM_NCDESTROY of the control, but best-effort safety:
                SnappyToggle_Free(st->chkSnappy);
            }

            delete st;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}