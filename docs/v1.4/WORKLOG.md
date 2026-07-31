# HallJoy v1.4 worklog

## 2026-07-31 - V14-00 baseline and provenance

### Inputs

- GitHub baseline: `v1.3`, commit `2467cb7`.
- Local v1.3 SDK checkpoint: commit `b3fefce`.
- Imported archive:
  `C:\Users\Proizvodstvo\Downloads\HallJoy_STABILITY_S02V1_VERIFICATION_TRACE.zip`.
- Archive SHA-256:
  `39727D2F63165F63B2AC0AA8105DBF4937C02442D09B0F8FA800F195D502A4CF`.

### Completed

- Verified GitHub authentication for account `PashOK7`; no push performed.
- Built the v1.3 SDK checkpoint with MSVC x64 Release: 0 errors, 6 warnings.
- Preserved the checkpoint on `checkpoint/v1.3-self-contained-sdk`.
- Re-extracted the source archive into a clean import directory.
- Compared 344 source-package files by SHA-256: 0 mismatches.
- Imported the archive on `v1.4-integration` as commit `f5e8c18`.
- Established the authoritative v1.4 documentation set.
- Rebuilt the imported branch with MSVC x64 Release: 0 errors and 1 vendored
  ViGEm PDB warning.
- Added the existing Build Tools Clang 19.1.5 directory to the user `PATH`.
- Passed all supplied static and portable C++20 tests with Clang.
- Installed ViGEmBus 1.22.0 through its hash-verified winget package.
- Passed UI startup and graceful shutdown with ViGEm initialized; exit code was
  0 and no analog-host child remained.
- Received the expected trace `WARN` because no SparkLink hardware was
  exercised. There were no backend or ViGEm failures after driver installation.
- Ignored the generated private UAP `.build-tools` directory so a clean build
  does not contaminate the Git worktree.

### Baseline observations

- The imported architecture embeds a private UAP and does not use the system
  Wooting SDK at runtime.
- The private plugin is currently extracted beside the executable. This can
  fail in a protected directory.
- Legacy dependency code may then recommend installing the system SDK, which
  cannot repair the embedded private UAP path.
- The imported risk register contains 44 open and 1 partial risks.
- The build bootstraps Sun from a moving branch; Soup itself is commit-pinned.

### Next

- Apply `V14-01` version identity changes.
- Add the `V14-02` development-only deterministic analog simulator.
- Begin `V14-03` self-contained UAP runtime and dependency diagnostics.

## 2026-07-31 - V14-01 product identity

### Completed

- Added central version macros for `1.4.0.0`.
- Added Windows `VERSIONINFO` metadata and updated the About dialog.
- Changed the runtime build identifier to `HallJoy-v1.4`.
- Updated active README and build output text to v1.4.
- Added an automatic version identity audit that preserves historical evidence
  while rejecting `3.9.0` on active product surfaces.

### Validation

- Static audits: PASS.
- Portable C++20 tests with Clang 19.1.5: PASS.
- Full MSVC x64 Release build: PASS with the inherited ViGEm PDB warning.
- Built EXE reports `FileVersion=1.4.0.0` and `ProductVersion=1.4.0.0`.

### Next

- Implement the `V14-02` development-only analog simulator and scenario runner.

## 2026-07-31 - V14-02 deterministic analog simulator

### Completed

- Added a portable deterministic model for WASD analog values.
- Covered ramp, hold, release, W+S, A+D, diagonal, disconnect, reconnect,
  post-reconnect input, source fault, and recovery.
- Registered a simulator backend only under `HALLJOY_ANALOG_SIMULATOR`.
- Required exact runtime opt-in with
  `--halljoy-simulate-analog=script`.
- Added temporary process-local WASD-to-left-stick bindings for the scenario.
- Added explicit `SIMULATED / NOT HARDWARE` telemetry and trace labels.
- Added a repeatable PowerShell build/runtime/trace gate.
- Excluded simulator translation units from ordinary MSBuild targets.

### Validation

- All static and portable C++20 tests passed with Clang 19.1.5.
- Simulator x64 Release build passed with 0 errors and the inherited ViGEm PDB
  warning.
- Scenario runner confirmed common curve/SOCD/report behavior, successful ViGEm
  updates, disconnect/fault neutralization, exit code 0, and no remaining
  process.
