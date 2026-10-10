#include "../HallJoy/bindings.h"

#include <cassert>

namespace {
void ExpectSide(const AxisKeys& keys, std::uint16_t a, std::uint16_t b = 0, std::uint16_t c = 0) {
    assert(keys[0] == a && keys[1] == b && keys[2] == c);
}
}

int main()
{
    using halljoy::keycode::kFn;

    // Empty axis.
    Bindings_SetAxisMinusForPad(0, Axis::LX, 0);
    Bindings_SetAxisPlusForPad(0, Axis::LX, 0);
    AxisBinding b = Bindings_GetAxisForPad(0, Axis::LX);
    ExpectSide(b.minusHids, 0);
    ExpectSide(b.plusHids, 0);

    // Set replaces the whole direction; Add appends in order.
    Bindings_SetAxisMinusForPad(0, Axis::LX, 4);
    assert(Bindings_AddAxisMinusForPad(0, Axis::LX, 7));
    assert(Bindings_AddAxisMinusForPad(0, Axis::LX, 22));
    b = Bindings_GetAxisForPad(0, Axis::LX);
    ExpectSide(b.minusHids, 4, 7, 22);
    assert(b.minusHid() == 4);

    // Adding an existing key is a successful no-op.
    assert(Bindings_AddAxisMinusForPad(0, Axis::LX, 7));
    ExpectSide(Bindings_GetAxisForPad(0, Axis::LX).minusHids, 4, 7, 22);

    // Zero and invalid keys are rejected.
    assert(!Bindings_AddAxisMinusForPad(0, Axis::LX, 0));
    assert(!Bindings_AddAxisMinusForPad(7, Axis::LX, 26));

    // Capacity: eight keys per direction, the ninth is refused.
    for (std::uint16_t hid = 26; hid < 26 + 5; ++hid) assert(Bindings_AddAxisMinusForPad(0, Axis::LX, hid));
    b = Bindings_GetAxisForPad(0, Axis::LX);
    for (int i = 0; i < BINDINGS_MAX_AXIS_KEYS; ++i) assert(b.minusHids[i] != 0);
    assert(!Bindings_AddAxisMinusForPad(0, Axis::LX, 31));

    // Removing one key keeps the list compact and in order.
    Bindings_RemoveAxisMinusForPad(0, Axis::LX, 7);
    b = Bindings_GetAxisForPad(0, Axis::LX);
    assert(b.minusHids[0] == 4 && b.minusHids[1] == 22 && b.minusHids[2] == 26);
    assert(b.minusHids[BINDINGS_MAX_AXIS_KEYS - 1] == 0);
    assert(Bindings_AddAxisMinusForPad(0, Axis::LX, 31));

    // Plus side is independent of minus.
    Bindings_SetAxisPlusForPad(0, Axis::LX, kFn);
    assert(Bindings_AddAxisPlusForPad(0, Axis::LX, 27));
    b = Bindings_GetAxisForPad(0, Axis::LX);
    ExpectSide(b.plusHids, kFn, 27);
    assert(b.minusHids[0] == 4);

    // A key on both sides is removed from both by RemoveAxisKey.
    Bindings_SetAxisPlusForPad(0, Axis::RX, 4);
    Bindings_RemoveAxisKeyForPad(0, Axis::LX, 4);
    b = Bindings_GetAxisForPad(0, Axis::LX);
    assert(b.minusHids[0] != 4);
    ExpectSide(Bindings_GetAxisForPad(0, Axis::RX).plusHids, 4);
    Bindings_RemoveAxisKeyForPad(0, Axis::RX, 4);
    assert(Bindings_GetAxisForPad(0, Axis::RX).plusHids[0] == 0);

    // ClearHid removes the key from every list on every pad.
    Bindings_SetAxisMinusForPad(1, Axis::RY, 9);
    assert(Bindings_AddAxisMinusForPad(1, Axis::RY, 10));
    assert(Bindings_IsHidBoundForPad(1, 10));
    Bindings_ClearHid(10);
    assert(!Bindings_IsHidBoundForPad(1, 10));
    ExpectSide(Bindings_GetAxisForPad(1, Axis::RY).minusHids, 9);

    // Snapshot round trip keeps every key of every direction.
    BindingsSnapshot snapshot{};
    Bindings_Capture(snapshot);
    Bindings_SetAxisMinusForPad(0, Axis::LX, 0);
    Bindings_SetAxisPlusForPad(0, Axis::LX, 0);
    Bindings_Apply(snapshot);
    b = Bindings_GetAxisForPad(0, Axis::LX);
    assert(b.minusHids[0] == 22 && b.minusHids[1] == 26 && b.plusHids[0] == kFn && b.plusHids[1] == 27);

    // Pad-0 shortcuts keep the single-key behaviour.
    Bindings_SetAxisMinus(Axis::LY, 22);
    assert(Bindings_GetAxis(Axis::LY).minusHid() == 22);
    Bindings_SetAxisMinus(Axis::LY, 0);
    assert(Bindings_GetAxis(Axis::LY).minusHid() == 0);
    return 0;
}
