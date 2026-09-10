# RM-16 engine-runtime owner design (2026-09-06)

## Purpose

Pause/Resume must release every HallJoy device lease without closing the UI. A
button that only removes hooks, stops realtime, or hides output is not a valid
implementation: vendor HID, UAP child resources, native readers, protocol
mutexes and virtual targets can otherwise remain alive.

The implementation therefore introduces one `EngineRuntimeOwner` command worker.
It is the only normal owner allowed to start, stop, or recover the aggregate
engine generation. `runtime_supervisor` remains a subordinate recovery worker:
it may recover the already-running realtime/output components, but is stopped
before the owner begins a release transaction and must never independently
resume a paused engine.

## Ownership boundary

`EngineRuntimeOwner` owns these operations in one serial command stream:

1. native routing reset/preparation and every provider-start generation;
2. `Backend_Init`, `Backend_Shutdown`, UAP child lifecycle and ViGEm lifecycle;
3. realtime start/stop and the subordinate runtime-supervisor start/stop;
4. all native backend phase start/stop operations;
5. the authoritative command state and admission decision.

The UI owns presentation-only resources: hook handles, cursor clipping, mouse
IPC publication, dependency dialogs and visible status. It receives a posted
operation token and acknowledges completion without calling provider/HID APIs.
The owner may wait only on that acknowledgement with a bounded deadline. On
window close, cancellation is signalled before joining the owner so the UI never
deadlocks while the owner waits for a message that can no longer be dispatched.

No direct `Backend_Init`, `Backend_Shutdown`, native phase start/stop or
`RuntimeSupervisor_Start/Stop` call may remain in startup timers, page handlers,
or device-change handlers after this migration. Initial startup and the existing
degraded MAD68 retry enter the same command owner; only a pre-explicit-pause
initial-recovery policy may retry a missing device. A user-issued Pause disables
that policy permanently for the process generation until an explicit Resume.

## Transaction

### Pause

1. Atomically close backend admission. Input callbacks and topology changes then
   cannot revive a generation.
2. Stop `runtime_supervisor`; an unconfirmed join is `PauseFaulted` and retains
   resources.
3. Publish one complete neutral backend/output snapshot, then stop realtime so
   a later tick cannot overwrite the neutral report.
4. Request the UI to pass input through, release hooks/cursor capture and close
   mouse IPC. The UI acknowledgement is bounded and required.
5. Stop every native provider and phase in reverse lifecycle order.
6. Run `Backend_Shutdown`, which performs the confirmed ViGEm neutral/remove and
   UAP/pre-UAP native release path.
7. Enter `Paused` only after every stop reports confirmed release. Any
   incomplete owner is retained and the state remains `PauseFaulted`; the UI
   must never display successful release in that case.

### Resume

1. Stay admission-closed and enumerate from scratch; never reuse a former HID
   path, layout proof, capability proof, UAP generation or virtual target.
2. Perform native routing preparation and `Backend_Init`; dependency guidance is
   requested from the UI as a bounded operation, not called by the worker.
3. Start realtime/native phases and the subordinate supervisor transactionally.
   Rollback follows the same confirmed-release rules.
4. Publish a fresh neutral generation, request UI hook/mouse-IPC restoration,
   then open admission and enter `Active`.
5. An ordinary unavailable/failed fresh proof returns to `Paused` after cleanup
   and permits another explicit Resume. A failed release or unconfirmed join is
   `PauseFaulted` and never permits overlap.

## Required proof

- pure command-state tests, including resume abort and fault barrier;
- static proof that closed admission gates tick, input and topology paths;
- production-linked owner-order oracle proving no second lifecycle owner;
- fake/UI bridge timeout and close-during-operation tests;
- no-survivor tests for all Pause phases and repeated Pause/Resume cycles;
- hardware/web-driver coexistence only after the preceding gates pass.

This design deliberately does not add a UI pause button yet. The button is the
last presentation step after the owner can truthfully report `Paused`.
