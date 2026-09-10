#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

HWND KeyboardUI_CreatePage(HWND hParent, HINSTANCE hInst);
void KeyboardUI_OnTimerTick(HWND hPage);
void KeyboardUI_OnEngineStateChanged();
// Cancel or failed save vetoes editor destruction and application shutdown.
bool KeyboardUI_CloseLayoutEditor(bool saveWithoutPrompt = false);

bool KeyboardUI_HasHid(uint16_t hid);
// The sole UI-side path for an immediate binding mutation: it persists the
// binding, records dirty state only after confirmed durability, and requests
// the separately-debounced settings save on the root window.
bool KeyboardUI_SaveBindingsAfterUserChange(HWND sourceWindow);

// NEW: Remap panel tells keyboard UI which key is currently hovered as drop target
void KeyboardUI_SetDragHoverHid(uint16_t hid); // 0 = none