- Ordinary production x64 Release build passed and its compile command omitted
  both simulator translation units.

### Limitations

- No analog keyboard was available on this workstation.
- This package does not verify HID transport, firmware, VID/PID routing, device
  timing, or any protocol-specific hardware behavior.

### Next

- Begin `V14-03`: private UAP extraction in protected locations and truthful
  embedded-runtime dependency diagnostics.

## 2026-07-31 - V14-03 self-contained private UAP runtime

### Completed

- Compared the v1.3 self-contained SDK checkpoint `b3fefce` with the isolated
  ABI1 architecture imported for v1.4.
- Retained portable extraction beside the executable when writable.
- Added a versioned `%LOCALAPPDATA%` fallback for protected installations.
- Made temporary extraction names process/thread-specific and required complete
  writes, flush, atomic replacement, and final byte comparison.
- Passed the verified absolute plugin path to the child process with tested
  Windows command-line quoting.
- Reclassified backend failures as private UAP conditions.
- Removed system Wooting SDK and global UAP download/install recovery.
- Kept ViGEmBus as the only external dependency HallJoy may offer to install.
- Added static and runtime fallback gates.

### Validation

- Forced per-user fallback initialized private UAP, backend, ViGEm, simulator,
  and shutdown successfully.
- A deliberately corrupted generated per-user DLL was replaced atomically.
- The repaired DLL SHA-256 exactly matched the embedded build artifact:
  `0C45419D8F615284B4D673CB369191E6ABFCD57A72D3564C744D5960682DD8B2`.
- No simulator or child process remained after shutdown.
- The ordinary portable path reported `location=executable` with exact resource
  equality and passed the complete simulator scenario.
- The clean official build passed all audits and production x64 Release with
  0 errors and the inherited `ViGEmClient.pdb` warning.
- The packaged production EXE exposed its main window, initialized the private
  runtime and ViGEm, accepted a graceful close, exited with code 0, and left no
  child process behind.

### Limitations

- No real analog keyboard was available; UAP device behavior remains pending.
- Hardware transport, firmware, VID/PID routing, and device timing remain for
  the `V14-12` hardware qualification gates.

### Next

- Begin `V14-04`: pin remaining build inputs and align local and CI gates.

## 2026-07-31 - V14-04 reproducible build inputs and gate parity

### Implemented

- Added one machine-readable lock for Sun, Soup, ViGEmClient, GitHub Actions,
  runner labels, and required toolchain families.
- Replaced the moving Sun branch bootstrap and arbitrary `PATH` tool selection
  with an exact detached commit checkout.
- Kept Soup on its existing audited exact commit and moved both repositories to
  lock-owned configuration.
- Added five reviewed HallJoy Soup overlay files with normalized per-file
  SHA-256 values and rejected every extra changed/untracked Soup file.
- Pinned all GitHub Actions by full SHA and replaced `ubuntu-latest` with
  `ubuntu-24.04`.
- Made the official Windows build run the same required portable C++20 tests as
  the portable CI entry point.
- Added deterministic Windows Clang discovery for stale or minimal `PATH`
  environments.
- Enforced a production warning allowlist containing only the inherited ViGEm
  `LNK4099` missing-PDB diagnostic.
- Added a static audit for dependency immutability and local/CI gate parity.
- Included the exact dependency lock in the packaged/CI artifact.

### Validation

- Lock audit and every existing static audit passed.
- All portable C++20 tests passed with Clang 19.1.5.
- Official build used Sun `83c195bd61314bdbfdccc161653dbb652e3b6678`
  and Soup `b02796b0b20276277c8a4b4d3759643eeab43ff7`.
- ViGEmClient size and SHA-256 preflight passed.
- Fresh shallow fetches by the locked Sun and Soup commit SHAs succeeded, and a
  newly patched Soup tree reproduced the locked diff SHA-256 exactly.
- Private UAP rebuild and MSVC x64 Release completed with 0 errors and no
  warning outside the documented allowlist.

### Limitations

- The first independent clone correctly exposed that the ignored local Soup
  cache contained more HallJoy changes than the old generator reproduced.
- The corrected clone at `2230dee` fetched both exact commits from scratch,
  verified the five-file overlay, completed the full build, and remained clean.
- Its packaged production EXE opened and shut down gracefully with exit code 0
  and no remaining parent or child process.
