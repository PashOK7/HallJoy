#include "vigem_output_producer_lease.h"

#include <cassert>

using namespace halljoy::vigem_output;

int main()
{
    constexpr std::uint64_t generation = 41u;
    ProducerLeaseViewV1 lease{};

    // Startup has a bounded grace period; a missing first tick is not called a
    // stall immediately, but must become one after the documented deadline.
    assert(EvaluateProducerLease(1000u, generation, lease, 1000u) ==
        ProducerLeaseState::Grace);
    assert(EvaluateProducerLease(1000u + kProducerLeaseDeadlineMs, generation,
        lease, 1000u) == ProducerLeaseState::Grace);
    assert(EvaluateProducerLease(1001u + kProducerLeaseDeadlineMs, generation,
        lease, 1000u) == ProducerLeaseState::Stalled);

    lease = { generation, 7u, 5000u };
    assert(EvaluateProducerLease(5000u + kProducerLeaseDeadlineMs, generation,
        lease, 1000u) == ProducerLeaseState::Fresh);
    assert(EvaluateProducerLease(5001u + kProducerLeaseDeadlineMs, generation,
        lease, 1000u) == ProducerLeaseState::Stalled);

    // A held key can publish identical reports indefinitely.  Its advancing
    // calculation lease remains fresh without looking at report contents.
    lease.sequence = 8u;
    lease.tickMs = 5200u;
    assert(EvaluateProducerLease(5350u, generation, lease, 1000u) ==
        ProducerLeaseState::Fresh);

    lease.generation = generation + 1u;
    assert(EvaluateProducerLease(5350u, generation, lease, 1000u) ==
        ProducerLeaseState::WrongGeneration);

    return 0;
}
