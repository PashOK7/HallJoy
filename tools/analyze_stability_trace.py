#!/usr/bin/env python3
"""Analyze temporary HallJoy stabilization traces and emit PASS/WARN/FAIL."""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

FIELD_RE = re.compile(r"\[([a-z_]+)=([^\]]*)\]")
TAIL_FIELD_RE = re.compile(r"(?:^|\s)([a-zA-Z0-9_.-]+)=([^\s]+)")


@dataclass(frozen=True)
class Event:
    seq: int
    level: str
    component: str
    name: str
    fields: dict[str, str]
    line_number: int


def parse_trace(path: Path) -> list[Event]:
    events: list[Event] = []
    text = path.read_text(encoding="utf-8-sig", errors="replace")
    for line_number, line in enumerate(text.splitlines(), 1):
        bracket_fields = dict(FIELD_RE.findall(line))
        required = {"seq", "level", "component", "event"}
        if not required.issubset(bracket_fields):
            continue
        try:
            seq = int(bracket_fields["seq"])
        except ValueError as exc:
            raise ValueError(f"line {line_number}: invalid seq") from exc
        tail_start = line.rfind("]") + 1
        tail_fields = dict(TAIL_FIELD_RE.findall(line[tail_start:]))
        events.append(Event(
            seq=seq,
            level=bracket_fields["level"],
            component=bracket_fields["component"],
            name=bracket_fields["event"],
            fields=tail_fields,
            line_number=line_number,
        ))
    return events


def parse_uint(event: Event, name: str) -> int | None:
    value = event.fields.get(name)
    if value is None:
        return None
    try:
        parsed = int(value, 0)
    except ValueError:
        return None
    return parsed if parsed >= 0 else None


