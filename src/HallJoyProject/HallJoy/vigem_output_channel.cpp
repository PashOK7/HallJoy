#include "vigem_output_channel.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <atomic>
#endif
#include <cstring>
#include <initializer_list>

namespace halljoy::vigem_output
{
namespace
{

constexpr std::size_t kProducerLeaseGenerationIndex = 0u;
constexpr std::size_t kProducerLeaseSequenceIndex = 1u;
constexpr std::size_t kProducerLeaseTickMsIndex = 2u;

#if defined(_WIN32)
static_assert(sizeof(LONG) == sizeof(std::uint32_t));
static_assert(sizeof(LONG64) == sizeof(std::uint64_t));

std::uint32_t AtomicLoad(std::uint32_t& value) noexcept
{
    return static_cast<std::uint32_t>(InterlockedCompareExchange(
        reinterpret_cast<volatile LONG*>(&value), 0, 0));
}

std::uint64_t AtomicLoad(std::uint64_t& value) noexcept
{
    return static_cast<std::uint64_t>(InterlockedCompareExchange64(
        reinterpret_cast<volatile LONG64*>(&value), 0, 0));
}

void AtomicStore(std::uint32_t& value, std::uint32_t desired) noexcept
{
    InterlockedExchange(
        reinterpret_cast<volatile LONG*>(&value), static_cast<LONG>(desired));
}

void AtomicStore(std::uint64_t& value, std::uint64_t desired) noexcept
{
    InterlockedExchange64(
        reinterpret_cast<volatile LONG64*>(&value), static_cast<LONG64>(desired));
}

bool AtomicCompareExchange(
    std::uint32_t& value,
    std::uint32_t& expected,
    std::uint32_t desired) noexcept
{
    const LONG observed = InterlockedCompareExchange(
        reinterpret_cast<volatile LONG*>(&value),
        static_cast<LONG>(desired),
        static_cast<LONG>(expected));
    if (static_cast<std::uint32_t>(observed) == expected)
        return true;
    expected = static_cast<std::uint32_t>(observed);
    return false;
}

bool AtomicCompareExchange(
    std::uint64_t& value,
    std::uint64_t& expected,
    std::uint64_t desired) noexcept
{
    const LONG64 observed = InterlockedCompareExchange64(
        reinterpret_cast<volatile LONG64*>(&value),
        static_cast<LONG64>(desired),
        static_cast<LONG64>(expected));
    if (static_cast<std::uint64_t>(observed) == expected)
        return true;
    expected = static_cast<std::uint64_t>(observed);
    return false;
}

std::uint32_t AtomicIncrement(std::uint32_t& value) noexcept
{
    return static_cast<std::uint32_t>(
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&value)));
}


std::uint64_t AtomicIncrement(std::uint64_t& value) noexcept
{
    return static_cast<std::uint64_t>(
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&value)));
}

void AtomicDecrement(std::uint32_t& value) noexcept
{
    InterlockedDecrement(reinterpret_cast<volatile LONG*>(&value));
}

std::uint64_t AtomicFetchAdd(std::uint64_t& value, std::uint64_t amount) noexcept
{
    return static_cast<std::uint64_t>(InterlockedExchangeAdd64(
        reinterpret_cast<volatile LONG64*>(&value), static_cast<LONG64>(amount)));
}
#else
static_assert(std::atomic_ref<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic_ref<std::uint64_t>::is_always_lock_free);

std::uint32_t AtomicLoad(std::uint32_t& value) noexcept
{
    return std::atomic_ref<std::uint32_t>(value).load(std::memory_order_acquire);
}

std::uint64_t AtomicLoad(std::uint64_t& value) noexcept
{
    return std::atomic_ref<std::uint64_t>(value).load(std::memory_order_acquire);
}

void AtomicStore(std::uint32_t& value, std::uint32_t desired) noexcept
{
    std::atomic_ref<std::uint32_t>(value).store(desired, std::memory_order_release);
}

void AtomicStore(std::uint64_t& value, std::uint64_t desired) noexcept
{
    std::atomic_ref<std::uint64_t>(value).store(desired, std::memory_order_release);
}

