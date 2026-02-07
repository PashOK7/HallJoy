#pragma once
#include <windows.h>

// Creates remap page (drag icons -> keyboard key to bind)
HWND RemapPanel_Create(HWND hParent, HINSTANCE hInst, HWND hKeyboardHost);

// Called when user selects a key on keyboard (optional for future UI; currently unused)
void RemapPanel_SetSelectedHid(uint16_t hid);