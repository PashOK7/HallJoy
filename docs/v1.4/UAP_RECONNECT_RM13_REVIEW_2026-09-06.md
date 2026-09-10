# RM-13 — UAP reconnect and invalidation review

## One-generation ownership map

`WM_DEVICECHANGE` → `Backend_NotifyDeviceChange` →
`AnalogHostClient_RequestDeviceRefresh` increments a coalesced request counter.
Only the analog-host supervisor consumes it.  It invalidates parent snapshots,
terminates the isolated child, confirms/reaps that child, retires the V2 plane,
then launches a fresh child enumeration.  The snapshot bridge and supervisor are
both lifecycle-owned workers; their failure invalidates the shared snapshot and
blocks unsafe replacement.

## Confirmed protections

- UAP periodic broad hotplug discovery remains disabled; no UI-thread HID
  enumeration is introduced.
- Restart occurs only after child reap.  A reap timeout retains the process
  handle, publishes error/neutral state and sets `restartBlocked`.
- Parent/child C++ and SEH faults, startup rollback, stop-before-ready, child
  unload hang, heartbeat failure and provider-plane resize all invalidate the
  current publication before reuse.
- The V2 plane and parent snapshot broker are generation-bound; a prior child
  cannot commit into the replacement generation.

## Evidence boundary

Static gates cover the ownership chain and simulator-only fault injection.
Physical UAP unplug/replug evidence is still required; no broad polling or
runtime test was run in the current gaming session.
