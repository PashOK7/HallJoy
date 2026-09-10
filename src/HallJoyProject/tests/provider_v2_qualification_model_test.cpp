#include "../HallJoy/provider_v2_qualification_model.h"

#include <cassert>
#include <iostream>

using namespace halljoy::provider_v2_qualification;

namespace
{
SnapshotV1 CompleteSnapshot()
{
    SnapshotV1 snapshot{};
    snapshot.available = true;
    snapshot.backendInitCount = 1;
    snapshot.eligibleTicks = 60001;
    snapshot.matchedReports = 60001;
    snapshot.uniqueSampleGenerations = 6000;
    snapshot.firstEligibleTickMs = 1000;
    snapshot.lastEligibleTickMs = 61000;
    snapshot.configuredFieldMask = kAllFieldMask;
    snapshot.activatedFieldMask = kAllFieldMask;
    snapshot.releasedFieldMask = kAllFieldMask;
    return snapshot;
}
}

int main()
{
    halljoy::controller::VirtualControllerFrameV1 frame{};
    frame.buttons = 1;
    frame.leftTrigger = 1;
    frame.rightTrigger = 1;
    frame.leftStickX = 1;
    frame.leftStickY = -1;
    frame.rightStickX = 1;
    frame.rightStickY = -1;
    assert(ActiveFieldMask(frame) == kAllFieldMask);

    const SnapshotV1 empty{};
    const EvaluationV1 emptyResult = Evaluate(empty, 0);
    assert(emptyResult.verdict == Verdict::Incomplete);
    assert((emptyResult.evidenceGaps & EvidenceGap_NoConfiguredFields) != 0);

    SnapshotV1 complete = CompleteSnapshot();
    assert(Evaluate(complete, 0).verdict == Verdict::Pass);
    SnapshotV1 shortComplete = complete;
    shortComplete.lastEligibleTickMs = shortComplete.firstEligibleTickMs + 1;
    assert(Evaluate(shortComplete, 0).verdict == Verdict::Pass);

    constexpr std::uint32_t releaseBit = 1u << 3;
    auto releaseUpdate = UpdateReleaseTracker(0, releaseBit, 0);
    assert(releaseUpdate.pendingMask == releaseBit);
    assert(releaseUpdate.releasedNowMask == 0);
    releaseUpdate = UpdateReleaseTracker(releaseUpdate.pendingMask, 0, 0);
    assert(releaseUpdate.pendingMask == releaseBit);
    assert(releaseUpdate.releasedNowMask == 0);
    releaseUpdate = UpdateReleaseTracker(releaseUpdate.pendingMask, 0,
        releaseBit);
    assert(releaseUpdate.pendingMask == 0);
    assert(releaseUpdate.releasedNowMask == releaseBit);

    SnapshotV1 noRelease = complete;
    noRelease.releasedFieldMask &= ~(1u << 4);
    const EvaluationV1 noReleaseResult = Evaluate(noRelease, 0);
    assert(noReleaseResult.verdict == Verdict::Incomplete);
    assert(noReleaseResult.missingReleasedFieldMask == (1u << 4));

    SnapshotV1 secondPad = complete;
    const std::uint32_t secondPadLeftX = 1u << (kFieldCount + 3u);
    secondPad.configuredFieldMask |= secondPadLeftX;
    secondPad.activatedFieldMask |= secondPadLeftX;
    const EvaluationV1 secondPadResult = Evaluate(secondPad, 0);
    assert(secondPadResult.verdict == Verdict::Incomplete);
    assert(secondPadResult.missingReleasedFieldMask == secondPadLeftX);
    secondPad.releasedFieldMask |= secondPadLeftX;
    assert(Evaluate(secondPad, 0).verdict == Verdict::Pass);

    SnapshotV1 mismatch = complete;
    mismatch.mismatchedReports = 1;
    mismatch.fieldMismatches[3] = 1;
    assert(Evaluate(mismatch, 0).verdict == Verdict::Fail);

    SnapshotV1 reinitialised = complete;
    reinitialised.backendInitCount = 2;
    assert(Evaluate(reinitialised, 0).verdict == Verdict::Incomplete);
    assert((Evaluate(reinitialised, 0).evidenceGaps &
        EvidenceGap_BackendGeneration) != 0);

    SnapshotV1 digitalFallback = complete;
    digitalFallback.digitalFallbackTicks = 1;
    assert(Evaluate(digitalFallback, 0).verdict == Verdict::Incomplete);

    SnapshotV1 excessiveUnavailable = complete;
    excessiveUnavailable.unavailableTicks = 1000;
    assert(Evaluate(excessiveUnavailable, 0).verdict == Verdict::Incomplete);

    SnapshotV1 allowedStartupUnavailable = complete;
    allowedStartupUnavailable.unavailableTicks = 100;
    assert(Evaluate(allowedStartupUnavailable, 0).verdict == Verdict::Pass);

    std::cout << "PROVIDER_V2_QUALIFICATION_MODEL_TEST=PASS"
              << " no_idle_pass=1"
              << " duration_informational=1"
              << " all_fields_exercised=1"
              << " per_pad_coverage=1"
              << " release_required=1"
              << " partial_travel_not_release=1"
              << " mismatch_fails=1"
              << " reset_rejected=1\n";
    return 0;
}
