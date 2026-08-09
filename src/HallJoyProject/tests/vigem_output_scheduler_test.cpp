#include <cassert>
#include <cstdint>
#include "../HallJoy/vigem_output_scheduler.h"

int main()
{
    using Decision = VigemOutputScheduler::Decision;
    VigemOutputScheduler scheduler;
    scheduler.Configure(1000);

    // First changed report is immediate.
    assert(scheduler.Evaluate(true, 10000) == Decision::SendNow);
    scheduler.MarkSent(10000);

    // Bursts are coalesced to one fixed deadline, not pushed out by each sample.
    assert(scheduler.Evaluate(true, 10100) == Decision::DeferUntilDeadline);
    assert(scheduler.DueTick() == 11000);
    assert(scheduler.Evaluate(true, 10999) == Decision::DeferUntilDeadline);
    assert(scheduler.DueTick() == 11000);
    assert(scheduler.Evaluate(true, 11000) == Decision::SendNow);
    scheduler.MarkSent(11000);

    // If the latest state returns to the last sent report, pending output clears.
    assert(scheduler.Evaluate(true, 11100) == Decision::DeferUntilDeadline);
    assert(scheduler.HasDeferred());
    assert(scheduler.Evaluate(false, 11200) == Decision::NoChange);
    assert(!scheduler.HasDeferred());

    // After an idle interval the next change is immediate again.
    assert(scheduler.Evaluate(true, 12000) == Decision::SendNow);
    scheduler.MarkSent(12000);

    // Reset removes all pending/valid state.
    scheduler.Reset();
    assert(!scheduler.HasDeferred());
    assert(scheduler.Evaluate(true, 12001) == Decision::SendNow);
    return 0;
}
