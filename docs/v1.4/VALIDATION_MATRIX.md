# HallJoy v1.4 validation matrix

## Gate definitions

| Gate | Purpose | Minimum evidence |
|---|---|---|
| `S` | Source and static correctness | Project parse, static audits, dependency and manifest checks |
| `P` | Portable behavioral tests | GCC and Clang C++20 tests, warnings, ASan and UBSan |
| `W` | Windows build/runtime | Clean MSVC x64 Release build, launch, clean shutdown, second launch |
| `F` | Failure and lifecycle | Injected partial start, timeout, exception, cancellation and restart |
| `H` | Hardware | Protocol-specific input, hotplug, held-key unplug and ViGEm output |
| `R` | Release | Clean checkout/package, hashes, notices, CI, soak and device matrix |

## Current baseline evidence

Date: 2026-07-31

| Check | Result | Notes |
|---|---|---|
| Original archive path safety | PASS | 0 unsafe ZIP entries |
| Archive SHA-256 | PASS | `39727D2F63165F63B2AC0AA8105DBF4937C02442D09B0F8FA800F195D502A4CF` |
| Clean import comparison | PASS | 344 files, 0 missing or hash mismatches |
| Imported archive static audits | PASS | All supplied static audits completed |
| Imported archive x64 Release build | PASS | 0 errors, 1 ViGEm PDB linker warning |
| v1.3 checkpoint x64 Release build | PASS | 0 errors, 6 recorded warnings |
| Portable tests with Clang 19.1.5 | PASS | All static and portable C++20 tests passed |
| GCC portable tests | INHERITED/PENDING | Byte-identical archive evidence exists; rerun in CI before publication |
| Windows UI smoke from integration branch | PASS | Window opened, ViGEm initialized, exit code 0, no remaining child process |
| Stability trace | WARN | Expected: no SparkLink device was exercised on this workstation |
| Hardware gates | PENDING | Required only by packages that touch the corresponding backend |

The workstation uses a membrane keyboard. The planned simulator can satisfy
common-pipeline behavioral gates, but never a protocol-specific hardware gate.

## Per-package evidence template

Every completed package appends:

```text
Package:
Commit:
Scope:
Commands:
Results:
Warnings:
Hardware:
Known limitations:
Rollback:
```

Historical PASS reports do not automatically verify changed v1.4 code. A gate
must be rerun whenever its covered code or build inputs change.

## V14-01 evidence

```text
Package: V14-01
Scope: Product identity and active documentation ownership
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  git clone --no-local --branch v1.4-integration --single-branch
    C:\VSCode\HallJoy C:\VSCode\HallJoy-v14-04-cleanroom
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
    (from the independent clone)
Results:
  Static audits: PASS
  Portable C++20 tests (Clang 19.1.5): PASS
  MSVC x64 Release: PASS, 0 errors, 1 inherited ViGEm PDB warning
  EXE FileVersion: 1.4.0.0
  EXE ProductVersion: 1.4.0.0
Hardware: Not required; runtime protocol behavior was unchanged
Known limitations: GCC reruns in CI before publication
Rollback: parent commit of the V14-01 implementation commit
```

## V14-02 evidence

```text
Package: V14-02
Scope: Development-only deterministic analog simulator
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_analog_simulator.ps1
  MSBuild HallJoy.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64
    /p:HallJoyMad68ProRNative=true /p:HallJoyStabilityTrace=true /m
Results:
  Static audits: PASS
  Portable simulator boundary/replay tests: PASS with Clang 19.1.5
  Simulator MSVC x64 Release: PASS, 0 errors, 1 inherited ViGEm PDB warning
  Script scenario: PASS
  Common pipeline: ramp and diagonal non-neutral reports observed
  SOCD: W+S and A+D both produced exact neutral left-stick reports
  Safety: disconnect and source fault produced exact neutral reports
  ViGEm: changed non-neutral and subsequent neutral reports accepted
  Shutdown: graceful exit code 0; no remaining simulator process
  Production MSVC x64 Release: PASS; simulator sources absent from compile
Hardware: NOT VERIFIED; simulator evidence cannot close any device gate
Known limitations: real protocol timing, HID transport and firmware behavior
  remain assigned to V14-12 hardware gates
Rollback: parent commit of the V14-02 implementation commit
```

