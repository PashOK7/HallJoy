#!/usr/bin/env python3
"""Fail if a test translation unit loses its declared execution route."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
TESTS = ROOT / "src/HallJoyProject/tests"
PORTABLE_RUNNER = (ROOT / "tools/run_native_backend_checks.py").read_text(encoding="utf-8-sig")
PROFILE_RUNNER = (ROOT / "tools/run_profile_transaction_tests.ps1").read_text(encoding="utf-8-sig")
SANITIZER_RUNNERS = "\n".join(
    (ROOT / runner).read_text(encoding="utf-8-sig")
    for runner in ("tools/run_protocol_fuzz_sanitizers.py", "tools/run_aula_win60he_sanitizers.py")
)
SANITIZER_HELPER = (ROOT / "tools/sanitizer_health.py").read_text(encoding="utf-8-sig")


def route_for(test: Path) -> str | None:
    if test.name == "sanitizer_health_control_test.cpp":
        return "sanitizer-health-only"
    if test.name == "profile_transaction_windows_test.cpp":
        return "isolated-simulator-only"
    if test.name.endswith("_protocol_test.cpp"):
        return "portable-protocol-convention"
    if test.name in PORTABLE_RUNNER:
        return "portable-explicit"
    return None


def main() -> int:
    tests = sorted(TESTS.glob("*_test.cpp"))
    routes = {test.name: route_for(test) for test in tests}
    missing = [name for name, route in routes.items() if route is None]
    checks = {
        "all test translation units have one declared route": not missing,
        "health control is owned by both sanitizer runners through their helper": (
            "require_address_sanitizer_health" in SANITIZER_RUNNERS
            and "sanitizer_health_control_test.cpp" in SANITIZER_HELPER
        ),
        "profile transaction test is owned by the isolated simulator runner": "PROFILE_TRANSACTION_WINDOWS_TEST=PASS" in PROFILE_RUNNER,
        "protocol convention verifies source counterpart existence": "Missing pure protocol source" in PORTABLE_RUNNER,
    }
    for name, route in routes.items():
        print(f"ROUTE: {name} -> {route}")
    failed = [name for name, passed in checks.items() if not passed]
    if failed:
        print("TEST_EXECUTION_COVERAGE_STATIC_AUDIT=FAIL", file=sys.stderr)
        for item in failed:
            print(" - " + item, file=sys.stderr)
        if missing:
            print(" - unassigned: " + ", ".join(missing), file=sys.stderr)
        return 1
    print(f"TEST_EXECUTION_COVERAGE_STATIC_AUDIT=PASS tests={len(tests)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
