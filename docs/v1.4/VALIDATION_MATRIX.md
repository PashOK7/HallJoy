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
| Stability trace | PASS | V14-06D.1 Irok trace has 3/3 balanced workers, 2 reconnects, analog recovery and no reconnect after service stop |
| Hardware gates | PARTIAL | Required SparkLink/Irok gate is Verified; other protocol devices and the release matrix remain pending |

The current workstation has an Irok MG75 Max (`VID 1CA6`, `PID 0529`). Its
SparkLink route is available for hardware gates, but simulator evidence remains
separate and cannot replace protocol-specific input, hotplug or shutdown proof.

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

## V14-06B evidence

```text
Package: V14-06B
Scope: Diagnostic logger cooperative shutdown
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1
    -InjectDebugLogStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild -RunSeconds 7
Results:
  Diagnostic logger TerminateThread occurrences: 0
  Queue/drain/flush ownership static audit: PASS
  Timeout resource retention and restart poison audit: PASS
  Poisoned process-exit audit: PASS
  Static audits and portable C++20 tests: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Simulator forced writer join timeout containment, expected exit 3: PASS
  Simulator common pipeline and cooperative shutdown: PASS
  Normal simulator ERROR trace events: 0
  Remaining simulator processes: 0
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: Not required; common pipeline simulation only
Known limitations: overlay, SparkLink and Sayo retain forced-termination paths
Rollback: parent commit of the V14-06B implementation commit
```

## V14-06C evidence

```text
Package: V14-06C
Scope: Overlay server cooperative shutdown
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1
    -InjectOverlayStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1
    -SkipBuild -StartOverlay -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
Results:
  Overlay TerminateThread occurrences: 0
  Start/stop lifecycle and ownership static audit: PASS
  Static audits and portable C++20 tests: PASS
  Loopback /state response: HTTP 200
  Graceful overlay worker join and stop.end: PASS
  Normal overlay simulator ERROR trace events: 0
  Simulator forced overlay join timeout containment, expected exit 2: PASS
  Timeout retained thread/WSA ownership and poisoned restart: PASS
  Dependent application cleanup skipped after poison: PASS
  Remaining simulator processes: 0
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: Not required; loopback IPC and common pipeline simulation only
Known limitations: HTTP framing/concurrency remain S16; SparkLink and Sayo
  retain forced-termination paths
Rollback: parent commit of the V14-06C implementation commit
```

## V14-06D evidence

```text
Package: V14-06D
Scope: SparkLink cooperative shutdown
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1
    -InjectSparkStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
Results:
  SparkLink TerminateThread occurrences: 0
  Exception and cooperative lifecycle static audits: PASS
  Late-hotplug worker remains covered by outer registry stop: PASS
  Static audits and portable C++20 tests: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Simulator forced Spark worker timeout containment, expected exit 2: PASS
  Thread/event ownership retained and inner restart poisoned: PASS
  Outer native registry poisoned and dependent cleanup skipped: PASS
  Normal simulator common pipeline and cooperative shutdown: PASS
  Normal simulator ERROR trace events: 0
  Remaining simulator processes: 0
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: Existing S02B.2 device gate predates V14-06D. Protocol behavior is
  unchanged; post-change SparkLink shutdown/reconnect regression remains part
  of release qualification
Known limitations: Sayo retains the final forced-termination path
Rollback: parent commit of the V14-06D implementation commit
```

## V14-06E evidence

```text
Package: V14-06E
Scope: Sayo cooperative reader-group shutdown
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1
    -InjectSayoStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
Results:
  Production TerminateThread occurrences: 0
  Sayo cooperative reader-group static audit: PASS
  Static audits and portable C++20 tests: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Simulator forced Sayo reader timeout containment, expected exit 2: PASS
  One shared three-second deadline for all readers: PASS
  Neutral publication before cancellation and after confirmed join: PASS
  Reader group/event retained and inner restart poisoned: PASS
  Outer native registry poisoned and dependent cleanup skipped: PASS
  Normal simulator common pipeline and cooperative shutdown: PASS
  Normal simulator ERROR trace events: 0
  Remaining simulator processes: 0
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: NOT RUN; Sayo hardware unavailable. Simulator proves lifecycle
  containment only, not Sayo protocol/device compatibility
Known limitations: Sayo C++/SEH and early-reader-exit containment remain V14-06F
Rollback: parent commit of the V14-06E implementation commit
```

## V14-06F evidence

```text
Package: V14-06F
Scope: Sayo reader exception containment and completion publication
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1
    -InjectSayoReaderCppFault -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectSayoStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
Results:
  Production TerminateThread occurrences: 0
  Sayo exception-boundary and cooperative-shutdown static audits: PASS
  Static audits and portable C++20 tests: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Simulator C++ reader exception containment, expected exit 0: PASS
  Fixed fault record, neutral input and group stop publication: PASS
  Early-reader-exit startup rejection: PASS
  Per-reader completion and final-reader connected reset: PASS
  Earlier forced Sayo timeout containment, expected exit 2: PASS
  Normal simulator common pipeline and cooperative shutdown: PASS
  Normal simulator ERROR trace events: 0
  Remaining simulator processes: 0
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: NOT RUN; Sayo hardware unavailable. Simulator proves exception and
  lifecycle containment only, not Sayo protocol/device compatibility
Known limitations: post-change Sayo device/reconnect and long-run gates remain
  V14-12 release qualification; V14-07 owns UAP/analog-host boundaries
Rollback: parent commit of the V14-06F implementation commit
```

## V14-07A evidence

```text
Package: V14-07A
Scope: Analog-host parent generation, partial-start rollback and bounded stop
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectAnalogHostSupervisorStartFailure -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectAnalogHostBridgeStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
Results:
  Analog-host generation ownership static audit: PASS
  All static audits and portable C++20 tests: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Normal simulator common pipeline and cooperative shutdown: PASS
  Normal simulator ERROR trace events: 0
  Injected supervisor-start partial rollback, expected exit 0: PASS
  Bridge joined before IPC/event/job resources were released: PASS
  Injected bridge stop timeout, expected exit 2: PASS
  Two bounded parent-worker waits completed before containment: PASS
  Thread/IPC/event/job ownership retained and restart poisoned: PASS
  Backend failure propagated; dependent teardown skipped: PASS
  Remaining simulator processes after every scenario: 0
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: Not required for parent lifecycle containment. Simulator evidence is
  not analog-keyboard protocol/device verification
Known limitations: analog-host worker exception/crash publication and private
  UAP C ABI/lock/state/unload safety remain V14-07B/C
Rollback: parent commit of the V14-07A implementation commit
```

## V14-07B evidence

```text
Package: V14-07B
Scope: Analog-host worker fault publication and bounded child restart/exit
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectAnalogHostSupervisorCppFault -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectAnalogHostChildCppFault -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectAnalogHostChildReapTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
Results:
  Analog-host exception/restart static audit: PASS
  All static audits and portable C++20 tests: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Parent supervisor C++ fault publication and clean owner shutdown: PASS
  Child C++ fault publication and one bounded restart: PASS
  Injected child reap timeout retained ownership and blocked restart: PASS
  Earlier supervisor-start and bridge-timeout scenarios: PASS
  Normal simulator common pipeline and cooperative shutdown: PASS
  Remaining simulator processes after every accepted scenario: 0
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Hardware: PARTIAL/FAIL. Irok MG75 Max proved native SparkLink discovery and
  polling, but changed analog rows were not observed and shutdown raced into a
  second worker generation. Tracked separately as HJ-V14-P1-004 / V14-06D.1
Known limitations: private UAP C ABI/lock/state/unload safety remains V14-07C
Rollback: parent commit of the V14-07B implementation commit
```

## V14-06C.1 evidence

```text
Package: V14-06C.1
Scope: Overlay one-shot connection responsiveness and canonical EXE name
Observed production evidence:
  overlay_perf.log isolated fetch averages: 5,001,400-5,002,000 us
  Root cause: the single HTTP worker waited on an idle /client_perf keep-alive
    connection for the configured 5,000 ms receive timeout
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild -StartOverlay -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectOverlayStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  python tools/check_overlay_responsiveness.py --port 18765
Results:
  All static audits and portable C++20 tests: PASS
  Overlay response-close static contract: PASS
  Simulator /client_perf -> /state latency: PASS, 0.3 ms (< 1,000 ms)
  Normal overlay lifecycle and graceful shutdown: PASS
  Forced overlay stop-timeout containment, expected exit 2: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Production /client_perf -> /state latency: PASS, 0.4 ms (< 1,000 ms)
  Production hidden-window WM_CLOSE shutdown: PASS, exit 0, 0 processes left
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,133,504 bytes
  SHA-256: 93AF87C6D8079BD48E21A53AE78342625CE0AB7050BDEFE98EA34690AA08A058
Hardware: Irok MG75 Max production run recorded 515 changed rows and 516 input
  notifications. SparkLink shutdown/reconnect remains HJ-V14-P1-004 / V14-06D.1
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Known limitations: general multi-client HTTP concurrency remains V14-10
Rollback: parent commit of the V14-06C.1 implementation commit
```

### V14-06D.1 evidence

```text
Package: V14-06D.1 SparkLink service-stop reconnect suppression
Date: 2026-07-31
Commands:
  python src/HallJoyProject/tests/sparklink_cooperative_shutdown_static_audit.py
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -InjectSparkShutdownRace -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectSparkStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
Results:
  Full static and portable C++20 gate: PASS
  Spark service-stop static audit: PASS
  Service-stop reconnect-suppression simulator: PASS, exit 0
  Existing Spark timeout-containment simulator: PASS, expected exit 2
  Normal deterministic simulator regression: PASS, exit 0
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Irok MG75 Max detected: VID 1CA6, PID 0529, usage page FFB0
  Production graceful WM_CLOSE: PASS, exit 0, 0 processes left
  Spark worker starts/exits: 1/1
  Reconnect/device-open/connect after service.stop.begin: 0/0/0
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,134,528 bytes
  SHA-256: 330748ACBB0EDC0E35A4BA39807EC16F3DF2CB849940ED10128CDCF714BFEE25
Hardware: shutdown structure passed on Irok MG75 Max; held-key unplug/reconnect
  acceptance passed: 3 balanced workers, 2 reconnects, 2,073 changed rows,
  2,075 input notifications, no stuck input, and input restored after reconnect
  Reconnect/device-open/connect after service.stop.begin: 0/0/0
  HJ-V14-P1-004: Verified
  Acceptance trace SHA-256:
    8595A84ACE70209A0CEE9DF9E814DFA2A8B188EAD992B3CC9AB110EB0308DC5E
  Analyzer verdict remains WARN only for unrelated mode/row-limit coverage and
    unavailable non-Irok routes; unplug/reconnect recognition itself passes
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Evidence: docs/stability/tests/V14-06D.1_SPARK_SERVICE_SHUTDOWN_2026-07-31.txt
Rollback: parent commit of the V14-06D.1 implementation commit
```

## V14-07C evidence

```text
Package: V14-07C
Scope: Private UAP C ABI exception, lock/state/null and bounded unload safety
Commands:
  python src/HallJoyProject/tests/private_uap_abi_safety_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  python tools/check_private_uap_abi.py
    third_party/UniversalAnalogPluginFixed/dist/universal-analog-plugin/abiv1.dll
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  build/output/HallJoy.exe (4-second production smoke, graceful task signal)
Results:
  Private UAP C ABI/unload static audit: PASS
  Portable C ABI exception fallback and RAII lock test: PASS
  All static audits and portable C++20 tests: PASS
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS, ABI=1
  State false before init, true after init, false after unload: PASS
  Null device-info/full-buffer inputs return zero: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Production smoke: parent, diagnostic-watch and analog-host child started
  Graceful termination: child.exit=1, stop.joined=1, backend/main shutdown end
  Remaining HallJoy processes: 0
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,141,184 bytes
  SHA-256: 15228FC17B70FB84AD2861FC04904E872B3571CCABBCEB26D6DFD3AE894D533B
Hardware: Irok MG75 Max remained on the independent native SparkLink route;
  this package changes only the private UAP/child boundary
Remote CI: NOT RUN; optional, account quota unavailable and no push permitted
Known limitations: UAP pacing/identity/contention remain V14-11; device-owner
  and long-run soak qualification remain V14-12
Evidence: docs/stability/tests/V14-07C_PRIVATE_UAP_ABI_UNLOAD_2026-07-31.txt
Rollback: parent commit of the V14-07C implementation commit
```

## V14-08A evidence

```text
Package: V14-08A
Scope: Transactional dependent startup, durable input wake and ordered curve publication
Commands:
  python src/HallJoyProject/tests/startup_wake_transaction_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -InjectRealtimeStartFailure -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectNativePhaseStartFailure -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  build/output/HallJoy.exe (6-second Irok production smoke, WM_CLOSE)
Results:
  Startup/wake/publication static audit: PASS
  All static audits and portable C++20 tests: PASS
  Pre-start, burst-coalescing and notify/consume race wake tests: PASS
  Release/acquire payload publication concurrency test: PASS
  Realtime-start failure rollback: PASS, realtime then backend joined, exit 0
  Native-phase failure rollback: PASS, AfterRealtime then realtime then backend
    joined in reverse acquisition order, exit 0
  Normal common-pipeline simulator: PASS alongside live SparkLink input
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS
  Production startup transaction: commit; AfterRealtime 0 failures/0 rejected,
    AfterRawInput 1 running/0 failures/0 rejected
  Irok MG75 Max connected: VID 1CA6, PID 0529, usage page FFB0
  Graceful WM_CLOSE: backend/main shutdown end, exit 0, 0 processes left
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,145,280 bytes
  SHA-256: E6A90BEE93EE28A25CDB7C3A03F13C773FE5B48204CDDE49A713BF6C040A8C43
  Production trace SHA-256:
    AFF96801D8A90AE41CB946E07ED019A1793BBF981401380B79A78109E27CEE08
Hardware: startup and balanced shutdown passed on Irok MG75 Max; simulator
  fault injections prove lifecycle behavior, not other keyboard compatibility
Remote CI: NOT RUN; optional, no push permitted
Known limitation: synchronous ViGEm submission remains HJ-AUD-P1-010/V14-08B
Evidence: docs/stability/tests/V14-08A_STARTUP_WAKE_PUBLICATION_2026-07-31.txt
Rollback: parent commit of the V14-08A implementation commit
```

## V14-08B evidence