bool AtomicCompareExchange(
    std::uint32_t& value,
    std::uint32_t& expected,
    std::uint32_t desired) noexcept
{
    return std::atomic_ref<std::uint32_t>(value).compare_exchange_strong(
        expected, desired, std::memory_order_acq_rel, std::memory_order_relaxed);
}

bool AtomicCompareExchange(
    std::uint64_t& value,
    std::uint64_t& expected,
    std::uint64_t desired) noexcept
{
    return std::atomic_ref<std::uint64_t>(value).compare_exchange_strong(
        expected, desired, std::memory_order_acq_rel, std::memory_order_relaxed);
}

std::uint32_t AtomicIncrement(std::uint32_t& value) noexcept
{
    return std::atomic_ref<std::uint32_t>(value).fetch_add(
        1u, std::memory_order_acq_rel) + 1u;
}

std::uint64_t AtomicIncrement(std::uint64_t& value) noexcept
{
    return std::atomic_ref<std::uint64_t>(value).fetch_add(
        1u, std::memory_order_acq_rel) + 1u;
}

void AtomicDecrement(std::uint32_t& value) noexcept
{
    std::atomic_ref<std::uint32_t>(value).fetch_sub(1u, std::memory_order_acq_rel);
}

std::uint64_t AtomicFetchAdd(std::uint64_t& value, std::uint64_t amount) noexcept
{
    return std::atomic_ref<std::uint64_t>(value).fetch_add(
        amount, std::memory_order_relaxed);
}
#endif

bool IsPayloadShapeValid(std::uint32_t padCount, std::uint32_t validMask) noexcept
{
    if (padCount > kMaxPads)
        return false;
    const std::uint32_t allowedMask = padCount == 0u ? 0u : ((1u << padCount) - 1u);
    return (validMask & ~allowedMask) == 0u;
}

SnapshotSlotV1* TryClaimPublishSlot(SharedStateV1& shared) noexcept
{
    for (const SlotState wanted : { SlotState::Empty, SlotState::Ready })
    {
        for (SnapshotSlotV1& slot : shared.slots)
        {
            std::uint32_t expected = static_cast<std::uint32_t>(wanted);
            if (AtomicCompareExchange(
                    slot.state, expected, static_cast<std::uint32_t>(SlotState::Writing)))
            {
                return &slot;
            }
        }
    }
    return nullptr;
}

void ReleaseSlot(SnapshotSlotV1& slot, SlotState state) noexcept
{
    AtomicStore(slot.state, static_cast<std::uint32_t>(state));
}

void BeginChildTelemetryUpdate(SharedStateV1& shared) noexcept
{
    // A force-killed predecessor may leave the sequence odd. Exactly one child
    // generation is alive, so its successor is the only writer allowed to
    // repair that incomplete transaction before beginning a new one.
    if ((AtomicLoad(shared.childTelemetrySequence) & 1u) != 0u)
        (void)AtomicIncrement(shared.childTelemetrySequence);
    (void)AtomicIncrement(shared.childTelemetrySequence);
}

void EndChildTelemetryUpdate(SharedStateV1& shared) noexcept
{
    (void)AtomicIncrement(shared.childTelemetrySequence);
}

bool ChildIdentityMatches(SharedStateV1& shared, std::uint64_t generation,
    std::uint32_t childPid) noexcept
{
    // activeOutputGeneration is the parent publication-admission gate, not
    // the child lifetime identity. The parent deliberately closes admission
    // before asking the child to neutralize/remove targets, so lifecycle
    // telemetry must remain writable until that uniquely identified child is
    // reaped. A successor is never launched before the predecessor is reaped.
    return generation != 0u && childPid != 0u &&
        AtomicLoad(shared.childGeneration) == generation &&
        AtomicLoad(shared.childPid) == childPid;
}

} // namespace

