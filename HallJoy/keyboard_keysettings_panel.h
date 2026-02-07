#pragma once
#include <windows.h>
#include <cstdint>

void KeySettingsPanel_Create(HWND parent, HINSTANCE hInst);
void KeySettingsPanel_SetSelectedHid(uint16_t hid);

// WM_COMMAND (Override / Invert / combo changes)
bool KeySettingsPanel_HandleCommand(HWND parent, WPARAM wParam, LPARAM lParam);

// Handle timer ticks for morphing
void KeySettingsPanel_HandleTimer(HWND parent, UINT_PTR timerId);

// Mouse on graph (WM_LBUTTONDOWN/UP/WM_MOUSEMOVE/WM_MOUSEWHEEL)
bool KeySettingsPanel_HandleMouse(HWND parent, UINT msg, WPARAM wParam, LPARAM lParam);

// Keyboard shortcuts (Config page should forward WM_KEYDOWN / WM_SYSKEYDOWN here)
bool KeySettingsPanel_HandleKey(HWND parent, UINT msg, WPARAM wParam, LPARAM lParam);

// Draw graph (called from Configuration page WM_PAINT)
void KeySettingsPanel_DrawGraph(HDC hdc, const RECT& rc);

// Owner-draw plumbing
bool KeySettingsPanel_HandleDrawItem(const DRAWITEMSTRUCT* dis);
bool KeySettingsPanel_HandleMeasureItem(MEASUREITEMSTRUCT* mis);

// -----------------------------------------------------------------------------
// Drag hint API
// -----------------------------------------------------------------------------
enum class KeySettingsPanel_DragHint : int
{
    None = 0,
    Cp1,
    Cp2
};

KeySettingsPanel_DragHint KeySettingsPanel_GetDragHint(float* outWeight01);

// -----------------------------------------------------------------------------
// Graph rect API
// -----------------------------------------------------------------------------
bool KeySettingsPanel_GetGraphRect(HWND parent, RECT* outRc);

// -----------------------------------------------------------------------------
// NEW: CP weight hint rect API (for precise invalidation)
// -----------------------------------------------------------------------------
// Returns the area where the "Use mouse wheel to change weight (xx%)" hint is drawn
// on the Config page (below the graph). Used to update it in real time.
bool KeySettingsPanel_GetCpWeightHintRect(HWND parent, RECT* outRc);

// -----------------------------------------------------------------------------
// Shutdown/cleanup (if you have it in your project)
// -----------------------------------------------------------------------------
// Frees internal cached GDI resources used by the graph renderer.
void KeySettingsPanel_Shutdown();