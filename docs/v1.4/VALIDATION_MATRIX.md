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
