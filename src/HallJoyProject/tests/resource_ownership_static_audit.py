#!/usr/bin/env python3
"""Verify audited Win32/UAP failure paths retain or release ownership correctly."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
HALL = ROOT / "src" / "HallJoyProject" / "HallJoy"
SOUP = ROOT / "third_party" / "UniversalAnalogPluginFixed" / "overlay" / "Soup" / "soup"
SOUP_PATCH = ROOT / "third_party" / "UniversalAnalogPluginFixed" / "tools" / "Apply-Soup-Madlions-Fix.ps1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")


ui = (HALL / "keyboard_subpages.cpp").read_text(encoding="utf-8-sig")
hid = (SOUP / "hwHid.cpp").read_text(encoding="utf-8-sig")
keyboard = (SOUP / "AnalogueKeyboard.cpp").read_text(encoding="utf-8-sig")
patcher = SOUP_PATCH.read_text(encoding="utf-8-sig")

clipboard_begin = ui.index("static void OverlayPage_SetClipboardText")
clipboard_end = ui.index("static void OverlayPage_UpdateColorControls", clipboard_begin)
clipboard = ui[clipboard_begin:clipboard_end]
require("if (!EmptyClipboard())" in clipboard and "CloseClipboard();" in clipboard,
        "clipboard failure closes the opened clipboard")
require("if (SetClipboardData(CF_UNICODETEXT, mem))" in clipboard and
        clipboard.index("if (SetClipboardData") < clipboard.index("mem = nullptr"),
        "global-memory ownership transfers only after SetClipboardData succeeds")
require("if (mem)" in clipboard and "GlobalFree(mem);" in clipboard,
        "failed clipboard transfer frees retained global memory")

require(hid.count("free(device_interface_list);") >= 3,
        "every Windows HID interface-list exit releases its allocation")
require("memset(&razer, 0, sizeof(madlions))" not in keyboard and
        "memset(&madlions, 0, sizeof(madlions))" in keyboard,
        "UAP zeroes through the complete union-sized member")
require("memset(&razer, 0, sizeof(madlions))" not in patcher and
        "memset(&madlions, 0, sizeof(madlions))" in patcher,
        "fresh Soup patching reproduces the safe union initialiser")
require("combined.reserve((64 - 5) * 3);" in keyboard and
        "buf.reserve((64 - 5) * 3);" not in keyboard,
        "DrunkDeer response aggregation reserves the destination buffer")

print("RESOURCE_OWNERSHIP_STATIC_AUDIT=PASS")
