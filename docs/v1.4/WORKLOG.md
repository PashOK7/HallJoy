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

## 2026-07-31 - V14-07B analog-host exception and child-exit containment

### Implemented

- Added C++ and SEH entry barriers for the snapshot bridge, supervisor and
  isolated child host. Faults publish an error snapshot and wake the owner.
- Made child job assignment mandatory. Assignment failure blocks restart and
  terminates the unowned child before another generation can be started.
- A timed-out child process HANDLE is retained until completion is confirmed;
  restart remains blocked, so overlapping child generations are forbidden.
- Added deterministic simulator injections for supervisor C++ failure, child
  C++ failure/restart and child reap timeout, plus matching static audit gates.
- Preserved the existing private UAP ABI and keyboard protocol behavior.

### Validation

- Native backend static/portable gate: PASS.
- Full production MSVC x64 Release build: PASS, 0 errors and only the
  allowlisted ViGEm `LNK4099` diagnostic.
- Normal simulator and all three new fault scenarios: PASS.
- Previous supervisor-start failure and bridge-stop timeout regressions: PASS.
- No simulator process remained after accepted scenarios.

### Irok MG75 Max hardware finding

- The connected keyboard identifies as `VID 1CA6`, `PID 0529`; the runtime
  opened the native SparkLink `FFB0` route and completed 77,061 successful
  route queries with no route failure in the observed worker generation.
- The trace did not observe changed analog rows, so analog-depth input remains
  unverified for this device/run.
- Shutdown stopped the first Spark worker, then the hotplug watchdog reconnected
  and started another generation. The trace therefore failed its balanced
  worker requirement.
- This pre-existing SparkLink lifecycle defect is `HJ-V14-P1-004`, assigned to
  hotfix `V14-06D.1` before V14-07C. It is not part of V14-07B.

### Package result

- `V14-07B` is Verified locally for analog-host fault/restart containment.
- Hardware status is `PARTIAL/FAIL`, not Verified.
- Next implementation package is `V14-06D.1`; `V14-07C` follows its regression
  fix and hardware gate.

## 2026-07-31 - V14-06C.1 overlay responsiveness and HallJoy.exe

### Root cause

- The user's production `overlay_perf.log` contained three isolated fetch
  averages of 5.001-5.002 seconds; ordinary fetches were about 1.6-2.1 ms.
- The overlay has one HTTP worker. Its periodic `/client_perf` request used a
  separate keep-alive connection, and after the 204 response the worker could
  wait for another request until `SO_RCVTIMEO = 5000` while `/state` stalled.
- A 404 response had the same framing mismatch: it advertised `Connection:
  close`, but the request loop did not honor the response-directed close.

### Implemented

- One-shot `/client_perf` and error responses now request immediate connection
  close; the client loop honors that response decision.
- High-rate `/state` polling keeps its existing persistent connection and hot
  path behavior.
- Added a socket-level runtime gate that sends telemetry with keep-alive,
  requires server EOF, then requires a valid `/state` response within 1 second.
- Renamed the official production target and packaged artifact to `HallJoy.exe`;
  build instructions and the trace collector use the same canonical name.

### Validation

- Full static and portable C++20 gate: PASS.
- Simulator responsiveness: PASS, 0.3 ms; graceful overlay shutdown: PASS.
- Forced overlay stop-timeout containment regression: PASS, expected exit 2.
- Production MSVC x64 Release: PASS, 0 errors and only allowlisted `LNK4099`.
- Production responsiveness: PASS, 0.4 ms; hidden-window `WM_CLOSE` shutdown:
  PASS, exit 0, no remaining `HallJoy` process.
- `HallJoy.exe`: 2,133,504 bytes,
  SHA-256 `93AF87C6D8079BD48E21A53AE78342625CE0AB7050BDEFE98EA34690AA08A058`.
- The production Irok trace recorded 515 changed Spark rows and 516 input
  notifications, upgrading analog input from unverified to proved.

### Package result

- `HJ-V14-P1-005` is Verified locally and `V14-06C.1` is complete.
- The user confirmed in the browser that the input overlay no longer exhibits
  the five-second freezes.
- General HTTP concurrency/security remains V14-10.
- SparkLink's shutdown reconnect race remains Open as `HJ-V14-P1-004`; the next
  implementation package remains `V14-06D.1`, followed by V14-07C.

## 2026-07-31 - V14-06D.1 SparkLink service-stop reconnect suppression

### Root cause

- Application shutdown stopped the native registry before the realtime loop.
  The still-running realtime tick could therefore enter `SparkTickHotplug`
  after the registry had joined the active Spark worker.
- SparkLink tracked the inner poller generation but had no explicit outer
  service-stop gate, so the hotplug path could open the Irok and publish a new
  generation during final shutdown.

### Implemented

- Added an outer SparkLink service-running/stop-requested gate, closed before
  the active poller is stopped.
- Both direct worker start and hotplug reconnect now require the outer service
  to be running; late connection publication rolls itself back if stop wins.
- The native registry descriptor now calls `SparkStopService`, which records
  service-level start/stop evidence and preserves the existing truthful inner
  join/poison result.
- Added simulator-only cooperative service worker and an explicit post-stop
  hotplug probe. The runner rejects any reconnect after `service.stop.begin`.
- HID discovery, commands, report parsing, polling modes and mappings are
  unchanged.

### Validation

- Full static and portable C++20 gate: PASS.
- New service-stop race simulator: PASS, exit 0 and no late reconnect.
- Existing Spark blocked-worker containment: PASS, expected exit 2.
- Normal deterministic simulator: PASS, exit 0 and no remaining process.
- Production MSVC x64 Release: PASS, 0 errors and only allowlisted `LNK4099`.
- Production Irok run: `VID 1CA6`, `PID 0529`, usage page `FFB0`; one Spark
  worker start and one exit, no reconnect/open/connect after service stop.
- Graceful hidden-window shutdown: exit 0 and no remaining HallJoy process.
- `HallJoy.exe`: 2,134,528 bytes,
  SHA-256 `330748ACBB0EDC0E35A4BA39807EC16F3DF2CB849940ED10128CDCF714BFEE25`.

### Package result

- The shutdown/reconnect defect is structurally fixed and verified against the
  real Irok transport.

### Hardware acceptance

- The user completed the held-key unplug/reconnect scenario on the exact
  packaged binary and confirmed correct recovery with no stuck input.
- The trace contains three Spark worker starts and three matching exits, two
  successful `hotplug.reconnect` events, and analog input in every generation:
  2,073 changed rows and 2,075 realtime notifications in total.
- Final shutdown closed the service gate before its last worker join and has no
  reconnect, device open or connection after `service.stop.begin`; exit code is
  0 and no HallJoy process remains.
- The analyzer now accepts both watchdog-stale and transport-disconnect paths as
  unplug evidence, provided reconnect occurs later in sequence. Its old
  `hotplug.stale`-only rule produced a false warning for Irok's transport-exit
  path; the static audit covers both forms.
- `HJ-V14-P1-004` and `V14-06D.1` are Verified. The next package is V14-07C.

## 2026-07-31 - V14-07C private UAP C ABI and unload safety

### Implementation

- Added a common catch-all C ABI invocation barrier and portable RAII mutex
  guard for the private UAP.
- Converted every manual Soup mutex lock/unlock pair to scope-bound ownership.
- Made initialization state truthful and validated null/zero-length ABI inputs.
- Wrapped UAP device/discovery workers and all throwing C exports so exceptions
  publish a transport fault, stop the generation and block unsafe restart.
- Reworked unload around one deadline: worker pointers are captured under the
  devices mutex, but HID cancellation and bounded joins happen after it is
  released. Device ownership is cleared only after confirmed completion.
- Added optional `halljoy_unload_bounded` consumption in the isolated child.
  An incomplete plugin unload ends the disposable child without `FreeLibrary`
  or dependent CRT cleanup; parent/job containment confirms its exit.
- Added the real ABI1 load/init/null/unload check to the official build.

### Validation

- Private UAP ABI/unload static audit and portable C ABI exception/RAII test:
  PASS.
- Full static and portable C++20 gate: PASS.
- Real newly built ABI1 runtime gate: PASS (`abi=1`, `initial_devices=0`; the
  Irok VID/PID is deliberately owned by the native SparkLink route).
- Production MSVC x64 Release: PASS, 0 errors and only allowlisted `LNK4099`.
- Four-second production smoke launched parent, diagnostic-watch and isolated
  analog-host child; termination produced balanced child/worker exits,
  `analog-host:stop.joined`, `backend:shutdown.end`, `main:session.end`, and
  left zero HallJoy processes.
- `HallJoy.exe`: 2,141,184 bytes,
  SHA-256 `15228FC17B70FB84AD2861FC04904E872B3571CCABBCEB26D6DFD3AE894D533B`.

### Package result

- `HJ-AUD-P1-015`, `HJ-AUD-P1-016`, `HJ-AUD-P2-008` and `HJ-AUD-P2-009` are
  Verified. UAP worker/C ABI coverage is complete; `HJ-AUD-P1-004` remains
  Partial only for deferred device-owner/soak gates.
- V14-07 is Verified. The next implementation package is V14-08.

## 2026-07-31 - V14-08A startup transaction, durable wake and curve publication

### Implementation

- Replaced best-effort dependent startup with one explicit transaction covering
  realtime, native `AfterRealtime`, Raw Input prerequisite and native
  `AfterRawInput`, with readiness published only at commit.
- Added structured native phase results so optional absent protocols do not
  fail startup, while present-device failures and rejected lifecycle ownership
  do.
- Added strict reverse-order rollback. Cleanup responsibility is acquired before
  each start call, including realtime, so partial creation is reaped. An
  unconfirmed stop poisons cleanup and prevents dependent backend teardown.
- Replaced restart-reset input counters with a process-lifetime monotonic wake
  sequence. Pending input is checked before every address wait and notifications
  between observe/consume remain pending for the next tick.
- Replaced relaxed curve cache invalidation with a release-published generation
  and acquire observation, backed by a portable payload-visibility test.
- Added simulator-only realtime-start and native-phase failure scenarios and
  made the normal SOCD gate axis-specific so unrelated live Irok input can
  coexist with the simulator without weakening the tested cancellation axis.

### Validation

- Startup/wake/publication static audit: PASS.
- Full static and portable C++20 gate: PASS, including the new wake race and
  release/acquire publication tests.
- Realtime-start rollback, native-phase reverse rollback and normal simulator:
  PASS, exit 0.
- Official production MSVC x64 Release: PASS, 0 errors and only allowlisted
  `LNK4099`; private ABI1 runtime gate also passed.
- Six-second production smoke launched UI, diagnostic-watch and analog-host,
  committed startup with the Irok MG75 Max on `1CA6:0529/FFB0`, shut down all
  components through `WM_CLOSE`, and left zero HallJoy processes.
- `HallJoy.exe`: 2,145,280 bytes,
  SHA-256 `E6A90BEE93EE28A25CDB7C3A03F13C773FE5B48204CDDE49A713BF6C040A8C43`.
- Pre-change source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-08A-prechange-20260731`.
- Pre-production runtime backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-08A-preproduction-20260731-2231`;
  user settings, bindings, layouts and profiles were restored hash-identically.

### Package result

- `HJ-AUD-P1-008`, `HJ-AUD-P1-009` and `HJ-AUD-P2-019` are Verified.
- V14-08 remains In progress. Next is V14-08B: isolate synchronous ViGEm
  submission and prove report equivalence plus bounded stalled-driver behavior.

## 2026-07-31 - V14-08B ViGEm output isolation

### Implementation

- Moved every runtime `vigem_target_x360_update`, reconnect and final ViGEm
  destruction behind a dedicated output-worker generation. Initial client and
  target creation remains an explicit startup-thread operation.
- Realtime now performs only a bounded, non-blocking latest-value mailbox
  publication and wake. Driver latency can no longer extend `Backend_Tick`.
- Added pending-batch merge semantics for up to four virtual pads: due-pad masks
  accumulate, while report payloads always come from the newest complete
  snapshot. A rate-limited newer state also refreshes an already-pending batch.
- Emergency realtime neutralization drains older pending generations without
  submitting them and makes neutral the final driver write.
- Added a three-second cooperative output stop. Timeout retains thread/event and
  ViGEm ownership, poisons restart, skips dependent backend cleanup and selects
  immediate process containment with exit code 2.
- Added simulator-only 60-second driver-update stall injection and changed the
  hidden simulator runner to post `WM_CLOSE` to windows belonging to the exact
  launched PID.
- Added the portable mailbox/report-equivalence test and the ViGEm ownership
  static audit to both the common native gate and official build.

### Validation

- Full static and portable C++20 gate: PASS.
- Normal MSVC simulator: PASS, output worker joined, exit 0; trace SHA-256
  `AEF9EB00E29E1963602EA1E1C0DFDD73750639C5C5FD3FB76D1C0B6A7C232DBA`.
- Injected update stall: PASS. Realtime completed all 634 simulator updates and
  stopped before the output worker hit its three-second bound; backend teardown
  stopped safely and process containment exited 2. Trace SHA-256
  `BD7C5ED4CBB3FF4CC3BE5DDC75F4F700C1057D46590893B4C6CDCC5E1B5F19DE`.
- Official production MSVC x64 Release: PASS, 0 errors and only allowlisted
  `LNK4099`; private ABI1 runtime gate also passed.
- Ten-second hidden production smoke connected the Irok MG75 Max through
  SparkLink `1CA6:0529/FFB0`, committed startup, cleanly joined realtime,
  ViGEm output and analog-host ownership, exited 0 and left zero processes.
- `HallJoy.exe`: 2,147,840 bytes,
  SHA-256 `73F425BFED6B090015842A987C79CBCA99E07CEA38FD6DB6C227084E4A719CA3`.
- Production trace SHA-256:
  `700E580F65862C2E4EC5FD8F8AFB406CC72306D3FE30B4B55F6C21438C87FE6C`.
- Pre-change source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-08B-prechange-20260731-2255`.
- Pre-production runtime backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-08B-preproduction-20260731-2313`;
  all 13 runtime-package files matched by SHA-256 and user state was restored
  exactly.

### Package result

- `HJ-AUD-P1-010` and V14-08 are Verified.
- Next is V14-09: transactional persistence and writable state migration.

## 2026-08-01 - V14-09A transactional settings and profile persistence

### Implementation

- Added one reusable prepare/write/flush/validate/replace transaction state
  machine and a Win32 adapter with unique same-directory temp files.
- Settings, profile settings, overlay metadata, active-profile metadata and
  bindings now check every write, physically flush, parse schema markers back
  and use one write-through atomic replacement as their commit boundary.
- Bindings streams explicitly check `flush`, `good`, `close` and final fail
  state. Overlay and active-profile updates copy and preserve unrelated base
  settings before making transactional changes.
- Failures publish data kind, stage, native error and path to the critical
  trace; production displays one actionable error dialog per process.
- Profile switching, creation and manual save no longer clear/switch state after
  a failed settings or bindings save.
- Added simulator-only failure injection for all five stages, a portable fake
  adapter test, a static audit and reusable production smoke runner.

### Validation

- Persistence static audit and the full static/portable C++20 gate: PASS.
- Normal simulator: PASS, exit 0.
- Prepare, write, flush, validation and replace injections: 5/5 PASS for real
  settings, bindings and overlay transactions. Known-good SHA-256 values stayed
  unchanged and no transaction temp file remained.
- Official x64 Release build: PASS, 0 errors and only allowlisted `LNK4099`;
  private UAP ABI1 runtime gate also passed.
- Eight-second production smoke connected Irok MG75 Max at `1CA6:0529/FFB0`,
  recorded 30,125 successful queries, 127 changed rows and zero route failures,
  then joined all workers and exited 0 with no process left.
- `HallJoy.exe`: 2,157,568 bytes,
  SHA-256 `989E1ADA3C0AAC9D973D2C28A5201AEAC130DCB7693029C6C797146CF53BB500`.
- Production trace SHA-256:
  `1A294BF1AA03E572F90E8B0532E26012593F153F7D428B1E4AB02E7F2DA3D151`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-09A-prechange-20260801-0920`.
- Pre-production runtime backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-09A-preproduction-20260801-0946`;
  all five mutable files were restored with zero hash mismatches.

### Package result

- `HJ-AUD-P2-011`, `HJ-AUD-P2-012` and `HJ-AUD-P2-013` are Verified.
- `HJ-AUD-P2-016` is Partial until layout/curve save callers are migrated.
- V14-09 remains In progress. Next is V14-09B transactional layout and curve
  preset/state persistence; `%LOCALAPPDATA%` migration follows in V14-09C.

## 2026-08-01 - V14-09B transactional layout and curve persistence

### Implementation

- Moved layout presets, curve presets and `_preset_state.ini` onto the common
  unique-temp, checked-write, physical-flush, readback and atomic-replace
  transaction contract.
- Added format kind/schema markers and exact value validation for every layout
  key entry and every curve field while retaining read compatibility with old
  schema-less files.