```text
Package: V14-08B
Scope: ViGEm runtime output isolation, newest-state equivalence and stalled-driver containment
Commands:
  python src/HallJoyProject/tests/vigem_output_isolation_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectVigemUpdateStall -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  build/output/HallJoy.exe (10-second hidden Irok production smoke, PID WM_CLOSE)
Results:
  ViGEm output-isolation static audit: PASS
  All static audits and portable C++20 tests: PASS
  Latest-value mailbox complete-copy/newest-payload/multi-pad mask tests: PASS
  Normal common-pipeline simulator: PASS, graceful output-worker join, exit 0
  Normal simulator trace SHA-256:
    AEF9EB00E29E1963602EA1E1C0DFDD73750639C5C5FD3FB76D1C0B6A7C232DBA
  Simulator-only 60-second driver-update stall: PASS; the complete realtime
    scenario and realtime stop finished before the bounded 3-second output
    timeout, dependent backend teardown was skipped and containment exited 2
  Stalled-driver simulator trace SHA-256:
    BD7C5ED4CBB3FF4CC3BE5DDC75F4F700C1057D46590893B4C6CDCC5E1B5F19DE
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS
  Production Irok route: SparkLink VID 1CA6, PID 0529, usage page FFB0
  Startup transaction committed; realtime and ViGEm output worker started
  Graceful WM_CLOSE: output worker joined before dependent backend teardown,
    analog-host child exited, backend/main shutdown ended, exit 0, 0 processes
  User settings, bindings, layouts and profiles restored hash-identically
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,147,840 bytes
  SHA-256: 73F425BFED6B090015842A987C79CBCA99E07CEA38FD6DB6C227084E4A719CA3
  Production trace SHA-256:
    700E580F65862C2E4EC5FD8F8AFB406CC72306D3FE30B4B55F6C21438C87FE6C
Hardware: startup, native Irok input route and balanced shutdown passed; the
  stalled-driver injection is simulator-only and is not hardware evidence
Remote CI: NOT RUN; optional, no push permitted
Known limitation: initial ViGEm client/target creation remains a startup-thread
  operation; runtime update, reconnect and destroy are output-worker-owned
Evidence: docs/stability/tests/V14-08B_VIGEM_OUTPUT_ISOLATION_2026-07-31.txt
Rollback: parent commit of the V14-08B implementation commit
```

## V14-09A evidence

```text
Package: V14-09A
Scope: Transactional settings, overlay, active-profile and bindings persistence
Commands:
  python src/HallJoyProject/tests/persistence_transaction_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectPersistenceFailure <prepare|write|flush|validate|replace> -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -RunSeconds 8
Results:
  Persistence transaction static audit: PASS
  Portable prepare/write/flush/validate/replace state-machine test: PASS
  All static audits and portable C++20 tests: PASS
  Windows stage injections: 5/5 PASS; settings, bindings and overlay probe
    hashes unchanged at every failure; zero *.halljoy-new-* files remained
  Normal common-pipeline simulator after implementation: PASS, exit 0
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS
  Production Irok route: SparkLink VID 1CA6, PID 0529, usage page FFB0
  Irok smoke: 30,125 successful route queries, 127 changed rows, 0 failures
  Graceful shutdown: persistence emitted no ERROR, all workers joined, exit 0,
    zero remaining HallJoy processes
  User mutable state: 5 restored files, 0 SHA-256 mismatches
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,157,568 bytes
  SHA-256: 989E1ADA3C0AAC9D973D2C28A5201AEAC130DCB7693029C6C797146CF53BB500
  Production trace SHA-256:
    1A294BF1AA03E572F90E8B0532E26012593F153F7D428B1E4AB02E7F2DA3D151
Hardware: Irok startup/input/balanced shutdown passed; persistence failure
  stages are simulator injections and do not claim hardware fault coverage
Remote CI: NOT RUN; optional, no push permitted
Known limitations: layout/curve writers remain V14-09B; LocalAppData migration
  and profile-name normalization remain V14-09C
Evidence: docs/stability/tests/V14-09A_TRANSACTIONAL_PERSISTENCE_2026-08-01.txt
Rollback: parent commit of the V14-09A implementation commit
```

## V14-09B evidence

```text
Package: V14-09B
Scope: Transactional layout preset, curve preset and curve-state persistence;
  completion of save failure propagation
Commands:
  python src/HallJoyProject/tests/persistence_transaction_static_audit.py
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectPersistenceFailure <prepare|write|flush|validate|replace> -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -RunSeconds 8
Results:
  Persistence transaction static audit: PASS
  Portable prepare/write/flush/validate/replace state-machine test: PASS
  All static audits and portable C++20 tests: PASS
  Windows stage injections: 5/5 PASS; settings, bindings, overlay, layout,
    curve-preset and curve-state probe hashes stayed unchanged at every stage;
    zero *.halljoy-new-* files remained
  Normal common-pipeline simulator: PASS, graceful shutdown, exit 0
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS
  Legacy user layout/state formats loaded without a persistence error
  Production Irok route: SparkLink VID 1CA6, PID 0529, usage page FFB0
  Final old-config smoke: 30,095 successful route queries, one transient query
    failure, no analog-row changes observed; prior V14-09A input proof remains
    the hardware input evidence because V14-09B changes persistence only
  Graceful shutdown: overlay, realtime, ViGEm output, backend and main joined,
    exit 0, zero remaining HallJoy processes
  User mutable state: 5 restored files, 0 SHA-256 mismatches
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,163,712 bytes
  SHA-256: 443DFBCE08E232A159C115BE391995D28CBD11186B45F9966C23766725B269FB
  Production trace SHA-256:
    EB4AE295A9E1C5ADE8B3AC595C6A788E2E527E302244F6FB636968D6E52EECF4
Hardware: Irok startup/route/balanced shutdown passed; no new input claim;
  persistence failure stages are simulator-only
Remote CI: NOT RUN; optional, no push permitted
Known limitations: LocalAppData migration and profile-name normalization remain
  V14-09C
Evidence:
  docs/stability/tests/V14-09B_LAYOUT_CURVE_TRANSACTIONAL_PERSISTENCE_2026-08-01.txt
Rollback: parent commit of the V14-09B implementation commit
```

## V14-09C evidence

```text
Package: V14-09C
Scope: LocalAppData/portable state root, source-preserving one-time migration,
  Unicode/case/reserved-name/path hardening
Commands:
  python src/HallJoyProject/tests/storage_migration_static_audit.py
  python src/HallJoyProject/tests/persistence_transaction_static_audit.py
  python tools/run_native_backend_checks.py
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_storage_migration_test.ps1 -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectPersistenceFailure <prepare|write|flush|validate|replace> -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -RunSeconds 10
Results:
  Storage and persistence static audits: PASS
  All static audits and portable C++20 tests: PASS
  Normal current simulator: PASS, graceful shutdown, exit 0
  Migration/replay/portable runtime gate: PASS
  Migration failure stages: 5/5 PASS; each exited 1 before startup, preserved
    source, committed no target and left zero *.halljoy-new-* files
  Existing persistence failure stages: 5/5 PASS across all six probes
  Unicode policy: composed/decomposed NFC and case aliases collide; reserved
    DOS basename, length and direct-child traversal cases pass
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS
  Official package retained five legacy mutable files with zero hash changes
  First LocalAppData migration: 5 sources preserved, 5 backup hashes exact,
    one completed marker, zero temporary files
  Replay launch: migration.skip reason=complete; LocalAppData root selected
  Production Irok route: SparkLink VID 1CA6, PID 0529, usage page FFB0;
    37,980 successful queries, zero failures, balanced shutdown, exit 0
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,216,960 bytes
  SHA-256: 7E84054C944698CBCD2ABF76EAF70B1500DD0A007FEBC3CCC9E23BF2AF0944C6
  Replay production trace SHA-256:
    F6CB87D888E45DFCDC013E917BE014C6330B638D3CD9B69D86A1E20A6546A4E0
Hardware: Irok startup/route/balanced shutdown passed; migration fault stages
  and filename policy cases are simulator evidence
Remote CI: NOT RUN; optional, no push permitted
Evidence:
  docs/stability/tests/V14-09C_STORAGE_MIGRATION_NAME_POLICY_2026-08-01.txt
Rollback: parent commit of the V14-09C implementation commit; runtime state can
  be restored from the recorded legacy backup, while legacy source files remain
  untouched beside the executable
```

## V14-10A evidence

```text
Package: V14-10A
Scope: Mouse IPC creation disposition, existing-schema preservation/validation
  and atomic peer-field reads
Commands:
  python src/HallJoyProject/tests/mouse_ipc_static_audit.py
  python tools/run_native_backend_checks.py
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_storage_migration_test.ps1 -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild -StartOverlay -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -RunSeconds 10
Results:
  Mouse IPC static audit: PASS
  All static audits and portable C++20 tests: PASS
  Simulator runtime policy self-test: PASS; existing sentinel payload retained,
    legacy zero size upgraded to 40, invalid schema rejected without overwrite
  Normal current simulator: PASS, graceful shutdown, exit 0
  Storage migration/replay/portable and five migration fault stages: PASS
  Overlay lifecycle: PASS; next /state response 0.4 ms
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS
  Production Mouse IPC init: created=1, schema_valid=1, size=40
  Production Irok route: SparkLink VID 1CA6, PID 0529, usage page FFB0;
    37,937 successful queries, zero failures, balanced shutdown, exit 0
  Runtime user state: zero differences from pre-smoke backup
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,217,472 bytes
  SHA-256: 7AB4EF791179AF4271F5307A5B695436599D047D4F9AC0530250A43A42B50E86
  Production trace SHA-256:
    C6B0B5D8C0938F1D8D78B203D94E646F7C14523F6E32AD41C33B6F56A889B6DC
Hardware: Irok startup/route/balanced shutdown passed; no analog-row changes,
  so this package makes no new hardware input claim
External ASI: NOT RUN; binary ABI compatibility is covered by unchanged public
  name/version/offsets/size and the simulator-emulated legacy zero size slot
Remote CI: NOT RUN; optional, no push permitted
Known limitation: analog-host IPC authentication/ACL/precreation remains
  V14-10B; overlay protocol/concurrency risks remain later V14-10 packages
Evidence:
  docs/stability/tests/V14-10A_MOUSE_IPC_CORRECTNESS_2026-08-01.txt
Rollback: parent commit of the V14-10A implementation commit
```

## V14-10B evidence

```text
Package: V14-10B
Scope: analog-host named IPC removal, explicit inherited handle capabilities,
  v10 owner/generation identity and invalid-handle rejection
Commands:
  python src/HallJoyProject/tests/analog_host_ipc_static_audit.py
  python tools/run_native_backend_checks.py
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectAnalogHostIpcHandleRejection -RunSeconds 9
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -<each of five existing AnalogHost fault switches> -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -RunSeconds 10
Results:
  Analog-host IPC static audit: PASS
  All static audits and portable C++20 tests: PASS
  Normal current simulator: PASS, graceful shutdown, exit 0
  Runtime policy: inherited_handles, named_objects=0, handle_list=1,
    owner_handle=1, generation_bound=1
  Invalid mapping handle: child exit 31 before shared-state use; one bounded
    restart; second child and common pipeline completed; balanced exit 0
  Existing analog-host lifecycle/fault regressions: 5/5 PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS
  Production Irok route: SparkLink VID 1CA6, PID 0529, usage page FFB0;
    37,953 successful queries, zero failures, balanced child/worker shutdown
  Runtime user state: 11 files, zero differences from pre-smoke backup
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,218,496 bytes
  SHA-256: 8FD6609DFF589DF76515EF62C6B1365C83C47BFB0452AC5D5BCB20B8DDE78223
  Production trace SHA-256:
    A098F363C4D52D7852D07BE0432B0A4B3AB6CD4F43ACE31AC8DD573C86B9A7EB
Hardware: Irok startup/route/balanced shutdown passed; no analog-row changes,
  so this package makes no new hardware input claim
Threat limit: removes named-object precreation/substitution; not a sandbox
  against a process already authorized to tamper with HallJoy itself
Remote CI: NOT RUN; optional, no push permitted
Known limitation: overlay framing, overflow, origin and concurrency remain
  V14-10C/V14-10D
Evidence:
  docs/stability/tests/V14-10B_ANALOG_HOST_IPC_CAPABILITIES_2026-08-01.txt
Rollback: parent commit of the V14-10B implementation commit
```

## V14-10C evidence

```text
Package: V14-10C
Scope: incremental overlay HTTP framing, strict request limits and overflow-safe
  telemetry query parsing
Commands:
  python -m py_compile tools/check_overlay_http_framing.py
    src/HallJoyProject/tests/overlay_http_framing_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -StartOverlay -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectOverlayStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -StartOverlay
    -OverlayPort 18765 -RunSeconds 7
Results:
  Overlay framing static audit: PASS
  Static audits: 33/33 PASS
  Portable C++20 tests: 18/18 PASS
  Simulator overlay responsiveness: PASS; next /state 0.3 ms
  Simulator socket framing: PASS; fragmented=1, pipelined=2,
    bounded_rejections=8 plus method and valid-telemetry cases
  Overlay timeout containment regression: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS
  Production overlay responsiveness: PASS; next /state 0.4 ms
  Production socket framing and graceful shutdown: PASS
  Production Irok route: SparkLink VID 1CA6, PID 0529, usage page FFB0;
    28,607 successful queries, zero failures, balanced shutdown, exit 0
  Runtime user state: 11 files, zero differences from pre-smoke backup
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,223,104 bytes
  SHA-256: 554693964189E4B2B4C256D992F271A1BD81082DC40CF057EC4F45E023C5C817
  Production trace SHA-256:
    A38C0ED1F9C64A304D71BF291E3D1417021706929BF7A44BEF6AC0AA8EC48AF3
Hardware: Irok startup/route/balanced shutdown passed; no analog-row changes,
  so this package makes no new hardware input claim
Remote CI: NOT RUN; optional, no push permitted
Known limitation: single-client scheduling and wildcard browser origin remain
  V14-10D (`HJ-AUD-P2-001`, `HJ-AUD-P2-004`)
Evidence:
  docs/stability/tests/V14-10C_OVERLAY_HTTP_FRAMING_2026-08-01.txt
Rollback: parent commit of the V14-10C implementation commit
```

## V14-10D evidence

