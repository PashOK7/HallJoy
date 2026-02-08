#pragma once
#include <cstdint>

// NOTE: We support "many keys per GAMEPAD BUTTON" by storing a HID bitmask (HID < 256).
// Axes and triggers remain single-HID as before.

enum class Axis
{
    LX, LY, RX, RY
};

struct AxisBinding
{
    uint16_t minusHid = 0;
    uint16_t plusHid = 0;
};

enum class Trigger
{
    LT, RT
};

enum class GameButton
{
    A, B, X, Y,
    LB, RB,
    Back, Start,
    Guide,          // NEW: Xbox / Guide button
    LS, RS,
    DpadUp, DpadDown, DpadLeft, DpadRight
};

constexpr int BINDINGS_MAX_GAMEPADS = 4;

// ---- Per-gamepad API ----
void Bindings_SetAxisMinusForPad(int padIndex, Axis a, uint16_t hid);
void Bindings_SetAxisPlusForPad(int padIndex, Axis a, uint16_t hid);
AxisBinding Bindings_GetAxisForPad(int padIndex, Axis a);

void Bindings_SetTriggerForPad(int padIndex, Trigger t, uint16_t hid);
uint16_t Bindings_GetTriggerForPad(int padIndex, Trigger t);

void Bindings_AddButtonHidForPad(int padIndex, GameButton b, uint16_t hid);
void Bindings_RemoveButtonHidForPad(int padIndex, GameButton b, uint16_t hid);
bool Bindings_ButtonHasHidForPad(int padIndex, GameButton b, uint16_t hid);
uint64_t Bindings_GetButtonMaskChunkForPad(int padIndex, GameButton b, int chunk);
uint16_t Bindings_GetButtonForPad(int padIndex, GameButton b);

void Bindings_ClearHidForPad(int padIndex, uint16_t hid);
bool Bindings_IsHidBoundForPad(int padIndex, uint16_t hid);

// Visual style (accent color identity) bound to pad slot.
// styleVariant: 1..4
void Bindings_SetPadStyleVariant(int padIndex, int styleVariant);
int  Bindings_GetPadStyleVariant(int padIndex);

// Removes one virtual gamepad slot and compacts following slots to the left.
// Example: removePadIndex=2, activePadCount=4 => old pad3 becomes pad2.
void Bindings_RemovePadAndCompact(int removePadIndex, int activePadCount);

// ---- Axes (unchanged: 1 key per direction) ----
void Bindings_SetAxisMinus(Axis a, uint16_t hid);
void Bindings_SetAxisPlus(Axis a, uint16_t hid);
AxisBinding Bindings_GetAxis(Axis a);

// ---- Triggers (unchanged: 1 key per trigger) ----
void Bindings_SetTrigger(Trigger t, uint16_t hid);
uint16_t Bindings_GetTrigger(Trigger t);

// ---- Buttons (NEW: unlimited keys for HID<256) ----
//
// Stored as 4x uint64 bitmask chunks covering HID 0..255:
// chunk 0 => HID 0..63, chunk 1 => 64..127, chunk 2 => 128..191, chunk 3 => 192..255
//
// HID==0 is treated as "none" and ignored.

void Bindings_AddButtonHid(GameButton b, uint16_t hid);     // add bit (HID<256)
void Bindings_RemoveButtonHid(GameButton b, uint16_t hid);  // remove bit (HID<256)

// Returns true if this HID is bound to the given button (HID<256)
bool Bindings_ButtonHasHid(GameButton b, uint16_t hid);

// Read-only access for fast iteration (backend/UI):
uint64_t Bindings_GetButtonMaskChunk(GameButton b, int chunk); // chunk = 0..3

// Legacy convenience:
// Returns ANY one bound HID (lowest set bit), or 0 if none.
// (Kept so old code can compile; new code should iterate mask.)
uint16_t Bindings_GetButton(GameButton b);

// Removes this HID from ALL actions:
// - axes (minus/plus)
// - triggers
// - buttons (mask bits)
// across ALL virtual gamepads.
void Bindings_ClearHid(uint16_t hid);

// Returns true if HID is used by any gamepad binding (axis/trigger/button).
// across ALL virtual gamepads.
bool Bindings_IsHidBound(uint16_t hid);
