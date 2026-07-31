# HallJoy v1.4 roadmap

## Release objective

v1.4 combines the mature user interface and self-contained dependency lessons
from v1.3 with the native protocol, isolation, scheduling, and stability work
from the imported advanced archive. The release must not require a system-wide
Wooting Analog SDK installation.

The word "final" is not used before every release gate in this roadmap passes.

## Package rules

- One package addresses one coherent risk area.
- Protocol bytes, mappings, timing, lifecycle, and UI changes are not mixed
  without characterization tests.
- Every package has a local rollback commit.
- Every package updates the authoritative v1.4 documentation.
- Static audits supplement behavioral tests; they do not replace them.
- No push, merge to `main`, tag, or GitHub Release occurs without explicit
  approval.

## Ordered packages

| Package | Scope | Status | Required completion gate |
|---|---|---|---|
| `V14-00` | Preserve v1.3 SDK work; import and verify the advanced archive | Verified | Provenance commits, clean tree, baseline build and test evidence |
| `V14-01` | Product identity, version resources, current README and documentation ownership | Verified | No active product surface reports 3.9.0; historical evidence remains intact |
| `V14-02` | Development-only deterministic analog simulator and scenario runner | Verified | Full common pipeline, ramps, SOCD, hotplug, disconnect and fault scenarios without production enablement |
| `V14-03` | Self-contained private UAP runtime and truthful dependency diagnostics | Verified | Works from writable and protected install locations; never recommends system SDK |
| `V14-04` | Reproducible dependencies, warning baseline, local/CI-equivalent build scripts | Verified | Pinned inputs, clean-room x64 Release, portable tests, documented warning policy |
| `V14-05` | Truthful lifecycle registry and generation-scoped stop contract | Verified | Failure-injected start/stop/restart tests; timeout/fault poisons the generation and blocks restart |
| `V14-06` | Cooperative lifecycle migration for realtime, logging and native protocol workers | Verified locally | Per-worker tests, no ordinary `TerminateThread`, plus unchanged protocol and mapping characterization |
| `V14-07` | Analog host and UAP ABI generation, exception, unload and restart safety | Verified | Partial-start, crash, hang, C ABI, null/state, unload and bounded restart gates passed |
| `V14-08` | Startup transaction, wake correctness and ViGEm output isolation | In progress | Reverse-order rollback, no lost wake, stalled-driver and report-equivalence tests |
| `V14-09` | Transactional persistence and writable state migration | Planned | Fault-injected atomic-save tests and safe `%LOCALAPPDATA%` migration |
| `V14-10` | IPC and overlay security/correctness | Planned | ACL, spoofing, framing, overflow, origin, concurrency and shutdown tests |
| `V14-11` | UAP pacing, identity, modularization and measured performance | Planned | CPU/USB/latency comparison with no unsupported sampling regression |
| `V14-12` | Release qualification and hardware matrix | Planned | Clean package, 8-24h soak, reconnect cycles and required device-owner gates |

## Completed package: V14-00

Completed:

- v1.3 self-contained SDK work preserved on
  `checkpoint/v1.3-self-contained-sdk` at `b3fefce`.
- Advanced archive imported byte-for-byte on `v1.4-integration` at `f5e8c18`.
- Original ZIP SHA-256 recorded in the worklog.
- Imported x64 Release build reproduced successfully before integration.
- establish the authoritative v1.4 documentation set;
- x64 Release build reproduced from `v1.4-integration`;
- all supplied static audits passed;
- all portable C++20 tests passed with Clang 19.1.5;
- ViGEmBus 1.22.0 installed from the verified winget/GitHub release;
- UI launch, ViGEm initialization, main shutdown, and child shutdown passed.

The trace verdict is `WARN`, not `FAIL`, because this workstation has no
SparkLink device. It proves generic startup/shutdown and ViGEm lifecycle only.
GCC evidence is inherited from the byte-identical archive baseline and will be
rerun in CI before publication.

## Completed package: V14-01

- Central version macros define `1.4.0.0` and runtime build ID `HallJoy-v1.4`.
- The Windows resource exposes `FileVersion` and `ProductVersion` `1.4.0.0`.
- The About dialog and current README identify v1.4.
- Active runtime and build surfaces no longer identify `3.9.0`.
- Historical imported evidence and provenance filenames remain intact.
- A version identity static audit prevents regression.
- Static audits, portable C++20 tests, and the full MSVC x64 Release build pass.

## Completed package: V14-02

- Added a deterministic pure C++ analog model with ramps, holds, releases,
  opposing axes, diagonal input, disconnect, reconnect, fault, and recovery.
- Added a compile-time and command-line gated native backend.
- The simulator uses common curve, SOCD, report construction, ViGEm scheduling,
  telemetry, and shutdown paths.
- The Windows scenario proves neutral SOCD output, non-neutral diagonal output,
  disconnect/fault neutralization, accepted ViGEm changes, and clean shutdown.
- Simulator sources are excluded from the production compile.
- Evidence is always labelled simulated and never closes hardware gates.

## Completed package: V14-03

