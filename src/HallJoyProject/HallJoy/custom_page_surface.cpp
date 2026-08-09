#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include <objidl.h>
#include <gdiplus.h>

#include "custom_page_surface.h"
#include "debug_log.h"
#include "ui_theme.h"
#include "win_util.h"

using namespace Gdiplus;

static int CpScale(HWND hwnd, int px)
{
    return WinUtil_ScalePx(hwnd, px);
}

static Color CpColor(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void CpAddRoundRectPath(GraphicsPath& path, const RectF& r, float rad)
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

static void CpDrawRoundRect(Graphics& g, const RECT& rc, COLORREF fill, COLORREF border, float radius, BYTE alpha)
{
    RectF r((REAL)rc.left, (REAL)rc.top, (REAL)(rc.right - rc.left), (REAL)(rc.bottom - rc.top));
    r.Inflate(-0.5f, -0.5f);
    GraphicsPath p;
    CpAddRoundRectPath(p, r, radius);
    SolidBrush br(CpColor(fill, alpha));
    g.FillPath(&br, &p);
    Pen pen(CpColor(border, alpha), 1.0f);
    g.DrawPath(&pen, &p);
}

void CustomPageSurface_Destroy(CustomPageSurface* surface)
{
    if (!surface) return;
    if (surface->contentCache)
    {
        DeleteObject(surface->contentCache);
        surface->contentCache = nullptr;
    }
    surface->cacheWidth = 0;
    surface->cacheHeight = 0;
    surface->cacheDirty = true;
}

void CustomPageSurface_MarkDirty(HWND hWnd, CustomPageSurface* surface)
{
    if (!surface) return;
    surface->cacheDirty = true;
    InvalidateRect(hWnd, nullptr, FALSE);
}

void CustomPageSurface_SetContentHeight(HWND hWnd, CustomPageSurface* surface, int contentHeight)
{
    if (!surface) return;
    surface->contentHeight = std::max(0, contentHeight);
    surface->scrollY = std::clamp(surface->scrollY, 0, CustomPageSurface_GetMaxScroll(hWnd, surface));
    surface->cacheDirty = true;
}

void CustomPageSurface_SetState(CustomPageSurface* surface, int scrollY, int contentHeight)
{
    if (!surface) return;
    surface->scrollY = std::max(0, scrollY);
    surface->contentHeight = std::max(0, contentHeight);
}

void CustomPageSurface_CopyState(const CustomPageSurface* surface, int* scrollY, int* contentHeight)
{
    if (!surface) return;
    if (scrollY) *scrollY = surface->scrollY;
    if (contentHeight) *contentHeight = surface->contentHeight;
}

int CustomPageSurface_GetMaxScroll(HWND hWnd, const CustomPageSurface* surface)
{
    if (!surface) return 0;
    RECT rc{};
    GetClientRect(hWnd, &rc);
    return std::max(0, surface->contentHeight - std::max(0, (int)(rc.bottom - rc.top)));
}

static void CpNoteScrollInput(CustomPageSurface* surface)
{
    if (!surface) return;
    ULONGLONG now = GetTickCount64();
    if (surface->scrollSampleStartMs == 0 || now - surface->lastScrollInputMs > 750 || now - surface->scrollSampleStartMs > 5000)
    {
        surface->scrollSampleStartMs = now;
        surface->lastScrollLogMs = now;
        surface->scrollPaints = 0;
        surface->cacheRebuilds = 0;
        surface->paintUsTotal = 0;
        surface->paintUsMax = 0;
        surface->cacheUsTotal = 0;
        surface->cacheUsMax = 0;
    }
    surface->lastScrollInputMs = now;
}

void CustomPageSurface_SetScrollY(HWND hWnd, CustomPageSurface* surface, int scrollY)
{
    if (!surface) return;
    int target = std::clamp(scrollY, 0, CustomPageSurface_GetMaxScroll(hWnd, surface));
    if (target == surface->scrollY)
        return;
    surface->scrollY = target;
    CpNoteScrollInput(surface);
    InvalidateRect(hWnd, nullptr, FALSE);
}

RECT CustomPageSurface_GetScrollTrackRect(HWND hWnd)
{
    RECT rc{};
    GetClientRect(hWnd, &rc);
    int w = CpScale(hWnd, 8);
    int m = CpScale(hWnd, 7);
    return RECT{ rc.right - m - w, rc.top + m, rc.right - m, rc.bottom - m };
}

RECT CustomPageSurface_GetScrollThumbRect(HWND hWnd, const CustomPageSurface* surface)
{
    RECT tr = CustomPageSurface_GetScrollTrackRect(hWnd);
    int trackH = std::max(1, (int)(tr.bottom - tr.top));
    int maxScroll = CustomPageSurface_GetMaxScroll(hWnd, surface);
    if (!surface || maxScroll <= 0)
        return tr;

    RECT rc{};
    GetClientRect(hWnd, &rc);
    int viewH = std::max(1, (int)(rc.bottom - rc.top));
    int thumbH = std::clamp((viewH * trackH) / std::max(viewH, surface->contentHeight), CpScale(hWnd, 36), trackH);
    int travel = std::max(0, trackH - thumbH);
    int top = tr.top;
    if (travel > 0)
    {
        double t = (double)std::clamp(surface->scrollY, 0, maxScroll) / (double)maxScroll;
        top += (int)std::lround(t * (double)travel);
    }
    return RECT{ tr.left, top, tr.right, top + thumbH };
}

void CustomPageSurface_DrawScrollbar(HWND hWnd, HDC hdc, const CustomPageSurface* surface, bool dragging)
{
    if (!surface || CustomPageSurface_GetMaxScroll(hWnd, surface) <= 0)
        return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    RECT tr = CustomPageSurface_GetScrollTrackRect(hWnd);
    RECT th = CustomPageSurface_GetScrollThumbRect(hWnd, surface);
    float rr = (float)(tr.right - tr.left) * 0.5f;
    CpDrawRoundRect(g, tr, RGB(44, 44, 48), RGB(44, 44, 48), rr, 180);
    CpDrawRoundRect(g, th, UiTheme::Color_Accent(), UiTheme::Color_Accent(), rr, dragging ? 240 : 205);
}

POINT CustomPageSurface_ClientToContent(const CustomPageSurface* surface, POINT clientPoint)
{
    if (surface)
        clientPoint.y += surface->scrollY;
    return clientPoint;
}

RECT CustomPageSurface_ContentToClient(const CustomPageSurface* surface, const RECT& contentRect)
{
    RECT result = contentRect;
    if (surface)
        OffsetRect(&result, 0, -surface->scrollY);
    return result;
}

CustomPageScrollResult CustomPageSurface_HandleScrollMessage(
    HWND hWnd,
    CustomPageSurface* surface,
    CustomPageScrollController* controller,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam,
    int wheelStepPx)
{
    if (!hWnd || !surface || !controller)
        return CustomPageScrollResult::NotHandled;

    const int before = surface->scrollY;
    switch (msg)
    {
    case WM_MOUSEWHEEL:
    {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta == 0)
            return CustomPageScrollResult::Handled;
        controller->wheelRemainder += delta;
        const int detents = controller->wheelRemainder / WHEEL_DELTA;
        controller->wheelRemainder %= WHEEL_DELTA;
        if (detents != 0)
            CustomPageSurface_SetScrollY(hWnd, surface,
                surface->scrollY - detents * std::max(1, wheelStepPx));
        break;
    }
    case WM_LBUTTONDOWN:
    {
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        const int maxScroll = CustomPageSurface_GetMaxScroll(hWnd, surface);
        if (maxScroll <= 0)
            return CustomPageScrollResult::NotHandled;
        const RECT thumb = CustomPageSurface_GetScrollThumbRect(hWnd, surface);
        const RECT track = CustomPageSurface_GetScrollTrackRect(hWnd);
        if (PtInRect(&thumb, pt))
        {
            controller->draggingThumb = true;
            controller->thumbGrabOffsetY = pt.y - thumb.top;
            controller->thumbHeight = std::max(1L, thumb.bottom - thumb.top);
            controller->maxScrollAtDragStart = maxScroll;
            SetCapture(hWnd);
            InvalidateRect(hWnd, &track, FALSE);
            return CustomPageScrollResult::Handled;
        }
        if (!PtInRect(&track, pt))
            return CustomPageScrollResult::NotHandled;

        RECT client{};
        GetClientRect(hWnd, &client);
        const int page = std::max(1, (int)(client.bottom - client.top) - CpScale(hWnd, 48));
        CustomPageSurface_SetScrollY(hWnd, surface,
            pt.y < thumb.top ? surface->scrollY - page : surface->scrollY + page);
        break;
    }
    case WM_MOUSEMOVE:
    {
        if (!controller->draggingThumb)
            return CustomPageScrollResult::NotHandled;
        POINT pt{ (short)LOWORD(lParam), (short)HIWORD(lParam) };
        const RECT track = CustomPageSurface_GetScrollTrackRect(hWnd);
        const int thumbH = std::max(1, controller->thumbHeight);
        const int travel = std::max(1, (int)(track.bottom - track.top) - thumbH);
        const int top = std::clamp((int)pt.y - controller->thumbGrabOffsetY,
            (int)track.top, (int)track.bottom - thumbH);
        const double position = (double)(top - track.top) / (double)travel;
        CustomPageSurface_SetScrollY(hWnd, surface,
            (int)std::lround(position * (double)controller->maxScrollAtDragStart));
        break;
    }
    case WM_LBUTTONUP:
        if (!controller->draggingThumb)
            return CustomPageScrollResult::NotHandled;
        controller->draggingThumb = false;
        if (GetCapture() == hWnd)
            ReleaseCapture();
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        if (!controller->draggingThumb)
            return CustomPageScrollResult::NotHandled;
        controller->draggingThumb = false;
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    default:
        return CustomPageScrollResult::NotHandled;
    }

    return surface->scrollY != before
        ? CustomPageScrollResult::OffsetChanged
        : CustomPageScrollResult::Handled;
}

