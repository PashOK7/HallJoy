# RM-26 raw input, hooks and mouse integration review (2026-09-06)

## Fixed boundary

The first `GetRawInputData` size query is external input. The UI previously
resized its thread-local receive buffer from that value before it could validate
the packet type or typed payload. It now rejects an envelope larger than 64 KiB
before allocation. The existing post-receive `ContainsTypedPayload` validation,
relative-mouse-only policy and saturating `[-32768, 32767]` raw-delta accumulator
remain in force.

## Input ownership and release matrix

| Situation | Hook/input result | State cleanup |
|---|---|---|
| Normal mouse-to-stick | Raw mouse deltas enter only while backend admission is open; low-level mouse hook records bind buttons/wheel and blocks physical mouse only when configured, enabled, unfocused and not temporarily paused. | Realtime consumes delta atoms; backend reset/shutdown clears deltas, held buttons, wheel pulses and filtering state. |
| Right Shift temporary pause | Keyboard hook always passes Right Shift through when it is the configured mouse-block escape route. | It flips the mouse IPC pause state; blocking/cursor clipping stops immediately. |
| Explicit engine Pause or failed Resume | UI bridge sets input pass-through before hook removal and closes backend admission before release. | Cursor clip is removed, hooks are removed, mouse IPC publisher closes, and backend reset neutralises retained state. |
| Device/focus/foreground change | Blocking is reevaluated per hook event; HallJoy's own foreground never blocks. `WM_INPUT_DEVICE_CHANGE` drives target-scoped native keyboard reset where compiled. | New/resumed generation starts with reset mouse state; stale pre-pause deltas cannot reappear after backend admission reopens. |
| Shutdown | Pass-through is set and cursor clip/hook state is released before owner stop. | Owner-controlled backend shutdown clears every mouse binding/delta/pulse; incomplete ownership stops retain resources and poison restart. |

`WM_INPUT` runs no global `SendInput`; injected low-level-hook packets are not
converted to bindings. Absolute mouse packets do not produce a relative delta.
The producer uses defined saturating arithmetic, so large valid relative deltas
cannot overflow before the controller-side clamp.

## Evidence boundary

The static arithmetic oracle now pins the pre-allocation size cap, while the
existing raw-packet portable test pins architecture-correct typed size handling.
The complete static audit suite passes. Manual verification of pause/focus/
lock-screen/device-removal sequences must occur later outside gameplay because
it deliberately exercises global hooks and controller output.
