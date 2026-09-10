#!/usr/bin/env python3
"""Build an evidence-only DrunkDeer matrix map from a schema-2 trace.

This tool runs after HallJoy exits and never participates in input publication.
Digital timing labels a matrix coordinate only in the saved evidence. Production
uses the resulting static model map from the beginning of analogue travel, so a
deep digital actuation point cannot add runtime latency or hide early travel.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path


MATRIX_CELLS = 126
RAW_ON_THRESHOLD = 5
MINIMUM_PEAK = 8
POST_EDGE_GRACE_MS = 100


@dataclass(frozen=True)
class Frame:
    time_ms: int
    values: tuple[int, ...]


@dataclass(frozen=True)
class DigitalInterval:
    hid: int
    start_ms: int
    end_ms: int


@dataclass(frozen=True)
class Episode:
    cell: int
    start_ms: int
    end_ms: int
    peak: int
    samples: int


TIME_RE = re.compile(
    r"^\[(?:\d{4}-\d{2}-\d{2}T)?(\d{2}):(\d{2}):(\d{2})\.(\d{3})\]"
)
SNAPSHOT_RE = re.compile(
    r"\[event=payload\.snapshot\].*?reason=(proof|session_initial) "
    r".*?chunk0=([0-9A-F ]+) chunk1=([0-9A-F ]+) chunk2=([0-9A-F ]+)$"
)
DELTA_RE = re.compile(
    r"\[drunkdeer\.payload\.delta\].*?changed=(\d+) listed=(\d+).*?values=(.*)$"
)
DIGITAL_RE = re.compile(
    r"\[event=digital\.(press|release)\].*?hid=([0-9A-F]{4})"
)
DELTA_VALUE_RE = re.compile(r"(\d+):(\d+)>(\d+)")


HID_NAMES = {
    **{0x04 + index: chr(ord("A") + index) for index in range(26)},
    0x1E: "1", 0x1F: "2", 0x20: "3", 0x21: "4", 0x22: "5",
    0x23: "6", 0x24: "7", 0x25: "8", 0x26: "9", 0x27: "0",
    0x28: "Enter", 0x29: "Esc", 0x2A: "Backspace", 0x2B: "Tab",
    0x2C: "Space", 0x2D: "Minus", 0x2E: "Equal",
    0x2F: "LeftBracket", 0x30: "RightBracket", 0x31: "Backslash",
    0x33: "Semicolon", 0x34: "Quote", 0x35: "Backquote",
    0x36: "Comma", 0x37: "Period", 0x38: "Slash", 0x39: "CapsLock",
    0x4F: "Right", 0x50: "Left", 0x51: "Down", 0x52: "Up",
    0xE0: "LeftCtrl", 0xE1: "LeftShift", 0xE2: "LeftAlt",
    0xE3: "LeftGui", 0xE4: "RightCtrl", 0xE5: "RightShift",
    0xE6: "RightAlt", 0xE7: "RightGui",
}


def timestamp_ms(line: str) -> int | None:
    match = TIME_RE.match(line)
    if not match:
        return None
    hour, minute, second, milli = map(int, match.groups())
    return (((hour * 60) + minute) * 60 + second) * 1000 + milli


def parse_log(path: Path) -> tuple[list[Frame], list[DigitalInterval], list[str]]:
    frames: list[Frame] = []
    intervals: list[DigitalInterval] = []
    warnings: list[str] = []
    payload: list[int] | None = None
    digital_down: dict[int, int] = {}

    for line_number, line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(), start=1
    ):
        time_ms = timestamp_ms(line)
        if time_ms is None:
            continue

        snapshot = SNAPSHOT_RE.search(line)
        if snapshot:
            values: list[int] = []
            for group in snapshot.group(2, 3, 4):
                values.extend(int(value, 16) for value in group.split())
            if len(values) != 177:
                warnings.append(
                    f"line {line_number}: snapshot has {len(values)} bytes"
                )
                continue
            payload = values
            frames.append(Frame(time_ms, tuple(payload[:MATRIX_CELLS])))
            continue

        delta = DELTA_RE.search(line)
        if delta:
            if payload is None:
                warnings.append(f"line {line_number}: delta before snapshot")
                continue
            changed, listed = map(int, delta.group(1, 2))
            updates = DELTA_VALUE_RE.findall(delta.group(3))
            if listed != changed or len(updates) != changed:
                warnings.append(
                    f"line {line_number}: incomplete delta {len(updates)}/{changed}; "
                    "frame excluded"
                )
                continue
            candidate = payload.copy()
            valid = True
            for offset_text, old_text, new_text in updates:
                offset, old, new = map(int, (offset_text, old_text, new_text))
                if offset >= len(candidate) or candidate[offset] != old:
                    warnings.append(
                        f"line {line_number}: delta continuity failed at {offset}"
                    )
                    valid = False
                    break
                candidate[offset] = new
            if valid:
                payload = candidate
                frames.append(Frame(time_ms, tuple(payload[:MATRIX_CELLS])))
            continue

        digital = DIGITAL_RE.search(line)
        if digital:
            kind, hid_text = digital.groups()
            hid = int(hid_text, 16)
            if not hid:
                continue
            if kind == "press":
                digital_down.setdefault(hid, time_ms)
            elif hid in digital_down:
                intervals.append(DigitalInterval(hid, digital_down.pop(hid), time_ms))

    for hid in sorted(digital_down):
        warnings.append(f"digital HID {hid:04X}: press without release")
    return frames, intervals, warnings


def build_episodes(frames: list[Frame]) -> list[Episode]:
    if not frames:
        return []
    active: list[list[int] | None] = [None] * MATRIX_CELLS
    episodes: list[Episode] = []
    for frame in frames:
        for cell, raw in enumerate(frame.values):
            state = active[cell]
            if raw >= RAW_ON_THRESHOLD:
                if state is None:
                    active[cell] = [frame.time_ms, raw, 1]
                else:
                    state[1] = max(state[1], raw)
                    state[2] += 1
            elif state is not None:
                episodes.append(Episode(
                    cell, state[0], frame.time_ms, state[1], state[2]
                ))
                active[cell] = None
    final_time = frames[-1].time_ms
    for cell, state in enumerate(active):
        if state is not None:
            episodes.append(Episode(cell, state[0], final_time, state[1], state[2]))
    return [episode for episode in episodes if episode.peak >= MINIMUM_PEAK]


def correlate(
    frames: list[Frame], episodes: list[Episode],
    intervals: list[DigitalInterval]
) -> dict[str, object]:
    votes: dict[int, dict[int, int]] = {}
    ambiguous: dict[int, int] = {}
    no_sample: dict[int, int] = {}
    used_episodes: set[int] = set()

    for interval in intervals:
        # A direct matrix frame captured while the key is digitally held is
        # stronger evidence than broad episode overlap in a sparse trace.
        # This is post-exit labelling only; production never waits for it.
        candidates = {
            cell
            for frame in frames
            if interval.start_ms <= frame.time_ms <=
                interval.end_ms + POST_EDGE_GRACE_MS
            for cell, raw in enumerate(frame.values)
            if raw >= MINIMUM_PEAK
        }
        if len(candidates) == 1:
            cell = next(iter(candidates))
            votes.setdefault(interval.hid, {})[cell] = (
                votes.setdefault(interval.hid, {}).get(cell, 0) + 1
            )
            for index, episode in enumerate(episodes):
                if episode.cell == cell and \
                        episode.start_ms <= interval.end_ms + POST_EDGE_GRACE_MS and \
                        episode.end_ms >= interval.start_ms:
                    used_episodes.add(index)
        elif candidates:
            ambiguous[interval.hid] = ambiguous.get(interval.hid, 0) + 1
            for index, episode in enumerate(episodes):
                if episode.cell in candidates and \
                        episode.start_ms <= interval.end_ms + POST_EDGE_GRACE_MS and \
                        episode.end_ms >= interval.start_ms:
                    used_episodes.add(index)
        else:
            no_sample[interval.hid] = no_sample.get(interval.hid, 0) + 1

    mappings: list[dict[str, object]] = []
    all_hids = sorted(set(votes) | set(ambiguous) | set(no_sample))
    for hid in all_hids:
        ranked = sorted(votes.get(hid, {}).items(), key=lambda item: (-item[1], item[0]))
        best_cell = ranked[0][0] if ranked else None
        best_votes = ranked[0][1] if ranked else 0
        second_votes = ranked[1][1] if len(ranked) > 1 else 0
        status = "stable" if best_votes > second_votes else (
            "conflict" if best_votes else (
                "ambiguous" if ambiguous.get(hid, 0) else "no_sample"
            )
        )
        mappings.append({
            "hid": hid,
            "name": HID_NAMES.get(hid, f"HID_{hid:04X}"),
            "status": status,
            "cell": best_cell,
            "row": best_cell // 21 if best_cell is not None else None,
            "column": best_cell % 21 if best_cell is not None else None,
            "best_votes": best_votes,
            "second_votes": second_votes,
            "ambiguous": ambiguous.get(hid, 0),
            "no_sample": no_sample.get(hid, 0),
        })

    orphan_by_cell: dict[int, list[Episode]] = {}
    for index, episode in enumerate(episodes):
        if index not in used_episodes:
            orphan_by_cell.setdefault(episode.cell, []).append(episode)
    orphans = [{
        "cell": cell,
        "row": cell // 21,
        "column": cell % 21,
        "presses": len(cell_episodes),
        "max_raw": max(episode.peak for episode in cell_episodes),
    } for cell, cell_episodes in sorted(orphan_by_cell.items())]
    repeated_orphans = [item for item in orphans if item["presses"] >= 2]
    fn_candidate = repeated_orphans[0] if len(repeated_orphans) == 1 else None
    return {
        "mappings": mappings,
        "orphans": orphans,
        "fn_candidate": fn_candidate,
        "coverage": {
            "digital_intervals": len(intervals),
            "distinct_hids": len(all_hids),
            "stable_hids": sum(item["status"] == "stable" for item in mappings),
            "conflicts": sum(item["status"] == "conflict" for item in mappings),
            "ambiguous_hids": sum(item["status"] == "ambiguous" for item in mappings),
            "no_sample_hids": sum(item["status"] == "no_sample" for item in mappings),
        },
    }


def analyse(frames: list[Frame], intervals: list[DigitalInterval]) -> dict[str, object]:
    result = correlate(frames, build_episodes(frames), intervals)
    result["frame_count"] = len(frames)
    return result


def print_report(result: dict[str, object], warnings: list[str]) -> None:
    coverage = result["coverage"]
    print(
        "frames={frame_count} digital_intervals={digital_intervals} "
        "distinct_hids={distinct_hids} stable={stable_hids} "
        "conflicts={conflicts} ambiguous={ambiguous_hids} "
        "no_sample={no_sample_hids}".format(
            frame_count=result["frame_count"], **coverage
        )
    )
    print("\nDIGITAL MAP")
    for item in result["mappings"]:
        coordinate = "-" if item["cell"] is None else (
            f"offset={item['cell']} row={item['row']} column={item['column']}"
        )
        print(
            f"{item['name']:12s} HID={item['hid']:04X} {item['status']:9s} "
            f"{coordinate} votes={item['best_votes']}/{item['second_votes']} "
            f"ambiguous={item['ambiguous']} no_sample={item['no_sample']}"
        )
    print("\nUNLABELLED ANALOG CELLS")
    for item in result["orphans"]:
        print(
            f"offset={item['cell']} row={item['row']} column={item['column']} "
            f"presses={item['presses']} max_raw={item['max_raw']}"
        )
    if result["fn_candidate"]:
        item = result["fn_candidate"]
        print(
            "\nFN_CANDIDATE "
            f"offset={item['cell']} row={item['row']} column={item['column']} "
            f"presses={item['presses']} (requires isolated Fn-only test phase)"
        )
    if warnings:
        print("\nWARNINGS")
        for warning in warnings:
            print("-", warning)


def self_test() -> None:
    zero = [0] * MATRIX_CELLS
    frames: list[Frame] = []
    for time_ms, cell10, cell12 in (
        (0, 0, 0), (100, 8, 0), (200, 24, 0), (300, 40, 0),
        (400, 0, 0), (500, 0, 20), (600, 0, 0),
        (700, 0, 30), (800, 0, 0),
    ):
        values = zero.copy()
        values[10] = cell10
        values[12] = cell12
        frames.append(Frame(time_ms, tuple(values)))
    # The analogue episode starts 150 ms before the deep digital actuation.
    result = analyse(frames, [DigitalInterval(0x04, 250, 350)])
    mapping = result["mappings"][0]
    assert mapping["status"] == "stable"
    assert mapping["cell"] == 10
    assert result["fn_candidate"]["cell"] == 12
    print("drunkdeer offline matrix analyzer self-test passed")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", nargs="?", type=Path)
    parser.add_argument("--json", action="store_true", dest="as_json")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if args.log is None:
        parser.error("log is required unless --self-test is used")
    frames, intervals, warnings = parse_log(args.log)
    result = analyse(frames, intervals)
    if args.as_json:
        print(json.dumps({**result, "warnings": warnings}, indent=2))
    else:
        print_report(result, warnings)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
