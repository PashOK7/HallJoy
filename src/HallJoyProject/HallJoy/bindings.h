#pragma once
#include <cstdint>

// NOTE: We support "many keys per GAMEPAD BUTTON" for the complete HallJoy
// key-code space, including Soup/UAP extended Fn/OEM codes.
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

// ---- Buttons (many source keys per button) ----
//
// Stored as uint64 bitmask chunks covering the complete supported key-code
// space. Bindings_GetButtonMaskChunkCount() is the iteration bound.
//
// HID==0 is treated as "none" and ignored.

void Bindings_AddButtonHid(GameButton b, uint16_t hid);
void Bindings_RemoveButtonHid(GameButton b, uint16_t hid);

bool Bindings_ButtonHasHid(GameButton b, uint16_t hid);

// Read-only access for fast iteration (backend/UI):
int Bindings_GetButtonMaskChunkCount();
uint64_t Bindings_GetButtonMaskChunk(GameButton b, int chunk);

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


// Complete prepared profile value; publication is guarded by profile_runtime_gate.
#include <array>
#include "analog_key_codes.h"
struct BindingsSnapshot {
    std::array<std::array<AxisBinding, 4>, BINDINGS_MAX_GAMEPADS> axes{};
    std::array<std::array<uint16_t, 2>, BINDINGS_MAX_GAMEPADS> triggers{};
    std::array<std::array<std::array<uint64_t, halljoy::keycode::kMaskChunkCount>, 15>,
        BINDINGS_MAX_GAMEPADS> buttons{};
};
void Bindings_Capture(BindingsSnapshot& out) noexcept;
void Bindings_Apply(const BindingsSnapshot& snapshot) noexcept;
