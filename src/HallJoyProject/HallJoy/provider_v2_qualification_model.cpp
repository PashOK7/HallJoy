#include "provider_v2_qualification_model.h"

#include <algorithm>

namespace halljoy::provider_v2_qualification
{
std::uint32_t ActiveFieldMask(
    const controller::VirtualControllerFrameV1& frame) noexcept
{
    std::uint32_t mask = 0;
    if (frame.buttons != 0)
        mask |= 1u << 0;
    if (frame.leftTrigger != 0)
        mask |= 1u << 1;
    if (frame.rightTrigger != 0)
        mask |= 1u << 2;
    if (frame.leftStickX != 0)
        mask |= 1u << 3;
    if (frame.leftStickY != 0)
        mask |= 1u << 4;
    if (frame.rightStickX != 0)
        mask |= 1u << 5;
    if (frame.rightStickY != 0)
        mask |= 1u << 6;
    return mask;
}

ReleaseTrackerUpdateV1 UpdateReleaseTracker(std::uint32_t pendingMask,
    std::uint32_t activatedNowMask,
    std::uint32_t neutralNowMask) noexcept
{
    ReleaseTrackerUpdateV1 result{};
    result.pendingMask = pendingMask | activatedNowMask;
    result.releasedNowMask = result.pendingMask & neutralNowMask;
    result.pendingMask &= ~result.releasedNowMask;
    return result;
}

EvaluationV1 Evaluate(const SnapshotV1& snapshot, int appExitCode,
    const PolicyV1& policy) noexcept
{
    EvaluationV1 result{};
    const std::uint32_t configured =
        snapshot.configuredFieldMask & kAllCoverageMask;
    result.missingActivatedFieldMask = configured &
        ~snapshot.activatedFieldMask;
    result.missingReleasedFieldMask = configured &
        ~snapshot.releasedFieldMask;
    if (snapshot.lastEligibleTickMs >= snapshot.firstEligibleTickMs &&
        snapshot.firstEligibleTickMs != 0)
    {
        result.eligibleDurationMs = snapshot.lastEligibleTickMs -
            snapshot.firstEligibleTickMs;
    }

    bool mismatch = snapshot.mismatchedReports != 0;
    for (const std::uint64_t count : snapshot.fieldMismatches)
        mismatch = mismatch || count != 0;
    if (mismatch)
    {
        result.verdict = Verdict::Fail;
        return result;
    }

    if (appExitCode != 0)
        result.evidenceGaps |= EvidenceGap_AppExit;
    if (snapshot.backendInitCount != 1)
        result.evidenceGaps |= EvidenceGap_BackendGeneration;
    if (!snapshot.available)
        result.evidenceGaps |= EvidenceGap_V2UnavailableAtEnd;
    if (snapshot.eligibleTicks < policy.minimumEligibleTicks)
        result.evidenceGaps |= EvidenceGap_EligibleTicks;
    if (snapshot.matchedReports < policy.minimumMatchedReports)
        result.evidenceGaps |= EvidenceGap_MatchedReports;
    if (snapshot.uniqueSampleGenerations <
        policy.minimumUniqueSampleGenerations)
    {
        result.evidenceGaps |= EvidenceGap_UniqueGenerations;
    }
    if (configured == 0)
        result.evidenceGaps |= EvidenceGap_NoConfiguredFields;
    if (result.missingActivatedFieldMask != 0)
        result.evidenceGaps |= EvidenceGap_FieldsNotActivated;
    if (result.missingReleasedFieldMask != 0)
        result.evidenceGaps |= EvidenceGap_FieldsNotReleased;
    if (snapshot.digitalFallbackTicks != 0)
        result.evidenceGaps |= EvidenceGap_DigitalFallback;
    if (snapshot.curveMutationTicks != 0)
        result.evidenceGaps |= EvidenceGap_CurveMutation;

    const std::uint64_t observedTicks = snapshot.eligibleTicks >
        UINT64_MAX - snapshot.unavailableTicks
        ? UINT64_MAX
        : snapshot.eligibleTicks + snapshot.unavailableTicks;
    const std::uint64_t unavailableLimit = observedTicks / 100u *
        policy.maximumUnavailablePercent +
        ((observedTicks % 100u) * policy.maximumUnavailablePercent + 99u) /
            100u;
    if (snapshot.unavailableTicks > unavailableLimit)
        result.evidenceGaps |= EvidenceGap_ExcessUnavailable;

    result.verdict = result.evidenceGaps == EvidenceGap_None
        ? Verdict::Pass : Verdict::Incomplete;
    return result;
}
}