void InitializeSharedState(
    SharedStateV1& shared,
    std::uint32_t ownerPid,
    std::uint64_t launchNonce) noexcept
{
    std::memset(&shared, 0, sizeof(shared));
    shared.magic = kSharedMagic;
    shared.version = kSharedVersion;
    shared.structSize = static_cast<std::uint32_t>(sizeof(shared));
    shared.slotCount = kSnapshotSlotCount;
    shared.ownerPid = ownerPid;
    shared.launchNonce = launchNonce;
    shared.childReportedState = static_cast<std::uint32_t>(ChildState::Stopped);
}

bool ValidateSharedState(
    const SharedStateV1& shared,
    std::uint32_t expectedOwnerPid,
    std::uint64_t expectedLaunchNonce) noexcept
{
    return expectedOwnerPid != 0u &&
        expectedLaunchNonce != 0u &&
        shared.magic == kSharedMagic &&
        shared.version == kSharedVersion &&
        shared.structSize == sizeof(SharedStateV1) &&
        shared.slotCount == kSnapshotSlotCount &&
        shared.ownerPid == expectedOwnerPid &&
        shared.launchNonce == expectedLaunchNonce;
}

bool BeginOutputGeneration(SharedStateV1& shared, std::uint64_t generation,
    std::uint32_t padCount) noexcept
{
    if (generation == 0u || padCount == 0u || padCount > kMaxPads)
        return false;
    std::uint64_t expected = 0u;
    if (!AtomicCompareExchange(shared.activeOutputGeneration, expected, generation))
        return false;
    AtomicStore(shared.requestedPadCount, padCount);
    AtomicStore(shared.configurationGeneration, generation);
    AtomicStore(shared.reservedControl[kProducerLeaseGenerationIndex], 0u);
    AtomicStore(shared.reservedControl[kProducerLeaseSequenceIndex], 0u);
    AtomicStore(shared.reservedControl[kProducerLeaseTickMsIndex], 0u);
    return true;
}

bool ReadOutputConfiguration(SharedStateV1& shared,
    std::uint64_t expectedGeneration,
    OutputConfigurationV1& configuration) noexcept
{
    if (expectedGeneration == 0u ||
        AtomicLoad(shared.activeOutputGeneration) != expectedGeneration)
    {
        return false;
    }

    OutputConfigurationV1 candidate{};
    candidate.generation = AtomicLoad(shared.configurationGeneration);
    candidate.padCount = AtomicLoad(shared.requestedPadCount);
    if (AtomicLoad(shared.activeOutputGeneration) != expectedGeneration ||
        candidate.generation != expectedGeneration ||
        candidate.padCount == 0u || candidate.padCount > kMaxPads)
    {
        return false;
    }
    configuration = candidate;
    return true;
}

bool DisableOutputGeneration(
    SharedStateV1& shared,
    std::uint64_t expectedGeneration) noexcept
{
    if (expectedGeneration == 0u)
        return false;
    return AtomicCompareExchange(shared.activeOutputGeneration, expectedGeneration, 0u);
}

bool PublicationQuiescent(SharedStateV1& shared) noexcept
{
    return AtomicLoad(shared.publisherLeaseCount) == 0u;
}

std::uint64_t ActiveOutputGeneration(SharedStateV1& shared) noexcept
{
    return AtomicLoad(shared.activeOutputGeneration);
}

bool PublishProducerLease(SharedStateV1& shared, std::uint64_t tickMs) noexcept
{
    const std::uint64_t generation = ActiveOutputGeneration(shared);
    if (generation == 0u ||
        AtomicLoad(shared.configurationGeneration) != generation)
    {
        return false;
    }

    // The parent is the sole writer. Sequence is committed last, allowing the
    // child to reject a torn observation by reading it twice.
    AtomicStore(shared.reservedControl[kProducerLeaseGenerationIndex], generation);
    AtomicStore(shared.reservedControl[kProducerLeaseTickMsIndex], tickMs);
    std::uint64_t sequence = AtomicIncrement(
        shared.reservedControl[kProducerLeaseSequenceIndex]);
    if (sequence == 0u)
        sequence = AtomicIncrement(shared.reservedControl[kProducerLeaseSequenceIndex]);
    return ActiveOutputGeneration(shared) == generation &&
        AtomicLoad(shared.configurationGeneration) == generation;
}

