#!/usr/bin/env python3
"""Verify the Soup pre-open exclusion patch embeds its own validation marker.

The build applies the patch to a freshly downloaded pinned Soup tree. A marker
that exists only in the PowerShell variable/validation code is insufficient:
it must be emitted into the generated C++ block before CreateFileW.
"""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PATCH = ROOT / "third_party" / "UniversalAnalogPluginFixed" / "tools" / "Apply-Soup-Madlions-Fix.ps1"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    text = PATCH.read_text(encoding="utf-8-sig", errors="strict")

    marker_match = re.search(r"\$preOpenMarker\s*=\s*'([^']+)'", text)
    require(marker_match is not None, "pre-open marker constant exists")
    marker = marker_match.group(1)

    block_match = re.search(
        r'\$preOpenBlock\s*=\s*@"(?P<body>.*?)^"@',
        text,
        flags=re.DOTALL | re.MULTILINE,
    )
    require(block_match is not None, "generated pre-open C++ block is locatable")
    block = block_match.group("body")

    require(marker in block, "validation marker is emitted into generated C++")
    require("UAP_EXCLUDE_HALLJOY_NATIVE" in block,
            "generated block is compile-time gated")
    require("halljoy_should_exclude_hid_interface(device_interface)" in block,
            "generated block delegates to the shared exact-interface ownership hook")
    require("HALLJOY_UAP_NATIVE_HID_IDS" not in text,
            "legacy coarse VID/PID route list is absent")
    require("continue;" in block,
            "generated block skips a claimed path before opening it")
    require("$hidSourceText.Insert($braceStart + 1, $preOpenBlock)" in text,
            "generated block is inserted at the enumeration-loop body start")
    require("$hidSourceText.IndexOf($preOpenMarker)" in text,
            "post-patch validation reuses the marker constant")

    # Model the ordering contract independently of PowerShell execution. This
    # catches the exact failure where the marker existed in the script but not
    # in the generated block.
    fixture = "for (...)\n{" + block + "\n\thwHid hid{};\n\thid.handle = CreateFileW(\n"
    require(fixture.index(marker) < fixture.index("hid.handle = CreateFileW"),
            "emitted marker precedes the first device CreateFileW")

    print("PLUGIN_PREOPEN_PATCH_CONTRACT_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
