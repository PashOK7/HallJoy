#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hall = root / "HallJoy"
settings = (hall / "key_settings.cpp").read_text(encoding="utf-8-sig")
curve = (hall / "backend_curve.cpp").read_text(encoding="utf-8-sig")
codes = (hall / "analog_key_codes.h").read_text(encoding="utf-8-sig")
runner = (root.parents[1] / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8-sig")

assert "kCount = static_cast<std::size_t>(kFn) + 1u" in codes
assert "std::array<KeyDeadzone, halljoy::keycode::kCount> g_data" in settings
assert "unordered_map" not in settings and "g_mapMutex" not in settings
assert "attempt < 3u" in settings and "SnapshotLoad" in settings
assert "std::array<CurveDef, halljoy::keycode::kCount> curves" in curve
assert "const KeyDeadzone ks = KeySettings_Get(hid);" in curve
assert "key_settings_domain_test.cpp" in runner
print("KEY_SETTINGS_DOMAIN_STATIC_AUDIT=PASS")