- GitHub Actions was not run because the account quota is unavailable; it is an
  optional post-publication check rather than a V14-04 blocker.

### Next

- Begin `V14-05`: truthful lifecycle registry and cooperative worker shutdown.

## 2026-07-31 - V14-05 truthful lifecycle registry

### Implemented

- Replaced `g_started` with a mutex-protected, fixed-capacity lifecycle state
  machine with monotonic generations and owner-thread enforcement.
- Changed native descriptor stop callbacks to return generation-scoped
  `StopResult` values.
- Made timeout, fault, and malformed generation results poison the entry and
  reject restart; `Reset()` no longer erases an incomplete stop.
- Exposed exact lifecycle snapshots and critical trace diagnostics.
- Made SparkLink and Sayo report forced termination as an incomplete stop.
- Added failure-injected tests for wrong-thread access, failed start, normal
  join/restart, timeout, and stale callback generations.

### Validation

- Static audits and portable C++20 tests: PASS with Clang 19.1.5.
- Full MSVC x64 Release build: PASS, 0 errors and only allowlisted `LNK4099`.
- Deterministic analog simulator runtime scenario: PASS, including graceful
  shutdown and no remaining process.
- Hardware behavior was not claimed by this registry package.

### Remaining risk

- `TerminateThread` remains open as `HJ-AUD-P1-001`. V14-06 begins bounded
  cooperative migration one worker at a time, starting with realtime.

## 2026-07-31 - V14-06A realtime cooperative shutdown

### Implemented

- Realtime lifecycle now uses the common monotonic `WorkerLifecycle` state
  machine and returns an exact generation-scoped `StopResult`.
- Stop clears the run flag and wakes `WaitOnAddress` before its bounded join.
- Confirmed completion closes the thread HANDLE and permits restart.
- Timeout or wait failure retains the HANDLE, marks the generation `Poisoned`,
  and blocks watchdog restart.
- Final shutdown skips backend destruction beneath a potentially live
  `Backend_Tick`; the process-level fallback skips CRT destruction and leaves
  resource reclamation to Windows.
- Added a reusable portable join-observation policy and fault-injection tests.
- Removed `TerminateThread` from realtime only; its processing loop, pacing,
  `Backend_Tick`, MMCSS, timer and multimedia-period ownership are unchanged.
- Corrected native registry trace severity discovered by the runtime gate:
  absent optional hardware is `WARN`, while rejected/poisoned lifecycle is
  still `ERROR`.

### Validation

- Realtime targeted static audit: PASS.
- Portable join policy tests for joined/timeout/wait-failure: PASS.
- All static and portable C++20 tests: PASS.
- Full production MSVC x64 Release build: PASS, only allowlisted `LNK4099`.
- Rebuilt deterministic analog simulator: PASS, graceful cooperative shutdown,
  no `ERROR` trace event and no remaining process.
- Simulator-only blocked-worker injection: PASS; timeout retained the HANDLE,
  skipped backend cleanup, blocked restart and exited with the expected code 2.

### Remaining V14-06 scope

- Diagnostic logger, overlay server, SparkLink and Sayo still contain their
  separately owned forced-termination fallbacks.

## 2026-07-31 - V14-06B diagnostic logger cooperative shutdown

### Implemented

- Diagnostic writer start/stop now uses the common generation-scoped
  `WorkerLifecycle`; lifecycle transitions are serialized independently of the
  queue lock.
- Shutdown closes the producer gate, wakes the writer, permits its bounded
  drain/flush and closes HANDLE, event and file only after confirmed join.
- Timeout or wait failure retains every resource reachable by the writer,
  marks the generation `Poisoned` and blocks replacement.
- Final shutdown exits at process level with code 3 after recording the
  containment event, avoiding CRT destruction under a potentially live writer.
- Removed `TerminateThread` from the logger only. Simulator builds enable the
  otherwise Release-disabled logger solely to exercise its lifecycle contract.

### Validation

- Diagnostic logger static audit: PASS.
- All static and portable C++20 tests: PASS.
- Full production MSVC x64 Release build: PASS, only allowlisted `LNK4099`.
- Simulator-only blocked-writer injection: PASS; resources retained, restart
  poisoned and expected process exit code 3 observed.
- Normal deterministic simulator after the injected run: PASS, no `ERROR`
  trace event and no remaining process.

