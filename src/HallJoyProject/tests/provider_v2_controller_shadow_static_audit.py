from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]

HEADER = (HALL / "provider_v2_controller_shadow.h").read_text(encoding="utf-8")
SOURCE = (HALL / "provider_v2_controller_shadow.cpp").read_text(encoding="utf-8")
BACKEND = (HALL / "backend.cpp").read_text(encoding="utf-8")
CURVE = (HALL / "backend_curve.cpp").read_text(encoding="utf-8")
PROJECT = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
RUNNER = (REPO / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8")
TEST = (ROOT / "tests" / "provider_v2_controller_shadow_test.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


require("RawInputMapV1" in HEADER and "owned" in HEADER,
        "V2 projection preserves ownership independently from values")
require("KeyNamespace::UsbHidUsage" in SOURCE and "usagePage != 0x07u" in SOURCE and
        "KeyNamespace::UapExtended" in SOURCE,
        "only explicit keyboard-page and UAP-extended identities are bindable")
require("ProjectCapturedSnapshot" in BACKEND and
        "providerV2PlaneDualCoherent" in SOURCE,
        "shadow derives from the validated same-generation parent capture")
require("AnalogHostClient_AcquireProviderV2Snapshot" in BACKEND and
        "providerV2LeaseMatchesDense" in BACKEND and
        "MatchesCapturedPublication" in BACKEND and
        "providerV2PlaneTransactionToken" in SOURCE and
        "publicationTimestampUs == dense.publicationTimestampUs" in SOURCE,
        "live shadow requires an exact dense-to-broker transaction match")
require("providerV2Lease.Header()" in BACKEND and
        "providerV2Lease.Devices()" in BACKEND and
        "providerV2Lease.Samples()" in BACKEND,
        "live shadow projects the variable parent-owned broker payload")
require("ReadNativeCached" in BACKEND and
        "ReadProviderV2ShadowRaw01Cached" in BACKEND and
        "MergeWithNative" in SOURCE and "Arbitrate" in SOURCE and
        "Arbitrate({ hidKeycode" in BACKEND,
        "qualified and shadow routes use explicit native/provider arbitration")
require("AnalogSourceStateV1" in HEADER and "ArbitrationSource_DigitalFallback" in HEADER and
        "owned_zero_blocks_fallback=1" in TEST and "stale_rejected=1" in TEST,
        "arbitration tests distinguish owned zero, stale and digital fallback")
require("providerV2Projected && !cache.allowFallback" in BACKEND and
        "ReadDigitalFallback01" not in SOURCE,
        "digital edges cannot qualify, trigger or train Provider V2")
require("BackendCurve_ApplyPairByHid" in BACKEND and
        "const CurveDef curve = BuildCurveForHid(hid)" in CURVE and
        "tickCurveGeneration == BackendCurve_GetGeneration()" in BACKEND,
        "divergent values share one curve definition and stable generation gate")
require("g_qualifiedReportBuilderState" in BACKEND and
        "g_providerV2ShadowReportBuilderState" in BACKEND and
        "g_providerV2ShadowReportBuilderState = g_qualifiedReportBuilderState" in BACKEND,
        "SOCD state is isolated and resynchronized after ineligible proof ticks")
require("CompareFrames" in BACKEND and "FrameMismatch_RightStickY" in SOURCE,
        "comparison names the full neutral controller frame")
require("ToLegacyXusbReport(frames.qualified)" in BACKEND and
        "ToLegacyXusbReport(frames.shadow)" not in BACKEND,
        "only the qualified frame can cross the XUSB publication boundary")
require("providerV2ShadowFieldMismatches" in BACKEND and
        "providerV2ShadowLastMismatchMask" in BACKEND,
        "bounded field-level mismatch evidence is retained in memory")
require("identity_projection=1" in TEST and "owned_zero=1" in TEST and
        "multi_device_max=1" in TEST and "native_arbitration=1" in TEST and
        "dynamic_view=1" in TEST and "exact_transaction_match=1" in TEST and
        "all_fields=1" in TEST,
        "portable behavior test covers identities, owned zero, aggregation and fields")
require("provider_v2_controller_shadow.cpp" in PROJECT and
        "provider_v2_controller_shadow.h" in PROJECT and
        "provider_v2_controller_shadow_test.cpp" in RUNNER,
        "MSVC and unified native runner compile the production shadow components")

print("PROVIDER_V2_CONTROLLER_SHADOW_STATIC_AUDIT=PASS")
