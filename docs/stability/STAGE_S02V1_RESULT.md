# S02V1: temporary evidence-based tracing

- Date: July 30, 2026
- Baseline: S02B.3 Addressed exception boundaries
- Status: Local implementation and V0 complete; Windows/MSVC and SparkLink
  evidence gate pending

## Rationale

A manual “everything works” report is insufficient to close a runtime package.
A bounded trace was required to prove that the intended scenario actually ran,
that no fault, timeout, or forced termination occurred, and that shutdown
completed correctly. S02V1 does not itself correct or close a runtime risk; it
is temporary measurement infrastructure for S02 and later packages.

## Implementation

- `stability_trace.h/.cpp`: compile-time `S02V1` trace;
- `analyze_stability_trace.py`: deterministic PASS/WARN/FAIL analysis;
- `stability_trace_static_audit.py`: bounded-design audit and synthetic trace
  cases;
- `COLLECT_STABILITY_TRACE.cmd` plus `collect_stability_trace.ps1`: package the
  current/previous logs after HallJoy closes;
- separate protocol, privacy, and performance documentation.

Tracing is present only with `/p:HallJoyStabilityTrace=true`; without
`HALLJOY_STABILITY_TRACE`, calls compile to no-ops. It covers session/shutdown,
realtime, overlay, backend and ViGEm lifecycle, SparkLink modes/hotplug/stats,
and MAD68/Hex80/Addressed boundaries. It does not alter polling, packets,
normalization, routing, output scheduling, or control values.

## Performance design

Synchronous event writes were rejected because a problematic disk could delay
join. The accepted design uses one preallocated 1 MiB memory mapping, performs
no event-time write or flush, records no per-poll/per-key event, flushes once
after `App_ForceFinalShutdown`, and treats overflow as `FAIL`.

Two trace-design defects were found and corrected: sequence allocation now
shares the append lock, preventing physical line/sequence reordering; and the
capacity marker is a structured, reserved-space event that disables further
recording. `worker.stats` plus scenario requirements prevent a mere connection
event from being accepted as meaningful coverage.

## Validation and acceptance

All static and portable C++ tests, GCC/Clang C++20, Python syntax, MSVC project
XML parsing, synthetic analyzer cases, and ASan/UBSan with `-Werror` passed.

The required physical session included 30 seconds of active input, all three
poll modes, at least two row limits including Unlimited/0, unplug with an
analogue key held, reconnect and renewed ViGEm activity, clean HallJoy shutdown,
and collection of `HallJoyStabilityTraceBundle.zip`. PASS accepts the evidence;
WARN requires a repeat; FAIL blocks the next feature package.

After acceptance, only the analyzer report and bundle SHA-256 should remain.
Events exclusive to already proven boundaries are removed, and the entire
temporary trace/analyzer/build flag/collector must be removed before release
unless a separate decision approves opt-in support tracing.
