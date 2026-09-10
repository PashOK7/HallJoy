#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "virtual_controller_frame.h"

namespace halljoy::provider_v2_qualification
{
constexpr std::size_t kFieldCount = 7u;
constexpr std::uint32_t kAllFieldMask = (1u << kFieldCount) - 1u;
constexpr std::size_t kMaximumPadCount = 4u;
constexpr std::size_t kCoverageBitCount = kFieldCount * kMaximumPadCount;
constexpr std::uint32_t kAllCoverageMask =
    (1u << kCoverageBitCount) - 1u;

enum class Verdict : std::uint8_t
{
    Incomplete = 0,
    Fail,
    Pass,
};

enum EvidenceGap : std::uint32_t
{
    EvidenceGap_None = 0,
    EvidenceGap_AppExit = 1u << 0,
    EvidenceGap_BackendGeneration = 1u << 1,
    EvidenceGap_V2UnavailableAtEnd = 1u << 2,
    EvidenceGap_EligibleTicks = 1u << 3,
    EvidenceGap_MatchedReports = 1u << 4,
    EvidenceGap_UniqueGenerations = 1u << 5,
    // Reserved from report schema 1. Duration is informational in schema 2;
    // equality is established by distinct samples and exercised fields.
    EvidenceGap_Duration = 1u << 6,
    EvidenceGap_NoConfiguredFields = 1u << 7,
    EvidenceGap_FieldsNotActivated = 1u << 8,
    EvidenceGap_FieldsNotReleased = 1u << 9,
    EvidenceGap_DigitalFallback = 1u << 10,
    EvidenceGap_CurveMutation = 1u << 11,
    EvidenceGap_ExcessUnavailable = 1u << 12,
};

struct SnapshotV1 final
{
    bool available = false;
    std::uint64_t backendInitCount = 0;
    std::uint64_t eligibleTicks = 0;
    std::uint64_t matchedReports = 0;
    std::uint64_t mismatchedReports = 0;
    std::uint64_t unavailableTicks = 0;
    std::uint64_t digitalFallbackTicks = 0;
    std::uint64_t curveMutationTicks = 0;
    std::array<std::uint64_t, kFieldCount> fieldMismatches{};
    std::uint64_t uniqueSampleGenerations = 0;
    std::uint64_t firstEligibleTickMs = 0;
    std::uint64_t lastEligibleTickMs = 0;
    std::uint64_t lastSampleGeneration = 0;
    std::uint32_t configuredFieldMask = 0;
    std::uint32_t activatedFieldMask = 0;
    std::uint32_t releasedFieldMask = 0;
};

struct PolicyV1 final
{
    std::uint64_t minimumEligibleTicks = 1000;
    std::uint64_t minimumMatchedReports = 1000;
    std::uint64_t minimumUniqueSampleGenerations = 100;
    std::uint32_t maximumUnavailablePercent = 1;
};

struct EvaluationV1 final
{
    Verdict verdict = Verdict::Incomplete;
    std::uint32_t evidenceGaps = EvidenceGap_None;
    std::uint32_t missingActivatedFieldMask = 0;
    std::uint32_t missingReleasedFieldMask = 0;
    std::uint64_t eligibleDurationMs = 0;
};

struct ReleaseTrackerUpdateV1 final
{
    std::uint32_t pendingMask = 0;
    std::uint32_t releasedNowMask = 0;
};

std::uint32_t ActiveFieldMask(
    const controller::VirtualControllerFrameV1& frame) noexcept;

ReleaseTrackerUpdateV1 UpdateReleaseTracker(std::uint32_t pendingMask,
    std::uint32_t activatedNowMask,
    std::uint32_t neutralNowMask) noexcept;

EvaluationV1 Evaluate(const SnapshotV1& snapshot, int appExitCode,
    const PolicyV1& policy = {}) noexcept;
}