- Layout editor and active-layout saves stage a complete candidate and update
  the in-memory preset only after the file commit succeeds. Curve active-name
  changes roll memory back if their state transaction fails.
- Curve create cleans a newly-created file after a failed compound save. Active
  curve rename now cleans a failed new file and reports an incomplete old-file
  deletion instead of claiming success.
- Extended simulator persistence probes and the static audit to layout preset,
  curve preset and curve-state files.

### Validation

- Persistence static audit and full static/portable C++20 gate: PASS.
- Normal simulator: PASS, exit 0.
- Prepare, write, flush, validation and replace injections: 5/5 PASS across six
  known-good files. All hashes remained unchanged and no transaction temporary
  file remained.
- Official x64 Release build: PASS, 0 errors and only allowlisted `LNK4099`;
  private UAP ABI1 runtime gate also passed.
- Production smoke with the user's pre-transaction settings/layout/state files
  loaded the legacy formats, started overlay and the Irok SparkLink route at
  `1CA6:0529/FFB0`, joined all workers and exited 0 without persistence ERROR.
  The final run recorded 30,095 successful route queries and one transient
  query failure; no key presses were observed, so input proof remains the prior
  V14-09A hardware trace.
- `HallJoy.exe`: 2,163,712 bytes,
  SHA-256 `443DFBCE08E232A159C115BE391995D28CBD11186B45F9966C23766725B269FB`.
- Production trace SHA-256:
  `EB4AE295A9E1C5ADE8B3AC595C6A788E2E527E302244F6FB636968D6E52EECF4`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-09B-prechange-20260801-0956`.
- Runtime backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-09B-runtime-20260801-0956`; five mutable
  user files were restored with zero hash mismatches after production testing.

### Package result

- `HJ-AUD-P2-014`, `HJ-AUD-P2-015` and `HJ-AUD-P2-016` are Verified.
- V14-09 remains In progress. Next is V14-09C writable-state migration to
  `%LOCALAPPDATA%` plus profile-name/path hardening.

## 2026-08-01 - V14-09C writable-state migration and filename hardening

### Implementation

- Centralized production settings, bindings, global profiles, layouts and
  curve presets under `%LOCALAPPDATA%\HallJoy`.
- Added explicit `HallJoy.portable` selection guarded by ordinary-file,
  non-reparse and physical write/flush checks. Simulator storage is isolated by
  explicit root overrides.
- Added source-specific one-time migration from the EXE directory. It preserves
  legacy files, transactionally creates byte-identical backups, copies only
  missing targets, rejects reparse traversal and commits a validated marker
  only after every file succeeds.
- Build packaging now preserves legacy mutable state instead of deleting it
  before the first migration.
- Added one shared filename policy for global profiles, layouts and curves:
  Unicode NFC, invariant case collision keys, invalid/control cleanup, trailing
  dot/space cleanup, DOS-device avoidance, 80-code-unit stems, direct-child
  validation and bounded collision suffixes.
- Added a storage static audit and a Windows runtime migration/replay/portable
  test, including migration failure injection at all five transaction stages.

### Validation

- Storage and persistence static audits: PASS.
- Full static and portable C++20 backend gate: PASS.
- Current simulator rebuild: PASS, 0 compile errors, only allowlisted
  third-party `LNK4099`; normal common-pipeline scenario exited 0.
- Migration/replay/portable runtime gate: PASS. Prepare, write, flush,
  validation and replace faults each blocked initialization with exit 1,
  preserved the legacy hash, committed no target and left no temp file.
- Existing settings/bindings/overlay/layout/curve fault suite: 5/5 PASS with
  every known-good hash unchanged and zero temp files.
- Official x64 Release build: PASS, 0 errors and only allowlisted `LNK4099`;
  embedded ABI1 runtime gate passed.
- Real first production migration copied five legacy files, created five
  byte-identical backup files and one completed marker, preserved all five
  legacy SHA-256 values and left zero transaction temps.
- Second production launch logged `migration.skip reason=complete`, connected
  Irok MG75 Max at `1CA6:0529/FFB0`, completed 37,980/37,980 route queries with
  zero failures, joined SparkLink/realtime/ViGEm/host ownership and exited 0.
- `HallJoy.exe`: 2,216,960 bytes,
  SHA-256 `7E84054C944698CBCD2ABF76EAF70B1500DD0A007FEBC3CCC9E23BF2AF0944C6`.
- Replay production trace SHA-256:
  `F6CB87D888E45DFCDC013E917BE014C6330B638D3CD9B69D86A1E20A6546A4E0`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-09C-prechange-20260801-1021`.
- Legacy runtime backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-09C-runtime-20260801-1021`.

### Package result

- `HJ-AUD-P2-017`, `HJ-AUD-P2-018` and V14-09 are Verified.
- Next is V14-10 IPC and overlay security/correctness.

## 2026-08-01 - V14-10A Mouse IPC creation and atomic-read correctness

### Implementation

- Captured `GetLastError()` immediately after successful
  `CreateFileMappingW`, before `MapViewOfFile` can overwrite the mapping
  creation disposition.
- Split initialization policy so only a genuinely new mapping is cleared.
  Existing state is preserved and accepted only after magic, version and size
  validation; invalid schemas are rejected without modifying their payload.
- Reused the final v1 `reserved1` slot as `structSize`, retaining the public
  name, 40-byte ABI and every field offset. Legacy zero is accepted once and
  upgraded atomically; new mappings publish magic last.
- Replaced ordinary reads of ASI-owned attach/heartbeat state with interlocked
  reads and added creation/schema trace events.
- Added a simulator runtime policy self-test and a build-required static audit.

### Validation

- Mouse IPC static audit and full static/portable C++20 backend gate: PASS.
- Current simulator rebuild and normal common-pipeline scenario: PASS, exit 0;
  the policy self-test preserved sentinel payload, upgraded a legacy zero size
  and rejected an invalid mapping without overwrite.
- Storage migration/replay/portable and all five migration fault stages: PASS.
- Overlay lifecycle gate: PASS; next `/state` response measured 0.4 ms.
- Official x64 Release build: PASS, 0 errors and only allowlisted `LNK4099`;
  embedded ABI1 runtime gate passed.
- Ten-second production smoke selected the LocalAppData root, skipped the
  completed migration, created and validated the 40-byte Mouse IPC mapping,
  connected Irok MG75 Max at `1CA6:0529/FFB0`, completed 37,937/37,937 route
  queries with zero failures, joined all workers and exited 0.
- No analog-row changes were produced during this smoke; it is startup/route/
  shutdown evidence, not a new input proof. No external ASI binary attach was
  exercised; compatibility is covered by unchanged ABI and the simulated
  legacy-zero slot.
- Runtime user state matched its pre-smoke backup with zero differences.
- `HallJoy.exe`: 2,217,472 bytes,
  SHA-256 `7AB4EF791179AF4271F5307A5B695436599D047D4F9AC0530250A43A42B50E86`.
- Production trace SHA-256:
  `C6B0B5D8C0938F1D8D78B203D94E646F7C14523F6E32AD41C33B6F56A889B6DC`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-10A-prechange-20260801-1103`.
- Runtime backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-10A-runtime-20260801-1110`.

### Package result

- `HJ-AUD-P1-011` and `HJ-AUD-P2-005` are Verified.
- V14-10 remains In progress. Next is V14-10B analog-host IPC
  precreation/spoofing resistance; overlay protocol risks remain separate.

## 2026-08-01 - V14-10B analog-host inherited-handle IPC

### Implementation

- Removed all named mapping/event construction and child-side
  `OpenFileMappingW`/`OpenEventW` calls from the analog-host transport.
- Parent now creates an unnamed mapping, unnamed stop/snapshot events and a
  synchronizable/queryable handle to itself. `STARTUPINFOEXW` limits child
  inheritance to exactly these four handles with
  `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`.
- Replaced the timing-derived nonce with `BCryptGenRandom`. Shared schema v10
  carries owner PID and launch token; the child validates handle existence,
  schema, token and owner-handle PID before writing shared state.
- Parent tracks the exact PID returned by `CreateProcessW` and refuses Ready
  from any different published host PID.
- Added simulator-only invalid mapping-handle injection, a required runtime
  policy trace and an official-build static audit. Updated the prior UAP path
  and generation audits for the new launch context without weakening them.

### Validation

- Analog-host IPC audit and full static/portable C++20 backend gate: PASS.
- Fresh simulator x64 Release rebuild and normal common pipeline: PASS, exit 0.
- Invalid-handle scenario: first child rejected the non-inherited mapping
  handle with exit 31; supervisor performed one bounded restart, the second
  child completed normal processing, and all owners joined.
- Prior analog-host regressions: 5/5 PASS for supervisor partial-start failure,
  supervisor C++ fault, child C++ fault/restart, child reap timeout and bridge
  stop timeout containment.
- Official x64 production build: PASS, 0 errors and only allowlisted `LNK4099`;
  all static/portable gates and the embedded ABI1 runtime gate passed.
- Ten-second production smoke published `transport=inherited_handles`,
  `named_objects=0`, connected Irok MG75 Max at `1CA6:0529/FFB0`, completed
  37,953/37,953 route queries with zero failures, joined the child and both
  parent workers, and exited 0.
- No analog rows changed during the smoke, so it is route/lifecycle evidence
  rather than a new hardware input proof.
- All 11 LocalAppData files matched the runtime backup after smoke; zero
  transaction temp files and zero HallJoy processes remained.
- `HallJoy.exe`: 2,218,496 bytes,
  SHA-256 `8FD6609DFF589DF76515EF62C6B1365C83C47BFB0452AC5D5BCB20B8DDE78223`.
- Production trace SHA-256:
  `A098F363C4D52D7852D07BE0432B0A4B3AB6CD4F43ACE31AC8DD573C86B9A7EB`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-10B-prechange-20260801-1128`.
- Runtime backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-10B-runtime-20260801-1138`.

### Package result

- `HJ-AUD-P1-012` is Verified.
- V14-10 remains In progress. Next is V14-10C overlay HTTP framing and
  bounded parser/overflow correctness.

## 2026-08-01 - V14-10C overlay HTTP framing and bounded parsing

### Implementation

- Replaced one-`recv`/one-request handling with a per-connection accumulator
  and incremental HTTP/1.0/1.1 parser. Fragmented headers and bodies wait for
  completion; exact frame bytes are removed while pipelined bytes remain.
- Added strict single `Content-Length` handling, rejected transfer coding and
  malformed headers, and bounded headers to 8 KiB, bodies to 4 KiB and request
  targets to 2 KiB. Rejections send explicit closing
  `400`/`405`/`413`/`414`/`431` responses.
- Replaced wrapping telemetry decimal accumulation and substring key lookup
  with exact-key `from_chars` conversion. Duplicate, missing-required, junk,
  overflowing or above-one-billion metrics are rejected before any counter is
  updated.
- Added build-required static and TCP regression gates. Both simulator and
  production smoke runners exercise fragmentation, pipelining, exact body
  consumption, strict framing rejections, telemetry bounds and final `/state`
  responsiveness.
- The production runner now supports an explicit `-StartOverlay` mode. Its
  process-clean check uses a bounded two-second reap observation to avoid a
  transient Windows process-table false positive after the already confirmed
  analog-host child exit.

### Validation

- Framing audit, all 33 static audits and all 18 portable C++20 tests: PASS.
- Fresh simulator overlay scenario: PASS; telemetry-to-state latency 0.3 ms,
  fragmented request, two pipelined requests, exact four-byte body consumption
  and eight bounded rejection cases all passed.
- Existing forced overlay stop-timeout containment regression: PASS.
- Official x64 production build: PASS, 0 errors and only allowlisted
  `LNK4099`; embedded ABI1 runtime gate passed.
- Production overlay smoke repeated the socket suite, joined the overlay,
  SparkLink, realtime, ViGEm output and analog-host owners, and exited 0.
- Irok MG75 Max route `1CA6:0529/FFB0` completed 28,607/28,607 queries with
  zero failures. No analog rows changed, so this is route/lifecycle evidence,
  not a new hardware input proof.
- All 11 LocalAppData files and preserved package settings, bindings, layouts
  and curve presets matched the pre-production backup; zero HallJoy processes
  and zero persistence temp files remained.
- `HallJoy.exe`: 2,223,104 bytes,
  SHA-256 `554693964189E4B2B4C256D992F271A1BD81082DC40CF057EC4F45E023C5C817`.
- Production trace SHA-256:
  `A38C0ED1F9C64A304D71BF291E3D1417021706929BF7A44BEF6AC0AA8EC48AF3`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-10C-prechange-20260801-1146`.
- Runtime/package backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-10C-preproduction-20260801-1157`.

### Package result

- `HJ-AUD-P2-002` and `HJ-AUD-P2-003` are Verified.
- V14-10 remains In progress. Next is V14-10D overlay concurrency, origin
  policy and shutdown gates (`HJ-AUD-P2-001`, `HJ-AUD-P2-004`).

## 2026-08-01 - V14-10D overlay concurrency, origin and shutdown

### Implementation

- Replaced synchronous client handling in the accept owner with a fixed table
  of 16 independently owned socket/thread slots. Completed workers are reaped
  before assignment; saturation is rejected immediately without reading
  attacker-controlled input on the accept thread.
- Added a common C++ exception boundary to every client worker. Normal stop
  shuts down all active client sockets, and the accept owner joins every client
  before its own completion. An incomplete outer join retains the accept
  HANDLE, WSA and reachable client ownership and poisons restart.
- Added a new 128-bit `BCryptGenRandom` session token for every server
  generation. Root navigation issues an `HttpOnly; SameSite=Strict` cookie;
  state and telemetry require the exact cookie. The embedded page detects a
  stale-generation `401`, refreshes the root to receive the new cookie and
  resumes polling without a manual browser reload.
- Removed wildcard CORS. Browser requests accept only the exact
  `http://127.0.0.1:<port>` origin and echo it with `Vary: Origin`; hostile and
  `null` origins receive closing `403` responses without a cookie.
- Updated responsiveness/framing tools to bootstrap the session and added a
  build-required concurrency/origin socket gate. The simulator runner also
  keeps eight partial clients alive with header heartbeats until application
  shutdown, preventing idle timeout from masquerading as shutdown coverage.

### Validation

- New concurrency/origin static audit, existing cooperative-shutdown audit,
  all 34 static audits and all 18 portable C++20 tests: PASS.
- Simulator overlay socket suite: PASS. With eight partial slow clients, eight
  parallel authenticated `/state` requests completed with 2.3 ms maximum
  latency. The 17th client was rejected promptly at the fixed 16-client limit,
  and authenticated state recovered immediately.
- Exact origin echo, no-origin direct client, missing session, hostile origin,
  `null` origin and hostile root bootstrap cases all passed. The cookie is 128
  bits and carries `HttpOnly` plus `SameSite=Strict`.
- Active-client shutdown: PASS. Eight heartbeat-held incomplete requests were
  closed by stop, and the simulator trace recorded `active_clients=8`.
- Existing forced overlay stop-timeout containment regression: PASS.
- Official x64 production build: PASS, 0 errors and only allowlisted
  `LNK4099`; embedded ABI1 runtime gate passed.
- Production overlay smoke repeated framing, concurrency and origin gates;
  next-state responsiveness was 0.4 ms and maximum parallel latency was
  1.6 ms. Overlay and process shutdown completed normally.
- Irok MG75 Max route `1CA6:0529/FFB0` completed 45,867/45,867 queries with
  zero failures, a 249 us average route interval and balanced shutdown. No
  analog rows changed, so this is route/lifecycle evidence only.
- All 11 LocalAppData files matched the pre-production backup; zero HallJoy
  processes and zero persistence transaction temp files remained.
- `HallJoy.exe`: 2,231,296 bytes,
  SHA-256 `BF44786B93C34C2E310949C69EBDF641753A8729339736F7D1F2278A6A1D9BE2`.
- Production trace SHA-256:
  `7ED922357B9E1F4EF8FBC95C86614F361CF6712C1B3D76BC49B87DA3C3694294`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-10D-prechange-20260801-1217`.
- Runtime/package backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-10D-preproduction-20260801-1235`.

### Package result

- `HJ-AUD-P2-001` and `HJ-AUD-P2-004` are Verified.
- V14-10 is complete. Next is V14-11 UAP pacing, device identity,
  modularization and measured performance.

## 2026-08-01 - V14-11A UAP poll deadline pacing

### Implementation

- Replaced all six private UAP targets' zero-delay poll configuration with a
  1000 us start-to-start deadline. The worker records the cycle start before a
  poll and sleeps only for the remaining interval, so a slow USB transaction
  receives no additive delay.
- Added a pure, constexpr-testable pacing policy with bounded exponential
  Madlions failure waits of 2, 4, 8, 16, 32 and 64 ms. A successful report
  resets the streak. The 64 ms cap keeps stop observation well inside the
  existing three-second unload boundary.
- Applied the policy only to `AnalogueKeyboard::isPoll()` devices. Report-
  stream Wooting, Razer and NuPhy workers retain their existing blocking path.
