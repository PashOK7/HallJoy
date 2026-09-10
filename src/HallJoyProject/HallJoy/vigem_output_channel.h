#pragma once

#include "vigem_output_shared.h"
#include "vigem_output_producer_lease.h"

namespace halljoy::vigem_output
{

enum class PublishResult : std::uint8_t
{
    Published,
    Inactive,
    InvalidPayload,
    Contended,
};

enum class ConsumeResult : std::uint8_t
{
    Updated,
    NoUpdate,
    InvalidArgument,
};

// Complete parent-side observation of the single-writer child health plane.
// `ReadChildTelemetry` returns false instead of exposing an in-progress or
// force-kill-torn update.
struct ChildTelemetrySnapshotV1
{
    std::uint64_t telemetrySequence = 0;
    std::uint64_t childGeneration = 0;
    std::uint32_t childPid = 0;
    ChildState state = ChildState::Stopped;
    std::uint32_t lastError = 0;
    std::uint32_t diagnosticFlags = 0;
    std::uint64_t readyGeneration = 0;
    std::uint64_t heartbeatTickMs = 0;
    std::uint64_t progressSequence = 0;
    std::uint64_t appliedPublicationSequence = 0;
    std::uint64_t completedStopGeneration = 0;
    std::uint64_t appliedReconnectGeneration = 0;
    std::uint64_t appliedConfigurationGeneration = 0;
    std::uint64_t diagnosticCheckpoint = 0;
};

struct OutputConfigurationV1
{
    std::uint64_t generation = 0;
    std::uint32_t padCount = 0;
};

// Parent-only producer progress. It is independent of publication/de-duplication:
// an unchanged held report is still a live calculated tick. The child reads a
// coherent snapshot and owns the stale-to-neutral decision.
[[nodiscard]] bool PublishProducerLease(
    SharedStateV1& shared,
    std::uint64_t tickMs) noexcept;

[[nodiscard]] bool ReadProducerLease(
    SharedStateV1& shared,
    std::uint64_t expectedGeneration,
    ProducerLeaseViewV1& lease) noexcept;

[[nodiscard]] ProducerLeaseState ReadProducerLeaseState(
    SharedStateV1& shared,
    std::uint64_t expectedGeneration,
    std::uint64_t nowMs,
    std::uint64_t readyTickMs,
    ProducerLeaseViewV1* lease = nullptr) noexcept;

// Initialization happens before child creation. ownerPid and launchNonce must
// both be nonzero; the nonce is not a secret, but binds inherited resources to
// one launch transaction.
void InitializeSharedState(
    SharedStateV1& shared,
    std::uint32_t ownerPid,
    std::uint64_t launchNonce) noexcept;

[[nodiscard]] bool ValidateSharedState(
    const SharedStateV1& shared,
    std::uint32_t expectedOwnerPid,
    std::uint64_t expectedLaunchNonce) noexcept;

// Exactly one supervisor owns these transitions. A generation may begin only
// from inactive state and must be disabled with the same expected generation.
[[nodiscard]] bool BeginOutputGeneration(
    SharedStateV1& shared,
    std::uint64_t generation,
    std::uint32_t padCount) noexcept;

// Configuration is immutable for the lifetime of one active generation. The
// child accepts it only when both generation reads match the expected owner.
[[nodiscard]] bool ReadOutputConfiguration(
    SharedStateV1& shared,
    std::uint64_t expectedGeneration,
    OutputConfigurationV1& configuration) noexcept;

[[nodiscard]] bool DisableOutputGeneration(
    SharedStateV1& shared,
    std::uint64_t expectedGeneration) noexcept;

// The supervisor first disables the generation, then observes this predicate
// outside realtime before issuing child stop. Publishers never wait; their
// bounded lease closes the final check-to-publication window.
[[nodiscard]] bool PublicationQuiescent(SharedStateV1& shared) noexcept;

[[nodiscard]] std::uint64_t ActiveOutputGeneration(
    SharedStateV1& shared) noexcept;

// Single realtime producer. The call never waits: it either publishes a full
// snapshot into an exclusively claimed slot or returns immediately. A payload
// whose pad count does not match the active generation is transiently Inactive,
// so a configuration boundary can never poison either generation.
[[nodiscard]] PublishResult TryPublishSnapshot(
    SharedStateV1& shared,
    const XusbReportV1* reports,
    std::uint32_t padCount,
    std::uint32_t validMask,
    std::uint64_t timestampUs,
    std::uint64_t* publicationSequence = nullptr) noexcept;

// Single child consumer. All ready slots are claimed one at a time; stale
// generations are discarded and only the newest matching complete snapshot is
// returned. No payload is read without owning its slot.
[[nodiscard]] ConsumeResult TryConsumeNewestSnapshot(
    SharedStateV1& shared,
    std::uint64_t expectedOutputGeneration,
    std::uint64_t afterPublicationSequence,
    SnapshotPayloadV1* snapshot) noexcept;

// Stable SDK-independent report checkpoint used by diagnostics and the fake
// exact-EXE transport gate. Timestamp and publication sequence are deliberately
// excluded so equivalence describes the four-pad output value itself.
[[nodiscard]] std::uint64_t ComputeReportCheckpoint(
    const XusbReportV1* reports,
    std::uint32_t padCount,
    std::uint32_t validMask) noexcept;

// The following functions are the only supported writers for the child health
// plane. Exactly one contained child process writes a generation at a time.
[[nodiscard]] bool ChildPublishStarting(
    SharedStateV1& shared,
    std::uint64_t generation,
    std::uint32_t childPid,
    std::uint64_t tickMs,
    std::uint32_t diagnosticFlags) noexcept;

[[nodiscard]] bool ChildPublishReady(
    SharedStateV1& shared,
    std::uint64_t generation,
    std::uint32_t childPid,
    std::uint64_t appliedConfigurationGeneration,
    std::uint64_t tickMs) noexcept;

[[nodiscard]] bool ChildPublishHeartbeat(
    SharedStateV1& shared,
    std::uint64_t generation,
    std::uint32_t childPid,
    std::uint64_t tickMs) noexcept;

[[nodiscard]] bool ChildPublishAppliedSnapshot(
    SharedStateV1& shared,
    std::uint64_t generation,
    std::uint32_t childPid,
    std::uint64_t publicationSequence,
    std::uint64_t diagnosticCheckpoint,
    std::uint64_t tickMs) noexcept;

// A contained child emits this once per stale episode after applying a neutral
// report. It is a health fact, not a transport fault or a restart command.
[[nodiscard]] bool ChildPublishProducerStalled(
    SharedStateV1& shared,
    std::uint64_t generation,
    std::uint32_t childPid,
    std::uint64_t producerLeaseSequence,
    std::uint64_t tickMs) noexcept;

[[nodiscard]] bool ChildPublishStopping(
    SharedStateV1& shared,
    std::uint64_t generation,
    std::uint32_t childPid,
    std::uint64_t tickMs) noexcept;

[[nodiscard]] bool ChildPublishStopped(
    SharedStateV1& shared,
    std::uint64_t generation,
    std::uint32_t childPid,
    std::uint32_t completionFlags,
    std::uint64_t tickMs) noexcept;

[[nodiscard]] bool ChildPublishFault(
    SharedStateV1& shared,
    std::uint64_t generation,
    std::uint32_t childPid,
    std::uint32_t error,
    std::uint64_t diagnosticCheckpoint,
    std::uint64_t tickMs,
    std::uint32_t additionalDiagnosticFlags = 0u) noexcept;

[[nodiscard]] bool ReadChildTelemetry(
    SharedStateV1& shared,
    ChildTelemetrySnapshotV1& snapshot) noexcept;

} // namespace halljoy::vigem_output