uint64_t CustomPageSurface_QpcNow()
{
    LARGE_INTEGER q{};
    QueryPerformanceCounter(&q);
    return (uint64_t)q.QuadPart;
}

uint64_t CustomPageSurface_QpcToUs(uint64_t ticks)
{
    static LARGE_INTEGER freq = []()
    {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return f;
    }();
    return (ticks * 1000000ull) / (uint64_t)std::max<LONGLONG>(1, freq.QuadPart);
}

void CustomPageSurface_BeginPaintSample(CustomPageSurface* surface, uint64_t paintStartQpc)
{
    if (!surface || surface->scrollSampleStartMs == 0)
        return;
    uint64_t paintUs = CustomPageSurface_QpcToUs(CustomPageSurface_QpcNow() - paintStartQpc);
    surface->scrollPaints++;
    surface->paintUsTotal += paintUs;
    surface->paintUsMax = std::max(surface->paintUsMax, paintUs);
}

void CustomPageSurface_MaybeLogScrollPerf(CustomPageSurface* surface, const wchar_t* tag)
{
    if (!surface || surface->scrollSampleStartMs == 0)
        return;

    ULONGLONG now = GetTickCount64();
    if (now - surface->lastScrollLogMs < 1000 && now - surface->lastScrollInputMs <= 250)
        return;

    ULONGLONG elapsed = std::max<ULONGLONG>(1, now - surface->scrollSampleStartMs);
    double fps = (double)surface->scrollPaints * 1000.0 / (double)elapsed;
    double paintAvg = surface->scrollPaints ? (double)surface->paintUsTotal / (double)surface->scrollPaints : 0.0;
    double cacheAvg = surface->cacheRebuilds ? (double)surface->cacheUsTotal / (double)surface->cacheRebuilds : 0.0;
    DebugLog_Write(
        L"[%s] paints=%u approx_fps=%.1f paint_us_avg=%.1f paint_us_max=%llu cache_rebuilds=%u cache_us_avg=%.1f cache_us_max=%llu scroll_y=%d content_h=%d cache_dirty=%d",
        tag ? tag : L"ui.custom.scroll",
        surface->scrollPaints,
        fps,
        paintAvg,
        (unsigned long long)surface->paintUsMax,
        surface->cacheRebuilds,
        cacheAvg,
        (unsigned long long)surface->cacheUsMax,
        surface->scrollY,
        surface->contentHeight,
        surface->cacheDirty ? 1 : 0);

    surface->lastScrollLogMs = now;
    if (now - surface->lastScrollInputMs > 250)
    {
        surface->scrollSampleStartMs = 0;
        surface->scrollPaints = 0;
        surface->cacheRebuilds = 0;
        surface->paintUsTotal = 0;
        surface->paintUsMax = 0;
        surface->cacheUsTotal = 0;
        surface->cacheUsMax = 0;
    }
}

