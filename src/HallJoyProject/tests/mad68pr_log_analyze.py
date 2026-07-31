#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import statistics
from collections import Counter, defaultdict
from pathlib import Path


def read_text(path: Path) -> str:
    data = path.read_bytes()
    for encoding in ('utf-8-sig', 'utf-16', 'utf-16-le', 'cp1251'):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    return data.decode('utf-8', errors='replace')


def pct(values: list[int], q: float) -> int | None:
    if not values:
        return None
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, round((len(ordered) - 1) * q)))
    return ordered[index]


def main() -> int:
    ap = argparse.ArgumentParser(description='Summarise HallJoy MAD68 Pro R Native Stable v3.5 logs')
    ap.add_argument('log', type=Path)
    args = ap.parse_args()
    text = read_text(args.log)
    lines = text.splitlines()

    build = next((line for line in lines if 'session start build=' in line), '')
    ack = Counter()
    cycles: list[tuple[int, int, str]] = []
    steady = []
    edge_latency: dict[str, list[int]] = defaultdict(list)
    edge_mismatch = Counter()
    misses = Counter()
    per_key_starvation = Counter()
    global_recovery = 0
    rates: list[float] = []
    max_gaps: list[int] = []
    coverage_max = 0
    publish_transitions: list[str] = []
    malformed_max = 0
    checksum_max = 0

    for line in lines:
        m = re.search(r'ACK valid .* opcode=([0-9A-F]{2})', line)
        if m:
            ack[m.group(1)] += 1
        m = re.search(r'ordered A0 matrix cycle complete cycle=(\d+)/4 duration_since_A8_ms=(\d+) kind=([^\s]+)', line)
        if m:
            cycles.append((int(m.group(1)), int(m.group(2)), m.group(3)))
        if 'STEADY-STATE A0 CONFIRMED' in line:
            steady.append(line)
        m = re.search(r'digital/analog (?:fresh packet OK|post-edge packet was already cached).*key=([^\s]+).*latency_ms=(\d+)', line)
        if m:
            edge_latency[m.group(1)].append(int(m.group(2)))
        m = re.search(r'digital/analog fresh packet VALUE MISMATCH key=([^\s]+)', line)
        if m:
            edge_mismatch[m.group(1)] += 1
        m = re.search(r'DIGITAL WITHOUT FRESH A0 key=([^\s]+)', line)
        if m:
            misses[m.group(1)] += 1
        m = re.search(r'(?:W/A/S/D|non-WASD) per-key A0 starvation key=([^;\s]+)', line)
        if m:
            per_key_starvation[m.group(1)] += 1
        if 'global A0 transport is dead' in line:
            global_recovery += 1
        m = re.search(r'A0=\d+\(\+\d+ rate_x10=(\d+) gap50=\d+ max_gap_ms=(\d+)\)', line)
        if m:
            rates.append(int(m.group(1)) / 10.0)
            max_gaps.append(int(m.group(2)))
        m = re.search(r'coverage=(\d+)/68', line)
        if m:
            coverage_max = max(coverage_max, int(m.group(1)))
        if 'publish mode ' in line:
            publish_transitions.append(line)
        m = re.search(r'checksum_errors=(\d+)', line)
        if m:
            checksum_max = max(checksum_max, int(m.group(1)))
        m = re.search(r'malformed=(\d+)', line)
        if m:
            malformed_max = max(malformed_max, int(m.group(1)))

    print('MAD68PRO_R_LOG_SUMMARY')
    print(f'file={args.log}')
    print(f'build_marker={build or "NOT_FOUND"}')
    print(f'ack_A8={ack["A8"]} ack_A9={ack["A9"]}')
    print(f'coverage_max={coverage_max}/68')
    print(f'ordered_cycles={len(cycles)} details={cycles}')
    print(f'steady_state_confirmed={1 if steady else 0}')
    if steady:
        print(f'steady_marker={steady[-1]}')
    print(f'checksum_errors_max={checksum_max} malformed_max={malformed_max}')
    if rates:
        print(f'a0_rate_packets_per_s_median={statistics.median(rates):.1f} min={min(rates):.1f} max={max(rates):.1f}')
        print(f'a0_max_gap_ms_observed={max(max_gaps)}')
    else:
        print('a0_rate_packets_per_s=NO_SUMMARY')

    for key in ('W', 'A', 'S', 'D'):
        values = edge_latency.get(key, [])
        print(
            f'{key}: correlated_edges={len(values)} '
            f'latency_median_ms={statistics.median(values) if values else "NA"} '
            f'latency_p95_ms={pct(values, .95) if values else "NA"} '
            f'latency_max_ms={max(values) if values else "NA"} '
            f'mismatches={edge_mismatch[key]} misses={misses[key]} starvation={per_key_starvation[key]}'
        )

    print(f'global_recovery_requests={global_recovery}')
    print(f'publish_transitions={len(publish_transitions)}')
    for line in publish_transitions[-8:]:
        print(f'  {line}')

    if not steady:
        print('verdict=STARTUP_TRANSPORT_MAY_WORK_BUT_POST_SWEEP_STEADY_STATE_NOT_PROVEN')
    elif any(misses.values()):
        print('verdict=STEADY_STATE_PRESENT_WITH_PER_KEY_SCHEDULER_STARVATION_OR_LOSS')
    else:
        print('verdict=STEADY_STATE_CONFIRMED_NO_RECORDED_PER_KEY_TIMEOUTS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