```text
Package: V14-10D
Scope: bounded multi-client overlay scheduling, strict browser origin/session
  policy and cooperative shutdown of active client workers
Commands:
  python -m py_compile tools/check_overlay_responsiveness.py
    tools/check_overlay_http_framing.py
    tools/check_overlay_concurrency_origin.py
  python src/HallJoyProject/tests/overlay_concurrency_origin_static_audit.py
  python src/HallJoyProject/tests/overlay_cooperative_shutdown_static_audit.py
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -StartOverlay -RunSeconds 10
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1 -SkipBuild
    -InjectOverlayStopTimeout -RunSeconds 7
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -StartOverlay
    -OverlayPort 18765 -RunSeconds 10
Results:
  Overlay concurrency/origin and cooperative-shutdown audits: PASS
  Static audits: 34/34 PASS
  Portable C++20 tests: 18/18 PASS
  Simulator overlay responsiveness/framing: PASS; next /state 0.4 ms,
    fragmented=1, pipelined=2 and bounded_rejections=8
  Simulator concurrency/origin: PASS; 8 slow clients plus 8 parallel /state,
    maximum latency 2.3 ms, fixed client limit 16, prompt reset rejection,
    hostile origins=2, stale-session 401/browser rebootstrap and 128-bit cookie
  Active-client shutdown: PASS; 8/8 heartbeat-held partial clients closed by
    server stop and trace recorded active_clients=8
  Overlay timeout containment regression: PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Real ABI1 load/init/null/bounded-unload runtime gate: PASS
  Production overlay responsiveness: PASS; next /state 0.4 ms
  Production concurrency/origin: PASS; maximum parallel latency 1.6 ms,
    fixed limit 16 and strict origin/session checks
  Production Irok route: SparkLink VID 1CA6, PID 0529, usage page FFB0;
    45,867 successful queries, zero failures, average route interval 249 us,
    balanced shutdown and exit 0
  Runtime user state: 11 files, zero differences from pre-smoke backup
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,231,296 bytes
  SHA-256: BF44786B93C34C2E310949C69EBDF641753A8729339736F7D1F2278A6A1D9BE2
  Production trace SHA-256:
    7ED922357B9E1F4EF8FBC95C86614F361CF6712C1B3D76BC49B87DA3C3694294
Hardware: Irok startup/route/balanced shutdown passed; no analog-row changes,
  so this package makes no new hardware input claim
Threat limit: loopback origin/session boundary; not a sandbox against a process
  already authorized to inspect HallJoy or the user's browser state
Remote CI: NOT RUN; optional, no push permitted
Known limitation: UAP pacing, device identity and snapshot contention remain
  V14-11
Evidence:
  docs/stability/tests/V14-10D_OVERLAY_CONCURRENCY_ORIGIN_2026-08-01.txt
Rollback: parent commit of the V14-10D implementation commit
```

## V14-11A evidence

```text
Package: V14-11A
Scope: deadline pacing and transient-error backoff for private UAP poll workers
Commands:
  python src/HallJoyProject/tests/uap_poll_pacing_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  python tools/check_private_uap_abi.py
    third_party/UniversalAnalogPluginFixed/dist/universal-analog-plugin/abiv1.dll
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -StartOverlay
    -OverlayPort 18765 -RunSeconds 15
Results:
  UAP pacing static audit: PASS
  Static audits: 35/35 PASS
  Portable C++20 tests: 19/19 PASS
  Deterministic 50 us transaction model: paced calls=1,000/s versus
    unthrottled calls=20,000/s; modeled busy=50,000 versus 1,000,000 us/s
  Production policy: six/six private UAP targets use 1000 us deadlines;
    failure waits=2,4,8,16,32,64 ms capped; stream transports unchanged
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  Rebuilt ABI1 load/init/name/null/bounded-unload gate: PASS;
    name=Universal Analog Plugin (HallJoy SafeHID v10 deadline-paced telemetry)
  Production overlay responsiveness/framing/concurrency/origin: PASS;
    maximum parallel state latency 1.6 ms, fixed client limit 16
  Production Irok route: SparkLink VID 1CA6, PID 0529, usage page FFB0;
    65,138/65,138 successful queries, zero failures, average route interval
    252 us, balanced shutdown and exit 0
  Runtime user state: 11 files, zero differences; no process or temp remained
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,232,832 bytes
  SHA-256: B7959FB6807CE0B6966380E0D3F9F1ECBEE170693CBF8E453EA31CF7914992A2
  Production trace SHA-256:
    7918E18FF9FCDD7C9CF619DDFA7224FE248CE0E404F04E106593BDE7B1C591AE
Hardware: native Irok route regression passed, but this device bypasses UAP;
  no actual UAP poll keyboard CPU/USB/latency measurement is claimed
Analyzer note: WARN only because no keys or unplug/reconnect were exercised;
  production smoke itself passed and trace contains no ERROR event
Remote CI: NOT RUN; optional, no push permitted
Status: HJ-AUD-P2-006 Verified by D-022 automated production-code gates;
  no physical USB/latency claim
Next: V14-11B stable identity for identical UAP devices
Evidence:
  docs/stability/tests/V14-11A_UAP_POLL_PACING_2026-08-01.txt
Rollback: parent commit of the V14-11A implementation commit
```

## V14-11B evidence

```text
Package: V14-11B
Scope: occurrence-independent identity for multiple identical private-UAP devices
Commands:
  python src/HallJoyProject/tests/uap_device_identity_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  g++ -std=c++20 -O2 -Wall -Wextra -pedantic
    src/HallJoyProject/tests/uap_device_identity_test.cpp
  cl.exe /std:c++20 /EHsc /W4
    src/HallJoyProject/tests/uap_device_identity_test.cpp
  clang++ -std=c++20 -O1 -g -Wall -Wextra -pedantic
    -fsanitize=address,undefined -fno-omit-frame-pointer
    src/HallJoyProject/tests/uap_device_identity_test.cpp
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  python tools/check_private_uap_abi.py
    third_party/UniversalAnalogPluginFixed/dist/universal-analog-plugin/abiv1.dll
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -StartOverlay
    -OverlayPort 18765 -RunSeconds 15
Results:
  UAP identity static audit: PASS
  Static audits: 36/36 PASS
  Portable C++20 tests: 20/20 PASS
  Identical-device enumeration orders: 40,320/40,320 PASS
  Reconnect/subset/shuffle generations: 100,000/100,000 PASS
  Synthetic unique HID paths: 250,000, zero observed collisions
  Pathless fallback occurrences: 1,024/1,024 unique and honestly unflagged
  Case/slash, descriptor, length-framing and four persisted-ID golden vectors: PASS
  GCC 15.2 warning-clean: PASS
  MSVC 19.44 /W4: PASS
  Clang 21 ASan+UBSan: PASS, zero reports
  Pacing regression: 10,232 deadline properties and uint64 overflow edge PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  ABI1 load/init/name/null/bounded-unload: PASS, SafeHID v11 identity loaded
  Production overlay suite: PASS, maximum parallel latency 1.9 ms
  Production Irok route: 65,610/65,610 queries, zero failures,
    4,224 changed rows, 4,225 realtime notifications, 313 us average interval,
    balanced shutdown and exit 0
  Runtime user state: 11 files, zero differences; no process or temp remained
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,233,856 bytes
  SHA-256: C7D28AA23D882A1ED57FA2562DF7F6C8375DE6E3B02D09C33EE08B184123B116
  Production trace SHA-256:
    A6C33C41A99D2FB7BFF540D439956E0F295290D458E9BB690B3D6D0624016111
Limit: finite tests cannot mathematically exclude every 64-bit collision;
  physical port moves and driver path volatility are not claimed
Remote CI: NOT RUN; optional, no push permitted
Status: HJ-AUD-P2-007 Verified by D-022 automated production-code gates
Next: V14-11C snapshot export contention
Evidence:
  docs/stability/tests/V14-11B_UAP_DEVICE_IDENTITY_2026-08-01.txt
Rollback: parent commit of the V14-11B implementation commit
```

## V14-11C evidence

```text
Package: V14-11C
Scope: bounded UAP registry ownership and snapshot/telemetry lock separation
Commands:
  python src/HallJoyProject/tests/uap_snapshot_pinning_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  g++ -std=c++20 -O2 -Wall -Wextra -pedantic
    src/HallJoyProject/tests/uap_snapshot_pinning_test.cpp
  cl.exe /std:c++20 /EHsc /W4 /WX
    src/HallJoyProject/tests/uap_snapshot_pinning_test.cpp
  clang++ -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Werror
    -fsanitize=address,undefined -fno-omit-frame-pointer
    src/HallJoyProject/tests/uap_snapshot_pinning_test.cpp
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
  python tools/check_private_uap_abi.py
    third_party/UniversalAnalogPluginFixed/dist/universal-analog-plugin/abiv1.dll
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -StartOverlay
    -OverlayPort 18765 -RunSeconds 10
Results:
  UAP snapshot pinning static audit: PASS
  Static audits: 37/37 PASS
  Portable C++20 tests: 21/21 PASS
  Blocked snapshot reader plus concurrent registry removal: PASS
  Owner remained alive after erase and was destroyed exactly once: PASS
  Lifetime pin/erase cycles: 100,000/100,000 PASS
  Concurrent coherent 256-value reads: 50,000 minimum PASS
  GCC 15.2 warning-clean: PASS
  MSVC 19.44 /W4 /WX: PASS
  Clang 21.1.8 ASan+UBSan: PASS, zero reports
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  ABI1 load/init/name/null/bounded-unload: PASS, SafeHID v12 pinned snapshot
  Production overlay suite: PASS, maximum parallel latency 2.1 ms
  Production Irok route: 45,872/45,872 queries, zero failures,
    1,552 changed rows and realtime notifications, 267 us average and
    979 us maximum route interval, balanced shutdown and exit 0
  Runtime user state: 11 files, zero differences; no process or temp remained
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,235,904 bytes
  SHA-256: 2C7D5F923D6C989C2B6354EF4114B3AB8F124D1FFA42AC50B10C87FB3DD552A6
  ABI0 DLL: 414,208 bytes,
    SHA-256 4870CBD4A2F49C7E16D29765CF480956555AD6C92126D8DF15D31F385E3A4047
  ABI1 DLL: 286,720 bytes,
    SHA-256 8CC08C5268F0EE7CB1B3DD78A48FA99E89E5C31D49B40B886B393BE51D7B4FA1
  Production trace SHA-256:
    1BB4ED75AF624307C2FC36B078137EE802AA731FD8EDA25725EF756963840870
Limit: software proves registry lock scope, object lifetime and coherent copies;
  no physical UAP device latency/USB throughput claim is made
Remote CI: NOT RUN; optional, no push permitted
Status: HJ-AUD-P2-010 Verified by D-022/D-023 automated production-code gates
Next: V14-11D bound remaining UAP modularization/performance scope
Evidence:
  docs/stability/tests/V14-11C_UAP_SNAPSHOT_PINNING_2026-08-01.txt
Rollback: parent commit of the V14-11C implementation commit
```

## V14-11D evidence

```text
Package: V14-11D
Scope: exact HID interface-path ownership across native routing and Soup/UAP
Commands:
  python src/HallJoyProject/tests/native_hid_interface_claim_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  g++ -std=c++20 -O2 -Wall -Wextra -pedantic
    src/HallJoyProject/tests/native_hid_interface_claim_test.cpp
  cl.exe /std:c++20 /EHsc /W4 /WX
    src/HallJoyProject/tests/native_hid_interface_claim_test.cpp
  clang++ -std=c++20 -O1 -g -Wall -Wextra -pedantic
    -fsanitize=address,undefined -fno-omit-frame-pointer
    src/HallJoyProject/tests/native_hid_interface_claim_test.cpp
  cmd /c BUILD.cmd
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -StartOverlay -RunSeconds 10
  python tools/analyze_stability_trace.py
    build/output/HallJoyStabilityTrace.log
Results:
  Exact-interface ownership static audit: PASS
  Static audits: 38/38 PASS
  Portable C++20 tests: 22/22 PASS
  Same VID/PID sibling interfaces: independently claimed and routed PASS
  Reorder/reconnect generations: 10,000/10,000 PASS, 32 claims each
  Exact token prefix/suffix/garbage rejection: PASS
  Unicode wide/UTF-8 equivalence and malformed UTF-8 replacement: PASS
  Synthetic interface tokens: 300,000, zero observed collisions
  GCC 15.2 warning-clean: PASS
  MSVC 19.44 /W4 /WX: PASS
  Clang 21 ASan+UBSan: PASS, zero reports
  Soup generated patch and locked overlay: shared pre-open hook before CreateFileW
  All five native families: foreign claim check before HID open PASS
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: PASS; only allowlisted LNK4099 ViGEm PDB diagnostic
  ABI1 load/init/name/null/bounded-unload: PASS, SafeHID v13 interface-path
  Production overlay suite: PASS, maximum parallel state latency 2.1 ms
  Production Irok route: 45,873/45,874 queries, one contained transient miss,
    67 changed rows/notifications, 286 us average and 1,754 us maximum route
    interval; balanced shutdown and exit 0
  Trace analyzer: WARN only for unavailable protocol devices and unexercised
    unplug/alternate-mode variants; zero ERROR events
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,215,424 bytes
  SHA-256: CF1C3B93381744005B7B2D32FB54FF17A1F8D8244C2F12610070D89D77DE7EE3
  ABI0 DLL: 387,072 bytes,
    SHA-256 28F5E14AE3CCD30A74A3F73D3BDDE6757CC7CC2BB0F5B9E80AF500A353314B58
  ABI1 DLL: 259,072 bytes,
    SHA-256 F6EBC8A3A65F152AFF918BDC0DBFE1B811F2AEB8B4D9C985FCD618B05A254CD5
  Production trace SHA-256:
    18C2ABA260D85CD44CFC0DAE930BB4BA623D989B74FFB472B4EA232703E76242
Limit: no physical UAP or multi-UAP device was available. D-022 code-level
  substitution verifies routing/normalization/integration but cannot prove
  device-firmware coexistence or mathematically exclude every 64-bit collision
Remote CI: NOT RUN; optional, no push permitted
Status: HJ-AUD-P2-021 Verified by D-022/D-024 automated production-code gates
Next: V14-12 release qualification and hardware matrix
Evidence:
  docs/stability/tests/V14-11D_EXACT_HID_INTERFACE_OWNERSHIP_2026-08-01.txt
Rollback: parent commit of the V14-11D implementation commit
```

## V14-12A evidence

