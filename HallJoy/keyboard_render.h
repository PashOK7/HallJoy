#pragma once
#include <windows.h>
#include <cstdint>
#include "binding_actions.h"

// reads analog with key-specific deadzones + global fallback
float KeyboardRender_ReadAnalog01(uint16_t hid);

// draws one key (owner-draw button)
// v01 can be a cached value (recommended). If v01 < 0 -> function will read itself.
void KeyboardRender_DrawKey(const DRAWITEMSTRUCT* dis, uint16_t hid, bool selected, float v01);

// -----------------------------------------------------------------------------
// NEW: gear animation support
// -----------------------------------------------------------------------------
// Returns list of HID codes (<=255) that currently need redraw for gear animation.
// Called from UI timer tick to invalidate only needed keys.
//
// outHids: buffer to write HIDs
// cap: buffer capacity
// returns: number of HIDs written
int KeyboardRender_GetAnimatingHids(uint16_t* outHids, int cap);

// -----------------------------------------------------------------------------
// NEW: gear "wow spin" interaction
// -----------------------------------------------------------------------------
//
// - Called by UI when selection changes on Configuration tab.
//   Used to stop spinning when user stops editing that key.
void KeyboardRender_NotifySelectedHid(uint16_t hid);

// - Called by UI when user clicks the gear marker on a key that has Override enabled.
//   Starts a smooth "fast burst -> slow idle spin" while the key remains selected/edited.
void KeyboardRender_OnGearClicked(uint16_t hid);

// Temporarily suppress one specific bound icon while drawing key(s), used by drag UX.
// Call KeyboardRender_ClearSuppressedBinding() after drawing.
void KeyboardRender_SetSuppressedBinding(uint16_t hid, int padIndex, BindAction action);
void KeyboardRender_ClearSuppressedBinding();
