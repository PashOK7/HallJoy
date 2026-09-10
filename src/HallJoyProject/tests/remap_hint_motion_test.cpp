#include "remap_hint_motion.h"
#include <cassert>
#include <cstdint>
#include <iostream>
int main()
{
    using halljoy::remap_hint::FrameAt;
    assert(FrameAt(0).opacity == 0 && FrameAt(0).progress == 0);
    assert(FrameAt(150).opacity == 1 && FrameAt(180).progress == 0);
    assert(FrameAt(680).progress == 0.5f);
    assert(FrameAt(1180).progress == 1 && FrameAt(1500).opacity == 1);
    assert(FrameAt(1650).opacity == 0.5f);
    assert(FrameAt(280).progress < 0.01f); // Gentle initial acceleration.
    assert(FrameAt(1080).progress > 0.99f); // Matching deceleration.
    assert(FrameAt(181).progress < 0.00000002f);
    float previous = 0;
    for (std::uint64_t ms = 0; ms < 1800; ++ms) {
        const auto f = FrameAt(ms);
        assert(!f.finished && f.progress >= previous && f.progress <= 1);
        assert(f.opacity >= 0 && f.opacity <= 1);
        previous = f.progress;
    }
    for (auto ms : {std::uint64_t{1800}, std::uint64_t{10000}, UINT64_MAX}) {
        const auto f = FrameAt(ms);
        assert(f.finished && f.opacity == 0 && f.progress == 1);
    }
    assert(!FrameAt(0).finished); // Every new click may start a fresh sequence.
    std::cout << "REMAP_HINT_MOTION=PASS\n";
}
