# UAP Provider V2 split data-plane design

Date: 2026-08-23.

Package: R2-B2i. Status: B2i-a and B2i-b complete; B2i-c pending.

## Problem and evidence

R2-B2h removed the fixed eight-owner limit inside the UAP producer and proved a
complete 12-owner capture. The next boundary remains fixed:

- `HallJoyAnalogHost::SharedState` embeds eight `denseDevices`, eight V2 devices
  and the corresponding fixed V2 sample array;
- both parent and child map that monolithic section with all-access rights;
- parent `uap_parent_snapshot::SnapshotV1` repeats the same fixed arrays;
- increasing capacity cannot resize a live Windows file mapping and silently
  truncating would falsely label an authoritative snapshot complete.

Production still consumes dense compatibility. Physical configured-output
equality proved semantic mapping, not this capacity or ownership boundary.

## Required invariants

1. The complete V2 payload has one writer: the current isolated UAP child.
2. The parent owns mapping lifetime but its payload view is read-only.
3. Capacity is derived from producer demand, checked for overflow and bounded by
   the existing 4,096-device/1,048,576-sample defensive ceilings.
4. A parent reads one immutable committed generation or no V2 snapshot. It never
   composes headers/devices/samples from adjacent publications.
5. Dense and V2 equality/shadowing uses an explicit same-capture token. Timing
   proximity is not coherence.
6. Resize occurs only between fully reaped child generations. No writer retains
   a pointer into the replaced section.
7. Snapshot capture performs no allocation, device I/O, wait or digital-key
   correlation. Parent storage is sized before the generation becomes Ready.
8. Invalidation revokes the commit; it does not synchronously clear a potentially
   large payload.
9. Unknown, truncated, unstable or over-capacity data fails closed while B2i
   leaves dense production output unchanged.

## Compared implementations

### A - Enlarge the existing arrays

This is a small diff but retains one arbitrary limit, all-access ownership and a
monolithic control/data ABI. It cannot satisfy negotiated capacity. Rejected.

### B - Replace SharedState with one resizable all-access mapping

A variable layout fixes static size, but Windows mappings are not resized in
place. Recreating the only mapping would also replace stop/status/telemetry and
dense rollback state, while both processes could still write payload bytes.
Rejected.

### C - Copy every snapshot through a pipe or RPC broker

This creates clear access ownership but adds serialization, another copy, queue
capacity, backpressure and a second wake boundary to every input generation. It
is appropriate for control/health, not the high-rate immutable data plane.
Rejected for payload.

### D - Separate parent-owned, parent-read-only, double-buffered mapping

The parent creates one variable-size section per plane generation, maps it
read-only locally and gives the inherited handle to exactly one isolated child.
The child maps it writable. Two slots allow the child to construct a complete
inactive snapshot before commit and allow the parent to find the slot matching
the dense transaction even if the next V2 publication has begun. Selected.

## Data layout

The section contains:

```text
fixed mapping header
slot 0: commit header | AnalogSnapshotHeaderV2 | devices[capacity] | samples[capacity]
slot 1: commit header | AnalogSnapshotHeaderV2 | devices[capacity] | samples[capacity]
```

All offsets, strides and total size are aligned and calculated with checked
addition/multiplication. Header fields include magic/version/structure sizes,
total mapping bytes, plane generation, launch nonce, capacities, offsets and
slot count. A section is rejected before pointer arithmetic if any declaration
does not match the locally calculated layout.

Each slot has an aligned sequence plus a nonzero transaction token. Odd sequence
means writing; even nonzero sequence means committed. The child writes the
inactive slot under an odd sequence, copies header/devices/samples, validates the
complete result, issues a release barrier and commits even. It then publishes
the same token and V2 header metadata inside the legacy dense seqlock.

The parent first captures the dense control transaction, then searches both V2
slots for its token. It copies into already-sized vectors, validates the semantic
V2 snapshot, rechecks the slot sequence and finally rechecks the dense seqlock.
Any movement discards V2 for that tick. Dense compatibility remains independently
valid.

## Capacity negotiation and replacement

The first child generation may start with a header-only zero-capacity plane. It
calls the negotiated producer export with zero V2 capacity, records exact
required device/sample counts in the bounded control mapping and requests a
controlled restart. The supervisor:

