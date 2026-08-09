#pragma once
#include <windows.h>
#include <cstdint>

HWND BindPanel_Create(HWND parent, HINSTANCE hInst);
void BindPanel_SetSelectedHid(uint16_t hid);
bool BindPanel_HandleCommand(HWND parent, WPARAM wParam, LPARAM lParam);