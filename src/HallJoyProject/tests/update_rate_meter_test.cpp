#include "update_rate_meter.h"

#include <cassert>
#include <cstdio>

int main()
{
    using halljoy::UpdateRateMeter;
    UpdateRateMeter m;
    assert(m.CurrentHz10() == 0 && m.PeakHz10() == 0);
    // 1000 updates per second, sampled every 16 ms like the UI timer.
    std::uint64_t count = 0, now = 0;
    m.Observe(count, now);
    for (int i = 0; i < 200; ++i) { now += 16000; count += 16; m.Observe(count, now); }
    assert(m.CurrentHz10() >= 9900 && m.CurrentHz10() <= 10100);
    assert(m.PeakHz10() >= 9900 && m.PeakHz10() <= 10100);
    // Keys held still on a change-only stream: now drops to 0, the peak stays.
    for (int i = 0; i < 100; ++i) { now += 16000; m.Observe(count, now); }
    assert(m.CurrentHz10() == 0 && m.PeakHz10() >= 9900);
    // One 8000 Hz burst window among 199 at 1000 Hz is the top 1% and is excluded.
    m.Reset();count = 0;now = 0;m.Observe(count, now);
    for (int w = 0; w < 200; ++w) { now += 500000; count += (w == 100 ? 4000 : 500); m.Observe(count, now); }
    assert(m.PeakHz10() == 10000);
    // Irregular sampling: a 2 s gap still yields the true average rate.
    m.Reset();m.Observe(0, 0);m.Observe(2000, 2000000);
    assert(m.CurrentHz10() == 10000);
    // A restarted counter (new session) starts a fresh window instead of underflowing.
    m.Observe(10, 2100000);
    assert(m.CurrentHz10() == 0);
    m.Observe(510, 2600000);
    assert(m.CurrentHz10() == 10000);
    // Windows shorter than 0.5 s do not publish a rate.
    m.Reset();m.Observe(0, 0);m.Observe(100, 100000);
    assert(m.CurrentHz10() == 0 && m.PeakHz10() == 0);
    std::puts("UPDATE_RATE_METER=PASS");
    return 0;
}
