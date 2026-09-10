#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"

backend = (HALL / "backend.cpp").read_text(encoding="utf-8")
bindings = (HALL / "bindings.cpp").read_text(encoding="utf-8")
profile = (HALL / "profile_ini.cpp").read_text(encoding="utf-8")
layout = (HALL / "keyboard_layout.cpp").read_text(encoding="utf-8")
ui = (HALL / "keyboard_ui.cpp").read_text(encoding="utf-8")
page = (HALL / "keyboard_page_main.cpp").read_text(encoding="utf-8")
protocol = (HALL / "drunkdeer_protocol.cpp").read_text(encoding="utf-8")

for marker in (
    "halljoy::keycode::kFn",
    "halljoy::keycode::kOem1",
    "halljoy::keycode::kCount",
    "halljoy::keycode::kMaskChunkCount",
):
    assert marker in backend + bindings + profile + layout + ui + page + protocol, marker

assert "!native.owned || halljoy::keycode::IsStandardHid(hidKeycode)" in backend
assert "NativeAnalogBackends_ReadMilli(hidKeycode)" in backend
assert "consider(halljoy::keycode::kOem1)" in backend
assert "consider(halljoy::keycode::kFn)" in backend
assert "Bindings_GetButtonMaskChunkCount()" in bindings
assert "halljoy::ini::Unsigned" in profile and "halljoy::keycode::kCount - 1" in profile
assert 'L"DrunkDeer G65 ANSI"' in layout
assert 'L"Fn",halljoy::keycode::kFn' in layout
assert 'L"Menu",halljoy::keycode::kOem1' in layout

print("extended key-code static audit passed")