- Added matching plugin/backend telemetry flags and UI text for deadline-paced
  production workers. An explicitly disabled target is labeled unthrottled
  diagnostic rather than being confused with the production policy.
- Updated the private plugin identity to `SafeHID v10 deadline-paced telemetry`
  and extended the runtime ABI gate to verify that exact rebuilt identity.
- Added a build-required static audit and portable pacing/rate model; the
  official runner now contains 35 static audits and 19 portable C++ tests.

### Validation

- Pacing static audit and complete static/portable gate: PASS. The deterministic
  50 us transaction model produced 1,000 paced versus 20,000 unthrottled calls
  per second and 50,000 versus 1,000,000 us of modeled busy time.
- Official x64 production build: PASS, 0 errors and only allowlisted `LNK4099`.
- Rebuilt private ABI1 load/init/name/null/bounded-unload: PASS, zero UAP
  devices on this workstation, exact v10 deadline-paced identity loaded.
- Production overlay smoke: responsiveness, framing, concurrency, origin and
  graceful shutdown PASS. Maximum parallel state latency was 1.6 ms.
- Native Irok MG75 Max route `1CA6:0529/FFB0` completed 65,138/65,138 queries,
  zero failures, 252 us average route interval and balanced exit 0. No keys or
  reconnect were exercised, so the analyzer WARN is not a product error.
- All 11 LocalAppData files were unchanged; no HallJoy process or transaction
  temp remained.
- `HallJoy.exe`: 2,232,832 bytes,
  SHA-256 `B7959FB6807CE0B6966380E0D3F9F1ECBEE170693CBF8E453EA31CF7914992A2`.
- Production trace SHA-256:
  `7918E18FF9FCDD7C9CF619DDFA7224FE248CE0E404F04E106593BDE7B1C591AE`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-11A-prechange-20260801-1305`.

### Package result

- `HJ-AUD-P2-006` is `Implemented`, not `Verified`: the pacing contract and
  isolation regression pass, but the available Irok uses native SparkLink and
  cannot supply real UAP poll-device CPU/USB/update-rate/latency evidence.
- V14-11 remains In progress. Next is V14-11B stable identity for identical
  UAP devices, followed by V14-11C snapshot contention.

## 2026-08-01 - V14-11B deterministic UAP device identity

### Implementation

- Replaced VID/PID/usage/name plus enumeration occurrence IDs with a versioned
  pure identity function shared verbatim by production and portable tests.
- A valid Soup HID interface path is ASCII-case/slash normalized, length-framed
  and hashed with VID, PID, usage page and usage. Occurrence cannot affect this
  path-based ID, so two identical devices cannot exchange IDs merely because
  enumeration order or the connected subset changes.
- Kept a metadata/occurrence fallback only for a missing path. Telemetry and
  dense snapshots now publish `DuplicateSafeId` conditionally instead of
  claiming every fallback is safe. Configuration UI shows the 64-bit ID and
  whether it is path-stable or an enumeration fallback.
- Added v2 golden vectors to prevent accidental persisted-ID drift. Plugin
  identity is now `SafeHID v11 stable-identity deadline-paced telemetry`.
- Expanded pacing coverage to 10,232 deadline/work-duration properties and the
  saturated `uint64_t` deadline edge.

### Validation

- Exact production identity function: all 40,320 orders of eight identical
  devices, 100,000 reconnect/subset/shuffle generations, 250,000 unique paths
  with zero observed collisions, 1,024 fallback occurrences, normalization,
  field framing and four golden vectors PASS.
- GCC 15.2 warning-clean, MSVC 19.44 `/W4`, and Clang 21 ASan+UBSan: PASS.
- Complete gate: 36/36 static audits and 20/20 portable C++ tests PASS.
- Official x64 production build: PASS, 0 errors, only allowlisted `LNK4099`.
- ABI1 loaded exact SafeHID v11, passed init/null/state/bounded unload with zero
  UAP devices present.
- Production overlay suite PASS; maximum parallel state latency 1.9 ms.
- Irok MG75 Max native regression: 65,610/65,610 queries, zero failures, 4,224
  changed rows, 4,225 realtime notifications, 313 us average route interval,
  balanced exit 0.
- All 11 LocalAppData files were unchanged; no process or temp file remained.
- `HallJoy.exe`: 2,233,856 bytes,
  SHA-256 `C7D28AA23D882A1ED57FA2562DF7F6C8375DE6E3B02D09C33EE08B184123B116`.
- Production trace SHA-256:
  `A6C33C41A99D2FB7BFF540D439956E0F295290D458E9BB690B3D6D0624016111`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-11B-prechange-20260801-1317`.

### Package result

- By D-022, `HJ-AUD-P2-006` and `HJ-AUD-P2-007` are `Verified` through exact
  production-code properties, three toolchains, sanitizers, ABI/build and real
  regression evidence. This makes no physical UAP USB/latency claim.
- A serial-less device moved to another port follows that port's HID path; no
  software-only algorithm can infer that two indistinguishable devices were
  physically swapped. Finite tests also cannot mathematically disprove every
  64-bit collision.
- V14-11 remains In progress. Next is V14-11C snapshot export contention.

## 2026-08-01 - V14-11C pinned UAP snapshot ownership

### Implementation

- Replaced the private UAP device registry's exclusive pointers with
  `shared_ptr<Device>` owners. Device construction, stopped-device callbacks
  and bounded unload now retain explicit lifetime pins whenever they operate
  outside `devices_mtx`.
- Added one fixed-capacity production helper that copies at most eight owner
  pins under the registry mutex. Dense snapshot and telemetry exports release
  that mutex before waiting for a device snapshot lock, collecting telemetry
  or copying all 256 key values.
- Updated the private plugin identity to `SafeHID v12 pinned-snapshot
  stable-identity deadline-paced telemetry` and made the ABI runtime gate
  require that exact generation.
- Added a build-required static audit and portable concurrency/lifetime test.
  Historical unload and identity audits were advanced to recognize and enforce
  the stronger ref-counted ownership contract.

### Validation

- Deterministic blocked-reader case: a sole registry entry was erased while its
  pinned reader waited on `snapshot_mtx`; registry removal completed, the object
  remained alive, and destruction occurred exactly once after reader release.
- 100,000 pin/erase lifetime cycles and at least 50,000 concurrent coherent
  256-value reads PASS.
- GCC 15.2 warning-clean, MSVC 19.44 `/W4 /WX`, and Clang 21.1.8
  ASan+UBSan: PASS with zero sanitizer reports.
- Complete gate: 37/37 static audits and 21/21 portable C++ tests PASS.
- Official x64 production build: PASS, 0 errors, only allowlisted `LNK4099`.
- ABI1 loaded exact SafeHID v12 and passed init/null/state/bounded unload with
  zero UAP devices present.
- Production overlay suite PASS; maximum parallel state latency 2.1 ms.
- Irok MG75 Max native regression: 45,872/45,872 queries, zero failures, 1,552
  changed rows and input notifications, 267 us average and 979 us maximum route
  interval, balanced exit 0.
- All 11 LocalAppData files were unchanged; no process or transaction temp
  remained.
- `HallJoy.exe`: 2,235,904 bytes,
  SHA-256 `2C7D5F923D6C989C2B6354EF4114B3AB8F124D1FFA42AC50B10C87FB3DD552A6`.
- ABI0/ABI1 DLL hashes:
  `4870CBD4A2F49C7E16D29765CF480956555AD6C92126D8DF15D31F385E3A4047` /
  `8CC08C5268F0EE7CB1B3DD78A48FA99E89E5C31D49B40B886B393BE51D7B4FA1`.
- Production trace SHA-256:
  `1BB4ED75AF624307C2FC36B078137EE802AA731FD8EDA25725EF756963840870`.
- Source backup (18 files after adding the two historical audit guards):
  `C:\github\HallJoy_v1.4_BACKUPS\V14-11C-prechange-20260801-1408`.

### Package result

- By D-022 and D-023, `HJ-AUD-P2-010` is `Verified`: exact production lock
  scope, owner lifetime, coherent snapshot and complete integration gates pass.
  This does not claim a physical UAP device latency or USB throughput result.
- V14-11 remains In progress. Next is V14-11D bounding the remaining UAP
  modularization/performance scope.

## 2026-08-01 - V14-11D exact HID interface ownership

### Implementation

- Replaced coarse native VID/PID claims with first-proof-wins ownership of a
  normalized full HID interface-path fingerprint. The shared header owns wide/
  UTF-8 normalization, UTF-16 hashing, token formatting and exact delimited
  membership; native routing publishes `HALLJOY_UAP_NATIVE_HID_PATHS`.
- Added a generic exact-interface claim registry. VID/PID is retained only as
  diagnostic metadata. Same-product sibling interfaces can be claimed by
  different protocols or remain available to UAP.
- Updated MAD68, Hex80, Addressed, SparkLink and Sayo to reject foreign claims
  after SetupAPI returns the path and before any `CreateFileW`, and to claim the
  exact path actually proved. SparkLink/Sayo reconnect is pinned to previously
  claimed paths; Sayo retains and claims every opened reader path.
- Replaced Soup's local VID/PID substring parser with one plugin-owned pre-open
  hook. Both the generated patch and locked overlay call it before `CreateFileW`;
  the post-open guard independently hashes `kbd.hid.path` through the same code.
- Added a build-required static audit and portable test; updated historical
  routing audits, ABI identity, dependency lock, build preflights and protocol
  authoring documentation.

### Validation

- Same-VID/PID sibling golden vectors, case/slash and UTF-8/wide normalization,
  exact prefix/suffix rejection, first-claim-wins and reset behavior PASS.
- 10,000 shuffled reconnect generations with 32 claims each PASS; 300,000
  synthetic interface tokens produced zero observed collisions.
- GCC 15.2 warning-clean, MSVC 19.44 `/W4 /WX`, and Clang 21 ASan+UBSan PASS.
- Complete gate: 38/38 static audits and 22/22 portable C++ tests PASS.
- Official x64 production build: PASS, 0 errors, only allowlisted `LNK4099`.
- ABI1 loaded exact SafeHID v13 interface-path generation and passed init/null/
  state/bounded unload with zero UAP devices present.
- Production overlay suite PASS; maximum parallel state latency 2.1 ms.
- Irok MG75 Max native regression: 45,873/45,874 queries, one contained
  transient miss, 67 changed rows/notifications, 286 us average and 1,754 us
  maximum route interval; startup, worker/service stop, analog host and exit 0
  balanced with zero trace ERROR events.
- `HallJoy.exe`: 2,215,424 bytes,
  SHA-256 `CF1C3B93381744005B7B2D32FB54FF17A1F8D8244C2F12610070D89D77DE7EE3`.
- ABI0/ABI1 DLL hashes:
  `28F5E14AE3CCD30A74A3F73D3BDDE6757CC7CC2BB0F5B9E80AF500A353314B58` /
  `F6EBC8A3A65F152AFF918BDC0DBFE1B811F2AEB8B4D9C985FCD618B05A254CD5`.
- Production trace SHA-256:
  `18C2ABA260D85CD44CFC0DAE930BB4BA623D989B74FFB472B4EA232703E76242`.
- Source backup: `C:\github\HallJoy_v1.4_BACKUPS\V14-11D-prechange-20260801-1406`,
  37 files and 37 unique SHA-256 hashes verified at copy time.

### Package result

- By D-022 and D-024, `HJ-AUD-P2-021` is `Verified`: exact-path ownership,
  pre-open exclusion and the complete available integration matrix pass. No
  physical UAP/multi-UAP hardware behavior or mathematical no-collision claim
  is inferred.
- V14-11 is complete. Next is V14-12 release qualification and hardware matrix.

## 2026-08-01 - V14-12A Aula WIN 60 HE MAX firmware-proven support

### Evidence review and implementation

- Audited the supplied 44-entry archive: zero unsafe paths, 37/37 manifest
  payload hashes, zero unlisted payloads and a complete source Git bundle.
- Reproduced the firmware verifier (57/57) and official oracle independently.
  Ten oracle sources matched fixed npm packages byte-for-byte; reproduced oracle
  JSON SHA-256 is
  `85C70BFAABE599F65A7EECBB5E6566D1B95679DB41520AAAEA8AF0566E7EFDC4`.
- Added pure protocol/parser, exclusive transaction client, device-selection
  policy and a production native backend. Exact fingerprint is `1CA2:1902`,
  `FFA0:0001`, 65-byte input/output and firmware `App V1.1.6 / Feb 4 2026`.
- Full proof is 17 read-only transactions. Every transaction flushes the input
  queue; any uncertainty poisons the session. Two complete Fn0 generations must
  match, both travel halves are sequential, and only 16-bit functions mapping
  safely to keyboard usages are published.
- The backend rejects foreign exact claims before opening HID, re-correlates
  identity/caps after exclusive open and claims the exact path only after proof.
  Ambiguous multiple candidates fail closed; no VID/PID reservation exists.

### Validation

- Aula static, protocol, official-oracle, end-to-end and session-policy suites:
  PASS. Complete gate: 39/39 audits and 26/26 portable tests PASS.
- GCC 15.2 warning-clean, all eight new units under MSVC 19.44 `/W4 /WX`, and
  four suites under Clang 21.1.8 ASan+UBSan: PASS, zero reports.
- Official `BUILD.cmd`: exit 0, 0 errors, only allowlisted `LNK4099`; private
  ABI1 and production overlay framing/concurrency/origin gates PASS.
- Irok MG75 Max regression: 65,379/65,379 SparkLink route queries, zero
  failures, 250 us average / 1,003 us maximum interval, balanced shutdown and
  zero trace ERROR. All 16 user-state files remained hash-identical.
- `HallJoy.exe`: 2,268,672 bytes,
  SHA-256 `C3F1F954619059C900A2F47DF861C2A7B0D02C1E7F7646D800101CFF5183F833`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-12A-Aula-prechange-20260801-1454`,
  30 files copied and hash-verified.

### Package result

- V14-12A is `Implemented`: firmware and production code are strongly proven,
  and the available Irok route shows no regression. Physical Aula input,
  unplug/reconnect, multiple devices and other firmware versions remain open;
  no hardware-tested claim is made.

## 2026-08-01 - V14-12B / S06 Addressed overlapped-I/O ownership

### Implementation

- Removed the cross-thread `ForceCloseReaderHandle` fallback. The Addressed
  reader now exclusively closes its HID handle after its stack `OVERLAPPED`,
  event and caller-owned buffer reach terminal completion.
- Cancellation remains cross-thread and cooperative; a completion racing stop
  is discarded instead of republishing non-neutral input.
- Moved the outer Addressed worker to a waitable `_beginthreadex` handle. Stop
  has one three-second deadline and returns a truthful generation-scoped result.
  Timeout retains the thread, signal events, active reader ownership and claim,
  so the native registry poisons restart and application teardown uses process
  containment.
- Added a simulator-only stop-timeout path and runner assertions for exit code 2,
  retained resources, registry poison and skipped dependent cleanup.

### Validation

- Complete gate: 39/39 static audits and 26/26 portable C++ tests PASS.
- Addressed production translation unit: MSVC 19.44 `/W4 /WX` PASS.
- Simulator rebuild and Addressed timeout containment scenario: PASS.
- Packet constructors and the complete session polling core are token-identical
  to the pre-change backup.
- Official `BUILD.cmd`: exit 0, 0 errors, only allowlisted `LNK4099`.
- 15-second Irok MG75 Max production regression: 57,161 successful route
  requests, one shutdown-window cancellation, 291 us average / 434 us maximum
  route interval, balanced shutdown and zero trace ERROR events. All 11 user
  state files remained hash-identical.
- `HallJoy.exe`: 2,270,208 bytes,
  SHA-256 `3D05EE5FA435343E633B991DC45B949C1484AD06CC086F993C6D788255510F7E`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-12B-S06-prechange-20260801`.

### Package result

- `HJ-AUD-P1-005` is implemented in code; physical Addressed qualification is
  still required before `Verified`.
- `HJ-AUD-P1-002` becomes `Partial`: Addressed is bounded and contained, while
  MAD68 and Hex80 remain assigned to S07.

## 2026-08-01 - V14-12C / S07 Hex80 bounded lifecycle

### Implementation

- Replaced Hex80's `std::thread` owner with a waitable `_beginthreadex` handle
  and serialized start/stop transitions.
- Stop publishes the cooperative flag, wakes the worker and calls `CancelIoEx`
  for the registered active session. Only the worker-owned `Session` closes its
  HID handle after terminal I/O reap.
- Added a post-request stop check before decode/publication so a completion
  racing shutdown cannot republish non-neutral input.
- Added one three-second generation join. Timeout retains thread/event/active-HID
  ownership, reports the exact stop result through the native registry, blocks
  restart and selects process containment.
- Added a simulator-only timeout scenario and a dedicated static ownership/
  shutdown audit.

### Validation

