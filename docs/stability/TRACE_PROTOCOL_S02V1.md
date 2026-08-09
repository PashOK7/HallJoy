# S02V1 temporary evidence-trace protocol

- Date: July 30, 2026
- Schema: `1`
- Stage marker: `S02V1`

## Purpose

`HallJoyStabilityTrace.log` is a temporary acceptance tool for a specific
stabilization package. It does not replace normal diagnostics and must not
remain in the final release.

For each change: add only the minimum events around the modified contract, run
the predefined Windows/hardware scenario, analyze the log, preserve the PASS
report as evidence, remove events for the accepted change, and only then begin
the next feature. WARN means the scenario was incomplete; FAIL means a detected
fault, emergency path, or trace-integrity violation.

## Files and bounded I/O

- `HallJoyStabilityTrace.log`: most recent run;
- `HallJoyStabilityTrace.previous.log`: preceding run;
- `HallJoyStabilityTraceBundle.zip`: created manually after HallJoy closes.

Each launch rotates only the current/previous pair. The current file is capped
at 1 MiB. Its storage is pre-mapped through
`CreateFileMappingW`/`MapViewOfFile`; event recording never calls `WriteFile`,
`FlushViewOfFile`, or `FlushFileBuffers`. There is no hot-path dynamic queue,
writer thread, or periodic flush. Fixed-buffer formatting and one copy under an
`SRWLOCK` are followed by a single final truncate/close after all workers stop.

Overflow writes a reserved structured `trace.capped` event, disables tracing,
and forces FAIL. Sequence assignment and append share the same lock, so file
order and sequence order cannot diverge.

## Record format and invariants

```text
[timestamp][elapsed_ms=N][seq=N][pid=N][tid=N][level=INFO|WARN|ERROR|FATAL][component=name][event=name] key=value ...
```

- first record: `main/session.start`, `seq=1`, `schema=1`, `stage=S02V1`;
- `seq` is continuous and strictly increasing in file order;
- each `worker.start` has one matching `worker.exit`;
- final record: `main/session.end exit_code=0`;
- PASS forbids ERROR, FATAL, `worker.fault`, `forced_termination`,
  `stop.timeout`, and `trace.capped`.

Only rare transitions and final aggregates are recorded: worker lifecycle,
resource failures, neutralize/disconnect/reconnect, ViGEm failures, Spark poll
mode/row-limit changes, and final Spark `worker.stats`.

The trace never records HID usages/key names, per-key analogue values, typed
text, HID paths, user/home/arbitrary paths, settings contents, or per-poll/frame
events. VID/PID and technical report dimensions may appear solely to identify
the active backend.

## Machine evaluation

```text
python analyze_stability_trace.py HallJoyStabilityTrace.log
```

- PASS / exit 0: complete SparkLink/ViGEm scenario with no error;
- WARN / exit 1: structurally valid but incomplete scenario;
- FAIL / exit 2: fault, timeout, forced termination, error/fatal event,
  incomplete shutdown, lost/reordered event, overflow, or damaged format.

A PASS requires successful ViGEm initialization, SparkLink connection and
route traffic, real analogue changes and realtime notifications, poll modes
0/1/2, at least two row limits, stale/unplug plus successful reconnect,
neutralization/disconnection, and completion of every worker and the process.
