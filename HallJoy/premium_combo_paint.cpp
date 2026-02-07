// premium_combo_paint.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <string>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "premium_combo_internal.h"
#include "ui_theme.h"

using namespace PremiumComboInternal;
using namespace Gdiplus;

static float EaseInOutCubic01(float t)
{
    t = Clamp01(t);
    if (t < 0.5f)
        return 4.0f * t * t * t;

    float u = -2.0f * t + 2.0f;
    return 1.0f - (u * u * u) * 0.5f;
}

// Returns display text for a given selection index.
// sel >=0 => items[sel]
// sel <0  => placeholderText (may be empty)
static std::wstring GetDisplayTextForSel(State* st, int sel, bool& outIsPlaceholder)
{
    outIsPlaceholder = false;
    if (!st) return L"";

    if (sel >= 0 && sel < (int)st->items.size())
        return st->items[sel];

    if (sel < 0 && !st->placeholderText.empty())
    {
        outIsPlaceholder = true;
        return st->placeholderText;
    }

    return L"";
}

static void DrawTextGpEllipsis(Graphics& g, HDC hdc, HFONT hFont,
    const std::wstring& text, const RectF& r, const Color& c)
{
    if (text.empty()) return;

    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    Font font(hdc, hFont ? hFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));

    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentNear);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap);

    SolidBrush br(c);
    g.DrawString(text.c_str(), -1, &font, r, &fmt, &br);
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static COLORREF LerpColor(COLORREF a, COLORREF b, float t)
{
    t = Clamp01(t);

    int ar = (int)GetRValue(a), ag = (int)GetGValue(a), ab = (int)GetBValue(a);
    int br = (int)GetRValue(b), bg = (int)GetGValue(b), bb = (int)GetBValue(b);

    int rr = (int)std::lroundf((float)ar + ((float)br - (float)ar) * t);
    int rg = (int)std::lroundf((float)ag + ((float)bg - (float)ag) * t);
    int rb = (int)std::lroundf((float)ab + ((float)bb - (float)ab) * t);

    rr = std::clamp(rr, 0, 255);
    rg = std::clamp(rg, 0, 255);
    rb = std::clamp(rb, 0, 255);

    return RGB(rr, rg, rb);
}

// Rounded rect with chamfer on top-right corner
static void AddRoundRectExceptTR_Chamfer(GraphicsPath& p, const RectF& r, float rad, float chamfer)
{
    float w = r.Width;
    float h = r.Height;

    rad = std::clamp(rad, 0.0f, std::min(w, h) * 0.5f);
    chamfer = std::clamp(chamfer, 0.0f, std::min(w, h) * 0.6f);

    float left = r.X;
    float top = r.Y;
    float right = r.GetRight();
    float bottom = r.GetBottom();
    float d = rad * 2.0f;

    p.StartFigure();

    p.AddLine(PointF(left + rad, top), PointF(right - chamfer, top));
    p.AddLine(PointF(right - chamfer, top), PointF(right, top + chamfer));
    p.AddLine(PointF(right, top + chamfer), PointF(right, bottom - rad));

    if (rad > 0.0f)
        p.AddArc(right - d, bottom - d, d, d, 0.0f, 90.0f);

    p.AddLine(PointF(right - rad, bottom), PointF(left + rad, bottom));

    if (rad > 0.0f)
        p.AddArc(left, bottom - d, d, d, 90.0f, 90.0f);

    p.AddLine(PointF(left, bottom - rad), PointF(left, top + rad));

    if (rad > 0.0f)
        p.AddArc(left, top, d, d, 180.0f, 90.0f);

    p.CloseFigure();
}