1. rejects zero/inconsistent, overflowing or over-ceiling demand;
2. stops and reaps the requesting child;
3. unmaps/closes the old V2 section;
4. creates the exact new section and a new plane generation;
5. maps the parent view read-only and only then launches the next child;
6. accepts Ready only after one authoritative commit fits that section.

Topology growth repeats the same transaction. Shrink does not require immediate
remapping; capacity is an allocation bound, while every snapshot still declares
exact current and required counts. A later restart may compact it. This avoids
resize churn without treating capacity as device truth.

## Failure and lifecycle behavior

- Child crash/timeout: existing supervisor invalidates the dense/V2 commit,
  reaps the child and may reuse capacity with a new plane generation.
- Required capacity grows: current V2 becomes unavailable, the old child is
  reaped, then the parent replaces the mapping. No live pointer crosses resize.
- Required capacity exceeds policy or arithmetic: V2 fails closed and the error
  is terminal for that generation; B2i does not change dense production output.
- Torn/corrupt slot, wrong nonce/generation/size or semantic validation failure:
  skip V2 and increment bounded health counters; never fall back to old V2 bytes.
- Parent capture contention: bounded retries only. Realtime never waits for the
  writer.

## Ordered implementation

### R2-B2i-a - Portable layout and publication contract

- checked layout calculation and validation;
- two-slot address/view helpers;
- greater-than-eight-device and greater-than-fixed-sample old-bug oracle;
- overflow, ceiling, corrupt-offset, odd/torn slot and stale-token rejection;
- no production route or Windows handle change.

Checkpoint: **PASS on 2026-08-23**. The production-compiled portable layout is
128-byte mapping header plus two independently committed 64-byte-aligned slots.
Checked calculation and exact local-layout validation pass for 0, 1, 8, 12, 32
and defensive-limit capacities. A 12-device/60-sample authoritative snapshot
passes, while wrong generation/nonce/token, odd or uncommitted sequence, corrupt
offset/stride, false completeness and truncation fail closed. A static audit
also proves that `backend.cpp` cannot select this plane and that the fixed live
`SharedState` arrays still exist. This is deliberately a foundation checkpoint,
not a live IPC or compatibility claim.

### R2-B2i-b - Windows ownership and resize lifecycle

- separate inherited mapping in the explicit process handle list;
- `FILE_MAP_READ` parent payload view and child-writable view;
- zero-capacity demand handshake, exact allocation and generation-bound restart;
- crash during publish, resize race, invalid handle, excessive demand and repeated
  topology change tests with zero surviving child/mapping writer;
- dense production route remains selected.

Checkpoint: **complete/PASS on 2026-08-23**. The parent now creates
the variable section, keeps only a non-inheritable read handle/view and drops
its transient writer handle immediately after `CreateProcessW`. The child alone
inherits a read/write section handle through an explicit handle list. Initial
zero capacity reports exact demand through the bounded control plane; only a
fully reaped child can authorize growth and a new plane generation. Shrink fits
the existing allocation and excessive demand fails closed.

The real Windows process regression rejects a forged numeric handle, rejects a
child terminated on an odd commit, blocks replacement under a live reader,
publishes six 1/8/12/32/8/1 topology generations and leaves no child or writer
capability. During the first exact-EXE attempt the parent crashed because an
`InterlockedCompareExchange64` used as a read is still a read-modify-write and
cannot touch the deliberately read-only view. It was replaced with an aligned
volatile load plus barriers and the process regression now exercises that
read-only capture contract.

The exact physical zero-capacity -> restart -> authoritative commit test passes
on both the corrected direct MSVC image and the final officially packaged image.
The final SHA-256 is
`1EAAFAD31C19AE9C3DC75D37E6081BD0120CB156DCBA9C5C95719D6A6F0F4EFF`.
Official private-ABI/runtime checks and a sequential ten-second production
startup/shutdown smoke also pass with no continuous/crash log and no surviving
process. The leaf ABI checker, exact dual-capture smoke, official build runtime
gate and production smoke refuse concurrent HallJoy before opening hardware.
This is test isolation, not a product singleton or an exclusive-HID policy.

### R2-B2i-c - Live shadow migration

- reusable parent-owned dynamic snapshot storage sized before Ready;
- same-token dense plus V2 capture and existing semantic dual validation;
- provider shadow/qualification consume the new plane;
- remove V2 device/sample payload arrays from monolithic `SharedState` and fixed
  parent snapshots; keep only bounded control/commit metadata required through
  dense rollback;