```text
Package: V14-12A
Scope: exact firmware-proven Aula WIN 60 HE MAX native read-only support
Commands:
  python src/HallJoyProject/tests/aula_win60he_backend_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  python tools/run_aula_win60he_sanitizers.py
  cl.exe /std:c++20 /EHsc /permissive- /W4 /WX /c
    (all eight new production/test translation units)
  cmd /c BUILD.cmd
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -StartOverlay
    -OverlayPort 18765 -RunSeconds 15
Results:
  Archive path/manifest/bundle audit: PASS
  Firmware verifier: 57/57 PASS
  Independent oracle-source hashes: 10/10 PASS
  Official oracle rerun: PASS, output hash identical
  Aula backend static audit: PASS
  Protocol/oracle/end-to-end/session-policy suites: PASS
  Complete repository gate: 39/39 static audits, 26/26 portable tests PASS
  GCC 15.2 warning-clean: PASS
  MSVC 19.44 /W4 /WX: PASS
  Clang 21.1.8 ASan+UBSan: PASS, zero reports
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: only allowlisted LNK4099 ViGEm PDB diagnostic
  ABI1 load/init/name/null/bounded-unload: PASS, SafeHID v13
  Production overlay suite: PASS, maximum parallel state latency 2.1 ms
  Production Irok route: 65,379/65,379 queries, zero failures,
    250 us average and 1,003 us maximum route interval
  Aula absent-device worker: one clean generation, fault_kind=0, joined
  Runtime user state: 16 files, zero differences; no process remained
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,268,672 bytes
  SHA-256: C3F1F954619059C900A2F47DF861C2A7B0D02C1E7F7646D800101CFF5183F833
  Production trace SHA-256:
    137915EA746C344FFF0C38E4C23E92ED88BEBD53B7E6AFBFA7FBEA2BC1616418
Hardware: Irok regression PASS; Aula physical hardware NOT AVAILABLE
Limit: exact V1.1.6 firmware only; Fn0 only; no physical input/hotplug,
  multiple-Aula or alternate-firmware claim
Status: Implemented under D-025; physical Aula gate remains open
Evidence:
  docs/stability/tests/V14-12A_AULA_WIN60HE_FIRMWARE_PROVEN_2026-08-01.txt
Rollback: checkpoint c4b6d36810581d716af322386040745ffca2042d
```

## V14-12B / S06 evidence

```text
Package: V14-12B / S06
Scope: Addressed reader-owned OVERLAPPED lifetime and bounded outer shutdown
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  cl.exe /std:c++20 /EHsc /permissive- /W4 /WX /c
    src/HallJoyProject/HallJoy/addressed_analog_backend.cpp
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1
    -InjectAddressedStopTimeout -RunSeconds 7
  cmd /c BUILD.cmd
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -RunSeconds 15
Results:
  Reader-only HID handle close and cancellation-reap audit: PASS
  No ForceCloseReaderHandle/TerminateThread: PASS
  Bounded 3000 ms main worker join and resource retention: PASS
  Registry truthful stop/poison and dependent cleanup containment: PASS
  Complete repository gate: 39/39 static audits, 26/26 portable tests PASS
  MSVC 19.44 /W4 /WX: PASS
  Simulator Addressed stop-timeout containment: PASS, exit code 2
  Packet constructors and session polling core: token-identical
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: only allowlisted LNK4099 ViGEm PDB diagnostic
  Production Irok route: 57,161 successful queries, one shutdown cancellation,
    291 us average and 434 us maximum route interval
  Shutdown: balanced, exit 0, zero trace ERROR; 11 user files unchanged
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,270,208 bytes
  SHA-256: 3D05EE5FA435343E633B991DC45B949C1484AD06CC086F993C6D788255510F7E
Hardware: Irok regression PASS; Addressed physical hardware NOT AVAILABLE
Status: Implemented; Addressed device gate deferred
Evidence:
  docs/stability/tests/V14-12B_S06_ADDRESSED_IO_OWNERSHIP_2026-08-01.txt
Rollback: parent commit of the V14-12B implementation commit
```

## V14-12C / S07 Hex80 evidence

```text
Package: V14-12C / S07 Hex80
Scope: bounded Hex80 worker generation, HID ownership and truthful containment
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  cl.exe /std:c++20 /EHsc /permissive- /W4 /WX /c
    src/HallJoyProject/HallJoy/hex80_backend.cpp
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1
    -InjectHex80StopTimeout -RunSeconds 7
  cmd /c BUILD.cmd
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -RunSeconds 15
Results:
  Native waitable worker and serialized start/stop: PASS
  Worker-owned active HID handle; owner cancellation without close: PASS
  Post-request stop guard before decode/publication: PASS
  Bounded 3000 ms join and retained-generation containment: PASS
  Complete repository gate: 40/40 static audits, 26/26 portable tests PASS
  MSVC 19.44 /W4 /WX: PASS
  Simulator Hex80 stop-timeout containment: PASS, exit code 2
  Hex80 protocol.cpp/.h: no Git diff
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: only allowlisted LNK4099 ViGEm PDB diagnostic
  Production Irok route: 57,276/57,276 successful queries,
    315 us average and 878 us maximum route interval
  Shutdown: balanced, exit 0, zero trace ERROR; 11 user files unchanged
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,271,232 bytes
  SHA-256: 540D4EB764FAD57E7431CA320F3E47420D7F0212C37A8D66EFFC1AAD3A6F6FAF
Hardware: Irok regression PASS; Hex80 physical hardware NOT AVAILABLE
Status: Implemented; Hex80 device gate deferred
Evidence:
  docs/stability/tests/V14-12C_S07_HEX80_LIFECYCLE_2026-08-01.txt
Rollback: parent commit of the V14-12C implementation commit
```

## V14-12D / S07 MAD68 evidence

```text
Package: V14-12D / S07 MAD68
Scope: bounded MAD68 generation with persistent-read ownership and final A9
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  cl.exe /std:c++20 /EHsc /permissive- /W4 /WX /c
    src/HallJoyProject/HallJoy/mad68pr_backend.cpp
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_analog_simulator.ps1
    -InjectMad68StopTimeout -RunSeconds 7
  cmd /c BUILD.cmd
  powershell -NoProfile -ExecutionPolicy Bypass
    -File .\tools\run_production_smoke.ps1 -RunSeconds 15
Results:
  Waitable worker and serialized start/stop: PASS
  Owner cancellation without cross-thread HID close: PASS
  Post-stop read publication blocked; new A8 blocked; final A9 retained: PASS
  Bounded 3000 ms join and retained-generation containment: PASS
  Complete repository gate: 41/41 static audits, 26/26 portable tests PASS
  MSVC 19.44 /W4 /WX: PASS
  Simulator MAD68 stop-timeout containment: PASS, exit code 2
  Protocol, command transports, A8/A9 strategy, restore and decoder: unchanged
  Production MSVC x64 Release: PASS, 0 errors
  Warning policy: only allowlisted LNK4099 ViGEm PDB diagnostic
  Production Irok route: 57,247 successful queries, one shutdown cancellation,
    250 us average and 498 us maximum route interval
  Shutdown: balanced, exit 0, zero trace ERROR; 11 user files unchanged
  Canonical artifact: build/output/HallJoy.exe
  Artifact size: 2,272,768 bytes
  SHA-256: 79F9E2509D56A80E71C17701F8FAB3DD65A39530E2F7F42C3E40E731AB139020
Hardware: Irok regression PASS; MAD68 physical hardware NOT AVAILABLE
Status: Implemented; MAD68 device gate deferred; S07 code complete
Evidence:
  docs/stability/tests/V14-12D_S07_MAD68_LIFECYCLE_2026-08-01.txt
Rollback: parent commit of the V14-12D implementation commit
```

## V14-12E normal-cycle qualification evidence

```text
Package: V14-12E
Scope: repeatable production start/graceful-stop release qualification harness
Commands:
  python tools/run_native_backend_checks.py --require-compiler
  cmd /c BUILD.cmd
  powershell -NoProfile -ExecutionPolicy Bypass -File
    tools/run_release_qualification.ps1 -Cycles 25 -RunSeconds 1
Results:
  Static audits: 42 PASS
  Portable C++20 tests: 26 PASS
  Official MSVC x64 Release build: PASS, 0 errors
  Normal production cycles: 25/25 PASS, fault injection false
  Shutdown: min 138 ms, max 326 ms, average 245.1 ms
  Peak HANDLE samples: 218 normally, one transient 225, final 218
  Remaining HallJoy processes: 0 after every cycle
  User state: 11/11 files, 0 changed
  Final trace: no ERROR; balanced shutdown; Irok route_ok=1830, route_fail=1
    where the single cancellation occurs during final shutdown
Artifact:
  build/output/HallJoy.exe
  Size: 2,272,768 bytes
  SHA-256: D0CCF7EF1743EDB301EA00FB8615E7AC2F3055E1B9BF612EFF046530E7F814EB
Evidence summary SHA-256:
  AA6C0B43202A7C48CC3CADB3EA4CDD0AFAE2FEB48560FF931426EBC37329C1CA
Limitations: 1000 cycles, 8-24h soak, physical key/input, reconnect and the
  unavailable device-owner matrix remain open. Analyzer WARN is expected for
  those unexercised hardware actions and is not promoted to PASS.
Evidence:
  docs/stability/tests/V14-12E_RELEASE_CYCLES_2026-08-01.txt
Rollback: parent commit of the V14-12E implementation commit
```

## V14-12F / S18 dependency guidance evidence

```text
Package: V14-12F / S18
Scope: remove embedded privileged installer and provide pinned manual guidance
Commands:
  python src/HallJoyProject/tests/dependency_installer_removal_static_audit.py
  python src/HallJoyProject/tests/dependency_lock_static_audit.py
  python tools/run_native_backend_checks.py --require-compiler
  cl /std:c++20 /EHsc /permissive- /W4 /WX dependency_guidance_policy_test.cpp
  cl /c /std:c++20 /EHsc /permissive- /W4 /WX app_deps.cpp
  cmd /c BUILD.cmd
  tools/run_release_qualification.ps1 -Cycles 3 -RunSeconds 2
Results:
  Installer/download/elevation/wait primitives absent: PASS
  Exact 1.22.0 URL and manual-only lock/production match: PASS
  Static audits: 43 PASS
  Portable C++20 tests: 27 PASS
  MSVC W4/WX targeted gates: PASS
  Official x64 Release build: PASS, 0 errors
  Normal production cycles: 3/3 PASS; exit 0; fault injection false
  Shutdown: 112-241 ms; max handles 209; remaining processes 0
  User state: 11/11 files, 0 changed
  Irok final trace: route_ok=6309, route_fail=0, trace ERROR=0
Artifact:
  build/output/HallJoy.exe
  Size: 2,206,208 bytes
  SHA-256: 6B5A3FB1009C1DB0C1916A3843A411EADA90DFAABFF5AE1D4EE08D2CC90E6C83
Limit: ViGEm was already installed, so the modal guidance was not manually
  displayed. Its exact message/policy and inability to execute are covered by
  pure/static gates. This is not a device compatibility gate.
Evidence:
  docs/stability/tests/V14-12F_S18_INSTALLER_REMOVAL_2026-08-01.txt
Rollback: parent commit of the V14-12F implementation commit
```

## V14-12G / S20 pre-qualification build and docs evidence

```text
Package: V14-12G / S20
Scope: current local gates/docs, x64-only project and W4 warning policy
Commands:
  python tools/run_native_backend_checks.py --static-only
  python tools/run_native_backend_checks.py --require-compiler
  cmd /c BUILD.cmd
  powershell -NoProfile -ExecutionPolicy Bypass -File
    tools/run_release_qualification.ps1 -Cycles 3 -RunSeconds 2
Results:
  Current Addressed catalog/lifecycle validator: PASS
  Repository static audits: 44 PASS
  Portable C++20 tests: 27 PASS
  Supported configurations: Debug|x64, Release|x64 only
  Warning policy: W4; narrow C4100/C4127/C4324/C4505 legacy baseline
  Official MSVC Release x64: PASS, 0 errors, 0 compiler warnings
  Direct MSVC Debug x64: PASS, 0 errors, 0 compiler warnings; release static CRT
    is intentional because bundled ViGEmClient.lib is /MT release
  Linker: only allowlisted external ViGEm LNK4099
  Normal production cycles: 3/3 PASS; shutdown 139-254 ms; max handles 209
  Remaining processes: 0; user state: 11/11 files unchanged
  Irok traces: no ERROR; SparkLink 17,674 success plus one expected
    shutdown-window cancellation
Artifact:
  build/output/HallJoy.exe
  Size: 2,206,208 bytes
  SHA-256: 01A046A667DA012237E12C597ED84BE531AF20BC6338F55881C1C5197272559A
Status: S20 Verified; pre-qualification work complete
Limits: this does not execute S21's 1000 cycles, 8-24 hour soak or unavailable
  physical-device matrix. Aula physical result remains release-blocking.
Evidence:
  docs/stability/tests/V14-12G_S20_BUILD_DOCS_2026-08-01.txt
Rollback: parent commit of the V14-12G implementation commit
```

## V14-12H / S21 qualification-runner evidence

Scope: persistent normal-cycle evidence and bounded long production soak.

- `run_release_qualification.ps1` now writes a checkpoint after every completed
  cycle, terminal pass/fail status, state manifests and per-cycle/aggregate
  Spark route counters.
- `run_long_soak.ps1` defaults to eight hours and records CSV resource samples,
  overlay probes, state hashes, trace/analyzer evidence and fixed HANDLE/private
  memory growth gates from a ten-second post-startup baseline.
- One-minute overlay pilot: PASS, 53 samples, HANDLE 210 -> 210 (max 210),
  private bytes -86,016, 234,846/234,846 successful Spark routes, no trace ERROR,
  no process survivor and 11/11 user files unchanged. Analyzer WARN is limited
  to manual-only input/reconnect/mode exercise.
- Official Release x64 build: 0 errors, 0 compiler warnings, only allowlisted
  external LNK4099. `HallJoy.exe` is 2,206,208 bytes, SHA-256
  `6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`.
- Post-build Irok regression: 3/3 PASS, shutdown 259-325 ms, max 209 HANDLEs,
  16,229 successful routes plus two shutdown-window cancellations, zero trace
  ERROR, zero process survivor and unchanged 11-file state.

Status: automation Verified; final S21 executions remain in progress.
Limits: 1000 cycles, 8-24 hours, manual input/reconnect, unavailable device
matrix and external Aula hardware validation are not claimed by this package.

## V14-12I / S21 1000-cycle evidence

Artifact: `build/output/HallJoy.exe`, 2,206,208 bytes, SHA-256
`6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`.

- Result: 1000/1000 production start/one-second-run/graceful-stop cycles PASS.
- Exit/process/state: exit zero every cycle, zero remaining HallJoy processes,
  before/after 11-file LocalAppData manifests identical.
- Trace: 1000/1000 files present, 1000/1000 recorded SHA-256 values reverified,
  zero mismatch, zero ERROR and zero capped/incomplete trace.