### Remaining V14-06 scope

- Overlay server, SparkLink and Sayo still contain separately owned
  forced-termination fallbacks.

## 2026-07-31 - V14-06C overlay cooperative shutdown

### Implemented

- Overlay start/stop now uses the common generation-scoped `WorkerLifecycle`
  with serialized lifecycle transitions.
- Stop closes the listen socket to wake `accept` and shuts down the active
  client socket before the bounded join.
- Confirmed completion alone releases the worker HANDLE and WSA ownership.
- Timeout or wait failure retains reachable worker ownership, marks the
  generation `Poisoned` and blocks replacement.
- Final application shutdown stops immediately after overlay poison and uses
  process-level containment before dependent state can be destroyed.
- Removed `TerminateThread` from overlay without changing HTTP parsing,
  response generation, telemetry, settings or its single-client model.

### Validation

- Overlay cooperative-shutdown static audit: PASS.
- All static and portable C++20 tests: PASS.
- Simulator loopback `/state`: HTTP 200; graceful worker join and `stop.end`:
  PASS, no `ERROR` trace event.
- Simulator-only blocked-worker injection: PASS; HANDLE/WSA retained, restart
  poisoned, dependent cleanup skipped and expected process exit code 2 observed.

### Remaining V14-06 scope

- SparkLink and Sayo still contain separately owned forced-termination
  fallbacks.

## 2026-07-31 - V14-06D SparkLink cooperative shutdown

### Implemented

- SparkLink hotplug workers now use a serialized internal generation within the
  existing native-registry generation.
- The outer registry generation now represents the long-lived hotplug service,
  fixing the case where a device connected after an absent initial probe could
  otherwise escape final registry stop.
- Stop signals the manual-reset event and calls `CancelIoEx` before its bounded
  worker join.
- Confirmed completion alone releases thread, HID and event HANDLEs.
- Join timeout retains all reachable worker resources, poisons internal restart
  and propagates an incomplete result to the outer registry.
- Final stop uses bounded timed-lock acquisition so a synchronous hotplug HID
  probe cannot make application shutdown unbounded.
- Native backend poison now stops dependent application teardown and selects
  process-level containment.
- Spark discovery, reversible protocol proof, VID/PID claim, command bytes,
  polling and normalization were not changed.

### Validation

- SparkLink exception-boundary and cooperative-shutdown static audits: PASS.
- All static and portable C++20 tests: PASS.
- Full production MSVC x64 Release build: PASS, only allowlisted `LNK4099`.
- Simulator-only blocked-worker injection: PASS; thread/event retained, inner
  and registry restart poisoned, dependent cleanup skipped and expected process
  exit code 2 observed.
- Normal deterministic simulator after the injected run: PASS, no `ERROR`
  trace event and no remaining process.
- Existing real-device S02B.2 evidence predates this lifecycle-only diff;
  post-change SparkLink device regression remains release qualification.

### Remaining V14-06 scope

- Sayo retains the final forced-termination fallback.

## 2026-07-31 - V14-06E Sayo cooperative group shutdown

### Implemented

- Sayo reader groups now use a serialized internal generation within a
  long-lived outer hotplug-service generation.
- Stop signals the shared event and calls `CancelIoEx` for every reader before
  joining.
- Up to eight readers now share one three-second `WaitForMultipleObjects`
  deadline instead of sequential three-second waits.
- Confirmed group completion alone releases reader thread, HID and shared-event
  HANDLEs.
- Stop publishes a neutral analog snapshot before cancellation and repeats the
  neutralization after group join to cover a final in-flight reader update.
- Timeout retains the complete reachable group, poisons restart, propagates an
  incomplete result through the registry and blocks dependent teardown.
- Lifecycle-lock acquisition is bounded so synchronous device discovery cannot
  make final shutdown unbounded.
- Late-hotplug readers remain covered by final registry stop even when no Sayo
  device existed at initial startup.
- Removed the final production `TerminateThread`; Sayo discovery, depth probe,
  parser, mapping, polling interval and normalization were not changed.

### Validation

- Sayo cooperative group-shutdown static audit: PASS.
- All static and portable C++20 tests: PASS.
- Full production MSVC x64 Release build: PASS, only allowlisted `LNK4099`.
- Simulator-only blocked-reader injection: PASS; one shared deadline, reader
  group/event retained, inner and registry restart poisoned, dependent cleanup
  skipped and expected process exit code 2 observed.
