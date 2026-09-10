#include "vigem_output_channel.h"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <thread>

using namespace halljoy::vigem_output;

int main()
{
    SharedStateV1 shared{};
    InitializeSharedState(shared, 1234u, 0xA55AA55AA55AA55Aull);
    assert(BeginOutputGeneration(shared, 1u, kMaxPads));

    assert(ChildPublishStarting(shared, 1u, 4321u, 10u,
        kChildDiagnosticContainedBeforeEntry));
    ChildTelemetrySnapshotV1 telemetry{};
    assert(ReadChildTelemetry(shared, telemetry));
    assert(telemetry.childGeneration == 1u);
    assert(telemetry.childPid == 4321u);
    assert(telemetry.state == ChildState::Starting);
    assert((telemetry.telemetrySequence & 1u) == 0u);

    assert(!ChildPublishReady(shared, 2u, 4321u, 2u, 11u));
    assert(!ChildPublishReady(shared, 1u, 9999u, 1u, 11u));
    assert(!ChildPublishReady(shared, 1u, 4321u, 2u, 11u));
    assert(ChildPublishReady(shared, 1u, 4321u, 1u, 11u));
    assert(ChildPublishProducerStalled(shared, 1u, 4321u, 9u, 11u));
    assert(ChildPublishAppliedSnapshot(shared, 1u, 4321u,
        7u, 0x12345678u, 12u));
    // Parent publication admission closes before the uniquely identified
    // child records neutralization/removal and its terminal state.
    assert(DisableOutputGeneration(shared, 1u));
    assert(!ChildPublishReady(shared, 1u, 4321u, 1u, 12u));
    assert(ChildPublishStopping(shared, 1u, 4321u, 13u));
    assert(ChildPublishStopped(shared, 1u, 4321u,
        kChildDiagnosticNeutralApplied | kChildDiagnosticTargetsRemoved, 14u));
    assert(ReadChildTelemetry(shared, telemetry));
    assert(telemetry.state == ChildState::Stopped);
    assert(telemetry.readyGeneration == 1u);
    assert(telemetry.appliedPublicationSequence == 7u);
    assert(telemetry.completedStopGeneration == 1u);
    assert((telemetry.diagnosticFlags & kChildDiagnosticNeutralApplied) != 0u);
    assert((telemetry.diagnosticFlags & kChildDiagnosticTargetsRemoved) != 0u);
    assert((telemetry.diagnosticFlags & kChildDiagnosticProducerStalled) != 0u);

    // A force-killed child may leave the transaction odd. The parent rejects
    // it, and the sole writer of the already-separated replacement repairs it.
    shared.childTelemetrySequence |= 1u;
    assert(!ReadChildTelemetry(shared, telemetry));
    assert(BeginOutputGeneration(shared, 2u, 2u));
    assert(ChildPublishStarting(shared, 2u, 5000u, 20u,
        kChildDiagnosticContainedBeforeEntry));
    assert(ReadChildTelemetry(shared, telemetry));
    assert(telemetry.childGeneration == 2u);
    assert(telemetry.childPid == 5000u);
    assert((telemetry.telemetrySequence & 1u) == 0u);
    assert(ChildPublishReady(shared, 2u, 5000u, 2u, 21u));

    constexpr std::uint64_t kRelation = 0x9E3779B97F4A7C15ull;
    constexpr std::uint64_t kIterations = 100000u;
    std::atomic<bool> writerDone{ false };
    std::thread writer([&]()
    {
        for (std::uint64_t sequence = 1u; sequence <= kIterations; ++sequence)
        {
            assert(ChildPublishAppliedSnapshot(shared, 2u, 5000u,
                sequence, sequence ^ kRelation, sequence + 100u));
        }
        writerDone.store(true, std::memory_order_release);
    });

    std::uint64_t accepted = 0u;
    do
    {
        ChildTelemetrySnapshotV1 observed{};
        if (!ReadChildTelemetry(shared, observed) ||
            observed.childGeneration != 2u ||
            observed.appliedPublicationSequence == 0u)
        {
            continue;
        }
        assert(observed.childPid == 5000u);
        assert(observed.readyGeneration == 2u);
        assert(observed.diagnosticCheckpoint ==
            (observed.appliedPublicationSequence ^ kRelation));
        assert((observed.telemetrySequence & 1u) == 0u);
        ++accepted;
    } while (!writerDone.load(std::memory_order_acquire));
    writer.join();

    assert(ReadChildTelemetry(shared, telemetry));
    assert(telemetry.appliedPublicationSequence == kIterations);
    assert(telemetry.diagnosticCheckpoint == (kIterations ^ kRelation));
    assert(accepted > 0u);
    assert(DisableOutputGeneration(shared, 2u));
    return 0;
}
