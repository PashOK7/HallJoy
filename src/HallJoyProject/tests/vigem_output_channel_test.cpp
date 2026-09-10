#include <atomic>
#include <cassert>
#include <cstdint>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "../HallJoy/vigem_output_channel.h"

namespace
{
using namespace halljoy::vigem_output;

bool ClaimForHeldRead(std::uint32_t& state)
{
    const std::uint32_t ready = static_cast<std::uint32_t>(SlotState::Empty);
    const std::uint32_t reading = static_cast<std::uint32_t>(SlotState::Reading);
#if defined(_WIN32)
    return static_cast<std::uint32_t>(InterlockedCompareExchange(
        reinterpret_cast<volatile LONG*>(&state),
        static_cast<LONG>(reading),
        static_cast<LONG>(ready))) == ready;
#else
    std::uint32_t expected = ready;
    return std::atomic_ref<std::uint32_t>(state).compare_exchange_strong(expected, reading);
#endif
}

void ReleaseHeldRead(std::uint32_t& state)
{
#if defined(_WIN32)
    InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&state),
        static_cast<LONG>(SlotState::Empty));
#else
    std::atomic_ref<std::uint32_t>(state).store(
        static_cast<std::uint32_t>(SlotState::Empty), std::memory_order_release);
#endif
}

XusbReportV1 MakeReport(std::uint64_t value, std::uint32_t pad)
{
    XusbReportV1 report{};
    report.buttons = static_cast<std::uint16_t>(value);
    report.leftTrigger = static_cast<std::uint8_t>(value >> 8u);
    report.rightTrigger = static_cast<std::uint8_t>(pad);
    report.thumbLX = static_cast<std::int16_t>(value);
    report.thumbLY = static_cast<std::int16_t>(~value);
    report.thumbRX = static_cast<std::int16_t>(value ^ (pad * 0x1111u));
    report.thumbRY = static_cast<std::int16_t>((value >> 1u) ^ (pad * 0x2222u));
    return report;
}

void AssertReportMatches(const SnapshotPayloadV1& snapshot)
{
    for (std::uint32_t pad = 0; pad < snapshot.padCount; ++pad)
    {
        const XusbReportV1 expected = MakeReport(snapshot.publicationSequence, pad);
        const XusbReportV1& actual = snapshot.reports[pad];
        assert(actual.buttons == expected.buttons);
        assert(actual.leftTrigger == expected.leftTrigger);
        assert(actual.rightTrigger == expected.rightTrigger);
        assert(actual.thumbLX == expected.thumbLX);
        assert(actual.thumbLY == expected.thumbLY);
        assert(actual.thumbRX == expected.thumbRX);
        assert(actual.thumbRY == expected.thumbRY);
    }
}
}

