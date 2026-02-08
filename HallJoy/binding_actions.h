#pragma once
#include <cstdint>

// Single source of truth: how a UI "action" maps to Bindings_* storage.
// Used by multiple UI panels (combo bind panel, drag&drop remap panel).

enum class BindAction : uint32_t
{
    Axis_LX_Minus, Axis_LX_Plus,
    Axis_LY_Minus, Axis_LY_Plus,
    Axis_RX_Minus, Axis_RX_Plus,
    Axis_RY_Minus, Axis_RY_Plus,

    Trigger_LT, Trigger_RT,

    Btn_A, Btn_B, Btn_X, Btn_Y,
    Btn_LB, Btn_RB,
    Btn_Back, Btn_Start,
    Btn_Guide,
    Btn_LS, Btn_RS,
    Btn_DU, Btn_DD, Btn_DL, Btn_DR,
};

// Clears existing usage of this HID, then applies the binding.
void BindingActions_Apply(BindAction a, uint16_t hid);
void BindingActions_ApplyForPad(int padIndex, BindAction a, uint16_t hid);

// NEW: reverse query (HID -> current action).
// Returns true if this HID is currently bound to some action.
bool BindingActions_TryGetByHid(uint16_t hid, BindAction& outAction);
bool BindingActions_TryGetByHidForPad(int padIndex, uint16_t hid, BindAction& outAction);
