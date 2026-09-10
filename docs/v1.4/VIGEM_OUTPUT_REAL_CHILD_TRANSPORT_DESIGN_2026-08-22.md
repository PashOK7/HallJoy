# ViGEm output real-child transport design

Date: 2026-08-22.

Package: `F2`, after the green F1.1 exact-image fake-transport boundary and
before any production routing change.

Status: selected architecture implemented; portable failure matrix, exact-image
real ViGEmBus gate and clean builds green; production route remains unchanged.

## Problem and exact scope

F1.1 proves the same HallJoy image can be launched as a contained output child,
consume complete generation-bound snapshots, acknowledge progress, be forcibly
reaped and qualify a clean stop. It deliberately invokes no ViGEm API. The
legacy `backend.cpp` worker therefore remains exposed to the old close/use race
and invalid-thread-handle poison.

F2 must replace only the fake transport action inside the already-proved child
with real ViGEm client and Xbox-target ownership. It must prove synchronous
client connect, target add, report update, neutral update, target removal,
disconnect and free through the exact HallJoy image. It does not switch realtime
publication, delete the legacy worker, claim O5 or produce a user artifact.

## Invariants

1. One child generation has one immutable target count in `[1, 4]`.
2. `Ready` is published only after the exact configured number of targets was
   allocated and synchronously added to ViGEmBus.
3. An applied snapshot acknowledgement is published only after every selected
   `vigem_target_x360_update` returned success.
4. Planned stop attempts neutral on every added target, then attempts removal
   and free on every target even if an earlier operation failed.
5. Clean stop is acknowledged only if every neutral and every removal returned
   success. A process exit or best-effort destructor is never upgraded to a
   clean stop.
6. Allocation, connect, partial-add, update, neutral and removal failures retain
   the exact ViGEm error and failing phase/pad while still exhausting cleanup.
7. A hung SDK call remains inside the child. The existing F1 supervisor is the
   only hard deadline and reaps the whole process before replacement.
8. The shared ABI remains 640 bytes. Initial pad count consumes reserved control
   capacity and is generation-bound; no command-line ABI or named IPC is added.
9. The real transport has one RAII owner. Raw client/target values never enter
   shared memory, parent telemetry, UI state or realtime publication.
10. `backend.cpp` remains the sole production route until F2 passes and a later
    atomic route-switch package removes the legacy owner completely.

## Option A - put direct SDK calls in `vigem_output_process_host.cpp`

Benefits: smallest source diff and fastest successful-path prototype.

Rejected: lifecycle, partial-start rollback and exhaustive stop would become a
second hand-written block inside command parsing. Failure-path tests would need
to launch the real driver and could not deterministically prove every cleanup
edge. The host would mix trust validation, IPC, transport ownership and policy.

## Option B - move the complete legacy worker and globals into the child

Benefits: superficially preserves current reconnect and logging behavior.

Rejected: the legacy worker carries the closeable wake, raw thread lifecycle,
global flags, mailbox and poisoned recovery model which F1/R1.1 replaced. It
would transplant the old ownership defect across the process boundary and
create two competing protocols.

## Option C - extract one transport and immediately make legacy and child use it

Benefits: removes temporary SDK-call duplication and exercises the abstraction
through the current production route.

Rejected for F2: changing the production owner before the exact real-child gate
passes destroys the atomic rollback boundary. A regression could no longer be
attributed to transport extraction or route migration. The legacy worker is
deleted at the later atomic switch rather than partially modernized now.

## Option D - child-owned RAII transport with injectable SDK table

Add a narrow `VigemChildTransport` used only by the real child mode. It owns one
client and a fixed array of four target records. A default immutable function
table points to the real ViGEmClient API. Deterministic native tests provide a
fake table to force every failure phase and verify call order, continued cleanup
and exact error preservation. The production child uses the real table.

The generation request carries an explicit initial pad count. The parent writes
it and a matching configuration generation into reserved shared-control space
before child creation. The child validates both before touching ViGEm and
publishes the applied configuration generation with `Ready`. Configuration is
immutable for a child lifetime; a later pad-count change is a planned generation
replacement, not an in-place mutation racing report submission.

## Comparison

