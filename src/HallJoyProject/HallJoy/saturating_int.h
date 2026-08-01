#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace halljoy::arithmetic
{
constexpr int SaturatingAddInt(
    int left,
    int right,
    int minimum = std::numeric_limits<int>::min(),
    int maximum = std::numeric_limits<int>::max()) noexcept
{
    const std::int64_t sum = static_cast<std::int64_t>(left) +
        static_cast<std::int64_t>(right);
    return static_cast<int>(std::clamp<std::int64_t>(sum, minimum, maximum));
}
}
