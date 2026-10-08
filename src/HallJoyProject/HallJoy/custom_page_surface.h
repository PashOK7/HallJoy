#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>

struct CustomPageSurface
{
    int scrollY = 0;
    int contentHeight = 0;
    int cacheWidth = 0;
    int cacheHeight = 0;
    HBITMAP contentCache = nullptr;
    bool cacheDirty = true;

    ULONGLONG scrollSampleStartMs = 0;
    ULONGLONG lastScrollInputMs = 0;
    ULONGLONG lastScrollLogMs = 0;
    uint32_t scrollPaints = 0;
    uint32_t cacheRebuilds = 0;
    uint64_t paintUsTotal = 0;
    uint64_t paintUsMax = 0;
    uint64_t cacheUsTotal = 0;
    uint64_t cacheUsMax = 0;
};

// Shared viewport input state. Scrollable pages keep one of these next to the
// surface; page-specific hover/pressed/drag state stays in the page model.
struct CustomPageScrollController
{
    bool draggingThumb = false;
    int thumbGrabOffsetY = 0;
    int thumbHeight = 0;
    int maxScrollAtDragStart = 0;
    int wheelRemainder = 0;
};

enum class CustomPageScrollResult
{
    NotHandled,
    Handled,
    OffsetChanged
};

using CustomPageRenderContentFn = void(*)(HWND hWnd, HDC hdc, const RECT& contentRc, void* user);

void CustomPageSurface_Destroy(CustomPageSurface* surface);
void CustomPageSurface_MarkDirty(HWND hWnd, CustomPageSurface* surface);
void CustomPageSurface_SetContentHeight(HWND hWnd, CustomPageSurface* surface, int contentHeight);
void CustomPageSurface_SetState(CustomPageSurface* surface, int scrollY, int contentHeight);
void CustomPageSurface_CopyState(const CustomPageSurface* surface, int* scrollY, int* contentHeight);

int CustomPageSurface_GetMaxScroll(HWND hWnd, const CustomPageSurface* surface);
void CustomPageSurface_SetScrollY(HWND hWnd, CustomPageSurface* surface, int scrollY);
RECT CustomPageSurface_GetScrollTrackRect(HWND hWnd);
RECT CustomPageSurface_GetScrollThumbRect(HWND hWnd, const CustomPageSurface* surface);
void CustomPageSurface_DrawScrollbar(HWND hWnd, HDC hdc, const CustomPageSurface* surface, bool dragging);
POINT CustomPageSurface_ClientToContent(const CustomPageSurface* surface, POINT clientPoint);
RECT CustomPageSurface_ContentToClient(const CustomPageSurface* surface, const RECT& contentRect);

CustomPageScrollResult CustomPageSurface_HandleScrollMessage(
    HWND hWnd,
    CustomPageSurface* surface,
    CustomPageScrollController* controller,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam,
    int wheelStepPx = 44);

uint64_t CustomPageSurface_QpcNow();
uint64_t CustomPageSurface_QpcToUs(uint64_t ticks);
void CustomPageSurface_BeginPaintSample(CustomPageSurface* surface, uint64_t paintStartQpc);
void CustomPageSurface_MaybeLogScrollPerf(CustomPageSurface* surface, const wchar_t* tag);

bool CustomPageSurface_RenderCache(
    HWND hWnd,
    HDC targetDC,
    CustomPageSurface* surface,
    CustomPageRenderContentFn renderContent,
    void* user);

bool CustomPageSurface_Present(
    HWND hWnd,
    HDC targetDC,
    CustomPageSurface* surface,
    CustomPageRenderContentFn renderContent,
    void* user,
    bool draggingScrollbar);

// ---------------------------------------------------------------------------
// The one paint standard for custom pages and their custom controls. Never
// pass the window DC to CustomPageSurface_Present: it clears the client
// first, so a direct call flashes the background and the scrollbar on every
// repaint. Every handler of WM_PAINT also handles WM_PRINTCLIENT with the same
// drawing code. (Checked by tests/custom_page_paint_static_audit.py.)

// One paint session for WM_PAINT and WM_PRINTCLIENT alike.
// WM_PAINT: composition happens in client coordinates in a pooled, persistent
// top-down 32bpp DIB (no allocation per paint; GDI+ writes the pixels
// directly), clipped to the invalidated region, which alone is copied to the
// window in one blit when the scope ends.
// WM_PRINTCLIENT: the whole client is drawn into the caller's DC (tab
// transition snapshots). Both start from the panel background, so one drawing
// code serves both and a snapshot cannot differ from the screen.
class CustomPagePaintScope
{
public:
    CustomPagePaintScope(HWND hWnd, UINT message, WPARAM wParam);
    ~CustomPagePaintScope();
    CustomPagePaintScope(const CustomPagePaintScope&) = delete;
    CustomPagePaintScope& operator=(const CustomPagePaintScope&) = delete;
    HDC Dc() const { return dc_; }
    // rcPaint is the region to compose (the client when printing).
    PAINTSTRUCT& Ps() { return ps_; }
    bool Printing() const { return printing_; }
private:
    HWND hwnd_ = nullptr;
    bool printing_ = false;
    PAINTSTRUCT ps_{};
    HDC dc_ = nullptr;
    HDC target_ = nullptr;
    HGDIOBJ oldBmp_ = nullptr;
    int buffer_ = -1;
    int savedDc_ = 0;
};

// Optional drawing on top of the presented page (same off-screen buffer).
using CustomPagePaintOverlayFn = void(*)(HWND hWnd, HDC hdc, void* user);
// Complete WM_PAINT / WM_PRINTCLIENT handler for a scrollable retained page.
void CustomPageSurface_Paint(HWND hWnd, UINT message, WPARAM wParam, CustomPageSurface* surface,
    CustomPageRenderContentFn renderContent, void* user, bool draggingScrollbar,
    CustomPagePaintOverlayFn overlay = nullptr, void* overlayUser = nullptr);

// Draws `root` and every descendant whose WS_VISIBLE style is set (whether or
// not an ancestor is hidden) into `dc`, in z-order, through WM_PRINTCLIENT.
// The client origin of `root` maps to the DC origin.
void CustomPage_PrintTree(HWND root, HDC dc);