- Complete gate: 40/40 static audits and 26/26 portable C++ tests PASS.
- Hex80 production translation unit: MSVC 19.44 `/W4 /WX` PASS.
- Simulator rebuild and Hex80 timeout containment: PASS with expected exit 2.
- `hex80_protocol.cpp` and `.h` are unchanged in Git.
- Official `BUILD.cmd`: exit 0, 0 errors, only allowlisted `LNK4099`.
- 15-second Irok MG75 Max regression: 57,276/57,276 successful route requests,
  315 us average / 878 us maximum interval, balanced shutdown and zero trace
  ERROR. All 11 user-state files remained hash-identical.
- `HallJoy.exe`: 2,271,232 bytes,
  SHA-256 `540D4EB764FAD57E7431CA320F3E47420D7F0212C37A8D66EFFC1AAD3A6F6FAF`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-12C-S07-Hex80-prechange-20260801`.

### Package result

- Hex80 code-level lifecycle is implemented; physical Hex80 input, full-matrix,
  hotplug and shutdown qualification remain required before `Verified`.
- `HJ-AUD-P1-002` remains `Partial`: Addressed and Hex80 are now bounded and
  contained; MAD68 is the only remaining S07 backend.

## 2026-08-01 - V14-12D / S07 MAD68 bounded lifecycle

### Implementation

- Replaced MAD68's `std::thread` owner with a serialized waitable
  `_beginthreadex` generation and one three-second stop deadline.
- The live worker session registers its persistent overlapped read. Owner stop
  signals and cancels that read but never closes HID; worker withdraws the
  registration, reaps I/O and closes read/write/control handles.
- Every processing loop rejects a read that completes after stop. `SendCommand`
  rejects A8 after stop while the direct final A9 cleanup remains worker-owned.
- Timeout retains all reachable generation resources, reports the exact result,
  blocks restart and selects process containment.

### Validation

- Complete gate: 41/41 static audits and 26/26 portable C++ tests PASS.
- MAD68 translation unit: MSVC 19.44 `/W4 /WX` PASS.
- Simulator timeout containment: PASS with expected exit 2.
- Protocol files, Session command send, both write transports,
  `BestEffortRestore`, `RunStrategy` and `ProcessPayload` are unchanged.
- Official `BUILD.cmd`: exit 0, 0 errors, only allowlisted `LNK4099`.
- 15-second Irok regression: 57,247 successful route requests, one shutdown
  cancellation, 250 us average / 498 us maximum interval, balanced shutdown,
  zero trace ERROR and 11 hash-identical user-state files.
- `HallJoy.exe`: 2,272,768 bytes,
  SHA-256 `79F9E2509D56A80E71C17701F8FAB3DD65A39530E2F7F42C3E40E731AB139020`.
- Source backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-12D-S07-MAD68-prechange-20260801`.

### Package result

- `HJ-AUD-P1-002` is `Implemented`: all three affected native backends now have
  bounded truthful containment. Physical qualification is still required before
  `Verified`.
- S07 is code-complete. MAD68 hardware A8/A9, full-matrix, reconnect and shutdown
  gates remain release qualification.

## 2026-08-01 - V14-12E normal start/stop qualification runner

### Implementation

- Added `tools/run_release_qualification.ps1` for 1-1000 ordinary production
  cycles. It passes no simulator or fault-injection arguments, refuses to run
  over an existing HallJoy session, closes the exact parent with `WM_CLOSE` and
  applies a bounded shutdown deadline.
- Every cycle requires exit code zero, a committed-start/full-shutdown trace,
  no trace ERROR, no remaining HallJoy process, and records process/trace
  metrics. The complete LocalAppData file set is SHA-256 compared before/after.
- Added a static audit and made the runner a required official build asset.

### Validation

- Pilot: 5/5 cycles PASS.
- Official `BUILD.cmd`: PASS, zero errors and only allowlisted external LNK4099.
- Post-build qualification: 25/25 PASS on `HallJoy.exe`; shutdown 138-326 ms,
  average 245.1 ms. Peak HANDLE count was 218 in 24 cycles and transiently 225
  once, returning to 218 afterward. Remaining process count: zero every cycle.
- All 11 LocalAppData files remained hash-identical. Final trace had balanced
  worker shutdown and 1,830 successful Irok routes plus one expected
  shutdown-window cancellation; analyzer WARN is limited to intentionally
  unexercised analog-key, polling-mode and unplug/reconnect hardware gates.
- 42 static audits and 26 portable C++ tests PASS.
- `HallJoy.exe`: 2,272,768 bytes, SHA-256
  `D0CCF7EF1743EDB301EA00FB8615E7AC2F3055E1B9BF612EFF046530E7F814EB`.

### Package result

- The repeatable normal-cycle harness and 25-cycle pilot are Verified locally.
- This does not close the required 1000-cycle run, 8-24 hour soak, key/input
  exercise, reconnect or unavailable hardware-owner gates.

## 2026-08-01 - V14-12F / S18 dependency installer removal

### Implementation

- Deleted runtime GitHub API/latest resolution, WinHTTP/URLMon download,
  predictable temp output, Authenticode-before-execute, `runas`, `msiexec` and
  infinite installer waits from `app_deps.cpp`.
- Replaced the install contract with truthful manual guidance. When ViGEmBus is
  absent HallJoy displays exact version 1.22.0 and the official pinned release
  page, then remains in degraded mode until manual installation and restart.
- Added a pure constexpr guidance plan. Its four issue combinations and exact
  URL/version are portable-tested. The dependency lock independently pins the
  same version, URL and `manual-only` policy.
- Updated the private-UAP recovery audit: a system SDK/global UAP remains
  explicitly irrelevant, and no dependency installer exists in that path.

### Validation

- Installer-removal and dependency-lock negative static gates: PASS. Forbidden
  production tokens include URLDownload, WinHTTP, WinVerifyTrust, ShellExecute,
  runas, msiexec, waits and `releases/latest`.
- 43 repository static audits and 27 portable C++20 tests: PASS.
- Policy test and production `app_deps.cpp`: MSVC `/W4 /WX` PASS.
- Final official `BUILD.cmd`: PASS, zero errors and only allowlisted LNK4099.
- Post-build normal Irok regression: 3/3 cycles, exit zero, shutdown 112-241 ms,
  stable 209 HANDLE, zero remaining processes and 11 unchanged user files.
  Final trace recorded 6,309/6,309 successful SparkLink route queries and no
  ERROR event.
- `HallJoy.exe`: 2,206,208 bytes, SHA-256
  `6B5A3FB1009C1DB0C1916A3843A411EADA90DFAABFF5AE1D4EE08D2CC90E6C83`.

### Package result

- `HJ-AUD-P1-013` and `HJ-AUD-P1-014`: Verified. The dangerous installer path
  is absent rather than hidden behind a timeout.
- The missing-ViGEm modal was not opened manually because this workstation has
  ViGEm installed; message/policy correctness is covered structurally and by the
  pure test. Normal installed-driver behavior is runtime-verified on Irok.

## 2026-08-01 - V14-12G / S20 pre-qualification build and docs

### Implementation

- Replaced stale V6/V11 project testing/build/readme instructions with the real
  v1.4 unified runner, `BUILD.cmd`, x64 `HallJoy.exe`, LocalAppData storage and
  explicit hardware limitations.
- Rewrote the stale Addressed validator for central catalog/start/read/stop,
  exact-path ownership and bounded retained-generation shutdown, then required
  it from `run_native_backend_checks.py`.
- Marked the v3.9 validation package historical and non-authoritative.
- Removed unsupported Win32/x86 configurations because only x64 ViGEm/UAP
  artifacts ship. Both x64 configurations now use W4 with the documented narrow
  C4100/C4127/C4324/C4505 legacy baseline; actionable conversion warnings were
  fixed explicitly.
- The first direct Debug|x64 gate exposed its old debug-CRT/release-ViGEm
  mismatch. Debug now retains symbols, runtime checks, `/Od` and a project-local
  feature macro while deliberately using the `/MT` release CRT required by the
  bundled ViGEm client.
- Added a regression audit for all S20 invariants and converted the old fixed
  four-configuration Synchronization.lib assertions to per-supported-config
  checks.

### Validation

- Current Addressed validator and 44/44 repository static audits: PASS.
- 27/27 portable C++20 tests: PASS.
- Official clean `BUILD.cmd`: PASS, 0 errors, 0 compiler warnings and only the
  allowlisted external ViGEm LNK4099 diagnostic.
- Direct Debug|x64 rebuild: PASS, 0 errors, 0 compiler warnings and the same
  external LNK4099 diagnostic.
- Post-build Irok normal cycles: 3/3 PASS, exit 0, shutdown 139-254 ms, max 209
  HANDLE, zero remaining processes and 11 unchanged user files. Traces contain
  no ERROR; SparkLink total was 17,674 successful queries plus one expected
  shutdown-window cancellation.
- `HallJoy.exe`: 2,206,208 bytes, SHA-256
  `01A046A667DA012237E12C597ED84BE531AF20BC6338F55881C1C5197272559A`.
- Verified backup:
  `C:\github\HallJoy_v1.4_BACKUPS\V14-12G-S20-prechange-20260801-173804`.

### Package result

- `HJ-AUD-P3-001` through `HJ-AUD-P3-006`: Verified. Risk count is now 0 Open,
  3 Implemented, 1 Partial and 41 Verified.
- S20 is complete. S21 qualification is next. The pending external Aula result
  does not block ongoing work, but remains mandatory before release approval.

## 2026-08-01 - V14-12H / S21 qualification automation

Extended `tools/run_release_qualification.ps1` with per-cycle checkpoints,
terminal failure evidence, before/after LocalAppData manifests, bounded progress
output and SparkLink route counters. Added `tools/run_long_soak.ps1`: an
eight-hour-by-default production soak with a ten-second warm-up baseline,
periodic HANDLE/thread/GDI/USER/memory/CPU CSV samples, overlay responsiveness
probes, state invariants, graceful `WM_CLOSE`, trace analysis and fixed leak
limits. Both runners enforce the exact `HallJoy.exe` name and prohibit test or
fault-injection arguments.

The first soak pilot exposed a harness-only false positive because its resource
baseline preceded normal startup allocation; the gate was corrected to use an
explicit post-warm-up baseline. The second pilot exposed PowerShell's empty
pipeline-to-null behavior in the unchanged-state list; array normalization was
added. The third one-minute overlay pilot passed with 53 samples, HANDLE
210 -> 210, private-memory growth -86,016 bytes, 234,846/234,846 Spark routes,
zero trace ERROR, zero surviving process and 11 unchanged user files.

The unified gate passed the current Addressed validator, 45 static audits and 27
portable tests. `BUILD.cmd` passed with zero compiler warnings and only the
allowlisted external LNK4099. The resulting 2,206,208-byte `HallJoy.exe` has
SHA-256 `6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`.
Post-build Irok 3/3 passed: shutdown 259-325 ms, max 209 HANDLEs, 16,229
successful routes plus two shutdown-window cancellations, zero trace ERROR,
zero survivors and unchanged 11-file state.

V14-12H verifies the automation, not the final duration/count/device claims.
The 1000-cycle run, 8-24-hour soak, manual Irok input/reconnect and external Aula
hardware result remain S21 release gates.

## 2026-08-01 - V14-12I / S21 final 1000 production cycles

Ran the persistent qualification runner against the exact 2,206,208-byte
`HallJoy.exe` SHA-256
`6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`.
All 1000/1000 ordinary production cycles passed with one second of operation,
bounded `WM_CLOSE`, exit zero, complete error-free trace, no process survivor
and unchanged 11-file LocalAppData state.

Independent evidence verification found 1000 trace files and zero SHA-256
mismatch. Shutdown min/avg/p50/p95/p99/max was
101/277.1/250/430/1315/2662 ms; 16 cycles exceeded one second and all remained
below the 15-second bound. Peak HANDLE count was 217 and peak working set was
13,828,096 bytes. Spark accounting was exact: 1,598,879 queries = 1,598,454
successful + 425 single shutdown-window cancellations; no cycle recorded more
than one, and no corresponding ERROR or worker fault occurred.

The 1000-cycle S21 gate is Verified. The long soak, manual Irok input/reconnect,
unavailable device-owner matrix and external Aula hardware result remain.

## 2026-08-01 - V14-12J / S21 unattended-soak power request

The long-soak runner now holds a thread-scoped Windows
`ES_CONTINUOUS | ES_SYSTEM_REQUIRED` request and clears it in `finally`, so the
normal idle timeout cannot suspend an unattended qualification. A first pilot
correctly rejected PowerShell's signed interpretation of the raw high-bit hex
constant before HallJoy launch; unsigned constants were moved into the C#
WinAPI wrapper. The corrected one-minute pilot passed with
`system_sleep_prevented=true`, 29 samples, HANDLE 209 -> 209, 240,624 successful
Spark routes, zero non-ok routes, zero survivor and unchanged 11-file state.

## 2026-08-01 - V14-12K / S21 one-hour finding and Spark age fix

The agreed one-hour production soak completed after 3,604.553 seconds with 703
resource samples, 12/12 responsive overlay probes, HANDLE 210 -> 211, private
memory growth 180,224 bytes, 92 ms shutdown, zero surviving process and all 11
user-state files unchanged. A deeper review of its complete trace found two
abnormal SparkLink reconnects. Each was preceded by an impossible
`hotplug.stale` age near `UINT64_MAX`, proving unsigned subtraction underflow
when the main-thread tick timestamp was captured just before the worker
published a newer packet timestamp.

SparkLink freshness age now saturates to zero when the observed clock is older
than the published packet timestamp. Exact boundary, backward-time and
high-bit/maximum-value cases are covered by a portable C++ test and a static
integration audit. The soak runner now aggregates every `worker.stats`
generation instead of reporting only the last reconnect generation, rejects
impossible stale ages, defaults to the agreed 60 minutes and stores evidence
under `build/evidence`, outside the directory cleared by `BUILD.cmd`.

Correct aggregation for the original hour is 14,138,221 queries, 14,138,219
successful, two failed, 139,473 changed rows and 139,474 input notifications
across three worker generations, with two stale/reconnect events. The current
gate passed 46 static audits, 28 portable C++20 tests and the official x64
Release build with zero errors, zero compiler warnings and only the allowlisted
external LNK4099. The corrected 2,206,208-byte `HallJoy.exe` has SHA-256
`81609DC44D12F7DF44C2A7D801D8992CBFDCB45221F07AE041ED4711F4EB840C`.

A focused two-minute production regression on that exact EXE passed: 56
samples, overlay 2/2, HANDLE 210 -> 211, private-memory growth -12,288 bytes,
464,905/464,905 Spark routes in one worker generation, zero stale/reconnect,
zero survivor and unchanged state. Per D-029, the already completed one-hour
and 1000-cycle qualifications were not repeated for this narrow deterministic
timestamp correction. The corrected raw evidence is retained in
`build/evidence/S21-spark-age-fix-2m`. Raw evidence from the earlier hour and
1000-cycle run had been stored below `build/output` and was subsequently
removed by the documented clean-build behavior; their independently verified
numeric results remain recorded here and in the validation matrix.

Verified pre-change backup:
`C:\github\HallJoy_v1.4_BACKUPS\V14-12K-S21-spark-age-prechange-20260801-204000`.

## 2026-08-01 - V14-12L / S21 second-pass stability audit fixes

A second complete code audit found the same future-timestamp underflow shape in
Sayo, an expired-deadline conversion that could pass `DWORD_MAX` to Addressed
HID waits, and signed overflow before Raw Mouse clamping. Shared header-only
helpers now provide saturating monotonic age, remaining timeout and widened
integer addition. Exact boundary and maximum-value cases have deterministic
portable coverage.

The audit also found that exception barriers contained overlay and ViGEm output
faults but left both services disabled until process restart. Under D-030, the
UI owner now supervises those workers, reaps only signaled and confirmed-joined
generations, recreates ViGEm transport after the old owner is gone, requests a
fresh report and preserves overlay autostart intent across a worker fault.
Simulator-only one-shot C++ exception injections prove both recovery paths and
balanced final shutdown without permitting overlapping generations.

Rare ownership exits were tightened: failed clipboard transfer frees its global
allocation, UAP SetupAPI interface lists are freed on both early returns, the
Madlions state reset uses its actual object extent, and DrunkDeer reserves the
destination buffer. Targeted arithmetic, static ownership, overlay and ViGEm
fault-injection gates pass. The pre-change Git backup is
`backup/pre-second-audit-fixes-20260801` at
`aab14229976115ac1d9503fb8ae8647cfca4f94d`.

Full qualification passed 48 static audits, 29 portable C++20 tests and all
four Aula Clang ASan+UBSan suites. The first official build correctly rejected
the intentionally changed Soup overlays against their old integrity hashes;
the two normalized hashes and the fresh-patch safe initialiser contract were
updated, after which locked plugin generation and the full Release x64 build
passed with zero compiler warnings and only the allowlisted external ViGEm
`LNK4099`. Both final simulator C++ fault scenarios recovered and shut down
balanced. The production Irok regression passed 3/3 with 18,103 successful
routes plus two shutdown-window cancellations, 196-232 ms shutdown, maximum
215 HANDLEs, no process survivor and all 11 state files unchanged.