bool ReadProducerLease(SharedStateV1& shared,
    std::uint64_t expectedGeneration,
    ProducerLeaseViewV1& lease) noexcept
{
    if (expectedGeneration == 0u ||
        ActiveOutputGeneration(shared) != expectedGeneration)
    {
        return false;
    }

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const std::uint64_t firstSequence = AtomicLoad(
            shared.reservedControl[kProducerLeaseSequenceIndex]);
        ProducerLeaseViewV1 candidate{};
        candidate.generation = AtomicLoad(
            shared.reservedControl[kProducerLeaseGenerationIndex]);
        candidate.sequence = firstSequence;
        candidate.tickMs = AtomicLoad(
            shared.reservedControl[kProducerLeaseTickMsIndex]);
        const std::uint64_t finalSequence = AtomicLoad(
            shared.reservedControl[kProducerLeaseSequenceIndex]);
        if (firstSequence == finalSequence &&
            ActiveOutputGeneration(shared) == expectedGeneration)
        {
            lease = candidate;
            return true;
        }
    }
    return false;
}

ProducerLeaseState ReadProducerLeaseState(SharedStateV1& shared,
    std::uint64_t expectedGeneration, std::uint64_t nowMs,
    std::uint64_t readyTickMs, ProducerLeaseViewV1* lease) noexcept
{
    ProducerLeaseViewV1 candidate{};
    if (!ReadProducerLease(shared, expectedGeneration, candidate))
        return ProducerLeaseState::WrongGeneration;
    if (lease)
        *lease = candidate;
    return EvaluateProducerLease(nowMs, expectedGeneration, candidate, readyTickMs);
}

PublishResult TryPublishSnapshot(
    SharedStateV1& shared,
    const XusbReportV1* reports,
    std::uint32_t padCount,
    std::uint32_t validMask,
    std::uint64_t timestampUs,
    std::uint64_t* publicationSequence) noexcept
{
    if (!IsPayloadShapeValid(padCount, validMask) || (padCount != 0u && reports == nullptr))
        return PublishResult::InvalidPayload;

    const std::uint64_t outputGeneration = ActiveOutputGeneration(shared);
    if (outputGeneration == 0u)
        return PublishResult::Inactive;

    // A report set is meaningful only for the pad topology committed to this
    // exact output generation. Configure may race the realtime producer: treat
    // that boundary as temporarily inactive instead of allowing a differently
    // shaped snapshot to reach the old/new child and fault its transport.
    if (AtomicLoad(shared.configurationGeneration) != outputGeneration ||
        AtomicLoad(shared.requestedPadCount) != padCount)
    {
        return PublishResult::Inactive;
    }

    if (AtomicIncrement(shared.publisherLeaseCount) == 0u)
    {
        AtomicDecrement(shared.publisherLeaseCount);
        return PublishResult::Contended;
    }
    if (ActiveOutputGeneration(shared) != outputGeneration ||
        AtomicLoad(shared.configurationGeneration) != outputGeneration ||
        AtomicLoad(shared.requestedPadCount) != padCount)
    {
        AtomicDecrement(shared.publisherLeaseCount);
        return PublishResult::Inactive;
    }

    SnapshotSlotV1* slot = TryClaimPublishSlot(shared);
    if (!slot)
    {
        AtomicDecrement(shared.publisherLeaseCount);
        return PublishResult::Contended;
    }

    // A stop/restart may race the claim. Never publish a snapshot after its
    // generation has ceased to be the active publication target.
    if (ActiveOutputGeneration(shared) != outputGeneration ||
        AtomicLoad(shared.configurationGeneration) != outputGeneration ||
        AtomicLoad(shared.requestedPadCount) != padCount)
    {
        ReleaseSlot(*slot, SlotState::Empty);
        AtomicDecrement(shared.publisherLeaseCount);
        return PublishResult::Inactive;
    }

    std::uint64_t sequence = AtomicFetchAdd(shared.publicationSequence, 1u) + 1u;
    if (sequence == 0u)
        sequence = AtomicFetchAdd(shared.publicationSequence, 1u) + 1u;

    SnapshotPayloadV1 next{};
    next.outputGeneration = outputGeneration;
    next.publicationSequence = sequence;
    next.timestampUs = timestampUs;
    const std::uint64_t producerLeaseGeneration = AtomicLoad(
        shared.reservedControl[kProducerLeaseGenerationIndex]);
    const std::uint64_t producerLeaseSequence = AtomicLoad(
        shared.reservedControl[kProducerLeaseSequenceIndex]);
    if (producerLeaseGeneration != outputGeneration || producerLeaseSequence == 0u)
    {
        ReleaseSlot(*slot, SlotState::Empty);
        AtomicDecrement(shared.publisherLeaseCount);
        return PublishResult::Inactive;
    }
    next.producerLeaseSequence = producerLeaseSequence;
    next.padCount = padCount;
    next.validMask = validMask;
    for (std::uint32_t i = 0; i < padCount; ++i)
        next.reports[i] = reports[i];

    slot->payload = next;
    ReleaseSlot(*slot, SlotState::Ready);
    AtomicDecrement(shared.publisherLeaseCount);
    if (publicationSequence)
        *publicationSequence = sequence;
    return PublishResult::Published;
}