- exact dual-capture, production smoke and representative hardware shadow
  regression before route selection;
- after those gates pass, make Provider V2 the immutable route of the normal
  local engineering build. Keep the dense implementation compiled/tested and
  selectable only by an explicit legacy build property for a separately named
  emergency/user-test artifact. No runtime or persisted fallback is allowed.

#### Parent live-snapshot handoff

Four implementations were compared before starting the live migration:

1. Taking an SRW lock and resizing/copying vectors in `Backend_Tick` was
   rejected. A topology change could allocate or block the realtime thread.
2. Returning pointers directly into either mapping slot was rejected. The child
   is allowed to reuse an inactive slot immediately after commit, so a pointer
   retained through controller construction would not remain immutable.
3. Allocating the defensive maximum as fixed process arrays was rejected. It
   would turn policy ceilings into permanent working-set cost and recreate the
   fixed-capacity design at a much larger size.
4. A bridge-owned, preallocated, three-slot parent snapshot broker was selected.

The existing snapshot bridge is the single broker writer. On a child snapshot
event it acquires a short mapping read lease, finds the V2 slot named by the
bounded dense/control transaction, copies header/devices/samples into an unused
pre-sized broker slot, validates semantics and rechecks both commit sequences.
Only then does it atomically publish that broker slot and wake realtime.

`Backend_Tick` acquires a nonblocking broker read lease using bounded atomic
rechecks. The lease exposes an immutable header and exact device/sample spans;
it performs no allocation, wait, device I/O or payload copy. A writer never
modifies the published slot or a slot with readers. If no free slot exists, the
bridge drops the intermediate publication rather than delaying realtime.

Before a plane resize/restart, the supervisor revokes broker publication and
waits outside realtime for the bridge writer and outstanding leases to drain.
It then resizes all broker slots before launching the replacement child and
before that generation may become Ready. Failure to drain or allocate fails the
new V2 generation closed and retains resources as required by the existing
poison/restart policy; it never frees storage still visible to a reader.

The child obtains the dynamic V2 plane and its dynamic per-device dense proof in
one `halljoy_get_dual_snapshot_v2` call after capacity negotiation. The same
transaction token and committed slot are published inside the legacy dense
seqlock. This replaces the current two independent fixed/dynamic acquisitions;
timing proximity is not accepted as same-capture evidence. The zero-capacity
generation may use the provider-only call solely to report exact demand and
cannot publish a live snapshot.

The local route selection is deliberately part of the end of B2i-c rather than
B2i-a/b. Selecting today would still consume V2 bytes from fixed `SharedState`
and would not exercise the new transport. Waiting until B2j would leave the new
route unused until release promotion. D-075 records this middle boundary.

## Required gates before R2-B2j

- old fixed-capacity implementation fails the greater-than-eight-device oracle;
- 0, 1, 8, 12, 32 and defensive-limit capacities calculate deterministically;
- arithmetic/offset corruption and false-complete snapshots fail closed;
- parent process has no writable V2 payload view;
- no allocation or wait occurs in `Backend_Tick` capture;
- topology resize cannot overlap a live old child and never publishes stale
  nonzero state;
- live configured shadow retains zero mismatches on representative hardware;
- full static/portable/native suite, MSVC Release x64, exact UAP ABI, child fault,
  reconnect and production lifecycle gates pass;
- both output routes compile and have direct regressions, and an exact artifact
  identifies its immutable selected route;
- before B2i-c completion every build continues publishing dense compatibility;
  after completion the normal local engineering build publishes V2 while the
  explicit legacy artifact remains dense;
- V2 loss/corruption fails the affected source closed without automatic dense
  substitution, and cannot transfer stateful output history between routes.

## Rollback boundary

Each subpackage has a hash-verified backup. B2i-a is unreachable support code;
B2i-b can disable the separate plane while retaining the old dense mapping;
B2i-c retains dense output until its live-plane gates pass, then selects V2 only
for the normal local engineering build. An explicit separately named legacy
artifact remains available without runtime fallback. R2-B2j alone may promote
V2 to users or remove dense runtime compatibility; no B2i package changes a
published keyboard compatibility claim.
