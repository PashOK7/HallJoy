#!/usr/bin/env python3
"""Audit the stock-firmware Keychron K4 HE ANSI UAP route and matrix."""

from pathlib import Path
import re
import sys


REPO = Path(__file__).resolve().parents[3]
SOURCE = REPO / "third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/AnalogueKeyboard.cpp"
text = SOURCE.read_text(encoding="utf-8-sig")
plugin = (REPO / "third_party/UniversalAnalogPluginFixed/main.cpp").read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    print(("PASS" if condition else "FAIL") + ": " + message)
    if not condition:
        failures.append(message)


failures: list[str] = []
match = re.search(
    r"layout_keychron_k4_he_ansi\[\]\s*=\s*\{\s*6\s*,\s*19\s*,(.*?)\};",
    text,
    re.S,
)
entries = re.findall(r"\bKEY_[A-Z0-9_]+\b", match.group(1)) if match else []

require('return "Keychron K4 HE ANSI";' in text, "K4 HE has a stable user-facing identity")
require(
    text.count("hid.product_id == 0x0E40") == 2,
    "PID 0E40 is gated once during discovery and once during layout routing",
)
require("hid.usage_page == 0xFF60 && hid.usage == 0x61" in text,
        "Keychron discovery remains restricted to the vendor analogue HID interface")
require(match is not None, "K4 HE ANSI matrix is declared as 6x19")
require(len(entries) == 6 * 19, "K4 HE ANSI matrix contains exactly 114 matrix cells")
require(entries.count("KEY_NONE") == 14, "K4 HE ANSI matrix contains exactly 100 physical keys")
require("kbd.keychron.layout = layout_keychron_k4_he_ansi;" in text,
        "K4 HE PID is routed to its dedicated matrix")
require("kbd.hid.product_id == 0x0B50 || kbd.hid.product_id == 0x0E40" in plugin,
        "private UAP telemetry reports the K4 HE 6x19 topology")
require("bc56b3c611dcc1a8ed9a2acb8bdc4da5e1a80c27" in text,
        "matrix provenance is pinned to an immutable official Keychron commit")

if failures:
    print("KEYCHRON_K4HE_STATIC_AUDIT=FAIL", file=sys.stderr)
    sys.exit(1)
print("KEYCHRON_K4HE_STATIC_AUDIT=PASS")
