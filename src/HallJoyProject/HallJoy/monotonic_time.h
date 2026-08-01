#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace halljoy::monotonic_time
{
constexpr std::uint64_t SaturatingAgeMs(
    std::uint64_t observedNowMs,
    std::uint64_t publishedMs) noexcept
{
    if (publishedMs == 0 || observedNowMs < publishedMs)
        return 0;
    return observedNowMs - publishedMs;
}

constexpr bool IsStale(
    std::uint64_t observedNowMs,
    std::uint64_t publishedMs,
    std::uint64_t staleAfterMs) noexcept
{
    return publishedMs != 0 &&
        SaturatingAgeMs(observedNowMs, publishedMs) > staleAfterMs;
}

constexpr std::uint32_t RemainingTimeoutMs(
    std::uint64_t observedNowMs,
    std::uint64_t deadlineMs,
    std::uint32_t maximumMs = std::numeric_limits<std::uint32_t>::max()) noexcept
{
    if (observedNowMs >= deadlineMs)
        return 0;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        deadlineMs - observedNowMs,
        maximumMs));
}
}
