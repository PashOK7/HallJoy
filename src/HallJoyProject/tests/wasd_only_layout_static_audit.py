#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LAYOUT = (ROOT / "HallJoy" / "keyboard_layout.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


array_start = LAYOUT.find("static const KeyDef g_wasdOnlyKeys[]")
array_end = LAYOUT.find("static const KeyDef g_generic100Keys[]", array_start)
require(array_start >= 0 and array_end > array_start,
        "WASD-only built-in has an independent key array")

body = LAYOUT[array_start:array_end]
expected = (
    '{L"W", 26, 0, 48, 42}',
    '{L"A",  4, 1,  0, 42}',
    '{L"S", 22, 1, 48, 42}',
    '{L"D",  7, 1, 96, 42}',
)
require(body.count('{L"') == 4, "WASD-only contains exactly four keys")
for entry in expected:
    require(entry in body, f"WASD-only contains {entry[3]} with the correct HID and geometry")

builtins_start = LAYOUT.find("static const PresetDef g_builtinPresets[]")
builtins_end = LAYOUT.find("static std::vector<PresetStore> g_presets", builtins_start)
builtins = LAYOUT[builtins_start:builtins_end]
wasd_entry = '{ L"WASD Only", g_wasdOnlyKeys'
require(builtins.count(wasd_entry) == 1,
        "WASD-only ships once as a selectable built-in preset")
require('L"DrunkDeer A75 Pro", g_drunkdeer_A75Pro' in builtins,
        "the historical A75 Pro preset remains shipped")
require('L"DrunkDeer G65 ANSI", g_drunkdeer_G65' in builtins,
        "the complete G65 preset remains shipped")
require(builtins.find('L"DrunkDeer A75 Pro", g_drunkdeer_A75Pro') <
        builtins.find('L"DrunkDeer G65 ANSI", g_drunkdeer_G65') <
        builtins.find(wasd_entry),
        "A75 Pro remains preset zero while G65 and WASD-only stay optional")
require("ActivatePreset(0);" in LAYOUT,
        "preset zero remains the startup default")

print("WASD_ONLY_LAYOUT_STATIC_AUDIT=PASS")
