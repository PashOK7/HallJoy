# F3 ViGEm production route design

Date: 22 August 2026.

Scope: atomically replace the legacy in-process ViGEm worker with the already
proved self-hosted output process. This package does not change the signed
ViGEmBus driver package or its installer.

## Rejected approaches

1. Patch the legacy in-process worker with more locks and handle checks.
   Rejected because a synchronous `vigem_target_x360_update` stall would still
   be inside the HallJoy UI/realtime process and could not be safely cancelled.
2. Add a temporary second production runtime and switch between old/new owners.
   Rejected because it creates an overlap mode, duplicates lifecycle state and
   would be discarded immediately after qualification.
3. Let UI/settings callers stop the child and close publication themselves.
   Rejected because stop ordering and raw-handle ownership would again be split
   across threads.

## Selected approach

Add one `VigemOutputRuntime` component. It owns one process-lifetime
`OutputProcessSession`, one dedicated parent owner thread and one private
control event. The realtime path can only publish a complete latest-value XUSB
snapshot through a bounded lease. UI/settings callers can only replace the
desired immutable configuration generation.

The same production package removes all direct ViGEm client/target calls,
legacy mailbox, closeable wake event, legacy output thread, poison lifecycle
and reconnect owner from `backend.cpp`. There is no build or runtime mode in
which both owners can submit reports.

## Required ordering

Every planned stop, forced recovery and configuration replacement follows:

1. disable the active output generation;
2. signal the child stop or force/reap it after the hard deadline;
3. verify the old process is gone;
4. wait for any already-admitted parent publisher to leave its bounded lease
   before reusing or closing the mapping;
5. only then begin the replacement generation and request the newest report.

The generic process supervisor receives one owner-thread prepare-to-reap hook.
The output session uses it to close generation admission before any child
stop/force. Producer quiescence is a separate post-reap resource-reuse gate;
making it part of the child stop deadline was rejected because a preempted but
already-invalidated publisher could falsely poison an otherwise clean stop.
Failure of either boundary can never skip containment/reap or admit a successor.

## Concurrency contract

- Session resources are created before publication is admitted and are closed
  only after publication is disabled, the owner thread is joined and the outer
  session-access lease count reaches zero.
- Realtime never waits for the child, the driver, the owner thread or a mutex.
- Configuration changes coalesce by revision and restart exactly one child
  generation; the private control event is not recreated between generations.
- A child fault, exit or progress timeout is reaped before retry. Retry has a
  bounded backoff to prevent a missing bus from becoming a tight process loop.
- The runtime publishes an immutable health snapshot; UI code never waits or
  closes a raw process/thread/job handle.
- Production converts ViGEm SDK reports to the fixed 12-byte IPC report at the
  backend edge. No SDK pointer or structure crosses the process ABI.

## Output semantics

Whenever at least one pad is due, production publishes the newest complete
snapshot and marks every configured pad valid. Updating up to four tiny XUSB
reports avoids losing a due pad when an older ready slot is reclaimed by a newer
latest-value snapshot. The existing per-pad scheduler still determines when a
publication is due; driver work remains outside realtime.

## Mandatory gates

- native unit/static suites and full MSVC rebuild;
- static absence of legacy owner tokens and direct ViGEm calls in `backend.cpp`;
- prepare-before-stop ordering test for planned and forced termination;
- routed startup, newest-value, configuration restart, child fault/stall
  recovery, neutral/remove and zero-survivor tests;
- legacy O1/O2 runners changed from expected RED to target PASS;
- O5 stress plus exact physical IROK and DrunkDeer qualification before release.

## Implementation result

The atomic route switch is implemented. `backend.cpp` now contains neither the
legacy output thread/mailbox/wake/reconnect owner nor a direct ViGEm client,
target or update call. One `OutputRuntime` owns the process session and its
parent supervisor thread. Realtime publishes a complete configured-pad snapshot
only through `OutputRuntime::TryPublish`.

MSVC exposed one deeper lifecycle distinction during the exact self-host gate.
The first implementation used `activeOutputGeneration` both as parent
publication admission and as child identity. Correct prepare-before-stop closed
that field, which then incorrectly prevented the exact child from recording
Stopping/Stopped plus neutral/remove completion. Delaying admission closure and
accepting an unqualified stop were both rejected. The corrected channel keeps
publication admission separate from the already-launched child's immutable
generation/PID identity. A successor is still forbidden until confirmed reap.

O5 then exposed two further boundary errors before release. First, a producer
could race a virtual-pad-count change and publish a differently shaped snapshot
to the old/new child. The channel now admits only the pad count committed to the
exact active generation; a boundary value returns immediately for retry and
never reaches ViGEm. Second, pre-stop admission closure and post-reap producer
drain had been conflated. They are now separate invariants, so a preempted
publisher cannot create a false unsafe stop, while mapping reuse still waits for
both the child reap and the outer/session leases to drain.

## Local qualification result

- all static/portable native checks pass, including 1,008 supervisor
  generations with prepare-before-reap and zero overlap;
- the exact same-image fake suite passes nine lifecycle generations and leaves
  zero child survivor;
- the exact real ViGEmBus child creates four pads, applies a complete snapshot,
  neutralizes/removes all four and restores the PnP baseline;
- the normal routed analogue simulator acknowledges child-side publication;
- routed child exit and progress-stall scenarios reap generation 1 before
  generation 2 begins, recover output, close cleanly and leave zero survivor.
- the exact runtime O5 command passes at least 100,000 publications across 101
  child generations, 50 topology changes and 10 disable/enable cycles. Twenty
  independent repetitions passed: at least 2,000,000 publications, 2,020 child
  generations, 1,000 topology changes and 200 disable/enable cycles, with no
  channel rebuild, unsafe generation, overlap or survivor;
- final clean Release x64 rebuild passed with 0 errors and only the allow-listed
  external ViGEmClient LNK4099. The resulting local production artifact is
  8,518,144 bytes, SHA-256
  `DADFF635AD4681DA3B4D8EAF7CC97BADD5A30541D2075910CABDE81889269B37`.

Code route and local F3 gates are complete. The release risks remain
`HARDWARE_PENDING` until long-duration active-input soak and one immutable final
artifact pass the IROK MG75 Max and DrunkDeer G65 physical scenarios. Evidence is retained in
`docs/stability/tests/V14_F3_VIGEM_OUTPUT_PRODUCTION_ROUTE_2026-08-22.txt`.