- Shutdown: min 101 ms, average 277.1 ms, p50 250 ms, p95 430 ms, p99 1315 ms,
  max 2662 ms; 16/1000 exceeded one second, all below the 15-second bound.
- Resources: maximum 217 HANDLEs, maximum working set 13,828,096 bytes; no
  cross-cycle process or user-state accumulation.
- Spark: 1,598,879 queries = 1,598,454 successful + 425 non-ok. Exactly 425
  cycles recorded one non-ok transaction, none recorded more than one; each was
  observed only in the worker shutdown statistics with no trace ERROR/fault.
- Evidence size: 11,617,796 bytes; machine summary/checkpoint status `passed`.

Status: 1000-cycle gate Verified.
Remaining: 8-24-hour soak, manual Irok input/reconnect, unavailable device-owner
matrix and external Aula hardware validation.

## V14-12K / S21 one-hour soak and Spark freshness correction

Qualification policy: D-029 accepts one hour continuous production plus the
verified 1000-cycle gate. The one-hour artifact was SHA-256
`6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`.

- Runtime envelope PASS: 3604.553 s, 703 samples, 12/12 overlay probes (max
  observed HTTP latency 0.6 ms), HANDLE 210 -> 211, private growth 180,224 bytes,
  shutdown 92 ms, processes 0, 11-file state identical.
- Trace review FINDING: three Spark worker generations, two impossible
  `silence_ms` values near `UINT64_MAX`, two false stale/reconnect cycles.
  Correct aggregate was 14,138,221 queries, 14,138,219 ok, two restart-window
  cancellations, 139,473 changed rows and 139,474 input notifications.
- The original runner summary reported only the final worker generation; it now
  aggregates all generations and rejects impossible freshness ages.
- Fix: `FreshnessAgeMs(now,last)` returns zero when `now < last`; deterministic
  test covers the exact observed arithmetic and the strict 1800 ms boundary.
- Full gate: 46 static audits and 28 portable C++ tests PASS. Official Release
  x64 build: 0 errors, 0 compiler warnings, allowlisted external LNK4099 only.
- Corrected artifact: 2,206,208 bytes, SHA-256
  `81609DC44D12F7DF44C2A7D801D8992CBFDCB45221F07AE041ED4711F4EB840C`.
- Targeted runtime: 120.666 s, 56 samples, 2/2 overlay probes, HANDLE 210 -> 211,
  private growth -12,288 bytes, 464,905/464,905 routes, one generation, zero
  stale/reconnect, zero ERROR/survivor/state change.

Evidence-retention note: the next official build removed earlier raw evidence
stored under `build/output`. Their committed, independently verified summaries
remain; current and future raw bundles use `build/evidence`. The corrected raw
bundle is `build/evidence/S21-spark-age-fix-2m`.

Status: Spark freshness fix and proportional requalification Verified.
Remaining: manual device-matrix coverage and external Aula hardware result.

## V14-12L / S21 second-pass stability-audit validation

Implemented checks:

- portable C++20 arithmetic regression covers backward monotonic observations,
  `UINT64_MAX`, expired/future deadlines and `INT_MIN`/`INT_MAX` mouse deltas;
- static integration audits verify Sayo/Addressed/Raw Mouse adoption and every
  clipboard/SetupAPI ownership exit;
- simulator ViGEm-output exception injection proves generation 1 faults, is
  confirmed stopped, recreates its transport, resubmits and shuts down a normal
  generation 2;
- simulator overlay exception injection proves the completed generation is
  owner-reaped and the enabled service is restarted without resource overlap;
- exception-barrier, ViGEm isolation, overlay cooperative-shutdown and unified
  static-only gates pass after the recovery-policy change.

Completed qualification:

- unified gate: 48 static audits and 29 portable C++20 tests PASS;
- Aula protocol/oracle/end-to-end/session suites: Clang ASan+UBSan 4/4 PASS;
- locked fresh Soup/plugin generation: PASS after updating the two intentional
  overlay hashes and the safe initialiser generator contract;
- official Release x64: 0 errors, 0 compiler warnings, allowlisted external
  ViGEm `LNK4099` only;
- normal Irok production regression: 3/3 PASS, 18,103/18,105 Spark routes,
  only two shutdown-window cancellations, 196-232 ms shutdown, maximum 215
  HANDLEs, zero survivor and all 11 user-state files unchanged.

Artifact: `build/output/HallJoy.exe`, 2,209,280 bytes, SHA-256
`9C5C206E196753D25C83F2DE012607B6ED372AEB3B72593E410865FF4B0777D4`.

Status: V14-12L Verified. External Aula hardware validation remains open and
release-blocking; no physical Aula claim is added by this package.

## V14-12M / S21 MAD68 HE UAP-shutdown validation

Classification and static/portable gates:

- MAD68 HE is covered by the private modified UAP/Soup path; MAD68 Pro R is a
  separate native A0 backend and was not used to infer the tester's failure;
- unified native gate: 48 static audits and 29 portable C++20 tests PASS,
  including the 250,000-frame parser fuzz smoke;
- dependency lock/notice audit PASS for exact Sun and Soup commits and required
  `THIRD_PARTY_NOTICES.md` packaging.

Deterministic process containment:

- `-InjectAnalogHostChildStopHang`: PASS. The child blocked forever before
  plugin unload, emitted `child.stop_timeout` after approximately 2.64 seconds,
  was terminated with restart disabled, reaped, and the parent exited zero;
- `-InjectMad68OwnerStopHang`: PASS. The owner thread blocked before any native
  join and the independent watchdog terminated the process after 12 seconds
  with expected exit code 4 and no survivor;
- normal simulator: PASS. These are lifecycle tests, not MAD68 HE hardware
  evidence. Raw traces are retained in
  `build/evidence/V14-12M-uap-shutdown-20260801`.

Production artifact and runtime:

- clean locked UAP generation, ABI gate and official x64 Release build PASS;
  zero compiler warnings and only allowlisted external ViGEm `LNK4099`;
- `build/output/HallJoy.exe`: 2,210,304 bytes, SHA-256
  `C06AD4C7257244E4370738465BBADF815DBC41A081065CA30B0BCFF8059FA1A3`;
- 5/5 Irok MG75 Max production cycles PASS, exit zero, 122-226 ms shutdown,
  maximum 209 HANDLEs, 33,461 successful routes plus one shutdown-window
  cancellation, zero survivor and unchanged 11-file user state;
- evidence: `build/evidence/release-qualification/20260801-235837`.

Status: code-level containment Verified. `HJ-V14-P1-008` remains Partial and
release-blocking until the physical MAD68 HE tester closes this exact artifact.
The external Aula hardware gate remains independently release-blocking.

## V14-12N / S21 all-keyboard shutdown-matrix validation

Commands:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_keyboard_shutdown_matrix.ps1 -RunSeconds 7
python .\tools\run_native_backend_checks.py
cmd /c BUILD.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_release_qualification.ps1 -Cycles 5 -RunSeconds 2 -ProgressEvery 1
```

Deterministic process matrix: 9/9 PASS. The scenarios are normal common
pipeline, permanent SparkLink stop, Sayo stop, Addressed stop, Hex80 stop,
MAD68 Pro R stop, Aula stop, private UAP/Soup child unload and the independent
process watchdog. Each scenario matched its exact trace and exit contract; all
traces were copied and SHA-256 hashed, and every post-scenario survivor count
was zero. Evidence:
`build/evidence/keyboard-shutdown-matrix/20260801-212947/summary.json`.

The full unified gate, locked private UAP build and official x64 Release build
pass with zero errors and zero unexpected warnings. Production artifact:
`build/output/HallJoy.exe`, 2,210,304 bytes, SHA-256
`E12080E95DD394462FC36C842517F168F6CE4423CE9357B89D2320A20A962BB8`.

Physical available-device regression: Irok MG75 Max 5/5 PASS, exit zero,
140-234 ms shutdown, maximum 209 HANDLEs, 33,409 successful SparkLink queries
plus three shutdown-window cancellations, zero survivor and unchanged 11-file
state. Evidence:
`build/evidence/release-qualification/20260802-003755/summary.json`.

Status: code-level shutdown containment Verified for every production route.
The matrix is explicitly simulator-only (`hardware_verified=false`) and does
not close physical protocol/input/hotplug gates. MAD68 HE physical retest and
Aula physical acceptance remain release-blocking.

## V14-12O / S21 input-to-overlay load validation

Commands:

```powershell
python .\src\HallJoyProject\tests\overlay_render_efficiency_static_audit.py
python .\src\HallJoyProject\tests\input_pipeline_profile_static_audit.py
cmd /c BUILD.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_input_pipeline_profile.ps1 `
  -ExePath .\build\output\HallJoy.exe -PhaseSeconds 15 `
  -BrowserWarmupSeconds 5
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_release_qualification.ps1 `
  -ExePath .\build\output\HallJoy.exe -Cycles 5 -RunSeconds 2 `
  -ProgressEvery 1
```

Static contracts and the complete official build gate PASS. Release x64 has
zero errors and only the allowlisted external ViGEm `LNK4099`. Production
artifact: `build/output/HallJoy.exe`, 2,212,352 bytes, SHA-256
`06CF73B59827E957DDF9644AC2557C601F5602FD454AC7B025FB6533C041A462`.

Final 15-second-per-phase evidence:
`build/evidence/input-pipeline-profile/20260802-113109/summary.json`.

- physical Irok/SparkLink: 277,055/277,055 successful route transactions,
  average 281 us, maximum 1,737 us;
- server idle: complete HallJoy tree 0.880% machine CPU;
- real overlay at the user's existing 1 ms setting: HallJoy 0.809%, Chrome
  5.451%; Spark 0.656%, realtime 0.017%, UI/short-lived HTTP 0.119%; 465.2
  state requests/s, JSON build 26.7 us average/409 us maximum, response send
  28.3 us average/227 us maximum;
- animated 32-key browser stress: HallJoy 0.766%, Chrome 14.110%; this is an
  intentionally continuous-redraw upper-pressure phase, not normal idle use;
- maximum HallJoy tree footprint 29.3 MiB working set, 9.6 MiB private, 515
  HANDLEs and 25 threads; unchanged user state and zero surviving processes.

The exact final-artifact 8 ms comparison is
`build/evidence/input-pipeline-profile/20260802-113432/summary.json`: HallJoy
0.721%, Chrome 3.747%, 143.2 state requests/s, one settled draw, 217,628/217,628
Spark routes, unchanged state and zero survivors. The user file was restored
byte-for-byte to its original 1 ms setting and SHA-256
`E1ED9B550CA111414D7558D7784CE6524C8C0A41CDB1CA81C89E3CD46B813BAE`.

Post-profile lifecycle qualification is 5/5 PASS, exit zero, 128-219 ms
shutdown, maximum 209 HANDLEs, unchanged 11-file state and zero survivors;
evidence is
`build/evidence/release-qualification/20260802-112901/summary.json`.

Status: V14-12O Verified for physical Irok and the shared realtime/ViGEm/
overlay/browser path. Headless Chrome uses disabled background throttling for a
repeatable comparison and is not an exact OBS claim. Physical MAD68 HE/UAP and
Aula validation remain release-blocking.

## V14-12P recoverable factory-reset validation

Commands:

```powershell
python .\src\HallJoyProject\tests\factory_reset_static_audit.py
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_factory_reset_test.ps1
python .\tools\run_native_backend_checks.py --static-only
cmd /c BUILD.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_release_qualification.ps1 `
  -ExePath .\build\output\HallJoy.exe -Cycles 1 -RunSeconds 2 `
  -ProgressEvery 1
```

Results:

- reset UI/static contract: PASS; one owner-draw action on Global settings,
  complete warning, explicit confirmation and `No` default;
- runtime transaction: PASS; atomic request, injected failure after three
  moves, byte-exact reverse rollback, successful retry, five exact backup
  hashes, preserved unrelated/migration files and zero survivors;
- unified static audit set: PASS;
- official x64 Release build: PASS, zero errors and only the allowlisted
  external ViGEm `LNK4099`;
- final `HallJoy.exe`: 2,225,664 bytes, SHA-256
  `33BEB1DE0DA8B896FA82E61A52F29ED4A8796B09A7134325346971C70CFEC597`;
- physical available-device regression: 1/1 PASS, 6,542/6,542 SparkLink
  queries, 164 ms shutdown, maximum 209 HANDLEs, unchanged 12-file user state
  and zero survivors.

Runtime reset evidence is
`build/evidence/factory-reset/20260802-121843-951/summary.json`; production
evidence is
`build/evidence/release-qualification/20260802-122413/summary.json`.

Status: V14-12P Verified. The backup is intentionally retained until the user
chooses to remove it; reset does not broaden any physical keyboard-support
claim or close the MAD68 HE/Aula release blockers.

## V14-12Q Global-settings scroll and danger-fill validation

Commands:

```powershell
python .\src\HallJoyProject\tests\factory_reset_static_audit.py
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_factory_reset_test.ps1 -SkipBuild
python .\tools\run_native_backend_checks.py --static-only
cmd /c BUILD.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_release_qualification.ps1 `
  -ExePath .\build\output\HallJoy.exe -Cycles 1 -RunSeconds 2 `
  -ProgressEvery 1
