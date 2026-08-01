#include "monotonic_time.h"
#include "saturating_int.h"

#include <cassert>
#include <cstdint>
#include <limits>

using halljoy::arithmetic::SaturatingAddInt;
using halljoy::monotonic_time::IsStale;
using halljoy::monotonic_time::RemainingTimeoutMs;
using halljoy::monotonic_time::SaturatingAgeMs;

static_assert(SaturatingAgeMs(100, 0) == 0);
static_assert(SaturatingAgeMs(100, 101) == 0);
static_assert(SaturatingAgeMs(101, 100) == 1);
static_assert(!IsStale(100, 101, 1800));
static_assert(IsStale(1901, 100, 1800));

static_assert(RemainingTimeoutMs(100, 100) == 0);
static_assert(RemainingTimeoutMs(101, 100) == 0);
static_assert(RemainingTimeoutMs(99, 100) == 1);
static_assert(RemainingTimeoutMs(0, 1000, 20) == 20);

static_assert(SaturatingAddInt(100, 20, -200, 200) == 120);
static_assert(SaturatingAddInt(190, 20, -200, 200) == 200);
static_assert(SaturatingAddInt(-190, -20, -200, 200) == -200);

int main()
{
    constexpr auto u64max = std::numeric_limits<std::uint64_t>::max();
    constexpr auto imax = std::numeric_limits<int>::max();
    constexpr auto imin = std::numeric_limits<int>::min();

    assert(SaturatingAgeMs(u64max - 15, u64max) == 0);
    assert(!IsStale(u64max - 15, u64max, 1800));
    assert(RemainingTimeoutMs(u64max, u64max) == 0);
    assert(RemainingTimeoutMs(u64max - 1, u64max) == 1);

    assert(SaturatingAddInt(32767, imax, -32768, 32767) == 32767);
    assert(SaturatingAddInt(-32768, imin, -32768, 32767) == -32768);
    assert(SaturatingAddInt(-100, 250, -32768, 32767) == 150);
    return 0;
}
