#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import re
from pathlib import Path
from statistics import median

ROOT = Path(__file__).resolve().parents[1]
PROTOCOL = ROOT / 'HallJoy/mad68pr_protocol.h'
EXPECTED_SHA256 = '2a7df4ffc491476b79da5333f51179f68fbbb5c5e146e987cfb5df764b79cead'
IMAGE_BASE = 0x5000

STAMP = re.compile(r'^\[(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3})\]')
A0_LINE = re.compile(
    r'packet20=A0\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})'
    r'\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})'
)


def stamp(line: str) -> dt.datetime | None:
    m = STAMP.match(line)
    return dt.datetime.strptime(m.group(1), '%Y-%m-%d %H:%M:%S.%f') if m else None


def expected_order() -> list[tuple[int, int, int]]:
    text = PROTOCOL.read_text(encoding='utf-8-sig')
    result = [
        tuple(int(x, 16) for x in m)
        for m in re.findall(
            r'\{\s*\d+,\s*\d+,\s*0x[0-9A-Fa-f]{2},\s*"[^"]+",\s*'
            r'\{\s*0x([0-9A-Fa-f]{2}),\s*0x([0-9A-Fa-f]{2}),\s*0x([0-9A-Fa-f]{2})\s*\}\s*\}',
            text,
        )
    ]
    if len(result) != 68 or len(set(result)) != 68:
        raise SystemExit(f'descriptor table parse failed: {len(result)} entries')
    return result


def at_va(data: bytes, va: int, length: int) -> bytes:
    off = va - IMAGE_BASE
    if off < 0 or off + length > len(data):
        raise AssertionError(f'VA 0x{va:X} outside image')
    return data[off:off + length]


def require_blob(data: bytes, va: int, expected_hex: str, name: str) -> None:
    expected = bytes.fromhex(expected_hex)
    actual = at_va(data, va, len(expected))
    if actual != expected:
        raise AssertionError(
            f'{name} mismatch at VA 0x{va:X}: expected={expected.hex(" ")} actual={actual.hex(" ")}'
        )


def verify_firmware(path: Path) -> list[str]:
    data = path.read_bytes()
    sha = hashlib.sha256(data).hexdigest()
    if sha != EXPECTED_SHA256:
        raise AssertionError(f'firmware SHA mismatch: {sha}')

    # Exact audited instruction sequences. These are deliberately byte-level
    # checks so a later document cannot accidentally merge the two state bytes.
    require_blob(data, 0x5312,
        'b7 67 00 20 93 87 47 80 f8 33 05 48 37 55 00 20 13 67 87 00 '
        '13 06 80 04 93 05 f0 0f 23 80 01 8b 13 05 45 be 0d 48 a3 8f 01 89 '
        'f8 b3 0d 37 ef 40 b0 61',
        'A8 state setup')
    require_blob(data, 0x63E0,
        '37 55 00 20 13 06 80 04 81 45 13 05 45 be 23 80 01 8a '
        'ef e0 ff e6 f5 bd',
        'A9 handler')
    require_blob(data, 0x63F8,
        '83 c7 01 8a e3 9b 07 ee ef e0 ff f0',
        'A8 handler guard/call')
    require_blob(data, 0xA0A8,
        'b7 67 00 20 83 c7 b7 80 a1 8b 81 c7',
        'A0 persistent telemetry gate')
    require_blob(data, 0x85FC,
        'f8 33 f9 9a 5d 9b a3 80 07 00 a3 82 01 86 f4 a3 f8 b3',
        'configuration-time telemetry-bit clear')
    require_blob(data, 0x87BE,
        '97 b1 ff 1f 93 81 21 ae',
        'global-pointer setup')

    # Derived addresses from decoded audited instructions:
    # gp = 0x87BE + 0x1FFFB000 - 0x51E = 0x200032A0.
    gp = 0x87BE + 0x1FFFB000 - 0x51E
    if gp != 0x200032A0:
        raise AssertionError(f'GP derivation changed: 0x{gp:X}')
    service_state = gp - 0x760
    forced_count = gp - 0x761
    telemetry_gate = 0x20006000 - 0x7F5
    if (service_state, forced_count, telemetry_gate) != (
        0x20002B40, 0x20002B3F, 0x2000580B
    ):
        raise AssertionError('derived RAM addresses changed')

    return [
        f'firmware_sha256={sha}',
        f'gp=0x{gp:08X}',
        f'A8_A9_service_state=0x{service_state:08X}',
        f'forced_sweep_count=0x{forced_count:08X}',
        f'A0_telemetry_gate=0x{telemetry_gate:08X}',
        'A9_touches_telemetry_gate=0',
        'firmware_state_machine=PASS',
    ]


