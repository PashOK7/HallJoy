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
