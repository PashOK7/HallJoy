# ViGEm output exact-EXE self-host fake-transport design

Date: 2026-08-22.

Package: `F1.1`, prerequisite for moving any ViGEm library call out of the
legacy output worker.

Status: selected architecture implemented and exact-EXE fake-transport gates
green; real ViGEm child transport and production route remain pending.

## Problem / evidence

R1.1 proves the fixed shared ABI and claimed-slot newest-value channel in a
dedicated parent/child test executable. F1 proves a generic contained process
generation with explicit handle inheritance and bounded reap in a separate
self-spawning test executable. Neither result proves that the real HallJoy
entry point can become an output child before logger, installer, UI, GDI+,
analogue-provider or backend startup, nor that the two primitives compose
without a stale-generation or incomplete-stop gap.

The legacy output path remains the sole ViGEm owner and O1/O2 remain RED. No
ViGEm call may move until the exact HallJoy executable exercises O3/O4 using a
fake transport.

## Affected invariants and scope

1. The same built HallJoy image is both parent and child; no second packaged
   helper identity exists.
2. `--halljoy-vigem-output-host` is dispatched before every logger, installer,
   UI, GDI+, analogue-host client and backend action.
3. The child accepts only an exact internal command shape and four explicitly
   inherited unnamed objects: mapping, wake, child-stop and owner-process.
4. The child verifies ABI magic/version/size, owner PID/handle identity, nonce,
   active generation and containment before publishing `Starting` or `Ready`.
5. Child telemetry has a generation and a transactional sequence; stale or
   torn telemetry cannot admit or sustain a replacement.
6. Shared mapping and wake event outlive every child generation in a session.
   Realtime-style publication remains bounded and never waits for lifecycle.
7. The generic supervisor remains the only process/job owner. The output
   adapter adds output postconditions but cannot expose or close raw process
   handles.
8. Generic `PlannedStop` is insufficient: successful output stop additionally
   requires the matching `completedStopGeneration`, neutral acknowledgement
   and target-removal acknowledgement.
9. A stalled child is killed and reaped before a replacement begins. The
   replacement consumes a freshly published newest complete four-pad snapshot.
10. Fake transport and fault modes exist only in the simulator build. This
    package does not route `backend.cpp` through the child and invokes no ViGEm
    API.

## Old-bug oracle and new composition oracles

O1/O2 remain the preserved legacy RED evidence. F1.1 adds exact-EXE oracles:

- launch the actual `HallJoyV14Simulator.exe` as its own early output host and
  prove that it never reaches normal main initialization;
- exit before ready, after ready, before snapshot read, during fake update,
  after acknowledgement and during planned neutral;
- stall after ready beyond the progress deadline, force/reap the generation,
  then run a replacement with no overlap and exact newest-report equivalence;
- reject `PlannedStop` when the child exits during neutral without the matching
  clean-stop acknowledgement;
- finish with zero live child and a restart-safe persistent session.

## Option A - separate output helper executable

Build and package a dedicated `HallJoyVigemHost.exe`.

Benefits: smallest child dependency surface and a naturally separate entry
point.

Rejected: it creates a second signed/distributed/update identity, duplicates
version/resource/build policy and introduces packaging skew as a new failure
class. It would not prove the selected self-host architecture and makes atomic
rollback/update more difficult.

## Option B - extend the analogue-host implementation

Add ViGEm modes and shared-state variants to `analog_host_client.cpp` and its
existing supervisor.

Benefits: reuses a field-tested self-host command and diagnostic machinery.

Rejected: the analogue host is coupled to UAP globals, plugin paths, snapshot
bridges, debugger capture and provider-specific restart policy. Its older
launch path also does not embody the new common supervisor contract. Combining
input-provider and output-driver service policies would create ambiguous
ownership and make either service's recovery affect the other.

## Option C - direct self-test wiring around raw F1/R1.1 primitives

Add an early HallJoy child branch, but let the simulator test itself create all
objects, build command lines, probe fields and qualify stop directly.

Benefits: narrow implementation and an exact-EXE proof with little production
surface.

Rejected as the final F1.1 shape: it would prove test wiring rather than the
adapter later used by production. F2 would repeat resource ownership, command
construction, telemetry admission and clean-stop logic, allowing the tested
and shipped paths to diverge.

## Option D - early self-host plus reusable output-process session

Add three narrow production-linked components:

- an early child dispatcher/host that owns only inherited IPC objects and the
  output transport;
- an output-process session that owns the process-lifetime mapping/wake/stop/
  owner objects, composes R1.1 publication with the F1 supervisor and qualifies
  output-specific completion;
- a simulator-only exact-EXE suite that drives the same host and session with a
  fake transport and deterministic lifecycle boundaries.

The session is synchronous on its owner thread. A future production supervisor
thread may call it while realtime only calls the bounded publication method.
F1.1 does not create that production thread or change routing.

## Comparison

