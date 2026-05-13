#include "binding_actions.h"

#include "bindings.h"

static void BindingActions_ApplyForPadInternal(int padIndex, BindAction a, uint16_t hid, bool clearExistingForHid)
{
    if (!hid) return;

    if (clearExistingForHid)
    {
        // Ensure uniqueness by KEY:
        // one keyboard key (HID) cannot be bound to multiple actions unless
        // the caller explicitly requests append mode.
        // This does NOT remove other keys from the same gamepad button anymore.
        Bindings_ClearHidForPad(padIndex, hid);
    }

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

void BindingActions_ApplyForPad(int padIndex, BindAction a, uint16_t hid)
{
    BindingActions_ApplyForPadInternal(padIndex, a, hid, true);
}

void BindingActions_AppendForPad(int padIndex, BindAction a, uint16_t hid)
{
    BindingActions_ApplyForPadInternal(padIndex, a, hid, false);
}

void BindingActions_Apply(BindAction a, uint16_t hid)
{
    BindingActions_ApplyForPad(0, a, hid);
}

static bool BindingActionToGameButton(BindAction a, GameButton& out)
{
    switch (a)
    {
    case BindAction::Btn_A: out = GameButton::A; return true;
    case BindAction::Btn_B: out = GameButton::B; return true;
    case BindAction::Btn_X: out = GameButton::X; return true;
    case BindAction::Btn_Y: out = GameButton::Y; return true;
    case BindAction::Btn_LB: out = GameButton::LB; return true;
    case BindAction::Btn_RB: out = GameButton::RB; return true;
    case BindAction::Btn_Back: out = GameButton::Back; return true;
    case BindAction::Btn_Start: out = GameButton::Start; return true;
    case BindAction::Btn_Guide: out = GameButton::Guide; return true;
    case BindAction::Btn_LS: out = GameButton::LS; return true;
    case BindAction::Btn_RS: out = GameButton::RS; return true;
    case BindAction::Btn_DU: out = GameButton::DpadUp; return true;
    case BindAction::Btn_DD: out = GameButton::DpadDown; return true;
    case BindAction::Btn_DL: out = GameButton::DpadLeft; return true;
    case BindAction::Btn_DR: out = GameButton::DpadRight; return true;
    default: return false;
    }
}

void BindingActions_RemoveFromPad(int padIndex, BindAction a, uint16_t hid)
{
    if (!hid) return;

    switch (a)
    {
    case BindAction::Axis_LX_Minus:
        if (Bindings_GetAxisForPad(padIndex, Axis::LX).minusHid == hid) Bindings_SetAxisMinusForPad(padIndex, Axis::LX, 0);
        break;
    case BindAction::Axis_LX_Plus:
        if (Bindings_GetAxisForPad(padIndex, Axis::LX).plusHid == hid) Bindings_SetAxisPlusForPad(padIndex, Axis::LX, 0);
        break;
    case BindAction::Axis_LY_Minus:
        if (Bindings_GetAxisForPad(padIndex, Axis::LY).minusHid == hid) Bindings_SetAxisMinusForPad(padIndex, Axis::LY, 0);
        break;
    case BindAction::Axis_LY_Plus:
        if (Bindings_GetAxisForPad(padIndex, Axis::LY).plusHid == hid) Bindings_SetAxisPlusForPad(padIndex, Axis::LY, 0);
        break;
    case BindAction::Axis_RX_Minus:
        if (Bindings_GetAxisForPad(padIndex, Axis::RX).minusHid == hid) Bindings_SetAxisMinusForPad(padIndex, Axis::RX, 0);
        break;
    case BindAction::Axis_RX_Plus:
        if (Bindings_GetAxisForPad(padIndex, Axis::RX).plusHid == hid) Bindings_SetAxisPlusForPad(padIndex, Axis::RX, 0);
        break;
    case BindAction::Axis_RY_Minus:
        if (Bindings_GetAxisForPad(padIndex, Axis::RY).minusHid == hid) Bindings_SetAxisMinusForPad(padIndex, Axis::RY, 0);
        break;
    case BindAction::Axis_RY_Plus:
        if (Bindings_GetAxisForPad(padIndex, Axis::RY).plusHid == hid) Bindings_SetAxisPlusForPad(padIndex, Axis::RY, 0);
        break;
    case BindAction::Trigger_LT:
        if (Bindings_GetTriggerForPad(padIndex, Trigger::LT) == hid) Bindings_SetTriggerForPad(padIndex, Trigger::LT, 0);
        break;
    case BindAction::Trigger_RT:
        if (Bindings_GetTriggerForPad(padIndex, Trigger::RT) == hid) Bindings_SetTriggerForPad(padIndex, Trigger::RT, 0);
        break;
    default:
    {
        GameButton b{};
        if (BindingActionToGameButton(a, b))
            Bindings_RemoveButtonHidForPad(padIndex, b, hid);
        break;
    }
    }
}

int BindingActions_CollectByHidForPad(int padIndex, uint16_t hid, BindAction* outActions, int maxActions)
{
    if (!hid || maxActions <= 0) return 0;
    int count = 0;

    auto add = [&](BindAction a) -> bool
        {
            if (outActions)
                outActions[count] = a;
            ++count;
            return count >= maxActions;
        };

    // Axes
    auto ax = [&](Axis a, BindAction minusA, BindAction plusA) -> bool
        {
            AxisBinding b = Bindings_GetAxisForPad(padIndex, a);
            if (hid == b.minusHid && add(minusA)) return true;
            if (hid == b.plusHid && add(plusA)) return true;
            return false;
        };

    if (ax(Axis::LX, BindAction::Axis_LX_Minus, BindAction::Axis_LX_Plus)) return count;
    if (ax(Axis::LY, BindAction::Axis_LY_Minus, BindAction::Axis_LY_Plus)) return count;
    if (ax(Axis::RX, BindAction::Axis_RX_Minus, BindAction::Axis_RX_Plus)) return count;
    if (ax(Axis::RY, BindAction::Axis_RY_Minus, BindAction::Axis_RY_Plus)) return count;

    // Triggers
    if (hid == Bindings_GetTriggerForPad(padIndex, Trigger::LT) && add(BindAction::Trigger_LT)) return count;
    if (hid == Bindings_GetTriggerForPad(padIndex, Trigger::RT) && add(BindAction::Trigger_RT)) return count;

    // Buttons (mask-based)
    auto bt = [&](GameButton b, BindAction a) -> bool
        {
            if (Bindings_ButtonHasHidForPad(padIndex, b, hid))
                return add(a);
            return false;
        };

    if (bt(GameButton::A, BindAction::Btn_A)) return count;
    if (bt(GameButton::B, BindAction::Btn_B)) return count;
    if (bt(GameButton::X, BindAction::Btn_X)) return count;
    if (bt(GameButton::Y, BindAction::Btn_Y)) return count;

    if (bt(GameButton::LB, BindAction::Btn_LB)) return count;
    if (bt(GameButton::RB, BindAction::Btn_RB)) return count;

    if (bt(GameButton::Back, BindAction::Btn_Back)) return count;
    if (bt(GameButton::Start, BindAction::Btn_Start)) return count;
    if (bt(GameButton::Guide, BindAction::Btn_Guide)) return count;

    if (bt(GameButton::LS, BindAction::Btn_LS)) return count;
    if (bt(GameButton::RS, BindAction::Btn_RS)) return count;

    if (bt(GameButton::DpadUp, BindAction::Btn_DU)) return count;
    if (bt(GameButton::DpadDown, BindAction::Btn_DD)) return count;
    if (bt(GameButton::DpadLeft, BindAction::Btn_DL)) return count;
    if (bt(GameButton::DpadRight, BindAction::Btn_DR)) return count;

    return count;
}

bool BindingActions_TryGetByHidForPad(int padIndex, uint16_t hid, BindAction& outAction)
{
    return BindingActions_CollectByHidForPad(padIndex, hid, &outAction, 1) > 0;
}

bool BindingActions_TryGetByHid(uint16_t hid, BindAction& outAction)
{
    return BindingActions_TryGetByHidForPad(0, hid, outAction);
}
