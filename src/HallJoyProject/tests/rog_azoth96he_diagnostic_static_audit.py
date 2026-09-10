#!/usr/bin/env python3
"""Fail closed if the unbuilt ROG M901 diagnostic loses its safe boundary."""

from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[1]
HALL = PROJECT / "HallJoy"

source = (HALL / "rog_azoth96he_diagnostic_backend.cpp").read_text(encoding="utf-8")
catalog = (HALL / "native_analog_backends.def").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
app = (HALL / "app.cpp").read_text(encoding="utf-8")
backend = (HALL / "backend.cpp").read_text(encoding="utf-8")

checks = {
    "exact ASUS M901 identity and paired interfaces": all(value in source for value in (
        "kVendorId = 0x0B05", "kProductId = 0x1C10",
        "kControlUsagePage = 0xFF00", "kEventUsagePage = 0xFFC0",
        "kControlReportBytes = 64", "kEventReportBytes = 21")),
    "normal-mode travel enable only": (
        "report[0] = 0x51;" in source and "report[1] = 0x61;" in source),
    "calibration command is absent from emitted data": (
        "report[0] = 0x80;" not in source and "report[1] = 0x26;" not in source),
    "calibration is explicitly prohibited": "calibration opcode 80 26" in source,
    "event decoder is exact and bounded": all(value in source for value in (
        "report[0] != kEventReportId", "report[1] != kTravelEvent",
        "static_cast<std::uint16_t>(report[2])", "static_cast<std::uint16_t>(report[4])")),
    "diagnostic has no gameplay ownership": (
        "bool Owns(std::uint16_t) { return false; }" in source and
        "std::uint16_t Get(std::uint16_t) { return 0; }" in source),
    "one-backend diagnostic catalog": (
        "#if defined(HALLJOY_ROG_AZOTH96HE_DIAGNOSTIC)\n"
        "HALLJOY_NATIVE_BACKEND(RogAzoth96HeDiagnostic_GetNativeBackendDescriptor)\n"
        "#elif" in catalog),
    "paired interface claims fail atomically": (
        "const bool controlClaimed" in source and
        "const bool eventClaimed = controlClaimed" in source and
        "if (!claimed && controlClaimed)" in source and
        "NativeAnalogRouting_Reset();" in source),
    "isolated build target": all(value in project for value in (
        "HallJoyRogAzoth96HeDiagnostic", "HallJoy-ROG-Azoth96HE-Diagnostic",
        "HALLJOY_ROG_AZOTH96HE_DIAGNOSTIC")),
    "catalog routing is active for the diagnostic": "defined(HALLJOY_ROG_AZOTH96HE_DIAGNOSTIC)" in app,
    "transport-only backend skips UAP and ViGEm": (
        "defined(HALLJOY_ROG_AZOTH96HE_DIAGNOSTIC)" in backend and
        "diagnostic.transport_only" in backend),
    "no test builder or package is claimed": (
        "build_rog_azoth" not in "\n".join(path.name for path in (ROOT / "tools").glob("*")) and
        not any("ROG" in path.name.upper() or "AZOTH" in path.name.upper()
                for path in (ROOT / "build" / "release").glob("*"))),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("ROG AZOTH 96 HE DIAGNOSTIC STATIC AUDIT FAILED\n" +
                     "\n".join(f" - {name}" for name in failed))

print("ROG AZOTH 96 HE DIAGNOSTIC STATIC AUDIT PASSED")
print("active transport: FF00 51/61 and FFC0 report 03/7E; no calibration, UAP, ViGEm, or gameplay ownership")
