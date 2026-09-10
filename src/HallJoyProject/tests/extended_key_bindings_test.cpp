#include "../HallJoy/analog_key_codes.h"
#include "../HallJoy/bindings.h"

#include <cassert>

int main()
{
    using namespace halljoy::keycode;
    static_assert(kOem1 == 0x403);
    static_assert(kFn == 0x409);
    static_assert(kMaskChunkCount == 17);

    assert(Bindings_GetButtonMaskChunkCount() ==
        static_cast<int>(kMaskChunkCount));

    Bindings_AddButtonHidForPad(0, GameButton::A, kFn);
    Bindings_AddButtonHidForPad(0, GameButton::A, kOem1);
    assert(Bindings_ButtonHasHidForPad(0, GameButton::A, kFn));
    assert(Bindings_ButtonHasHidForPad(0, GameButton::A, kOem1));
    assert(Bindings_IsHidBoundForPad(0, kFn));
    assert(Bindings_IsHidBoundForPad(0, kOem1));

    const int fnChunk = kFn / 64;
    const int fnBit = kFn % 64;
    assert(Bindings_GetButtonMaskChunkForPad(0, GameButton::A, fnChunk) &
        (1ull << fnBit));

    Bindings_SetAxisPlusForPad(0, Axis::LX, kFn);
    Bindings_SetTriggerForPad(0, Trigger::LT, kOem1);
    assert(Bindings_GetAxisForPad(0, Axis::LX).plusHid == kFn);
    assert(Bindings_GetTriggerForPad(0, Trigger::LT) == kOem1);

    Bindings_ClearHidForPad(0, kFn);
    assert(!Bindings_ButtonHasHidForPad(0, GameButton::A, kFn));
    assert(Bindings_GetAxisForPad(0, Axis::LX).plusHid == 0);
    assert(Bindings_ButtonHasHidForPad(0, GameButton::A, kOem1));

    Bindings_ClearHidForPad(0, kOem1);
    assert(!Bindings_IsHidBoundForPad(0, kOem1));
    assert(Bindings_GetTriggerForPad(0, Trigger::LT) == 0);
    return 0;
}
