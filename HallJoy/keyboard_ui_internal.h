#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

// Internal helpers for splitting keyboard_ui.cpp into smaller modules.

uint16_t KeyboardUI_Internal_GetSelectedHid();

// Subpage window procedures (implemented in keyboard_subpages.cpp)
LRESULT CALLBACK KeyboardSubpages_ConfigPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardSubpages_TesterPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);