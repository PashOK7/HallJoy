#include "sparklink_row_freshness.h"

#include <array>
#include <cassert>
#include <cstdint>

using halljoy::sparklink::IsRowFresh;

namespace
{
constexpr std::uint64_t kDeadline = 2160u;

std::uint16_t AggregateFresh(const std::array<std::uint16_t, 2>& values,
    const std::array<std::uint64_t, 2>& lastOk, std::uint64_t now)
{
    std::uint16_t result = 0;
    for (std::size_t row = 0; row < values.size(); ++row)
        if (IsRowFresh(now, lastOk[row], kDeadline))
            result = values[row] > result ? values[row] : result;
    return result;
}
}

int main()
{
    assert(!IsRowFresh(100u, 0u, kDeadline)); // never seen
    assert(IsRowFresh(2260u, 100u, kDeadline));
    assert(!IsRowFresh(2261u, 100u, kDeadline));

    // Row A stays live while B's old nonzero value expires.
    assert(AggregateFresh({ 700u, 900u }, { 3000u, 2900u }, 3000u) == 900u);
    assert(AggregateFresh({ 700u, 900u }, { 5200u, 2900u }, 5200u) == 700u);
    assert(AggregateFresh({ 0u, 900u }, { 0u, 100u }, 3000u) == 0u);
    return 0;
}