| Criterion | A: inline calls | B: legacy transplant | C: immediate shared extraction | D: child RAII transport |
|---|---|---|---|---|
| Ownership | mixed into host parser | imports old globals | cleaner object, premature dual-route change | one child-only owner |
| Failure cleanup | manual and hard to exhaust | inherits known lifecycle defects | testable but changes production early | exhaustive fixed-array cleanup |
| Liveness | process bound exists | process bound plus obsolete thread state | process bound | unchanged proved F1 bound |
| Configuration | likely implicit first report | global atomics | legacy globals leak into contract | immutable generation config |
| Testability | real-driver success only | fault injection coupled to legacy | good but attribution is mixed | fake exhaustive matrix plus exact real driver |
| Latency | off realtime | off realtime | off realtime | off realtime; one existing import-style indirection |
| Rollback | small but tangled host | very large | route already changed | isolated new module and control fields |
| Maintainability | parser becomes service monolith | duplicates obsolete architecture | eventual shape but wrong stage order | reusable narrow transport removed only with service |

## Selected implementation

Option D. The SDK table is dependency inversion, not a runtime fallback: the
production instance always receives the immutable real table and no policy can
select a fake implementation. Simulator fault modes remain separate from real
mode. The transport reports structured results; the host alone translates them
to child telemetry and exit codes.

`Stop()` is deliberately exhaustive. It first visits every added target with a
zero XUSB report, then visits every target for remove/free, then disconnects and
frees the client. It records the first failure while continuing later cleanup.
Its result separately states `neutralApplied` and `targetsRemoved`; only both
permit `ChildPublishStopped`.

## Required gates

- fixed ABI/offset tests with generation-bound pad-count configuration;
- deterministic transport tests for invalid count, alloc failure, connect
  failure, partial add failure at every pad, update failure, neutral failure and
  removal failure, including exact order and no leaked fake object;
- production-linked static proof that real mode uses only the RAII transport,
  fake modes cannot select the real SDK test adapter, and legacy routing is
  unchanged;
- MSVC Release x64 simulator and production builds with no new warning;
- exact simulator-image real-child run against the installed ViGEmBus: four
  targets created, a complete four-pad non-neutral report acknowledged, all
  four neutralized and removed, clean stop qualified, process reaped and no PnP
  or process survivor relative to the pre-run baseline;
- the existing nine-generation fake O3/O4 suite and full native suite remain
  green.

## Rollback boundary

The pre-F2 backup contains the shared ABI/channel, process session/host/protocol,
self-test, project files, native runner, decisions, architecture and unchanged
legacy backend. F2 adds the real transport module and exact real-child tests.
No production routing file is changed. Rollback restores the complete set; old
and new protocol files are never mixed.

## F2 implementation result

`VigemChildTransport` now owns one real ViGEm client and a fixed array of four
targets. Pad count is written with the generation into reserved shared-control
capacity; the mapping remains exactly 640 bytes. The child publishes `Ready`
only after all configured targets were added and explicitly initialized with a
neutral report. It acknowledges a snapshot only after every selected real SDK
update succeeds.

The deterministic fake API test exhausts client allocate/connect, all four
target allocate/add edges, all four initial-neutral edges, all four report
update edges, all four planned-neutral edges and all four removal edges. Cleanup
continues across all targets and leaves no fake object. The complete native
suite, prior 1,008-generation supervisor test and nine-generation fake exact-
image O3/O4 suite remain green.

On the local running ViGEmBus, the actual simulator image launched its exact
early real-child mode, created four Xbox targets, applied one complete nonneutral
four-pad snapshot, neutralized and removed all four, reaped the child and
returned the PnP device set to its pre-run baseline. Simulator and production
MSVC Release x64 rebuilds passed with no new warning. Evidence:
`../stability/tests/V14_F2_VIGEM_OUTPUT_REAL_CHILD_TRANSPORT_2026-08-22.txt`.

F2 does not close `HJ-V14-P0-005/006`: `backend.cpp` is hash-identical to the
pre-F2 backup and normal realtime output still uses the legacy worker. The next
stage is F3, a separately designed atomic parent-route switch and complete old-
owner removal, followed by routed O1/O2/O5 and physical qualification.