| Criterion | A: helper EXE | B: analogue-host graft | C: raw test wiring | D: reusable self-host session |
|---|---|---|---|---|
| Correctness | clean child, but new image-skew class | mixed input/output owners and policies | can prove one test but not later adapter | exact selected boundary and later production adapter |
| Latency | off realtime | off realtime but coupled recovery | publication can stay bounded | bounded R1.1 publish; lifecycle stays on owner thread |
| Liveness | possible with another supervisor | inherits unrelated supervisor behavior | test-specific | F1 hard reap plus output clean-stop qualification |
| Ownership | extra package/update owner | ambiguous service ownership | ownership duplicated again in F2 | one session for IPC, one generic supervisor for process/job |
| Data contract | version skew between images | parallel UAP/output schemas in one module | ad-hoc telemetry reads likely | explicit generation and transactional telemetry snapshot |
| Compatibility | second artifact to distribute | risks qualified UAP path | no route change, but throwaway code | no route change and direct staged migration path |
| Testability | tests wrong final image topology | failures hard to attribute | good exact-EXE test, weak production linkage | deterministic exact-EXE O3/O4 using production-linked adapter |
| Security/privacy | larger distribution surface | larger inherited authority/coupling | depends on test details | exact unnamed handle allowlist; no paths/raw input in mapping |
| Maintainability | duplicated entry/build/version policy | growing multi-service monolith | intentional duplication | narrow modules with one responsibility |
| Rollback | package-wide two-image rollback | entangled with UAP | isolated but discarded next stage | isolated new modules and entry/project/test additions |

## Chosen option

Option D. It is the smallest coherent stage that tests the architecture which
F2 will actually extend. It preserves the correct long-term split: generic
process ownership remains reusable, while output protocol and stop
postconditions remain explicit and ViGEm-specific calls can later replace only
the fake transport inside the child.

## Telemetry and stop contract correction

The 640-byte V1 layout has reserved health capacity. F1.1 names two reserved
64-bit fields without changing size or existing offsets:

- `childTelemetrySequence`: odd while the sole child writer updates telemetry,
  even when a parent may accept a complete scalar snapshot;
- `childGeneration`: present from `Starting`, so pre-ready observations are
  generation-bound and stale data is ignored.

The remaining reserved health capacity stays reserved. Child state updates use
documented Win32 interlocked operations. A child killed during an odd update
cannot leave an accepted torn snapshot; the next sole writer repairs the stale
odd sequence before publishing its own generation.

For a clean planned stop the fake child publishes `Stopping`, applies neutral,
removes its fake targets, publishes both diagnostic acknowledgements and only
then writes `completedStopGeneration` and `Stopped`. F2 replaces those fake
actions with real ViGEm neutral/update/remove operations without weakening the
postcondition.

## Rollback boundary

Before implementation, preserve hash-verified copies of `main.cpp`, the shared
ABI/channel, project/filter files and native runner. F1.1 consists of the named
ABI fields, child/session/self-test modules, early dispatch, project/test
integration and documentation. Legacy `backend.cpp` is outside the diff and
remains the only ViGEm caller.

## Required gates

- static order proof that output-host/self-test dispatch precedes every normal
  startup action;
- fixed ABI remains exactly 640 bytes and all channel/concurrency tests pass;
- actual simulator EXE launches itself through the exact command grammar and
  verifies job containment, owner identity and explicit handles;
- O3 forced stall/reap/restart/newest-equivalence pass;
- all six O4 lifecycle boundaries produce their expected truthful result;
- incomplete planned neutral is rejected by the output layer;
- normal apply and clean stop acknowledge exact generation and sequence;
- no named IPC; the F1 inherited-decoy gate remains green and the exact session
  statically exposes only its four intended handles; no child overlap, survivor
  or normal logger/UI startup path from child mode;
- full compiler-required native suite and MSVC x64 simulator/production builds;
- static proof of no ViGEm call in the new modules and no `backend.cpp` route.

## Documentation to update

`DECISIONS.md`, ViGEm architecture, roadmap, validation matrix, worklog,
risk/test manifest and retained privacy-safe evidence. No user artifact or
hardware request is produced by F1.1.

## F1.1 implementation result

The same built `HallJoyV14Simulator.exe` now executes the early output-host
branch before every existing command and normal startup action. One persistent
output session owns the unnamed mapping, auto-reset wake, manual-reset child
stop and inherited owner-process handle. It composes the F1 suspended/job-
contained supervisor with R1.1 publication and never exposes a process/job
handle to realtime-style publication.

The child health plane retained the exact 640-byte ABI while naming
`childTelemetrySequence` and `childGeneration` in reserved capacity. A portable
100,000-update concurrency test accepted only complete even telemetry
transactions and verified that a replacement repairs an odd sequence left by
a force-killed predecessor. Wake failure is a typed publication error.

The exact simulator suite completed nine sequential generations: normal apply
and qualified stop; exit before ready; exit after ready; exit before snapshot
read; exit during fake update; exit after acknowledgement; exit during planned
neutral; progress-stalled force/reap; and a clean replacement. The replacement
discarded the stale generation and acknowledged the exact sequence/checkpoint
of the newest complete four-pad value. The incomplete-neutral child was reaped
but classified `IncompletePlannedStop`, not successful output shutdown.

All children were reaped and no output-host process survived. The full native
suite, MSVC Release x64 simulator and production builds passed with no new
warnings (only the existing allow-listed ViGEmClient `LNK4099`). The production
image rejects malformed host input with exit 60 and the simulator-only suite
with exit 91 before normal application startup. `backend.cpp` was not changed;
the legacy worker remains the sole ViGEm caller. Evidence:
`../stability/tests/V14_F1_1_VIGEM_OUTPUT_SELF_HOST_FAKE_TRANSPORT_2026-08-22.txt`.

F1.1 therefore closes only the exact-image composition gate. F2 may now replace
the fake child transport with real ViGEm client/target ownership behind the
same command, session, telemetry and clean-stop contract. Production routing
and removal of the legacy worker remain later atomic migration gates.