ConsumeResult TryConsumeNewestSnapshot(
    SharedStateV1& shared,
    std::uint64_t expectedOutputGeneration,
    std::uint64_t afterPublicationSequence,
    SnapshotPayloadV1* snapshot) noexcept
{
    if (expectedOutputGeneration == 0u || !snapshot)
        return ConsumeResult::InvalidArgument;

    bool found = false;
    SnapshotPayloadV1 newest{};
    for (SnapshotSlotV1& slot : shared.slots)
    {
        std::uint32_t expected = static_cast<std::uint32_t>(SlotState::Ready);
        if (!AtomicCompareExchange(
                slot.state, expected, static_cast<std::uint32_t>(SlotState::Reading)))
        {
            continue;
        }

        const SnapshotPayloadV1 candidate = slot.payload;
        ReleaseSlot(slot, SlotState::Empty);
        if (candidate.outputGeneration != expectedOutputGeneration ||
            candidate.publicationSequence <= afterPublicationSequence)
        {
            continue;
        }
        if (!found || candidate.publicationSequence > newest.publicationSequence)
        {
            newest = candidate;
            found = true;
        }
    }

    if (!found)
        return ConsumeResult::NoUpdate;
    *snapshot = newest;
    return ConsumeResult::Updated;
}

std::uint64_t ComputeReportCheckpoint(const XusbReportV1* reports,
    std::uint32_t padCount, std::uint32_t validMask) noexcept
{
    if (!IsPayloadShapeValid(padCount, validMask) ||
        (padCount != 0u && reports == nullptr))
    {
        return 0u;
    }

    constexpr std::uint64_t kOffset = 1469598103934665603ull;
    constexpr std::uint64_t kPrime = 1099511628211ull;
    std::uint64_t hash = kOffset;
    const auto addBytes = [&hash](const void* source, std::size_t size) noexcept
    {
        const auto* bytes = static_cast<const unsigned char*>(source);
        for (std::size_t index = 0; index < size; ++index)
        {
            hash ^= bytes[index];
            hash *= kPrime;
        }
    };
    addBytes(&padCount, sizeof(padCount));
    addBytes(&validMask, sizeof(validMask));
    for (std::uint32_t index = 0; index < padCount; ++index)
        addBytes(&reports[index], sizeof(reports[index]));
    return hash == 0u ? 1u : hash;
}

