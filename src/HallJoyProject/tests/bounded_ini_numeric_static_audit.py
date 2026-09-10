#!/usr/bin/env python3
"""Require persisted numeric loaders to use the strict bounded parser."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hall = root / "HallJoy"
bounded = (hall / "bounded_ini.h").read_text(encoding="utf-8")
settings = (hall / "settings_ini.cpp").read_text(encoding="utf-8")
profiles = (hall / "keyboard_profiles.cpp").read_text(encoding="utf-8")
layout = (hall / "keyboard_layout.cpp").read_text(encoding="utf-8")
bindings = (hall / "profile_ini.cpp").read_text(encoding="utf-8")

for token in ("inline bool Signed(", "inline bool ReadUnsigned(", "inline bool ReadSigned(",
              "FILE_FLAG_OPEN_REPARSE_POINT", "GetFileInformationByHandle"):
    assert token in bounded, token
for source in (settings, profiles, layout):
    assert "GetPrivateProfileIntW" not in source
assert "IniReadI32" in settings and "halljoy::ini::ReadSigned" in settings
assert "ReadM01" in profiles and "halljoy::ini::ReadSigned" in profiles
assert "halljoy::ini::ReadFile inputFile(path.c_str());" in profiles
assert "ReadOptionalLayoutInteger" in layout and "halljoy::ini::ReadSigned" in layout
assert "_wtoi" not in bindings
assert "halljoy::ini::Unsigned(pads, BINDINGS_MAX_GAMEPADS, parsedPads)" in bindings
print("Bounded INI numeric static audit passed")
