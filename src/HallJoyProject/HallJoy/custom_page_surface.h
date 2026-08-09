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