## V14-03 evidence

```text
Package: V14-03
Scope: Self-contained private UAP runtime and truthful dependency diagnostics
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -ForceUserUapRuntime
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild -RunSeconds 8
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  MSBuild HallJoy.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64
    /p:HallJoyMad68ProRNative=true /p:HallJoyStabilityTrace=true /m
Results:
  Static audits: PASS
  Windows command-line quoting portable test: PASS
  Forced protected-directory fallback: PASS, location=user
  Ordinary portable runtime: PASS, location=executable
  Exact resource comparison after extraction: PASS
  Corrupted per-user runtime atomic self-repair: PASS
  Embedded source SHA-256 equals repaired runtime SHA-256:
    0C45419D8F615284B4D673CB369191E6ABFCD57A72D3564C744D5960682DD8B2
  Child private-runtime load and backend initialization: PASS
  ViGEm initialization and simulated common pipeline: PASS
  Graceful parent/child shutdown: PASS
  System SDK/global UAP recovery URLs: absent
  Full build script and production x64 Release: PASS, 0 errors,
    1 inherited ViGEmClient PDB warning
  Packaged production UI smoke: PASS, graceful exit 0, no child left behind
Hardware: NOT VERIFIED; runtime evidence does not close device gates
Known limitations: UAP protocol/device behavior remains in hardware qualification
Rollback: parent commit of the V14-03 implementation commit
```

## V14-04 evidence

```text
Package: V14-04
Scope: Reproducible inputs, local/CI gate parity, and warning policy
Commands:
  python src/HallJoyProject/tests/dependency_lock_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
Results:
  Dependency lock audit: PASS
  Static audits and portable C++20 tests with Clang 19.1.5: PASS
  Locked Sun revision: 83c195bd61314bdbfdccc161653dbb652e3b6678
  Locked Soup revision: b02796b0b20276277c8a4b4d3759643eeab43ff7
  Five-file HallJoy Soup overlay SHA-256 verification: PASS
  Fresh shallow fetch by both locked commit SHAs: PASS
  ViGEmClient size/SHA-256 verification: PASS
  Private UAP rebuild from locked inputs: PASS
  MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  GitHub workflow lock/parity audit: PASS
  Independent clean-room clone at 2230dee: PASS
  Clean-room production smoke: PASS, graceful exit 0, no process left
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: Not required; runtime/protocol behavior is unchanged
Known limitations: GitHub-hosted jobs were not run; they are not a V14-04 gate
Rollback: parent commit of the V14-04 implementation commit
```

## V14-05 evidence

```text
Package: V14-05
Scope: Truthful lifecycle registry and generation-scoped stop contract
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -RunSeconds 8
Results:
  Static audits and portable C++20 tests: PASS
  Registry failure injection: PASS
  Wrong-thread mutation rejection: PASS
  Failed start then new generation: PASS
  Confirmed join then restart: PASS
  Timeout and stale generation poison/restart block: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Simulator common-pipeline and graceful shutdown scenario: PASS
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: Not required for the registry contract; per-worker device gates remain
Known limitations: ordinary TerminateThread paths remain open for V14-06+
Rollback: parent commit of the V14-05 implementation commit
```

## V14-06A evidence

```text
Package: V14-06A
Scope: Realtime loop cooperative shutdown
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -RunSeconds 8
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectRealtimeStopTimeout -RunSeconds 7
Results:
  Realtime TerminateThread occurrences: 0
  Portable joined/timeout/wait-failure policy: PASS
  Timeout ownership retention and restart poison audit: PASS
  Backend teardown guard and poisoned process-exit audit: PASS
  Static audits and portable C++20 tests: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Simulator common pipeline and cooperative shutdown: PASS
  Simulator forced realtime join timeout containment, expected exit 2: PASS
  Normal simulator ERROR trace events: 0
  Timeout simulator expected containment ERROR events: present
  Remaining simulator processes: 0
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: Protocol behavior unchanged; hardware gates remain separate
Known limitations: a blocking ViGEm call is isolated later; four other worker
  families still retain forced-termination paths
Rollback: parent commit of the V14-06A implementation commit
```