bool CustomPageSurface_RenderCache(
    HWND hWnd,
    HDC targetDC,
    CustomPageSurface* surface,
    CustomPageRenderContentFn renderContent,
    void* user)
{
    if (!surface || !renderContent) return false;

    RECT client{};
    GetClientRect(hWnd, &client);
    int width = std::max(1, (int)(client.right - client.left));
    int height = std::max(1, surface->contentHeight);

    if (surface->contentCache && surface->cacheWidth == width && surface->cacheHeight == height && !surface->cacheDirty)
        return true;

    if (!surface->contentCache || surface->cacheWidth != width || surface->cacheHeight != height)
    {
        CustomPageSurface_Destroy(surface);
        surface->contentCache = CreateCompatibleBitmap(targetDC, width, height);
        if (!surface->contentCache)
            return false;
        surface->cacheWidth = width;
        surface->cacheHeight = height;
    }

    HDC cacheDC = CreateCompatibleDC(targetDC);
    if (!cacheDC)
        return false;

    uint64_t cacheStart = CustomPageSurface_QpcNow();
    HGDIOBJ oldBmp = SelectObject(cacheDC, surface->contentCache);
    RECT full{ 0, 0, width, height };
    FillRect(cacheDC, &full, UiTheme::Brush_PanelBg());
    renderContent(hWnd, cacheDC, full, user);

    SelectObject(cacheDC, oldBmp);
    DeleteDC(cacheDC);
    surface->cacheDirty = false;
    uint64_t cacheUs = CustomPageSurface_QpcToUs(CustomPageSurface_QpcNow() - cacheStart);
    surface->cacheRebuilds++;
    surface->cacheUsTotal += cacheUs;
    surface->cacheUsMax = std::max(surface->cacheUsMax, cacheUs);
    return true;
}

