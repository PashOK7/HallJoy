#!/usr/bin/env python3
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[1]
HALL = PROJECT / "HallJoy"
source = (HALL / "aula_hero84he_diagnostic_backend.cpp").read_text(encoding="utf-8")
protocol = (HALL / "aula_hero84he_diagnostic_protocol.cpp").read_text(encoding="utf-8")
protocol_header = (HALL / "aula_hero84he_diagnostic_protocol.h").read_text(encoding="utf-8")
header = (HALL / "native_analog_routing.h").read_text(encoding="utf-8")
catalog = (HALL / "native_analog_backends.def").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
app = (HALL / "app.cpp").read_text(encoding="utf-8")
builder = (ROOT / "tools" / "build_aula_hero84he_diagnostic.ps1").read_text(encoding="utf-8")

checks = {
    "exact HERO84 vendor interface": all(x in protocol_header for x in (
        "kVendorId = 0x372E", "kProductId = 0x103E", "kUsagePage = 0xFF60",
        "kUsage = 0x0061", "kReportId = 0x09", "kReportBytes = 64")),
    "bidirectional report-ID proof": "HasReportId" in source and "reportId9" in source,
    "checksum and bounded builders": all(x in protocol + protocol_header for x in (
        "BuildIdentityRead", "BuildAssignmentRead", "BuildDirectRead",
        "HasValidChecksum", "kMaxPositions = 9", "UniquePositions")),
    "official identity and assignment only": "Build(0x82, 0x01" in protocol and "Build(0x83, layer" in protocol,
    "direct candidate explicitly marked": "firmware-derived-read-only-candidate" in source,
    "forbidden command builders absent": all(x not in protocol for x in (
        "Build(0x94, 0x00", "Build(0x94, 0x04", "Build(0x94, 0x05", "Build(0x98")),
    "no feature or configuration API": "HidD_SetFeature" not in source and "HidD_SetOutputReport" not in source,
    "one outstanding staged request": "outstanding_limit=1" in source and "Exchange(session, request" in source,
    "active traffic waits for raw input": all(x in source for x in (
        "waiting_for_raw_input_registration", "g_rawInputReady", "raw_input_not_ready")),
    "strict packet correlation": all(x in protocol for x in (
        "CorrelatePositions", "expectedCount * 6", "current & 0x7fffu")),
    "uuid before route claim": source.index("ParseIdentityResponse") < source.index("NativeAnalogRouting_Claim"),
    "no gameplay ownership": "bool Owns(std::uint16_t) { return false; }" in source,
    "target scoped raw input": "VID_372E&PID_103E" in source and "AulaHero84HeDiagnostic_RecordRawKeyboardEvent" in app,
    "dedicated diagnostic catalog": "#elif defined(HALLJOY_AULA_HERO84HE_DIAGNOSTIC)" in catalog,
    "diagnostic catalog excludes all foreign protocols": (
        "#elif defined(HALLJOY_AULA_HERO84HE_DIAGNOSTIC)\n"
        "HALLJOY_NATIVE_BACKEND(AulaHero84HeDiagnostic_GetNativeBackendDescriptor)\n"
        "#elif" in catalog),
    "independent routing type": "AulaHero84HeDiagnostic = 11" in header,
    "isolated build target": all(x in project for x in (
        "HallJoyAulaHero84HeDiagnostic", "HallJoy-AULA-HERO84HE-Diagnostic",
        "HALLJOY_AULA_HERO84HE_DIAGNOSTIC")),
    "builder selects test target": "HallJoyAulaHero84HeDiagnostic=true" in builder,
    "diagnostic does not compile MAD68 backend": "HALLJOY_MAD68PR_NATIVE;HALLJOY_AULA_HERO84HE_DIAGNOSTIC" not in project,
}
failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("AULA HERO84 HE DIAGNOSTIC STATIC AUDIT FAILED\n" + "\n".join(f" - {name}" for name in failed))
print("AULA HERO84 HE DIAGNOSTIC STATIC AUDIT PASSED")
print("active allow-list: 82/01, 83, firmware-derived 94/02; no input ownership")
