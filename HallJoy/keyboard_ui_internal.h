#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

// Internal helpers for splitting keyboard_ui.cpp into smaller modules.

uint16_t KeyboardUI_Internal_GetSelectedHid();

static constexpr UINT WM_APP_KEYBOARD_LAYOUT_CHANGED = WM_APP + 260;

// Subpage window procedures (implemented in keyboard_subpages.cpp)
LRESULT CALLBACK KeyboardSubpages_ConfigPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardSubpages_LayoutPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardSubpages_TesterPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardSubpages_GlobalSettingsPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