Final `HallJoy.exe` is 2,209,280 bytes, SHA-256
`9C5C206E196753D25C83F2DE012607B6ED372AEB3B72593E410865FF4B0777D4`.
V14-12L is Verified. External Aula hardware remains release-blocking.

## 2026-08-01 - V14-12M / S21 MAD68 HE UAP shutdown containment

A tester reported that an earlier executable remained running with MAD68 HE.
The device clarification changed the affected route: MAD68 HE uses HallJoy's
modified private UAP and Soup, while MAD68 Pro R is the unrelated native A0
backend. The historical root cause is not asserted without the old tester
trace. Physical retest instructions request waiting up to 15 seconds, checking
whether only the window or also the process remains, and returning
`HallJoyStabilityTrace.log` beside the old EXE.

The isolated analog-host supervisor now treats a heartbeat loss during shutdown
as a shutdown deadline, not a runtime crash. A simulator child that blocks
forever before plugin unload is terminated after the 2.5-second graceful
deadline, confirmed exited, and joined without restart. Its saved trace records
2,641 ms from shutdown arm to `child.stop_timeout`, normal parent exit zero and
released IPC/worker ownership.

A process-wide 12-second watchdog is armed before the first app cleanup call
and stays armed through GDI+, debug-log and stability-trace teardown. It uses an
independent Win32 thread and `TerminateProcess` without logger or CRT calls.
The permanent owner-stop injection exits with expected code 4 and leaves zero
processes. Native MAD68 Pro R owner cancellation was separately moved behind
its bounded join boundary; the worker retains its OVERLAPPED/buffer/HID lifetime
and observes stop in 25 ms read slices, preserving final A9 recovery.

Dependency provenance was made explicit. HallJoy uses both layers: a locally
modified UAP, with pinned Soup inside it; Sun is only the pinned build tool.
All three are MIT-licensed at the locked revisions. Full notices now ship beside
the EXE, and updates cannot enter automatically because immutable commits and
all five Soup overlay hashes are enforced by the build.

The full gate passed 48 static audits and 29 portable C++20 tests. Permanent
UAP-child, permanent owner-shutdown and normal simulator scenarios passed. The
clean locked plugin/ABI build and official Release x64 build passed with zero
compiler warnings and only the allowlisted external ViGEm `LNK4099`.

Final `HallJoy.exe` is 2,210,304 bytes, SHA-256
`C06AD4C7257244E4370738465BBADF815DBC41A081065CA30B0BCFF8059FA1A3`.
The exact production artifact passed 5/5 Irok cycles: 122-226 ms shutdown,
maximum 209 HANDLEs, 33,461 successful Spark routes plus one shutdown-window
cancellation, no survivor and unchanged 11-file state. Raw evidence is retained
under `build/evidence/V14-12M-uap-shutdown-20260801` and
`build/evidence/release-qualification/20260801-235837`.

V14-12M code containment is Verified. Physical MAD68 HE retest remains a P1
release blocker, independently of the already open physical Aula gate. The old
tester EXE is superseded and must not be used for release acceptance.

## 2026-08-02 - V14-12N / S21 all-keyboard shutdown containment

Audited every production keyboard route against the MAD68 HE class of failure:
bounded owner stop, truthful incomplete result, retained lifetime on timeout,
process containment and actionable stability trace. Five native routes already
had permanent-stop injections; Aula had the correct three-second bounded join
and resource-retention policy but no process-level permanent-stop proof. Added
a simulator-only Aula injection without changing its protocol, HID commands or
production selection behavior.

Added `run_keyboard_shutdown_matrix.ps1` and a static coverage audit. One
simulator build now runs a normal control, all six native permanent-stop paths,
the shared private UAP/Soup child-unload stall and the global 12-second
watchdog. The matrix is catalog-aware, retains and hashes a separate trace for
each scenario, checks exact expected exit/evidence and rejects any surviving
HallJoy process. All 9/9 scenarios passed with zero survivors; evidence is in
`build/evidence/keyboard-shutdown-matrix/20260801-212947`.

The full unified regression gate passed, including the Aula protocol/oracle/
session suites and 250,000-frame parser fuzz smoke. The clean locked UAP build
and official Release x64 build completed with zero errors and no unexpected
warnings. Final `HallJoy.exe` is 2,210,304 bytes, SHA-256
`E12080E95DD394462FC36C842517F168F6CE4423CE9357B89D2320A20A962BB8`.
On the physically available Irok MG75 Max, 5/5 production cycles passed with
140-234 ms shutdown, maximum 209 HANDLEs, 33,409 successful queries plus three
shutdown-window cancellations, zero survivors and unchanged 11-file state.

V14-12N is Verified for code-level containment. Per D-033, simulator evidence
is not hardware evidence. Physical MAD68 HE retest and Aula acceptance remain
release blockers and continue asynchronously.

## 2026-08-02 - V14-12O / S21 input-to-overlay production profiling

Built a production-only load profiler around the exact released architecture,
not the simulator. It records the complete HallJoy process tree (main, private
UAP host and diagnostic watcher), persistent worker TIDs from the stability
trace, residual UI/short-lived worker CPU, the full headless Chrome tree,
system context, memory, HANDLEs, threads, physical Spark route timing and both
server/browser overlay telemetry. Three separate phases cover server idle, the
real overlay page and a continuously animated 32-key stress page. The runner
hashes raw evidence, rejects user-state mutation and rejects surviving HallJoy
processes.

The baseline production artifact showed HallJoy at 0.835% machine CPU and
Chrome at 14.407% during real 1 ms polling, with roughly 174.7 complete canvas
draws per second. The browser was the dominant cost. The page now keeps the
last canvas frame, converges smoothing to an exact target and redraws only for
resize, layout, visual style or visible-depth changes. Sprite and label caches
are bounded at 512/256 entries and use constant-time insertion-order LRU.
Fresh profiles default to 8 ms; the supported 1 ms option and all existing user
settings remain untouched.

The final 2,212,352-byte `HallJoy.exe` SHA-256 is
`06CF73B59827E957DDF9644AC2557C601F5602FD454AC7B025FB6533C041A462`.
The official build passed with zero errors and only the allowlisted external
ViGEm `LNK4099`. Two final 1 ms profiles place the complete HallJoy tree at
0.809-0.929% and Chrome at 5.451-6.120%. The fully attributed run records Spark
0.656%, realtime 0.017% and UI/short-lived HTTP 0.119%; JSON/send averages are
26.7/28.3 us. The exact final 8 ms comparison is HallJoy 0.721%, Chrome 3.747%
and one settled draw in ten seconds.

Physical Irok accounting passed 277,055/277,055 in the full-stage run and
217,628/217,628 in the 8 ms comparison. Both preserved user state and left zero
processes. Five final production lifecycle cycles also pass with exit zero,
128-219 ms shutdown, maximum 209 HANDLEs and unchanged 11-file state. Evidence
is under `build/evidence/input-pipeline-profile/20260802-113109`,
`build/evidence/input-pipeline-profile/20260802-113432` and
`build/evidence/release-qualification/20260802-112901`.

V14-12O is Verified within the physical evidence boundary: Irok/SparkLink and
the downstream realtime/ViGEm/overlay/browser chain. It does not claim physical
USB cost or protocol correctness for unavailable keyboards. MAD68 HE/UAP retest
and Aula acceptance remain asynchronous release blockers.

## 2026-08-02 - V14-12P recoverable factory reset

Added a visually consistent owner-draw `Reset All Settings` button to Global
settings. It uses the existing dark action-button renderer with a restrained
red border/accent, keyboard focus, muted explanatory copy and a full destructive
scope confirmation whose default action is `No`.

The reset itself is a restart-time transaction. The live process atomically
writes and validates a request, performs normal settings persistence and the
complete bounded shutdown, tears down logging/trace/watchdog state, and only
then relaunches. Before loading settings, the new process moves the two root INI
files and three preset/profile directories into a unique recoverable backup.
Migration markers and unrelated data remain untouched. The request is committed
only after fresh directories exist; every earlier failure performs reverse
rollback and distinguishes complete from incomplete recovery in both trace and
UI text.

Added `factory_reset_static_audit.py` and `run_factory_reset_test.ps1`. The
runtime gate created isolated state, committed the atomic request, injected a
failure after three actual moves, proved byte-exact restoration, retried, then
verified all five backup hashes, clean defaults, preserved unrelated/migration
files and zero process survivors. Evidence:
`build/evidence/factory-reset/20260802-121843-951/summary.json`.

The complete static gate and official build pass with no unexpected warning.
Final `HallJoy.exe` is 2,225,664 bytes, SHA-256
`33BEB1DE0DA8B896FA82E61A52F29ED4A8796B09A7134325346971C70CFEC597`.
One physical Irok cycle passed with 6,542/6,542 routes, 164 ms shutdown,
maximum 209 HANDLEs, unchanged 12-file state and zero survivor; evidence is
`build/evidence/release-qualification/20260802-122413/summary.json`.

V14-12P is Verified. Reset backups are deliberately retained for recovery.
Physical MAD68 HE and Aula acceptance remain independent release blockers.

## 2026-08-02 - V14-12Q Global-settings scroll and danger-fill correction

User review exposed two presentation defects in the new factory-reset section.
Global settings was still a fixed-position page, so compact windows could clip
its lower content instead of exposing the themed scrollbar used elsewhere. The
danger renderer also changed only the border/accent at rest and inherited the
ordinary gray control background.

Global settings now uses the shared `CustomPageSurface` geometry and owns
bounded scroll state, conditional track/thumb drawing, wheel and vertical
commands, track paging, captured thumb dragging and capture-safe teardown. All
children are laid out in content coordinates minus the current scroll offset.
The reset action now uses a solid dark-red idle fill with separate hover and
pressed colors. The page inventory audit records the deliberate exception:
Gamepad Tester scales its card grid to the viewport and does not overflow.

The expanded static audit, reset rollback/retry runtime gate, unified static
checks and official x64 Release build pass. Final `HallJoy.exe` is 2,228,224
bytes, SHA-256
`6DFC616422D89783A846F7EE8CAEA64AD7D951591092576DEFB717320543DF96`.
One physical Irok cycle passed with 6,660 successful SparkLink queries, 193 ms
shutdown, maximum 209 HANDLEs, unchanged 12-file state and zero survivor;
evidence is
`build/evidence/release-qualification/20260802-125134/summary.json`. Reset
transaction evidence is
`build/evidence/factory-reset/20260802-124758-494/summary.json`.

V14-12Q is Verified. The correction changes no keyboard protocol and does not
close the pending physical MAD68 HE/UAP or Aula release gates.

## 2026-08-02 - V14-12Q manual UI rejection and clean-audit handoff

The owner performed a physical Irok visual review after the automated V14-12Q
gate and rejected the UI. Analog key input visibly flickers the interface,
especially the tab row. Configuration telemetry remains stale until hover over
the custom-painted `HE poll mode` or `Rows` selectors. Those selectors are not
real comboboxes: each click cycles the value. Backend diagnostics are duplicated
between Configuration and Gamepad Tester. The reset button's separate left red
accent and standard dotted GDI focus rectangle were also rejected.

Code inspection confirms the reset accent/focus implementation, fake-combo
implementation and cache-dirty cause of hover-dependent telemetry refresh. The
precise invalidation chain behind the global flicker is not yet proven and must
be instrumented rather than guessed. V14-12Q is reopened and the current EXE is
not a release candidate.

The complete defect inventory, confirmed facts, open hypotheses, audit plan and
acceptance criteria are recorded in
`RELEASE_UI_AUDIT_HANDOFF_2026-08-02.md`. Backup branch
`backup/pre-release-ui-audit-20260802` preserves commit `3b3e9a3` before the
new audit. Physical MAD68 HE/UAP and Aula acceptance remain separate blockers.

## 2026-08-02 - V14-12R clean code/log UI audit

Created backup branch `backup/pre-clean-ui-audit-20260802` at
`63024a9907dc` before source changes. Re-read the complete v1.4 documentation
set and audited the six-tab HWND/owner-draw/custom-surface tree without using
old visual status as evidence and without retaining screenshots.

Implemented dirty-rect buffer commits, a separately buffered tab row, active-
tab Remap invalidation, changed-hash live telemetry gates, partial Configuration
status updates, real Spark `PremiumCombo` children, canonical route diagnostics
in Gamepad Tester, conditional Tester scrolling, and one rounded reset renderer
without the rejected strip/dotted focus. Added `HallJoyUiAudit` as a build-only
trace target plus `pre_release_ui_static_audit.py` in the official build gate.

Validation:

- UI static audit PASS;
- production and UI-audit x64 builds PASS;
- safe instrumented Irok run: Configuration steady dirty area `720x36`, no
  telemetry-driven tab-row paints, exit 0 and normal shutdown marker;
- release qualification 3/3 PASS with unchanged state and zero survivors;
- simulator 15-second isolated-data rerun PASS (the first 8-second attempt was
  too short for two late opposing-key phases);
- final package: 2,228,224 bytes, SHA-256
  `6BCBA47D86448E7D262250AEAF7EAFD89FD448CA8A917B1C514711F31FCB6CC3`.

An earlier external automation attempt used an invalid cross-process
`TCM_GETITEMRECT` pointer and crashed COMCTL32. It was discarded, its crash
artifacts were removed, and the scalar-message rerun completed normally.

Status remains In progress. The owner must perform the visual/DPI/control-state
matrix on the final rebuilt EXE; MAD68 HE/UAP and Aula are unchanged blockers.
Detailed record: `PRE_RELEASE_UI_AUDIT_2026-08-02.md`.

## 2026-08-02 - V14-12S UI refresh root correction

After owner testing exposed refresh failures, backed up commit `63024a9907dc`
as `backup/pre-ui-refresh-root-fix-20260802` and saved the complete dirty layer
under `build/backups/ui-refresh-root-fix-20260802-142333`.

Found and corrected the lost keyboard-preview release transition: the preview
is outside the tab pages and always visible, so consuming dirty bits behind a
Remap-only invalidation gate was invalid. Split Tester animation-rate gamepad
reports from 100 ms route telemetry. Rebuilt Remap, Configuration and Global
thumb scrolling around 16 ms last-value frame coalescing, batched no-redraw
child positioning and one no-erase commit. Configuration now batches both
key-mode/profile combos and both Spark combos. Removed unrelated key-driven
tab-row repaint. Fresh overlay smoothing now defaults to 15 while persisted
profiles retain their value.

The code harness activated all six pages, generated input and stress-dragged
scrollbars without screenshots. Configuration dropped from 120 paints/s and
258 erase messages to 24 burst paints and zero active-interval erases; tab-row
key churn dropped from about 60 paints/s to selection-only paints. Full build,
static UI audit, UI-audit build, 15 s simulator and final production
qualification 3/3 pass. Final EXE is 2,230,784 bytes, SHA-256
`1B5671F36EDE9CD2CB1153A2D02729387D0974D25FFB031D457A4FDC7AB523D1`.
Visual acceptance remains pending owner review.

## 2026-08-02 - V14-12S.1 scroll deadline starvation

Owner accepted the corrected visuals but reported low scroll FPS on several
pages. Runtime stress reproduced ~29 FPS and proved that low-priority
`WM_TIMER` delivery plus the 15/16 ms Windows timing boundary skipped frames.
Remap, Configuration and Global now commit elapsed deadlines directly while
handling mouse input; the timer only drains the final pending target. Cadence
follows UI refresh within 8–16 ms with a 1 ms clock-granularity tolerance.

Alternating Configuration stress reached approximately 70 commits/s with zero
active erases. Full build and final 3/3 production qualification pass, with 22
user-state files unchanged. Artifact: 2,231,808 bytes, SHA-256
`F9A32FEB956E7ED38CE7CB75BBE8254D64B296259821B9922ECA8EFF9D5B040C`.

## 2026-08-02 - V14-12T unified scroll viewport architecture

The owner rejected V14-12S.1 after visual testing: FPS was higher, but elements
could disappear while scrolling. That invalidated the per-page child-window
coalescing direction. The root fault was mixed composition ownership, not one
timer interval.

Backed up the complete working state to branch
`backup/pre-unified-scroll-architecture-20260802` at
`63024a9907dc946f4533c94a459fd65acec03df5` and to
`build/backups/unified-scroll-architecture-20260802-150715`.

Extended `custom_page_surface` with one wheel/thumb/track/capture controller,
content/client coordinate conversion and one retained viewport presenter.
Migrated the active paths of Remap, Configuration, Gamepad Tester, Global
settings, Input Overlay and Mouse settings to that controller. Remap cards and
icons now render as one retained layer. Configuration/Global closed combo faces
render in their page caches; native combo HWNDs are popup/keyboard controllers
only. Dynamic Tester bars remain live but use the same viewport contract.

