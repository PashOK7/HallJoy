#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "binding_actions.h"

// Рисует LT/RT как "триггер": более высокий прямоугольник с сильнее
// скруглёнными верхними углами и более слабыми нижними.
void RemapTriggers_DrawGlyphAA(HDC hdc, const RECT& rc,
    BindAction action,   // Trigger_LT / Trigger_RT
    COLORREF baseColor,  // из RemapIconDef (можно использовать в режиме derive)
    bool brightFill,     // сейчас по умолчанию игнорируется (в конфиге)
    float padRatio,      // внешний паддинг как у остальных иконок
    COLORREF borderOverride = CLR_INVALID);