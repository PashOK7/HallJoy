#pragma once
#include <algorithm>
#include <cstdint>

namespace halljoy::remap_hint {
struct Frame { float progress; float opacity; bool finished; };
inline float Ease(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    // Quintic smoothstep: velocity AND acceleration meet the holds at zero.
    return static_cast<float>(t * t * t * (t * (t * 6.0 - 15.0) + 10.0));
}
inline Frame FrameAt(std::uint64_t elapsed)
{
    if (elapsed >= 1800) return {1.0f, 0.0f, true};
    const float ms = static_cast<float>(elapsed);
    const float t = std::clamp((ms - 180.0f) / 1000.0f, 0.0f, 1.0f);
    return {Ease(t),
        std::min(Ease(ms / 150.0f), Ease((1800.0f - ms) / 300.0f)), false};
}
}
