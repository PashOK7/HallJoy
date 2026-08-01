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
