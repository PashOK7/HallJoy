# HallJoy v1.4 worklog

## 2026-09-06 — RM-04 SparkLink per-row stale neutralization

Fixed the confirmed partial-snapshot stale-value path. SparkLink now retains
row-local normalized HID values and publishes an aggregate only from active,
in-limit, fresh rows. Expiry, RowLimit reduction and reset recompute only HIDs
owned by the affected row; duplicate HID ownership keeps a fresh row's value.
The 2160 ms row deadline is derived from the bounded worst safe-mode round.
The old-bug portable oracle and static audit passed, as did the unified
static/portable suite. The dormant burst path remains disabled. No HallJoy EXE,
HID, keyboard injection or gamepad output ran. Backup:
`.local/backups/rm04_spark_row_freshness_20260906_145000`.

## 2026-09-06 — RM-03 Sayo automatic-letter ambiguity

Fixed a verified cross-report association defect without changing the automatic
letter UX. The former single global pending physical index was overwritten by a
second down edge, allowing a later one-letter keyboard report for A to learn B.
Production now uses a mutex-protected per-index `SayoLetterMatcher`: it maps
only one live candidate inside the 80 ms window, preserves mappings under
ambiguity, and retries automatically after release/timeout/reset. The pure test
executes that production header and covers the old bug, reverse order, split and
combined reports, auto-repeat, held neighbour, duplicate letter, timeout and
reconnect reset. Full static plus portable compiler suite passed. No HallJoy EXE,
HID, keyboard injection or gamepad output was run. Backup:
`.local/backups/rm03_sayo_matcher_20260906_143000`.

## 2026-09-06 — RM-01 boundary evidence and ROG containment

Strengthened the file-only profile boundary with a simulator-only attempted
`Backend_Init` counter. Both app test branches fail if it is nonzero, and the
production-linked transaction test emits `backend_init_attempts=0`. The existing
ViGEm transport already uses an injected fake API with local call records;
there is no repository `SendInput` path. A broad counter around unrelated HID
opens was deliberately not added because it would not prove all protocol-owned
opens. The new source-only ROG Azoth 96 HE diagnostic target was also corrected
to stay transport-only: no UAP, ViGEm, output recovery, or Raw Input keyboard
registration. ROG and file-only static audits, then the full static native
backend suite, passed. The paired ROG interface claim is additionally rolled
back on partial admission, so UAP cannot be left excluding one unusable
interface. No EXE was built or launched. Backup:
`.local/backups/rm01_documentation_20260906_141500`.

## 2026-09-06 — RM-01 side-effect boundary (partial)

Created [the runner side-effect registry](RUNNER_SIDE_EFFECT_REGISTRY_2026-09-06.md)
for all 19 `run_*.ps1` scripts. All normal/simulator/ViGEm routes remain
output-capable and were not run. The two file-only profile invocations now pass
`--halljoy-test-forbid-backend-init`; `Backend_Init` rejects it before any
ViGEm stop, HID discovery, native worker lifecycle, or virtual-controller
activity. Six structural wiring checks passed. No EXE was built or launched.
The card remains partial until a fake transport exposes zero-valued
CreateTarget/SendInput/HID-open counters. Backup:
`.local/backups/rm01_file_only_guard_20260906_134455`.

## 2026-09-06 — RM-00 current-state baseline

Completed the documentation-only baseline in
[CURRENT_STATE_INDEX_2026-09-06.md](CURRENT_STATE_INDEX_2026-09-06.md). It
records the current ordinary release SHA-256 `A4D8…E37` separately from the
historical `44CF…E012`, locks the observed toolchain/dependency identity, and
reconciles every historical P0/P1 to an owning RM. It also states explicitly
that the new ROG Azoth 96 HE diagnostic source is unbuilt and absent from that
release artifact. Backup verified at `.local/backups/rm00_baseline_20260906_133258`.
No build, application, HID, keyboard-injection, or gamepad-output test ran.
Next: RM-01 runner side-effect registry.

## 2026-09-06 — Completed filesystem restructuring (owner authorized)

One ordinary package: build/release. HallJoy variants/configurations compile into
build/bin and build/obj; plugin work copies and portable tests also stay in build.
Soup/Sun caches moved to .cache. 16 documents relocated with redirects and a
current layout/index. Old root/source/build debris preserved, not deleted:
2358 archived/evidence files (1677.96 MiB), 1502 dependency/runtime files (104.38 MiB).
Full 4712-file baseline and all move hashes: .local/backups/structure_20260906_122303.
Static-library trap resolved: wooting_analog_common.lib/.a are required pragma-linked
inputs and remain in third_party. No runtime algorithm edits.

Official and Aula diagnostic clean builds exited 0; 80 static audits, 49 portable
C++ checks, ABI/resource checks passed; 28 configuration paths do not overlap.
227 primary source files match baseline. Layout checker: 37 docs, 75 local links.
Only baseline LNK4099; no gameplay input tests. Release SHA256:
A4D8B0C52D826B7ED723819DD441A7DDD05501D84AEAF647541F0F6DDCEE6E37.
[Result and limits](../validation/STRUCTURE_MIGRATION_2026-09-06.md);
[current locations](../current/PROJECT_LAYOUT.md). This closes the bounded
structural package, not all RM-32, architecture or device qualification work.


## 2026-09-06 — Structure review and previously missed Sayo design notes

See [documentation index](../README.md) and
[file-structure review / FS-00..05](../PROJECT_STRUCTURE_REVIEW_2026-09-06.md).
The tree had 4707 files / 1840.57 MiB at measurement; 927 generated intermediates
in three source-local x64 roots account for 1130.35 MiB, subject to rebuild/owner
checks before cleanup. No source/output files were deleted or moved.
Important RM-03 input: [SAYO_DEVICE_NOTES.md](../../src/HallJoyProject/SAYO_DEVICE_NOTES.md)
explicitly documented automatic user-letter matching on 2026-05-14. The earlier
audit missed this source-adjacent document. Read it before proposing Sayo changes;
its capture paths are placeholders, not newly verified raw evidence. FS tasks
feed RM-00/32/36; Discord RM-37 remains the final feature card in this Roadmap.


## 2026-09-06 — Disputed behavior: ask the owner; Discord feature planned

If intended behavior is disputed, unclear or contradicted by documents, ask the
owner what HallJoy should do in that concrete scenario before implementing the
contested change. Continue independent work; record the answer and do not re-ask
already resolved decisions. Routine implementation choices remain autonomous.
FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md v1.2 adds RM-37 at the very end:
unsupported-keyboard status with a Discord community invitation, plus a permanent
Discord link in the UI. Total: 37 audit cards + 1 feature card. Official community
invite must be obtained from the owner before shipping buttons; no invite was
found in the inspected support docs. This is documentation only, not implemented UI.


## 2026-09-06 — Owner clarification: preserve automatic Sayo letter mapping

The owner requires automatic user-configured letters with no manual assignment,
setup wizard or mandatory confirmation, and reports no complaints about current
behavior. This supersedes earlier blanket prohibitions on Sayo letter learning:
physical depth remains measured independently; automatic letter association is
intentional. RM-03 now preserves it and tests ambiguous correlation (addedCount==1
already guards multiple new HID keys in one report; cross-report candidates need
review). No per-press activation-threshold delay was established. Treat stale-depth
fallback separately according to its intended digital/analog contract. Do not
remove Sayo, replace letters with physical-only controls, or label learning itself
P0. Reconcile old HJ-V14-P0-003 subclaims rather than copying its blanket verdict.
See FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md v1.1, section 0, for other potential
intent traps: fallback/shadow removal, single-instance, async save, process
isolation and release scope. Proposed rewrites require Purpose/compatibility
review and evidence; product behavior changes require an explicit product decision.
Only documentation changed in this clarification; no new runtime PASS is claimed.


## 2026-09-06 — Full audit execution roadmap (planning, not qualification)

New detailed execution entry point:
[FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md](FULL_AUDIT_EXECUTION_ROADMAP_2026-09-06.md).
It contains 37 packages / 185 steps, 17 protocol-family review cards, dependencies,
negative checks and acceptance criteria, plus 234-file inventory with review depth
and hashes. All implementation tasks start TODO; historical P0/P1 and release gates
remain authoritative until reconciled with current source/evidence. Current reads
confirm Sayo digital-derived mapping and the SparkLink row-freshness gap; first
packages after RM-00/01 are RM-03/04/05. Native contract, containment and host DLL
trust also require dedicated work. Production files and EXE were not changed;
no runtime/input/controller/hardware tests ran for this audit. A comprehensive
roadmap is not a claim of a completed line-by-line or hardware audit.


## 2026-09-06 — Architecture review (analysis only)

See [ARCHITECTURE_REVIEW_2026-09-06.md](ARCHITECTURE_REVIEW_2026-09-06.md).
Eight architectural work areas are grounded in current code: output freshness,
immutable configuration, extended-key cost, V2 migration completion, UI/disk and
supervision ownership, module boundaries, release catalog, and qualification
provenance. First proposed package: producer-progress lease and independent
neutralization, tested with fake transport. No production code, artifacts or
settings changed; no input/controller tests ran. These are proposals and source
review risks, not new runtime PASS claims. The 2026-09-05 EXE is unchanged.


## 2026-09-05 — Profile audit remediation (D-079)

The eight findings F-01–F-08 in INDEPENDENT_CODE_AUDIT_2026-09-05.md are
implemented in source: bounded layout parsing; prepare-before-switch and guarded
active deletion; strict bindings validation without mutation on failure; complete
1033-key CSV parsing; checked numeric conversion; staged settings/bindings commit
behind a nonblocking realtime reader gate; and one atomic settings+bindings INI.
Legacy pairs remain readable. First bundled save preserves the old settings as
`.pre-bundle.bak` and leaves the old bindings file untouched. Older executables
require restoring the matching legacy pair; see README storage instructions.

A further startup/shutdown defect was found during validation: final shutdown
saved partial defaults after a rejected profile load. Autosave is now enabled
only after successful initialization. The file-only rejected-startup probe
checks exit code 1 and byte-identical settings and bindings after final shutdown.

Evidence: production-linked file/memory tests pass layout bounds, malformed
profiles, full CSV domain, legacy migration, switch/delete failure, 100 concurrent
profile loads with zero mixed reads, and all five atomic-save failure stages.
The test runner is `tools/run_profile_transaction_tests.ps1`; its explicit modes
exit before Backend_Init and never produce gamepad reports. The unified native
checks and ordinary MSVC build also pass; final artifact identity is recorded in
the remediation evidence directory and handoff after packaging.

Limits: the earlier storage migration simulator passed migration, replay and five
migration failure stages, but its portable-mode run exited on rejected existing
profile data. That full runtime suite is NOT qualified by this change. Synthetic
WASD mixed with live hardware first exposed an oracle contamination issue; the
optional simulator-only isolated input mode addresses its source selection, but
still emits gamepad reports. User is playing CS: do not run input/controller
simulation (including this isolated mode). Hardware/ViGEm validation and the full
portable runtime rerun remain pending. A separate fresh, marker-selected portable
file-only startup probe passed (exit 0 before backend/window creation); its log is
included in the remediation evidence. Existing protocol/release blockers remain
open. The root-level HallJoy.exe is not the new build.

Backups: `.analysis/backups/profile_audit_fixes_20260905_175549` (sources/docs)
and `.analysis/backups/profile_fixes_artifacts_20260905_181448` (build artifacts).

## 2026-09-05 - Exact ordinary-release artifact qualification

### Completed

- Rebuilt the ordinary package through `BUILD.cmd` only after backing up the
  previous output folders and confirming HallJoy was closed.
- Verified byte-identical output and release executables at SHA-256
  `1DD724B6E2CFC47CD5CBA152AE59B9FC2926EE1A70EA2F65D04047B7F57C5961`.
- Passed the hash-bound UAP Provider V2 dual-capture smoke, production overlay
  smoke (including 2,000 fuzz cases), and 25/25 isolated normal lifecycle
  cycles without modifying the 110-file user state snapshot.
- Built and ran the dedicated Provider V2 qualification artifact. It confirmed
  the authoritative V2 route, the legacy-shadow non-submission contract, and
  correctly rejected an idle no-device run as `INCOMPLETE` rather than a false
  analogue success.

### Still required before publication

- Make the Hero84 HE scope decision; run the representative physical keyboard
  regression; run the one-hour soak; then establish version/release notes and
  Git provenance. These remain explicit release gates rather than being
  inferred from automated success.

## 2026-09-05 - Release evidence alignment for Provider V2 promotion

### Completed

- Corrected the Provider V2 qualification report and its build/smoke audits to
  state the actual current route: a validated Provider V2 snapshot is the
  authoritative production source, while the legacy dense projection is the
  same-transaction comparison shadow.
- Preserved the deliberate compatibility fallback when the V2 plane is absent;
  it is not counted as V2 qualification evidence.
- Updated the release-readiness gate from implementation-in-progress to code
  complete with fresh exact-artifact and physical qualification still pending.

### Next

- Run the updated exact-artifact Provider V2 qualification and ordinary release
  lifecycle gates after every currently running HallJoy instance is closed.

## 2026-09-05 - Simulator storage-test automation

### Completed

- Kept the production storage-error dialog intact, but suppressed it for the
  explicit simulator-only temporary-root argument used by the migration test.
  Deliberate migration-failure cases now report through their exit code and
  stability trace without interrupting the desktop user.
- Recorded that the first storage-test attempt exercised an outdated simulator
  image; the simulator must be rebuilt before this test is treated as current
  evidence.
- Shortened the test-only temporary root after measuring that its former path
  plus the deliberately long atomic-write suffix was 268 characters. The test
  now exercises the same nested and Unicode migration data below a bounded
  `%TEMP%` root rather than manufacturing a legacy `MAX_PATH` failure.
- Made the simulator runner distinguish its intentional storage-startup failure
  (exit 1 before a main window exists) from an unexpected missing `WM_CLOSE`
  target. The normal path still requires graceful-window shutdown.
- Raised the simulator and migration test minimum duration from seven to eight
  seconds. Seven seconds could preempt the scripted final phase during normal
  startup scheduling and produce a false-red trace check.
- Made `run_analog_simulator.ps1 -UsePortableStorage` create and remove its own
  temporary portable marker. It can no longer silently fall through to the
  developer's real `%LOCALAPPDATA%` state when invoked independently.
- Rebuilt the simulator and passed the complete storage-migration suite after
  these corrections. The full required native backend source/C++ gate also
  passed again.

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

## 2026-08-10: GitHub v1.4.0 publication

Backed up the complete working tree outside the repository together with a Git
bundle of every reachable ref. Audited all 64 untracked files before staging;
no executable, DLL, object, log, firmware, archive, credential, build, or backup
path entered the 189-file release commit. Staged whitespace and forbidden-path
checks passed.