- Normal deterministic simulator after the injected run: PASS, no `ERROR`
  trace event and no remaining process.
- Sayo hardware is unavailable; protocol-specific compatibility is not inferred
  from lifecycle simulation.

### Remaining V14-06 scope

- `V14-06F`: Sayo C++/SEH exception boundary, neutral fail-safe publication,
  early-reader-exit handling and a final package-wide regression gate.

## 2026-07-31 - V14-06F Sayo exception containment

### Implemented

- Split the Sayo reader algorithm from its Win32 entry and routed every reader
  through the common allocation-free C++ exception barrier.
- Preserved a separate outer SEH boundary for structured faults.
- Added fixed per-reader C++ fault records and SEH codes without introducing
  exception-driven control flow into the polling loop.
- C++ and SEH faults neutralize published Sayo input and signal the shared stop
  event so sibling readers leave the same group generation.
- Every normal or exceptional reader exit publishes completion; exit of the
  last expected reader clears connected state and wakes the neutral pipeline.
- Startup now rejects an already exited or faulted reader group and rechecks
  fault publication after connected-state publication.
- Added simulator-only C++ fault injection and a trace gate proving containment
  without requiring an analog keyboard.
- Sayo discovery, protocol proof, packet parsing, mapping, poll timing and
  normalization were not changed.

### Validation

- Sayo exception-boundary and cooperative-shutdown static audits: PASS.
- All static and portable C++20 tests: PASS.
- Full production MSVC x64 Release build: PASS, only allowlisted `LNK4099`.
- Simulator-only Sayo C++ exception: PASS; fixed fault publication, neutral
  input, group stop, completion, rejected startup and process exit code 0.
- Earlier blocked-reader timeout scenario repeated: PASS, expected exit code 2.
- Normal deterministic simulator repeated: PASS, no `ERROR` trace event and no
  remaining process.
- Sayo hardware is unavailable; device compatibility and reconnect are not
  inferred from simulation.

### Package result

- `V14-06` is Verified locally against its automated acceptance gate.
- Production has no ordinary `TerminateThread` call.
- Device-specific regressions and long-run soak remain `V14-12` release
  qualification; `V14-07` begins analog-host and UAP boundary hardening.

## 2026-07-31 - V14-07A analog-host parent generation

### Implemented

- Replaced the analog-host parent's lossy `started` flag with one explicit
  lifecycle generation and permanent restart poison after an incomplete join.
- Snapshot bridge and supervisor now form one ownership group. Their thread
  HANDLEs, shared mapping, events and child job are released only after a
  confirmed group join.
- Supervisor creation failure now requests stop and joins the already-created
  bridge before partial-start rollback. Failed rollback retains ownership and
  poisons restart instead of unmapping IPC beneath a live thread.
- Final shutdown first performs a bounded graceful group wait, then terminates
  the isolated child job and performs one bounded parent-worker retry.
- A surviving parent worker retains all reachable resources and propagates a
  failed result through backend and application shutdown. The process then
  avoids dependent static/CRT teardown.
- Added simulator-only bridge-timeout and supervisor-start-failure injections,
  plus a static audit for generation, cleanup ordering and failure propagation.
- Private UAP polling, ABI exports and keyboard protocol behavior were not
  changed in this subpackage.

### Validation

- Analog-host generation ownership static audit: PASS.
- All static and portable C++20 tests: PASS.
- Full production MSVC x64 Release build: PASS, only allowlisted `LNK4099`.
- Fresh simulator rebuild and normal common-pipeline scenario: PASS, no
  `ERROR` trace event and no remaining process.
- Injected supervisor-start failure: PASS with exit code 0; bridge joined and
  IPC/event/job ownership released only after completion.
- Injected bridge stop timeout: PASS with expected process exit code 2 after
  bounded graceful and containment waits; ownership retained, restart blocked,
  backend failure propagated and dependent teardown skipped.
- Evidence is lifecycle simulation, not analog keyboard hardware verification.

### Remaining V14-07 scope

- `V14-07B`: analog-host worker exception/crash publication and bounded child
  process restart/exit behavior.
- `V14-07C`: private UAP C ABI exception barriers, RAII locks, null/state
  validation and unload safety.
