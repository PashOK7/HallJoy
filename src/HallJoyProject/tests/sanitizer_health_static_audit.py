#!/usr/bin/env python3
"""Keep the sanitizer health control distinct from the passing parser corpus."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
RUNNER = (ROOT / "tools/run_protocol_fuzz_sanitizers.py").read_text(encoding="utf-8-sig")
AULA_RUNNER = (ROOT / "tools/run_aula_win60he_sanitizers.py").read_text(encoding="utf-8-sig")
HELPER = (ROOT / "tools/sanitizer_health.py").read_text(encoding="utf-8-sig")
CONTROL = (ROOT / "src/HallJoyProject/tests/sanitizer_health_control_test.cpp").read_text(encoding="utf-8-sig")

checks = {
    "health control contains a volatile heap overflow": "volatile auto* bytes" in CONTROL and "bytes[1]" in CONTROL,
    "both sanitizer runners invoke the shared health control": "require_address_sanitizer_health(" in RUNNER and "require_address_sanitizer_health(" in AULA_RUNNER,
    "health control compiles with the runner sanitizer flags": "sanitizer_health_control_test.cpp" in HELPER and "-fsanitize=address,undefined" in RUNNER and "-fsanitize=address,undefined" in AULA_RUNNER,
    "health control requires a failing process": "health.returncode == 0" in HELPER,
    "health control requires an AddressSanitizer diagnostic": '"AddressSanitizer" not in health.stdout' in HELPER,
    "normal protocol fuzz remains a separate passing stage": "PROTOCOL_FUZZ_SANITIZERS=PASS" in RUNNER,
}
failed = [name for name, passed in checks.items() if not passed]
if failed:
    print("SANITIZER_HEALTH_STATIC_AUDIT=FAIL", file=sys.stderr)
    for name in failed:
        print(" - " + name, file=sys.stderr)
    raise SystemExit(1)
for name in checks:
    print("PASS: " + name)
print("SANITIZER_HEALTH_STATIC_AUDIT=PASS")