int main()
{
    using namespace halljoy::vigem_output;

    SharedStateV1 shared{};
    InitializeSharedState(shared, 1234u, 0x1122334455667788ull);
    assert(ValidateSharedState(shared, 1234u, 0x1122334455667788ull));
    assert(!ValidateSharedState(shared, 1235u, 0x1122334455667788ull));
    assert(!ValidateSharedState(shared, 1234u, 0u));
    const std::uint32_t savedVersion = shared.version;
    shared.version += 1u;
    assert(!ValidateSharedState(shared, 1234u, 0x1122334455667788ull));
    shared.version = savedVersion;

    XusbReportV1 reports[kMaxPads]{};
    assert(TryPublishSnapshot(shared, reports, 4u, 0x0fu, 1u) == PublishResult::Inactive);
    assert(!BeginOutputGeneration(shared, 0u, 1u));
    assert(!BeginOutputGeneration(shared, 7u, 0u));
    assert(!BeginOutputGeneration(shared, 7u, kMaxPads + 1u));
    assert(BeginOutputGeneration(shared, 7u, kMaxPads));
    assert(PublicationQuiescent(shared));
    ProducerLeaseViewV1 producerLease{};
    assert(!ReadProducerLease(shared, 8u, producerLease));
    assert(PublishProducerLease(shared, 100u));
    assert(ReadProducerLease(shared, 7u, producerLease));
    assert(producerLease.generation == 7u);
    assert(producerLease.sequence != 0u);
    assert(producerLease.tickMs == 100u);
    assert(ReadProducerLeaseState(shared, 7u, 299u, 1u, nullptr) ==
        ProducerLeaseState::Fresh);
    assert(ReadProducerLeaseState(shared, 7u, 301u, 1u, nullptr) ==
        ProducerLeaseState::Stalled);
    OutputConfigurationV1 configuration{};
    assert(ReadOutputConfiguration(shared, 7u, configuration));
    assert(configuration.generation == 7u);
    assert(configuration.padCount == kMaxPads);
    assert(!ReadOutputConfiguration(shared, 8u, configuration));
    assert(!BeginOutputGeneration(shared, 8u, 1u));
    assert(TryPublishSnapshot(shared, nullptr, 1u, 1u, 1u) == PublishResult::InvalidPayload);
    assert(TryPublishSnapshot(shared, reports, 5u, 0x1fu, 1u) == PublishResult::InvalidPayload);
    assert(TryPublishSnapshot(shared, reports, 2u, 0x04u, 1u) == PublishResult::InvalidPayload);

    // When the consumer is delayed, each new publication remains bounded and
    // reclaims only an unowned Ready slot. The newest complete value survives.
    for (std::uint64_t i = 1u; i <= 10u; ++i)
    {
        for (std::uint32_t pad = 0; pad < kMaxPads; ++pad)
            reports[pad] = MakeReport(i, pad);
        std::uint64_t published = 0u;
        assert(TryPublishSnapshot(shared, reports, kMaxPads, 0x0fu, i * 10u, &published) ==
            PublishResult::Published);
        assert(published == i);
    }

    SnapshotPayloadV1 snapshot{};
    assert(TryConsumeNewestSnapshot(shared, 7u, 0u, &snapshot) == ConsumeResult::Updated);
    assert(snapshot.outputGeneration == 7u);
    assert(snapshot.publicationSequence == 10u);
    assert(snapshot.timestampUs == 100u);
    AssertReportMatches(snapshot);
    assert(TryConsumeNewestSnapshot(shared, 7u, 10u, &snapshot) == ConsumeResult::NoUpdate);

    // A stale child generation can drain nothing belonging to the replacement.
    assert(DisableOutputGeneration(shared, 7u));
    assert(PublicationQuiescent(shared));
    assert(!DisableOutputGeneration(shared, 7u));
    assert(BeginOutputGeneration(shared, 8u, 2u));
    assert(PublishProducerLease(shared, 200u));
    for (std::uint32_t pad = 0; pad < kMaxPads; ++pad)
        reports[pad] = MakeReport(11u, pad);
    assert(TryPublishSnapshot(shared, reports, kMaxPads, 0x0fu, 110u) ==
        PublishResult::Inactive);
    assert(TryPublishSnapshot(shared, reports, 2u, 0x03u, 110u) ==
        PublishResult::Published);
    assert(TryConsumeNewestSnapshot(shared, 7u, 10u, &snapshot) == ConsumeResult::NoUpdate);

    // Hold one slot as Reading. The producer never waits and continues by
    // using/reclaiming the other two slots.
    SnapshotSlotV1& held = shared.slots[0];
    assert(ClaimForHeldRead(held.state));
    for (std::uint64_t i = 12u; i <= 1011u; ++i)
    {
        for (std::uint32_t pad = 0; pad < kMaxPads; ++pad)
            reports[pad] = MakeReport(i, pad);
        assert(TryPublishSnapshot(shared, reports, 2u, 0x03u, i) == PublishResult::Published);
    }
    ReleaseHeldRead(held.state);
    assert(TryConsumeNewestSnapshot(shared, 8u, 10u, &snapshot) == ConsumeResult::Updated);
    assert(snapshot.publicationSequence == 1011u);
    AssertReportMatches(snapshot);

    // Production-linked contention stress: the report fields must always come
    // from one complete publication and consumption is monotonic.
    constexpr std::uint64_t kStressCount = 100000u;
    std::atomic<bool> producerDone{ false };
    std::thread producer([&]() {
        XusbReportV1 local[kMaxPads]{};
        std::uint64_t successful = 0u;
        while (successful < kStressCount)
        {
            const std::uint64_t expectedPublication = 1011u + successful + 1u;
            for (std::uint32_t pad = 0; pad < kMaxPads; ++pad)
                local[pad] = MakeReport(expectedPublication, pad);
            std::uint64_t published = 0u;
            const PublishResult result = TryPublishSnapshot(
                shared, local, 2u, 0x03u, successful + 1u, &published);
            if (result == PublishResult::Contended)
            {
                // The production call already returned without waiting. The
                // harness retries so it can validate 100,000 accepted values.
                std::this_thread::yield();
                continue;
            }
            assert(result == PublishResult::Published);
            assert(published == expectedPublication);
            ++successful;
        }
        producerDone.store(true, std::memory_order_release);
    });

    std::uint64_t observed = 1011u;
    const std::uint64_t finalPublication = 1011u + kStressCount;
    while (!producerDone.load(std::memory_order_acquire) || observed < finalPublication)
    {
        SnapshotPayloadV1 current{};
        if (TryConsumeNewestSnapshot(shared, 8u, observed, &current) == ConsumeResult::Updated)
        {
            assert(current.publicationSequence > observed);
            AssertReportMatches(current);
            observed = current.publicationSequence;
        }
        else
        {
            std::this_thread::yield();
        }
    }
    producer.join();
    assert(observed == finalPublication);
    assert(DisableOutputGeneration(shared, 8u));
    assert(PublicationQuiescent(shared));
    assert(ActiveOutputGeneration(shared) == 0u);
}
