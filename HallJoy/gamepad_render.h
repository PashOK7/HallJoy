#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <string>

#include <ViGEm/Client.h>

// Shared UI helpers for rendering an XUSB_REPORT (used by Debug page and Tester page).

HBRUSH GamepadRender_GetAxisFillBrush();    // cached
HBRUSH GamepadRender_GetTriggerFillBrush(); // cached

void GamepadRender_DrawAxisBarCentered(HDC hdc, RECT rc, SHORT v);
void GamepadRender_DrawTriggerBar01(HDC hdc, RECT rc, uint8_t v);

// Converts wButtons bitmask to a compact string like "A LB DU"
std::wstring GamepadRender_ButtonsToString(WORD wButtons);