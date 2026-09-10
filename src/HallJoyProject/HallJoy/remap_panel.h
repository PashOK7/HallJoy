#pragma once
#include <windows.h>

// Creates remap page (drag icons -> keyboard key to bind)
HWND RemapPanel_Create(HWND hParent, HINSTANCE hInst, HWND hKeyboardHost);

// Read-only, repeatable onboarding animation for a click on an unbound keyboard.
void RemapPanel_ShowBindingHint(HWND hPanel);

// Called when user selects a key on keyboard (optional for future UI; currently unused)
void RemapPanel_SetSelectedHid(uint16_t hid);
