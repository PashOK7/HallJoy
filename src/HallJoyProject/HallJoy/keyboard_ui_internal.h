#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

// Internal helpers for splitting keyboard_ui.cpp into smaller modules.

uint16_t KeyboardUI_Internal_GetSelectedHid();
bool KeyboardUI_SaveBindingsAfterUserChange(HWND sourceWindow);

static constexpr UINT WM_APP_KEYBOARD_LAYOUT_CHANGED = WM_APP + 260;
static constexpr UINT WM_APP_CONFIG_TELEMETRY_REFRESH = WM_APP + 261;
static constexpr UINT WM_APP_TESTER_LIVE_REFRESH = WM_APP + 262;
static constexpr UINT WM_APP_KEYBOARD_GLOBAL_PROFILE_DIRTY = WM_APP + 122;
static constexpr WPARAM TESTER_REFRESH_DIAGNOSTICS = 1u << 8;

// Subpage window procedures (implemented in keyboard_subpages.cpp)
LRESULT CALLBACK KeyboardSubpages_ConfigPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardSubpages_LayoutPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardSubpages_TesterPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardSubpages_GlobalSettingsPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardSubpages_InputOverlayPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardSubpages_MouseSettingsPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
