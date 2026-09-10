#pragma once

#include <cstdint>

namespace halljoy::vigem_output
{

// The realtime loop guarantees a heartbeat no slower than 20 ms.  Ten missed
// intervals leave room for ordinary scheduling jitter while bounding a frozen
// parent's retained XUSB state to one child wait (25 ms) beyond this deadline.
constexpr std::uint64_t kProducerLeaseDeadlineMs = 200u;

enum class ProducerLeaseState : std::uint8_t
{
    Fresh,
    Grace,
    Stalled,
    WrongGeneration,
};

struct ProducerLeaseViewV1
{
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint64_t tickMs = 0;
};

// `readyTickMs` is the child-side start of the initial no-producer grace
// period.  All operands use GetTickCount64's monotonic domain.  A clock value
// earlier than its anchor is treated conservatively as grace rather than as a
// wrapped enormous age.
constexpr ProducerLeaseState EvaluateProducerLease(
    std::uint64_t nowMs,
    std::uint64_t expectedGeneration,
    const ProducerLeaseViewV1& lease,
    std::uint64_t readyTickMs) noexcept
{
    if (expectedGeneration == 0u ||
        (lease.generation != 0u && lease.generation != expectedGeneration))
    {
        return ProducerLeaseState::WrongGeneration;
    }

    const bool hasCurrentLease = lease.generation == expectedGeneration &&
        lease.sequence != 0u;
    const std::uint64_t anchorMs = hasCurrentLease ? lease.tickMs : readyTickMs;
    if (nowMs < anchorMs || nowMs - anchorMs <= kProducerLeaseDeadlineMs)
        return hasCurrentLease ? ProducerLeaseState::Fresh : ProducerLeaseState::Grace;
    return ProducerLeaseState::Stalled;
}

} // namespace halljoy::vigem_output
