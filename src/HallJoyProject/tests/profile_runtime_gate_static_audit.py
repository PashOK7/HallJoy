#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
gate = (root / "HallJoy" / "profile_runtime_gate.h").read_text(encoding="utf-8-sig")
test = (root / "tests" / "profile_runtime_gate_test.cpp").read_text(encoding="utf-8-sig")

assert "attempt < 3u" in gate
assert "if (state & kWriter) return;" in gate
assert "compare_exchange_weak" in gate
assert "simultaneous" in test
assert "CommitLease timeout" in test
print("PROFILE_RUNTIME_GATE_STATIC_AUDIT=PASS")
