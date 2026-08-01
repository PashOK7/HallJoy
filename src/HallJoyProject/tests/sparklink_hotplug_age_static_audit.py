#!/usr/bin/env python3
"""Verify SparkLink freshness arithmetic cannot underflow into a false restart."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HALL = ROOT / "src" / "HallJoyProject" / "HallJoy"
TESTS = ROOT / "src" / "HallJoyProject" / "tests"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


header = (HALL / "sparklink_hotplug_age.h").read_text(encoding="utf-8-sig")
backend = (HALL / "backend_sparklink.inc").read_text(encoding="utf-8-sig")
test = (TESTS / "sparklink_hotplug_age_test.cpp").read_text(encoding="utf-8-sig")
runner = (ROOT / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8-sig")
build = (ROOT / "tools" / "build.ps1").read_text(encoding="utf-8-sig")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8-sig")

require("observedNowMs < lastPacketMs" in header and "return 0;" in header,
        "a timestamp observed before a concurrent packet update saturates to zero age")
require("FreshnessAgeMs" in header and "IsPacketStale" in header and
        "> staleAfterMs" in header,
        "normal age and the existing strict stale threshold remain explicit")
require('#include "sparklink_hotplug_age.h"' in backend and
        "FreshnessAgeMs(nowMs, lastPacket)" in backend and
        "nowMs - lastPacket" not in backend,
        "production hotplug monitoring uses only the saturating helper")
require("maximum - 15" in test and "IsPacketStale" in test,
        "portable regression covers the observed high-bit underflow shape")
require("sparklink_hotplug_age_test.cpp" in runner,
        "the unified portable runner executes the freshness regression")
require("sparklink_hotplug_age.h" in project and "sparklink_hotplug_age.h" in build and
        "sparklink_hotplug_age_test.cpp" in build,
        "MSVC project and official build require the regression assets")

print("SPARKLINK_HOTPLUG_AGE_STATIC_AUDIT=PASS")
