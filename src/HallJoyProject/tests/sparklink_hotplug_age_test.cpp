#include "sparklink_hotplug_age.h"

#include <cassert>
#include <cstdint>
#include <limits>

using halljoy::sparklink::FreshnessAgeMs;
using halljoy::sparklink::IsPacketStale;

static_assert(FreshnessAgeMs(100, 0) == 0);
static_assert(FreshnessAgeMs(100, 101) == 0);
static_assert(FreshnessAgeMs(101, 100) == 1);
static_assert(!IsPacketStale(100, 101, 1800));
static_assert(!IsPacketStale(1900, 100, 1800));
static_assert(IsPacketStale(1901, 100, 1800));

int main()
{
    constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    assert(FreshnessAgeMs(maximum - 15, maximum) == 0);
    assert(!IsPacketStale(maximum - 15, maximum, 1800));
    assert(FreshnessAgeMs(maximum, maximum - 15) == 15);
    assert(!IsPacketStale(maximum, maximum - 15, 1800));
    assert(IsPacketStale(2001, 100, 1800));
    return 0;
}
