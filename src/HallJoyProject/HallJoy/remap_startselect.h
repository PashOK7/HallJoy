#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "binding_actions.h"

// Рисует иконку Start или Select(Back) в стиле "пиктограмма вместо текста".
// Полностью рисует glyph (фон/рамку + пиктограмму), чтобы не затрагивать остальную систему.
//
// action должен быть BindAction::Btn_Start или BindAction::Btn_Back.
void RemapStartSelect_DrawGlyphAA(HDC hdc, const RECT& rc,
    BindAction action,
    COLORREF baseColor,
    bool brightFill,
    float padRatio,
    COLORREF borderOverride = CLR_INVALID);