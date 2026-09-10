#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace halljoy::window_placement {
struct Rect { int x, y, w, h; };
inline Rect Fit(Rect r, const std::vector<Rect>& workAreas)
{
    if (workAreas.empty()) return r;
    const Rect* best = nullptr;
    std::int64_t bestArea = -1, bestDistance = INT64_MAX;
    for (const auto& a : workAreas) {
        if (a.w <= 0 || a.h <= 0) continue;
        using I = std::int64_t;
        const I overlapW = std::max<I>(0, std::min<I>(I(r.x) + r.w, I(a.x) + a.w) - std::max(r.x, a.x));
        const I overlapH = std::max<I>(0, std::min<I>(I(r.y) + r.h, I(a.y) + a.h) - std::max(r.y, a.y));
        const I dx = std::max<I>({0, I(a.x) - (I(r.x) + r.w), I(r.x) - (I(a.x) + a.w)});
        const I dy = std::max<I>({0, I(a.y) - (I(r.y) + r.h), I(r.y) - (I(a.y) + a.h)});
        const I distance = dx + dy; // Overflow-safe nearest rectangle tie-break.
        if (overlapW * overlapH > bestArea || (overlapW * overlapH == bestArea && distance < bestDistance)) {
            best = &a; bestArea = overlapW * overlapH; bestDistance = distance;
        }
    }
    if (!best) return r;
    r.w = std::clamp(r.w, 1, best->w);
    r.h = std::clamp(r.h, 1, best->h);
    r.x = std::clamp(r.x, best->x, best->x + best->w - r.w);
    r.y = std::clamp(r.y, best->y, best->y + best->h - r.h);
    return r;
}
inline int ScaleSize(int value, int oldDpi, int dpi)
{
    if (oldDpi < 48 || oldDpi > 768 || dpi < 48 || dpi > 768) return value;
    return static_cast<int>(std::clamp<std::int64_t>((std::int64_t(value) * dpi + oldDpi / 2) / oldDpi, 1, 10000));
}
}
