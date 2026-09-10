# RM-22 realtime scheduler review (2026-09-06)

The current scheduler already implements the required control-plane shape:
durable input sequence before every wait, address wake only as a latency hint,
deadline-aware output scheduling, an exact high-resolution timer tail, and a
bounded heartbeat. A changed input sequence in either notify/wait window is
observed before a later wait; output deadline handling rebuilds from the newest
state rather than sending a fabricated intermediate report.

Diagnostic-only tracing collects input-notify-to-wake and tick duration
avg/p50/p95/p99/max without synchronous per-tick logging. Production does not
allocate those trace buffers. The scheduler therefore should not be rewritten
or have its priority/spin policy changed without an A/B measurement identifying
a real bottleneck.

The remaining RM-22 evidence is an actual matched workload baseline (idle,
burst, multi-device and multi-pad) and its A/B comparison. It requires a
controlled HallJoy runtime session, which remains deferred while the user may
be gaming. No runtime behavior was changed by this review.
