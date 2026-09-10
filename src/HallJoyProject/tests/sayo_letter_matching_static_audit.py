#!/usr/bin/env python3
"""Ensure RM-03 uses the tested multi-candidate matcher in production Sayo code."""

from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
HALL = PROJECT / "HallJoy"
ROOT = PROJECT.parents[1]

sayo = (HALL / "backend_sayo.inc").read_text(encoding="utf-8-sig")
matcher = (HALL / "sayo_letter_matcher.h").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
runner = (ROOT / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8")
test = (PROJECT / "tests" / "sayo_letter_matcher_test.cpp").read_text(encoding="utf-8")

checks = {
    "production uses a per-index matcher": (
        '#include "sayo_letter_matcher.h"' in sayo and
        "SayoLetterMatcher g_sayoLetterMatcher" in sayo and
        "g_sayoPendingIndex" not in sayo),
    "physical edges and keyboard matching share a mutex": (
        "std::mutex" in sayo and "g_sayoMappingMutex;" in sayo and
        sayo.count("g_sayoMappingMutex") >= 3),
    "only one live candidate may learn": (
        "TakeUniqueCandidate" in sayo and
        "addedCount == 1" in sayo and
        "g_sayoLetterMatcher.Expire" in sayo),
    "matcher preserves ambiguity and expires candidates": (
        "if (candidate != kNoCandidate)" in matcher and
        "nowMs - since > windowMs" in matcher and
        "pendingSinceMs_[candidate] = kNoTimestamp" in matcher),
    "production project owns the shared matcher": "sayo_letter_matcher.h" in project,
    "portable test runs the production matcher": (
        "sayo_letter_matcher" in runner and
        "CrossReportAmbiguityDoesNotMislearn" in test and
        "old_bug=blocked" in test),
}

failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("SAYO LETTER MATCHING STATIC AUDIT FAILED\n" +
                     "\n".join(f" - {name}" for name in failed))

print("SAYO_LETTER_MATCHING_STATIC_AUDIT=PASS")
