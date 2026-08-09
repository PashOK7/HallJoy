#include "aula_win60he_diagnostic_metrics.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main()
{
    using namespace aula_win60he;
    KeyMap map{};
    TravelMatrix travel{};
    for (std::size_t index = 0; index < 12; ++index)
        map[index / kColumns][index % kColumns] = static_cast<std::uint8_t>(4u + index);

    DiagnosticMetrics metrics;
    metrics.Begin(1'000'000u);
    auto first = metrics.Observe(map, travel, false, 1'002'000u, 900u);
    assert(first.activeCount == 0);
    assert(!first.firstNonzero);

    travel[0][0] = 500;
    auto one = metrics.Observe(map, travel, true, 1'004'000u, 1'100u);
    assert(one.activeCount == 1);
    assert(one.firstNonzero && one.newActiveMaximum && !one.firstTenPlus);
    assert(one.active[0].hid == 4 && one.active[0].travelUm == 500);

    for (std::size_t index = 0; index < 10; ++index)
        travel[index / kColumns][index % kColumns] = static_cast<std::uint16_t>(100u + index * 100u);
    auto ten = metrics.Observe(map, travel, true, 1'006'000u, 2'500u);
    assert(ten.activeCount == 10);
    assert(ten.firstTenPlus && ten.newActiveMaximum);
    assert(ten.minimumPositiveUm == 100 && ten.maximumUm == 1000);
    assert(metrics.MaximumActiveKeys() == 10);
    assert(metrics.ObservedHids() == 10);

    auto eleven = metrics.Observe(map, travel, false, 1'008'000u, 4'500u);
    assert(!eleven.firstTenPlus && !eleven.newActiveMaximum);
    travel = TravelMatrix{};
    auto released = metrics.Observe(map, travel, true, 1'010'000u, 8'500u);
    assert(released.activeCount == 0);

    const auto lifetime = metrics.Lifetime(1'010'000u);
    assert(lifetime.updates == 5);
    assert(lifetime.changedUpdates == 3);
    assert(lifetime.nonzeroUpdates == 3);
    assert(lifetime.pressTransitions == 1);
    assert(lifetime.releaseToZeroTransitions == 1);
    assert(lifetime.minimumIntervalUs == 2000);
    assert(lifetime.maximumIntervalUs == 2000);
    assert(lifetime.AverageIntervalUs() == 2000);
    assert(lifetime.minimumTransactionUs == 900);
    assert(lifetime.maximumTransactionUs == 8500);
    assert(lifetime.transactionBuckets[0] == 2);
    assert(lifetime.transactionBuckets[1] == 1);
    assert(lifetime.transactionBuckets[2] == 1);
    assert(lifetime.transactionBuckets[3] == 1);
    assert(metrics.MaximumByHid()[4] == 500);
    assert(metrics.MaximumByHid()[13] == 1000);

    assert(!metrics.WindowReady(5'999'999u));
    assert(metrics.WindowReady(6'000'000u));
    const auto window = metrics.TakeWindow(6'000'000u);
    assert(window.elapsedUs == 5'000'000u);
    assert(window.RateMilliHz() == 1000u);
    const auto empty = metrics.TakeWindow(11'000'000u);
    assert(empty.updates == 0 && empty.currentActiveKeys == 0);

    std::cout << "aula win60he diagnostic metrics tests passed\n";
    return 0;
}
