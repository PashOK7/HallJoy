#!/usr/bin/env python3
"""Keep runtime timestamp and raw-input arithmetic saturating at boundaries."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HALL = ROOT / "src" / "HallJoyProject" / "HallJoy"
TESTS = ROOT / "src" / "HallJoyProject" / "tests"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


time_math = (HALL / "monotonic_time.h").read_text(encoding="utf-8-sig")
int_math = (HALL / "saturating_int.h").read_text(encoding="utf-8-sig")
sayo = (HALL / "backend_sayo.inc").read_text(encoding="utf-8-sig")
addressed = (HALL / "addressed_analog_backend.cpp").read_text(encoding="utf-8-sig")
backend = (HALL / "backend.cpp").read_text(encoding="utf-8-sig")
test = (TESTS / "runtime_arithmetic_test.cpp").read_text(encoding="utf-8-sig")
runner = (ROOT / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8-sig")

require("observedNowMs < publishedMs" in time_math and "return 0;" in time_math,
        "concurrently published future timestamps saturate to zero age")
require("observedNowMs >= deadlineMs" in time_math and "RemainingTimeoutMs" in time_math,
        "expired deadlines return zero instead of wrapping to INFINITE")
require("static_cast<std::int64_t>(left)" in int_math and
        "static_cast<std::int64_t>(right)" in int_math,
        "raw-input accumulation widens before addition")
require("IsStale(nowMs, lastPacket" in sayo and "nowMs - lastPacket" not in sayo,
        "Sayo hotplug monitoring cannot underflow into a false restart")
require(addressed.count("RemainingTimeoutMs(nowMs, deadline)") == 2 and
        "deadline - GetTickCount64()" not in addressed,
        "Addressed probe waits cannot wrap into an infinite timeout")
require(backend.count("SaturatingAddInt(old") == 2 and "old + dx" not in backend and
        "old + dy" not in backend,
        "both raw mouse axes use defined saturating addition")
require("u64max" in test and "imax" in test and "imin" in test,
        "portable regression exercises integer boundary values")
require("runtime_arithmetic_test.cpp" in runner,
        "the unified portable runner executes the arithmetic regression")

print("RUNTIME_ARITHMETIC_STATIC_AUDIT=PASS")