def analyze(events: list[Event]) -> tuple[str, list[str], list[str], Counter[str]]:
    failures: list[str] = []
    warnings: list[str] = []
    counts: Counter[str] = Counter()

    if not events:
        return "FAIL", ["no structured trace events found"], warnings, counts

    if events[0].seq != 1:
        failures.append(f"first structured event must have seq=1, got {events[0].seq}")
    expected = events[0].seq
    for event in events:
        counts[f"{event.component}:{event.name}"] += 1
        if event.seq != expected:
            failures.append(
                f"sequence break at line {event.line_number}: expected {expected}, got {event.seq}")
            expected = event.seq
        expected += 1

        normalized_level = event.level.upper()
        if normalized_level in {"ERROR", "FATAL"}:
            failures.append(
                f"{event.level} {event.component}:{event.name} at seq {event.seq}")
        elif normalized_level == "WARN" and not (
            event.component == "spark" and event.name == "hotplug.stale"
        ):
            warnings.append(f"WARN {event.component}:{event.name} at seq {event.seq}")
        if event.name in {"worker.fault", "forced_termination", "trace.capped"}:
            failures.append(f"critical event {event.component}:{event.name} at seq {event.seq}")

    starts = [event for event in events if event.component == "main" and event.name == "session.start"]
    ends = [event for event in events if event.component == "main" and event.name == "session.end"]
    if len(starts) != 1:
        failures.append(f"expected one session.start, found {len(starts)}")
    else:
        if starts[0].fields.get("schema") != "1":
            failures.append(f"unsupported trace schema={starts[0].fields.get('schema', '?')}")
        if starts[0].fields.get("stage") != "S02V1":
            failures.append(f"unexpected trace stage={starts[0].fields.get('stage', '?')}")
    if len(ends) != 1:
        failures.append(f"expected one session.end, found {len(ends)}")
    elif ends[0].fields.get("exit_code") != "0":
        failures.append(f"non-zero process exit_code={ends[0].fields.get('exit_code', '?')}")
    if ends and events[-1] is not ends[-1]:
        failures.append("session.end is not the final structured event")

    active_workers: defaultdict[str, int] = defaultdict(int)
    for event in events:
        if event.name == "worker.start":
            active_workers[event.component] += 1
        elif event.name == "worker.exit":
            if active_workers[event.component] == 0:
                failures.append(f"unmatched worker.exit for {event.component} at seq {event.seq}")
            else:
                active_workers[event.component] -= 1
    for component, active in sorted(active_workers.items()):
        if active:
            failures.append(f"{component}: {active} worker generation(s) did not exit")

    if any(event.name == "stop.timeout" for event in events):
        failures.append("at least one worker stop timed out")

    spark_events = [event for event in events if event.component == "spark"]
    if not spark_events:
        warnings.append("SparkLink was not exercised in this trace")
    else:
        connected = [event for event in spark_events if event.name == "connected"]
        if not connected:
            warnings.append("SparkLink trace contains no connected event")

        stats = [event for event in spark_events if event.name == "worker.stats"]
        if not stats:
            warnings.append("SparkLink worker statistics are missing")
        else:
            stat_fields = ("route_queries", "route_ok", "changed_rows", "input_notifies")
            totals: dict[str, int] = {}
            for field in stat_fields:
                values = [parse_uint(event, field) for event in stats]
                if any(value is None for value in values):
                    failures.append(f"SparkLink worker.stats contains invalid {field}")
                    continue
                totals[field] = sum(value for value in values if value is not None)
            if totals.get("route_queries", 0) == 0 or totals.get("route_ok", 0) == 0:
                warnings.append("SparkLink polling was not proven by successful route queries")
            if totals.get("changed_rows", 0) == 0:
                warnings.append("no SparkLink analog row change was observed; press analog keys during the test")
            if totals.get("input_notifies", 0) == 0:
                warnings.append("no SparkLink realtime input notification was observed")

        disconnects = [
            event for event in spark_events
            if event.name in {"hotplug.stale", "disconnected"}
        ]
        reconnects = [
            event for event in spark_events if event.name == "hotplug.reconnect"
        ]
        reconnected_after_disconnect = any(
            disconnect.seq < reconnect.seq
            for disconnect in disconnects
            for reconnect in reconnects
        )
        if not reconnected_after_disconnect:
            warnings.append("SparkLink unplug/reconnect scenario was not fully exercised")
        if not any(event.name in {"neutralized", "disconnected"} for event in spark_events):
            warnings.append("SparkLink neutralization/disconnect path was not exercised")

        modes: set[int] = set()
        row_limits: set[int] = set()
        for event in connected:
            mode = parse_uint(event, "poll_mode")
            row_limit = parse_uint(event, "row_limit")
            if mode is not None:
                modes.add(mode)
            if row_limit is not None:
                row_limits.add(row_limit)
        for event in events:
            if event.component != "spark.settings":
                continue
            if event.name == "poll_mode":
                mode = parse_uint(event, "new")
                if mode is not None:
                    modes.add(mode)
            elif event.name == "row_limit":
                row_limit = parse_uint(event, "new")
                if row_limit is not None:
                    row_limits.add(row_limit)
        missing_modes = sorted({0, 1, 2} - modes)
        if missing_modes:
            warnings.append(f"SparkLink polling modes not all exercised; missing={','.join(map(str, missing_modes))}")
        if len(row_limits) < 2:
            warnings.append("fewer than two distinct SparkLink row-limit values were exercised")

    if not any(event.component == "vigem" and event.name == "init.ok" for event in events):
        warnings.append("ViGEm init.ok was not observed; virtual gamepad output is not proven")

    if failures:
        verdict = "FAIL"
    elif warnings:
        verdict = "WARN"
    else:
        verdict = "PASS"
    return verdict, failures, warnings, counts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    args = parser.parse_args()

    try:
        events = parse_trace(args.trace)
        verdict, failures, warnings, counts = analyze(events)
    except (OSError, ValueError) as exc:
        print(f"FAIL: {exc}")
        return 2

    print(f"VERDICT: {verdict}")
    print(f"EVENTS: {len(events)}")
    for key in sorted(counts):
        print(f"  {key}={counts[key]}")
    for warning in warnings:
        print(f"WARN: {warning}")
    for failure in failures:
        print(f"FAIL: {failure}")
    return 0 if verdict == "PASS" else (1 if verdict == "WARN" else 2)


if __name__ == "__main__":
    raise SystemExit(main())