bool ChildPublishStarting(SharedStateV1& shared, std::uint64_t generation,
    std::uint32_t childPid, std::uint64_t tickMs,
    std::uint32_t diagnosticFlags) noexcept
{
    if (generation == 0u || childPid == 0u ||
        ActiveOutputGeneration(shared) != generation)
    {
        return false;
    }

    BeginChildTelemetryUpdate(shared);
    AtomicStore(shared.childGeneration, generation);
    AtomicStore(shared.childPid, childPid);
    AtomicStore(shared.childLastError, 0u);
    AtomicStore(shared.childDiagnosticFlags, diagnosticFlags);
    AtomicStore(shared.readyGeneration, 0u);
    AtomicStore(shared.heartbeatTickMs, tickMs);
    AtomicStore(shared.progressSequence, 1u);
    AtomicStore(shared.appliedPublicationSequence, 0u);
    AtomicStore(shared.completedStopGeneration, 0u);
    AtomicStore(shared.appliedReconnectGeneration, 0u);
    AtomicStore(shared.appliedConfigurationGeneration, 0u);
    AtomicStore(shared.diagnosticCheckpoint, 0u);
    AtomicStore(shared.childReportedState,
        static_cast<std::uint32_t>(ChildState::Starting));
    EndChildTelemetryUpdate(shared);
    return true;
}

bool ChildPublishReady(SharedStateV1& shared, std::uint64_t generation,
    std::uint32_t childPid, std::uint64_t appliedConfigurationGeneration,
    std::uint64_t tickMs) noexcept
{
    if (appliedConfigurationGeneration != generation ||
        ActiveOutputGeneration(shared) != generation ||
        !ChildIdentityMatches(shared, generation, childPid))
        return false;
    BeginChildTelemetryUpdate(shared);
    AtomicStore(shared.readyGeneration, generation);
    AtomicStore(shared.appliedConfigurationGeneration,
        appliedConfigurationGeneration);
    AtomicStore(shared.heartbeatTickMs, tickMs);
    (void)AtomicIncrement(shared.progressSequence);
    AtomicStore(shared.childReportedState,
        static_cast<std::uint32_t>(ChildState::Ready));
    EndChildTelemetryUpdate(shared);
    return true;
}

bool ChildPublishHeartbeat(SharedStateV1& shared, std::uint64_t generation,
    std::uint32_t childPid, std::uint64_t tickMs) noexcept
{
    if (!ChildIdentityMatches(shared, generation, childPid))
        return false;
    BeginChildTelemetryUpdate(shared);
    AtomicStore(shared.heartbeatTickMs, tickMs);
    (void)AtomicIncrement(shared.progressSequence);
    EndChildTelemetryUpdate(shared);
    return true;
}

bool ChildPublishAppliedSnapshot(SharedStateV1& shared,
    std::uint64_t generation, std::uint32_t childPid,
    std::uint64_t publicationSequence, std::uint64_t diagnosticCheckpoint,
    std::uint64_t tickMs) noexcept
{
    if (publicationSequence == 0u ||
        !ChildIdentityMatches(shared, generation, childPid))
    {
        return false;
    }
    BeginChildTelemetryUpdate(shared);
    AtomicStore(shared.appliedPublicationSequence, publicationSequence);
    AtomicStore(shared.diagnosticCheckpoint, diagnosticCheckpoint);
    AtomicStore(shared.heartbeatTickMs, tickMs);
    (void)AtomicIncrement(shared.progressSequence);
    EndChildTelemetryUpdate(shared);
    return true;
}

bool ChildPublishProducerStalled(SharedStateV1& shared,
    std::uint64_t generation, std::uint32_t childPid,
    std::uint64_t producerLeaseSequence, std::uint64_t tickMs) noexcept
{
    if (!ChildIdentityMatches(shared, generation, childPid))
        return false;
    BeginChildTelemetryUpdate(shared);
    AtomicStore(shared.childDiagnosticFlags,
        AtomicLoad(shared.childDiagnosticFlags) | kChildDiagnosticProducerStalled);
    AtomicStore(shared.diagnosticCheckpoint, producerLeaseSequence);
    AtomicStore(shared.heartbeatTickMs, tickMs);
    (void)AtomicIncrement(shared.progressSequence);
    EndChildTelemetryUpdate(shared);
    return true;
}

bool ChildPublishStopping(SharedStateV1& shared, std::uint64_t generation,
    std::uint32_t childPid, std::uint64_t tickMs) noexcept
{
    if (!ChildIdentityMatches(shared, generation, childPid))
        return false;
    BeginChildTelemetryUpdate(shared);
    AtomicStore(shared.heartbeatTickMs, tickMs);
    (void)AtomicIncrement(shared.progressSequence);
    AtomicStore(shared.childReportedState,
        static_cast<std::uint32_t>(ChildState::Stopping));
    EndChildTelemetryUpdate(shared);
    return true;
}