Added architecture guards and `tools/run_ui_scroll_stress.ps1`. Final gates:
static audits PASS; full production build PASS; 6/6 page stress with 240
wheel/update cycles per page PASS and no steady-state GUI resource growth; 3/3
lifecycle qualification PASS with 22 user-state files unchanged. Final EXE:
2,232,832 bytes, SHA-256
`851C84A63AB6A1532C21E4FE477A16F9D9248BF4AB76090E3DD9AEAA969C2509`.
Automated status is Implemented/PASS; owner visual acceptance remains pending.

## 2026-08-02 - V14-12T.1 retained-control visual parity

Owner testing accepted the unified scroll behavior and found four visual
regressions: a mojibake-like Remap disable glyph, button-like retained combos,
an invalid dirty suffix, and a face/font/focus change while a popup was open.
Backed up the dirty tree to
`build/backups/post-unified-scroll-visual-regressions-20260802-162140`.

Added `PremiumCombo::PaintRetainedFace` so KSP, Spark and Global closed faces use
the canonical combo painter instead of approximations. Restored the vector
power glyph and vector save icon, removed the added inner focus outline, and
added an explicit popup state notification so controller HWNDs hide on every
close path.

Full rebuild and static gates pass. The production runner passed all six scroll
pages plus popup lifecycle checks for Configuration preset, Global profile and
Keyboard layout; each had one popup/controller while open and zero after close.
GDI and USER handles were flat. Lifecycle qualification passed 3/3 with 28
state files unchanged. Artifact: 2,233,344 bytes, SHA-256
`76CB02D76131E72B78652969C3673F3A2E586BCB843FD4ED57B78F51F6FD4677`.
Visual acceptance of this exact artifact remains pending owner review.

## 2026-08-02 - V14-12T.2 retained interaction semantics

Fixed three root interaction regressions without changing the accepted unified
scroll path. Remap remove IDs (`3000+`) had been swallowed by an unbounded
`>= REMAP_ICON_ID_BASE` drag test; icon classification is now bounded by the
actual collection. Every user binding mutation now uses one transaction which
persists active bindings, marks the global profile dirty, refreshes Global
settings and requests normal application persistence.

Input Overlay direction, depth and label font are now real PremiumCombos. The
former click-to-cycle branches were removed. Global dirty state shows
`Global profile - unsaved` plus the existing save icon.

Backup: `build/backups/pre-remap-profile-overlay-semantics-20260802-164735`.
Full build and static audit PASS. Stress: 6/6 pages x 240 cycles and 6/6 popup
lifecycle cases PASS; GDI `203/207/204`, USER `231/231/230`, exit 0. Artifact:
2,232,832 bytes, SHA-256
`8BE26D58294AD7E02D38C0970E4A8F6BD321A976638DC0E809A0425020D714CA`.
No screenshots; owner interaction acceptance is pending.

## 2026-08-02 - V14-12T.3 PremiumCombo popup wheel routing

The font list exposed a controller/popup routing gap: `WM_MOUSEWHEEL` is sent
to the separate top-level popup under the pointer, while list scroll state is
owned by the combo controller. `PopupProc` now forwards the original wheel
message synchronously to that single implementation. Backup:
`build/backups/pre-premium-combo-wheel-20260802-174051`.

Static audit and full production build PASS. The runtime runner now injects six
wheel messages into every open popup; all 6/6 popup cases remain responsive and
open during routing, then close with zero visible controllers. Six-page stress
also passed at 240 cycles/page. GDI `105/105/102`, USER `174/177/176`, exit 0.
Artifact: 2,232,832 bytes, SHA-256
`45AEDB2FA1952843B004FED3F80EAD7F18DC8357FAD5EF2D64BBE237A6AC221B`.

## 2026-08-02 - V14-12T.4 overflow-only combo viewport scrolling

Owner review rejected V14-12T.3 because routed wheel input still called
`MoveHot`, changing the highlighted option instead of scrolling the list.
Replaced it with `ScrollPopupWheel`: it changes only `scrollTop`, activates only
when `GetMaxScrollTop() > 0`, respects the Windows wheel-lines setting and
accumulates high-resolution deltas. Selection and hot option are not mutated.
Backup: `build/backups/pre-premium-combo-viewport-wheel-20260802-175224`.

The runtime guard now reads combo scroll state. Five fitting popups report
`maxTop=0`, stay `0->0`, and preserve selection. The 13-font popup reports
`maxTop=3`, scrolls `1->3`, and preserves selection. Full build and 6/6 page
stress PASS; GDI `105/106/102`, USER `176/176/176`, exit 0. Artifact:
2,233,856 bytes, SHA-256
`ED0082DFDC24F8A4137B1559D1B43058186C19ED4B9142E7F9BEE107F65EB00D`.

## 2026-08-02 - V14-12U Aula physical diagnostic and backend isolation

The second physical Aula log proved that SparkLink repeatedly opened the exact
Aula `1CA2:1902 / FFA0` interface: 9 opens and 9 failed Spark protocol probes in
14.9 seconds. Added a pre-open dedicated-family gate so Spark never sends its
protocol to Aula; a rejected Aula path remains unclaimed and available to UAP.

Added an isolated aggressive diagnostic build. It continues past semantic
capability mismatches to later read-only stages, while strict claim/publication
remain mandatory. Transport/correlation failures close and reopen the session.
Raw reports are traced with sync serial bytes redacted and HID identities hashed.

Backups: `pre-aula-aggressive-trace-20260802-221717` and
`pre-spark-aula-routing-20260802-222839`. Sanitizers and three routing/static
audits PASS; production and diagnostic builds PASS. Diagnostic artifact:
2,245,632 bytes, SHA-256
`D6F48D134481668DE9819A457CEFFC6FE5A97F5A6BD5980CFE6FE4529F1F8036`.
The delivery was simplified to exactly one EXE. It creates/overwrites one
64 MiB `HallJoy.log` beside itself; no collector, script, archive, previous log
or portable marker is delivered. The full native backend suite also passed.
Physical strict proof remains pending the returned `HallJoy.log`.

## 2026-08-02 - V14-12U.1 physical 60-byte sync envelope

Two single-EXE traces separated a transient ownership failure from the actual
protocol barrier. `HallJoy (1).log` had 9/9 exclusive-open sharing violations.
`HallJoy (2).log` then opened exclusively 62/62 times and received one stable,
checksum-valid `0x81` response to every sync request. Its physical payload is
60 bytes, not the 54 bytes inferred by the hardware-unvalidated oracle.

The isolated aggressive parser now accepts either the pinned 54-byte oracle
envelope or the physically observed 60-byte envelope. Production remains
54-byte strict. The extended response continues through read-only proof with a
firmware mismatch, so claim and analogue publication remain blocked. Added an
ASan/UBSan aggressive end-to-end fixture that reproduces the redacted physical
frame and proves all 17 transactions execute while the mismatch mask remains
set. Backup: `build/backups/pre-aula-physical-sync60-20260802-230529`.

Sanitizers 5/5, Aula/routing static audits, full native backend checks,
production build and diagnostic build PASS. New single-file diagnostic:
2,245,632 bytes, SHA-256
`4AC9B51E9EE1824E6050400FF09F94B763084EEEE7A816A6D8A4290E938D54CA`.

## 2026-08-03 - V14-12U.2 physical Aula production contract

`HallJoy (3).log` (66,147 bytes, 223 lines, SHA-256
`30FFE7CFB512F9FCE5988D71FF38D2F58922957DCEE7B675CA5113E7A7979DAB`)
completed three exclusive 17-transaction proofs. Precision `10/10/3400`, the
61-position/60-usage default map, two active Fn0 generations and both travel
envelopes matched exactly. The sole mismatch was our interpretation of the
physical sync build descriptor.

Replaced the hardware-unvalidated 54-byte sync oracle with the repeated physical
60-byte contract in production. The three 16-byte descriptor blocks and final
`FF` are pinned; the device-specific serial block is retained for reconnect but
excluded from firmware equality. Misleading build-date decoding was replaced
with the proven ASCII label prefix. Legacy 54-byte responses now fail closed.
Backup: `build/backups/pre-aula-physical-production-contract-20260803-001133`.

Sanitizers 5/5, full native backend suite, documentation/static gates and both
MSVC Release builds PASS. Production artifact: 2,235,392 bytes, SHA-256
`8F6F85CD17BCE471728006BA4642203142815C708407C64E2C21F5DCF25817D0`.
Single-file claim-capable diagnostic: 2,245,120 bytes, SHA-256
`23CEC8D7EF2479B353EABE7AAB8857CB04BDF3CC6BD0FE3D1FC89DAA1C02BB14`.

## 2026-08-04 - V14-12U.3 physical Aula runtime input

The owner returned two claim-capable traces and reported that analogue input
appeared in HallJoy. `HallJoy (4).log` is 1,840,992 bytes / 9,311 lines, SHA-256
`8529C724EDA13F93892237B11C5012D85FA8CD36A38BC71D85B64EF4BAC7E52C`;
`HallJoy (5).log` is 629,044 bytes / 3,015 lines, SHA-256
`5C1FC4F0DFC1AE152EA395463CD458C6123F08A27F783C05E2B8E9B8EDFF2A48`.
Both runs completed strict proof with zero mismatch, claimed the dedicated Aula
route, connected and published the 60-key matrix. They then sustained polling
for about 58 and 17.5 minutes respectively.

The trace cap records only the first 256 ordinary protocol reports. Its 58
complete travel-half frames per log were captured during startup and are all
zero; the later physical presses are therefore owner-observed evidence, not a
claimed non-zero byte capture. The first run ends with a real HID disappearance
and 59 bounded rediscovery attempts. The keyboard did not reappear before exit,
so disconnect/retry passed but reconnect remains pending. The second run's sole
continuation-read failure coincides exactly with shutdown and is cancellation
of an in-flight transaction. Backup:
`build/backups/pre-aula-physical-input-evidence-20260804-001`.

## 2026-08-04 - V14-12U.4 useful single-run Aula telemetry

Logs 4/5 exposed three diagnostic design failures: a 256-report cap retained
startup proof traffic instead of runtime activity, Spark emitted thousands of
identical per-interface skip lines, and shutdown `CancelIoEx` was classified as
a protocol warning. Replaced that blind trace shape with a diagnostic-only,
allocation-free matrix metrics path. It emits 5-second real polling-rate and
transaction-latency windows, active-key histograms including a dedicated 10+
bucket, press/release-to-zero transitions, event snapshots with HID/row/column/
micrometre values, final per-HID maxima, session summaries and reconnect
downtime. Spark skip evidence is aggregated to at most one line per minute;
shutdown cancellation is INFO and does not increment runtime failures.

Added a portable metrics test covering 10 simultaneous keys, complete release,
frequency, latency buckets and coverage. Aula ASan/UBSan is now 6/6 PASS; the
full native/static/portable suite and official production build pass. The
production linked image contains none of the high-detail diagnostic markers.
The isolated package contains exactly one 2,254,336-byte `HallJoy.exe`, SHA-256
`F2727D0A7E901DF89D95B27B1D0CD86D7D2B9655B59EB4998F62594FCAF158C5`.
Its builder verifies the linked schema before delivery. Backups:
`build/backups/pre-aula-diagnostic-telemetry-v2-20260804-001` and
`build/backups/pre-aula-diagnostic-v2-docs-20260804-001`.

## 2026-08-05 - V14-12U.5 physical rate and multi-key evidence

`HallJoy (7).log` (142,904 bytes / 417 lines, SHA-256
`EBDDF2DCEA3D72BBCA1E6219A340312A0BB55167826F6BC2187FC41079B968A9`)
validated telemetry v2 and the physical runtime path. Strict proof/claim passed;
21,027/21,027 matrices completed in 61.129 seconds with zero failed updates.
Lifetime rate was 343.973 Hz and all twelve 5-second windows stayed between
340.245 and 346.178 Hz. Both travel transactions averaged 2,076 us, peaked at
2,644 us and never entered a bucket above 4 ms.

The keyboard reached 22 simultaneous active keys, spent 2,654 frames at 10+,
returned fully to zero eight times and exercised 40 HID usages up to 3,400 um.
Shutdown cancellation was correctly informational and every worker joined.
No disconnect/reconnect occurred, so the same diagnostic EXE still needs one
short unplug/replug run; no replacement binary is required. Documentation
backup: `build/backups/pre-aula-log7-evidence-20260805-001`.

## 2026-08-05 - V14-12U.6 physical reconnect closure

Reviewed `HallJoy (8).log` (592,665 bytes / 1,800 lines, SHA-256
`3360D442A527DA993E846B6F88456406BAD2EADD02B4A18E3FAF49C63A0041C7`).
The run contains three disconnects and three successful reconnects. Every
recovered connection retains the original physical identity and completes the
strict Aula proof with zero mismatch.

The first recovered session is decisive functional evidence: 1,008 matrices at
341.463 Hz, 329 non-zero frames, 238 changed frames, four complete releases and
12 observed HIDs reaching 3,400 um. Thus the result proves restored analogue
data, not merely HID re-enumeration. Later write/read failures coincide with two
additional USB transitions; the bounded retry loop reclaims the device both
times. Shutdown is clean, all workers join, and the process exits 0.

`HJ-AULA-P1-009` is Closed/PASS. Together with log 7's sustained rate and
multi-key evidence, all Aula physical blockers required for the production
artifact are closed. The next build should remove diagnostic telemetry and keep
only crash-oriented production logging. Documentation backup:
`build/backups/pre-aula-log8-evidence-20260805-001`.

## 2026-08-05 - V14-12U.7 final production build

Converted the official artifact from the temporary verification profile to a
true zero-continuous-telemetry release. `tools/build.ps1` explicitly passes
`HallJoyStabilityTrace=false`, no longer packages trace collectors and validates
the linked EXE for absence of stability, diagnostic and high-detail Aula markers.

Moved ordinary debug and stability APIs to production compile-away call sites.
Discarded `if constexpr(false)` branches retain type checking and mark diagnostic
locals as used, but generate no argument evaluation, strings, calls or runtime
branches. This avoids depending on cross-TU optimizer behavior. The ordinary log
writer/file path stays disabled. Production now installs only the unhandled crash
filter; it performs no normal I/O and writes privacy-limited `HallJoyCrash.txt`
only after a crash. The vectored first-chance hook remains diagnostic-only, while
the silent native A9 exit watchdog is preserved.

The full native suite passed. Official W4 Release x64 completed with zero errors
and zero unexpected warnings; only the accepted third-party ViGEmClient LNK4099
baseline remains. Linked-image marker audit passed. An isolated hidden portable
runtime smoke ran for five seconds, accepted WM_CLOSE, exited 0 and created no
ordinary, trace, diagnostic or crash log.

Published clean `build/release/HallJoy.exe`: 2,161,152 bytes, SHA-256
`AF7C536FF454AF94278C457E2A978E447E9345580240253F7B603748AB79C39F`.
Package also contains README, third-party notices and `SHA256SUMS.txt`. Backup:
`build/backups/pre-final-production-profile-20260805-001`.

## 2026-08-05 - public v1.4 release notes and repository status

Reviewed the complete `v1.3..v1.4-integration` history and the remaining
working-tree changes before GitHub publication. The 55 committed changesets are
all v1.4 work: runtime/device architecture, persistence/security, stability,
qualification and UI hardening. The remaining source, test and documentation
diff is the final unified UI scroll/control work, Aula physical protocol and
rate/reconnect proof, and conversion from the temporary diagnostic profile to
the zero-continuous-telemetry production target. No unrelated tracked log or
crash artifact was found in the publication scope.

Added the public Russian release document
`RELEASE_NOTES_v1.4.md`. It describes user-visible device support, the shared
six-tab scroll architecture, control fixes, low-latency ViGEm scheduling,
persistence migration, security boundaries, cooperative shutdown, production
diagnostics, validation evidence, v1.3 upgrade steps and known limitations.
Updated the repository README to point to the release notes, use the clean
`build/release` artifact, record physical Aula acceptance and document the
crash-only production log contract. Replaced the stale development/rejected UI
summary in this index with the final qualification state.

Verified backup before documentation writes at
`build/backups/pre-public-v14-release-notes-20260805-002`; all three copied files
matched their sources by SHA-256. The earlier `-001` flat backup is intentionally
not authoritative because the two same-named README files collided in one
directory; no source file was affected.

## 2026-08-05 - explicit supported-hardware matrix

Audited the actual discovery and proof boundaries of all six native protocols
after the release-scope review exposed ambiguity between brand support, dynamic
protocol compatibility and physical model validation. Aula is intentionally
restricted to the exact `1CA2:1902 / FFA0:0001 / 65-byte / App V1.1.6`
identity and complete 17-transaction proof; another VID/PID with the same wire
commands is not automatically accepted. Hex80 accepts proven PID variants only
within `VID 373B`; Sayo accepts proven PID variants only within `VID 8089`;
MAD68 additionally requires the 68-key family boundary; Addressed and SparkLink
are deliberately dynamic after their stronger live protocol proofs.

