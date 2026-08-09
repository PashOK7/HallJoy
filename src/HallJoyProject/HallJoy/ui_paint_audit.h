#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>

#if defined(HALLJOY_UI_AUDIT_TRACE)
#include "debug_log.h"

struct UiPaintAuditCounter
{
    const wchar_t* surface = L"unknown";
    ULONGLONG windowStart = 0;
    unsigned paints = 0;
    unsigned erases = 0;
    unsigned long long dirtyPixels = 0;

    explicit UiPaintAuditCounter(const wchar_t* name) : surface(name) {}

    void Event(UINT message, const RECT* dirty = nullptr)
    {
        const ULONGLONG now = GetTickCount64();
        if (windowStart == 0) windowStart = now;
        if (message == WM_PAINT)
        {
            ++paints;
            if (dirty)
                dirtyPixels += (unsigned long long)(std::max)(0L, dirty->right - dirty->left) *
                    (unsigned long long)(std::max)(0L, dirty->bottom - dirty->top);
        }
        else if (message == WM_ERASEBKGND)
        {
            ++erases;
        }
        if (now - windowStart >= 1000)
        {
            DebugLog_WriteBuffered(L"[ui.audit.paint] surface=%s paints=%u erases=%u dirty_pixels=%llu window_ms=%llu",
                surface, paints, erases, dirtyPixels, now - windowStart);
            windowStart = now;
            paints = erases = 0;
            dirtyPixels = 0;
        }
    }
};

inline void UiAuditTraceInvalidation(const wchar_t* surface, const wchar_t* reason,
    const RECT* dirty = nullptr)
{
    if (dirty)
        DebugLog_WriteBuffered(L"[ui.audit.invalidate] surface=%s reason=%s rect=%ld,%ld,%ld,%ld",
            surface, reason, dirty->left, dirty->top, dirty->right, dirty->bottom);
    else
        DebugLog_WriteBuffered(L"[ui.audit.invalidate] surface=%s reason=%s rect=full",
            surface, reason);
}
#else
struct UiPaintAuditCounter
{
    explicit UiPaintAuditCounter(const wchar_t*) {}
    void Event(UINT, const RECT* = nullptr) {}
};
inline void UiAuditTraceInvalidation(const wchar_t*, const wchar_t*, const RECT* = nullptr) {}
#endif