- Portable installs retain exact-resource verification beside the EXE.
- Protected installs fall back without elevation to a versioned per-user
  runtime under `%LOCALAPPDATA%`.
- Writes are atomic, flushed, and verified byte-for-byte after replacement.
- A stale or corrupted per-user DLL is self-repaired from the embedded resource.
- The verified absolute DLL path is quoted and passed explicitly to the child.
- System Wooting SDK and global UAP download/install recovery was removed.
- ViGEmBus remains the only offered external runtime dependency.
- Static, portable, forced-fallback, corruption-repair, production build, and
  runtime shutdown gates pass.

## Completed package: V14-04

Implemented locally:

- one machine-readable lock owns Sun, Soup, ViGEm, Action commits, runner
  labels, and toolchain families;
- Sun and Soup bootstrap by immutable commit, ignore arbitrary tools from
  `PATH`, and apply a five-file hash-locked Soup overlay;
- local and CI portable gates both require a C++20 compiler;
- the official Windows build fails on every warning outside the documented
  `LNK4099` ViGEm PDB allowlist;
- workflow Actions use full commit SHAs and fixed runner labels;
- a static audit prevents mutable dependency and gate regressions.

The corrected independent clone completed fresh dependency bootstrap, portable
C++20 tests, the full Windows build, and a production runtime smoke without
using repository build caches. GitHub Actions remains an optional
post-publication check.

## Completed package: V14-05

- Replaced the lossy global `g_started` array with a fixed-capacity,
  generation-aware lifecycle registry protected by a mutex and owner-thread
  contract.
- Changed the internal native descriptor ABI to return a generation-scoped
  `StopResult`.
- A timed-out, faulted, or malformed stop result leaves the entry `Poisoned`;
  reset cannot erase that state and restart is rejected.
- Exact lifecycle diagnostics are available through a registry snapshot and
  critical stability trace.
- SparkLink and Sayo no longer report forced termination as a successful
  cooperative join.
- Failure-injected portable tests cover wrong-thread access, failed start,
  normal join/restart, timeout poisoning, and stale-generation poisoning.
- Portable/static gates, full MSVC x64 Release build, and the deterministic
  analog simulator runtime scenario pass.

`TerminateThread` removal and bounded cooperative shutdown remain open and
begin in V14-06; V14-05 makes those failures truthful instead of masking them.

## Completed package: V14-06

Migrate workers one ownership boundary at a time to bounded cooperative
shutdown, beginning with the realtime loop. Protocol bytes, mappings, and
polling behavior remain characterization-locked during each migration.

Progress:

- `V14-06A` realtime loop: Verified locally. Stop wakes the address wait,
  returns a generation-scoped result, and closes the thread HANDLE only after
  confirmed completion. Timeout/failure retains ownership, poisons restart,
  guards backend teardown, and uses process-level exit without CRT destruction
  if a potentially live realtime worker survives final shutdown.
- `V14-06B` diagnostic logger: Verified locally. Shutdown closes its producer
  gate, wakes and drains the writer, and releases HANDLE/file/event ownership
  only after a confirmed join. Timeout retains all reachable resources,
  poisons restart and uses process-level exit without CRT destruction.
- `V14-06C` overlay server: Verified locally. Stop wakes both `accept` and
  client `recv`, and releases the worker HANDLE and WSA ownership only after a
  confirmed join. Timeout retains reachable ownership, poisons restart and
  prevents dependent application teardown. Loopback `/state` and forced-timeout
  simulator scenarios pass.
- `V14-06C.1` overlay responsiveness hotfix: Verified locally. The periodic
  `/client_perf` response and all close-framed error responses now close their
  connection immediately, so the single HTTP worker cannot wait for the
  five-second receive timeout before returning to `/state`. A socket-level
  regression gate measured the next state response at 0.3 ms in simulation and
  0.4 ms in production. The canonical production artifact is `HallJoy.exe`.
- `V14-06D` SparkLink: Verified locally. The hotplug worker has its own
  serialized generation inside the registry generation; stop signals its event,
  cancels HID I/O and releases thread/HID/event HANDLEs only after confirmed
  join. Join or lifecycle-lock timeout poisons restart and prevents dependent
  teardown. The outer generation represents the long-lived hotplug service, so
  a worker connected after initial device absence is still stopped at shutdown.
  Protocol discovery, claim policy, commands and polling are unchanged.
- `V14-06D.1` SparkLink service-stop hotfix: Verified on the Irok MG75 Max.
  Final service stop closes a dedicated outer
  start/reconnect gate before joining the active poller. The production trace
  has three balanced worker generations, two unplug/reconnect cycles, analog
  input before and after reconnect, and no reconnect/device-open/connect after
  `service.stop.begin`.
- `V14-06E` Sayo shutdown: Implemented and verified locally without hardware.
  All readers share one generation and one three-second group deadline. Stop
  signals the shared event, cancels every HID operation and releases reader,
  HID and event HANDLEs only after the whole group joins. Timeout retains the
  group, poisons restart and prevents dependent teardown. The old worst case of
  sequential per-reader waits followed by `TerminateThread` is removed. Stop
  publishes neutral analog input before cancellation and again after join.