Added `SUPPORTED_HARDWARE.md` with separate sections for physical HallJoy
evidence, native protocol-compatible families and the device list declared by
the pinned UAP/Soup runtime. Linked it from the public README and release notes.
Updated the Aula protocol document's stale pre-hardware wording with the final
rate, rollover and reconnect evidence. Documentation backup verified by SHA-256
at `build/backups/pre-supported-hardware-matrix-20260805-001`.

## 2026-08-05 - V14-12V bounded Aula/SparkPlayJoy 6x21 family

Replaced the exact-PID/exact-firmware admission architecture with an exact
known profile plus a bounded compatible-family profile. The discovery prefilter
now accepts Aula VID `1CA2` or Aula/SparkPlayJoy SetupAPI identity, but opens no
unrelated HID metadata handles. Every candidate must still expose
`FFA0:0001`, exact 65-byte reports and complete a strict exclusive-session
read-only proof before its exact interface path is claimed.

Generalized the 60-byte sync, precision/travel and default-map validators without
weakening framing, checksum, correlation or session-poison rules. Default maps
may contain a unique dynamic set of up to 126 physical positions. Fn0 reads use
up to nine 14-record batches per generation and require two identical complete
generations. Total proof is bounded to 25 transactions; the physically verified
61-position WIN 60 HE MAX remains the exact 17-transaction profile.

Added alternate firmware/precision and 84-position end-to-end fixtures plus
negative duplicate, malformed and exact-profile rejection coverage. Aula Clang
ASan+UBSan passed 6/6; Aula/routing static audits and the complete native suite
passed. Official MSVC Release x64 build passed with zero errors and no unexpected
warnings. New production artifact: 2,164,224 bytes, SHA-256
`2833DA24AF9D086A084B045FCEEA78F08883536FD96F48E1EFEEC938B652E1BB`.

## 2026-08-05 - public GitHub README draft

Reworked the top of the repository README from a build-first engineering page
into a user-facing HallJoy overview. Added a concise feature summary, quick
start, a complete Input Overlay/OBS setup section and an initial keyboard list.
The overlay description is grounded in the production controls and browser
implementation: loopback-only URL, transparent canvas, raw/after-curve depth,
fill direction, label/color/effect controls, 15% default smoothing and retained
idle rendering.

The hardware list explicitly separates three HallJoy physical devices from
models declared by the pinned UAP/Soup runtime and from dynamically proven
protocol families. Explicit UAP names were taken from the vendored runtime
README and decoder; NuPhy/DrunkDeer/Wooting remain family-level entries where
the local runtime does not publish a complete per-model guarantee. This is
intentionally a review draft rather than an inflated physical-validation claim.

Verified backup before the README rewrite:
`build/backups/pre-readme-homepage-20260805-001`. README version/build static
audits and `git diff --check` pass.

## 2026-08-05 - public keyboard list review pass

Reorganized the GitHub compatibility draft into a native HallJoy block, a
separate pinned UAP/Soup block and a Discord/support-request block. Added Irok
MG75 Pro, clarified the tested ATK Hex80 name, recorded SayoDevice O3C as the
tested Sayo model while retaining protocol-proof discovery for sibling devices,
and consolidated all named MADLIONS models into one public row with their actual
native-versus-UAP routing stated explicitly.

Added the fallback behavior and contribution boundary: HallJoy attempts only
known safe protocol families; failed automatic detection should be reported to
Discord user `pash.ok`. Open-source readers, an open SDK/protocol description,
firmware or an offline `.exe` updater/configurator are sufficient starting
evidence for implementation. The README also states that some firmware exposes
no external analogue protocol, in which case HallJoy alone cannot manufacture
one.

Updated `SUPPORTED_HARDWARE.md` to keep the Irok, ATK Hex80 and SayoDevice O3C
claims consistent. Verified backup:
`build/backups/pre-readme-keyboard-list-20260805-001`.

Backup before the architecture change was verified at
`build/backups/pre-aula-family-protocol-20260805-001`. No new physical model is
claimed: untested siblings are only `protocol-compatible` until hardware proof.

The final packaging pass also moved the Russian tester README out of an inline
Windows PowerShell here-string into an explicitly UTF-8-read template. This
removes codepage-dependent mojibake from `build/release/README_FOR_TESTER.txt`
without changing the production binary. The final qualified EXE hash and
`SHA256SUMS.txt` both equal
`2833DA24AF9D086A084B045FCEEA78F08883536FD96F48E1EFEEC938B652E1BB`.

## 2026-08-06 - English GitHub landing README

Replaced the mixed Russian/English repository README with a fully English
GitHub landing page. The rewrite covers the product overview, quick start,
OBS Input Overlay setup and customization, native and embedded-UAP keyboard
lists, Discord support request guidance, source build, runtime architecture,
protocol arbitration, diagnostics, limitations, and contributor workflow.

The detailed Russian release notes and hardware matrix remain linked and are
explicitly labelled as Russian references. Automated validation confirms zero
Cyrillic characters in `README.md`, all local Markdown targets exist, the
version/build documentation audits pass, and `git diff --check` reports no
formatting error. Pre-change backup:
`build/backups/pre-english-github-readme-20260806-001`.

## 2026-08-07 - physical Keychron K4 HE ANSI support

Investigated the newly connected `3434:0E40` keyboard before attempting any
firmware mutation. Windows exposes its analogue endpoint as the expected
vendor-defined `FF60:0061` HID interface with 33-byte input/output reports. The
pinned UAP already implemented Keychron's `A9 01` version and `A9 30` per-key
read-only protocol, but rejected K4 HE because its PID and 6x19 matrix were not
catalogued.

Added an exact K4 HE ANSI identity and a 114-cell/100-key matrix derived from
the official `Keychron/qmk_firmware` `hall_effect_playground` branch at immutable
commit `bc56b3c611dcc1a8ed9a2acb8bdc4da5e1a80c27`. Added matching private-UAP
topology telemetry and a static audit that enforces PID routing, vendor usage,
matrix size, physical-key count and source provenance. Updated the normalized
Soup overlay hash in the dependency lock.

The rebuilt private ABI detected exactly one physical device. A 20-second live
read measured seven active key codes, 202 distinct analogue levels, full-scale
`1.0000`, 567 value transitions, 537 coherent worker publications and clean
bounded unload. The official K4 HE source exposes `A9 30` and does not implement
the optional `A9 31` bulk command, so the compatible per-key route is intentional.
No bootloader entry, firmware write or configuration mutation was performed.

Pre-change backup with matching SHA-256 copies:
`build/backups/pre-keychron-k4he-20260807-001`.

The official release pipeline then exposed two independent stale harness bugs.
Packaging referenced an undefined `$repoRoot` variable after a successful link;
the production smoke and cycle runner still required a stability trace even
though the final target deliberately compiles continuous tracing out. The build
root reference was corrected and regression-guarded. Both runners now enforce
the actual production contract: clean exit, bounded graceful shutdown, no
surviving process, unchanged user state, no continuous diagnostic file and no
crash report.

Final MSVC Release x64 build passed with zero errors and no unexpected warnings.
The linked-image telemetry exclusion passed. Overlay response, framing,
origin/concurrency and 500-case parallel fuzz passed. Three release cycles
passed with 118-217 ms shutdowns, zero logs and all 30 user-state files
unchanged. The UAP DLL extracted from the final EXE independently reported
`3434:0E40`, `FF60:0061`, topology 6x19/114 slots, 236 nominal levels and clean
bounded unload. Final artifact: 2,165,248 bytes, SHA-256
`B8FFE5ACB43DDDDB2C0C9634057E7D5A34771E90F1BF9907B0576E5E4A7762ED`.

## 2026-08-07 - K4 HE stock latency finding and full-report firmware candidate

The user performed the missing gameplay-oriented physical check and found that
the K4 HE analogue value can appear about one second after initial travel, or
only once the key crosses its digital actuation point. This invalidates the
earlier release-support conclusion. The prior run proved value correctness and
multi-key publication but did not measure first-value latency below actuation.

Root cause is the stock-firmware fallback in bundled Soup/UAP. K4 HE implements
only `A9 30`, which returns one matrix position. The worker prioritizes digitally
active and already-moving keys but samples only four new background positions
per update. A first sub-actuation movement can therefore wait for most of the
6x19 sweep. Digital actuation promotes the key and explains the observed sudden
appearance. Public documentation now explicitly excludes stock K4 HE from
release support instead of masking this as a performance limitation.

A firmware candidate was built from the current official Keychron `2025q3`
branch at commit `ee7390c3bbdc1f71a1cc8d54323f3f1d97868593`. The only functional
change adds the established packetized `A9 31` full-matrix read command and its
capability marker; HallJoy's existing fast path consumes four 32-byte reports
covering all 114 matrix slots per sample. The `keychron/k4_he/ansi:keychron`
build completed for STM32F401 with `3434:0E40` and STM32 DFU. Candidate SHA-256:
`FCEBEBD31E5D54A72D3C7C23619878F983606FE390584F2D9317693F40E31AA4`.

The official K4 HE ANSI stock v1.1.1 firmware was downloaded from Keychron and
saved as rollback material; SHA-256:
`4E877497A0EDC1A0D97CD52F5FF9BA86EF7DC84D56969E81DB7DE19EB6151E5F`.
No firmware write occurred before exact DFU enumeration. Physical validation is
still required before K4 HE can return to the supported-hardware list.

## 2026-08-07 - K4 HE full-report flash and Windows receive latency fix

Entered STM32 DFU only after exactly one `0483:DF11` device with serial
`3381347A3035` was present. Before writing, saved a complete 256 KiB internal
flash image as `keychron_k4_he_ansi_preflash_full_3381347A3035.bin`; SHA-256:
`BF53C24706D761EDDF5AF447549627020F1410B5DAF431438FBCAB1FF63AB0A0`.
Flashed the exact `A9 31` candidate and verified successful DFU manifest/leave
and normal re-enumeration as `3434:0E40` with all HID interfaces.

The first private-UAP physical run proved immediate values below digital
actuation (minimum positive `5/235`, 222 positive levels), full travel,
five-of-five simultaneous keys and bounded unload. It also exposed a separate
host bottleneck: a complete four-report snapshot ran at only 65.4 Hz because
Soup polled `GetOverlappedResult(..., FALSE)` with `Sleep(1)` for every report.
Direct hidapi on the same firmware sustained about 172.3 Hz, isolating the
delay to the Windows receive loop rather than firmware or USB descriptors.

Replaced timer polling with the blocking kernel completion form
`GetOverlappedResult(..., TRUE)`. The established Soup handle/OVERLAPPED layout
and cancellation ownership remain unchanged. A new mandatory static audit
prevents reintroduction of `Sleep` polling, and the normalized overlay hash is
pinned in `tools/dependency-lock.json`. The private ABI lifecycle gate detects
one device and unloads cleanly; the full Release build passes all static and
portable tests, MSVC compilation, the production warning allowlist and the
linked-image no-continuous-telemetry check.

On the rebuilt private UAP, a 20-second physical-device idle measurement
recorded 3,458 complete updates at 181.6 Hz, 5,508 us average interval and
7,099 us maximum interval. A subsequent 12-second pressed run sustained
191.1 Hz with a 5,233 us average and 7,011 us maximum interval. It observed
20 physical keys, 231 distinct positive levels, minimum `5/235`, full-scale
1.0 and 2,916 value transitions. A separate three-second idle run confirmed
zero active keys for the complete final second at 185-187 Hz and clean bounded
unload. This closes the 65 Hz host-performance and release-to-zero gates.

USB disconnect/reconnect remains the final physical gate before public support
status changes; the firmware rollback images remain retained.

## 2026-08-08: AULA W669 / WIN60 HE Standard adaptive diagnostic

Re-audited the supplied W669 V3.17.08 firmware and the official AULA WebHID
driver from first principles. Corrected the earlier live-packet
reconstruction: both scanner implementations emit analogue subtype `01`;
their declared lengths are `03` and `05`. Subtype `05` is a per-key trigger
configuration response, not a second live format. Also removed the unsupported
inference that descriptor byte `08` itself identifies high-precision units.

Added a separate `AulaW669` native backend rather than extending the physically
proved WIN60 HE MAX transport. Admission is interface-path scoped and requires
the exact `FF1B:0091` 64-byte HID shape plus two independent read-only protocol
proofs: `21/04` travel range and the complete ten-fragment `18/80` 132-position
map. The corrected backend uses the official SI2825 factory layout only for a
confirmed `WIN 60 HE` identity and overlays explicit `18/80` remaps; unknown
siblings must prove their own explicit map instead of inheriting WIN60
geometry. It tries shared/exclusive sessions and WriteFile/HidD_SetOutputReport
transports, subscribes through the RAM-only `21/02` mask, and clears it with
`21/03`.

The single-EXE diagnostic logs every W669 TX/RX report, HID identity/caps,
configured 1/2/4/8 kHz polling-rate query `21/0A`, live event
frequency/intervals, active
keys and publication counts to `HallJoy.log`. A one-shot `21/0E` matrix
snapshot independently checks the stream; after three quiet seconds the
diagnostic build retries bounded snapshots every two seconds without invoking
calibration, reset or persistent configuration commands. The isolated UAP
host uses parent-side shared telemetry in this profile and no longer creates a
second `.log` file.

Protocol tests, truncation/random-input sanitizer coverage and the complete
native-backend gate pass. Optimized single-log diagnostic artifact:
`build/aula-w669-diagnostic/HallJoy.exe`, 2,249,216 bytes, SHA-256
`DF91FB6D9487235A43ACF84B3FA56D47A41D562112B1E9E2CC820D2D65CDE6BA`.
Local no-W669 smoke exited cleanly with code zero and produced `HallJoy.log`;
physical W669 analogue correctness remains intentionally unclaimed until the
user returns that log.

## 2026-08-08: W669 first physical log and factory-map root correction

The returned `HallJoy.log` (SHA-256
`455BD1BE26976F53CEC0AE287D2FDE8CE9C28B9078E5C52CFB696349A1793524`)
proved the exact `2E3C:C365`, `WIN 60 HE`, `FF1B:0091`, 64-byte interface and
the `21/04` descriptor (`maximum=340`). Both shared and exclusive WriteFile
routes received all ten `18/80` fragments, but the first diagnostic rejected
them as `map_failed` before subscription. The process itself then shut down
normally with exit code zero.

Root cause: the implementation treated an all-zero four-byte map record as an
empty matrix position. The real device uses zero to inherit the factory layer;
its only explicit record was `01 FA` at logical position 122, exactly the Fn
position in the official SI2825 layout. Consequently the first build erased 60
valid factory keys and its `>=20 mapped` safety gate correctly stopped the
session, but for the wrong decoded premise.

The corrected protocol keeps the official 61-position SI2825 factory map for
the confirmed `WIN 60 HE` product, then overlays non-zero `18/80` records.
Unknown products do not inherit WIN60 geometry and must still prove enough
explicit entries. A regression fixture reproduces all ten physical fragments,
including the position-122 Fn record. Corrected live-subscription and timing
remain pending one physical rerun. The corrected optimized single-log artifact
is `build/aula-w669-diagnostic/HallJoy.exe`, 2,250,240 bytes, SHA-256
`D3212F8F3C419D9FE5BA103B3D313B2F7EC325B64DB13A2D92439CC7CF07B54B`.

The follow-up full Ghidra export covered 717 discovered functions and traced
the previously unexercised live path end to end. Handler `0x080118C2` copies
the `21/02` request mask directly to RAM `0x2000E878`. The normal producer's
literal at `0x08018558` and alternate producer's literal at `0x08018E94` both
resolve to the same address. Their constructors (`0x080180E2` and
`0x08018BB4`) check `mask[column] & (1 << row)` without a digital-actuation
condition and queue subtype `01` with declared length 3 or 5, row, column and
little-endian processed travel. HallJoy's request and parser match these exact
wire fields. No missing start command or second enable flag exists in this
path; remaining physical validation is Windows delivery, real event timing and
release-to-zero behavior rather than an unresolved firmware command.

## 2026-08-09: W669 live proof and removal of the corrupting snapshot path

Analyzed the returned `HallJoy (2).log` (SHA-256
`1B1934BDC281F7CA83A7273EFC89B0A706623E7574F111FF73F4B0FBF9273955`)
at packet level instead of trusting the diagnostic rollups. The corrected
factory map and firmware subscription work: the trace contains 3,950 valid
subtype-`01` events across 21 positions, 325 distinct positive levels, maximum
340 and 204 explicit zero releases. Reconstructing state from live packets
alone ends all 21 positions at zero.

The apparent stuck state was created inside HallJoy. `21/0E` returned idle
sensor-domain values around `0x0Axx`, which the diagnostic incorrectly divided
by the processed maximum 340 and clamped to full travel. Each requested
132-packet burst retained only 64 packets, and the synchronous snapshot reader
also intercepted and discarded 532 valid live events. It therefore both
created 14 false active keys and lost real release updates.

Removed snapshot collection and publication from startup and quiet-stream
recovery. The event-driven subtype-`01` stream now has one receive owner and an
idle keyboard triggers no extra request. Corrected overlapped timeout handling
so `CancelAndDrain`'s terminal `ERROR_OPERATION_ABORTED` does not replace the
original `WAIT_TIMEOUT` and inflate transport failures. The physical polling
query's code zero is accepted as firmware-default with unspecified nominal
rate rather than logged as a response timeout.

