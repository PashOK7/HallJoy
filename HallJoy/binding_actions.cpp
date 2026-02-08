#include "binding_actions.h"

#include "bindings.h"

void BindingActions_ApplyForPad(int padIndex, BindAction a, uint16_t hid)
{
    if (!hid) return;

    // Ensure uniqueness by KEY:
    // one keyboard key (HID) cannot be bound to multiple actions.
    // This does NOT remove other keys from the same gamepad button anymore.
    Bindings_ClearHidForPad(padIndex, hid);

    switch (a)
    {
        // ---- Axes (still single HID per direction) ----
    case BindAction::Axis_LX_Minus: Bindings_SetAxisMinusForPad(padIndex, Axis::LX, hid); break;
    case BindAction::Axis_LX_Plus:  Bindings_SetAxisPlusForPad(padIndex, Axis::LX, hid);  break;
    case BindAction::Axis_LY_Minus: Bindings_SetAxisMinusForPad(padIndex, Axis::LY, hid); break;
    case BindAction::Axis_LY_Plus:  Bindings_SetAxisPlusForPad(padIndex, Axis::LY, hid);  break;
    case BindAction::Axis_RX_Minus: Bindings_SetAxisMinusForPad(padIndex, Axis::RX, hid); break;
    case BindAction::Axis_RX_Plus:  Bindings_SetAxisPlusForPad(padIndex, Axis::RX, hid);  break;
    case BindAction::Axis_RY_Minus: Bindings_SetAxisMinusForPad(padIndex, Axis::RY, hid); break;
    case BindAction::Axis_RY_Plus:  Bindings_SetAxisPlusForPad(padIndex, Axis::RY, hid);  break;

        // ---- Triggers (still single HID) ----
    case BindAction::Trigger_LT: Bindings_SetTriggerForPad(padIndex, Trigger::LT, hid); break;
    case BindAction::Trigger_RT: Bindings_SetTriggerForPad(padIndex, Trigger::RT, hid); break;

        // ---- Buttons (NOW: add HID into mask, no overwriting) ----
    case BindAction::Btn_A: Bindings_AddButtonHidForPad(padIndex, GameButton::A, hid); break;
    case BindAction::Btn_B: Bindings_AddButtonHidForPad(padIndex, GameButton::B, hid); break;
    case BindAction::Btn_X: Bindings_AddButtonHidForPad(padIndex, GameButton::X, hid); break;
    case BindAction::Btn_Y: Bindings_AddButtonHidForPad(padIndex, GameButton::Y, hid); break;

    case BindAction::Btn_LB: Bindings_AddButtonHidForPad(padIndex, GameButton::LB, hid); break;
    case BindAction::Btn_RB: Bindings_AddButtonHidForPad(padIndex, GameButton::RB, hid); break;

    case BindAction::Btn_Back:  Bindings_AddButtonHidForPad(padIndex, GameButton::Back, hid); break;
    case BindAction::Btn_Start: Bindings_AddButtonHidForPad(padIndex, GameButton::Start, hid); break;
    case BindAction::Btn_Guide: Bindings_AddButtonHidForPad(padIndex, GameButton::Guide, hid); break;

    case BindAction::Btn_LS: Bindings_AddButtonHidForPad(padIndex, GameButton::LS, hid); break;
    case BindAction::Btn_RS: Bindings_AddButtonHidForPad(padIndex, GameButton::RS, hid); break;

    case BindAction::Btn_DU: Bindings_AddButtonHidForPad(padIndex, GameButton::DpadUp, hid); break;
    case BindAction::Btn_DD: Bindings_AddButtonHidForPad(padIndex, GameButton::DpadDown, hid); break;
    case BindAction::Btn_DL: Bindings_AddButtonHidForPad(padIndex, GameButton::DpadLeft, hid); break;
    case BindAction::Btn_DR: Bindings_AddButtonHidForPad(padIndex, GameButton::DpadRight, hid); break;
    }
}

void BindingActions_Apply(BindAction a, uint16_t hid)
{
    BindingActions_ApplyForPad(0, a, hid);
}

bool BindingActions_TryGetByHidForPad(int padIndex, uint16_t hid, BindAction& outAction)
{
    if (!hid) return false;

    // Axes
    auto ax = [&](Axis a, BindAction minusA, BindAction plusA) -> bool
        {
            AxisBinding b = Bindings_GetAxisForPad(padIndex, a);
            if (hid == b.minusHid) { outAction = minusA; return true; }
            if (hid == b.plusHid) { outAction = plusA;  return true; }
            return false;
        };

    if (ax(Axis::LX, BindAction::Axis_LX_Minus, BindAction::Axis_LX_Plus)) return true;
    if (ax(Axis::LY, BindAction::Axis_LY_Minus, BindAction::Axis_LY_Plus)) return true;
    if (ax(Axis::RX, BindAction::Axis_RX_Minus, BindAction::Axis_RX_Plus)) return true;
    if (ax(Axis::RY, BindAction::Axis_RY_Minus, BindAction::Axis_RY_Plus)) return true;

    // Triggers
    if (hid == Bindings_GetTriggerForPad(padIndex, Trigger::LT)) { outAction = BindAction::Trigger_LT; return true; }
    if (hid == Bindings_GetTriggerForPad(padIndex, Trigger::RT)) { outAction = BindAction::Trigger_RT; return true; }

    // Buttons (mask-based)
    auto bt = [&](GameButton b, BindAction a) -> bool
        {
            if (Bindings_ButtonHasHidForPad(padIndex, b, hid)) { outAction = a; return true; }
            return false;
        };

    if (bt(GameButton::A, BindAction::Btn_A)) return true;
    if (bt(GameButton::B, BindAction::Btn_B)) return true;
    if (bt(GameButton::X, BindAction::Btn_X)) return true;
    if (bt(GameButton::Y, BindAction::Btn_Y)) return true;

    if (bt(GameButton::LB, BindAction::Btn_LB)) return true;
    if (bt(GameButton::RB, BindAction::Btn_RB)) return true;

    if (bt(GameButton::Back, BindAction::Btn_Back)) return true;
    if (bt(GameButton::Start, BindAction::Btn_Start)) return true;
    if (bt(GameButton::Guide, BindAction::Btn_Guide)) return true;

    if (bt(GameButton::LS, BindAction::Btn_LS)) return true;
    if (bt(GameButton::RS, BindAction::Btn_RS)) return true;

    if (bt(GameButton::DpadUp, BindAction::Btn_DU)) return true;
    if (bt(GameButton::DpadDown, BindAction::Btn_DD)) return true;
    if (bt(GameButton::DpadLeft, BindAction::Btn_DL)) return true;
    if (bt(GameButton::DpadRight, BindAction::Btn_DR)) return true;

    return false;
}

bool BindingActions_TryGetByHid(uint16_t hid, BindAction& outAction)
{
    return BindingActions_TryGetByHidForPad(0, hid, outAction);
}
