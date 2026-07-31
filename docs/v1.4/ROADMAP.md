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
| `V14-06` | Cooperative lifecycle migration for realtime, logging and native protocol workers | In progress | Per-worker tests, no ordinary `TerminateThread`, plus unchanged protocol and mapping characterization |
| `V14-07` | Analog host and UAP ABI generation, exception, unload and restart safety | Planned | Partial-start, crash, hang, unload and bounded restart tests |
| `V14-08` | Startup transaction, wake correctness and ViGEm output isolation | Planned | Reverse-order rollback, no lost wake, stalled-driver and report-equivalence tests |
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

## Current package: V14-06

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
- `V14-06D` SparkLink: Verified locally. The hotplug worker has its own
  serialized generation inside the registry generation; stop signals its event,
  cancels HID I/O and releases thread/HID/event HANDLEs only after confirmed
  join. Join or lifecycle-lock timeout poisons restart and prevents dependent
  teardown. The outer generation represents the long-lived hotplug service, so
  a worker connected after initial device absence is still stopped at shutdown.
  Protocol discovery, claim policy, commands and polling are unchanged.
- `V14-06E` Sayo shutdown: Implemented and verified locally without hardware.
  All readers share one generation and one three-second group deadline. Stop
  signals the shared event, cancels every HID operation and releases reader,
  HID and event HANDLEs only after the whole group joins. Timeout retains the
  group, poisons restart and prevents dependent teardown. The old worst case of
  sequential per-reader waits followed by `TerminateThread` is removed. Stop
  publishes neutral analog input before cancellation and again after join.
- Remaining V14-06 work: `V14-06F` Sayo C++/SEH exception containment and
  early-reader-exit publication. There are no remaining ordinary
  `TerminateThread` calls, but V14-06 stays open until that P1 worker boundary
  and the final package-wide regression gate pass.

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
