#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// Draws a Wooting-like glossy face button (A/B/X/Y) into rc.
// Assumes GDI+ is already initialized somewhere in the app.
void RemapABXY_DrawGlyphAA(HDC hdc, const RECT& rc,
    const wchar_t* letter, COLORREF color,
    bool brightFill, float padRatio,
    COLORREF borderOverride = CLR_INVALID);
