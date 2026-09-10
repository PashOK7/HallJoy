#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Versioned, fixed-layout protocol for the future HallJoy ViGEm output child.
// This header deliberately contains no Win32 HANDLE, pointer, STL container or
// ViGEm SDK type. All cross-process ownership is represented by generations.
namespace halljoy::vigem_output
{

constexpr std::uint32_t kSharedMagic = 0x314F5648u; // "HVO1"
constexpr std::uint32_t kSharedVersion = 2u;
constexpr std::uint32_t kMaxPads = 4u;
constexpr std::uint32_t kSnapshotSlotCount = 3u;

enum class SlotState : std::uint32_t
{
    Empty = 0,
    Writing = 1,
    Ready = 2,
    Reading = 3,
};

enum class ChildState : std::uint32_t
{
    Stopped = 0,
    Starting = 1,
    Ready = 2,
    Stopping = 3,
    Failed = 4,
};

// Output-specific completion facts. A process exit after a stop request is not
// a clean output stop unless the matching child generation publishes both.
constexpr std::uint32_t kChildDiagnosticContainedBeforeEntry = 1u << 0u;
constexpr std::uint32_t kChildDiagnosticNeutralApplied = 1u << 1u;
constexpr std::uint32_t kChildDiagnosticTargetsRemoved = 1u << 2u;
constexpr std::uint32_t kChildDiagnosticProducerStalled = 1u << 3u;

// Exact field domain used by an Xbox 360 report, without importing a third-
// party SDK structure into the IPC ABI. Conversion is explicit at each edge.
struct XusbReportV1
{
    std::uint16_t buttons;
    std::uint8_t leftTrigger;
    std::uint8_t rightTrigger;
    std::int16_t thumbLX;
    std::int16_t thumbLY;
    std::int16_t thumbRX;
    std::int16_t thumbRY;
};

struct SnapshotPayloadV1
{
    std::uint64_t outputGeneration;
    std::uint64_t publicationSequence;
    std::uint64_t timestampUs;
    // Binds this frame to a parent calculation lease so that a child which
    // already neutralized a stalled producer cannot replay an old nonzero slot.
    std::uint64_t producerLeaseSequence;
    std::uint32_t padCount;
    std::uint32_t validMask;
    XusbReportV1 reports[kMaxPads];
};

// State is acquired atomically before payload access. A writer may touch the
// payload only in Writing; the reader may touch it only in Reading. This avoids
// the formal C++ data race inherent in a plain-data seqlock.
struct alignas(64) SnapshotSlotV1
{
    alignas(4) std::uint32_t state;
    std::uint32_t reserved;
    SnapshotPayloadV1 payload;
};

struct alignas(64) SharedStateV1
{
    // Immutable after the child is created.
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t structSize;
    std::uint32_t slotCount;
    std::uint32_t ownerPid;
    std::uint32_t reservedHeader;
    alignas(8) std::uint64_t launchNonce;

    // Parent-written control/data-plane generations.
    alignas(8) std::uint64_t activeOutputGeneration;
    alignas(8) std::uint64_t publicationSequence;
    alignas(8) std::uint64_t requestedStopGeneration;
    alignas(8) std::uint64_t requestedReconnectGeneration;
    alignas(8) std::uint64_t configurationGeneration;
    alignas(8) std::uint64_t commandSequence;
    alignas(4) std::uint32_t publisherLeaseCount;
    std::uint32_t reservedPublisher;
    // Immutable while `activeOutputGeneration` is nonzero.  Three words form a
    // parent-only producer lease (generation, sequence, GetTickCount64 ms).
    // Keeping the reserved array preserves the fixed mapping layout.
    std::uint32_t requestedPadCount;
    std::uint32_t reservedConfiguration;
    std::uint64_t reservedControl[4];

    SnapshotSlotV1 slots[kSnapshotSlotCount];

    // Child-written health plane. Parent reads immutable snapshots of these
    // scalar atomics. Authoritative lifecycle/restart state belongs to the
    // parent supervisor and is deliberately not stored in this child plane.
    alignas(4) std::uint32_t childPid;
    std::uint32_t childReportedState;
    std::uint32_t childLastError;
    std::uint32_t childDiagnosticFlags;
    alignas(8) std::uint64_t readyGeneration;
    alignas(8) std::uint64_t heartbeatTickMs;
    alignas(8) std::uint64_t progressSequence;
    alignas(8) std::uint64_t appliedPublicationSequence;
    alignas(8) std::uint64_t completedStopGeneration;
    alignas(8) std::uint64_t appliedReconnectGeneration;
    alignas(8) std::uint64_t appliedConfigurationGeneration;
    alignas(8) std::uint64_t diagnosticCheckpoint;
    // The sole child writer makes this sequence odd around a health update and
    // even after it. The parent accepts only a stable even snapshot. A new
    // child repairs an odd sequence left by a force-killed predecessor before
    // publishing its own generation.
    alignas(8) std::uint64_t childTelemetrySequence;
    alignas(8) std::uint64_t childGeneration;
    std::uint64_t reservedHealth[3];
};

static_assert(sizeof(XusbReportV1) == 12u);
static_assert(sizeof(SnapshotPayloadV1) == 88u);
static_assert(sizeof(SnapshotSlotV1) == 128u);
static_assert(sizeof(SharedStateV1) == 640u);
static_assert(alignof(SnapshotSlotV1) == 64u);
static_assert(alignof(SharedStateV1) == 64u);
static_assert(offsetof(SharedStateV1, slots) == 128u);
static_assert(offsetof(SharedStateV1, requestedPadCount) == 88u);
static_assert(offsetof(SharedStateV1, childPid) == 512u);
static_assert(offsetof(SharedStateV1, childTelemetrySequence) == 592u);
static_assert(offsetof(SharedStateV1, childGeneration) == 600u);
static_assert(std::is_standard_layout_v<XusbReportV1>);
static_assert(std::is_trivially_copyable_v<XusbReportV1>);
static_assert(std::is_standard_layout_v<SnapshotPayloadV1>);
static_assert(std::is_trivially_copyable_v<SnapshotPayloadV1>);
static_assert(std::is_standard_layout_v<SharedStateV1>);
static_assert(std::is_trivially_copyable_v<SharedStateV1>);

} // namespace halljoy::vigem_output
