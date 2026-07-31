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
