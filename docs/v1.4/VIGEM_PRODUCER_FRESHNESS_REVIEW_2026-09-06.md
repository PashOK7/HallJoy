# RM-05 — output producer freshness review

Date: 2026-09-06. Scope: parent realtime producer versus isolated ViGEm child.

## Evidence

The child publishes a healthy heartbeat every 25 ms. Its `progressSequence` also
advances on that heartbeat, so it proves only that the child wait loop survives.
It cannot prove that `Backend_Tick` still calculated an output frame. A wedged
parent can therefore leave a previous nonzero XUSB report applied indefinitely
while the child continues to look healthy.

## Options considered

| Option | Assessment |
|---|---|
| Use child heartbeat or applied publication sequence | Rejected: neither changes when the parent calculation stalls. |
| Use timestamp of the last changed XUSB report | Rejected: a held key is valid progress even when its report is unchanged. |
| Parent-written generation-bound progress lease after every successful calculation | Chosen: distinct writer/owner, supports held input, and lets the child neutralize without UI involvement. |
| Kill/restart the child on every missing frame | Rejected: neutralization is required first; restart policy belongs to the existing supervisor. |

## Chosen contract

Three existing `reservedControl` words become a versioned parent-only producer
lease: generation, monotonically increasing sequence, and monotonic tick ms.
The parent refreshes it only after a successful `Backend_Tick` calculation;
snapshot contention does not prevent lease progress. The child accepts it only
for its launch generation. After a bounded lease expiry it sends one neutral
report to every target, records a diagnostic reason, and refuses to reapply an
old nonzero snapshot. A newer producer lease plus a newer snapshot resumes
normal output. Stopping/owner exit remains governed by the existing lifecycle.

The exact deadline and fake-clock state matrix are part of the implementation
package; no heartbeat field is repurposed and no ABI size is changed.

## Implemented contract (2026-09-06)

The wire version is now 2. `SharedStateV1` remains 640 bytes: the existing
control capacity carries `{generation, sequence, tickMs}` and each 128-byte
snapshot slot now records the producer lease sequence that calculated it. The
single parent writer commits generation and monotonic `GetTickCount64` time
before incrementing the sequence; the child double-reads the sequence and
checks the active generation before accepting it.

The deadline is 200 ms. It is ten times the documented maximum 20-ms realtime
interval, so normal scheduler jitter and unchanged held input remain live; the
child's 25-ms wait makes observed neutralization bounded by about 225 ms after
the last calculation. A child gets one initial 200-ms grace period after Ready.

`Backend_Tick` refreshes the lease only after it has built the complete output
report set, even where report deduplication means no snapshot needs sending.
The realtime-fault neutral publication also refreshes the lease once, so its
immediate safe frame is admitted without pretending that subsequent ticks run.

On expiry the real child applies exactly one all-pad neutral frame, publishes
the sticky `kChildDiagnosticProducerStalled` health fact, and retains the lease
sequence at which it neutralized. It accepts resumption only from a fresh lease
and a snapshot whose bound lease sequence is strictly newer, so a queued
pre-stall nonzero frame cannot revive held input. Normal stop/reap ownership is
unchanged.

Verification: the fake-clock portable test covers startup grace, deadline
boundary, stalled parent, unchanged held input, and wrong generation. The
channel test covers cross-process lease visibility and the all-project static
audit pins the child neutral/replay rules. Hardware and real-driver execution
remain deliberately pending.
