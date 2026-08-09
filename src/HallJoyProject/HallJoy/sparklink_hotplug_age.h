#pragma once

#include <cstdint>

namespace halljoy::sparklink
{
constexpr std::uint64_t FreshnessAgeMs(
    std::uint64_t observedNowMs,
    std::uint64_t lastPacketMs) noexcept
{
    if (lastPacketMs == 0 || observedNowMs < lastPacketMs)
        return 0;
    return observedNowMs - lastPacketMs;
}

constexpr bool IsPacketStale(
    std::uint64_t observedNowMs,
    std::uint64_t lastPacketMs,
    std::uint64_t staleAfterMs) noexcept
{
    return lastPacketMs != 0 &&
        FreshnessAgeMs(observedNowMs, lastPacketMs) > staleAfterMs;
}
}
