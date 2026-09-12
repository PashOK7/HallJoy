#include "block_keys_policy.h"
#include <cassert>
int main() {
    using namespace halljoy::block_keys;
    const unsigned scans[] = {0x52,0x4f,0x50,0x51,0x4b,0x4c,0x4d,0x47,0x48,0x49,0x53};
    const unsigned off[] = {0x2d,0x23,0x28,0x22,0x25,0x0c,0x27,0x24,0x26,0x21,0x2e};
    for (unsigned i = 0; i < 11; ++i) {
        const unsigned key = i == 10 ? 0x6e : 0x60+i;
        assert(ShortcutKey(off[i], scans[i], false) == key);
        assert(ShortcutKey(key, scans[i], false) == key);
        assert(ShortcutKey(off[i], scans[i], true) == off[i]);
    }
    ShortcutPress shortcut;
    bool toggle = false;
    const unsigned num8 = 0x68, num8Hid = 96;
    assert(shortcut.Filter(num8Hid,true,ShortcutKey(0x26,0x48,false),0,num8,true,toggle) && toggle);
    assert(shortcut.Filter(num8Hid,true,num8,0,num8,true,toggle) && !toggle);
    assert(shortcut.Filter(num8Hid,false,num8,4,0,false,toggle) && !toggle);
    assert(shortcut.Filter(num8Hid,true,num8,0,num8,true,toggle) && toggle);
    assert(shortcut.Filter(num8Hid,false,num8,0,num8,true,toggle));
    // Navigation Up is distinct; setting a shortcut while its key is held
    // must not let a repeat become a new press.
    assert(!shortcut.Filter(82,true,0x26,0,num8,true,toggle));
    assert(!shortcut.Filter(82,false,0x26,0,num8,true,toggle));
    assert(!shortcut.Filter(num8Hid,true,num8,0,num8,false,toggle));
    assert(!shortcut.Filter(num8Hid,true,num8,0,num8,true,toggle));
    assert(!shortcut.Filter(num8Hid,false,num8,0,num8,true,toggle));
    // Toggle is committed before the next W down; no UI-message processing is
    // needed. An already passed W still gets its up when blocking changes.
    PressRoutes gameplay;
    bool blocking = false;
    assert(shortcut.Filter(num8Hid,true,num8,0,num8,true,toggle) && toggle);
    if (toggle) blocking = !blocking;
    assert(gameplay.Filter(26,true,blocking));
    assert(gameplay.Filter(26,false,blocking));
    assert(!gameplay.HasHeld());
    assert(!gameplay.Filter(26,true,false));
    assert(!gameplay.Filter(26,false,true));
    for (unsigned key = 0; key < 256; ++key)
        assert(IsAltOrTab(key) == (key == 43 || key == 226 || key == 230));
    assert(ValidShortcut(0));
    assert(ValidShortcut((6u << 8) | 0x77));
    assert(!ValidShortcut((16u << 8) | 0x77));
    assert(!ValidShortcut(16)); assert(!ValidShortcut(162));
    PressRoutes routes;
    assert(!routes.Filter(4, false, true));
    assert(!routes.Filter(0, true, true));
    assert(!routes.Filter(9999, true, true));
    assert(!routes.HasHeld());
    assert(routes.Filter(4, true, true));
    assert(routes.Filter(4, true, false));
    assert(routes.Filter(4, false, false));
    assert(!routes.HasHeld());
    assert(!routes.Filter(4, true, false));
    assert(!routes.Filter(4, true, true));
    assert(!routes.Filter(4, false, true));
    assert(!routes.HasHeld());
    routes.SeedPassed(4); routes.SeedPassed(4);
    assert(!routes.Filter(4, true, true));
    assert(routes.Filter(5, true, true));
    assert(!routes.Filter(4, false, true));
    assert(routes.HasHeld());
    assert(routes.Filter(5, false, false));
    assert(!routes.HasHeld());
    routes.SeedPassed(4); routes.Reset();
    assert(!routes.HasHeld());
}
