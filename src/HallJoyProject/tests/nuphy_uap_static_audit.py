#!/usr/bin/env python3
"""Lock the safety and raw-precision contract of the UAP NuPhy A0 reader."""

from pathlib import Path
import sys


REPO = Path(__file__).resolve().parents[3]
SOURCE = (REPO / "third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/AnalogueKeyboard.cpp").read_text(encoding="utf-8-sig")
HEADER = (REPO / "third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/AnalogueKeyboard.hpp").read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    print(("PASS" if condition else "FAIL") + ": " + message)
    if not condition:
        failures.append(message)


failures: list[str] = []
require("uint16_t buffer[NUM_KEYS];" in HEADER,
        "NuPhy cache retains the 16-bit raw travel domain")
require("sizeof(decltype(nuphy)) >= sizeof(decltype(madlions))" in HEADER,
        "the widened NuPhy cache is statically kept as the complete-union initializer")
require("if (report.size() < 8)" in SOURCE and "bool valid_report = true;" in SOURCE,
        "A0 records are length-gated before any field access")
require("const uint16_t scancode = static_cast<uint16_t>(report[2] << 8) | report[3];" in SOURCE and
        "const uint16_t value = static_cast<uint16_t>(report[4] << 8) | report[5];" in SOURCE,
        "A0 big-endian identity and travel fields have explicit bounded decoding")
require("if (value > full_scale)" in SOURCE and "disconnected = true;" in SOURCE,
        "out-of-domain raw travel fails closed instead of an unsafe byte conversion")
require("static_cast<float>(nuphy.buffer[i]) / static_cast<float>(full_scale)" in SOURCE,
        "publication normalizes directly from raw travel without 8-bit quantization")
require("hid.product_id == 0x6120 || hid.product_id == 0xFEE0" in SOURCE,
        "the established 1600-scale product exception remains explicit")

if failures:
    print("NUPHY_UAP_STATIC_AUDIT=FAIL", file=sys.stderr)
    sys.exit(1)
print("NUPHY_UAP_STATIC_AUDIT=PASS")
