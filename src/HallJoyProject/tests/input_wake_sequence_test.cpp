#include "../HallJoy/input_wake_sequence.h"

#include <cassert>

int main()
{
    halljoy::realtime::InputWakeSequence sequence;
    std::uint64_t consumed = sequence.Consumed();
    assert(!sequence.Pending(consumed));

    // An edge before the worker begins waiting must remain pending.
    const std::uint64_t beforeStart = sequence.Notify();
    assert(beforeStart == 1u);
    assert(sequence.Pending(consumed));

    // Bursts may coalesce into one tick, but the latest sequence is consumed.
    (void)sequence.Notify();
    const std::uint64_t burst = sequence.Notify();
    sequence.MarkConsumed(burst);
    consumed = burst;
    assert(!sequence.Pending(consumed));

    // If another edge arrives after the worker's observation but before it
    // marks that observation consumed, the newer edge remains pending.
    const std::uint64_t observed = sequence.Observe();
    (void)sequence.Notify();
    sequence.MarkConsumed(observed);
    consumed = observed;
    assert(sequence.Pending(consumed));

    sequence.MarkConsumed(sequence.Observe());
    assert(!sequence.Pending(sequence.Consumed()));
    return 0;
}
