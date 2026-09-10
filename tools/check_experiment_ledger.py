#!/usr/bin/env python3
"""Fail closed if an opt-in HallJoy experiment lacks an RM-35 ledger row.

This is intentionally a source-only audit.  It must not build or execute a
HallJoy target, access HID devices, or infer protocol support.
"""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PROJECT = ROOT / "src" / "HallJoyProject" / "HallJoy" / "HallJoy.vcxproj"
ROG_SOURCE = ROOT / "src" / "HallJoyProject" / "HallJoy" / "rog_azoth96he_diagnostic_backend.cpp"
LEDGER = ROOT / "docs" / "v1.4" / "EXPERIMENT_LEDGER_RM35_2026-09-06.md"

EXPECTED_PROPERTIES = {
    "HallJoyMadlionsDiagnostic",
    "HallJoyAnalogSimulator",
    "HallJoyUiAudit",
    "HallJoyAulaAggressiveTrace",
    "HallJoyDiagnostic",
    "HallJoyIrokNd75Diagnostic",
    "HallJoyDrunkDeerDiagnostic",
    "HallJoyMchoseAce68Diagnostic",
    "HallJoyRogAzoth96HeDiagnostic",
    "HallJoyAulaHero84HeDiagnostic",
    "HallJoyAulaHero84HeExperimental",
    "HallJoyTitan68TurboDiagnostic",
    "HallJoyTitan68TurboExperimental",
    "HallJoyProviderV2Qualification",
}

# These switches belong to the normal MAD68 production image or are shared
# instrumentation toggles selected only by one of the ledgered targets.  They
# are not independent experiments and therefore must not create a duplicate
# ledger row.
NON_EXPERIMENT_PROPERTIES = {
    "HallJoyBuildRoot",
    "HallJoyBuildVariant",
    "HallJoyMad68ProRNative",
    "HallJoySingleLogDiagnostic",
    "HallJoyStabilityTrace",
}


def main() -> int:
    project = PROJECT.read_text(encoding="utf-8")
    ledger = LEDGER.read_text(encoding="utf-8")
    properties = set(re.findall(r"\$\((HallJoy[A-Za-z0-9]+)\)", project))
    missing_from_project = sorted(EXPECTED_PROPERTIES - properties)
    unexpected = sorted(
        property_name
        for property_name in properties
        if property_name.startswith("HallJoy")
        and property_name not in EXPECTED_PROPERTIES
        and property_name not in NON_EXPERIMENT_PROPERTIES
    )
    missing_from_ledger = sorted(
        property_name for property_name in EXPECTED_PROPERTIES if f"`{property_name}`" not in ledger
    )

    failures: list[str] = []
    if missing_from_project:
        failures.append("expected properties absent from project: " + ", ".join(missing_from_project))
    if unexpected:
        failures.append("unledgered HallJoy project properties: " + ", ".join(unexpected))
    if missing_from_ledger:
        failures.append("properties absent from RM-35 ledger: " + ", ".join(missing_from_ledger))
    if "| E-08 | `HallJoyRogAzoth96HeDiagnostic` | Frozen" not in ledger:
        failures.append("ROG row must remain explicitly Frozen")
    if "No build, launch, HID access or distribution while frozen." not in ledger:
        failures.append("ROG frozen safety boundary missing")
    exclusion = "<ExcludedFromBuild Condition=\"'$(HallJoyRogAzoth96HeDiagnostic)'!='true'\">true</ExcludedFromBuild>"
    if exclusion not in project:
        failures.append("ROG source lacks ordinary-build exclusion")
    if not ROG_SOURCE.is_file():
        failures.append("ROG diagnostic source is unexpectedly absent")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print("RM35_EXPERIMENT_LEDGER=PASS")
    print(f"properties={len(EXPECTED_PROPERTIES)}")
    print("rog_status=frozen ordinary_build_excluded=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
