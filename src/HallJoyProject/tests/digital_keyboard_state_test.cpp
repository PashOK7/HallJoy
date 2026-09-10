#include "digital_keyboard_state.h"
#include <cassert>

int main()
{
    halljoy::digital_keyboard::State s;
    // Every current canonical code uses the same path, including future
    // extended keys; no renderer-specific whitelist is involved.
    for (std::uint16_t key = 1; key < halljoy::keycode::kCount; ++key)
    {
        s.Observe(1, key, true);
        s.Observe(1, key, true); // typematic repeat
        s.Observe(2, key, true);
        s.Observe(1, key, false);
        assert(s.IsDown(key));
        s.Remove(2); // unplug while held
        assert(!s.IsDown(key));
    }
    s.Observe(1, 40, true);
    s.Observe(1, 88, true);
    s.Observe(1, 40, false);
    assert(!s.IsDown(40) && s.IsDown(88)); // independent Enter keys
    s.Observe(999, 88, false); // foreign release cannot cancel it
    assert(s.IsDown(88));
    s.Remove(1);
    assert(!s.IsDown(88));
    s.Observe(1, 0, true);
    s.Observe(1, 65535, true);
    assert(!s.IsDown(0) && !s.IsDown(65535));
    s.Observe(1, 89, true);
    s.Reset();
    assert(!s.IsDown(89));
    s.ObservePulse(1, 72, 150);
    s.Advance(149);
    assert(s.IsDown(72));
    s.Advance(150);
    assert(!s.IsDown(72));
    s.ObservePulse(1, 72, 300);
    s.Remove(1);
    s.Observe(1, 72, true); // reused device identity has no old expiry
    s.Advance(300);
    assert(s.IsDown(72));
}
