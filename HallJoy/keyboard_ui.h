#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

HWND KeyboardUI_CreatePage(HWND hParent, HINSTANCE hInst);
void KeyboardUI_OnTimerTick(HWND hPage);

bool KeyboardUI_HasHid(uint16_t hid);

// NEW: Remap panel tells keyboard UI which key is currently hovered as drop target
void KeyboardUI_SetDragHoverHid(uint16_t hid); // 0 = none