Published integration commit `723eb6a10ee1d9814df9d5e158d599a9ce439f3c`
to `v1.4-integration` and opened PR
[#3](https://github.com/PashOK7/HallJoy/pull/3). Both push and PR workflow runs
passed their portable and Windows Release jobs. The PR was merged normally into
`main` as `b63b3b209bf0cbf1736810e5d04489568e6cb79c`; no force push or check bypass
was used.

Created annotated tag `v1.4.0` on that merge commit and published
[HallJoy v1.4.0](https://github.com/PashOK7/HallJoy/releases/tag/v1.4.0) as the
latest stable, non-draft, non-prerelease GitHub Release. The initial upload used
the four clean package files. GitHub reports the EXE asset at 2,187,776 bytes
with digest
`sha256:c03cb7a19db73921d69904a7abdab954d54f3d5ce42899add9c24012d93d0402`,
matching the qualified local file and `SHA256SUMS.txt`.

After publication, shortened the visible release summary and moved the complete
release notes into a collapsed `<details>` section. Added a direct
`HallJoy.exe` download link at the top so users do not need to scroll through
the changelog. Removed the test-only `README_FOR_TESTER.txt` public asset; the
release now exposes only `HallJoy.exe`, `SHA256SUMS.txt`, and
`THIRD_PARTY_NOTICES.md`. The executable and its digest were not changed.

## 2026-08-10: Redragon K673RGB-M W669 diagnostic candidate

Statically unpacked the supplied signed Redragon `V2.06.01` desktop package
without running its installer or firmware tools. Neither that package, its
newer `V2.06.08` application payload, nor the referenced firmware channel
contains a K673 firmware image. No unrelated embedded firmware is considered
safe to flash.

Cross-checked the exact `K673RGB-M` against the public iLLumiPC WebHID catalog
and client. It identifies three exact firmware products on `2E3C:C365`,
`FF1B:0091`, report ID 1 and uses the existing W669 `0D/18/21` protocol,
including the real `21/01` per-key change stream and the `21/02` 22-column live
subscription. Recovered and hash-pinned the three official 132-position
factory maps rather than inferring a layout from the shared PID or marketing
name.

Added distinct BR, UK and US K673 profiles selected only by the exact read-only
opcode-`0D` firmware product. The ambiguous HID name `K673RGB-M` remains an
invalid fallback. Unknown W669 products still receive no guessed factory map.
Portable protocol tests compare all 132 BR positions with the recovered source
and cover the variant differences and exact/unknown identity policy.

Extended diagnostic-only W669 telemetry with the exact selected product and
profile, raw reports, event rate and intervals, minimum positive raw travel,
release-to-zero edges, unique pressed-key count, peak simultaneous keys,
session summary, and per-HID coverage. Production code retains the existing
allocation-free live publication path because the new counters compile only
under `HALLJOY_DIAGNOSTIC`.

Built the single-file candidate at `build/aula-diagnostic/HallJoy.exe`,
2,330,112 bytes, SHA-256
`E38E2291DAA63CB28597E55FC683EAC7B38365E32874749D3167F28CC69D18F7`.
The candidate is intentionally unsigned and not a release artifact. Physical
K673 validation remains pending. A four-second isolated no-device smoke created
the one expected `HallJoy.log`, accepted graceful `WM_CLOSE`, exited with code
zero, and left no surviving process.

## 2026-08-10: Redragon K673RGB-M physical BR validation

Analyzed the returned `HallJoy (9).log` (SHA-256
`B2D76566F0BCD93B4997081400DE6539694A0814F32415C8BCD3C2E2F7A72A94`).
The device identifies as `K673RGB-M`, firmware
`W669,34,KB,FR,7272BRHEXYXK673JCARGB,V3.18.01`. The exact
`redragon_k673_br_81` route completed all ten map fragments, proved 81
publishable usages and travel maximum 340, and entered the real `21/01` stream
through a shared non-exclusive session.

The 12.234-second run decoded and published 1,491 live change events across 17
HID usages. It recorded 72 positive edges and exactly 72 release-to-zero edges,
minimum positive travel 7/340 (about 2.06%), maximum travel 340, four keys held
simultaneously, `active_at_stop=0`, and zero final output for every observed
key. The tester did not hold ten keys and the read-only polling query returned
firmware-default code zero, so neither a ten-key stress result nor a numeric
1,000 Hz claim is inferred from the motion-dependent event rollups.

The only anomalous counter was terminal `failures=1`. The preceding telemetry
was zero-failure and the same timestamp shows clean stop, unsubscribe and
`fault_kind=0`; the pending `ReadFile` was completed by normal stop-time
`CancelIoEx`. W669 now treats `ERROR_OPERATION_ABORTED` and
`ERROR_INVALID_HANDLE` as informational only when stop has already been
requested, while preserving every such error as a real failure at runtime.
The backend static audit now guards that distinction. The complete native
backend gate, documentation audit and optimized MSVC diagnostic rebuild pass.
Corrected unsigned diagnostic artifact: `build/aula-diagnostic/HallJoy.exe`,
2,330,112 bytes, SHA-256
`EE6135930FDB9D950CFC6F3FB2270E4EFBD12F861893AD6E4B3CB53191904D1B`.
The public README and hardware matrix now list the physically validated BR
K673 separately from the official-profile-only UK and US variants.
An isolated four-second smoke of that exact rebuilt EXE accepted `WM_CLOSE`,
exited with code zero, left no surviving process and produced the expected
single `HallJoy.log` beside its extracted private host DLL.

## 2026-08-10: Redragon K673 production release candidate

Ran the official `tools/build.ps1` production pipeline against the complete
K673 tree. All native/static/portable gates passed, MSVC `Release|x64`
completed with zero errors and only the allow-listed external ViGEm `LNK4099`
missing-PDB warning. The linked-image audit found no continuous, W669 raw,
telemetry, diagnostic, summary, or coverage markers and retained only the
crash-report contract.

The first packaging attempt correctly stopped because the previous
`build/release/HallJoy.exe` and its child processes were still running. After a
normal user-requested close, the complete official pipeline was rerun and
packaged successfully. Initial unsigned candidate before the packaging cleanup:
`build/release/HallJoy.exe`, 2,188,288 bytes, SHA-256
`33A08D48AFE088814CEAC319CFB52D216F65F96003A74BC133E1359C9D2C9814`.
The digest exactly matches `build/release/SHA256SUMS.txt`.

An independent five-second smoke copied that exact EXE to a clean temporary
directory, accepted `WM_CLOSE`, exited with code zero, left no parent or child
process, retained an unchanged executable hash, and created neither a
continuous log nor a crash report. Only the expected private analogue-host DLL
was extracted beside it.

## 2026-08-10: Remove obsolete tester README packaging

Removed the legacy `README_FOR_TESTER.txt` template and its mandatory
production packaging path. It had no runtime consumer and had already been
excluded manually from the public GitHub release, but the official local build
still regenerated it in both staging and release directories. Production now
packages only `HallJoy.exe`, `SHA256SUMS.txt`, and
`THIRD_PARTY_NOTICES.md`. The build-documentation audit now rejects any future
return of the obsolete filename to `tools/build.ps1`.

The official production pipeline was then rerun to completion with HallJoy
closed. It produced exactly three release files and no tester README. All
native/static/portable gates, the linked-image telemetry exclusion and MSVC
`Release|x64` passed again. Final unsigned candidate:
`build/release/HallJoy.exe`, 2,188,288 bytes, SHA-256
`EFD250C67F07CDB477788BF3461C042527E64566B946A7621F079B4E54945D19`;
the checksum file matches. An independent five-second smoke exited zero after
`WM_CLOSE`, left no process, did not alter the EXE and created no log or crash
report. The superseded local release folder was moved to the recoverable backup
`C:\github\HallJoy_K673_analysis_20260810\retired_release_with_tester_readme_20260810_2328`.

## 2026-08-10: Public Redragon compatibility wording

Expanded the README's two user-facing hardware tables with readable Redragon
marketing model names from the official driver catalog. The text deliberately
separates evidence levels: K673RGB-M is the only physically tested Redragon;
the other listed magnetic-switch models are described as expected compatible
through the same official analogue family, not as physically validated. Raw
firmware product identities were removed from the public Redragon rows while
remaining preserved in the technical protocol and validation records.

## 2026-08-11: HallJoy v1.4.1 release qualification

Raised the centralized product identity from `1.4.0.0` to `1.4.1.0` for the
Redragon patch release. The runtime build family remains `HallJoy-v1.4`, while
the protected-directory private runtime path receives the full `v1.4.1`
version through the existing centralized macro.

The official `tools/build.ps1` production pipeline passed on the complete
release tree. Every native, static, portable C++20, documentation, dependency,
private-UAP ABI, warning-policy, and linked-image gate passed. MSVC
`Release|x64` completed with zero errors and only the allow-listed external
ViGEm `LNK4099` missing-PDB warning. The release folder contains exactly
`HallJoy.exe`, `SHA256SUMS.txt`, and `THIRD_PARTY_NOTICES.md`.

The final unsigned `HallJoy.exe` is 2,188,288 bytes, reports FileVersion and
ProductVersion `1.4.1.0`, and has SHA-256
`B21060D0FE5676A6301DDB2EEB0412DFBF5EDC4850BAD575B82045905FDE4243`.
An independent five-second portable smoke accepted `WM_CLOSE`, exited with
code zero, preserved the executable hash, and created neither a continuous log
nor a crash report. This qualifies the tree for the `v1.4.1` GitHub release
with partial Redragon support: only K673RGB-M is physically validated.

## 2026-08-17: IROK ND75 M484 experimental owner build

Statically analyzed the official signed ND75 V2.03.07 desktop package, V12
updater and raw firmware without launching the vendor tools. Recovered the
runtime endpoint `0416:7372`, `FF1B:0091`, report ID 1, identity
`M484,...,X86HERGB`, the asymmetric host/device analogue opcodes, subscription
and unsubscribe frames, and one-byte row/column/travel event format.

Extracted the Qt v1 resource bundle and derived the exact row-major `6x22` map
from `KeyInfo_X86HERGB.config`. The map has 81 publishable positions, FNV-1a-64
`4BEF9FCF48E36C37`, and subscription mask
`3F 26 3F 1F 1F 1F 3E 1F 1F 3F 3F 1F 3F 21 3F 2F 00 00 00 00 00 00`.
Vendor binaries and configurations remain outside the HallJoy source/package.

Implemented a separate compile-time-gated `IrokNd75M484` native backend. It
requires exact USB/HID framing, exact firmware controller/product and a valid
read-only capability response before claiming the interface. The worker owns
all mapped keys immediately after subscription, clears them on disconnect,
uses bounded overlapped I/O, classifies stop cancellation, and remains ready
for device arrival after no-device startup. The only state transition is a
documented RAM live-stream subscription with a matching unsubscribe.

Added protocol builder/parser/map/fuzz fixtures, a backend static audit,
reproducible research notes, an owner test procedure, a dedicated diagnostic
MSBuild property, packaging script and no-device smoke runner. The complete
native gate passed. Pinned Sun/Soup inputs rebuilt both private UAP ABI DLLs.
MSVC `Release|x64` completed with zero errors and only the allow-listed external
ViGEm PDB warning. The isolated five-second smoke accepted `WM_CLOSE`, exited
zero, preserved the EXE hash, logged a fault-free ND75 worker exit and left no
child process.

This is not production support yet. Physical ND75 evidence is still required
for firmware identity, subscription, travel range/polarity, smooth values,
release-to-zero, representative key-map positions, simultaneous keys,
unplug/reconnect and clean shutdown.

The first packaging pass exposed that `Compress-Archive -LiteralPath` does not
expand the former `*` input. The build script now supplies an explicit sorted
three-file list and reopens the ZIP to enforce exact entry names. The final
unsigned owner EXE is 2,333,696 bytes, reports FileVersion and ProductVersion
`1.4.1.0`, and has SHA-256
`AEC283C6C5D0F158462BEC703975FC71B0C665F8B6C692FBE5644313D06BDE84`.
The final three-entry ZIP is 1,209,903 bytes with SHA-256
`0F3430B200B9C674344D8E7C09A6DD6BEE2B3D95BEC923A6AD0F58C583ACD6A5`.
Its internal checksum matches the EXE, and the exact final executable passed a
second isolated no-device smoke.

The definitive rebuild also requires exactly 64-byte input/output reports,
publishes device presence only after complete identity/capability proof, and
registers the cancellable active handle only after subscription succeeds. The
IROK diagnostic profile now retains up to 64 MiB of stability trace. The full
native gate and the exact rebuilt executable's no-device smoke passed again.

## 2026-08-20: IROK corrective stabilization and pause

Reviewed the experimental ND75 backend before moving development back to the
general production build. The review found three lifecycle/admission defects
not covered by the original string-based static audit: a `V1.*` firmware could
be claimed after the mandatory `21/04` capability proof timed out, a dead live
HID handle never returned to the reconnect loop, and proof-session handles were
not registered for stop-time cancellation.

Removed the capability soft fallback. Admission and the reopened live session
now both require a decoded asymmetric `21/04` response. Every proof and live
session registers its handle for `CancelIoEx`, proof loops observe the stop
request, device-loss errors end the session immediately, other non-timeout read
errors end it after three attempts, and transport loss neutralizes all owned
input before returning to discovery. The QPC-to-microseconds conversion was
also changed to quotient/remainder arithmetic to avoid long-uptime overflow.

The portable read-failure policy test and strengthened backend static audit
pass as part of the complete native gate. MSVC `Release|x64` diagnostic rebuild
completed with zero errors and only the allow-listed external ViGEm PDB
warning. The rebuilt no-device executable accepted `WM_CLOSE`, exited zero,
retained an unchanged hash and left no child process.

Corrected unsigned diagnostic artifact:

- `HallJoy-IROK-ND75-Test.exe`: 2,335,744 bytes, SHA-256
  `E73921F163A1C599C9B70CCDDF43BEC67D8637C50C3C0B337633E730916AE223`;
- `HallJoy-v1.4-IROK-ND75-TEST.zip`: 1,210,772 bytes, SHA-256
  `9C0300447639F83C095370FC0380B86120013626DAF65C31F14471AC7FF1BB55`.

The earlier `AEC283...` / `0F3430...` artifact pair is superseded. IROK work is
now paused, remains compile-time excluded from production, and still requires
the complete physical owner gate before it can be considered supported.

## 2026-08-20: stable production rebuild after the IROK pause

Ran the complete official `tools/build.ps1` pipeline for the general HallJoy
release profile. All native/static/portable C++20, documentation, dependency,
private-UAP ABI, warning-policy and linked-image gates passed. MSVC
`Release|x64` completed with zero errors and only the allow-listed external
ViGEm `LNK4099` missing-PDB warning.

The production profile contains MAD68, Hex80, Addressed, Aula MAX/W669,
SparkLink, Sayo and private UAP/Wooting routing. The experimental IROK catalog
entry is not enabled. Direct linked-image inspection found no IROK backend,
telemetry or removed capability-fallback markers.

The clean release folder contains exactly `HallJoy.exe`, `SHA256SUMS.txt`, and
`THIRD_PARTY_NOTICES.md`. Final unsigned `HallJoy.exe`: 2,189,824 bytes,
FileVersion/ProductVersion `1.4.1.0`, SHA-256
`A66C00DEEFDF9FD408E9511A8C66A541FBFAF040719DA06049EF96B16028B906`.
The checksum file matches the executable.

An independent startup/shutdown smoke accepted `WM_CLOSE`, exited zero,
preserved the executable hash and created no diagnostic/crash file or surviving
process. A second production runtime smoke passed overlay responsiveness
(0.4 ms), fragmented/pipelined HTTP framing, authorization/origin and
concurrency limits, plus 2,000 fuzz cases with zero timeouts, then shut down
cleanly with no continuous log or crash report.

## 2026-08-20: persistent UAP audit and Pwnage Zenblade 65 V2 reconnaissance

Promoted the all-family UAP audit to an explicit open release risk so it cannot
be lost when work moves to another chat. `UAP_ALL_KEYBOARDS_AUDIT.md` remains
the authoritative defect list. The release qualification above does not close
the special-key, parser or hotplug findings. Added P0 risk entries
`HJ-V14-P0-001` and `HJ-V14-P0-002`.

Inspected the official Pwnage Web Hub selected on 2026-08-20. The pinned assets
identify the newer Zenblade interface as expected `3662:1002`, report ID 0,
usage `FF60:0061`; the hub internally calls it Zenblade protocol/screen v3 even
though the retail product is Zenblade 65 V2. It uses 64-byte output reports and
input-report replies. Corrected the initial protocol-generation mix-up: the
WASM and `00 71` request belong to the older PID `1001`; PID `1002` screen 3
constructs a different direct VIA/QMK-like protocol in JavaScript.

The usage pair matches Madlions/Keychron, but the wire protocols do not:
Madlions uses `02 96 1C` in a 33-byte request and Keychron uses `A9`. The Web Hub
exports configuration/actuation-setting operations but no live travel-matrix
operation. No PID graft or production code was added. The next safe step is the
bounded single-EXE hardware diagnostic specified in
`PWNAGE_ZENBLADE_65_V2_PROTOCOL_RECON.md`.

Later recovered the Pwnage-representative-linked Zenblade SOCD updater V0022
from the public subreddit post. Its updater manifest proves old runtime
`3662:1001`, command interface `MI_02`, and bootloader `3662:1002`. This image
predates and is not firmware for the retail V2; the bootloader PID collides with
V2's normal runtime PID, so the old updater must never be run against V2.
Static firmware disassembly enumerated the old dispatcher and proved wire
command `71` is low command `31`, reading persistent 68-key trigger arrays, not
current Hall travel. None of the old host command handlers exports the dynamic
sensor arrays. No executable or protocol admission was added.
## 2026-08-20 - production-native all-family static audit

- Audited the common native descriptor/registry/routing contract and all seven
  production routes: MAD68 Pro R, Hex80, Addressed `09/94/02`, Aula MAX 6x21,
  Aula W669, SparkLink and SayoDevice depth.
- Confirmed two P0 data-integrity defects without changing code: Sayo derives
  physical analog identity from a later binary boot-keyboard event and can
  fabricate full depth; W669 has no valid-live-event silence deadline and may
  retain stale connected/nonzero state indefinitely.
- Confirmed P1 Hex80 partial-generation/per-chunk failure defects, Addressed
  canonical-layout overreach, SparkLink discovery/scale/duplicate defects and
  a shared native 8-bit special-key limitation. Aula MAX remains the strongest
  static implementation and had no new P0 in its main data path.
- Added `NATIVE_ALL_KEYBOARDS_AUDIT.md` and open risks `HJ-V14-P0-003` through
  `HJ-V14-P1-013`. This was documentation-only work: no source fix, build,
  hardware claim or git operation was performed.

## 2026-08-20 - common analog pipeline static audit

- Audited the downstream path shared by UAP and native sources: source merge,
  raw/filtered caches, curves, bindings, SOCD, complete XUSB construction,
  ViGEm publication, profile persistence, key identity, desktop UI and overlay.
- Added P1 risks `HJ-V14-P1-014` through `016`: the hand-maintained global
  digital-fallback policy, unvalidated/non-transactional live profile loading,
  and incompatible 16-bit/8-bit identity domains across actions and UI.
- Added P2 risks `HJ-V14-P2-010` and `011` for stale SOCD state and ambiguous or
  partial layout/UI snapshots. Recorded strong existing XUSB/ViGEm, clamping,
  curve and immutable-layout boundaries separately from the defects.
- Audited the relevant green gates: current persistence checks prove atomic
  save, curve checks prove the generation primitive, routing checks prove only
  native-owned fallback exclusion, and the simulator checks ordinary opposite
  direction cancellation. None exercises transactional load, whole-profile
  publication, source arbitration, SOCD state reset or extended identity end to
  end.
- Added `COMMON_ANALOG_PIPELINE_AUDIT.md` to the mandatory correctness release
  blocker. This was documentation-only work: no source fix, build, hardware
  claim or git operation was performed.

## 2026-08-20 - artificial limits and performance static audit

- Proved that the native `[0..1000]`/`uint16_t milli` ABI is an internal
  HallJoy limit rather than an XUSB/ViGEm requirement. It prematurely reduces
  Sayo 4001, MAD68 Pro R 1601, Hex80/Aula MAX roughly 3301/3401 and dynamic
  Addressed/SparkLink source domains before user curves, while UAP retains
  normalized float input.
- Traced the generic UAP hot path end to end. Every per-key merged/per-device
  lookup copies and scans a full 256-float shared snapshot under an exclusive
  API lock; plugin and child host also rebuild/copy complete snapshots and wake
  realtime for unchanged poll generations. Bind capture scans all 255 HIDs.
- Separated useful deadline coalescing/failure backoff from two unproven hard
  1 kHz ceilings: compile-time UAP poll pacing and fixed ViGEm publication.
  Recorded the independent 1..20 ms heartbeat semantics, Sayo unchanged wake,
  Aula 1 ms success pause, Hex80 four-entry chunks and inconsistent UI/overlay
  refresh work.
- Recorded the private UAP capacity mismatch: dynamic plugin device storage,
  eight dense/telemetry records and 16 parent IDs. Four XUSB pads remain an
  external XInput constraint, not an artificial HallJoy defect.
- Added `ARTIFICIAL_LIMITS_PERFORMANCE_AUDIT.md`, P1 risks
  `HJ-V14-P1-017` through `020`, P2 risks `HJ-V14-P2-012` through `015`, and
  roadmap package `V14-20`. The new P1 groups are mandatory release blockers.
- This was documentation-only work. No source implementation, build, hardware
  claim or git operation was performed.

## 2026-08-20 - input-provider architecture static audit

- Traced the current UAP data plane from private full/dense snapshots through
  the monolithic shared mapping, parent bridge, Wooting-style wrappers and the
  generic per-HID/per-device realtime path. Confirmed that a complete coherent
  snapshot already exists before HallJoy turns it back into serialized per-key
  reads.
- Audited the analog-host IPC ownership and evolution model. The fixed
  all-access `SharedState` combines parent control, child values/status,
  telemetry and diagnostics under one ABI/event and fixed 256-key/eight-device
  capacity; sample, value-change and health generations are not separated.
- Confirmed the process-containment asymmetry: UAP/Soup is disposable-child
  isolated, while native catalog workers and storage execute inside the main
  UI/realtime process and expose a separate milli/per-HID contract.
- Specified a staged `AnalogProviderV2` direction: preserve shared memory and
  existing ownership/lifecycle/output strengths; add one immutable snapshot
  acquisition, complete key/source identity, separate data/control/health
  planes, negotiated capacity, common UAP/native semantics, native provider
  isolation and immutable runtime-config generations.
- Explicitly rejected per-key pipe/RPC, binary-derived analog identity,
  unbounded every-sample queues, magic-capacity increases and a big-bang UAP
  parser rewrite. UAP remains the first adapter; a native DrunkDeer provider can
  replace it family-by-family after physical protocol evidence.
- Added `INPUT_PROVIDER_ARCHITECTURE_AUDIT.md`, new P1 risks
  `HJ-V14-P1-021` through `023`, and release-blocking roadmap package `V14-21`.
  This was documentation-only work. No source implementation, build, hardware
  claim or git operation was performed.

## 2026-08-20 - test and evidence trust audit

- Inventoried 98 files under `src/HallJoyProject/tests`: 59 Python source-text
  audits, 34 portable C++ executables, four additional hardware/log analysis
  tools and one shared Aula oracle fixture header. None of the 59 audits uses
  AST/control-flow analysis or executes the production function it reads.
- Ran `python tools\run_native_backend_checks.py --require-compiler`: PASS, all
  59 static audits and 34 C++ executables.
- Ran `python tools\run_protocol_fuzz_sanitizers.py`: PASS, 250,000 iterations
  and 61,998 accepted cases, but only Aula MAX/Hex80/MAD68PR parsers are linked.
- Ran `python tools\run_aula_win60he_sanitizers.py`: PASS, six executables.
  Both Windows sanitizer runs emitted `interception_win: unhandled instruction`;
  the effect is not inferred, and a mandatory known-bad sanitizer health control
  is now required.
- Re-ran the prerelease-hardening, UI prerelease, release-qualification-runner
  and S20 build/documentation static audits after documentation updates: all
  PASS. Their green result does not alter the audit finding that several labels
  prove only token/filename presence rather than execution.
- Proved that mandatory build/tests/docs/generator enforce legacy defects:
  disabled UAP hotplug, milli `[0..1000]`, 256-key storage and fixed 1 kHz.
- Proved that official build/CI does not execute sanitizer, production smoke,
  fault, soak, migration/reset, hardware shutdown or exact-artifact release
  qualification runners. Several static gates confuse filename presence with
  execution and overstate actual parser/family coverage.
- Mapped every open P0/P1 to its current oracle and missing behavioral evidence.
  Added `TEST_EVIDENCE_TRUST_AUDIT.md`, risks `HJ-V14-P1-024` through `027`,
  `HJ-V14-P2-016` through `017`, and release-blocking package `V14-22`.
- This audit changed documentation only. No production/test/build-script fix,
  new build artifact, hardware claim or git operation was performed.

## 2026-08-20 - concurrency, lifecycle and ownership static audit

- Traced application startup/rollback/shutdown, realtime, ViGEm output,
  native catalog workers, SparkLink/Sayo reader generations, analog-host,
  overlay, common OVERLAPPED HID I/O and compound runtime publication.
- Confirmed a P0 close/use race: realtime and control publishers read the plain
  ViGEm wake `HANDLE` while UI watchdog recovery can join, close and recreate
  it. Making only the handle atomic would not pin its lifetime.
- Confirmed a P1 C++ data race in desktop tracked-HID publication: clearing an
  atomic count does not stop a realtime reader that already loaded the old
  count from racing writes to the plain array.
- Confirmed lifecycle truth gaps: production does not call the tested
  `MarkFaulted` transition; the native registry can remain `Running` after a
  worker exits, and ready acknowledgement is not a common start contract.
- Confirmed that SparkLink/Sayo Start, Stop, HID discovery/proof and joins are
  called directly from `Backend_Tick`, so reconnect may stall every input and
  XUSB path for milliseconds, seconds or longer.
- Confirmed that `HidIoOperation::CancelAndDrain` correctly preserves
  OVERLAPPED memory but uses an unbounded final wait across eight native
  implementations; hard deadlines require provider process containment, not
  early destruction of request storage.
- Confirmed that the shutdown watchdog allocates its own event/thread only at
  shutdown and silently loses the process deadline if either allocation fails.
- Recorded P2 compound-state incoherence, including a concrete simultaneous
  Sayo reader-completion lost update and provider connected/ownership/value
  TOCTOU reads.
- Added `CONCURRENCY_LIFECYCLE_OWNERSHIP_AUDIT.md`, P0 risk
  `HJ-V14-P0-005`, P1 risks `HJ-V14-P1-028` through `032`, P2 risk
  `HJ-V14-P2-018`, and release-blocking package `V14-23`.
- Re-ran four targeted documentation/build-contract static audits after the
  edits: S20 build/docs, release-qualification runner, pre-release UI and
  prerelease hardening all PASS. Their source-token scope is unchanged and is
  not evidence that any newly found runtime interleaving is fixed.
- This was documentation-only work. No source implementation, test contract,
  build, EXE, hardware claim or git operation was performed.

## 2026-08-21 - safe one-click embedded ViGEmBus installation

- Re-audited the historical installer and V14-12F. Downloading was not the
  intrinsic defect: mutable `latest` selection, a predictable temp path across
  UAC and an infinite UI wait were. The later manual-only message removed the
  unsafe code but made first-run installation unacceptably difficult.
- Embedded the exact official ViGEmBus 1.22.0 installer (6,278,576 bytes,
  SHA-256 `89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A`)
  and retained its BSD-3-Clause license and notice. Runtime network download
  and mutable release lookup remain absent.
- Added exact resource size/SHA checks, CSPRNG `CREATE_NEW` extraction, a
  writer-to-read-only lock transition, post-lock and pre-elevation re-hashing,
  Authenticode verification through the locked handle, and a non-executing
  read/execute compatibility probe. The file denies write/delete sharing while
  setup runs.
- Replaced the copyable error box with explicit command links: Install,
  official release page, or continue without virtual output. Installation is
  user-approved through UAC; waiting pumps messages and is bounded to 20
  minutes; success retries backend initialization in-process and restart/error
  outcomes are explicit.
- Added negative/static regression coverage and made the official build verify
  source identity/publisher plus the exact linked RCDATA extraction, hash,
  signature, lock and executable-open path without elevation.
- Full compiler-required native suite and final clean official build passed.
  `build/release/HallJoy.exe`: version 1.4.1.0, 8,485,376 bytes, SHA-256
  `7EB42CF1687D0DB1FF16721875C5F36B502FD96D1EB1DEC686C3AC4EEA85AB6B`.
  Compiled, `build/output` and `build/release` copies are identical; self-test
  exit is zero and leaves no new temp directory.
- Boundary: ViGEmBus was already installed on this workstation, so the actual
  missing-driver/UAC setup was not executed. The embedded bytes and complete
  non-elevating preparation path are verified; this is not a new keyboard
  compatibility or broad release-qualification claim. No git/GitHub operation
  was performed.

## 2026-08-21 - release-readiness mega audit and IROK freeze forensics

- Consolidated the nine mandatory UAP/native/pipeline/performance/provider/
  evidence/concurrency/pause/security audits into one non-duplicating execution
  roadmap, `RELEASE_READINESS_MEGA_AUDIT_2026-08-21.md`.
- Defined release invariants, dependency order R0..R8, compile-time exclusion
  policy, targeted hardware matrix and exact signed-artifact definition of done.
- Read the two newest physical IROK logs and both exit sidecars. The failing run
  kept SparkLink healthy for `279189/279189` route queries while ViGEm output
  received `WAIT_FAILED/ERROR_INVALID_HANDLE`, became poisoned and rejected 161
  recovery attempts. This is new P0 `HJ-V14-P0-006`.
- The next 160-second run kept ViGEm alive and SparkLink completed
  `421858/421858`, but `sayo stop.lock_timeout` poisoned common native shutdown.
  The crash sidecar was explicitly synthetic. This is new P1
  `HJ-V14-P1-038`.
- Compared the owner-confirmed stable EXE (`B21060D0...DE4243`) with the current
  candidate as an A/B pair. The available ViGEm output-worker source section is
  line-for-line identical, so the stable build is a runtime oracle rather than
  a justified blind source rollback.
- Extended the SparkLink code audit: a successful neighbouring row resets the
  global failure/freshness state, allowing one permanently failed row to retain
  stale nonzero values. Added `HJ-V14-P1-039`; the supplied logs did not trigger
  it because both had zero route failures.
- Added a privacy-safe evidence summary under `docs/stability/tests`; raw user
  logs were not copied into the project.
- Updated README, handoff, blockers, roadmap, risk register, decision log,
  validation matrix and both owning audits. This package changed documentation
  only: no production/test/build code, EXE, support claim or Git/GitHub state
  changed.
- Before the global documentation update, copied all ten pre-change documents
  into `_backups/release_readiness_mega_audit_pre_20260821_2345` and verified
  each source/backup SHA-256 pair.

## 2026-08-21 - mandatory engineering working method

- Recorded the owner's primary objective as a normative engineering contract:
  correctness and durable architecture take priority over speed, diff size or
  preserving a flawed implementation.
- Added `ENGINEERING_WORKING_METHOD.md`. Every package now starts with context,
  evidence, invariants and an old-bug oracle, then compares a local root fix,
  staged migration and clean redesign/rewrite before production edits.
- Defined the selection criteria: correctness, latency, hard liveness,
  ownership, complete data contracts, compatibility, production-linked
  testability, security/privacy, maintainability and rollback.
- Explicitly prohibited digital-derived analog identity, guessed layouts or
  scales, stale nonzero fallback, unbounded reconnect/timeout masking,
  lifecycle work in realtime, silent domain truncation and diagnostic behavior
  presented as production proof.
- Added stop conditions that force a return to architecture design when a fix
  adds special cases, lacks one owner, depends on late digital input, destroys
  live I/O state or cannot be tested through production.
- Linked the contract from the mega-roadmap, roadmap package rules, handoff,
  README and D-052. No production/test/build code or EXE changed.
- Created and hash-verified the pre-change backup
  `_backups/engineering_working_method_pre_20260821_2355` before the global
  documentation update. No Git/GitHub operation was performed.

## 2026-08-21 - R0 ViGEm output architecture selection

- Restored the complete current output path: realtime latest-value mailbox,
  raw wake handle, output thread, ViGEm create/update/reconnect/destroy owner,
  UI watchdog, lifecycle model and existing fault/stall simulator gates.
- Confirmed the architectural limit: the current three-second thread join can
  contain an ordinary fault but cannot provide safe recovery from a synchronous
  ViGEm call that never returns. `TerminateThread` would destroy live state and
  remains forbidden.
- Compared three options under D-052: local process-lifetime wake/RAII repair,
  an in-process generation owner, and a supervised self-hosted output process.
- Selected the process boundary in D-053. It preserves newest-value shared
  publication and realtime non-blocking behavior while making the complete
  child the hard-kill/reap boundary for a stalled driver call.
- Added `VIGEM_OUTPUT_PROCESS_ARCHITECTURE_2026-08-21.md` with owner/data/control
  contracts, five old-bug/fault oracles, eight migration stages, rollback and
  exact release evidence. No production/test/build source or EXE changed yet.
- Created and hash-verified `_backups/vigem_output_r0_architecture_pre_20260821_2359`
  before updating the release documentation. No Git/GitHub operation occurred.

## 2026-08-21 - R0 O2 ViGEm invalid-thread-handle old-bug oracle

- Added a simulator-only injection that closes the legacy output thread handle
  immediately before the real watchdog observation while retaining its raw
  value. Non-simulator production code cannot activate the switch.
- Extended the existing simulator runner and production-linked static audit.
  The runner expects the future target contract: recovery markers, clean
  shutdown and exit 0; it does not treat reproduction of the bug as success.
- Rebuilt with MSVC successfully and ran the oracle. The legacy code produced
  the exact physical-failure class: `WAIT_FAILED/ERROR_INVALID_HANDLE`, one
  worker exit, permanent recovery blocking, poisoned backend shutdown and
  `session.end exit_code=2`.
- Preserved the privacy-safe evidence and exact 22,312-byte trace SHA-256
  `920F9586B5611AF480D74ED394A41E15FB5F199C40DBDF8B131AFFD66C497BEA`
  in `docs/stability/tests/V14_R0_VIGEM_INVALID_THREAD_HANDLE_ORACLE_2026-08-21.txt`.
- O2 is now a deterministic RED gate. `HJ-V14-P0-006` remains open until the
  process-owned generation supervisor passes this same runner with exit 0.
- Before documentation changes, created and hash-verified
  `_backups/vigem_p0_006_o2_evidence_pre_20260822_0015`. No Git/GitHub
  operation occurred.

## 2026-08-21 - R1 ViGEm output shared ABI and claimed-slot channel

- Implemented the first isolated migration package from D-053: a fixed-width,
  SDK-independent, 640-byte shared ABI and a production-linked three-slot
  newest-value channel. The new files compile into the normal MSVC target but
  are not connected to `backend.cpp`; the legacy worker remains the only
  production ViGEm owner.
- Rejected the initially proposed plain-payload seqlock before using it. Its
  retry check would detect concurrent copying but leave a formal C++ data race.
  The replacement uses atomic `Empty/Writing/Ready/Reading` ownership, so only
  one side can touch a slot payload at a time.
- Preserved the realtime contract: publication performs a bounded slot scan and
  contains no waiting or OS I/O. Ready snapshots may be coalesced; a Reading
  snapshot is never overwritten. Every payload carries its child generation,
  and stale generations are discarded.
- Added ABI, malformed-input, delayed-consumer, held-slot, generation and
  concurrent full-report tests. Five consecutive 100,000-publication runs
  passed without torn reports or non-monotonic consumption.
- The complete native static/portable suite passed. A direct MSVC 17.14 Release
  x64 build then completed with 0 errors and only the existing allow-listed
  ViGEmClient LNK4099 warning.
- The compile-only `src/HallJoyProject/x64/MAD68ProRNative/HallJoy.exe` is
  8,485,376 bytes, SHA-256
  `908D34E893CD6A24B279A98A57712979039BD148351397E211FCE6C3AA14F512`.
  It was not promoted to either delivery directory because output routing has
  not switched and `HJ-V14-P0-005/006` remain open.
- Evidence is recorded in
  `docs/stability/tests/V14_R1_VIGEM_OUTPUT_SHARED_CHANNEL_2026-08-21.txt`.
  Pre-change backups are
  `_backups/vigem_output_r1_shared_channel_pre_20260822_0030` and
  `_backups/vigem_output_r1_shared_channel_docs_pre_20260822_0110`.
  No Git/GitHub operation occurred.

## 2026-08-22 - R0/R1.1 ViGEm ownership foundation completed

- Added O1 to the real legacy production path. The simulator-only scheduler
  pauses the actual publisher after it copies the wake handle, then the actual
  owner executes `VigemOutput_Stop -> close -> Vigem_Create ->
  VigemOutput_Start`. Four runs reproduced stale-generation use.
- The strongest run did not return `ERROR_INVALID_HANDLE`: stale handle value
  `0x284` had rebound to a foreign event, so `SetEvent` returned success while
  the current HallJoy wake was `0x29C`. This proves silent wrong-object
  signalling and keeps the future target runner RED.
- Added generation/owner/wait/close provenance for legacy output thread and
  wake resources. Normal production behavior is unchanged; forced scheduling
  remains simulator-only.
- Reopened the R1 channel contract before advancing. Windows shared state now
  uses documented Interlocked operations, an in-flight publisher lease makes
  generation disable quiescent, and child health fields cannot publish
  parent-authoritative lifecycle/restart state.
- Added a real Windows self-spawning process test. It rejects a wrong nonce,
  exchanges 100,000 complete four-pad snapshots, verifies exact progress,
  disables/quiesces publication and reaps the child. Interprocess observations
  are atomic and a timeout cannot leak the test-owned child.
- The complete native static and production-linked C++ suite passed with a
  required compiler. MSVC x64 simulator and production builds passed with zero
  errors and only the existing allow-listed external ViGEmClient LNK4099.
- Added `RISK_TEST_MANIFEST_V1.json` and its fail-closed audit. All 38 current
  unresolved/partial P0/P1 risks are explicit; absent gates remain `MISSING`,
  O1/O2 remain `OLD_BUG_CONFIRMED`, and release state remains `BLOCKED`.
- Preserved privacy-safe O1 and corrected R1.1 evidence. No build was promoted
  or sent to a user; legacy production routing remains the sole ViGEm owner.
- Hash-verified rollback checkpoints:
  `_backups/vigem_p0_005_o1_provenance_pre_20260822_0200`,
  `_backups/vigem_output_r1_abi_contract_reopen_pre_20260822_0245`, and
  `_backups/vigem_r0_r1_docs_pre_20260822_1015`.
- Next package is the generic fake-child process supervisor under D-054. No
  ViGEm calls move into a child until explicit handle inheritance, job/deadline
  ownership, exact reap, no-overlap restart and zero-survivor gates pass.

## 2026-08-22 - F1 generic process-generation supervisor

- Compared direct analog-host reuse, immediate analog-host extraction/migration
  and a clean reusable owner before code. Direct reuse retained a pre-job child
  execution window and UAP coupling; immediate migration expanded the package
  across a qualified route. Selected the isolated common primitive under D-055.
- Implemented a non-copyable production-linked owner and synchronous
  one-generation state machine. Every child is created suspended with an
  explicit handle list, assigned to a private kill-on-close job, then resumed.
  Process/job handles never leave the owner and close only after confirmed reap.
- Added typed startup/progress/planned-stop/hard-reap behavior outside realtime.
  PID/generation mismatch and child fatal state are distinct from timeout and
  ordinary exit. Unconfirmed reap remains restart-blocking.
- Added a real Windows self-spawning fake-child test covering cooperative stop,
  exit before ready, never-ready, progress stall, ignored stop, post-ready exit,
  wrong generation, child fault, non-inheritable handle and missing executable.
- Corrected the decoy oracle during development: a numeric handle can rebound
  inside child, so the parent checks that the exact decoy event remains
  unsignalled rather than trusting `GetHandleInformation(number)`.
- The same supervisor completed 1,008 child generations, including 1,000
  sequential stress replacements. Containment existed at child entry, maximum
  overlap was one, every returned PID was non-live, and final enumeration found
  zero fake-child survivors.
- Full native suite passed. MSVC Release x64 production and simulator targets
  passed with 0 errors and only the existing allow-listed ViGEmClient LNK4099.
  Compile-only artifacts were not promoted or sent to users.
- Evidence: `docs/stability/tests/V14_F1_PROCESS_GENERATION_SUPERVISOR_2026-08-22.txt`.
  Backups: `_backups/process_generation_supervisor_f1_pre_20260822_1125` and
  `_backups/process_generation_supervisor_f1_docs_pre_20260822_1215`.
- Production output remains unchanged. The next package is early
  `HallJoy.exe --halljoy-vigem-output-host` dispatch with fake transport and the
  R1.1 mapping; real ViGEm calls remain in the legacy owner until it passes.

## 2026-08-22 - F1.1 exact HallJoy self-host fake transport

- Compared four implementations before code: a helper EXE, UAP-host graft,
  direct test-only wiring and a reusable early same-image output session. D-056
  selects the last option; the complete comparison is in
  `VIGEM_OUTPUT_SELF_HOST_FAKE_TRANSPORT_DESIGN_2026-08-22.md`.
- Added the output-host dispatch as the first `wWinMain` action. It accepts one
  exact ten-argument internal command, inherited unnamed handles, owner PID/
  handle identity, launch nonce, active generation and job containment. Fake
  behavior is simulator-only; production rejects it before normal startup.
- Added a persistent output session which owns mapping/wake/child-stop/owner
  resources and composes the F1 supervisor with R1.1 publication. Its four-
  handle allowlist is explicit, process/job handles stay private, and
  publication has no lifecycle wait or resource close. `SetEvent` failure is a
  typed error.
- Named `childTelemetrySequence` and `childGeneration` in reserved shared
  capacity without changing the 640-byte ABI. The sole child writes odd/even
  transactions; a parent accepts only stable even snapshots. A separated new
  writer repairs an odd transaction left by a killed predecessor.
- Added a 100,000-update portable telemetry concurrency test. Every accepted
  applied sequence/checkpoint pair was coherent; wrong PID/generation writes
  were rejected and the odd-writer recovery fixture passed.
- Added output-specific stop qualification above the generic process result.
  `PlannedStop` succeeds only with matching completed generation plus neutral
  and target-removal acknowledgements. Exit during planned neutral is reaped
  but returned as `IncompletePlannedStop`.
- Corrected the ready/exit polling race: when the process signal wins the next
  supervisor poll, stable post-reap telemetry from the exact generation
  truthfully classifies a committed Ready followed by exit as
  `UnexpectedExit`, not `ExitBeforeReady`.
- The actual simulator image completed nine sequential generations: normal
  apply/clean stop, all six O4 lifecycle exits, O3 progress stall/force/reap,
  and one clean replacement. The replacement ignored stale-generation data and
  acknowledged the exact sequence/checkpoint of the newest of three complete
  four-pad publications. Every PID was reaped; final survivor count was zero.
- Full compiler-required native checks passed, including the prior 1,008 F1
  generations. MSVC Release x64 simulator and production builds passed with 0
  errors and only the existing allow-listed ViGEmClient LNK4099.
- Final compile/test artifacts: simulator 8,634,368 bytes, SHA-256
  `F29E803225DDB2462FA570114AD202F3448F31DC08E4A8AFEF89D7B456783051`;
  production compile-only 8,487,424 bytes, SHA-256
  `4780C488EF2A2280F85611FF5310A3A7105B3275DF35DDD5326948F8ED7B988F`.
  Neither was promoted or sent to a user.
- Evidence:
  `docs/stability/tests/V14_F1_1_VIGEM_OUTPUT_SELF_HOST_FAKE_TRANSPORT_2026-08-22.txt`.
  Backups:
  `_backups/vigem_output_f1_1_pre_20260822_1345` and
  `_backups/vigem_output_f1_1_docs_pre_20260822_1420`.
- `backend.cpp` is unchanged and remains the sole real ViGEm owner. The next
  package is F2 real child-side create/update/neutral/remove under the proved
  session; O5, atomic route switch and hardware qualification remain later.

## 2026-08-22 - F2 exact HallJoy real-child ViGEm transport

- Re-read the engineering method and F1.1 boundary, then compared four F2
  implementations before code: direct SDK calls in the command host, complete
  legacy-worker transplant, immediate transport extraction into both routes,
  and a child-only fixed-capacity RAII transport. D-057 selects the last option;
  the complete comparison is in
  `VIGEM_OUTPUT_REAL_CHILD_TRANSPORT_DESIGN_2026-08-22.md`.
- Named `requestedPadCount` in reserved control space without growing the 640-
  byte ABI. Configuration is immutable for an active generation and `Ready`
  now acknowledges the matching applied configuration generation.
- Added `VigemChildTransport`: one client, four fixed target records, explicit
  real initial neutral before Ready, exact snapshot conversion/update, and an
  exhaustive stop that visits all neutral/remove/free operations while keeping
  the first exact SDK phase/error.
- Added an immutable production ViGEm API table and deterministic fake table.
  The portable test passed 8 partial allocate/add, 4 initial-neutral, 4 report-
  update, 4 planned-neutral and 4 removal failure edges with zero fake object.
- Routed only the already-private early host `real` mode through this transport.
  Fake modes remain simulator-only. `backend.cpp` was not modified and its
  current SHA-256 equals the pre-F2 backup.
- Added a separate exact real-driver command and runner. With the local
  ViGEmBus service running, the actual simulator image created four real X360
  targets, applied and acknowledged a full nonneutral four-pad snapshot,
  neutralized/removed all four, restored the 3-entry PnP baseline and reaped the
  child with zero survivor.
- The prior nine-generation fake O3/O4 exact suite and 1,008-generation generic
  supervisor remained green. The complete compiler-required native suite
  passed on the final sources.
- Clean MSVC Release x64 simulator and production rebuilds passed with 0 errors
  and no new warning; only the existing external ViGEmClient LNK4099 remained.
  Final compile/test artifacts are simulator 8,649,216 bytes SHA-256
  `922CE616A4FBD01ED59F2BDDD6441B8C70DF5311B7AE299EFC91391D83402174`
  and production 8,500,736 bytes SHA-256
  `D4A5EA026EE7BD852DAEA91F20DBBAD00B632F5A7401C8F4A43A4782F703E147`.
  Neither was promoted or delivered.
- Production malformed-host/self-test rejection remained exit 60/91 and final
  live output-host count was zero.
- Evidence:
  `docs/stability/tests/V14_F2_VIGEM_OUTPUT_REAL_CHILD_TRANSPORT_2026-08-22.txt`
  (7,270 bytes, SHA-256
  `E92CE44F8BC77B20AA2EA6E323CE99C15E694159D53E720BB14F467F1BC9C130`).
  Backups: `_backups/vigem_output_f2_pre_20260822_1118` and
  `_backups/vigem_output_f2_docs_pre_20260822_1145`.
- `HJ-V14-P0-005/006` remain open: normal production still uses the legacy
  closeable wake/thread worker. The next package is F3 atomic parent routing
  plus complete legacy owner removal, followed by routed O1/O2/O5 and physical
  IROK/DrunkDeer qualification. No Git/GitHub operation occurred.

## 2026-08-22 - F3 atomic production route and legacy-owner removal

- Re-read the mega-roadmap, engineering method, F1/F1.1/F2 decisions and exact
  ownership boundaries. Compared patching the legacy in-process worker,
  introducing a temporary dual runtime and routing through the proved process
  session. D-058 selects the last option because only process containment gives
  a safe hard bound to a stuck SDK call without an owner-overlap mode.
- Created hash-verified backups
  `_backups/vigem_output_f3_production_route_pre_20260822_1815` and
  `_backups/vigem_output_f3_docs_pre_20260822_1840`.
- Added one production `OutputRuntime`: one process-lifetime output session,
  one parent owner thread, immutable desired revisions, bounded recovery
  backoff and an outer session-access lease. Realtime only publishes complete
  configured-pad snapshots and never waits/closes/enters ViGEm.
- Atomically removed the old backend mailbox, output thread, closeable wake,
  reconnect/watchdog owner, direct client/target creation and SDK update calls.
  Static gates prove `vigem_child_transport.cpp` has the only real
  `vigem_target_x360_update` call site.
- Corrected termination ordering in the generic supervisor: every natural
  reap, planned stop, wait failure, protocol/fault force and progress timeout
  runs the output prepare hook first. The hook closes publication admission and
  drains publisher leases; a replacement cannot begin before confirmed reap.
- The first exact clean-stop test exposed a deeper control-plane error:
  `activeOutputGeneration` was both publication admission and child identity.
  Closing admission therefore rejected the child's Stopping/Stopped
  neutral/remove acknowledgement. Delaying admission closure and accepting an
  unqualified stop were rejected. Child generation/PID lifetime identity is now
  separate and remains valid only until the predecessor is reaped.
- Full static/portable checks pass, including 1,008 supervisor generations,
  prepare-before-reap and zero overlap. Exact same-image fake transport passes
  nine generations and zero survivor. Exact real ViGEmBus passes four-pad
  create/apply/neutral/remove, PnP baseline restoration and zero survivor.
- Normal routed simulator and four routed recovery scenarios pass. A stalled
  or exited generation records disabled=1, quiescent=1, restart_safe=1 and is
  reaped before exactly one replacement becomes Ready; application shutdown is
  clean exit 0 with child_survivors=0.
- Exact final routed simulator: 8,649,728 bytes, SHA-256
  `117CE5CE0041F1F4DB2B1AE7779183F7284C5B0FD0F27BED202D082E033943D5`.
  Evidence:
  `docs/stability/tests/V14_F3_VIGEM_OUTPUT_PRODUCTION_ROUTE_2026-08-22.txt`.
- F3 code route and local O1/O2 target gates are PASS. P0-005/P0-006 are now
  `Implemented / hardware pending`, not Verified. Final rebuild, longer O5 and
  one immutable candidate on IROK MG75 Max and DrunkDeer G65 remain mandatory.
  No Git/GitHub operation occurred and no build was delivered to a user.

## 2026-08-22 - F3/O5 generation-bound topology and lifecycle stress

- The first repeated runtime stress found a real boundary defect rather than a
  flaky test: pad-count reconfiguration could cross a child-generation boundary,
  and pre-stop admission closure was incorrectly treated as if every already-
  admitted publisher lease had already drained.
- Compared pausing the whole analogue flow, rejecting mismatched payloads in the
  child and generation-bound publication in the parent. Selected the parent
  boundary: realtime remains bounded and independent of digital activation,
  while every report must match both active generation and configured pad count.
- Split the lifecycle barriers deliberately. Prepare-stop atomically closes new
  publication admission; the supervisor then stops or forces and reaps that exact
  child; only after reap does the session wait boundedly for an already-admitted
  producer before shared-resource reuse. Unsafe sessions rebuild immediately.
- Added the exact same-image `--halljoy-test-vigem-output-runtime-stress` gate and
  runner. Twenty independent runs passed at least 2,000,000 complete snapshots,
  2,020 child generations, 1,000 topology changes and 200 disable/enable cycles,
  with exact final sequence/checkpoint acknowledgement and zero session rebuild,
  unsafe generation, overlap or survivor.
- All static/portable/native compiler checks, exact fake and real ViGEm child
  suites, normal routed output and four routed recovery scenarios passed. The
  full Release x64 build completed with 0 errors; only the allow-listed external
  ViGEmClient LNK4099 remains.
- Final local simulator: 8,654,336 bytes, SHA-256
  `14FFE00730DD939D1D74E6866FC9EF4439F98E0E5C2E457C77FB45FE930296E7`.
  Final local production artifact: 8,518,144 bytes, SHA-256
  `DADFF635AD4681DA3B4D8EAF7CC97BADD5A30541D2075910CABDE81889269B37`.
- Backups: `_backups/vigem_output_generation_shape_pre_20260822_1930`,
  `_backups/vigem_output_o5_runtime_stress_pre_20260822_1900`,
  `_backups/vigem_output_quiescence_split_pre_20260822_2030`,
  `_backups/vigem_output_o5_audits_pre_20260822_2000`,
  `_backups/vigem_output_o5_docs_pre_20260822_2100` and
  `_backups/vigem_output_o5_final_docs_pre_20260822_2145`.
- This artifact is local qualification evidence, not an immutable hardware
  candidate and not a user delivery. Long active-input soak and exact-candidate
  IROK MG75 Max/DrunkDeer G65 qualification remain mandatory. No Git/GitHub
  operation occurred.

## 2026-08-22 - R2-A/B1 AnalogProviderV2 semantic foundation

- Re-read the mega-roadmap and common pipeline/provider/precision/native audits
  before changing code. The current integer domain conflates USB usages, UAP
  extended controls and HallJoy mouse pseudo-codes; native input is still
  irreversibly quantized to milli.
- Compared enlarging numeric arrays, a big-bang rewrite, staged versioned
  adapters and realtime string/UUID identities. D-060 selects the staged route;
  the complete comparison is in
  `ANALOG_PROVIDER_V2_FOUNDATION_DESIGN_2026-08-22.md`.
- Added production-compiled POD `KeyIdentityV1`, `AnalogValueV2`,
  `AnalogDeviceV2`, `AnalogSampleV2` and `AnalogSnapshotHeaderV2`, plus strict
  snapshot and generation-transition validation. The contract names source,
  ownership, freshness, precision, completeness and capacity explicitly.
- Added portable negative tests for numeric namespace collision, USB Menu and
  Consumer media, UAP Fn/OEM1, invalid identities, adjacent 12-bit values,
  explicit legacy quantization, authoritative zero release, duplicate records,
  hidden truncation, generation regression and sample-only wake suppression.
  Complete 1/8/16/32-device models pass.
- The unified compiler-required native suite passed, then the full official
  build passed embedded dependency identity/signature gates and MSVC Release
  x64 with 0 errors and no unexpected warning.
- Local compile artifact: 8,518,144 bytes, SHA-256
  `A51E36B3B2A0E99B73D403D179D3FA28D448E53697D9FCD55C64F85409F1A51A`.
  It was not promoted or delivered; production input routing is unchanged.
- Backups: `_backups/r2_provider_contract_pre_20260822_2215` and
  `_backups/r2_provider_contract_docs_pre_20260822_2245`, both hash-verified.
  No Git/GitHub operation occurred.
- Next package: read-only UAP-to-V2 adapter, one acquisition per observed
  generation/tick and deterministic old/new XUSB equivalence before any route
  switch. Native and IPC migration remain later rollback-separated packages.

## 2026-08-22 - V14-19 GravaStar Mercury V75 6x21 native admission

- Statically unpacked the supplied unsigned `GravaStar75.exe` without executing
  it. Recovered exact V75, V75 Pro and V75 Lite firmware and proved the direct
  SparkPlayJoy framed 6x21 analogue path independently in ARM and RISC-V code.
- Compared a copied GravaStar backend, broad brand probing, a hard-coded layout,
  runtime digital correlation and a bounded exact-profile extension. D-062
  selects the last option: one existing transaction/session engine, one shared
  exact USB/board registry, and the unchanged full live proof before claim.
- Added exact profiles `1CA5:2201/16052201`, `1CA5:2202/16052202` and
  `1CA2:2201/2E022201`. Legacy SparkLink rejects the same profiles before open;
  native ownership now retains the actual selected VID/PID instead of the old
  Aula constant.
- Migrated the 6x21 active map, publication and diagnostics to 16-bit/common
  extended key capacity. Firmware `F001` maps directly to existing analogue Fn
  `0x409`; Menu remains USB `0065`; unknown vendor functions are filtered. No
  Windows keydown, UAP, guessed map or guessed scale participates.
- Expanded diagnostic position retention from 60 to all 126 positions. Added
  exact identity/board/path negatives, Fn snapshots and coverage, oracle,
  end-to-end, routing and source-linkage guards.
- Targeted tests, the complete native suite and 250,000 parser-fuzz iterations
  pass. A full official build exposed a nondeterministic pre-signalled
  supervisor-test assertion: Windows may reap a correctly Job-contained child
  before its first user-mode instruction records the entry marker. The oracle
  now accepts only a positive child containment observation or the parent's
  forced contained reap. Five standalone 1,008-generation repetitions and the
  subsequent official full run pass without overlap or survivors.
- Official production Release x64 passed with 0 errors and only the allow-listed
  ViGEmClient LNK4099. After the conclusive-diagnostic follow-up, the rebuilt
  artifact is 8,526,848 bytes, SHA-256
  `7545C40611A808F27142C219660368BA10AFA97AE5777876F15194FF762BAC7D`.
- The first dedicated diagnostic (`29E123...A19`) was rejected, not delivered:
  isolated runtime acceptance showed private/raw HID paths and unrelated device
  inventory in `HallJoy.log`. Final-sink redaction now covers every diagnostic,
  and the V75 one-file bridge keeps only targeted 6x21 backend lines.
- The privacy-safe `8FEB8E...BD50` diagnostic was then superseded because its
  separate branch events could still leave a non-programmer without one
  decisive answer. Three approaches were reviewed: infer the outcome offline,
  keep adding ad-hoc branch lines, or maintain monotonic proof/failure state and
  emit one final verdict. The last was selected because it covers early
  SetupAPI/resource/thread failures as well as protocol/runtime failures without
  adding HID traffic, runtime learning or a digital-input dependency.
- Rechecked the extracted binaries and updater configuration byte-for-byte:
  exact VID/PID/board identities, `FFA0:0001`, and all three recorded firmware
  hashes still agree. Diagnostic-only enumeration now safely records an unknown
  `1CA2`/`1CA5` identity or GravaStar-branded SetupAPI identity but never opens,
  claims or publishes it. Each run emits one conclusive `diagnostic.verdict`:
  either `analog_stream` or an explicit `send_log` result with the deepest proof
  and first failure.
- The current one-file diagnostic is 8,679,936 bytes, SHA-256
  `3F987F6CFCBD38396D8E3DE30237A75183CC7D5D7884DFB36C05ABB399548AD0`.
  Its exact-artifact smoke requires the verdict and passed hidden startup,
  `WM_CLOSE`, exit code 0, complete lifecycle, no NUL/private/raw/unrelated
  inventory, stable hash and zero surviving HallJoy processes. Full native
  checks, the 250,000-iteration parser fuzz and official production rebuild
  passed; production contains no continuous diagnostic markers.
- Backups: `.analysis/backups/gravastar_v75_admission_20260822_1615`,
  `.analysis/backups/sparkplayjoy_6x21_extended_fn_20260822_1627`,
  `.analysis/backups/gravastar_v75_diagnostic_packaging_20260822_1642` and
  `.analysis/backups/gravastar_v75_docs_20260822_1640`,
  `.analysis/backups/gravastar_diagnostic_privacy_pre_20260822_1700` and
  `.analysis/backups/process_supervisor_oracle_pre_20260822_1720`, plus
  `.analysis/backups/gravastar_conclusive_diagnostic_pre_20260822_1745`.
- Status is Implemented / physical validation pending. No physical V75 claim is
  made until one returned trace proves HID shape, board/map/scale, ordinary and
  special-key travel, releases, sustained polling and reconnect. No Git/GitHub
  operation occurred.

### Physical V75 trace correction

- Audited returned `HallJoy (9).log` packet-by-packet. Although it was copied
  while the process was still active (4.969 seconds, therefore no
  `session.end`/`diagnostic.verdict`), it contains enough complete proof cycles
  to establish the real Windows device: `1CA5:2201`, `FFA0:0001`, 65-byte input
  and output, board `16052201`, App `V1.0.8`, 5 um precision, 3500 um range,
  physical/default/live map counts 79/78/79, `F001` Fn and valid non-zero direct
  travel responses.
- Found the publication blocker in HallJoy, not in the keyboard. The generic
  6x21 firmware predicate required the Aula model bytes `C0/01/00`; the physical
  V75 legitimately reports `00/04/00`. All later proof stages were valid, but
  mismatch bit `00000001` prevented claim/publication and triggered eight proof
  attempts/336 protocol-report lines in under five seconds.
- Compared broad signature relaxation, a V75 byte exception, removing firmware
  validation, and an exact-known-board compatibility policy. Selected the last:
  exact registered VID/PID chooses its expected nonzero board ID and still must
  pass structured descriptors plus the complete scale/map/stability/travel
  proof; unregistered family candidates retain the strict Aula signature.
- Deterministic semantic rejection now waits for an actual device-change event
  rather than repeating the full HID proof each second. Only transient
  transport/claim failures retain timed retry.
- Added a byte-exact physical V75 sync regression and an end-to-end transport
  fixture with the returned map, scale, `F001` and travel data. Static audit,
  full native/portable suite, 250,000 parser-fuzz iterations and the official
  MSVC rebuild pass. Production: 8,527,872 bytes, SHA-256
  `3837CB510B64A3E2C9685BB2A7188DC8B47CD305B8DD6A5F1F34FB749A9D0F2D`.
  Corrected diagnostic: 8,680,960 bytes, SHA-256
  `E71EDB6BA9734F6019F5C22EAF48ED6DD299EA91D1F00E4D564705D66312C3D5`.
- The corrected diagnostic passed exact-artifact startup/privacy/`WM_CLOSE`
  smoke with exit 0, a complete 19,438-byte log and no survivor. Production
  smoke passed with no continuous diagnostic/crash file. Backup:
  `.analysis/backups/gravastar_physical_sync_fix_pre_20260822_1848`. No
  Git/GitHub operation occurred.
- Remaining physical gate is intentionally narrower: analogue publication,
  smooth/released/simultaneous input, sustained polling and reconnect on the
  corrected EXE. The user does not need to repeat map discovery.

## 2026-08-22 - Physical V75 analogue PASS and ViGEm protected-owner correction

- Audited returned `HallJoy (10).log`: 90,844 bytes, 437 lines, SHA-256
  `18A91FB5067595680B3922E76744208E14EAFF750C5C12ED29AC39070DD23556`.
  It is a partial 99.922-239.735 s slice without header or handled finalization,
  but contains conclusive independent input and output evidence.
- Corrected V75 admission is physically successful. Twenty-eight health windows
  reach 76,773 matrices with zero failures at 308.5-342.1 Hz, 108-3500 um
  intermediate travel, release-to-zero transitions, two simultaneous active
  keys and nine observed HIDs. This closes analogue publication, smooth travel,
  release, simultaneous-input and sustained-polling gates; unplug/reconnect and
  unpressed special-key coverage remain pending.
- The same trace reopened `HJ-V14-P0-006` in the F3 process route. After 46
  acknowledged output publications, generation 1 ended `ReapFailed` with Win32
  6 at 135.641 s; the parent-owned child PID was restart-unsafe. The old UI then
  emitted 218 rebuild attempts, all blocked, while matrix input stayed healthy.
- Compared relying on private raw handles, duplicating them, reopening by PID,
  whole-app restart and kernel-protected sole ownership. D-064 selects protected
  ownership: process and job handles set `HANDLE_FLAG_PROTECT_FROM_CLOSE`, are
  non-copyable, retain exact PID evidence, and are unprotected only by the sole
  owner for confirmed close.
- Added an illicit-close Windows regression. `CloseHandle` through the protected
  value returns error 6, protection and waitability remain intact, and owner
  close succeeds. The complete 1,008-generation supervisor matrix passes with
  protected process/job handles, PID match, no overlap and no survivor.
- Replaced the infinite watchdog retry with one-shot truthful containment. A
  failed Stop or Start emits one `watchdog.recovery_blocked` record containing
  state, last outcome/error, unsafe generation/flags and required user action;
  later ticks perform no new lifecycle or log action.
- Full static/portable/native tests pass. Exact same-image output tests pass:
  self-host 9 generations/6 boundaries, real ViGEmBus 4-pad apply-neutral-remove,
  and runtime stress 100,000+ publications/101 generations/50 topology changes/
  10 disable-enable cycles, all with zero survivor.
- The test-runner warning allowlist was made robust to MSVC's empty missing-PDB
  text while remaining narrower: only LNK4099 from the exact
  `ViGEmClient.lib(ViGEmClient.obj)` is accepted. The first simulator build had
  compiled successfully but the old text-dependent rule rejected that localized
  line; rerunning the actual exact-EXE gates after correction passed.
- New diagnostic: `build/gravastar-v75-diagnostic/HallJoy.exe`, 8,683,008 bytes,
  SHA-256 `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`;
  exact startup/privacy/WM_CLOSE smoke exits 0 with a complete log.
- New local production: `build/release/HallJoy.exe`, 8,529,408 bytes, SHA-256
  `B2C87A69F45EFBB046913B5B1DA4F7AB2D97AD33A3C0CC1494D91D06649238C0`;
  official dependency/UAP/embedded ViGEmBus/build gates and silent production
  smoke pass. Neither artifact is promoted yet; old V75 diagnostic `E71ED...`
  is DO NOT DISTRIBUTE.
- Backups: `.analysis/backups/vigem_protected_generation_handles_pre_20260822_2005`
  and `.analysis/backups/vigem_warning_allowlist_pre_20260822_2018`. No
  Git/GitHub operation occurred.

## 2026-08-22 - R2-B2a read-only UAP Provider V2 adapter and exact DLL gate

- Re-read the mega-roadmap, D-060/D-061 and the provider design before touching
  the route. The partially prepared source already retained full Soup-key
  values, exported V2, carried it in shared IPC V11 and exposed a parent capture,
  while intentionally leaving `Backend_Tick` unchanged.
- Audited that work instead of declaring it complete. Found two missing proofs:
  the pinned-owner list silently forgot registry demand beyond eight devices,
  and legacy/V2 child exports are separate calls, so final same-generation XUSB
  equivalence was not yet established.
- Added `required_count` to lock-safe owner pinning and a production-linked UAP
  projection core. Snapshot capacity now describes the actually captured
  immutable window, not an oversized caller buffer; internal device/sample
  truncation is explicit and validator-consistent.
- Added deterministic behavioral coverage for two separate devices, ordinary
  USB HID, Menu, Consumer media, UAP Fn, exact zero release, duplicate/NaN
  rejection and exact ordinary-HID legacy projection. The oversized-buffer test
  proves a 2-of-12 internal capture remains truncated.
- Extended the exact ABI1 runtime gate to call the new export before initialise,
  while active and after bounded unload. The freshly built DLL returned one
  real local device and 127 valid uniquely identified samples; all sizes,
  generations, capacity/completeness, namespaces and finite values passed.
- Full compiler-required native/static/portable checks pass. The official build
  rebuilt pinned Sun/Soup/UAP, passed the exact private ABI, embedded ViGEmBus
  signature/resource checks and MSVC Release x64 with 0 errors and no unexpected
  warnings. Production startup/WM_CLOSE smoke passed with no continuous log or
  crash report.
- Local production artifact: 8,529,920 bytes, SHA-256
  `34F062D7770110F1FD72A45AF10206628BFD8673CA33791C530FD022C4CB91B5`.
  Embedded production UAP ABI1: 264,704 bytes, SHA-256
  `5DD236DFC84772D7F07FB123CCF435EFB02E5CE00ABB889F6137C252EE1C39B3`.
- The active V75 hardware candidate was not overwritten and remains exactly
  `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`.
  No artifact from this R2 checkpoint was sent to a user.
- Backup: `.analysis/backups/r2_uap_projection_gate_pre_20260822_2110`.
  Evidence: `docs/stability/tests/V14_R2_B2A_UAP_PROVIDER_V2_ADAPTER_2026-08-22.txt`.
  No Git/GitHub operation occurred.
- Next package: obtain/derive old and V2 views from one immutable generation and
  compare the actual configured bindings, curves and every XUSB report field in
  read-only shadow. Production routing cannot switch before that passes.

## 2026-08-22 - R2-B2b same-generation UAP dual capture

- Re-read the R2 design, roadmap, decisions, validation ledger and risk register.
  Compared retrying two exports, replacing legacy immediately, and one pinned
  dual export with rollback. D-066 selects the pinned dual export and retains
  legacy only as the still-qualified production fallback.
- Extended the exact private ABI gate to validate both returned views
  independently. Its first run correctly failed because discovered topology
  could expose constructor-zero data as fresh (`generation=0`, `timestamp=0`).
  The export now refuses publication until a real hardware acquisition exists.
- The final exact ABI1 run returned one local device and 127 samples. Every one
  of the 256 dense values was independently reconstructed from namespaced V2
  ordinary-HID samples and matched; inactive pre-init/post-unload calls reject.
- Integrated the mandatory dual export into the isolated child. IPC is V12 and
  includes explicit dual-coherence/failure telemetry. The child validates V2,
  dense metadata, finite values, active count and the full ordinary-HID
  projection before publishing both views in one transaction. A violation
  fails V2 closed and uses the old export only to keep current production input
  alive; no Windows digital event participates.
- Added an exact-image hidden self-test. It starts the actual analog child and
  requires the parent to capture a coherent authoritative non-empty V2 snapshot
  within a bound. The final production image passed:
  `UAP_PROVIDER_V2_EXACT_EXE_DUAL_CAPTURE=PASS`.
- Full compiler-required static/portable/native checks pass. Two naive shutdown
  audits initially mistook the early self-test cleanup for the final relaunch
  transaction; they now scope their order proof from the explicit final
  `App_TakeRelaunchRequest()` transaction instead of relying on first textual
  occurrence. The production contract was not weakened.
- Official MSVC Release x64 passed with 0 errors and only the allow-listed
  external ViGEmClient LNK4099. Ordinary ten-second startup/WM_CLOSE passed with
  no continuous log or crash report. The smoke runner now distinguishes the
  exact artifact path, so another already-running HallJoy build is neither
  reported as this build's survivor nor terminated.
- Final local production: `build/release/HallJoy.exe`, 8,535,552 bytes, SHA-256
  `CE26D65EFBF0DBB354B36AF8FC6BDD13B7C29F369775957545003462C18987F2`.
  ABI0 is 393,216 bytes/SHA-256
  `3F2A0B0CE3E83CA3FD82360F9C8397D5A863D9AE78E68A9F3E3C8E9EFF32FD35`;
  ABI1 is 265,728 bytes/SHA-256
  `7F9B0818A04D5D080C877A9EE952E0F89A2EC45FD4B1E7545F8ABC3BE7706859`.
- The V75 user artifact remains exactly 8,683,008 bytes/SHA-256
  `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`.
  It was not overwritten or terminated by this package. No new R2 artifact was
  sent to a user.
- Backups:
  `.analysis/backups/r2_uap_dual_view_pre_20260822_2140`,
  `.analysis/backups/r2_uap_dual_host_pre_20260822_2150`,
  `.analysis/backups/production_smoke_scope_pre_20260822_2200`,
  `.analysis/backups/r2_uap_exact_exe_capture_pre_20260822_2210`,
  `.analysis/backups/factory_reset_gate_scope_pre_20260822_2215`,
  `.analysis/backups/mad68_shutdown_gate_scope_pre_20260822_2218` and
  `.analysis/backups/r2_uap_dual_capture_docs_pre_20260822_2235`.
  Evidence: `docs/stability/tests/V14_R2_B2B_UAP_SAME_GENERATION_DUAL_CAPTURE_2026-08-22.txt`.
  No Git/GitHub operation occurred.
- Next package: use the same immutable dual capture to run the user's actual
  bindings, curves and complete XUSB report builder in read-only shadow. Compare
  every field and do not submit the shadow report or switch `Backend_Tick`.

## 2026-08-22 - R2-B2c configured XUSB builder foundation

- Audited the real `Backend_Tick` report path before adding a shadow. Found that
  the old `BuildReportForPad` mutates global Snappy Joystick/Last Key Priority
  state and acquires/updates mouse filtering internally. Calling it twice would
  create a false oracle and could perturb the report sent to the game.
- Compared double-call, raw-only comparison, a copied V2 builder and one shared
  explicit-state builder. D-067 selects the shared production component.
- Added `configured_xusb_builder` with immutable configuration and filtered input
  values plus caller-owned conflict state. It owns all XUSB fields but cannot
  access providers, global bindings/settings, mouse acquisition or ViGEm.
- Migrated the qualified legacy report path first. Existing bindings and input
  values are captured into the new arguments, the builder runs once per pad and
  its result is converted to the unchanged production `XUSB_REPORT`. No V2
  shadow result is produced or submitted in this checkpoint.
- Added production-linked portable/static gates for all buttons/triggers/sticks,
  extended Fn binding, mouse merge, thresholds, Snappy/LKP analog retrigger,
  independent paired state and localized divergence. Full static/portable suite
  passes.
- Official MSVC Release x64 passed with 0 errors and only the allow-listed
  external ViGEmClient LNK4099. Exact UAP child/IPC/parent capture and ordinary
  ten-second startup/WM_CLOSE both pass with no continuous log or crash report.
- Intermediate local production: 8,536,576 bytes, SHA-256
  `35714CD31518CC853DD4484259C4F8BF32C450FD20A9F7F465AFD3DF38AEAD05`.
  It is not promoted. The active V75 diagnostic remains 8,683,008 bytes/SHA-256
  `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`.
- Backups: `.analysis/backups/r2_configured_xusb_builder_pre_20260822_2250`
  and `.analysis/backups/r2_configured_xusb_builder_docs_pre_20260822_2300`.
  Evidence: `docs/stability/tests/V14_R2_B2C_CONFIGURED_XUSB_BUILDER_FOUNDATION_2026-08-22.txt`.
  No Git/GitHub operation occurred.
- Next: add one parent transaction returning dense compatibility plus V2, merge
  identical native ownership into both raw maps, apply the same curve/config/
  mouse snapshot, run a separate shadow state and record field-level mismatch
  counters. Only the qualified report may reach ViGEm.

## 2026-08-22 - R2-B2d neutral controller frame and exact XUSB adapter

- Rechecked the R2-B2c static, production-linked and documentation gates before
  changing the output boundary; all passed.
- Compared three scopes: retain XUSB as the canonical mapped state, introduce a
  universal touch/motion/arbitrary-axis graph now, or extract the complete
  standard controls HallJoy already emits. D-068 selects the third option.
- Added internal `VirtualControllerFrameV1` with semantic standard-gamepad
  buttons, two triggers and four sticks. The historically named
  `configured_xusb_builder` now returns this neutral value; no provider, global
  settings, mouse acquisition or output I/O entered the component.
- Added `xusb_output_adapter` as the sole owner of Xbox button masks and exact
  `XusbReport` conversion. `Backend_Tick` still publishes only the qualified
  XUSB path. No DS4 runtime, Provider V2 shadow or route switch was added.
- Extended production-linked regressions to prove neutral mapping and exact
  XUSB conversion for every current button, both triggers and all four axes.
  `CONFIGURED_XUSB_BUILDER_TEST=PASS ... neutral_frame=1
  exact_xusb_adapter=1`; the complete compiler-required suite passes.
- Official MSVC Release x64 passed with 0 errors and only the allow-listed
  external ViGEmClient LNK4099. The exact-image UAP dual-capture gate and an
  ordinary ten-second startup/WM_CLOSE smoke passed without a continuous log,
  crash artifact or remaining exact-image process.
- Intermediate local production: `build/release/HallJoy.exe`, 8,536,576 bytes,
  SHA-256
  `21DDA421768F4320744BD052F1C9D85AB3B3E2663CBDAEC04015FCB64D56948D`.
  It is not promoted. The V75 diagnostic remains exactly 8,683,008 bytes,
  SHA-256
  `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`.
- Backups: `.analysis/backups/neutral_controller_frame_pre_20260822_2350` and
  `.analysis/backups/neutral_controller_frame_docs_pre_20260822_2359`.
  Evidence:
  `docs/stability/tests/V14_R2_B2D_NEUTRAL_CONTROLLER_FRAME_2026-08-22.txt`.
  No Git/GitHub operation occurred.
- Next: make `Backend_Tick` consume one parent transaction containing the
  same-generation dense and Provider V2 views, then build a read-only shadow
  from one curve/config/mouse snapshot with independent state. Only after
  bounded field-level equality evidence may a provider route switch be planned.

## 2026-08-22 - R2-B2e one parent UAP tick capture

- Traced the actual production UAP reads before adding shadow evaluation and
  found a deeper coherence defect: production did not enable its diagnostic
  full-buffer preference, so separately bound keys could be read from adjacent
  host publications inside one controller frame.
- Compared counter-matched separate reads, a V2-only side capture, immediate V2
  replacement and one parent dense+V2 transaction. D-069 selects the last
  option without removing the qualified dense compatibility route.
- Added `uap_parent_snapshot` and `AnalogHostClient_CaptureTickSnapshot`. One
  stable sequence copies publication metadata, aggregate dense, per-device
  dense and optional V2. The validator is shared by child and parent; the
  parent also proves aggregate max merge, active counts and finite range.
- `Backend_Tick` now takes one parent snapshot and uses its dense compatibility
  plane for every standard UAP key in that tick. Provider V2 stays attached to
  the immutable snapshot but is not consumed by mapping. A complete capture
  failure retains only the old analogue read; no digital event participates in
  correlation, learning or fallback selection.
- Added a production-linked negative test for corrupt aggregate, corrupt device
  plane versus V2, non-finite input and valid dense-without-V2 fallback.
  `UAP_PARENT_SNAPSHOT_TEST=PASS one_publication=1 dense_fallback=1
  aggregate_checked=1 dual_checked=1`; the complete native suite passes.
- Official MSVC Release x64 passed with 0 errors and only the allow-listed
  external ViGEmClient LNK4099. Exact real-child UAP parent capture and ordinary
  ten-second startup/WM_CLOSE pass with no continuous log, crash artifact or
  exact-image survivor.
- Intermediate local production: `build/release/HallJoy.exe`, 8,637,952 bytes,
  SHA-256
  `C60CA71237663766354CC6E34B562BCE27E71A0DA446741C4BC969F8A13111DF`.
  It is not promoted. The protected V75 diagnostic remains exactly 8,683,008
  bytes/SHA-256
  `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`.
- Backups: `.analysis/backups/r2_parent_dual_capture_pre_20260823_0015` and
  `.analysis/backups/r2_parent_dual_capture_docs_pre_20260823_0045`.
  Evidence:
  `docs/stability/tests/V14_R2_B2E_PARENT_UAP_TICK_CAPTURE_2026-08-22.txt`.
  No Git/GitHub operation occurred.
- Next: derive the V2 shadow raw map from this exact tick capture, merge the
  same native ownership, capture bindings/curves/mouse once, run independent
  builder state and retain bounded field-level mismatch evidence. Shadow output
  must never reach ViGEm.

## 2026-08-23 - R2-B2f live read-only configured Provider V2 shadow

- Re-audited the possible promotion paths before changing production. D-070
  rejects an immediate V2 switch, raw-only equality, a copied builder and every
  form of digital-key correlation. The selected shadow compares final neutral
  controller frames while preserving the qualified dense route as rollback
  oracle.
- Added `provider_v2_controller_shadow`. It projects explicit USB keyboard-page
  and supported UAP extended identities, keeps ownership separate from value,
  merges duplicate devices by maximum and leaves consumer/semantic namespaces
  unaliased. Native extended ownership remains authoritative; there is no
  digital-input parameter.
- `Backend_Tick` now uses one native read cache, one configuration snapshot, one
  curve definition/generation and one mouse sample for qualified and shadow
  frames. The same production builder runs with separate SOCD/LKP state.
  Unavailable V2, digital fallback or curve mutation disqualifies the tick and
  exactly resynchronizes the shadow history.
- Added bounded atomic telemetry for availability, eligible ticks, matched and
  mismatched reports, three skip causes, seven field mismatch counts and the
  latest mismatch pad/mask/sample generation. Continuous production logging
  remains disabled.
- Full-frame comparison covers the semantic button mask, both triggers and all
  four stick axes. Only `frames.qualified` is converted by the XUSB adapter and
  submitted; the shadow cannot cross the ViGEm boundary.
- Added production-linked portable and static gates. The new portable result is
  `PROVIDER_V2_CONTROLLER_SHADOW_TEST=PASS identity_projection=1 owned_zero=1
  multi_device_max=1 native_arbitration=1 all_fields=1`. The complete static and
  portable suites pass, including parent-snapshot and configured-builder tests.
- Official MSVC Release x64 passed with 0 errors and only the allow-listed
  external ViGEmClient LNK4099. Exact production-image real-child capture and a
  sequential ten-second startup/WM_CLOSE smoke pass with no continuous log,
  crash report or process survivor. An earlier parallel invocation caused one
  test harness to see the other smoke process; the sequential rerun passed and
  no product defect was inferred from that orchestration mistake.
- Intermediate local production: `build/release/HallJoy.exe`, 8,643,584 bytes,
  SHA-256
  `9D3376EF6247F73283140F27B472AB22FA74435F112E9B2B416901CC3E394F20`.
  It is not promoted. The protected V75 diagnostic remains exactly 8,683,008
  bytes/SHA-256
  `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`.
- Backups: `.analysis/backups/r2_provider_v2_shadow_pre_20260823_0115` and
  `.analysis/backups/r2_provider_v2_shadow_docs_pre_20260823_0230`. Evidence:
  `docs/stability/tests/V14_R2_B2F_LIVE_PROVIDER_V2_SHADOW_2026-08-23.txt`.
  No Git/GitHub operation occurred.
- The implementation/local gate is complete, but sustained physical
  zero-mismatch has not been observed through an external evidence path. Next:
  add an explicit bounded diagnostic export/snapshot for these counters, run it
  on representative UAP hardware and only then decide whether to promote V2.

## 2026-08-23 - R2-B2g fail-closed Provider V2 physical qualification

- Re-audited what the R2-B2f counters could honestly prove. Found that idle
  zero-mismatch, repeated ticks of one UAP generation, counter resets and stale
  crash output could all create misleading evidence if counters were exported
  directly. D-071 compares four evidence mechanisms and selects one opt-in
  ordinary HallJoy lifecycle with a transactional summary.
- Added the first pure verdict model plus portable regressions. Its initial
  policy included a fixed duration threshold alongside normal exit, one backend
  init, 1,000 eligible ticks/matches, 100 unique generations, configured-field
  activation/release, bounded unavailability and zero fallback/mutation/
  mismatch. The duration requirement was later rejected and removed in the
  correction recorded below; all other fail-closed requirements remain.
- The first implementation tracked activation from the final qualified frame.
  Before promotion it was rejected because mouse/native activity could falsely
  exercise a V2 field. Coverage is now gated by `providerRaw` ownership/value
  before consulting the mapped shadow field; digital input is absent. A second
  audit found cross-pad aliasing, so coverage is now 28 independent bits for
  four pads times seven fields.
- A final proof audit rejected the edge-based release tracker: dropping below
  its activity threshold could count partial travel as release. Activation now
  requires Provider V2 raw >=0.5 plus a non-neutral shadow field, then latches
  pending until both raw bindings and the mapped field are neutral. The portable
  regression reports `partial_travel_not_release=1`.
- Added `provider_v2_qualification_report`. It replaces stale evidence with an
  atomic flushed `INCOMPLETE` at startup and finalizes only after the normal app
  and logger shutdown path. Poisoned/crashed exits cannot finalize. No file I/O
  occurs in the realtime backend; the user returns one small text report.
- Added a dedicated MSVC opt-in configuration, exact builder and exact smoke.
  The final short smoke produced 1,836 eligible/matched reports, 250 unique UAP
  generations, zero mismatches/unavailable/fallback/mutation ticks and correctly
  remained `INCOMPLETE` after 2,562 ms with incomplete activation/release. No
  temp file or continuous diagnostic log remained.
- Qualification artifact: `build/provider-v2-qualification/HallJoy.exe`,
  8,653,312 bytes, SHA-256
  `0357A68E60E3DFDDC496DAC7D514C862A873C6FC3EC47AC56AF50F9D4DE9A8BB`.
- Rebuilt the ordinary production image. Full native/static/portable checks,
  MSVC Release x64, embedded ViGEm signature/hash test, exact UAP V2 dual capture
  and sequential ten-second startup/WM_CLOSE pass. The production linked image
  and runtime directory contain no qualification report marker/file.
- Local production: `build/release/HallJoy.exe`, 8,644,608 bytes, SHA-256
  `36A547F0FFAF3F4BD6DCE92EFF94F6975E808D1D87A727244356D2D65DF39B16`.
  It is not promoted and still publishes dense compatibility output only.
- The protected V75 diagnostic remains exactly 8,683,008 bytes/SHA-256
  `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`.
  Three unrelated running v1.4 HallJoy processes were observed and left alone.
- Backup:
  `.analysis/backups/r2_provider_v2_qualification_pre_20260823_0330` and
  `.analysis/backups/r2_provider_v2_qualification_docs_pre_20260823_1130`.
  Evidence:
  `docs/stability/tests/V14_R2_B2G_PROVIDER_V2_QUALIFICATION_2026-08-23.txt`.
  No GitHub/network or version-control mutation was performed for this package;
  the locked UAP overlay verifier ran its normal read-only local check.
- This entry's original next step was a representative hardware rerun under the
  initial fixed-duration policy. It is superseded by the physical evidence and
  duration-policy correction below; no additional R2-B2g user run is required.
  Production remains unswitched pending a separate route-promotion decision.

## 2026-08-23 - R2-B2g physical evidence closure and duration-policy correction

- Audited two finalized physical reports from the same schema-1 qualification
  EXE (`0357A68E...4DE9A8BB`). Report hashes are `BE0084B5...614A51` and
  `DE1380EE...62620`. The first contributes 35,744 matched frames/8,776 unique
  generations and complete `0x78` activation/release; the second contributes
  8,135/2,001 and `0x26`, explicitly including both LT and RT.
- Their direct evidence aggregates to 43,879 matched frames, 10,777 unique
  generations and configured/activated/released union `0x7E`, with zero field
  mismatches, unavailable ticks, digital fallback or curve mutation. This closes
  representative physical configured Provider V2 equality without digital
  correlation, runtime learning or another user run.
- Rejected the original fixed 60-second verdict threshold after review. A
  59.9/60.0-second boundary establishes neither equality nor stability, while no
  finite test can prove a bug will not appear after a week. Duration remains in
  the report for context; stability is a separate soak/reconnect/fault gate.
- Bumped the qualification report to schema 2 with
  `duration_is_informational=1`, removed duration from the pure verdict model and
  added static/model/build/smoke regressions that reject reintroducing it.
- Focused qualification gates and the full static/portable/native suite pass.
  The schema-2 exact smoke finalized `INCOMPLETE` after 2,547 ms only because no
  fields were activated/released: 2,039 matched frames, 504 unique generations,
  zero mismatches/skips and evidence gaps `0x300`, not the old duration bit.
- Corrected qualification image: 8,653,312 bytes, SHA-256
  `60C2E205C61CF6F61EE6216DB46A825121A34975924D35D2C9A96FC85473ED71`.
- Full official production rebuild passed with 0 errors and only allow-listed
  ViGEmClient LNK4099. Exact dual capture and sequential ten-second production
  smoke pass; no continuous/crash/qualification report or temporary file was
  created. Production image: 8,644,608 bytes, SHA-256
  `9E7FD20D41E441AF50D42A12FB9C44AEDB917C29D5A69FB4CA0778678DC63166`.
- The production route is still dense compatibility and was not promoted
  automatically. The next R2 decision is explicit Provider V2 route promotion;
  long-duration stability remains an independent release gate.
- Protected V75 diagnostic reverified unchanged at 8,683,008 bytes/SHA-256
  `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`.
  Unrelated HallJoy processes were never terminated or modified.
- Backups:
  `.analysis/backups/r2_b2g_duration_policy_pre_20260823_1150` and
  `.analysis/backups/r2_b2g_physical_docs_pre_20260823_1200`. No GitHub/network
  or version-control mutation was performed; the overlay verifier's local check
  remained read-only.

## 2026-08-23 - R2-B2h negotiated-capacity UAP producer

- Re-audited the post-qualification promotion boundary before selecting a route.
  The deeper blocker was not configured mapping equality but a fixed eight-owner
  window inside the UAP Provider V2 producer plus the separate eight-slot live
  IPC. Immediate selection and a larger constant were rejected; D-072 records a
  staged producer/data-plane/route sequence.
- Added exact owner-count query and caller-storage pinning. Reusable owner,
  projection and mutex storage grows before locks; a move-only lease clears every
  shared owner reference, and a caller-provided lock view unwinds in reverse.
  Ownership generation is checked around complete capture; four topology retries
  and 4,096-device/1,048,576-sample defensive ceilings fail closed.
- Added static and portable regressions that capture 12 owners, verify lease
  cleanup, project a complete 12-device/60-sample authoritative generation and
  preserve the explicit 2-of-12 truncation rejection oracle.
- Updated the exact private UAP ABI gate to allocate Provider V2 and dual buffers
  from zero-capacity demand. Exact DLL runtime reports
  `dual_view_equivalent=1 negotiated_capacity=1`.
- Full static/portable/native checks pass. Official MSVC Release x64 passed with
  0 errors and only allow-listed external ViGEmClient LNK4099; embedded
  ViGEmBus/private-UAP gates passed. Exact dual capture and sequential ten-second
  startup/shutdown smoke passed with no continuous log or crash report.
- Release image: 8,650,240 bytes, SHA-256
  `15E6DADC75B367ABAA031BF5B239EC6DCD88A528AD8E797B5D9CAAE70FFE4509`.
  Protected V75 diagnostic reverified unchanged at 8,683,008 bytes/SHA-256
  `2C5A1E05D374FF0D5C1AC0F2A9663F328F0E1A54C43DEAA69AE46DA9AAEB098B`.
- Production route remains dense and the old fixed `SharedState` is not called
  solved. R2-B2i is the next package: a separately owned, capacity-negotiated,
  parent-read-only Provider V2 data plane. R2-B2j follows with route selection
  and dense runtime fallback removal.
- Backup: `.analysis/backups/r2_b2h_negotiated_uap_pre_20260823_1210`.
  No GitHub/network or version-control mutation was performed.

## 2026-08-23 - Mandatory non-release-blocking firmware/HID testbed roadmap

- Recorded the requested reusable virtual-keyboard program without changing the
  release-blocking order: R2-B2i remains the next HallJoy package and R2-B2j
  remains the separate Provider V2 route decision.
- Rejected both a one-off ND75 protocol mock and a full-board-first big bang.
  D-073 selects hash-pinned layered profiles with explicit replay/model/original-
  code/full-emulation evidence levels, a thin Windows VHF transport and all
  firmware execution in user mode.
- Added staged `LAB-01..LAB-06` gates for catalog/provenance, Windows virtual
  HID, the exact X86HERGB hybrid slice, automated HallJoy scenarios, additional
  firmware families and optional reset-to-main emulation.
- The testbed is mandatory long-term work but cannot block the next release,
  relax production admission or replace physical qualification. Unknown behavior
  fails closed and no digital key event participates in analogue identity.
- This was documentation-only. Production/test/build code and current artifacts
  were not changed.
- Hash-verified backup:
  `.analysis/backups/firmware_virtual_hid_testbed_plan_pre_20260823`.

## 2026-08-23 - R2-B2i-a split Provider V2 data-plane layout

- Re-read the engineering method, provider architecture and capacity audits,
  then compared four IPC implementations. A larger fixed array preserves the
  bug; resizing the bidirectional monolith preserves excessive rights and
  couples lifecycle; per-frame pipe/RPC adds copies/backpressure. D-074 selects
  a separate parent-owned, parent-read-only, child-writable double-buffered data
  mapping with exact capacity and generation-bound replacement.
- Added `provider_v2_data_plane_layout.h/.cpp`, compiled into production but not
  referenced by `backend.cpp`. The contract uses a 128-byte mapping header, two
  64-byte-aligned commit slots, checked offsets/sizes, exact locally recalculated
  layout, generation/nonce/token binding and the existing authoritative Provider
  V2 semantic validator.
- Added a portable old-bug/regression executable and static guard. Deterministic
  capacities 0/1/8/12/32/limit pass; a complete 12-device/60-sample generation
  passes. Overflow/ceiling, corrupt offsets/stride, odd or uncommitted sequence,
  stale generation/nonce/token, false completeness and truncation fail closed.
  The guard also asserts that production has not selected the plane and that the
  fixed live arrays still remain for B2i-b/c.
- Focused portable result:
  `provider_v2_split_plane_layout=pass negotiated_devices=12 negotiated_samples=60 parent_payload_route_selected=0`.
  The complete `run_native_backend_checks.py --require-compiler` suite passed.
- Official MSVC Release x64 passed with 0 errors and the one allow-listed
  external ViGEmClient LNK4099; exact private UAP ABI and embedded installer
  checks passed. Exact release dual capture and startup/WM_CLOSE smoke passed
  with no continuous diagnostic log or crash report.
- Local checkpoint image: `build/release/HallJoy.exe`, 8,650,240 bytes,
  SHA-256
  `920CBFB75229F9820FFB20C30CC11A3ED6940EEEA16D66E23E926099F087F5FE`.
  It is not promoted and does not require a user run; dense output is unchanged.
- B2i-a is complete, not all of B2i. B2i-b next owns Windows mapping rights,
  exact demand/restart and reaped-before-resize lifecycle. B2i-c then migrates
  the live shadow/removes fixed V2 payload arrays; only B2j may select V2.
- Source backup:
  `.analysis/backups/r2_b2i_a_layout_pre_20260823`. Documentation backup:
  `.analysis/backups/r2_b2i_a_docs_pre_20260823`. No GitHub/network or
  version-control mutation was performed.

## 2026-08-23 - Provider V2 local-route timing correction

- Reconsidered the policy of retaining dense output through every B2i package.
  The owner correctly identified that a route first selected at release
  promotion would receive too little ordinary local testing.
- Immediate selection was rejected because the current Provider V2 shadow still
  travels through the fixed eight-device `SharedState`; it would exercise the
  semantic builder but not the new mapping, access rights, capacity negotiation
  or restart lifecycle. Waiting until B2j was also rejected because it delays
  actual-use failures until release qualification.
- D-075 selects the end of B2i-c as the boundary. Once the split live plane and
  all B2i-c gates pass, the normal local engineering build uses V2 output for its
  entire lifetime. Missing/invalid V2 fails the affected source closed and may
  not silently substitute dense data.
- Dense remains implemented, compiled and directly tested, but is selected only
  by an explicit immutable build property for a separately named emergency/user
  artifact. No runtime/UI/persisted switch is introduced, preventing failures
  from being hidden and stateful route history from changing mid-process.
- B2j is now explicitly user-release promotion/removal of the already locally
  exercised route, not its first use. No source/build/artifact changed in this
  documentation-only correction.
- Hash-verified backup:
  `.analysis/backups/r2_b2i_route_policy_pre_20260823`. No GitHub/network or
  version-control mutation was performed.

## 2026-08-23 - R2-B2i-b Windows data plane and physical-test isolation

- Implemented the separate Windows Provider V2 mapping selected by D-074. The
  parent owns a non-inheritable `FILE_MAP_READ` handle/view; one transient
  inheritable read/write handle is placed in the explicit child handle list and
  closed in the parent immediately after `CreateProcessW`.
- Added bounded zero-capacity demand, exact growth, new plane generations and
  replacement only after confirmed child reap. Sufficient capacity is reused on
  shrink; malformed/inconsistent or over-ceiling demand fails closed.
- The initial exact-EXE capture crashed at the read-only commit recheck. Symbols
  localized it to `AnalogHostClient_CaptureProviderV2PlaneHeader`: an
  `InterlockedCompareExchange64` used as a read is actually a read-modify-write.
  Replaced it with an aligned volatile read plus barriers and added a static
  regression forbidding the write-requiring form.
- Added a real Windows parent/child regression using an explicit handle list. It
  rejects a forged numeric handle, rejects a child killed after the odd commit,
  blocks retire/replace while a reader lease exists, publishes capacities
  1/8/12/32/8/1 and proves zero surviving child/writer and stable handle count.
  Result: `PROVIDER_V2_WINDOWS_PROCESS_TEST=PASS ... surviving_writer=0
  surviving_child=0`.
- The full `run_native_backend_checks.py --require-compiler` suite exited 0.
  Direct production-config MSVC Release x64 rebuilt with 0 errors and only the
  existing allow-listed ViGEmClient LNK4099. Local image: 8,662,528 bytes,
  SHA-256
  `6F28B367A31170ADD172A53B142A5BFFE19C865A733212F4EBEA655BB91B014C`.
- Investigated the user's K4 HE matrix shift without stopping or restarting any
  of their three HallJoy processes. The custom full-matrix path reads four
  same-command `A9 31` packets without visible part/token identity, while
  `discardStaleReports()` does not prove a drained queue. Parallel physical-UAP
  ownership can therefore cross-contaminate frames despite the named
  transaction mutex. This matches the observation but remains an inference
  rather than packet-trace proof.
- D-076 selects narrow test isolation: official build runtime check, leaf ABI
  checker, exact dual-capture and production smoke now fail closed if any
  HallJoy is active. No product singleton, exclusive-HID change, digital-key
  correlation, automatic relearning or user-process termination was added.
- After every other HallJoy instance was closed, the direct candidate passed the
  exact physical zero-capacity -> controlled restart -> parent-read-only coherent
  commit gate. The official `tools/build.ps1` then passed the complete rebuild,
  private UAP ABI runtime gate and embedded ViGEm verification.
- The final packaged `build/release/HallJoy.exe` is 8,662,528 bytes with SHA-256
  `1EAAFAD31C19AE9C3DC75D37E6081BD0120CB156DCBA9C5C95719D6A6F0F4EFF`.
  It independently passed the exact physical dual-capture gate and a sequential
  10-second production startup/shutdown smoke. No diagnostic/crash artifact or
  surviving HallJoy process remained.
- B2i-b is complete. Dense production remains selected; B2i-c is next and owns
  the live-shadow migration plus removal of the fixed Provider V2 payload arrays.
- Backups: `.analysis/backups/r2_b2i_b_windows_plane_pre_20260823`,
  `.analysis/backups/physical_uap_test_isolation_pre_20260823` and
  `.analysis/backups/r2_b2i_b_docs_pre_20260823`. No GitHub/network or
  version-control mutation was performed.

## 2026-08-23 - R2-B2i-c live-shadow architecture start

- Re-read the complete split-plane design and the actual child publication,
  bridge, mapping lease, teardown and `Backend_Tick` paths before changing live
  routing. The current B2i-b path deliberately captures only a variable-plane
  header; its fixed and dynamic plugin calls are adjacent but not one capture.
- Compared a realtime lock/vector copy, direct mapping lease, defensive-maximum
  arrays and a bridge-owned snapshot broker. D-077 selects the broker: three
  preallocated dynamic slots, one bridge writer, nonblocking realtime leases and
  revoke/drain/resize outside realtime.
- B2i-c will also make the child use one dynamic dual plugin capture after the
  zero-capacity demand restart. The plane token/slot will be committed inside the
  dense seqlock, so parent coherence no longer relies on timing proximity.
- Hash-verified pre-change backup:
  `.analysis/backups/r2_b2i_c_broker_pre_20260823`. No GitHub/network or
  version-control mutation was performed.

## 2026-09-05 - UAP event-driven reconnect repair and release-audit correction

- Re-read the engineering method, Provider V2 design/decision chain and the
  actual parent supervisor before editing. D-078 records the alternatives:
  periodic UAP discovery and live plugin re-enumeration are rejected; a real
  `WM_DEVICECHANGE` requests only a supervisor-owned isolated-child replacement.
- Added monotonic coalesced device-refresh generation state to the analog-host
  client. The UI thread only queues it. The supervisor invalidates the snapshot,
  terminates/reaps its job-contained child, and launches a fresh child through
  the existing negotiated Provider V2 plane path. `UAP_DISABLE_HOTPLUG=1`
  remains enabled; no periodic HID scan was restored.
- Corrected the independent `native_backend_architecture_static_audit.py`
  false-red: a valid `NativeAnalogBackendDescriptor &Getter()` declaration was
  rejected because the regex required no whitespace before `&`.
- Added static regression coverage for the event path and reran the complete
  `run_native_backend_checks.py --require-compiler` gate successfully. Physical
  exact-EXE unplug/replug evidence remains required; Provider V2 is still a
  shadow output route and is not falsely marked complete.
- Hash-verified backup before edits:
  `.analysis/backups/r2_uap_hotplug_provider_v2_pre_20260905_1930`.
  No GitHub/network or version-control mutation was performed.
## 2026-09-06 - RM-05 independent ViGEm producer freshness

- Added a generation-bound parent producer lease with a 200-ms documented
  deadline. It advances after each complete enabled `Backend_Tick` calculation,
  including an unchanged held XUSB report, rather than observing snapshot or
  child-heartbeat activity.
- Upgraded the fixed 640-byte output IPC semantics to wire version 2. Each
  snapshot records its producer lease sequence; the child neutralizes exactly
  once on expiry and rejects a pre-neutral queued nonzero snapshot on recovery.
- The real child applies the neutral frame itself and records explicit sticky
  producer-stalled telemetry; lifecycle stop/reap and the existing isolation
  boundary remain unchanged.
- Added fake-clock/portable and static regression gates. Full static and
  portable compiler-required checks passed; no HallJoy build, real ViGEm child,
  keyboard, or controller was run. Hardware/output qualification is pending.
## 2026-09-06 - RM-06 profile runtime transaction characterization

- Kept the existing prepared profile/settings/key/bindings commit boundary:
  it protects one complete `Backend_Tick` from a partial profile application.
- Changed read admission from a single CAS to a bounded three-attempt CAS loop.
  It now distinguishes reader contention from an active writer without adding
  a realtime wait, spin loop, allocation, or mutable shared configuration.
- Added portable simultaneous-reader coverage and a static contract gate. A
  full immutable RuntimeConfig is deliberately not claimed: the remaining
  unbounded key snapshot reader is owned by RM-07.
## 2026-09-06 - RM-07 unified supported-key settings path

- Replaced the extended-key unordered-map/shared-mutex reader with one bounded
  fixed table for the actual 1033-code HallJoy domain (`0x001..0x409`).
- Extended the atomic key snapshot and curve cache to the same domain, so Fn
  and OEM use the identical cached path as ordinary HID keys without aliasing.
- Bound snapshot reads to three attempts with a data-race-safe atomic fallback;
  profile commits still protect the multi-key publication boundary.
- Added portable Fn/OEM/profile-preparation regression coverage and static
  guards. No input device, controller, or ROG diagnostic image was run.
## 2026-09-06 - RM-08 production-linked controller replay

- Confirmed the existing configured-XUSB builder is already the production
  pure calculation core; backend owns provider reads, mouse sampling, config
  capture, telemetry and output transport around it.
- Extended its portable fixture with release and replacement-generation state
  reset, guarding against inherited SOCD/LKP direction after a profile-shaped
  configuration change.

## 2026-09-06 - RM-09 native/UAP common snapshot pilot

- Chose the already-versioned Provider V2 contract as the sole common native/UAP
  snapshot format. The new native endpoint is pull-only and source-preserving;
  it deliberately does not change the legacy `ReadMilli` max path or output
  arbitration, which remains RM-11 work.
- Added a bounded, capacity-aware legacy-milli adapter. It derives stable device
  identity from provider plus exact normalized HID interface fingerprint (never
  VID/PID alone), retains `LegacyQuantized`, and reports truncation explicitly.
- Ported SparkLink as the first native producer. Its callback uses a bounded
  publication seqlock, exports only proved-fresh row samples, and deliberately
  leaves whole-device `Complete` clear because row polling is not one atomic
  keyboard scan.
- Portable oracle proves same HID usage from two exact interfaces remains two
  identities and that removing one does not erase the other. Static and unified
  portable compiler-required gates passed. No HallJoy runtime, HID, controller,
  output-child, or ROG diagnostic image was run; Spark hardware timing and
  transport qualification remain pending.
- Hash-verified pre-change backup:
  `.local/backups/rm09_native_snapshot_pre_20260906_160000`
  (`SHA256SUMS.txt` SHA-256
  `0CB1107329635D9ECD0AD4A19527F8FA85FB8B80AE27F65F7919E2B9B3042835`).

## 2026-09-06 - RM-10 numerical precision evidence gate

- Audited SparkLink's 16-bit `routeRaw` path. Its current 3500..5000 denominator
  is an observed-max heuristic, not a firmware-proven sensor scale, so exporting
  it as V2 raw/domain would falsely claim precision.
- Kept the RM-09 `LegacyQuantized` adapter and all existing normalization intact.
  Packet/firmware plus hardware travel evidence is required before raw retention
  can be implemented. The precise capture/reopen gate is documented in
  `NUMERICAL_PRECISION_RM10_REVIEW_2026-09-06.md`.

## 2026-09-06 - RM-11 explicit source arbitration

- Characterized and extracted the production input decision as a pure
  `Arbitrate` function. Source state now explicitly carries availability,
  ownership, freshness and normalized value; the result records contributing
  source bits.
- Preserved standard native/UAP max, native authority for extended keys,
  owned-zero digital-fallback blocking, and mouse pseudo-key isolation. The
  shadow uses the same policy without a digital source.
- Added portable cases for standard max, extended authority, owned-zero,
  stale rejection and fallback, then ran the unified portable/static suite.
  Hardware concurrent-device/disconnect/hotplug evidence remains pending.

## 2026-09-06 - RM-12 Provider V2 removal gate review

- Verified that a dedicated qualification artifact already exists and is
  fail-closed: it requires same-transaction V2/dense capture, capacity,
  coverage, equality and input-stability evidence before a PASS report.
- No exact real-device finalized PASS artifact was available and the user is
  gaming, so no qualification image or input/output route was launched. Dense
  capture and the ordinary shadow remain deliberately intact; the required
  promotion evidence is documented in
  `PROVIDER_V2_RM12_REMOVAL_REVIEW_2026-09-06.md`.

## 2026-09-06 - RM-13 UAP reconnect/lifecycle review

- Mapped the existing single-owner refresh path from `WM_DEVICECHANGE` through
  the analog-host supervisor. It invalidates before child replacement, retains
  resources and blocks restart after an unconfirmed reap, and keeps the legacy
  broad UAP scan disabled.
- Existing static/simulator gates cover startup rollback, parent/child faults,
  stop hang, reap timeout and generation ownership. No rewrite was warranted;
  physical unplug/replug evidence remains pending. Details are in
  `UAP_RECONNECT_RM13_REVIEW_2026-09-06.md`.

## 2026-09-06 - RM-14 native HID containment review

- Confirmed that active native transports use cancellation, bounded join,
  neutral publication and restart poison while retaining live worker/HID/
  `OVERLAPPED` resources after an unconfirmed join. Addressed's reader join is
  deliberately lifetime-safe.
- Did not introduce an unproven per-provider process boundary or detach any
  worker. Hardware pending-I/O evidence per protocol remains RM-30 work; the
  current and required vertical-adapter gate are documented in
  `NATIVE_HID_CONTAINMENT_RM14_REVIEW_2026-09-06.md`.

## 2026-09-06 - RM-15 independent runtime supervisor

- Moved periodic realtime/output recovery from the UI timer to one dedicated,
  500 ms bounded supervisor. It owns only recovery of already-initialized
  dependencies; it never opens HID, initializes providers or calls UI code.
- Startup creates it only after the dependency transaction; rollback and
  shutdown stop/join it before native, realtime, backend or overlay teardown.
  An unconfirmed join retains the handles, poisons restart and skips dependent
  cleanup for immediate process containment.
- Added a product-linked static oracle for ownership/order/no-force-termination.
  No HallJoy runtime, HID, controller, output child or ROG diagnostic image was
  run. UI-free recovery/output-fault evidence remains pending. Details:
  `RUNTIME_SUPERVISOR_RM15_REVIEW_2026-09-06.md`.

## 2026-09-06 - RM-15 shutdown containment budget

- By explicit product decision, raised the process-wide shutdown watchdog from
  12 to 30 seconds. The prior 12-second value could pre-empt a valid in-flight
  supervisor recovery together with the bounded ViGEm/UAP containment path.
- This remains a hard final containment limit, not a replacement for local
  bounded joins, retained-resource poison, or a future shared pause deadline.
  The new static oracle pins both the exact deadline and that interpretation.
- Hash-verified pre-change backup:
  `.local/backups/rm15_shutdown_deadline_pre_20260906_170500`
  (`SHA256SUMS.txt` SHA-256
  `6BFE1145666C607446448AF8DFC4BAD1F82AA41ECCC4BC0263D9EC3EEE1131DC`).

## 2026-09-06 - RM-15 recovery-oracle ownership alignment

- Corrected two stale ViGEm static oracles after recovery ownership moved from
  the UI timer to `runtime_supervisor`. They now require that the UI contains
  no output-recovery call and that the supervisor alone records a one-shot
  fail-closed recovery failure.
- Targeted audits and the complete compiler-required portable/static suite
  pass. No HallJoy runtime, HID, controller, output child or ROG diagnostic
  image was run.
- Hash-verified backup:
  `.local/backups/rm15_audit_oracle_alignment_pre_docs_20260906_174300`
  (`SHA256SUMS.txt` SHA-256
  `53CBA20379C2FC252A6B916AE13E89F844AAD7EFB7B40CB02ACC6D3AF32EED0B`).

## 2026-09-06 - Permanent owner quality policy

- The owner confirmed a standing decision for all future work: do not choose a
  quick implementation that knowingly leaves a correctness, safety, ownership,
  lifecycle, compatibility or release-quality gap when a complete solution is
  available through further engineering work.
- A staged migration remains acceptable only if every shipped stage is itself
  safe and correct for its explicitly limited contract; it must not be presented
  as the final feature. Routine speed-versus-quality alternatives do not require
  a new owner question.
- Recorded in `ENGINEERING_WORKING_METHOD.md`. Hash-verified backup:
  `.local/backups/engineering_quality_policy_pre_20260906_175300`
  (`SHA256SUMS.txt` SHA-256
  `1CA90181F22B140279E11EE8988062EE074263EDC0765194F0CD8EF0F0225ACB`).

## 2026-09-06 - RM-16 command/lease foundation

- Corrected the command model so a newly created owner starts `Paused`, not
  falsely `Active`; a failed fresh enumeration/proof may return to `Paused`
  only after the failed attempt has released its resources, while an incomplete
  stop remains terminal `PauseFaulted`.
- Added one explicit backend admission gate. Once the future owner closes it,
  realtime ticks, input callbacks and topology notifications cannot revive a
  releasing generation. The gate is default-open until the owner is wired, so
  this foundation does not alter current startup behavior or claim Pause/Resume.
- Added portable/state and static admission oracles; the full
  compiler-required portable/static suite passed. No HallJoy runtime, HID,
  controller, output child or ROG diagnostic image was run.
- Hash-verified backup:
  `.local/backups/rm16_command_foundation_pre_docs_20260906_181700`
  (`SHA256SUMS.txt` SHA-256
  `1D91A2E7D08BE5CBE326929E947E378BE3375026C4AD27F0039839BE7FAF82DB`).

## 2026-09-06 - RM-16 serialized engine-owner foundation

- Added the closed-start `EngineRuntimeOwner` component: one bounded command
  worker, one serialized pending request, complete noexcept operation table,
  authoritative snapshot, common transaction executor and retained-resource
  poison on an unconfirmed join. It intentionally has no adoption path for a
  generation started by legacy code.
- Linked the component into the project and added a product-linked structural
  oracle. It is not yet connected to `app.cpp`; no feature is exposed and no
  existing lifecycle behavior is claimed to have changed. The next atomic
  migration must remove direct owners rather than introduce an overlap.
- Full static checks passed. No HallJoy runtime, HID, controller, output child
  or ROG diagnostic image was run. Design/implementation record:
  `ENGINE_RUNTIME_OWNER_IMPLEMENTATION_RM16_2026-09-06.md`.

## 2026-09-06 - RM-16 owner, UI bridge, and global control migration

- Moved initial generation, native/UAP/backend proof, dependent startup,
  device-release shutdown, and pre-explicit-pause late-device retry into the
  one closed-start engine owner. Direct timer lifecycle work was removed.
- Added a bounded posted-message UI bridge for hook/cursor/mouse-IPC release
  and restore. During Pause hooks pass input through before physical unhook;
  backend admission stays closed until a fresh neutral generation and UI
  restoration both complete.
- Added the Global Settings Pause/Resume control. It displays only the owner
  snapshot, rejects transition/fault states, and permanently prevents automatic
  device-change reopen after the first explicit Pause.
- Updated stale lifecycle oracles to the new single-owner boundary and added
  owner/UI/control oracles. Static checks and source syntax checks passed. No
  HallJoy runtime, HID, controller, output child or ROG diagnostic image was
  run; physical coexistence/adverse-I/O evidence remains pending.

## 2026-09-06 - RM-16 truthful transition publication

- The owner now mirrors every accepted command-state transition to its
  lock-free public snapshot, including retryable Resume cleanup and faults.
  The Global Settings control therefore shows an in-progress release/start
  rather than a stale `Active`/`Paused` label while the serialized worker is
  executing.
- Added portable assertions for the complete observed transition sequences and
  extended the owner structural oracle. Static audits and the focused portable
  transaction executable passed; source-only syntax validation passed for the
  owner and page changes (with pre-existing third-party/source warnings only).
  No HallJoy runtime, HID, controller, output child or ROG diagnostic image was
  run.

## 2026-09-06 - RM-17 per-user conflicting-instance protection

- Added a fail-closed global mutex guard keyed by the current Windows user SID.
  It is acquired after same-image child-role dispatch and before logger,
  storage, provider, qualification, or application startup. It covers the
  same user across sessions without blocking a different Windows user.
- Kept the aggregate parent guard separate from device-specific protocol
  leases, added a process-only Windows parent/child conflict-and-release test
  and a startup-order structural oracle, and recorded the policy in
  `INSTANCE_GUARD_RM17_2026-09-06.md`. Static audits and the focused test
  passed; no HallJoy runtime, HID, controller, output child or ROG diagnostic
  image was run.

## 2026-09-06 - RM-18 synchronous persistence ownership review

- Verified that the existing persistence model has one synchronous UI-side
  writer and atomic durability per file; retained it instead of introducing an
  async queue with stale-profile and shutdown races. RM-17 now also excludes a
  concurrent same-user parent process before it reaches the settings root.
- Routed binding-panel mutations through the common UI persistence hand-off,
  made aggregate settings-save success explicit at timer/shutdown boundaries,
  and added a structural oracle. The design record is
  `PERSISTENCE_OWNER_RM18_2026-09-06.md`. Full static audits and source-only
  syntax checks passed; no HallJoy runtime, HID, controller, output child or
  ROG diagnostic image was run.

## 2026-09-06 - RM-19 interrupted factory-reset recovery

- Corrected the crash window where a validated factory-reset marker and its
  partly populated backup tree previously caused a permanent collision on the
  next launch. The reset now validates and resumes that exact owned tree,
  preserving it for retry if another resumed attempt cannot complete.
- Added topology checks for resumed targets and extended the reset oracle. The
  migration path was reviewed as already idempotent copy-only recovery. Record:
  `RECOVERY_RM19_2026-09-06.md`. Runtime simulator evidence remains pending;
  static and source-only syntax checks passed without starting HallJoy, HID,
  controller, output child, or ROG diagnostic.

## 2026-09-06 - RM-20 bounded persisted-number contract

- Added strict shared signed/unsigned INI numeric parsing and migrated the
  settings, curve, layout, and bindings validation paths away from permissive
  integer conversion. Invalid external values no longer alias into ordinary
  zero/default-looking values through permissive parsing.
- Hardened the staged input lease against reparse points/directories and added
  a parser boundary test plus static oracle. Details are recorded in
  `BOUNDED_INI_CONTRACT_RM20_2026-09-06.md`; source-only validation and focused
  portable tests passed without starting HallJoy, HID, controller, output
  child, or ROG diagnostic.

## 2026-09-06 - RM-21 curve and final-output finite boundary

- Sanitized the shared curve evaluator and made inverse endpoints exact;
  hardened configured XUSB conversion so non-finite input becomes neutral
  before rounding or button interpretation.
- Added portable curve/XUSB boundary cases and a structural oracle. The record
  is `CURVE_NUMERIC_BOUNDARY_RM21_2026-09-06.md`. No HallJoy runtime, HID,
  controller, output child, or ROG diagnostic was started.

## 2026-09-06 - RM-22 realtime scheduler review

- Reviewed the existing sequence/deadline scheduler and retained it: it already
  closes the notify/wait race, has a precise legal-output deadline tail, and
  provides diagnostic-only percentile telemetry without per-tick logging.
- The required real workload baseline/A-B measurement remains explicitly
  pending rather than motivating an unmeasured scheduler rewrite. Record:
  `REALTIME_SCHEDULER_REVIEW_RM22_2026-09-06.md`.

## 2026-09-06 - RM-23 private UAP child image verification

- Added a child-side byte-for-byte resource verification and retained file
  lease across plugin load, closing the ordinary check-to-load replacement
  window for the parent-provided path. Reparse points and non-files are
  rejected at the same boundary.
- Updated the private-runtime trust oracle and documented the precise
  same-user integrity scope in `PRIVATE_UAP_TRUST_BOUNDARY_RM23_2026-09-06.md`.
  Static/syntax checks passed without running HallJoy, HID, controller, output
  child, or ROG diagnostic.

## 2026-09-06 - RM-24 IPC ownership and mouse commit boundary

- Recorded the creator, visibility, reader/writer roles, schema, session
  identity, retire rule and closer for every production IPC/data boundary.
  Private host/output channels remain unnamed capability transports; the public
  mouse mapping retains its fixed 40-byte v1 ABI.
- Made the mouse publisher commit its four related state scalars between odd
  and even heartbeat values. This preserves the legacy monotonic heartbeat
  while enabling an updated ASI reader to reject a torn capture without a
  mapping resize or a second public object.
- The external ASI is not in this tree, so its opt-in coherent-read migration
  is explicitly pending rather than asserted. Details:
  `IPC_OWNERSHIP_RM24_2026-09-06.md`. The mouse static audit and source-only
  syntax check passed; no HallJoy runtime, HID, controller, output child or ROG
  diagnostic was run.

## 2026-09-06 - RM-25 overlay HTTP review

- Audited every endpoint, access rule, framing limit, client owner and shutdown
  path. The server is already loopback-only, session/origin-bound, bounded in
  parsing and concurrency, and isolates slow clients from the engine.
- Retained the existing architecture rather than weakening the access boundary
  or introducing an unmeasured rewrite. Server socket/fuzz/slow-client evidence
  is output-capable and remains deferred while gameplay input must not be
  disturbed. Details: `OVERLAY_HTTP_REVIEW_RM25_2026-09-06.md`.

## 2026-09-06 - RM-26 raw-input allocation and mouse lifecycle review

- Added a 64 KiB raw-input envelope cap before the UI thread grows its receive
  buffer, retaining the architecture-correct typed payload check afterward.
  The arithmetic/static oracle now prevents removal or relocation of that
  boundary.
- Reviewed raw input, hooks, blocking escape, cursor clip, pause/resume and
  shutdown ownership. Global-hook/controller manual sequences remain deferred
  during gameplay; details are in `RAW_INPUT_MOUSE_REVIEW_RM26_2026-09-06.md`.

## 2026-09-06 - RM-27 bounded UI glyph resources

- Found and fixed unbounded size/style-keyed GDI glyph caches in the keyboard
  preview and Remap panel. Each now evicts and frees a complete deselected
  DC/bitmap pair at 256 entries.
- Recorded existing retained-page/scroll/resource ownership and extended the
  UI static oracle. DPI/multi-monitor repeated-open handle measurements remain
  manual runtime evidence; see `UI_RESOURCE_REVIEW_RM27_2026-09-06.md`.

## 2026-09-06 - RM-28 failure-evidence classification

- Kept release logging disabled and existing asynchronous/bounded diagnostics;
  added analyzer-only stable labels for stale input, producer stall, output,
  storage and incomplete-lifecycle evidence without changing hot-path writes.
- Extended trace fixtures and passed the complete static suite. Runtime storage
  and capture-overhead evidence remains deferred; see
  `OBSERVABILITY_RM28_2026-09-06.md`.

## 2026-09-06 - RM-29 AULA HERO84 HE release-scope freeze

- Per owner decision, retained the AULA HERO84 HE implementation but removed it
  from the ordinary catalog and ordinary build. It now requires the explicit
  `HallJoyAulaHero84HeExperimental` test target, which has its own output name
  and trace support for a future consenting hardware owner.
- Recorded the exact re-enable evidence gate in `SUPPORTED_HARDWARE.md` and
  `DECISIONS.md` (D-084). The source/static check confirms that an ordinary
  build cannot accidentally claim this untested route. No executable was built
  or run, and no HID/controller/output activity was started.

## 2026-09-06 - RM-30-SPARK protocol review

- Re-audited the enabled SparkLink/XD row protocol end-to-end: exact request /
  response correlation, 8×21 layout bounds, row-local freshness, duplicate-HID
  aggregation, row-limit retirement, hotplug age and cooperative-stop
  ownership. The existing implementation and focused portable/static gates
  already cover the modeled malformed and partial-row cases; no speculative
  protocol rewrite was warranted.
- The raw scale remains explicitly legacy/observed rather than firmware-proven,
  so this review does not promote it to a precision claim. Physical multi-device
  input/release/reconnect evidence remains pending; see
  `PROTOCOL_AUDIT_RM30_SPARK_2026-09-06.md`.

## 2026-09-06 - RM-30-SAYO protocol review

- Reconfirmed the O3C `0x21` edge / `0x22` depth wire contract, length and
  index gates, exact normalization boundary, read-only sibling-PID admission,
  ambiguity-safe automatic letter matching, stale-depth fallback and reader
  group lifecycle. Existing focused portable/static tests cover the association
  model and shutdown/exception ownership.
- Kept the documented single-O3C-session boundary: concurrent separate O3C
  devices are not promoted to independent mappings without a per-physical-device
  aggregation model and real evidence. O3C physical input remains the only
  tested model; other PID routes remain capability-gated. See
  `PROTOCOL_AUDIT_RM30_SAYO_2026-09-06.md`.

## 2026-09-06 - RM-30-MAD68 protocol review

- Revalidated the exact MAD 68 Pro R A0 route: audited identity and 68-entry
  descriptor table, strict packet/header/raw-domain parser, raw-input-to-A0
  correlation, post-sweep ownership proof, per-key freshness fallback and
  bounded recovery. The only write opcodes remain the specifically audited,
  reversible A8/A9 pair and are protected by a finite strategy/response gate.
- Existing protocol, route, safety and cooperative-stop checks cover the source
  contract. Fresh physical timing, recovery and multi-device qualification are
  still required outside this no-runtime review; see
  `PROTOCOL_AUDIT_RM30_MAD68_2026-09-06.md`.

## 2026-09-06 - RM-30-HEX80 protocol review

- Rechecked exact 0x96 identity/probe behavior, the 104-slot/82-key layout,
  four-entry chunk correlation, travel-domain validation, cycle completeness,
  disconnect neutralization and bounded lifecycle. The existing parser corpus
  covers short, wrong-offset, wrong-size and implausible-travel frames.
- Retained the post-proof `03 96 19` calibration-exit command because the
  existing evidence documents it as the required idempotent restoration from
  calibration mode; it is sent only after two GET proofs. See
  `PROTOCOL_AUDIT_RM30_HEX80_2026-09-06.md`.

## 2026-09-06 - RM-30-ADDRESSED protocol review

- Revalidated checksum/framing, dynamic-map/canonical fallback boundaries,
  nine-key request correlation, late-response invalidation, scheduler fairness,
  reader ownership and per-key freshness. The established `09 98 02` session
  command remains: owner-confirmed real hardware evidence and the historical
  contract identify it as disabling legacy last-key diagnostics.
- No supported route was removed or broadened. The protocol remains one active
  physically proven interface at a time; see
  `PROTOCOL_AUDIT_RM30_ADDRESSED_2026-09-06.md` and D-085.

## 2026-09-06 - RM-30-AULA6X21 protocol review

- Rechecked the independent Aula/SparkPlayJoy `5C` wire contract, exact
  identity/board registry, bounded read-only capability sequence, 6x21 matrix
  and semantic-map validation, retained-device identity, ownership and
  cooperative lifecycle behavior. No protocol behavior was changed.
- Existing parser fuzzing, source audit and physical WIN 60 HE MAX evidence
  support the current route. Sibling 6x21 models retain their explicit
  protocol-compatible versus physically-tested boundary; see
  `PROTOCOL_AUDIT_RM30_AULA6X21_2026-09-06.md`.

## 2026-09-06 - RM-30-W669 protocol review

- Rechecked the independent report-ID-1 `0D/18/21` contract, exact product
  profiles, zero-record factory inheritance, live-event-only publication,
  sensor-domain exclusion, identity re-proof and lifecycle/neutralization.
- Confirmed a real evidence boundary rather than adding a speculative timeout:
  the event stream has no source-proven idle heartbeat, so an elapsed-time
  reconnect would also disrupt a healthy untouched keyboard. Lost-release
  recovery remains an open hardware/protocol proof gate (`HJ-V14-P0-004`), not
  a justified source-only rewrite. See `PROTOCOL_AUDIT_RM30_W669_2026-09-06.md`.

## 2026-09-06 - RM-30-HERO84 protocol review

- Revalidated the HERO84 exact report-09/UUID admission, firmware-derived
  read-only `82/01`, `83` and `94/02` boundary, and separation of vendor replies
  from normal typing. All calibration/persistent/feature/flash commands remain
  outside the allow-list.
- Kept the owner-approved release freeze intact: ordinary builds cannot claim
  the keyboard, the future diagnostic owns no input, and no executable was
  built or run. See `PROTOCOL_AUDIT_RM30_HERO84_2026-09-06.md` and D-084.

## 2026-09-06 - RM-30-ND75 protocol review

- Revalidated the experimental ND75 M484 boundary: exact USB/firmware/
  capability proof, asymmetric host `29/18` versus device `21` framing, pinned
  6x22 factory map, bounded event parser and neutralizing reconnect lifecycle.
- The candidate remains outside ordinary builds and still needs its explicit
  owner procedure for real-device evidence. No executable or HID session ran;
  see `PROTOCOL_AUDIT_RM30_ND75_2026-09-06.md`.

## 2026-09-06 - RM-30-DDNATIVE protocol review

- Revalidated the native DrunkDeer diagnostic boundary: serialized three-chunk
  frame assembly, raw-domain validation, exact G65 map separation, extended
  Fn/Menu codes, logger-only digital observations and failure neutralization.
- No native production support was inferred from the diagnostic or UAP family,
  and no executable/HID session ran. See
  `PROTOCOL_AUDIT_RM30_DDNATIVE_2026-09-06.md`.

## 2026-09-06 - RM-30-UAPWOOTING protocol review

- Rechecked pinned UAP Wooting V1/V2 admission, report-domain parsing, full
  special-key handling, finite normalization, explicit release snapshots and
  disconnect-to-zero behavior through the owner-pinned V2 export path.
- No model-specific layout or physical multi-device claim was inferred from
  generic decoding. No executable/HID session ran; see
  `PROTOCOL_AUDIT_RM30_UAPWOOTING_2026-09-06.md`.

## 2026-09-06 - RM-30-UAPRAZER protocol review

- Rechecked exact Razer model/report-ID admission, the required live Synapse
  condition, V2/V3 record handling, empty-report disconnect and the disabled
  mode-changing feature command.
- The receiver's per-frame report identity and post-discovery Synapse lifecycle
  require a physical capture; no guessing or vendor mode write was added. See
  `PROTOCOL_AUDIT_RM30_UAPRAZER_2026-09-06.md`.

## 2026-09-06 - RM-30-UAPDD protocol review

- Hardened the embedded DrunkDeer transaction against malformed/short and
  reordered replies: success now requires a completed request and exact
  64-byte `04/B7` chunks 0, 1 and 2 before ordered payload assembly.
- This fixes the direct underflow/OOB and wrong-arrival-order matrix hazards
  without guessing a new model layout or widening admission. The locked overlay
  hash and static audits pass; no executable/HID session ran. See
  `PROTOCOL_AUDIT_RM30_UAPDD_2026-09-06.md`.

## 2026-09-06 - RM-30-UAPKEYCHRON protocol review

- Revalidated the exact K4 HE custom `A9/31` four-frame ABI, its 6x19 matrix
  provenance, and the intentional stock `A9/30` performance boundary. Hardened
  UAP version/per-key reads before field access and full snapshots before fixed,
  ordered 30-byte fragment assembly; failed sends cannot consume a stale reply.
- The firmware exposes no part/token identity, so cross-process contention is
  correctly retained as the D-076 physical-test-isolation boundary rather than
  guessed away with a timeout or exclusive open. No firmware/configuration
  action, executable or HID session ran; see
  `PROTOCOL_AUDIT_RM30_UAPKEYCHRON_2026-09-06.md`.

## 2026-09-06 - RM-30-UAPNUPHY protocol review

- Hardened the `A0` stream reader before decoding: it now requires the full
  eight-byte record, fails closed for raw travel outside the established
  800/1600 domain, and retains 16-bit travel through publication instead of
  prematurely reducing it to 256 cache levels.
- Kept the broad VID/interface discovery and its lack of per-model capability
  proof explicitly evidence-bound; no guessed PID table, vendor write,
  executable or HID session was added. See
  `PROTOCOL_AUDIT_RM30_UAPNUPHY_2026-09-06.md`.

## 2026-09-06 - RM-30-UAPMADLIONS protocol review

- Corrected the late-chunk failure hole: UAP now counts failures per four-key
  chunk, resets only the matching successful chunk, neutralizes the failed
  chunk immediately and disconnects after eight repeated failures of it.
- Retained the existing SafeHID transaction boundary and documented unresolved
  reply-correlation, MAD68R-layout and full-snapshot evidence rather than
  guessing a protocol. No executable, HID session or firmware action ran; see
  `PROTOCOL_AUDIT_RM30_UAPMADLIONS_2026-09-06.md`.

## 2026-09-06 - RM-30-RESEARCH protocol review

- Revalidated MCHOSE Ace 68 and Titan68 Turbo as exact-device,
  transport/diagnostic-only routes. Ace `A0` descriptors remain unclaimed
  evidence, while Titan is restricted to the paired control/stream topology,
  read-only mapping and reversible `36:01 -> 36:00` visual observation.
- Corrected the historical Titan ambiguity in the current audit record:
  calibration `37` is not emitted by the active diagnostic, and observed stock
  stream activity is not evidence of simultaneous typing. See
  `PROTOCOL_AUDIT_RM30_RESEARCH_2026-09-06.md`.

## 2026-09-06 - RM-31 sanitizer health control

- Added a separately compiled, intentionally invalid ASan/UBSan control before
  the ordinary pure-protocol fuzz corpus. The runner now requires that exact
  child to fail with an AddressSanitizer diagnostic, then independently requires
  the normal 250,000-iteration parser corpus to pass.
- No HallJoy executable, plugin, HID session or controller was run. This is a
  tool-health proof for the three covered pure parser modules, not a broad
  sanitizer claim; see `SANITIZER_HEALTH_RM31_2026-09-06.md`.

## 2026-09-06 - RM-31 sanitizer health parity

- Moved the intentional ASan failure check into one shared helper and required
  it before both the parser-fuzz and Aula WIN60HE sanitizer suites. This keeps
  the same compiler flags, runtime diagnosis requirement and fail-closed
  semantics across both pure portable test gates.

## 2026-09-06 - RM-31 test execution coverage matrix

- Added a machine-checked route inventory for every `*_test.cpp`. It separates
  explicit and convention-routed portable tests from the intentional sanitizer
  control and the production-linked, isolated simulator profile test, so neither
  is silently skipped or misreported as ordinary portable evidence.
- The simulator route remains unrun under the owner’s no-HallJoy-runtime rule;
  its backend-forbidden contract and execution command are recorded rather than
  inferred from a source-only pass. See `TEST_EXECUTION_COVERAGE_MATRIX_RM31_2026-09-06.md`.

## 2026-09-06 - RM-32 UAP overlay compile correction

- The first isolated UAP build exposed a real C++ type error in the hardened
  DrunkDeer fragment-order path: Soup’s `Buffer` is a class template, so retained
  fragment pointers must spell `Buffer<>*`. Corrected only that type spelling;
  frame validation, ordering and disconnect behavior remain unchanged.

## 2026-09-06 - RM-32 isolated UAP build

- Rebuilt both native-routing UAP ABI targets from pinned Sun/Soup after the
  compile correction. The locked overlay and rebuilt cache hashes agree, and
  both DLLs were produced. No HallJoy or plugin runtime/HID test was started;
  see `BUILD_CHAIN_RM32_2026-09-06.md` for the exact boundary.

## 2026-09-06 - RM-32 production compile boundary

- A no-run MSVC rebuild found and corrected the invalid mixing of C++ exception
  handling and Windows SEH in the runtime-supervisor thread entry. The new
  static oracle requires the same two-layer boundary used by the other workers.
- The repeat Release/x64 production compilation then passed, with only the
  explicitly permitted ViGEm PDB linker warning. No executable was launched.

## 2026-09-06 - RM-33 profile startup retry

- The first permitted production profiling attempt exposed a harness defect:
  PowerShell treated an expected refused connection during listener startup as a
  terminating native-command error, so its intended retry loop never retried.
  The bounded readiness probe now temporarily receives that exit status as data
  and restores strict error handling before every later phase.

## 2026-09-06 - RM-33 portable production-profile isolation

- The permitted profile run proved that an ordinary `--overlay-server` launch
  can rewrite real user settings. The profiler now copies the requested,
  hash-verified production EXE and a checked copy of the current HallJoy state
  into its evidence directory, places the existing `HallJoy.portable` marker
  beside that copy, and runs only that portable instance. This is the normal
  production portable-storage mode, not a simulator or fault-injection route.
- It snapshots and compares the real `%LOCALAPPDATA%\\HallJoy` tree before and
  after the run, while retaining the isolated runtime and its changed test state
  as evidence. A reparse-point state item or a path outside the evidence root
  fails closed.
- The runner reads the actual listener port from HallJoy's production stability
  trace during bounded startup. This is necessary because a selected global
  profile can legitimately replace the pre-start overlay setting; the probe now
  measures the listener that was actually created rather than guessing from a
  stale configuration file.

## 2026-09-06 - RM-33 isolated real-runtime profile result

- The production copy completed all three ten-second phases and the real user
  state before/after manifests were identical (`8B57F525…1094035B2`); no
  HallJoy process remained. The isolated run retained its full trace and
  browser/overlay evidence under
  `build/evidence/rm33_input_profile_portable_20260906_191300`.
- The run exposed an obsolete harness assumption, not a missing user device:
  it required the historical Irok/SparkLink route even though the current
  machine has an active UAP provider plane (one device, 127 sample slots) and
  real ViGEm publication. RM-33 now requires that generic, live supported
  input-to-output path. SparkLink transaction timing remains optional evidence
  when that specific native family is connected, and is never fabricated or
  re-labelled as UAP evidence.
## 2026-09-06 - RM-33 long-soak isolation

- Applied the same production-portable isolation model to the long-soak runner.
  It now runs a SHA-256-verified copy of the requested ordinary EXE beside the
  copied state and `HallJoy.portable` marker inside evidence, then compares the
  untouched live state before and after. The long soak continues to exercise
  real HID/UAP/ViGEm behavior; only storage is isolated.

## 2026-09-09 — DrunkDeer layout/model integration delivered

- Added seven source-derived physical layouts, shared UI/editor/overlay catalog,
  conservative upgrade of unedited A75 Pro/G65 defaults and verified first-run
  selection. Preserved manual/saved choices and existing model ordering.
- Connect-time Antler identity runs on the owning UAP handle after deduplication,
  before worker publication, with bounded waits. Known models use generated
  tracking maps; unverified devices keep compatibility behavior without claiming
  an exact layout. G65 navigation follows the owner's official-source decision.
- Extended Soup with distinct hash/Ro/Yen HID identities; existing IDs unchanged.
  Updated seven-file integrity lock and packaged the rebuilt plugin. Tests now
  resolve sorted combo rows by catalog identity instead of assuming two models.
- Static/portable C++, source/import, ABI and production profile/editor/picker
  gates PASS. Details and limitations: `../current/DRUNKDEER_LAYOUTS.md`.
- Release `F9FF13072B96C85FC0078EB5B0A45B98D9BAE6599FF14844E75914102213680A`
  installed and launched. Backup: `.local/backups/drunkdeer-integration-20260909/`.
  Stop at this brand for owner evaluation; no DrunkDeer hardware PASS claimed.
