#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "binding_actions.h"

// Рисует LB/RB как "геймпадный бампер":
// горизонтальный прямоугольник, один угол сильно скруглён, остальные — слабо.
// Полностью рисует glyph (сама фигура + обводка + опционально текст LB/RB).
void RemapBumpers_DrawGlyphAA(HDC hdc, const RECT& rc,
    BindAction action,        // Btn_LB / Btn_RB
    COLORREF baseColor,       // из RemapIconDef (можно использовать в режиме derive)
    bool brightFill,          // pressed/ghost
    float padRatio,           // внешний паддинг как у остальных иконок
    COLORREF borderOverride = CLR_INVALID);