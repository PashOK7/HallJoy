#!/usr/bin/env python3
from __future__ import annotations
import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROTOCOL = ROOT / "HallJoy/mad68pr_protocol.h"

def expected_descriptors() -> set[tuple[int, int, int]]:
    text = PROTOCOL.read_text(encoding="utf-8")
    found = {
        tuple(int(x, 16) for x in m)
        for m in re.findall(
            r'\{\s*\d+,\s*\d+,\s*0x[0-9A-Fa-f]{2},\s*"[^"]+",\s*'
            r'\{\s*0x([0-9A-Fa-f]{2}),\s*0x([0-9A-Fa-f]{2}),\s*0x([0-9A-Fa-f]{2})\s*\}\s*\}',
            text,
        )
    }
    if len(found) != 68:
        raise SystemExit(f"protocol table parse failed: expected 68, got {len(found)}")
    return found

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    text = args.log.read_text(encoding="utf-8", errors="replace")
    expected = expected_descriptors()

    observed: set[tuple[int, int, int]] = set()
    raw_values: list[int] = []
    for line in text.splitlines():
        match = re.search(
            r'packet20=A0\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})'
            r'\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})',
            line,
        )
        if match:
            observed.add(tuple(int(match.group(i), 16) for i in range(1, 4)))
            raw_values.append((int(match.group(4), 16) << 8) | int(match.group(5), 16))

    missing = expected - observed
    unexpected = observed - expected
    required_markers = [
        "ACK valid strategy=interrupt-caps-normal-strict opcode=A9",
        "ACK valid strategy=interrupt-caps-normal-strict opcode=A8",
        "strategy 1 end result=SUCCESS",
    ]
    marker_status = {m: m in text for m in required_markers}
    checksum_errors = len(re.findall(r"checksum-error|checksum_errors=[1-9]", text, re.I))

    print(f"expected_descriptors={len(expected)}")
    print(f"observed_descriptors={len(observed)}")
    print(f"missing={len(missing)}")
    print(f"unexpected={len(unexpected)}")
    print(f"raw_samples={len(raw_values)}")
    print(f"raw_min={min(raw_values) if raw_values else 'n/a'}")
    print(f"raw_max={max(raw_values) if raw_values else 'n/a'}")
    print(f"checksum_error_markers={checksum_errors}")
    for marker, present in marker_status.items():
        print(f"marker[{marker}]={int(present)}")

    if missing:
        print("missing descriptors:")
        for d in sorted(missing):
            print("  " + ":".join(f"{x:02X}" for x in d))
    if unexpected:
        print("unexpected descriptors:")
        for d in sorted(unexpected):
            print("  " + ":".join(f"{x:02X}" for x in d))

    ok = (
        not missing
        and not unexpected
        and raw_values
        and min(raw_values) >= 0
        and max(raw_values) <= 1600
        and checksum_errors == 0
        and all(marker_status.values())
    )
    print("HARDWARE_LOG_VALIDATION=" + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1

if __name__ == "__main__":
    raise SystemExit(main())
