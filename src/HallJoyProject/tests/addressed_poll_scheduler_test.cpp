#include "addressed_poll_scheduler.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{
constexpr std::size_t kCount = 82;
std::array<addressed::PollKeyConfig, kCount> MakeKeys()
{
    std::array<addressed::PollKeyConfig, kCount> keys{};
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        keys[i].keyId = static_cast<std::uint8_t>(i + 1);
        keys[i].hidUsage = static_cast<std::uint16_t>(i + 1);
    }
    return keys;
}

struct Result
{
    std::array<std::uint64_t, kCount> hits{};
    std::array<std::uint64_t, kCount> maxGapUs{};
};

Result Simulate(std::size_t boundCount, std::size_t activeBegin, std::size_t activeCount,
                std::uint64_t durationUs = 3000000, std::uint64_t packetUs = 1170)
{
    const auto keys = MakeKeys();
    addressed::PollScheduler scheduler(keys.data(), keys.size());
    scheduler.Reset(0);
    for (std::size_t i = 0; i < boundCount; ++i) scheduler.SetBound(keys[i].hidUsage, true);

    Result result{};
    std::array<std::uint64_t, kCount> last{};
    for (std::uint64_t now = 0; now < durationUs; now += packetUs)
    {
        const auto plan = scheduler.BuildPlan(now);
        assert(plan.count > 0 && plan.count <= 9);
        std::array<bool, 256> seen{};
        for (std::size_t p = 0; p < plan.count; ++p)
        {
            assert(!seen[plan.keyIds[p]]);
            seen[plan.keyIds[p]] = true;
            const std::size_t index = static_cast<std::size_t>(plan.keyIds[p] - 1);
            assert(index < kCount);
            if (last[index] != 0) result.maxGapUs[index] = std::max(result.maxGapUs[index], now - last[index]);
            last[index] = now;
            ++result.hits[index];
            const bool active = index >= activeBegin && index < activeBegin + activeCount;
            scheduler.OnSample(plan.keyIds[p], static_cast<std::uint16_t>(10000 - (active ? 4000 : 0)),
                               static_cast<std::uint16_t>(active ? 500 : 0), now);
        }
    }
    return result;
}
}

int main()
{
    // More than one packet of binds: every bound key remains serviced and the
    // background matrix still advances.
    {
        const auto r = Simulate(18, 40, 0);
        for (std::size_t i = 0; i < 18; ++i)
        {
            assert(r.hits[i] > 700);
            assert(r.maxGapUs[i] <= 15000);
        }
        for (std::size_t i = 18; i < kCount; ++i)
        {
            assert(r.hits[i] > 45);
            assert(r.maxGapUs[i] <= 50000);
        }
    }

    // Active, unbound keys get a live share without displacing either binds or
    // the complete background sweep.
    {
        const auto r = Simulate(12, 30, 12);
        for (std::size_t i = 0; i < 12; ++i) assert(r.hits[i] > 700);
        for (std::size_t i = 30; i < 42; ++i)
        {
            assert(r.hits[i] > 150);
            assert(r.maxGapUs[i] <= 15000);
        }
        for (std::size_t i = 42; i < kCount; ++i)
        {
            assert(r.hits[i] > 40);
            assert(r.maxGapUs[i] <= 50000);
        }
    }

    // Small bound sets fit in every packet, while background still receives at
    // least the intended ~20 Hz at an 850 pps transport rate.
    {
        const auto r = Simulate(4, 40, 0);
        for (std::size_t i = 0; i < 4; ++i) assert(r.hits[i] > 2400);
        for (std::size_t i = 4; i < kCount; ++i) assert(r.hits[i] > 45);
    }

    // Under an extreme mixed load, each bound key still receives more updates
    // than each unbound active key, preserving the declared class priority.
    {
        const auto r = Simulate(30, 40, 20);
        std::uint64_t boundTotal = 0;
        std::uint64_t activeTotal = 0;
        for (std::size_t i = 0; i < 30; ++i) boundTotal += r.hits[i];
        for (std::size_t i = 40; i < 60; ++i) activeTotal += r.hits[i];
        assert(boundTotal / 30 > activeTotal / 20);
    }


    // Full-matrix chord stress: every physical key is non-zero at once. The
    // scheduler must stay bounded to nine unique slots and continue servicing
    // the complete matrix without starvation or index overflow.
    {
        const auto r = Simulate(24, 0, kCount, 5000000);
        for (std::size_t i = 0; i < kCount; ++i)
        {
            assert(r.hits[i] > 100);
            assert(r.maxGapUs[i] <= 25000);
        }
    }

    std::cout << "addressed scheduler tests passed\n";
    return 0;
}
