#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "binding_actions.h"

// Single source of truth for Remap icons (and for drawing them consistently in other places).

struct RemapIconDef
{
    const wchar_t* label = L"";
    BindAction action{};
    COLORREF color = RGB(180, 180, 180);
    bool round = false;
};

int RemapIcons_Count();
const RemapIconDef& RemapIcons_Get(int idx);

// Helper: brighten an RGB color by +add per channel (clamped to 0..255).
COLORREF RemapIcons_Brighten(COLORREF c, int add = 15);

// Draws ONLY the "glyph" (colored circle/rounded-rect + outline + label) with AA (GDI+).
// - No square tile/background.
// - brightFill=true uses Brighten(color) for a "ghost/pressed" look.
// - padRatio controls inner padding (default 0.135 like ghost); smaller => fatter icon.
// - styleVariant:
//   0 = neutral single-gamepad style (no accent color),
//   1 = gamepad #1 accent style (yellow),
//   2..4 = gamepad #2..#4 accent styles (extended accent outlines).
void RemapIcons_DrawGlyphAA(HDC hdc, const RECT& rc, int iconIdx, bool brightFill, float padRatio = 0.135f, int styleVariant = 0);
