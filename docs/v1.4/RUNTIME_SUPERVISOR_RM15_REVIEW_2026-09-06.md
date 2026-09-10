# RM-15 Runtime supervisor review (2026-09-06)

## Decision

`runtime_supervisor` is the sole periodic recovery owner for the already
initialized realtime loop and isolated ViGEm output runtime. It runs every
500 ms and is deliberately narrow: it does not enumerate/open HID devices,
initialize a provider, call UI code, or acquire an input hook.

## Owners

| Resource/action | Owner |
| --- | --- |
| Window messages, hooks, overlay UI and status presentation | UI thread |
| Realtime start/stop recovery after successful application startup | Runtime supervisor |
| Provider/native start and stop | Startup transaction / shutdown transaction |
| Isolated ViGEm child generation reaping/recreation | Output runtime, invoked by runtime supervisor |

The UI timer retains UI-only work. It no longer calls `RealtimeLoop_Stop`,
`RealtimeLoop_Start`, or `Backend_EnsureOutputRuntimeHealthy`.

## Lifecycle and containment

The application starts the supervisor only after the realtime/native startup
transaction is complete. Any later startup failure rolls it back before native
or realtime dependencies. Shutdown first removes timers/hooks, then requests a
bounded five-second supervisor join before overlay, native, realtime or backend
teardown. An incomplete join retains its thread/event handles, poisons the
generation, skips dependent cleanup and requires immediate process exit.

This preserves the existing no-`TerminateThread`, no-detach lifetime policy.
The runtime supervisor adds at most one 5-second join attempt. The application
uses a 30-second outer process-containment deadline, chosen to cover one
in-flight supervisor recovery plus bounded output/UAP containment. It remains
an absolute last resort, not an allowance to sum or extend local waits.

## Scope boundary

RM-16 owns user-visible pause/resume and device-lease release. No artificial
pause command was added here: there was no existing whole-engine pause contract
to safely route through the supervisor. The only commands presently owned are
startup, bounded recovery and shutdown.

## Evidence

`runtime_supervisor_static_audit.py` verifies product-project inclusion,
startup/rollback/shutdown ordering, removal of UI recovery calls, bounded
join/poison/handle retention, and the absence of forced thread termination.
Physical UI-free recovery and an actual output fault remain hardware/runtime
validation work; no HallJoy process, HID device, controller, output child, or
ROG diagnostic was launched for this review.
