#include "../../../third_party/UniversalAnalogPluginFixed/halljoy_uap_poll_pacing.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

int main()
{
    using halljoy::uap::PollPacingPolicy;

    PollPacingPolicy pacing;
    pacing.BeginCycle(10000);
    assert(pacing.CompleteCycle(10050, true) == 950);

    pacing.BeginCycle(20000);
    assert(pacing.CompleteCycle(21500, true) == 0);

    pacing.BeginCycle(30000);
    assert(pacing.CompleteCycle(30100, false) == 2000);
    assert(pacing.failure_streak() == 1);
    pacing.BeginCycle(40000);
    assert(pacing.CompleteCycle(40100, false) == 4000);
    pacing.BeginCycle(50000);
    assert(pacing.CompleteCycle(50100, false) == 8000);
    pacing.BeginCycle(60000);
    assert(pacing.CompleteCycle(60100, false) == 16000);
    pacing.BeginCycle(70000);
    assert(pacing.CompleteCycle(70100, false) == 32000);
    pacing.BeginCycle(80000);
    assert(pacing.CompleteCycle(80100, false) == 64000);
    pacing.BeginCycle(90000);
    assert(pacing.CompleteCycle(90100, false) == 64000);

    pacing.BeginCycle(100000);
    assert(pacing.CompleteCycle(100050, true) == 950);
    assert(pacing.failure_streak() == 0);
    pacing.BeginCycle(110000);
    assert(pacing.CompleteCycle(110050, false) == 2000);

    PollPacingPolicy disabled(0);
    disabled.BeginCycle(0);
    assert(disabled.CompleteCycle(50, true) == 0);
    assert(disabled.CompleteCycle(100, false) == 0);

    constexpr std::uint64_t window_us = 1000000;
    constexpr std::uint32_t fast_transaction_us = 50;
    std::uint64_t modeled_clock_us = 0;
    std::uint64_t paced_calls = 0;
    std::uint64_t paced_busy_us = 0;
    PollPacingPolicy model;
    while (modeled_clock_us < window_us)
    {
        model.BeginCycle(modeled_clock_us);
        modeled_clock_us += fast_transaction_us;
        paced_busy_us += fast_transaction_us;
        const auto wait_us = model.CompleteCycle(modeled_clock_us, true);
        modeled_clock_us += wait_us;
        ++paced_calls;
    }
    const std::uint64_t unpaced_calls = window_us / fast_transaction_us;
    const std::uint64_t unpaced_busy_us = unpaced_calls * fast_transaction_us;
    assert(paced_calls == 1000);
    assert(unpaced_calls == 20000);
    assert(paced_busy_us == 50000);
    assert(unpaced_busy_us == 1000000);

    std::uint64_t deadline_property_cases = 0;
    for (const std::uint32_t target_us : { 1u, 17u, 1000u, 4096u })
    {
        for (std::uint32_t work_us = 0; work_us <= target_us * 2u; ++work_us)
        {
            PollPacingPolicy property(target_us);
            property.BeginCycle(1000000);
            const auto wait_us = property.CompleteCycle(1000000 + work_us, true);
            assert(wait_us <= target_us);
            assert(static_cast<std::uint64_t>(work_us) + wait_us ==
                (work_us < target_us ? target_us : work_us));
            ++deadline_property_cases;
        }
    }

    PollPacingPolicy overflow_safe(1000);
    overflow_safe.BeginCycle(std::numeric_limits<std::uint64_t>::max() - 500);
    assert(overflow_safe.CompleteCycle(
        std::numeric_limits<std::uint64_t>::max() - 250, true) == 250);

    std::cout << "UAP_POLL_PACING_TEST=PASS paced_calls=" << paced_calls
              << " unpaced_calls=" << unpaced_calls
              << " paced_modeled_busy_us=" << paced_busy_us
              << " unpaced_modeled_busy_us=" << unpaced_busy_us
              << " deadline_properties=" << deadline_property_cases
              << " overflow_safe=1\n";
    return 0;
}