```

Results:

- six-tab inventory and page-overflow contract: PASS; Remap, Configuration,
  Global settings, Input Overlay and Mouse settings own themed scrolling;
  Gamepad Tester uses adaptive card height instead of overflowing;
- Global settings interaction contract: PASS; common track/thumb drawing,
  bounded wheel and vertical-command scrolling, track paging, thumb capture/
  drag and teardown are present;
- danger action contract: PASS; idle fill is `RGB(108, 35, 43)`, with distinct
  hover/press colors and danger-specific border/text;
- reset runtime regression: PASS, including injected partial-move rollback and
  successful retry; evidence is
  `build/evidence/factory-reset/20260802-124758-494/summary.json`;
- unified static audits and official x64 Release build: PASS, zero errors and
  only the allowlisted external ViGEm `LNK4099`;
- final `HallJoy.exe`: 2,228,224 bytes, SHA-256
  `6DFC616422D89783A846F7EE8CAEA64AD7D951591092576DEFB717320543DF96`;
- physical available-device regression: 1/1 PASS, 6,660 successful SparkLink
  queries, 193 ms shutdown, maximum 209 HANDLEs, unchanged 12-file user state
  and zero survivors. Evidence is
  `build/evidence/release-qualification/20260802-125134/summary.json`.

Automated status: PASS. Manual physical-Irok UI review after this gate: REJECTED.
V14-12Q is reopened because analog input visibly flickers static UI/tab areas,
Configuration telemetry refresh depends on hover, the two poll selectors are
painted click-cyclers rather than real comboboxes, diagnostics are duplicated,
and the reset accent/focus states were rejected. See
`RELEASE_UI_AUDIT_HANDOFF_2026-08-02.md`. Scrollbar visibility remains
conditional on actual overflow. Physical MAD68 HE/UAP and Aula acceptance
remain independent release blockers.

## V14-12R clean pre-release UI audit

Commands:

```powershell
python .\src\HallJoyProject\tests\pre_release_ui_static_audit.py
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_release_qualification.ps1 `
  -ExePath .\build\output\HallJoy.exe -Cycles 3 -RunSeconds 2 `
  -EvidenceRoot .\build\evidence\pre-release-ui-audit-20260802\release-qualification
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\run_analog_simulator.ps1 -SkipBuild -RunSeconds 15
```

Results:

- UI static regression audit: PASS;
- normal production and release-excluded UI-audit builds: PASS, zero compile
  errors and only allowlisted external ViGEm `LNK4099`;
- instrumented physical Irok/SparkLink process: Configuration steady-state
  repaint is only `720x36` at 10 Hz; tab row does not repaint for telemetry;
  clean shutdown has exit 0 and no crash/vectored artifact;
- production lifecycle: 3/3 PASS, 85–241 ms, max 213 HANDLEs,
  19,542/19,542 SparkLink queries, unchanged 18-file user state, zero survivor;
- simulator: the initial 8-second run was too short for two late opposing-key
  phases; the already-built 15-second isolated-data rerun PASS;
- final `HallJoy.exe`: 2,228,224 bytes, SHA-256
  `6BCBA47D86448E7D262250AEAF7EAFD89FD448CA8A917B1C514711F31FCB6CC3`.

Automated/code-log status: PASS. Visual status: PENDING OWNER REVIEW. No UI
item is `Verified`. Full details and the remaining matrix are in
`PRE_RELEASE_UI_AUDIT_2026-08-02.md`. Physical MAD68 HE/UAP and Aula remain
independent release blockers.

## V14-12S UI refresh root correction

- backup: `backup/pre-ui-refresh-root-fix-20260802` plus full dirty patch in
  `build/backups/ui-refresh-root-fix-20260802-142333`;
- `pre_release_ui_static_audit.py`: PASS, including always-visible preview,
  Tester cadence, frame-coalesced scroll, atomic combo layout and 15% default;
- full production build: PASS, 0 compile errors, only allowlisted `LNK4099`;
- UI-audit build and six-tab scalar-message harness: PASS, exit 0 and normal
  shutdown marker; no screenshots;
- Configuration stress drag: 24 paints in the active burst, 0 erase versus the
  pre-correction 120 paints/s and 258 erase messages;
- focused tab row under input: selection-only 1–2 paints versus about 60/s;
- simulator, isolated data root, 15 s: PASS;
- final production qualification: 3/3 PASS, shutdown 148–249 ms, max 209
  HANDLEs, unchanged 22-file user state, zero survivors;
- exact artifact: 2,230,784 bytes, SHA-256
  `1B5671F36EDE9CD2CB1153A2D02729387D0974D25FFB031D457A4FDC7AB523D1`.

Automated/code-log status: PASS. Visual status: PENDING OWNER REVIEW.
Evidence root: `build/evidence/ui-refresh-root-fix-20260802/`.

## V14-12S.1 adaptive scroll cadence

- pre-fix reproduction: Configuration approximately 29 FPS under continuous
  thumb input because `WM_TIMER` was delayed and 15/16 ms deadlines alternated;
- final alternating stress: approximately 70 active commits/s, 0 active erase,
  clean diagnostic exit and normal shutdown marker;
- static UI audit and full production build: PASS;
- exact final production qualification: 3/3 PASS, shutdown 190–341 ms, max 209
  HANDLEs, 22-file state unchanged, zero survivors;
- exact artifact: 2,231,808 bytes, SHA-256
  `F9A32FEB956E7ED38CE7CB75BBE8254D64B296259821B9922ECA8EFF9D5B040C`.

Evidence: `build/evidence/ui-refresh-root-fix-20260802/ui-scroll-alternating-final/`
and `release-qualification-scroll-final/`. Owner visual recheck of scroll
smoothness is pending.

## V14-12T unified viewport evidence

Previous V14-12S.1 visual result: REJECTED by owner because elements disappeared
during otherwise faster scroll. Its measured cadence is not release evidence.

- backup branch: `backup/pre-unified-scroll-architecture-20260802` at
  `63024a9907dc946f4533c94a459fd65acec03df5`;
- dirty backup: `build/backups/unified-scroll-architecture-20260802-150715`;
- `pre_release_ui_static_audit.py`: PASS;
- `factory_reset_static_audit.py`: PASS;
- full production build: PASS, 0 errors, one allowlisted `LNK4099`;
- six-page runtime scroll stress: PASS, 240 wheel/update cycles per page;
- stress steady-state GDI start/max/end: `211/211/207`;
- stress steady-state USER start/max/end: `227/227/227`;
- stress shutdown: exit 0;
- production lifecycle qualification: 3/3 PASS, shutdown 133–223 ms, max 209
  handles, 22 user-state files unchanged, zero survivors;
- exact artifact: 2,232,832 bytes, SHA-256
  `851C84A63AB6A1532C21E4FE477A16F9D9248BF4AB76090E3DD9AEAA969C2509`.

Evidence:
`build/evidence/pre-release-ui-audit-20260802/ui-scroll-stress-unified-release/`
and `release-qualification-unified-release/`.

Automated/code-log status: PASS. Visual status: PENDING OWNER REVIEW. No
screenshots were produced.

## V14-12T.1 retained-control regression evidence

- static UI audit: PASS; canonical retained combo renderer, no second focus
  outline, popup close notification and vector glyph rules are guarded;
- full production rebuild: PASS, 0 errors, one allowlisted `LNK4099`;
- scroll stress: all 6 pages PASS, 240 wheel/update cycles per page;
- combo lifecycle: Configuration preset, Global profile and Keyboard layout
  each expose one popup/controller while open and zero after Escape close;
- steady-state GDI handles: `151/151/151` start/max/end;
- steady-state USER handles: `200/200/200` start/max/end;
- lifecycle qualification: 3/3 PASS, shutdown 104–223 ms, max 201 handles,
  28 state files unchanged and zero survivors;
- artifact: 2,233,344 bytes, SHA-256
  `76CB02D76131E72B78652969C3673F3A2E586BCB843FD4ED57B78F51F6FD4677`.

Evidence:
`build/evidence/pre-release-ui-audit-20260802/ui-scroll-combo-regression-release/`
and `release-qualification-combo-regression-release/`.
Automated status: PASS. Visual status: PENDING OWNER REVIEW. No screenshots.

## V14-12T.2 interaction-semantics evidence

- static audit: PASS for bounded Remap hits, shared binding dirty transaction
  and non-cycling Overlay combos;
- full production build: PASS, 0 errors, one allowlisted `LNK4099`;
- scroll stress: 6/6 pages, 240 cycles each; popup lifecycle 6/6 PASS;
- GDI `203/207/204`; USER `231/231/230`; exit 0;
- artifact: 2,232,832 bytes, SHA-256
  `8BE26D58294AD7E02D38C0970E4A8F6BD321A976638DC0E809A0425020D714CA`;
- evidence: `build/evidence/ui-scroll-stress/20260802-170602/`.

Automated status: PASS. Owner visual/interaction review pending. No screenshots.

## V14-12T.3 popup-wheel evidence

- static UI audit and full production build: PASS;
- six-page stress: 6/6, 240 cycles/page;
- popup lifecycle/wheel routing: 6/6, six injected wheel messages per popup;
- font popup remained open/responsive during routing and closed cleanly;
- GDI `105/105/102`; USER `174/177/176`; exit 0;
- evidence: `build/evidence/ui-scroll-stress/20260802-174747/`;
- artifact: 2,232,832 bytes, SHA-256
  `45AEDB2FA1952843B004FED3F80EAD7F18DC8357FAD5EF2D64BBE237A6AC221B`.

No screenshots were produced; owner visual confirmation remains pending.

## V14-12T.4 overflow-only popup scroll evidence

- V14-12T.3 behavior rejected: wheel routed to `MoveHot` rather than viewport;
- static audit and full production build: PASS;
- five fitting popups: `maxTop=0`, `scrollTop 0->0`, selection unchanged;
- 13-item font popup: `maxTop=3`, `scrollTop 1->3`, selection unchanged;
- six-page stress: 6/6, 240 cycles/page;
- GDI `105/106/102`; USER `176/176/176`; exit 0;
- evidence: `build/evidence/ui-scroll-stress/20260802-175747/`;
- artifact: 2,233,856 bytes, SHA-256
  `ED0082DFDC24F8A4137B1559D1B43058186C19ED4B9142E7F9BEE107F65EB00D`.

No screenshots. Owner visual confirmation remains pending.

## V14-12U Aula physical diagnostic evidence

- input log SHA-256 `BA14561242740E0985F3A12EDE325DA926436008E9C76FD1AA2815279018CFED`;
- observed: 9 Spark opens of Aula `1CA2:1902 / FFA0`, 9 failed foreign probes;
- new Spark dedicated-Aula pre-open gate: static audits PASS;
- Aula protocol/oracle/end-to-end/session-policy sanitizers: 4/4 PASS;
- production Release compile: PASS, no unexpected warning;
- aggressive diagnostic Release compile: PASS, only allowlisted `LNK4099`;
- full native backend static/portable suite: PASS;
- diagnostic EXE: 2,245,632 bytes, SHA-256
  `D6F48D134481668DE9819A457CEFFC6FE5A97F5A6BD5980CFE6FE4529F1F8036`;
- delivery directory: exactly one `HallJoy.exe`;
- embedded output contract: one overwritten `HallJoy.log`, maximum 64 MiB;
- old trace names, BAT/PowerShell/Python collectors and portable marker absent.

Status: root cross-backend collision Implemented/PASS. Exact physical Aula
capability result remains Pending the aggressive `HallJoy.log` from the tester.

## V14-12U.1 physical sync-envelope evidence

- `HallJoy (1).log`: 37,904 bytes, SHA-256
  `E01FC3176933C7BB32DD72AABC07F4D6364CD344D217ABBCA6C58570BDBD1B5E`;
  clean exit, 9/9 transient exclusive-open error 32, no Aula TX;
- `HallJoy (2).log`: 325,015 bytes, SHA-256
  `9103611B57190C541A15136CD7FF98DBBF9D0D5AC3AE8689CE1115A7D919A006`;
  62/62 exclusive opens and stable checksum-valid `5C 3C 81 4D` sync frames;
- root: inferred 54-byte first-report gate rejected physical 60-byte payload;
- normal ASan/UBSan tests preserve strict production rejection;
- aggressive ASan/UBSan end-to-end fixture reproduces the physical envelope,
  completes all 17 read-only transactions and retains firmware mismatch;
- sanitizers 5/5, static audits and full native backend suite: PASS;
- production and aggressive MSVC Release builds: PASS, no unexpected warnings;
- diagnostic package: exactly one 2,245,632-byte `HallJoy.exe`, SHA-256
  `4AC9B51E9EE1824E6050400FF09F94B763084EEEE7A816A6D8A4290E938D54CA`.

Status: first physical parser barrier corrected for diagnostics. Strict physical
support remains Pending the next trace; claim/publication remain blocked.

## V14-12U.2 complete physical proof evidence

- input: `HallJoy (3).log`, 66,147 bytes / 223 lines, SHA-256
  `30FFE7CFB512F9FCE5988D71FF38D2F58922957DCEE7B675CA5113E7A7979DAB`;
- clean lifecycle: `session.end exit_code=0`;
- exclusive open: 3/3; full proof: 3/3, failure stage none;
- physical sync: stable 60-byte payload, exact descriptor blocks;
- precision/min/max: `10/10/3400` micrometres;
- physical/default/active mapped: `61/60/60`;
- two active-map generations stable in every proof;
- both 128-byte travel halves structurally valid in every proof;
- observed travel values: all zero during initial proof; runtime non-zero
  physical travel remains the next owner gate;
- Spark dedicated-family skip remains active; no shared-open fallback.

Status: physical production identity and complete read-only proof Implemented.
Non-zero runtime travel and reconnect remain Pending physical recheck.

- ASan/UBSan Aula suites: 5/5 PASS;
- full native backend checks: PASS;
- production MSVC Release: PASS, 0 errors, no unexpected warnings;
- aggressive MSVC Release: PASS, 0 errors, only allowlisted `LNK4099`;
- production EXE: 2,235,392 bytes, SHA-256
  `8F6F85CD17BCE471728006BA4642203142815C708407C64E2C21F5DCF25817D0`;
- diagnostic EXE: one 2,245,120-byte file, SHA-256
  `23CEC8D7EF2479B353EABE7AAB8857CB04BDF3CC6BD0FE3D1FC89DAA1C02BB14`.

## V14-12U.3 physical runtime-input evidence

- `HallJoy (4).log`: 1,840,992 bytes / 9,311 lines, SHA-256
  `8529C724EDA13F93892237B11C5012D85FA8CD36A38BC71D85B64EF4BAC7E52C`;
- `HallJoy (5).log`: 629,044 bytes / 3,015 lines, SHA-256
  `5C1FC4F0DFC1AE152EA395463CD458C6123F08A27F783C05E2B8E9B8EDFF2A48`;
- strict proof and claim: 2/2 PASS, `mismatch_mask=00000000`,
  `routing.claimed=1`, `connected=1`, mapped usages 60;
- sustained physical runtime polling: about 58 minutes and 17.5 minutes;
- physical result reported by tester: analogue key travel visible in HallJoy;
- byte-level boundary: the 256-report cap retained only startup traffic; all
  58 complete retained travel-half frames per log are zero, so no non-zero raw
  byte capture is claimed;
- disconnect/retry: PASS in log 4; one read failure, followed by 59 searches
  after the device disappeared from HID enumeration;
- reconnect: NOT TESTED, because the device did not return before shutdown;
- log 5 terminal continuation-read failure coincides with shutdown start and
  is an in-flight read cancellation, not a sustained-runtime transport fault;
- both process lifecycles ended cleanly with `exit_code=0`.

Status: physical Aula identity, claim, runtime polling and owner-observed
analogue input PASS. Return-after-disconnect reconnect remains Pending.

## V14-12U.4 diagnostic telemetry v2 evidence

- root reproduced from logs 4/5: raw cap consumed by startup, no runtime Hz or
  10-key evidence; 8,695/2,635 duplicate Spark skip lines; shutdown cancellation
  presented as a protocol warning;
- new fixed 5-second health windows: exact completed-matrix Hz, interval and
  two-transaction min/avg/max, seven latency buckets;
- active-key evidence: `0/1/2-4/5-9/10+` frame histogram, changed/nonzero counts,
  press and release-to-zero transitions;
- event evidence: first nonzero, every new simultaneous-key maximum, first
  10-key frame, HID/row/column/travel values;
- final coverage: maximum observed micrometres per HID and observed-HID count;
- lifecycle: per-session summary, disconnect reason, reconnect downtime and
  strict retained-identity proof;
- noise correction: Spark dedicated skips aggregated to <=1 event/minute;
  shutdown cancellation is INFO and excluded from failed-update counts;
- diagnostic metrics ASan/UBSan and logic suite: PASS; Aula sanitizer total 6/6;
- full native static/portable checks: PASS;
- diagnostic MSVC Release: PASS, 0 errors, only allowlisted `LNK4099`;
- official production MSVC Release: PASS; linked high-detail markers absent;
- package: exactly one 2,254,336-byte `HallJoy.exe`, SHA-256
  `F2727D0A7E901DF89D95B27B1D0CD86D7D2B9655B59EB4998F62594FCAF158C5`.

- held-key unplug ordering: disconnect event follows authoritative clearing and
  records active-before-clear plus published-active-after-clear zero;

Status: telemetry implementation and automated gates PASS. One physical run
must now close 10-key, measured-rate and return-after-disconnect acceptance.

## V14-12U.5 physical matrix-rate and 10-key evidence

- input: `HallJoy (7).log`, 142,904 bytes / 417 lines, SHA-256
  `EBDDF2DCEA3D72BBCA1E6219A340312A0BB55167826F6BC2187FC41079B968A9`;
- lifecycle: 61.703 seconds, clean `exit_code=0`;
- strict proof/claim/connect: PASS, mismatch zero, mapped 60;
- matrices: 21,027 successful / 0 failed over 61.129 seconds;
- lifetime rate: 343.973 Hz; 5-second window min/max 340.245/346.178 Hz;
- matrix interval min/avg/max: 2,138/2,907/15,812 us;
- two-travel transaction min/avg/max: 1,677/2,076/2,644 us;
- transaction buckets: all 21,027 <=4 ms, none above 4 ms;
- simultaneous keys: maximum 22; 2,654 frames in the 10+ bucket;
- complete press/release cycles: 8/8; final active state zero;
- coverage: 40 non-zero HID usages; maximum travel 3,400 um;
- protocol errors, semantic mismatches and runtime failed updates: 0;
- shutdown cancellation classified as INFO; all worker joins PASS;
- disconnect/reconnect events: 0/0, therefore return-after-disconnect NOT TESTED.

## V14-12U.6 physical reconnect and analogue recovery

- input: `HallJoy (8).log`, 592,665 bytes / 1,800 lines, SHA-256
  `3360D442A527DA993E846B6F88456406BAD2EADD02B4A18E3FAF49C63A0041C7`;
- lifecycle: three `disconnected` events and three `reconnect.success` events;
- identity/proof: all recovered sessions retain the same path/instance hashes
  and pass strict proof with `mismatch_mask=00000000`;
- post-reconnect analogue: session 2 records 1,008 matrices at 341.463 Hz,
  329 non-zero frames, 238 changed frames, four press/release-to-zero cycles and
  12 observed HIDs up to 3,400 um;
- transition behavior: the device temporarily exposes HID paths before firmware
  writes are ready during further USB cycles; retries remain bounded and recover;
- shutdown: cancellation is classified as INFO, every worker joins and process
  exits with code 0;
- result: physical disconnect, reclaim, strict re-authentication and real analogue
  recovery PASS. `HJ-AULA-P1-009` is closed.

## V14-12U.7 final production telemetry and smoke gate

- build command: `powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1`;
- native/static/portable suite: PASS;
- MSVC Release x64: PASS, 0 errors, 0 unexpected C/LNK warnings; the sole
  `LNK4099` is the pinned third-party ViGEmClient missing-PDB baseline;
- compile-time hot path: ordinary `DebugLog_Write*`, checkpoints and all
  `StabilityTrace_*` events use discarded constexpr call sites; their arguments
  are type-checked but never evaluated or emitted in production;
- linked-image audit: no `HallJoyStabilityTrace`, `HallJoyDiagnostic.log`, matrix
  health/activity/session/coverage or 10-key telemetry marker; `HallJoyCrash.txt`
  is present;
- runtime smoke: isolated portable launch, five seconds running, four PID-owned
  top-level windows accepted `WM_CLOSE`, process exited 0 within the deadline;
- smoke filesystem: zero ordinary, stability, diagnostic or crash log files;
- clean release package: EXE, Russian README, third-party notices and SHA256SUMS;
- artifact: `build/release/HallJoy.exe`, 2,161,152 bytes, SHA-256
  `AF7C536FF454AF94278C457E2A978E447E9345580240253F7B603748AB79C39F`;
- result: PASS, suitable for final user release.

Status: physical rate, latency, 10-key+, release-to-zero, range and shutdown
PASS. Reconnect remains the only open Aula physical gate.

## V14-12V bounded Aula family proof

- backup: `build/backups/pre-aula-family-protocol-20260805-001`, copied hashes
  matched source files before the architecture change;
- `python tools\run_aula_win60he_sanitizers.py`: PASS 6/6 under Clang
  ASan+UBSan, including exact/oracle, alternate 84-position family profile,
  diagnostics and session policy;
- Aula backend static audit: PASS; family discovery is brand-scoped, proof is
  dynamic and bounded, incompatible diagnostics cannot claim or publish;
- protocol-family routing audit: PASS;
- `python tools\run_native_backend_checks.py --require-compiler`: PASS, all
  static and portable C++ tests;
- `powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1`: MSVC
  Release x64 PASS, 0 errors, only allowed third-party `LNK4099`, no continuous
  telemetry markers in the linked image;
- artifact: 2,164,224-byte `build/release/HallJoy.exe`, SHA-256
  `2833DA24AF9D086A084B045FCEEA78F08883536FD96F48E1EFEEC938B652E1BB`;
- physical scope: unchanged and proven for WIN 60 HE MAX; alternate family
  profiles are implementation-tested but not hardware-tested.

## V14-13 Keychron K4 HE ANSI physical protocol gate

- device identity: `VID 3434 / PID 0E40 / Usage FF60:0061`;
- transport: 33-byte input and output reports on the vendor HID interface;
- official source evidence: `Keychron/qmk_firmware`, branch
  `hall_effect_playground`, commit
  `bc56b3c611dcc1a8ed9a2acb8bdc4da5e1a80c27`;
- exact layout: ANSI 100-key, protocol matrix 6x19 (114 slots, 14 holes);
- protocol: read-only `A9 01` version discovery plus `A9 30` per-key travel;
- private UAP ABI runtime: PASS, exactly one device detected and bounded unload
  completed;
- 20-second physical input run: seven active key codes, 202 distinct analogue
  levels, maximum normalized travel 1.0000, 567 value transitions and 537
  coherent worker updates;
- static K4 route/matrix/provenance audit: PASS;
- firmware operations: none; stock firmware retained because the required
  protocol is already present;
- backup: `build/backups/pre-keychron-k4he-20260807-001` with matching hashes.

Additional release evidence:

- official MSVC Release x64 build: PASS, zero errors, only allowlisted third-party
  `LNK4099`, zero unexpected warnings;
- production linked-image audit: continuous telemetry markers absent and
  crash-only marker retained;
- final embedded DLL runtime: one `3434:0E40` device, `FF60:0061`, 6x19,
  114 slots, 236 nominal levels, 68 updates in the two-second idle proof and
  clean bounded unload;
- overlay smoke: responsiveness, fragmented/pipelined HTTP framing,
  hostile-origin/client-limit concurrency and 500-case parallel fuzz PASS;
- normal release qualification: 3/3 cycles PASS, shutdown 118-217 ms, no
  surviving HallJoy process, all 30 user-state files byte-identical, zero
  continuous/crash logs;
- release artifact: 2,165,248-byte `build/release/HallJoy.exe`, SHA-256
  `B8FFE5ACB43DDDDB2C0C9634057E7D5A34771E90F1BF9907B0576E5E4A7762ED`;
- clean package restored to four files after smoke-generated DLL/log evidence
  was moved to `build/evidence/keychron-k4he-20260807/smoke-generated`.

Status: physical analogue protocol, multi-key publication, final production
build, embedded-resource identity, smoke and bounded release cycles PASS.

### V14-13 regression finding: stock K4 HE latency

The earlier PASS established protocol compatibility and correct values, but it
did not measure first-value latency below the keyboard's digital actuation
point. A subsequent physical gameplay-oriented check invalidated the release
claim: stock K4 HE firmware exposes only per-key `A9 30`; a previously idle key
can wait for the background 6x19 matrix sweep and appears roughly one second
late, while crossing digital actuation merely promotes that key in the polling
queue. Correct value range and release-to-zero do not compensate for this
latency.

Status correction: **FAIL for release support on stock firmware**. K4 HE ANSI
must remain excluded until the `A9 31` full-matrix firmware is flashed and a
physical test proves immediate sub-actuation onset, coherent multi-key updates,
release-to-zero, stable sustained update rate, reconnect and stock rollback.

Candidate build (not yet physically flashed at the time of this entry):

- official base: `Keychron/qmk_firmware` branch `2025q3`, commit
  `ee7390c3bbdc1f71a1cc8d54323f3f1d97868593`;
- target: `keychron/k4_he/ansi:keychron`, STM32F401, `3434:0E40`,
  `stm32-dfu`;
- change: packetized read-only `AMC_GET_REALTIME_TRAVEL_ALL` (`A9 31`) plus
  capability marker `0x45`; calibration, trigger, wireless, RGB and keymap logic
  are unchanged;
- candidate SHA-256:
  `FCEBEBD31E5D54A72D3C7C23619878F983606FE390584F2D9317693F40E31AA4`;
- official stock v1.1.1 rollback SHA-256:
  `4E877497A0EDC1A0D97CD52F5FF9BA86EF7DC84D56969E81DB7DE19EB6151E5F`.

### V14-13 full-report flash and host-rate follow-up

- exact DFU device: `0483:DF11`, serial `3381347A3035`, one device only;
- complete pre-flash 256 KiB backup SHA-256:
  `BF53C24706D761EDDF5AF447549627020F1410B5DAF431438FBCAB1FF63AB0A0`;
- candidate flash: erase/download/manifest/leave PASS; normal `3434:0E40`
  composite HID re-enumeration PASS;
- immediate sub-actuation: PASS, minimum positive `5/235` (about 2.13%);
- range/precision: PASS, 222 distinct positive levels and full-scale 1.0;
- simultaneous input: PASS for all five physically pressed keys;
- bounded UAP unload: PASS;
- initial complete-snapshot rate: FAIL, 65.4 Hz / 15,301 us average;
- direct hidapi control: 172.3 Hz / 5,804 us average for the same four-report
  `A9 31` transaction, proving a host receive-loop bottleneck;
- root fix: Windows Soup receive uses blocking
  `GetOverlappedResult(..., TRUE)` and no `Sleep(1)` completion polling;
- post-fix private-UAP rate: PASS, 181.6 Hz, 5,508 us average, 7,099 us max,
  3,458 updates in 20 seconds;
- ABI runtime/lifecycle and official Release build: PASS;
- post-fix pressed run: PASS, 191.1 Hz, 5,233 us average, 7,011 us max,
  20 observed physical keys, 231 positive levels, `5/235` minimum, 1.0 maximum
  and 2,916 value transitions;
- post-release idle: PASS, zero active keys throughout the final second while
  sustaining 185-187 Hz; bounded unload PASS;
- the original instrumented run did not record a USB disconnect/reconnect cycle;
  stock rollback material is hash-verified, and a destructive rollback/reflash
  cycle is not required for ordinary release qualification;
- subsequent owner evidence: the same physical K4 HE is now a long-term daily
  HallJoy keyboard with immediate analogue response and no observed stability,
  release, reconnect, or gameplay regressions, PASS owner acceptance;
- public boundary: custom full-report firmware PASS; stock per-key firmware
  remains FAIL for gaming latency;
- external reference check: `https://analogsense.org/firmware/` returned HTTP
  200 and documents patched full-report firmware plus QMK flashing guidance;
  its current pre-built list does not include K4 HE, so HallJoy documentation
  does not present that page as a K4 download.

