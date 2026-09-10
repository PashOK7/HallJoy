# RM-28 observability review (2026-09-06)

Production logging remains disabled in ordinary release builds. Diagnostic
logging is queued to one writer with bounded flush cadence; the structured
trace is separately bounded, sequence-ordered, redacted at its final sink and
does not synchronously flush from ordinary lifecycle/realtime writes.

The trace analyzer now adds deduplicated evidence labels without changing any
producer: `INPUT_STALE`, `PRODUCER_STALLED`, `OUTPUT_FAILED`,
`STORAGE_FAILED`, and `INCOMPLETE`. The raw original event is retained and
unknown critical events remain failures, so labels never hide root cause.

Static fixtures prove sequence/error/worker analysis and each label. Disk-full,
lock-contention, real capture overhead and a support-capture workflow remain
runtime/release evidence, deferred while no HallJoy process may disturb active
gameplay.
