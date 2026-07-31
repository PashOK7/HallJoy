#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import re
from pathlib import Path
from statistics import median

STAMP = re.compile(r'^\[(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3})\]')
A0 = re.compile(
    r'packet20=A0\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})'
)


def timestamp(line: str) -> dt.datetime | None:
    match = STAMP.match(line)
    if not match:
        return None
    return dt.datetime.strptime(match.group(1), '%Y-%m-%d %H:%M:%S.%f')


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    lines = args.log.read_text(encoding='utf-8', errors='replace').splitlines()

    a8_time: dt.datetime | None = None
    first_by_descriptor: dict[str, dt.datetime] = {}
    a0_times: list[dt.datetime] = []
    for line in lines:
        stamp = timestamp(line)
        if stamp is None:
            continue
        if a8_time is None and 'ACK valid strategy=interrupt-caps-normal-strict opcode=A8' in line:
            a8_time = stamp
        match = A0.search(line)
        if match:
            descriptor = ':'.join(match.groups()).upper()
            first_by_descriptor.setdefault(descriptor, stamp)
            a0_times.append(stamp)

    intervals_ms = [
        (b - a).total_seconds() * 1000.0
        for a, b in zip(a0_times, a0_times[1:])
        if 0.0 <= (b - a).total_seconds() <= 0.2
    ]
    coverage_seconds = None
    if a8_time and len(first_by_descriptor) == 68:
        coverage_seconds = (max(first_by_descriptor.values()) - a8_time).total_seconds()

    print(f'a8_ack_found={int(a8_time is not None)}')
    print(f'unique_descriptors={len(first_by_descriptor)}')
    print(f'a0_packets={len(a0_times)}')
    print(f'first_68_coverage_seconds={coverage_seconds if coverage_seconds is not None else "n/a"}')
    print(f'median_a0_interval_ms={median(intervals_ms) if intervals_ms else "n/a"}')

    ok = (
        a8_time is not None
        and len(first_by_descriptor) == 68
        and coverage_seconds is not None
        and 0.25 <= coverage_seconds <= 2.5
        and intervals_ms
        and 8.0 <= median(intervals_ms) <= 30.0
    )
    print('HARDWARE_TIMING_VALIDATION=' + ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