static Color GpFromColorRef(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

// -----------------------------------------------------------------------------
// Premium Floppy icon config (Save)
// -----------------------------------------------------------------------------
namespace FloppyCfg
{
    static constexpr float kPaddingRatio = 0.00f;

    static constexpr float kCornerRadiusRatio = 0.12f;
    static constexpr float kChamferSizeRatio = 0.28f;
    static constexpr float kStrokeWidthRatio = 0.08f;

    static constexpr float kTopBarHeightRatio = 0.4f;
    static constexpr float kTopBarMarginLeft = 0.15f;
    static constexpr float kTopBarMarginRight = 0.31f;

    static constexpr float kBottomBarHeightRatio = 0.25f;
    static constexpr float kBottomBarMarginLeft = 0.15f;
    static constexpr float kBottomBarMarginRight = 0.15f;

    static constexpr COLORREF kColorBody = RGB(50, 50, 55);
    static constexpr COLORREF kColorAccent = RGB(255, 140, 0);
    static constexpr COLORREF kColorBorder = RGB(160, 160, 170);

    static constexpr BYTE kBodyAlpha = 255;
    static constexpr BYTE kAccentAlpha = 230;
}

// -----------------------------------------------------------------------------
// Icons
// -----------------------------------------------------------------------------
static void DrawIconSave(HDC hdc, const RECT& rr, COLORREF /*colorOverride*/)
{
    int w = rr.right - rr.left;
    int h = rr.bottom - rr.top;
    if (w < 6 || h < 6) return;

    int sI = std::min(w, h);
    float s = (float)sI;
    float x0 = (float)rr.left + (float)(w - sI) * 0.5f;
    float y0 = (float)rr.top + (float)(h - sI) * 0.5f;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingQuality(CompositingQualityHighQuality);

    float pad = s * FloppyCfg::kPaddingRatio;
    RectF bodyRect(x0 + pad, y0 + pad, s - 2 * pad, s - 2 * pad);

    float radius = bodyRect.Width * FloppyCfg::kCornerRadiusRatio;
    float chamfer = bodyRect.Width * FloppyCfg::kChamferSizeRatio;
    float strokeW = std::max(1.0f, bodyRect.Width * FloppyCfg::kStrokeWidthRatio);

    RectF pathRect = bodyRect;
    pathRect.Inflate(-strokeW * 0.5f, -strokeW * 0.5f);

    GraphicsPath pathBody;
    AddRoundRectExceptTR_Chamfer(pathBody, pathRect, radius, chamfer);

    Color cBody = GpFromColorRef(FloppyCfg::kColorBody, FloppyCfg::kBodyAlpha);
    Color cAccent = GpFromColorRef(FloppyCfg::kColorAccent, FloppyCfg::kAccentAlpha);
    Color cBorder = GpFromColorRef(FloppyCfg::kColorBorder);

    SolidBrush brBody(cBody);
    g.FillPath(&brBody, &pathBody);

    Region rgn(&pathBody);
    g.SetClip(&rgn);

    {
        SolidBrush brAccent(cAccent);

        float topH = bodyRect.Height * FloppyCfg::kTopBarHeightRatio;
        float topPadL = bodyRect.Width * FloppyCfg::kTopBarMarginLeft;
        float topPadR = bodyRect.Width * FloppyCfg::kTopBarMarginRight;

        RectF rectTop(
            bodyRect.X + topPadL,
            bodyRect.Y,
            bodyRect.Width - (topPadL + topPadR),
            topH
        );
        g.FillRectangle(&brAccent, rectTop);

        float botH = bodyRect.Height * FloppyCfg::kBottomBarHeightRatio;
        float botPadL = bodyRect.Width * FloppyCfg::kBottomBarMarginLeft;
        float botPadR = bodyRect.Width * FloppyCfg::kBottomBarMarginRight;

        RectF rectBot(
            bodyRect.X + botPadL,
            bodyRect.GetBottom() - botH,
            bodyRect.Width - (botPadL + botPadR),
            botH
        );
        g.FillRectangle(&brAccent, rectBot);
    }

    g.ResetClip();

    Pen penBorder(cBorder, strokeW);
    penBorder.SetLineJoin(LineJoinRound);
    g.DrawPath(&penBorder, &pathBody);
}

static void DrawRoundBorder(HDC hdc, const RECT& rc, int radiusPx, COLORREF color, int penW)
{
    HPEN pen = CreatePen(PS_SOLID, std::max(1, penW), color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radiusPx, radiusPx);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void DrawChevronRotated(HDC hdc, int cx, int cy, int sizePx, float t01, COLORREF color)
{
    t01 = Clamp01(t01);
    float ang = t01 * 3.14159265f;

    float cs = cosf(ang);
    float sn = sinf(ang);

    float x0 = (float)(-sizePx);
    float y0 = (float)(-sizePx) * 0.5f;
    float x1 = 0.0f;
    float y1 = (float)(+sizePx) * 0.5f;
    float x2 = (float)(+sizePx);
    float y2 = (float)(-sizePx) * 0.5f;

    auto rot = [&](float x, float y, POINT& out)
        {
            float rx = x * cs - y * sn;
            float ry = x * sn + y * cs;
            out.x = cx + (int)std::lroundf(rx);
            out.y = cy + (int)std::lroundf(ry);
        };

    POINT p0{}, p1{}, p2{};
    rot(x0, y0, p0);
    rot(x1, y1, p1);
    rot(x2, y2, p2);

    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    MoveToEx(hdc, p0.x, p0.y, nullptr);
    LineTo(hdc, p1.x, p1.y);
    LineTo(hdc, p2.x, p2.y);

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void DrawIconDelete(HDC hdc, const RECT& r, COLORREF color)
{
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    int cx = (r.left + r.right) / 2;
    int cy = (r.top + r.bottom) / 2;

    int s = std::min(w, h) / 2;

    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    MoveToEx(hdc, cx - s, cy - s, nullptr);
    LineTo(hdc, cx + s + 1, cy + s + 1);

    MoveToEx(hdc, cx + s, cy - s, nullptr);
    LineTo(hdc, cx - s - 1, cy + s + 1);

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void DrawIconRename(HDC hdc, const RECT& r, COLORREF color)
{
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    if (w < 6 || h < 6) return;

    int cx = (r.left + r.right) / 2;
    int cy = (r.top + r.bottom) / 2;

    int s = std::min(w, h);
    int len = std::max(6, (int)std::lround(s * 0.65));
    int off = std::max(2, (int)std::lround(s * 0.18));

    POINT a{ cx - len / 2, cy + len / 2 - off };
    POINT b{ cx + len / 2, cy - len / 2 - off };

    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    MoveToEx(hdc, a.x, a.y, nullptr);
    LineTo(hdc, b.x, b.y);

    POINT t1{ b.x - std::max(2, s / 8), b.y + std::max(2, s / 10) };
    POINT t2{ b.x + std::max(2, s / 10), b.y + std::max(2, s / 8) };
    MoveToEx(hdc, t1.x, t1.y, nullptr);
    LineTo(hdc, b.x, b.y);
    LineTo(hdc, t2.x, t2.y);

    int uy = r.bottom - std::max(2, s / 6);
    MoveToEx(hdc, r.left + 3, uy, nullptr);
    LineTo(hdc, r.right - 3, uy);

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

// -----------------------------------------------------------------------------
// Double-buffer helper
// -----------------------------------------------------------------------------
struct PcBuf
{
    HDC memDC = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    int w = 0;
    int h = 0;

    bool Begin(HDC outDC, int ww, int hh)
    {
        w = ww; h = hh;
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

    void End(HDC outDC, int dstX, int dstY)
    {
        if (!memDC || !bmp) return;
        BitBlt(outDC, dstX, dstY, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(bmp);
        DeleteDC(memDC);
        memDC = nullptr;
        bmp = nullptr;
        oldBmp = nullptr;
        w = h = 0;
    }
};

static RECT ComputeExtraIconRectForPaint(HWND hwndCombo, State* st, const RECT& rcCombo)
{
    RECT empty{};
    if (!st) return empty;

    if (st->extraIconDraw == PremiumCombo::ExtraIconKind::None) return empty;
    if (st->extraIconT <= 0.001f) return empty;

    int w = rcCombo.right - rcCombo.left;
    int h = rcCombo.bottom - rcCombo.top;
    if (w <= 4 || h <= 4) return empty;

    int arrowW = std::clamp(S(hwndCombo, 26), 18, 34);
    int arrowLeft = rcCombo.right - arrowW;

    int size = S(hwndCombo, PC_EXTRAICON_SIZE);
    size = std::clamp(size, 10, std::max(10, h - 6));

    int padX = S(hwndCombo, PC_EXTRAICON_PAD_X);

    int right = arrowLeft - padX;
    int left = right - size;
    int top = rcCombo.top + (h - size) / 2;
    int bottom = top + size;

    if (left < rcCombo.left + 2) return empty;

    RECT r{ left, top, right, bottom };
    return r;
}

static RECT ScaleRectFromCenter(const RECT& r, float t01)
{
    RECT out{};
    t01 = Clamp01(t01);

    int w = r.right - r.left;
    int h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return out;

    int cx = (r.left + r.right) / 2;
    int cy = (r.top + r.bottom) / 2;

    int sw = std::max(1, (int)std::lroundf((float)w * t01));
    int sh = std::max(1, (int)std::lroundf((float)h * t01));

    out.left = cx - sw / 2;
    out.right = out.left + sw;
    out.top = cy - sh / 2;
    out.bottom = out.top + sh;

    return out;
}

// ============================================================================
// Paint closed combo (impl)
// ============================================================================
static void PaintCombo_Impl(HWND hwndCombo, State* st, HDC hdc)
{
    RECT rc{};
    GetClientRect(hwndCombo, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 1 || h <= 1) return;

    const bool enabled = st ? st->enabled : true;
    const bool hovered = st ? st->hovered : false;

    const float accentT = st ? Clamp01(st->arrowT) : 0.0f;

    FillRect(hdc, &rc, UiTheme::Brush_ControlBg());

    int arrowW = std::clamp(S(hwndCombo, 26), 18, 34);
    RECT rcArrow = rc;
    rcArrow.left = rc.right - arrowW;

    RECT rcArrowInner = rcArrow;
    InflateRect(&rcArrowInner, -1, -1);

    RECT rcText = rc;
    rcText.left += S(hwndCombo, 10);
    rcText.right -= arrowW;

    if (st && st->extraIconDraw != PremiumCombo::ExtraIconKind::None && st->extraIconT > 0.001f)
    {
        RECT rIconFull = ComputeExtraIconRectForPaint(hwndCombo, st, rc);
        if (rIconFull.right > rIconFull.left)
        {
            rcText.right = rIconFull.left - S(hwndCombo, PC_EXTRAICON_GAP);

            RECT rIconDraw = ScaleRectFromCenter(rIconFull, st->extraIconT);

            if (st->extraIconDraw == PremiumCombo::ExtraIconKind::Save)
            {
                COLORREF c = st->extraIconHot ? UiTheme::Color_Accent() : UiTheme::Color_TextMuted();
                DrawIconSave(hdc, rIconDraw, c);
            }
        }
    }

    if (st && st->arrowHot && enabled)
    {
        HBRUSH br = CreateSolidBrush(Brighten(UiTheme::Color_ControlBg(), 10));
        FillRect(hdc, &rcArrowInner, br);
        DeleteObject(br);
    }

    COLORREF baseBorder = UiTheme::Color_Border();
    if (hovered)
        baseBorder = Brighten(baseBorder, 18);

    COLORREF borderC = LerpColor(baseBorder, UiTheme::Color_Accent(), accentT);

    int radius = std::clamp(S(hwndCombo, 8), 4, 12);
    DrawRoundBorder(hdc, rc, radius, borderC, 1);

    {
        HPEN p2 = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
        HGDIOBJ op = SelectObject(hdc, p2);
        MoveToEx(hdc, rcArrow.left, rc.top + 2, nullptr);
        LineTo(hdc, rcArrow.left, rc.bottom - 2);
        SelectObject(hdc, op);
        DeleteObject(p2);
    }

    // ---------------- TEXT (static or animated scroll) ----------------
    HFONT hFont = GetFont(st);

    // Determine current displayed text (for non-animated path)
    bool curIsPlaceholder = false;
    std::wstring curText = GetDisplayTextForSel(st, st ? st->curSel : -1, curIsPlaceholder);

    auto textColorFor = [&](bool isPlaceholder) -> COLORREF
        {
            if (!enabled) return UiTheme::Color_TextMuted();
            if (isPlaceholder) return UiTheme::Color_TextMuted();
            return UiTheme::Color_Text();
        };

    // Animate only in closed state
    if (st && st->selAnimRunning && !st->dropped)
    {
        DWORD now = GetTickCount();
        DWORD dt = now - st->selAnimStartTick;
        DWORD dur = (st->selAnimDurationMs > 0) ? st->selAnimDurationMs : 1;

        float t = Clamp01((float)dt / (float)dur);
        float e = EaseInOutCubic01(t);

        // If animation is effectively finished, snap and stop.
        // (TickSelAnim should stop it too, but this avoids a 1-frame "half state" if paint happens late.)
        if (t >= 1.0f - 1e-4f)
        {
            st->selAnimRunning = false;

            COLORREF tc = textColorFor(curIsPlaceholder);
            SetBkMode(hdc, TRANSPARENT);
            HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
            SetTextColor(hdc, tc);

            DrawTextW(hdc, curText.c_str(), (int)curText.size(), &rcText,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            SelectObject(hdc, oldFont);
        }
        else
        {
            // Old/New texts
            bool oldIsPlaceholder = false;
            bool newIsPlaceholder = false;

            std::wstring oldText = GetDisplayTextForSel(st, st->selAnimFromSel, oldIsPlaceholder);
            std::wstring newText = GetDisplayTextForSel(st, st->selAnimToSel, newIsPlaceholder);

            // Colors (placeholder uses muted)
            COLORREF oldCRef = textColorFor(oldIsPlaceholder);
            COLORREF newCRef = textColorFor(newIsPlaceholder);

            // Alpha crossfade
            BYTE aOld = (BYTE)std::clamp((int)std::lroundf((1.0f - e) * 255.0f), 0, 255);
            BYTE aNew = (BYTE)std::clamp((int)std::lroundf(e * 255.0f), 0, 255);

            // Vertical slide distance/direction
            float dist = (st->selAnimDistPx > 0.0f) ? st->selAnimDistPx : 22.0f;
            int dir = (st->selAnimDir >= 0) ? 1 : -1;

            float yOld = -(float)dir * dist * e;
            float yNew = (float)dir * dist * (1.0f - e);

            Graphics g(hdc);
            g.SetSmoothingMode(SmoothingModeHighQuality);
            g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            g.SetCompositingQuality(CompositingQualityHighQuality);

            // Clip strictly to text area so it doesn't overlap icons/arrow
            RectF clip((REAL)rcText.left, (REAL)rcText.top,
                (REAL)(rcText.right - rcText.left),
                (REAL)(rcText.bottom - rcText.top));

            GraphicsState stSave = g.Save();
            g.SetClip(clip);

            RectF rOld = clip; rOld.Y += yOld;
            RectF rNew = clip; rNew.Y += yNew;

            Color oldCol(aOld, GetRValue(oldCRef), GetGValue(oldCRef), GetBValue(oldCRef));
            Color newCol(aNew, GetRValue(newCRef), GetGValue(newCRef), GetBValue(newCRef));

            DrawTextGpEllipsis(g, hdc, hFont, oldText, rOld, oldCol);
            DrawTextGpEllipsis(g, hdc, hFont, newText, rNew, newCol);

            g.Restore(stSave);
        }
    }
    else
    {
        // Static (no scroll animation)
        COLORREF tc = textColorFor(curIsPlaceholder);

        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, tc);

        DrawTextW(hdc, curText.c_str(), (int)curText.size(), &rcText,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(hdc, oldFont);
    }

    {
        int cx = (rcArrow.left + rcArrow.right) / 2;
        int cy = (rcArrow.top + rcArrow.bottom) / 2;
        int s = std::clamp(S(hwndCombo, 5), 3, 8);

        COLORREF chev = UiTheme::Color_TextMuted();
        if (st && st->arrowHot && enabled) chev = Brighten(chev, 25);

        float t = st ? Clamp01(st->arrowT) : 0.0f;
        DrawChevronRotated(hdc, cx, cy, s, t, chev);
    }
}

// ============================================================================
// Paint dropdown popup list (impl)
// ============================================================================
static void PaintPopup_Impl(HWND hwndPopup, State* st, HDC hdc)
{
    RECT rc{};
    GetClientRect(hwndPopup, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 1 || h <= 1) return;

    FillRect(hdc, &rc, UiTheme::Brush_ControlBg());

    {
        HPEN pen = CreatePen(PS_SOLID, 1, UiTheme::Color_Border());
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    if (!st) return;

    int ih = GetItemHeightPx(st->hwnd, st);
    int rows = GetVisibleRows(st);
    int n = (int)st->items.size();

    ClampScroll(st);

    HFONT oldFont = (HFONT)SelectObject(hdc, GetFont(st));
    SetBkMode(hdc, TRANSPARENT);

    int y = 0;
    for (int r = 0; r < rows; ++r)
    {
        int idx = st->scrollTop + r;
        if (idx < 0 || idx >= n) break;

        RECT row{};
        row.left = 0;
        row.right = w;
        row.top = y;
        row.bottom = y + ih;

        bool hotRow = (idx == st->hotIndex);
        bool sel = (idx == st->curSel);

        if (hotRow || sel)
        {
            COLORREF c = hotRow ? UiTheme::Color_Accent() : RGB(55, 55, 55);
            HBRUSH br = CreateSolidBrush(c);
            FillRect(hdc, &row, br);
            DeleteObject(br);
        }

        RECT textRc = row;
        textRc.left += S(hwndPopup, 10);
        textRc.right -= S(hwndPopup, 10);

        ItemBtnMask mask = BTN_NONE;
        if (idx >= 0 && idx < (int)st->itemBtnMask.size())
            mask = st->itemBtnMask[idx];

        if (mask != BTN_NONE)
        {
            RECT rDel = GetPopupItemButtonRect(st, idx, PremiumCombo::ItemButtonKind::Delete);
            RECT rRen = GetPopupItemButtonRect(st, idx, PremiumCombo::ItemButtonKind::Rename);

            int minLeft = INT_MAX;
            if (rDel.right > rDel.left) minLeft = std::min(minLeft, (int)rDel.left);
            if (rRen.right > rRen.left) minLeft = std::min(minLeft, (int)rRen.left);

            if (minLeft != INT_MAX)
                textRc.right = minLeft - S(hwndPopup, PC_ITEMBTN_GAP);

            if (rRen.right > rRen.left)
            {
                bool btnHot = (st->hotBtnIndex == idx && st->hotBtnKind == PremiumCombo::ItemButtonKind::Rename);
                COLORREF iconC = btnHot ? Brighten(UiTheme::Color_TextMuted(), 35) : UiTheme::Color_TextMuted();
                if (hotRow) iconC = RGB(12, 12, 12);
                DrawIconRename(hdc, rRen, iconC);
            }

            if (rDel.right > rDel.left)
            {
                bool btnHot = (st->hotBtnIndex == idx && st->hotBtnKind == PremiumCombo::ItemButtonKind::Delete);
                COLORREF iconC = btnHot ? RGB(255, 80, 80) : UiTheme::Color_TextMuted();
                if (hotRow && !btnHot) iconC = RGB(25, 25, 25);
                DrawIconDelete(hdc, rDel, iconC);
            }
        }

        if (hotRow)
            SetTextColor(hdc, RGB(12, 12, 12));
        else
            SetTextColor(hdc, UiTheme::Color_Text());

        bool editingThisRow = (st->hwndEdit && st->editIndex == idx);
        if (!editingThisRow)
        {
            DrawTextW(hdc, st->items[idx].c_str(), (int)st->items[idx].size(), &textRc,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        y += ih;
        if (y >= h) break;
    }

    // Scrollbar
    if (n > rows)
    {
        int maxTop = GetMaxScrollTop(st);
        if (maxTop > 0)
        {
            int trackW = std::clamp(S(hwndPopup, 6), 4, 10);
            RECT track{};
            track.left = w - trackW - 2;
            track.right = w - 2;
            track.top = 2;
            track.bottom = h - 2;

            {
                HBRUSH br = CreateSolidBrush(RGB(42, 42, 42));
                FillRect(hdc, &track, br);
                DeleteObject(br);
            }

            float tH = (float)(track.bottom - track.top);
            float thumbH = std::max((float)S(hwndPopup, 18), tH * ((float)rows / (float)n));
            float tt = (float)st->scrollTop / (float)maxTop;
            tt = std::clamp(tt, 0.0f, 1.0f);
            float y0 = (float)track.top + (tH - thumbH) * tt;

            RECT thumb{};
            thumb.left = track.left + 1;
            thumb.right = track.right - 1;
            thumb.top = (int)std::lroundf(y0);
            thumb.bottom = (int)std::lroundf(y0 + thumbH);

            {
                HBRUSH br = CreateSolidBrush(RGB(90, 90, 90));
                FillRect(hdc, &thumb, br);
                DeleteObject(br);
            }
        }
    }

    SelectObject(hdc, oldFont);
}

// ============================================================================
// Public paint entrypoints (buffered)
// ============================================================================
void PremiumComboInternal::PaintCombo(HWND hwndCombo, State* st, HDC hdc)
{
    RECT rc{};
    GetClientRect(hwndCombo, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 1 || h <= 1) return;

    PcBuf buf;
    if (!buf.Begin(hdc, w, h))
    {
        PaintCombo_Impl(hwndCombo, st, hdc);
        return;
    }

    RECT local{ 0,0,w,h };
    FillRect(buf.memDC, &local, UiTheme::Brush_ControlBg());

    SaveDC(buf.memDC);
    SetViewportOrgEx(buf.memDC, -rc.left, -rc.top, nullptr);

    PaintCombo_Impl(hwndCombo, st, buf.memDC);

    RestoreDC(buf.memDC, -1);
    buf.End(hdc, 0, 0);
}

void PremiumComboInternal::PaintPopup(HWND hwndPopup, State* st, HDC hdc)
{
    RECT rc{};
    GetClientRect(hwndPopup, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 1 || h <= 1) return;

    PcBuf buf;
    if (!buf.Begin(hdc, w, h))
    {
        PaintPopup_Impl(hwndPopup, st, hdc);
        return;
    }

    RECT local{ 0,0,w,h };
    FillRect(buf.memDC, &local, UiTheme::Brush_ControlBg());

    SaveDC(buf.memDC);
    SetViewportOrgEx(buf.memDC, -rc.left, -rc.top, nullptr);

    PaintPopup_Impl(hwndPopup, st, buf.memDC);

    RestoreDC(buf.memDC, -1);
    buf.End(hdc, 0, 0);
}