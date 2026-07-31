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