Current status: **physically supported with the custom full-report firmware;
stock firmware remains unsupported for gaming**.

### V14-14 AULA W669 / WIN60 HE Standard adaptive diagnostic

- supplied firmware: W669/SI2825KZHEARGB V3.17.08, `2E3C:C365`,
  `FF1B:0091`, 64-byte report, 6x22 logical matrix;
- firmware correction: both live producers are subtype `01` with declared
  length `03` or `05`; subtype `05` is configuration only;
- safe admission: HID shape + read-only `21/04` range + all ten `18/80` map
  fragments; exact interface-path claim only;
- adaptive transport: shared/exclusive open and WriteFile/control-write
  fallback, with every attempt and raw response recorded;
- primary analogue route: RAM-only `21/02` subscription; teardown `21/03`;
- independent fallback: bounded ordered `21/0E` snapshot, diagnostic-only;
- polling information: read-only `21/0A` configured-rate query plus measured
  live-event intervals and event frequency;
- parser unit/fuzz test under Clang ASan/UBSan: PASS;
- `python tools/run_native_backend_checks.py`: PASS, including the new W669
  protocol test and all existing backend regressions;
- MSVC optimized diagnostic build: PASS; only external ViGEm missing-PDB
  linker warning remains;
- no-device smoke: PASS, exit code 0, clean shutdown, one main `HallJoy.log`;
- artifact: `build/aula-w669-diagnostic/HallJoy.exe`, 2,249,216 bytes,
  SHA-256 `DF91FB6D9487235A43ACF84B3FA56D47A41D562112B1E9E2CC820D2D65CDE6BA`;
- physical Standard/W669 device test: PENDING; no release support claim yet.

### V14-14A W669 first physical response and map correction

- returned `HallJoy.log` SHA-256:
  `455BD1BE26976F53CEC0AE287D2FDE8CE9C28B9078E5C52CFB696349A1793524`;
- identity/shape: PASS, `2E3C:C365`, product `WIN 60 HE`, `FF1B:0091`,
  64-byte IN/OUT;
