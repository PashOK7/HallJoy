#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "binding_actions.h"

// Рисует D-Pad (крестовину).
// Визуально это единая фигура (крест), у которого:
// - Есть внешние скругления (концы лепестков).
// - Есть внутренние скругления (стыки лепестков).
// - В зависимости от action (DU/DD/DL/DR) один из лепестков заливается активным цветом.
void RemapDpad_DrawGlyphAA(HDC hdc, const RECT& rc,
    BindAction action,
    COLORREF baseColor, // из RemapIconDef (можно игнорировать в конфиге)
    bool brightFill,
    float padRatio);