The complete portable/static native-backend gate and optimized MSVC diagnostic
build pass. New single-EXE artifact:
`build/aula-w669-diagnostic/HallJoy.exe`, 2,313,728 bytes, SHA-256
`D640F7D4C497DED890FE7C45CE7188CA883137A9737FF9A080694A04423C3A1F`.
A physical rerun remains required to close the corrected host-state gate.

## 2026-08-09: W669 reported stalls, diagnostic ownership and discovery correction

Analyzed `HallJoy (4).log` (SHA-256
`C24DD91D26EF4056202DCA659E168E43DADFF8AA9BF0FB8A2A5C320BBA39E28A`).
The 64 MiB file contains only 35,120 meaningful bytes/175 lines and ends
abruptly at 32.984 seconds without `session.end`. The structured timeline has
no process-wide event gap above 1.1 seconds, but it cannot measure the reported
W669 stalls: `StabilityTrace` and `DebugLog` both opened `HallJoy.log` with
`CREATE_ALWAYS`, so the mapped trace won ownership and discarded all W669 raw
and interval telemetry from the asynchronous logger.

The surviving evidence exposed a separate runtime interference source. With
no MAX-family device present, `aula-win60he` performed 36 full SetupAPI/HID
enumerations in 33 seconds, each walking 21-22 interfaces and taking roughly
8-20 ms. This happened while the W669 backend was already active. Absent MAX
discovery is now event-driven: after the initial scan it waits indefinitely
for the existing `WM_DEVICECHANGE` wake; timed retries remain only when an
exact candidate exists but its admission/open is transiently incomplete.

The diagnostic logging architecture now has one file owner. `StabilityTrace`
owns the bounded mapped `HallJoy.log`; the asynchronous `DebugLog` writer
sanitizes and appends its queued lines to that same sink. MAD68 diagnostics are
routed into it instead of creating `HallJoyMAD68ProR.log`. The build script now
explicitly enables `HallJoyDiagnostic` and `HallJoySingleLogDiagnostic` and
fails packaging unless W669 raw/telemetry and MAD markers are linked.

A local lifecycle run then found an independent shutdown stall after a valid
`session.end`: synchronous `FlushViewOfFile`/`FlushFileBuffers` could exceed
the 12-second shutdown watchdog. Clean mapped-log shutdown now unmaps,
truncates and closes through the Windows cache manager without blocking disk
flushes. Normal exit no longer creates `HallJoyDiagnosticExit.txt`; crash-only
sidecars remain available for abnormal termination.

Final local eight-second smoke: exit code 0, close latency 75 ms, all workers
joined, one 38,703-byte `HallJoy.log`, zero diagnostic/MAD sidecars, inline
`log.init` and MAD evidence, and no one-second absent-MAX enumeration train.
All native/static gates and optimized MSVC build pass. Corrected artifact:
`build/aula-w669-diagnostic/HallJoy.exe`, 2,318,848 bytes, SHA-256
`6931EA0AA3B32205F0AEA395C90C7081C3A4F9A1606CE1C1A74583D00FDB377E`.
The reported physical W669 stall still requires one run of this observable,
non-interfering build before release support can be closed.

## 2026-08-09: Configuration live graph retained-layer correction

The selected-key graph appeared to update only after clicking HallJoy. The
backend values, selected HID, 1 ms default UI timer and graph-region
invalidation were all active; focus was not the trigger. The real fault was a
nested-cache ownership violation: `Config_RenderCacheContent` captured the
live marker inside Configuration's retained full-content bitmap. A telemetry
repaint therefore copied the old marker, while an unrelated click happened to
dirty and rebuild the page cache.

The graph now has an explicit retained-content phase and a viewport-overlay
phase. The stable plot/curve remains cached. The current analog marker is
composed after `CustomPageSurface_Present`, followed by the handles so their
original z-order is unchanged. Analog changes still invalidate only the graph
rectangle and never rebuild the full page. The legacy non-retained renderer
uses both phases and therefore preserves its behavior.

`pre_release_ui_static_audit.py` now prevents the live marker from re-entering
the retained callback, requires post-present composition, preserves
marker-before-handles ordering and forbids full-cache dirtying from the live
timer. Full static and portable native checks pass. Release x64 MSVC build
passes with only the allow-listed external ViGEm missing-PDB warning. Unified
UI stress passes 6/6 pages at 120 wheel events each; Configuration measured
63.0 update cycles/s, GDI handles were 109/109/106 and USER handles
194/195/194 (start/max/end), and shutdown returned zero. Evidence from the
official packaged executable:
`build/evidence/ui-scroll-stress/20260809-172117`; tested EXE SHA-256
`EF018D57768C5E0249E9B33D53C8A5D4396EEEAA4887B6790F7F9488337E2BBC`.
Owner visual confirmation of continuous physical-key motion remains pending.

## 2026-08-09: AULA Standard/W669 multi-geometry family coverage

Re-audited both Aula protocol families rather than treating the shared brand or
PID as a layout identity. The MAX `5C/12/23/2B` family was already dynamic
inside its proved 6x21 matrix: it derives the physical-key count from the
device's default map and reads `ceil(keys/14)` active-map batches. The remaining
coverage gap was Standard/W669, where all-zero `18/80` records mean “inherit
factory layout” and therefore cannot describe a previously unknown geometry.

The current official Standard web driver catalog lists seven products on
`2E3C:C365`. Its code sends read-only opcode `0D`, parses the fifth CSV field as
the firmware product, then loads `config/keys/<product>.json`. Audited all seven
live official files. Four SI2825 products share the byte-identical 61-key map
SHA-256 `04E2FDA00FDB1645C74D42121102C1CF233658DDF43FB2611ACE57460CFCB448`;
two SI2828 products share the 68-key map
`CC2CBBC9C051230279BA8CE3B52054739446DC19C78CF1A38ADFD2D7C6CDF9E1`;
SI2851/KP-TE153 UK has a distinct 69-key map
`FCB98F5DF82C2E00D94501389C452172C4FAB6F103DF3CBF489FFFE9B89945C3`.

HallJoy now performs the same offline bounded classification. Every proof and
reconnect queries `0D`; exact SI2825/SI2828/SI2851 product IDs select their own
factory map before the complete `18/80` override generation is applied. An
exact HID marketing-name fallback exists only when the identity command is
unavailable. Unknown firmware products start from an empty map and must prove
enough explicit assignments; they never inherit WIN60/68 geometry by PID,
substring, or key count. A proved firmware identity must remain byte-identical
when the streaming session is reopened.

Added parser rejection/fuzz coverage, all seven product aliases, exact
61/68/69-key map fixtures, session-identity and no-guessed-layout static guards.
`python tools/run_native_backend_checks.py --require-compiler` passes. The full
official build passes all required gates and MSVC x64 release linking with only
the allow-listed external ViGEm missing-PDB warning. Production artifact:
`build/release/HallJoy.exe`, SHA-256
`0BD04CBF5FDA26B9B1A5C7BBBCFB40E357A5CF51A3EB8534816D354791F39A14`.
WIN68 and KP-TE153 remain official-driver-derived/implementation-tested, not
physically validated on their own hardware.

## 2026-08-09: public README contract and Keychron K4 HE layout preset

Reworked the public English README around user-visible behavior. The feature
introduction now describes low latency and last-key priority without exposing
the internal analogue/curve/conflict/ViGEm pipeline. The keyboard support table
uses Aula, Irok/SparkLink and protocol-compatible-brand wording, explicitly
records the physically tested unsupported Irok MG75 v2, spells out Universal
Analog Plugin, and moves Input Overlay below hardware compatibility. Removed the
claim that physical hardware is required for an initial implementation, added a
high-risk custom-firmware warning, and placed an advanced-reader boundary before
the source-build sections.

Re-audited dependency behavior before changing Quick start. HallJoy does not
auto-install ViGEmBus: V14-12F/S18 intentionally replaced downloader/elevation
code with exact pinned ViGEmBus 1.22.0 manual guidance. A system Wooting Analog
SDK has not been required since the private embedded ABI1 runtime decision on
2026-07-31; the verified runtime is prepared automatically without UAC. README
now states both boundaries exactly.

Added `Keychron K4 HE` as a separate 100-key built-in layout using the exact
geometry of the physically configured local K4 HE layout. It is the third
built-in and cannot replace preset zero. Fixed built-in discovery for existing
users: built-ins are registered first, user files then override same-name
presets, and only missing preset files are created. This preserves edited files
while allowing newly shipped layouts to appear in a non-empty `Layouts` folder.

Validation: exact 100/100 geometry comparison PASS; persistence static audit
PASS with built-in merge/default guards; version-identity audit updated to keep
the product heading version-independent while requiring the current v1.4 release
record; `python tools/run_native_backend_checks.py --require-compiler` PASS;
MSVC `Release|x64` validation build PASS with only the allow-listed external
ViGEm missing-PDB warning. Isolated artifact:
`build/validation-keychron/HallJoy.exe`, SHA-256
`5481699CC583156904AEFB7DE3950CFC01B245D3CD2029B82F512CF507A28499`.

## 2026-08-09: Git author-name canonicalization

Rewrote commit metadata so the obsolete author/committer spelling is
canonicalized to `PashOK7`. Email addresses, author/committer dates, messages and
file trees were retained. The operation ran in a bundle-backed mirror because
the active integration worktree contained 99 modified/untracked paths. A full
workspace copy plus verified before/after all-ref bundles are stored under
`C:\github\HallJoy_v1.4_PRE_PASHA_REWRITE_20260809-212131`.

The public update used one atomic force-with-lease transaction for `main`, the
legacy Codex branch and tags v1.0-v1.3. Public `main` changed from
`2467cb75e3e2c124c29a57000e47a85892d20a41` to
`1abf87fea235bf655de8fbec8cce8a9a928f63dd`; its tree remained exactly
`c3cf7d18adb37dd9a1c597af7ee83dcf3bd95802`. Local status and binary-diff hashes
matched before/after ref replacement, and the old sample object was absent after
local reflog expiry and pruning. Reachable local history now contains 0 author
and 0 committer occurrences of the obsolete spelling.

GitHub still serves old commits by direct SHA and PR #1 retains the read-only
old `refs/pull/1/head`; the repository has zero forks. Current branches/tags and
new patch views are canonical, but complete GitHub cache/PR-ref expungement
requires a GitHub Support request. First changed commit:
`24c7398a647ff7d5067c715b71ac6696db01a56c`; rewritten root:
`74b81c9fd62af9b1b274607ea0959a6a7dfb1285`; affected pull requests: 1.

## 2026-08-09: corrected built-in numpad tall-key height

The first investigation incorrectly treated the user's manually validated 87 px
Keychron height as corruption because the historical Generic preset used 88 px.
The resulting revision-1 migration expanded both tall keys to 88. A Reset then
reloaded those persisted values and visual evidence showed `Num+` and numpad
`Enter` ending one rendered pixel too low. That hypothesis and migration were
wrong and are superseded by this correction.

Both `Generic 100% ANSI` and `Keychron K4 HE` now define HID usages 87/88 at the
visually validated height of 87 px. Because Reset reloads the persisted preset,
`BuiltinGeometryRevision=2` narrowly converts the old 88 px value to 87 for only
those two usages and names. It also supersedes files marked by bad revision 1,
never expands an existing 87 px key, and preserves every unrelated layout edit.
Both local presets were repaired. Pre-correction source, presets and release exe
are backed up under `backups/tall-key-correction-20260809-215235`.

Validation: four exact 87 px source guards PASS; migration direction/scope and
obsolete 87-to-88 rejection guards PASS; complete native backend and production
packaging gates PASS; optimized MSVC x64 build has zero errors and only the
allow-listed ViGEm missing-PDB warning. Continuous telemetry is absent and
crash-only reporting is retained. A production startup/WM_CLOSE smoke exited 0;
both persisted presets finished at revision 2 with exact 87/87 tall-key values.
Production artifact:
`build/release/HallJoy.exe`, SHA-256
`CF3FE3A87DC913B986A8837E59084154126438B4EC9DEF4C3815F64C95BCDEB9`.

## 2026-08-09: restored current user-facing README content

Compared public `main` README blob
`b4f711f9f4dd055ba50e790eef5aad2838f89635` with the current local README and
restored useful content that had been lost during the technical rewrite. The
README now exposes the working video overview, project origin, Windows x64
requirements, four-pad limit, Snap Stick, Block Bound Keys, layout-editor and
shareable-preset capabilities, current run flow, saved-data/portable-mode paths,
current troubleshooting, and AGPL/commercial licensing plus third-party notices.

Obsolete system Wooting SDK/UAP installation, plugin-folder cleanup, SDK rollback,
old Aula limitations, and legacy Visual Studio-only build guidance remain absent.
Claims were checked against bindings/runtime/UI/storage code; the YouTube target
returned HTTP 200; every relative Markdown link resolves; README content/static
checks and `git diff --check` pass. Backup:
`backups/readme-restoration-20260809-221133`.

## 2026-08-09: project-wide English documentation migration

Removed the obsolete README sentence that linked readers to Russian-only
release and hardware documents. Restored the author's original personal
paragraph verbatim, including its emoji.

Created the first complete English migration of every project-maintained
document and source comment. The
46 documentation files whose names ended in `_RU` were renamed to neutral
English names, all internal references were updated, and the two HallJoy-owned
notes inside the pinned Universal Analog Plugin tree were translated and
renamed as well. The public `RELEASE_NOTES_v1.4.md` and
`SUPPORTED_HARDWARE.md` were rewritten as reviewed English documents rather
than retaining raw machine-translated copy.

The pre-migration files are preserved under
`backups/english-migration-20260809-223645`; the local `backups/` tree is
explicitly ignored by Git so historical Russian copies cannot enter a release.
Validation found no Cyrillic in maintained documentation or source comments,
no `_RU.md`/`_RU.txt` names or stale references, no broken project-relative
Markdown links, and no inconsistent Markdown tables. Vendored Soup language
dictionaries remain intact because they are localization data, not HallJoy
documentation. The complete compiler-backed native backend gate,
`s20_build_docs_static_audit.py`, and `git diff --check` pass.

## 2026-08-09: editorial review of the English documentation

The first migration draft was treated as an intermediate artifact, not as
publication-ready prose. Every document that had contained Russian narrative
text was compared with its pre-migration source and rewritten in idiomatic
technical English. The review covered the public release and hardware pages,
current protocol investigations, archived historical notes, stability plans,
risk and decision records, stage results, tester instructions, firmware notes,
and the HallJoy-owned Universal Analog Plugin documentation.

Long historical records were condensed where repetition obscured the result,
but their decisions, risk identifiers, evidence boundaries, commands, and
unresolved hardware requirements were preserved. Existing English validation
logs and machine-generated manifests were not paraphrased: they remain primary
evidence rather than editorial prose. The mixed-language v1.4 index was repaired,
and the official build guide now states the exact Windows x64 and unsupported
Win32/x86 contract required by the build audit.

Post-review validation found no Cyrillic in maintained documentation or source
comments and no mojibake in UTF-8 documentation. It also found no `_RU`
filenames or stale paths, malformed headings, unclosed code fences, inconsistent
Markdown tables, or broken HallJoy-relative links.
`s20_build_docs_static_audit.py`, `version_identity_static_audit.py`,
`protocol_family_routing_static_audit.py`, and the complete compiler-backed
`run_native_backend_checks.py --require-compiler` gate pass.

## 2026-08-10: Keychron K4 HE support boundary and final publication build

Corrected the public K4 HE status after the owner confirmed that the custom
read-only `A9 31` full-report firmware has been used as a daily HallJoy keyboard
for an extended period with immediate analogue response and no observed
stability, release, reconnect, or gameplay regressions. Public support now
clearly applies to that custom firmware. Stock K4 firmware remains unsupported
for gaming because its one-key-at-a-time `A9 30` path can delay a previously
idle key by roughly one second.

Added `https://analogsense.org/firmware/` as a background and flashing-guide
reference. The page returned HTTP 200 and describes patched full analogue
reports, but its current pre-built image list contains Q1 HE, Q3 HE, Q5 HE,
K2 HE, and Lemokey P1 HE—not K4 HE. Documentation therefore warns users not to
flash another model's image and does not describe the page as a K4 download.

Rebuilt the complete current tree with `BUILD.cmd`. The full static and portable
C++20 gate passed, MSVC `Release|x64` completed with zero errors and only the
allow-listed external ViGEm missing-PDB `LNK4099`, and the linked-image audit
found no continuous telemetry markers while retaining `HallJoyCrash.txt`.
Production startup/shutdown smoke passed without a continuous or crash log.
The release directory was rebuilt after smoke so it contains only the intended
four files. Final EXE: 2,187,776 bytes, SHA-256
`C03CB7A19DB73921D69904A7ABDAB954D54F3D5CE42899ADD9C24012D93D0402`.
