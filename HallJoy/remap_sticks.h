#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "binding_actions.h"

// Рисует стики (Left Stick / Right Stick).
// Поддерживает:
// 1. Кнопки (L3/R3 или LS/RS) -> подсвечивается всё кольцо.
// 2. Оси (LX+, LY- и т.д.) -> подсвечивается сектор (дуга) в нужном направлении.
//
// Визуальный стиль (Wooting-like):
// - Внешний круг (тёмная база).
// - Внутренний круг (светлая шляпка).
// - Между ними: цветной индикатор.
void RemapSticks_DrawGlyphAA(HDC hdc, const RECT& rc,
    BindAction action,
    COLORREF baseColor, // из RemapIconDef (можно игнорировать при фикс. цветах)
    bool brightFill,
    float padRatio);