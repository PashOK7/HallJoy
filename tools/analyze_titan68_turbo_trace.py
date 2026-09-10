#!/usr/bin/env python3
"""Summarise an isolated Madlions Titan68 Turbo diagnostic trace.

This parser is deliberately read-only: it never opens a HID device and only
accepts a text trace produced by HallJoy's Titan68 diagnostic image.
"""

import argparse
import re
from collections import Counter, defaultdict
from pathlib import Path

CALIBRATION_ENTER = "06 37 00 00 01 39 00 00 01"
SIMULATION_ENTER = "06 36 00 00 01 38 00 00 01"
SIMULATION_EXIT = "06 36 00 00 01 37 00 00 00"
HEX = re.compile(r"\b[0-9A-Fa-f]{2}\b")
TX = re.compile(r"\[titan68\.tx\] command=([0-9A-Fa-f]{2}) enabled=(\d+) bytes=(\d+) data=(.*)")
RX = re.compile(r"\[titan68\.rx\] bytes=(\d+) data=(.*)")
TRAVEL = re.compile(r"\[titan68\.travel\] key_index=(\d+) raw12=(\d+)")
RAW_KEY = re.compile(r"\[titan68\.raw_key\]")
ACK = re.compile(r"\[titan68\.control\] ack command=([0-9A-Fa-f]{2}) enabled=(\d)")
REJECT = re.compile(r"\[titan68\.control\] rejected command=([0-9A-Fa-f]{2}) enabled=(\d)")
ERROR = re.compile(r"\[titan68\.(?:rx_error|control)\].*(?:failed|timeout|rejected)")


def frame_prefix(text: str) -> str:
    return " ".join(token.upper() for token in HEX.findall(text)[:9])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="HallJoyStabilityTrace.log")
    args = parser.parse_args()
    try:
        lines = args.trace.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as error:
        parser.error(str(error))

    tx = Counter()
    tx_frames = defaultdict(Counter)
    rx_reports = Counter()
    travel = defaultdict(list)
    raw_keys = 0
    ack = Counter()
    rejects = Counter()
    errors = []

    for line in lines:
        if match := TX.search(line):
            command, enabled, byte_count, data = match.groups()
            key = (command.upper(), int(enabled))
            tx[key] += 1
            tx_frames[key][frame_prefix(data)] += 1
            if byte_count != "64":
                errors.append(f"TX frame had unexpected byte count {byte_count}")
        if match := RX.search(line):
            byte_count, data = match.groups()
            values = HEX.findall(data)
            if values:
                rx_reports[values[0].upper()] += 1
            if int(byte_count) != len(values):
                errors.append(f"RX byte count mismatch: header={byte_count}, parsed={len(values)}")
        if match := TRAVEL.search(line):
            key, value = map(int, match.groups())
            travel[key].append(value)
        if RAW_KEY.search(line):
            raw_keys += 1
        if match := ACK.search(line):
            ack[(match.group(1).upper(), int(match.group(2)))] += 1
        if match := REJECT.search(line):
            rejects[(match.group(1).upper(), int(match.group(2)))] += 1
        if ERROR.search(line):
            errors.append(line.strip())

    print(f"trace: {args.trace}")
    expected = {
        ("37", 1): (CALIBRATION_ENTER, "calibration_enter"),
        ("36", 1): (SIMULATION_ENTER, "simulation_enter"),
        ("36", 0): (SIMULATION_EXIT, "simulation_exit"),
    }
    print("TX: " + "; ".join(f"{name}={tx[key]}" for key, (_, name) in expected.items()))
    for key, (frame, name) in expected.items():
        print(f"TX {name} frame: {dict(tx_frames[key]) or 'none'}")
    print("ACK: " + "; ".join(f"{name}={ack[key]}" for key, (_, name) in expected.items()) + f"; rejects: {dict(rejects) or 'none'}")
    print(f"RX report IDs: {dict(sorted(rx_reports.items())) or 'none'}")
    print(f"raw keyboard events: {raw_keys}")
    print(f"decoded raw12 keys: {len(travel)}; samples: {sum(map(len, travel.values()))}")
    for key in sorted(travel):
        values = travel[key]
        print(f"  key_index={key}: samples={len(values)} min={min(values)} max={max(values)}")

    failures = []
    for key, (frame, name) in expected.items():
        if tx[key] != 1 or tx_frames[key] != Counter({frame: 1}):
            failures.append(f"expected exactly one canonical {name} frame")
    if sum(tx.values()) != 3:
        failures.append("unexpected diagnostic control frame outside the three-command allow-list")
    if any(ack[key] != 1 for key in expected):
        failures.append("missing acknowledgement for an approved diagnostic control command")
    if rejects:
        failures.append("firmware rejected a diagnostic control command")
    if errors:
        failures.append(f"trace contains {len(errors)} transport/control error(s)")
    if not travel:
        failures.append("no decoded report-07 raw12 samples")

    if failures:
        print("VERDICT: INCONCLUSIVE/FAIL")
        for item in failures:
            print(f" - {item}")
        return 2
    print("VERDICT: REPORT-07 RAW12 OBSERVED")
    if raw_keys:
        print("Raw keyboard edges were also observed; inspect their timestamps against the tester's before/during/after typing.")
    else:
        print("No target-scoped raw keyboard edges were logged; typing coexistence remains unproven.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
