from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]

backend = (HALL / "backend.cpp").read_text(encoding="utf-8")
header = (HALL / "backend.h").read_text(encoding="utf-8")
main = (HALL / "main.cpp").read_text(encoding="utf-8")
model = (HALL / "provider_v2_qualification_model.cpp").read_text(
    encoding="utf-8")
report = (HALL / "provider_v2_qualification_report.cpp").read_text(
    encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
runner = (REPO / "tools" / "run_native_backend_checks.py").read_text(
    encoding="utf-8")
builder = (REPO / "tools" / "build_provider_v2_qualification.ps1").read_text(
    encoding="utf-8")
smoke = (REPO / "tools" / "run_provider_v2_qualification_smoke.ps1").read_text(
    encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


require("providerV2ShadowBackendInitCount" in header and
        "providerV2ShadowUniqueSampleGenerations" in header and
        "providerV2ShadowConfiguredFieldMask" in header and
        "providerV2ShadowActivatedFieldMask" in header and
        "providerV2ShadowReleasedFieldMask" in header,
        "telemetry exposes generation, activity and release evidence")
require("g_providerV2ShadowBackendInitCount.fetch_add(1" in backend,
        "every backend reinitialisation is counted")
for forbidden_reset in (
        "g_providerV2ShadowEligibleTicks.store(0",
        "g_providerV2ShadowMatchedReports.store(0",
        "g_providerV2ShadowMismatchedReports.store(0",
        "g_providerV2ShadowFieldMismatches[field].store(0"):
    require(forbidden_reset not in backend,
            f"process evidence is never erased: {forbidden_reset}")
require("providerActiveFieldMask" in backend and
        "shadowTick.providerRaw.owned.test(hid)" in backend and
        "value >= 0.5f" in backend and
        "ActiveFieldMask(pair.shadow)" in backend and
        "providerDeepTravelFieldMask & shadowActive" in backend and
        "providerNeutralFieldMask" in backend and
        "MouseBind_IsPseudoHid(hid)" in backend and
        "g_providerV2ShadowReleasedFieldMask.fetch_or" in backend and
        "g_providerV2ShadowConfiguredFieldMask.fetch_or" in backend,
        "coverage derives only from Provider V2 raw ownership and travel")
require("coverageShift" in backend and "index) * fieldsPerPad" in backend and
        "kAllCoverageMask" in model,
        "coverage remains independent for every field on every virtual pad")
require("UpdateReleaseTracker" in backend and
        "g_providerV2ShadowPendingReleaseMasks" in backend and
        "releasedNowMask" in model and
        "ActiveFieldMask(pair.qualified)" not in backend and
        "pair.configuredFieldMask" not in backend,
        "deep V2 travel plus shadow output is latched until proven neutral")
require("previousSample != sampleGeneration" in backend and
        "g_providerV2ShadowUniqueSampleGenerations.fetch_add" in backend,
        "repeated realtime ticks cannot impersonate unique UAP generations")
require("mismatch" in model and "Verdict::Fail" in model and
        "EvidenceGap_NoConfiguredFields" in model and
        "EvidenceGap_FieldsNotReleased" in model and
        "EvidenceGap_BackendGeneration" in model,
        "pure verdict model fails closed against mismatch and empty evidence")
require("result.eligibleDurationMs < policy.minimumDurationMs" not in model and
        "schema=2" in report and "duration_is_informational=1" in report,
        "arbitrary wall-clock duration cannot gate equality qualification")
require(main.index("ProviderV2QualificationReport_Begin()") <
        main.index("App_Run(hInst, nCmdShow)"),
        "qualification starts before ordinary application execution")
require(main.index("App_RequiresImmediateProcessExit()") <
        main.index("ProviderV2QualificationReport_Finalize(result)"),
        "poisoned shutdown cannot receive a finalized qualification report")
require(main.index("ShutdownDebugLogSafely()", main.index("App_Run(hInst, nCmdShow)")) <
        main.index("ProviderV2QualificationReport_Finalize(result)"),
        "qualification finalizes only after successful logger shutdown")
require("#if defined(HALLJOY_PROVIDER_V2_QUALIFICATION)" in report and
        "MoveFileExW" in report and "MOVEFILE_WRITE_THROUGH" in report and
        "DeleteFileW(finalPath.c_str())" in report and
        "finalized=%u" in report and
        "legacy_dense_shadow_submitted_to_vigem=0" in report,
        "opt-in report is transactional, bounded and names legacy isolation")
require("coverage_source=provider_v2_raw_gated_legacy_shadow" in report and
        "activation_policy=provider_raw_at_least_0.5_and_shadow_non_neutral" in report and
        "release_policy=latched_until_provider_raw_zero_and_shadow_neutral" in report,
        "report makes the V2-versus-legacy coverage source explicit")
require("production_route=provider_v2_authoritative" in report,
        "report identifies Provider V2 as the actual production route")
require("coverage_mask_layout=bit_(pad_index*7+field_index)" in report,
        "report defines its per-pad coverage mask")
require("WriteAtomic" not in backend and "CreateFileW" not in backend,
        "realtime backend performs no qualification file I/O")
require("HallJoyProviderV2Qualification" in project and
        "HALLJOY_PROVIDER_V2_QUALIFICATION" in project and
        "provider_v2_qualification_model.cpp" in project and
        "provider_v2_qualification_report.cpp" in project,
        "MSVC project owns the opt-in build and both qualification modules")
require("provider_v2_qualification_model_test.cpp" in runner,
        "unified portable runner executes the fail-closed verdict model")
require("HallJoyProviderV2Qualification=true" in builder and
        "HallJoyProviderV2Qualification.txt" in builder and
        "run_provider_v2_qualification_smoke.ps1" in builder,
        "dedicated builder validates the exact single-EXE artifact")
require("verdict=INCOMPLETE" in smoke and "verdict=PASS" in smoke and
        "HallJoyProviderV2Qualification.tmp" in smoke,
        "short idle smoke rejects false PASS and leaked temporary state")

print("PROVIDER_V2_QUALIFICATION_STATIC_AUDIT=PASS")
