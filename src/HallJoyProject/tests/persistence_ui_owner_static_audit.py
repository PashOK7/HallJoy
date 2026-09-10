#!/usr/bin/env python3
"""Keep all interactive binding saves on the UI persistence hand-off path."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hall = root / "HallJoy"
app = (hall / "app.cpp").read_text(encoding="utf-8")
panel = (hall / "keyboard_bind_panel.cpp").read_text(encoding="utf-8")
ui = (hall / "keyboard_ui.cpp").read_text(encoding="utf-8")
header = (hall / "keyboard_ui.h").read_text(encoding="utf-8")

assert "static bool SaveSettingsByActiveGlobalProfile()" in app
assert "const bool saved = activeMarkerSaved && overlaySaved && profileSaved && windowSaved;" in app
assert "return saved;" in app
assert panel.count("KeyboardUI_SaveBindingsAfterUserChange(parent)") == 2
assert "GlobalProfiles_Save(GlobalProfiles_GetActiveName()); // autosave" not in panel
for source in (ui, header):
    assert "KeyboardUI_SaveBindingsAfterUserChange" in source
assert "GlobalProfiles_SetDirty(true);" in ui
assert "PostMessageW(root, WM_APP + 1, 0, 0);" in ui
print("Persistence UI owner static audit passed")
