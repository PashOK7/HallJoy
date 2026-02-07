#include "binding_actions.h"

#include "bindings.h"

void BindingActions_Apply(BindAction a, uint16_t hid)
{
    if (!hid) return;

    // Ensure uniqueness by KEY:
    // one keyboard key (HID) cannot be bound to multiple actions.
    // This does NOT remove other keys from the same gamepad button anymore.
    Bindings_ClearHid(hid);

    switch (a)
    {
        // ---- Axes (still single HID per direction) ----
    case BindAction::Axis_LX_Minus: Bindings_SetAxisMinus(Axis::LX, hid); break;
    case BindAction::Axis_LX_Plus:  Bindings_SetAxisPlus(Axis::LX, hid);  break;
    case BindAction::Axis_LY_Minus: Bindings_SetAxisMinus(Axis::LY, hid); break;
    case BindAction::Axis_LY_Plus:  Bindings_SetAxisPlus(Axis::LY, hid);  break;
    case BindAction::Axis_RX_Minus: Bindings_SetAxisMinus(Axis::RX, hid); break;
    case BindAction::Axis_RX_Plus:  Bindings_SetAxisPlus(Axis::RX, hid);  break;
    case BindAction::Axis_RY_Minus: Bindings_SetAxisMinus(Axis::RY, hid); break;
    case BindAction::Axis_RY_Plus:  Bindings_SetAxisPlus(Axis::RY, hid);  break;

        // ---- Triggers (still single HID) ----
    case BindAction::Trigger_LT: Bindings_SetTrigger(Trigger::LT, hid); break;
    case BindAction::Trigger_RT: Bindings_SetTrigger(Trigger::RT, hid); break;

        // ---- Buttons (NOW: add HID into mask, no overwriting) ----
    case BindAction::Btn_A: Bindings_AddButtonHid(GameButton::A, hid); break;
    case BindAction::Btn_B: Bindings_AddButtonHid(GameButton::B, hid); break;
    case BindAction::Btn_X: Bindings_AddButtonHid(GameButton::X, hid); break;
    case BindAction::Btn_Y: Bindings_AddButtonHid(GameButton::Y, hid); break;

    case BindAction::Btn_LB: Bindings_AddButtonHid(GameButton::LB, hid); break;
    case BindAction::Btn_RB: Bindings_AddButtonHid(GameButton::RB, hid); break;

    case BindAction::Btn_Back:  Bindings_AddButtonHid(GameButton::Back, hid); break;
    case BindAction::Btn_Start: Bindings_AddButtonHid(GameButton::Start, hid); break;
    case BindAction::Btn_Guide: Bindings_AddButtonHid(GameButton::Guide, hid); break;

    case BindAction::Btn_LS: Bindings_AddButtonHid(GameButton::LS, hid); break;
    case BindAction::Btn_RS: Bindings_AddButtonHid(GameButton::RS, hid); break;

    case BindAction::Btn_DU: Bindings_AddButtonHid(GameButton::DpadUp, hid); break;
    case BindAction::Btn_DD: Bindings_AddButtonHid(GameButton::DpadDown, hid); break;
    case BindAction::Btn_DL: Bindings_AddButtonHid(GameButton::DpadLeft, hid); break;
    case BindAction::Btn_DR: Bindings_AddButtonHid(GameButton::DpadRight, hid); break;
    }
}

bool BindingActions_TryGetByHid(uint16_t hid, BindAction& outAction)
{
    if (!hid) return false;

    // Axes
    auto ax = [&](Axis a, BindAction minusA, BindAction plusA) -> bool
        {
            AxisBinding b = Bindings_GetAxis(a);
            if (hid == b.minusHid) { outAction = minusA; return true; }
            if (hid == b.plusHid) { outAction = plusA;  return true; }
            return false;
        };

    if (ax(Axis::LX, BindAction::Axis_LX_Minus, BindAction::Axis_LX_Plus)) return true;
    if (ax(Axis::LY, BindAction::Axis_LY_Minus, BindAction::Axis_LY_Plus)) return true;
    if (ax(Axis::RX, BindAction::Axis_RX_Minus, BindAction::Axis_RX_Plus)) return true;
    if (ax(Axis::RY, BindAction::Axis_RY_Minus, BindAction::Axis_RY_Plus)) return true;

    // Triggers
    if (hid == Bindings_GetTrigger(Trigger::LT)) { outAction = BindAction::Trigger_LT; return true; }
    if (hid == Bindings_GetTrigger(Trigger::RT)) { outAction = BindAction::Trigger_RT; return true; }

    // Buttons (mask-based)
    auto bt = [&](GameButton b, BindAction a) -> bool
        {
            if (Bindings_ButtonHasHid(b, hid)) { outAction = a; return true; }
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