def verify_log(path: Path) -> list[str]:
    lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
    order = expected_order()

    final_a9_tx: dt.datetime | None = None
    final_a9_ack: dt.datetime | None = None
    a8_ack: dt.datetime | None = None
    host_a0_tx = 0
    events: list[tuple[dt.datetime, tuple[int, int, int], int]] = []
    for line in lines:
        t = stamp(line)
        if t is None:
            continue
        if 'ACK valid strategy=interrupt-caps-normal-strict opcode=A8' in line:
            a8_ack = t
        if 'TX strategy=interrupt-caps-normal-strict' in line and 'opcode=A9' in line:
            final_a9_tx = t  # last one wins
        if 'ACK valid strategy=interrupt-caps-normal-strict opcode=A9' in line:
            final_a9_ack = t  # last one wins
        if re.search(r'\bTX\b.*\bopcode=A0\b', line):
            host_a0_tx += 1
        m = A0_LINE.search(line)
        if m:
            desc = tuple(int(m.group(i), 16) for i in range(1, 4))
            raw = (int(m.group(4), 16) << 8) | int(m.group(5), 16)
            events.append((t, desc, raw))

    if a8_ack is None or final_a9_tx is None or final_a9_ack is None:
        raise AssertionError('A8/final A9 TX/ACK timestamps missing')
    post_tx = [e for e in events if e[0] > final_a9_tx]
    post_ack = [e for e in events if e[0] > final_a9_ack]
    if host_a0_tx:
        raise AssertionError(f'host opcode A0 was transmitted {host_a0_tx} times')

    def exact_cycles(stream: list[tuple[dt.datetime, tuple[int, int, int], int]]) -> list[tuple[dt.datetime, dt.datetime]]:
        cycles: list[tuple[dt.datetime, dt.datetime]] = []
        pos = 0
        cycle_start: dt.datetime | None = None
        for t, desc, _ in stream:
            if desc == order[pos]:
                if pos == 0:
                    cycle_start = t
                pos += 1
                if pos == len(order):
                    assert cycle_start is not None
                    cycles.append((cycle_start, t))
                    pos = 0
                    cycle_start = None
            else:
                pos = 1 if desc == order[0] else 0
                cycle_start = t if pos == 1 else None
        return cycles

    cycles_after_tx = exact_cycles(post_tx)
    cycles_after_ack = exact_cycles(post_ack)
    intervals = [
        (b[0] - a[0]).total_seconds() * 1000.0
        for a, b in zip(post_tx, post_tx[1:])
        if 0 <= (b[0] - a[0]).total_seconds() <= 0.2
    ]
    if len(cycles_after_tx) < 4:
        raise AssertionError(f'expected >=4 exact ordered cycles after final A9 TX, got {len(cycles_after_tx)}')
    if len(cycles_after_ack) < 3:
        raise AssertionError(f'expected >=3 exact ordered cycles after final A9 ACK, got {len(cycles_after_ack)}')
    if not intervals or not (8.0 <= median(intervals) <= 30.0):
        raise AssertionError('A0 rate limiter timing not observed')
    if (cycles_after_tx[3][1] - final_a9_ack).total_seconds() < 3.5:
        raise AssertionError('post-A9 cycle duration too short to exclude queued packets')
    if any(raw > 1600 for _, _, raw in post_tx):
        raise AssertionError('A0 raw value outside audited range')

    return [
        f'a8_ack={a8_ack.isoformat(sep=" ")}',
        f'final_a9_tx={final_a9_tx.isoformat(sep=" ")}',
        f'final_a9_ack={final_a9_ack.isoformat(sep=" ")}',
        f'post_final_A9_TX_A0_packets={len(post_tx)}',
        f'post_final_A9_ACK_A0_packets={len(post_ack)}',
        f'exact_ordered_cycles_after_final_A9_TX={len(cycles_after_tx)}',
        f'exact_ordered_cycles_after_final_A9_ACK={len(cycles_after_ack)}',
        f'fourth_cycle_end_after_final_A9_ACK_s={(cycles_after_tx[3][1] - final_a9_ack).total_seconds():.3f}',
        f'median_A0_interval_ms={median(intervals):.1f}',
        f'host_opcode_A0_TX={host_a0_tx}',
        'queued_packet_explanation_possible=0',
        'hardware_A9_persistence_evidence=PASS',
        'post_startup_physical_edge_steady_state=NOT_TESTED_BY_THIS_LOG',
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('firmware', type=Path)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        output = verify_firmware(args.firmware) + verify_log(args.log)
    except (AssertionError, OSError) as exc:
        print(f'MAD68PR_A8_A9_CRITIQUE_VERIFY=FAIL\nreason={exc}')
        return 1
    print('\n'.join(output))
    print('MAD68PR_A8_A9_CRITIQUE_VERIFY=PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
