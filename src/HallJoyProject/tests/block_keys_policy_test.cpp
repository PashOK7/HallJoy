#include "block_keys_policy.h"
#include <cassert>
int main() {
    using namespace halljoy::block_keys;
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