- read-only travel proof: PASS, descriptor `54 01 01 01 08`, maximum 340;
- ten-fragment `18/80` transport: PASS on shared and exclusive WriteFile;
- first-build map interpretation: FAIL, all-zero default records were erased,
  leaving only explicit Fn `01 FA` at official position 122 and causing the
  pre-subscription `map_failed` gate;
- root correction: official 61-key SI2825 factory baseline plus explicit
  non-zero override records; unknown products do not inherit this geometry;
- physical-log regression fixture: PASS;
- corrected optimized diagnostic build and complete native-backend checks:
  PASS; artifact 2,250,240 bytes, SHA-256
  `D3212F8F3C419D9FE5BA103B3D313B2F7EC325B64DB13A2D92439CC7CF07B54B`;
- process lifecycle: PASS, normal shutdown marker and exit code zero;
- corrected live analogue/rate validation: PENDING physical rerun.

### V14-14B W669 end-to-end firmware subscription audit

- Ghidra export: 717 discovered firmware functions reviewed;
- `21/02` handler `0x080118C2`: PASS, copies exactly 22 mask bytes to RAM
  `0x2000E878`, no secondary enable command;
- normal producer `0x080180E2`: PASS, reads the same mask through literal
  `0x08018558`, declared length 3, subtype 1;
- alternate producer `0x08018BB4`: PASS, reads the same mask through literal
  `0x08018E94`, declared length 5, subtype 1;
- both producers: PASS, row/column plus little-endian processed travel match
  `DecodeLiveEvent`; mask gating is independent of digital actuation;
- HallJoy subscription mask orientation: PASS,
  `mask[column] |= 1 << row`;
- missing firmware start/enable command: NONE in the complete traced path;
- remaining gate: physical Windows live delivery, measured timing,
  simultaneous keys and release-to-zero using the corrected artifact.

### V14-14C W669 first live stream and snapshot-contamination correction

- returned `HallJoy (2).log` SHA-256:
  `1B1934BDC281F7CA83A7273EFC89B0A706623E7574F111FF73F4B0FBF9273955`;
- corrected identity/range/map proof: PASS twice, 61 mapped keys, maximum 340;
- live subtype-`01` delivery: PASS, 3,950 valid raw events across 21 physical
  positions, both declared lengths (`03`: 246, `05`: 3,704), no invalid
  row/column and maximum processed value 340;
- release-to-zero on the wire: PASS, 204 explicit zero events; every one of
  the 21 observed positions ended at zero when reconstructed from live events
  only;
- old snapshot publisher: FAIL, `21/0E` returned sensor-domain idle values
  around `0x0Axx`, outside `0..340`, and created 14 false active keys;
- old synchronous fallback: FAIL, each 132-report request retained only 64
  reports and its private read loop intercepted 532 valid live reports;
- root correction: all initial/quiet snapshot collection and publication
  removed; one receive loop exclusively owns the event-driven live stream;
- idle read accounting correction: cancellation after the requested timeout is
  preserved as `WAIT_TIMEOUT`, not counted as `ERROR_OPERATION_ABORTED`;
- polling query: code `0` physically observed and now truthfully reported as
  firmware-default/nominal unspecified;
- `python tools/run_native_backend_checks.py --require-compiler`: PASS;
- optimized MSVC diagnostic rebuild: PASS, only allow-listed external
  ViGEmClient missing-PDB warning;
- corrected artifact: `build/aula-w669-diagnostic/HallJoy.exe`, 2,313,728
  bytes, SHA-256
  `D640F7D4C497DED890FE7C45CE7188CA883137A9737FF9A080694A04423C3A1F`;
- remaining gate: one physical rerun must show live analogue, zero false active
  keys after release, stable idle `failures`, and clean shutdown.

### V14-14D W669 stall report and single-owner diagnostic correction

- returned `HallJoy (4).log`: 67,108,864 allocated bytes, 35,120 meaningful
  bytes, 175 lines, SHA-256
  `C24DD91D26EF4056202DCA659E168E43DADFF8AA9BF0FB8A2A5C320BBA39E28A`;
- lifecycle: INCOMPLETE, trace ends at 32.984 seconds without shutdown markers;
- W669 timing evidence: INVALID/ABSENT because two `CREATE_ALWAYS` owners
  competed for `HallJoy.log` and hid DebugLog raw/telemetry lines;
- interference evidence: 36 absent-MAX enumerations over 33 seconds, 21-22 HID
  interfaces per pass and approximately 8-20 ms per pass;
- correction: absent MAX discovery waits on `WM_DEVICECHANGE`; timed retry is
  retained only for a present but transiently unavailable candidate;
- single-log correction: mapped stability sink is the sole file owner;
  asynchronous DebugLog and MAD68 messages append to it;
- build reproducibility: diagnostic + single-log properties are mandatory and
  W669/MAD marker presence is package-checked;
- shutdown correction: no blocking mapped-file/disk flush; normal exit creates
  no exit-report sidecar, while abnormal crash evidence remains enabled;
- native/static gates: PASS; optimized MSVC build: PASS (only allow-listed
  external ViGEm missing-PDB warning);
- local eight-second smoke: PASS, exit 0, 75 ms shutdown, all worker exits,
  one 38,703-byte log, zero diagnostic/MAD sidecars, four startup/device-change
  MAX enumerations and no periodic one-second train;
- corrected artifact: 2,318,848 bytes, SHA-256
  `6931EA0AA3B32205F0AEA395C90C7081C3A4F9A1606CE1C1A74583D00FDB377E`;
- physical W669 non-stall and timing verification: PENDING one tester rerun.

### V14-12T.5 Configuration selected-key live graph

- root trace: PASS; UI timer, selected HID and backend milli-values update
  without focus, while the retained Configuration page cached the old marker;
- architecture: PASS; cacheable plot/curve and viewport telemetry overlay are
  separate phases around `CustomPageSurface_Present`;
- z-order compatibility: PASS; live marker remains below editable handles;
- invalidation scope: PASS; changed analog values invalidate only the graph
  rectangle and do not dirty the retained full-page cache;
- `pre_release_ui_static_audit.py`: PASS, including four new layer/invalidation
  regression guards;
- `python tools/run_native_backend_checks.py`: PASS, all static and portable
  C++ tests;
- MSVC `Release|x64`: PASS; only allow-listed external ViGEm missing-PDB
  warning;
- unified UI stress: PASS 6/6 pages, 120 wheel events/page, Configuration
  63.0 update cycles/s, GDI 109/109/106, USER 194/195/194, exit code 0;
- official package evidence: `build/evidence/ui-scroll-stress/20260809-172117`,
  EXE SHA-256
  `EF018D57768C5E0249E9B33D53C8A5D4396EEEAA4887B6790F7F9488337E2BBC`;
- physical selected-key visual continuity: PENDING owner confirmation.

### V14-14E W669 official multi-geometry admission

- current official device catalog: PASS, seven `2E3C:C365` products audited;
- official identity flow: PASS, read-only opcode `0D`, fifth CSV field selects
  `config/keys/<firmware-product>.json`;
- SI2825 aliases: PASS, four products resolve to the exact 61-key profile;
- SI2828 aliases: PASS, two products resolve to the exact 68-key profile;
- SI2851/KP-TE153 UK: PASS, exact 69-key profile;
- official layout SHA-256 values: PASS,
  `04E2FDA00FDB1645C74D42121102C1CF233658DDF43FB2611ACE57460CFCB448`,
  `CC2CBBC9C051230279BA8CE3B52054739446DC19C78CF1A38ADFD2D7C6CDF9E1`,
  `FCB98F5DF82C2E00D94501389C452172C4FAB6F103DF3CBF489FFFE9B89945C3`;
- override semantics: PASS, complete `18/80` generation overlays the selected
  factory profile and all-zero inheritance preserves factory keys;
- unknown-product safety: PASS, no known baseline is guessed from VID/PID,
  substring or key count; explicit-only admission remains available;
- reconnect identity: PASS, a proved firmware product must be returned again
  before the live subscription is installed;
- protocol test: PASS under optimized portable C++20 gate, including malformed
  identity rejection and 0..64-byte fuzz smoke;
- `aula_w669_backend_static_audit.py`: PASS with identity/profile/safety guards;
- `python tools/run_native_backend_checks.py --require-compiler`: PASS;
- official optimized MSVC x64 build: PASS, 0 errors, only allow-listed external
  `LNK4099` ViGEm missing-PDB warning;
- production artifact SHA-256:
  `0BD04CBF5FDA26B9B1A5C7BBBCFB40E357A5CF51A3EB8534816D354791F39A14`;
- physical WIN68/KP-TE153 validation: PENDING; do not label those models as
  physically verified HallJoy hardware.

### Public README and Keychron K4 HE layout preset

- README product heading is version-independent while the current v1.4 release
  record remains linked: PASS;
- user feature block contains low-latency and last-key-priority wording and no
  internal pipeline diagram or SOCD abbreviation: PASS;
- Quick start matches production dependency behavior: pinned manual ViGEmBus
  1.22.0 guidance, automatic private Universal Analog Plugin preparation, no
  system Wooting Analog SDK: PASS;
- hardware table uses Aula, Irok/SparkLink and compatible-protocol wording;
  Irok MG75 v2 is explicitly physically tested/unsupported: PASS;
- Input Overlay follows hardware support and the advanced-reader boundary
  precedes Building from source: PASS;
- risky custom-firmware route includes no-result and permanent-brick warnings:
  PASS;
- Keychron K4 HE source preset versus current physical local layout: exact
  100/100 key tuple match, PASS;
- new built-ins appear for non-empty user layout stores; same-name user files
  override built-ins; preset zero remains the default: PASS static guard;
- `persistence_transaction_static_audit.py`: PASS;
- `version_identity_static_audit.py`: PASS;
- `python tools/run_native_backend_checks.py --require-compiler`: PASS, all
  static and portable C++20 tests;
- isolated MSVC `Release|x64` link: PASS, zero errors, only allow-listed
  external ViGEm `LNK4099`;
- validation artifact SHA-256:
  `5481699CC583156904AEFB7DE3950CFC01B245D3CD2029B82F512CF507A28499`.

### Corrected built-in numpad tall-key height

- Reset with revision-1 88 px values: visual FAIL, both tall keys end one
  rendered pixel too low;
- user-validated contract: 87 px for `Num+` and `NEnt`, accepted;
- Generic 100% ANSI and Keychron K4 HE source definitions: all four affected
  entries are exactly 87 px, PASS static guard;
- persisted migration revision: 2, superseding bad revision 1, PASS;
- migration direction: only old 88 px values on HID usages 87/88 become 87 px;
  an existing 87 px value cannot be expanded, PASS static guard;
- migration scope: only the two named built-ins are eligible and unrelated
  positions, widths, bindings, labels, spacing and heights remain unchanged;
- both current local presets: 87/87 and no stale revision-1 value, PASS;
- production startup plus graceful `WM_CLOSE`: exit 0; both files persisted
  `BuiltinGeometryRevision=2` while retaining exact 87/87 values, PASS;
- `persistence_transaction_static_audit.py`: PASS;
- complete native backend gate: PASS;
- complete production packaging gate: PASS, zero compilation errors, only the
  allow-listed external ViGEm `LNK4099`;
- continuous telemetry absent; crash-only reporting retained, PASS;
- production `build/release/HallJoy.exe` SHA-256:
  `CF3FE3A87DC913B986A8837E59084154126438B4EC9DEF4C3815F64C95BCDEB9`.

### Public README useful-content restoration

- public `main` README blob inspected through read-only GitHub API:
  `b4f711f9f4dd055ba50e790eef5aad2838f89635`, PASS;
- video overview target: HTTP 200, PASS;
- up to four pads: `BINDINGS_MAX_GAMEPADS = 4`, PASS code evidence;
- Snap Stick and Block Bound Keys: runtime settings, backend behavior and UI
  controls present, PASS code evidence;
- visual layout editor: add/delete/move, label/HID/dimension/position/spacing and
  transactional preset save paths present, PASS code evidence;
- storage wording: `%LOCALAPPDATA%\HallJoy` default and writable
  `HallJoy.portable` marker behavior match `app_paths.cpp`, PASS;
- user requirements, current quick-start flow, zero-analogue troubleshooting,
  AGPL/commercial licensing and third-party notices restored, PASS;
- obsolete manual system Wooting SDK/UAP, dual-plugin cleanup and rollback
  guidance remains absent, PASS;
- all relative README links resolve and `git diff --check` passes.

### Project-wide English documentation migration

- README Russian-document link paragraph: absent, PASS;
- author's personal README paragraph: restored verbatim, PASS;
- project-maintained documentation/source-comment Cyrillic scan, excluding
  backups, generated output and vendored Soup localization dictionaries: zero
  matches, PASS;
- `_RU.md` / `_RU.txt` filename scan: zero matches, PASS;
- stale `_RU.md` / `_RU.txt` reference scan: zero matches, PASS;
- local `backups/` tree excluded by `.gitignore`, PASS;
- project-relative Markdown links: all targets resolve, PASS;
- Markdown table column-count audit: zero inconsistent groups, PASS;
- `s20_build_docs_static_audit.py`: PASS;
- complete `run_native_backend_checks.py --require-compiler` gate: PASS;
- `git diff --check`: PASS.

### Editorial review after the English migration

- every document with translated narrative prose compared against the preserved
  pre-migration source and manually rewritten for meaning, terminology, and
  evidence boundaries: PASS;
- existing English validation logs, checksums, manifests, and compiler output
  retained as primary machine-generated evidence: PASS;
- mixed-language source check: `docs/v1.4/VALIDATION_MATRIX.md` and
  `docs/v1.4/WORKLOG.md` were already English; only their new English entries
  required review, PASS;
- maintained documentation and source-comment Cyrillic scan: zero matches,
  PASS;
- maintained UTF-8 documentation mojibake scan: zero matches, PASS;
- `_RU.md` / `_RU.txt` filenames and stale references: zero matches, PASS;
- malformed Markdown headings: zero matches, PASS;
- unclosed fenced blocks: zero matches, PASS;
- inconsistent Markdown table groups: zero matches, PASS;
- broken HallJoy-relative Markdown links: zero matches, PASS;
- official build guide explicitly documents Windows x64, both supported x64
  configurations, the unsupported Win32/x86 target, and release/staging output
  directories: PASS;
- `s20_build_docs_static_audit.py`: PASS;
- `version_identity_static_audit.py`: PASS;
- `protocol_family_routing_static_audit.py`: PASS;
- complete `run_native_backend_checks.py --require-compiler`: PASS.
