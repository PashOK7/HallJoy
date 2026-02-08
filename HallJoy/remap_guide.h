#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// Guide/Home icon
void RemapGuide_DrawGlyphAA(HDC hdc, const RECT& rc, bool brightFill, float padRatio, COLORREF homeAccentOverride = CLR_INVALID);