bool CustomPageSurface_Present(
    HWND hWnd,
    HDC targetDC,
    CustomPageSurface* surface,
    CustomPageRenderContentFn renderContent,
    void* user,
    bool draggingScrollbar)
{
    if (!hWnd || !targetDC || !surface || !renderContent)
        return false;

    RECT client{};
    GetClientRect(hWnd, &client);
    FillRect(targetDC, &client, UiTheme::Brush_PanelBg());

    if (!CustomPageSurface_RenderCache(hWnd, targetDC, surface, renderContent, user))
        return false;

    HDC cacheDC = CreateCompatibleDC(targetDC);
    if (!cacheDC)
        return false;
    HGDIOBJ old = SelectObject(cacheDC, surface->contentCache);
    const int copyW = std::min((int)(client.right - client.left), surface->cacheWidth);
    const int copyH = std::min((int)(client.bottom - client.top),
        std::max(0, surface->cacheHeight - surface->scrollY));
    if (copyW > 0 && copyH > 0)
        BitBlt(targetDC, 0, 0, copyW, copyH, cacheDC, 0, surface->scrollY, SRCCOPY);
    SelectObject(cacheDC, old);
    DeleteDC(cacheDC);

    CustomPageSurface_DrawScrollbar(hWnd, targetDC, surface, draggingScrollbar);
    return true;
}
