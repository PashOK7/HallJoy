#include "../HallJoy/raw_input_packet_size.h"

#include <cassert>

int main()
{
    using halljoy::raw_input::ContainsTypedPayload;

    // Windows x64: RAWINPUTHEADER=24 and RAWKEYBOARD=16. This is the exact
    // 40-byte packet rejected by the old sizeof(RAWINPUT)==48 check.
    assert(ContainsTypedPayload(40, 40, 24, 16));
    assert(!ContainsTypedPayload(39, 39, 24, 16));

    // RAWMOUSE remains the larger 48-byte shape.
    assert(ContainsTypedPayload(48, 48, 24, 24));
    assert(!ContainsTypedPayload(40, 48, 24, 16));
    assert(!ContainsTypedPayload(40, 40, 25, 16));
    return 0;
}