bool ChildPublishStopped(SharedStateV1& shared, std::uint64_t generation,
    std::uint32_t childPid, std::uint32_t completionFlags,
    std::uint64_t tickMs) noexcept
{
    if (!ChildIdentityMatches(shared, generation, childPid))
        return false;
    BeginChildTelemetryUpdate(shared);
    AtomicStore(shared.childDiagnosticFlags,
        AtomicLoad(shared.childDiagnosticFlags) | completionFlags);
    AtomicStore(shared.completedStopGeneration, generation);
    AtomicStore(shared.heartbeatTickMs, tickMs);
    (void)AtomicIncrement(shared.progressSequence);
    AtomicStore(shared.childReportedState,
        static_cast<std::uint32_t>(ChildState::Stopped));
    EndChildTelemetryUpdate(shared);
    return true;
}

bool ChildPublishFault(SharedStateV1& shared, std::uint64_t generation,
    std::uint32_t childPid, std::uint32_t error,
    std::uint64_t diagnosticCheckpoint, std::uint64_t tickMs,
    std::uint32_t additionalDiagnosticFlags) noexcept
{
    if (error == 0u || !ChildIdentityMatches(shared, generation, childPid))
        return false;
    BeginChildTelemetryUpdate(shared);
    AtomicStore(shared.childLastError, error);
    AtomicStore(shared.childDiagnosticFlags,
        AtomicLoad(shared.childDiagnosticFlags) | additionalDiagnosticFlags);
    AtomicStore(shared.diagnosticCheckpoint, diagnosticCheckpoint);
    AtomicStore(shared.heartbeatTickMs, tickMs);
    (void)AtomicIncrement(shared.progressSequence);
    AtomicStore(shared.childReportedState,
        static_cast<std::uint32_t>(ChildState::Failed));
    EndChildTelemetryUpdate(shared);
    return true;
}

bool ReadChildTelemetry(SharedStateV1& shared,
    ChildTelemetrySnapshotV1& snapshot) noexcept
{
    constexpr unsigned kMaxAttempts = 8u;
    for (unsigned attempt = 0; attempt < kMaxAttempts; ++attempt)
    {
        const std::uint64_t before = AtomicLoad(shared.childTelemetrySequence);
        if ((before & 1u) != 0u)
            continue;

        ChildTelemetrySnapshotV1 candidate{};
        candidate.telemetrySequence = before;
        candidate.childGeneration = AtomicLoad(shared.childGeneration);
        candidate.childPid = AtomicLoad(shared.childPid);
        candidate.state = static_cast<ChildState>(
            AtomicLoad(shared.childReportedState));
        candidate.lastError = AtomicLoad(shared.childLastError);
        candidate.diagnosticFlags = AtomicLoad(shared.childDiagnosticFlags);
        candidate.readyGeneration = AtomicLoad(shared.readyGeneration);
        candidate.heartbeatTickMs = AtomicLoad(shared.heartbeatTickMs);
        candidate.progressSequence = AtomicLoad(shared.progressSequence);
        candidate.appliedPublicationSequence =
            AtomicLoad(shared.appliedPublicationSequence);
        candidate.completedStopGeneration =
            AtomicLoad(shared.completedStopGeneration);
        candidate.appliedReconnectGeneration =
            AtomicLoad(shared.appliedReconnectGeneration);
        candidate.appliedConfigurationGeneration =
            AtomicLoad(shared.appliedConfigurationGeneration);
        candidate.diagnosticCheckpoint =
            AtomicLoad(shared.diagnosticCheckpoint);

        const std::uint64_t after = AtomicLoad(shared.childTelemetrySequence);
        if (before == after && (after & 1u) == 0u)
        {
            candidate.telemetrySequence = after;
            snapshot = candidate;
            return true;
        }
    }
    return false;
}

} // namespace halljoy::vigem_output
