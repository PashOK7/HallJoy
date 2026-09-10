#pragma once

#include "sparklink_hotplug_age.h"

#include <cstdint>

namespace halljoy::sparklink
{

constexpr bool IsRowFresh(std::uint64_t nowMs, std::uint64_t lastOkMs,
    std::uint64_t deadlineMs) noexcept
{
    return lastOkMs != 0u && !IsPacketStale(nowMs, lastOkMs, deadlineMs);
}

} // namespace halljoy::sparklink