- `V14-06F` Sayo exception and completion boundary: Verified locally without
  hardware. Every reader enters through the common allocation-free C++ barrier
  inside a separate SEH wrapper. Faults retain a fixed per-reader diagnostic,
  neutralize published input, stop the complete reader group and publish
  completion. Startup rejects an already exited or faulted reader generation;
  loss of the final live reader clears connected state.
- The simulator C++-fault injection, the earlier blocked-reader timeout
  injection, the normal common-pipeline scenario, all static/portable tests and
  the official production MSVC build pass. Production contains no ordinary
  `TerminateThread` call.

Sayo protocol/device compatibility is not inferred from simulation. It remains
an explicit device-owner gate in `V14-12`, together with the post-change
SparkLink regression and long-run qualification.

## Completed package: V14-07

Harden the isolated analog host and private UAP boundary: generation ownership,
partial-start rollback, bounded restart/unload and exception-safe C ABI exports.
This package must preserve the self-contained runtime and exact ABI behavior
verified in `V14-03`/`V14-04`.

Progress:

- `V14-07A` analog-host parent generation: Verified locally. The bridge and
  supervisor are one lifecycle generation; a confirmed group join is required
  before their HANDLEs, IPC mapping/events or child job are released.
- Supervisor partial-start failure now requests stop and joins the already
  created bridge before rollback. A failed join retains all reachable resources,
  poisons the generation and rejects restart.
- Final shutdown has bounded graceful and child-job containment phases. If a
  parent worker still does not join, backend shutdown reports failure and the
  application selects process-level containment without dependent teardown.
- Simulator-only bridge-timeout and supervisor-start-failure scenarios pass,
  as do the normal simulator, all static/portable gates and the production MSVC
  build. These tests prove lifecycle containment, not keyboard compatibility.
- `V14-07B` analog-host worker and child exit safety: Verified locally. Both
  parent Win32 workers and the isolated child entry have C++ and SEH barriers
  with neutral fault publication. A child HANDLE is retained until confirmed
  process completion; job-assignment or reap failure blocks replacement.
  Parent-fault, child-fault/restart and child-reap-timeout simulator gates pass.

- `V14-07C` private UAP safety: Verified locally. Every throwing C export is
  contained by a common exception barrier, all Soup mutex ownership is RAII,
  initialization state and null arguments are truthful, and device workers are
  cancelled and bounded-joined without holding the global devices mutex.
- The child host uses the optional private bounded-unload export. If plugin
  worker completion cannot be confirmed, it does not unload the DLL or run CRT
  teardown beneath a live worker; the disposable child process terminates and
  the existing parent/job containment owns recovery.
- The official build now runs a real ABI1 load/init/null/unload state gate after
  compiling the private plugin. Static audits, portable exception/RAII tests,
  production MSVC build and a process-clean smoke test all pass.

The Irok MG75 Max proved native SparkLink identification, successful vendor
polling and analog-row changes. `V14-06D.1` now suppresses the shutdown-time
reconnect and passed its held-key unplug/reconnect and balanced-shutdown hardware
gate. `HJ-V14-P1-004` and V14-07 are Verified; V14-08 is the next package.

## In-progress package: V14-08

Split the package at the ownership boundary so startup/wake publication changes
do not obscure the later ViGEm worker transition.

Progress:

- `V14-08A` startup transaction and wake correctness: Verified locally. Backend
  dependents start as one transaction and publish readiness only after realtime,
  required native phases and Raw Input prerequisites succeed. Optional absent
  protocol families remain valid; a present backend failure or rejected
  lifecycle ownership aborts startup.
- Rollback follows reverse acquisition order. It stops at the first unconfirmed
  join, retains dependent ownership and selects process-level containment rather
  than tearing backend state down beneath a live worker.
- Realtime input notifications now use a process-lifetime monotonic sequence.
  Notifications before worker start, during restart, or between the final check
  and `WaitOnAddress` remain observable; the address wake is only a latency hint.
- Curve invalidation now release-publishes an ordered generation and every
  thread-local curve cache acquire-observes it before reading atomic settings.
- Portable concurrency tests, static audit, two startup fault injections, the
  normal simulator, the official production build and an Irok MG75 Max startup/
  shutdown smoke all pass.
- `V14-08B` remains Planned: move synchronous ViGEm submission out of the
  realtime worker and prove report equivalence plus bounded behavior under a
  stalled driver. `HJ-AUD-P1-010` remains Open until that gate passes.

V14-08 is not complete until V14-08B is Verified.

## Release definition

v1.4 can be tagged only when:

1. all P1 risks in the v1.4 register are `Verified` or explicitly excluded with
   a release-blocking decision;
2. all automated Windows and portable CI gates pass from a clean checkout;
3. the application starts without a system Wooting SDK;
4. dependency failures produce truthful, actionable diagnostics;
5. settings and profiles survive injected write failures;
6. the required device matrix and long-run qualification are complete;
7. the release archive, executable, sources, notices, and evidence have recorded
   SHA-256 values.
