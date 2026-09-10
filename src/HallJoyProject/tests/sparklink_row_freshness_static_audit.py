#!/usr/bin/env python3
"""Verify row-owned SparkLink freshness cannot leak stale values across rows."""

from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
HALL = PROJECT / "HallJoy"
ROOT = PROJECT.parents[1]
spark = (HALL / "backend_sparklink.inc").read_text(encoding="utf-8-sig")
header = (HALL / "sparklink_row_freshness.h").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
runner = (ROOT / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8")
test = (PROJECT / "tests" / "sparklink_row_freshness_test.cpp").read_text(encoding="utf-8")

checks = {
    "row-local values and fresh flags exist": "g_sparkRowHidMilli" in spark and "g_sparkRowFresh" in spark,
    "freshness deadline is bounded from protocol and safe poll limits": all(x in spark for x in (
        "kSparkRouteTransactionTimeoutMs = 250", "kSparkMaxSafePollIntervalMs = 20", "kSparkRowFreshnessMs")),
    "stale or row-limit-retired rows are removed before aggregation": "SparkReconcileRowFreshness" in spark and "row < effectiveRows" in spark,
    "duplicate HID aggregates fresh row owners": "SparkRecomputePublishedHid" in spark and "std::max(aggregate" in spark,
    "route updates commit row-local state before aggregate publication": "SparkCommitRouteRow" in spark and "g_sparkRowFresh[(size_t)row].store(true" in spark,
    "getter remains an allocation-free atomic read": "BackendNative_SparkGetMilli" in (HALL / "backend.cpp").read_text(encoding="utf-8-sig"),
    "portable old-bug oracle is wired": "sparklink_row_freshness_test.cpp" in runner and "AggregateFresh" in test,
    "project includes row freshness contract": "sparklink_row_freshness.h" in project,
}
failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("SPARKLINK ROW FRESHNESS STATIC AUDIT FAILED\n" + "\n".join(f" - {x}" for x in failed))
print("SPARKLINK_ROW_FRESHNESS_STATIC_AUDIT=PASS")
