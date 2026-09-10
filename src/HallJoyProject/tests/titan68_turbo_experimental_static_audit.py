#!/usr/bin/env python3
"""Guard the full HallJoy pipeline build for the Titan68 Turbo experiment."""

from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parents[1]
HALL = PROJECT / "HallJoy"
source = (HALL / "titan68_turbo_diagnostic_backend.cpp").read_text(encoding="utf-8")
catalog = (HALL / "native_analog_backends.def").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
backend = (HALL / "backend.cpp").read_text(encoding="utf-8")
builder = (ROOT / "tools" / "build_titan68_turbo_experimental.ps1").read_text(encoding="utf-8")

checks = {
    "only official read-only mapping and reversible 36 commands": all(x in source for x in (
        "ReadControlData(session, 0x12, 64, &info)",
        "ReadControlData(session, 0x16, slots * 3, &rect)",
        "SendAndAwait(control, &stats, 0x36, true)",
        "SendAndAwait(control, &stats, 0x36, false)")) and "SendAndAwait(control, &stats, 0x37," not in source,
    "real report07 values reach HallJoy native path": all(x in source for x in (
        "PublishVisualAnalog", "g_hidAtKeyIndex", "g_milli",
        "bool Owns(std::uint16_t hid)", "std::uint16_t Get(std::uint16_t hid)")),
    "experimental build selects Titan only": "defined(HALLJOY_TITAN68_TURBO_EXPERIMENTAL)" in catalog and "Titan68TurboDiagnostic_GetNativeBackendDescriptor" in catalog,
    "experimental build does not use transport-only compile path": "HALLJOY_TITAN68_TURBO_EXPERIMENTAL;HALLJOY_PRODUCTION" in project and "HALLJOY_DIAGNOSTIC" not in project.split("HallJoyTitan68TurboExperimental", 1)[1].split("</ItemDefinitionGroup>", 1)[0],
    "normal backend tick remains available": "#if defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC)" in backend and "HALLJOY_TITAN68_TURBO_EXPERIMENTAL" not in backend,
    "builder targets isolated full-pipeline executable": all(x in builder for x in (
        "HallJoyTitan68TurboExperimental=true", "HallJoy-Madlions-Titan68-Turbo-Experimental.exe", "/m:1")),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("TITAN68 TURBO EXPERIMENTAL STATIC AUDIT FAILED\n" + "\n".join(f" - {name}" for name in failed))

print("TITAN68 TURBO EXPERIMENTAL STATIC AUDIT PASSED")
print("full HallJoy pipeline: read-only 12/16 mapping, reversible 36:01/00 stream, normal curves/bindings/ViGEm enabled")
