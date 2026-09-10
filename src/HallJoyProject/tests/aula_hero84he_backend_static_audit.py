#!/usr/bin/env python3
"""Regression guard for the frozen, explicitly opt-in AULA HERO84 HE route."""

from pathlib import Path
import re

PROJECT = Path(__file__).resolve().parents[1]
HALL = PROJECT / "HallJoy"

source = (HALL / "aula_hero84he_backend.cpp").read_text(encoding="utf-8")
header = (HALL / "aula_hero84he_backend.h").read_text(encoding="utf-8")
protocol = (HALL / "aula_hero84he_diagnostic_protocol.cpp").read_text(encoding="utf-8")
routing = (HALL / "native_analog_routing.h").read_text(encoding="utf-8")
catalog = (HALL / "native_analog_backends.def").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")


def has(pattern: str, text: str = source) -> bool:
    return re.search(pattern, text) is not None

checks = {
    "separate production descriptor": "AulaHero84He_GetNativeBackendDescriptor" in header and "aula-hero84he-9402-experimental" in source,
    "independent routing identity": "AulaHero84He = 12" in routing and "NativeAnalogProtocol::AulaHero84He" in source,
    "ordinary catalog excludes frozen route": (
        "#if defined(HALLJOY_AULA_HERO84HE_EXPERIMENTAL)\n"
        "HALLJOY_NATIVE_BACKEND(AulaHero84He_GetNativeBackendDescriptor)\n"
        "#endif" in catalog),
    "MSVC keeps shared parser and opt-in production module": all(marker in project for marker in (
        '<ClCompile Include="aula_hero84he_backend.cpp">',
        '<ClCompile Include="aula_hero84he_diagnostic_protocol.cpp" />')),
    "production module is excluded unless explicit test target is selected": (
        "'$(HallJoyAulaHero84HeExperimental)'!='true'" in project),
    "isolated experimental target enables the route": all(marker in project for marker in (
        "HallJoyAulaHero84HeExperimental", "HallJoy-AULA-HERO84HE-Experimental",
        "HALLJOY_AULA_HERO84HE_EXPERIMENTAL;HALLJOY_PRODUCTION;HALLJOY_STABILITY_TRACE")),
    "exact interface fingerprint": all(marker in source for marker in (
        "hero::kVendorId", "hero::kProductId", "hero::kUsagePage", "hero::kUsage",
        "hero::kReportId", "hero::kReportBytes", "HasId")),
    "identity before routing claim": source.index("Identity(s)") < source.index("NativeAnalogRouting_Claim"),
    "exact UUID admission": has(r"kExpectedUuid\s*\{\{0x11,\s*0,\s*0,\s*0,\s*0,\s*0x05\}\}") and has(r"uuid\s*==\s*kExpectedUuid"),
    "live map is read-only 83": "BuildAssignmentRead(0" in source and "ParseAssignmentResponse" in source,
    "macro/internal values fail closed": has(r"\(a\.value\s*&\s*0xffffff00u\)\s*!=\s*0") and "nextHas[hid]" in source,
    "only approved transmit builders": all(marker in source for marker in (
        "BuildIdentityRead", "BuildAssignmentRead", "BuildDirectRead")),
    "forbidden builders absent": all(marker not in source + protocol for marker in (
        "Build(0x94, 0x00", "Build(0x94, 0x03", "Build(0x94, 0x04",
        "Build(0x94, 0x05", "Build(0x98", "HidD_SetFeature", "HidD_SetOutputReport")),
    "selected-key one-request loop": has(r"Plan\(\s*&positions\s*\)") and has(r"s\.Exchange\(q,\s*&r,\s*&us\)") and has(r"next\s*\+=\s*std::chrono::milliseconds\(1\)"),
    "freshness fails neutral": "kFreshMs = 750" in source and has(r"now\s*-\s*sample\s*<=\s*kFreshMs"),
    "adaptive range is observed": "g_top" in source and "g_bottom" in source and has(r"top\s*>\s*bottom\s*\+\s*32"),
    "bounded stop cancels active I/O": has(r"CancelIoEx\(g_active,\s*nullptr\)") and "kStopTimeoutMs = 3000" in source,
    "read-only descriptor flags": has(r"NativeAnalogBackendFlag_PolledTransport\s*\|\s*NativeAnalogBackendFlag_ReadOnlyProbe"),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("AULA HERO84 HE PRODUCTION STATIC AUDIT FAILED\n" + "\n".join(f" - {name}" for name in failed))
print("AULA HERO84 HE FROZEN-ROUTE STATIC AUDIT PASSED")
print("ordinary builds exclude it; opt-in target admission: exact 372E:103E/FF60:0061/report-09/UUID")
