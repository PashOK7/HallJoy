# HallJoy v1.4 testing

Run commands below from the repository root. This file describes only tests and
scripts that are shipped in the current tree.

## Required automated gate

```powershell
python tools/run_native_backend_checks.py --require-compiler
BUILD.cmd
```

The unified Python runner executes every `src/HallJoyProject/tests/*audit.py`,
the current catalog-driven Addressed validator, and all portable C++20 tests.
`--require-compiler` prevents a missing `g++`/`clang++` from silently reducing
coverage.

`BUILD.cmd` repeats the automated gate, validates locked dependencies, builds
the private UAP runtime and the MSVC Release x64 application, rejects unexpected
compiler/linker warnings, and writes:

```text
build\output\HallJoy.exe
```

For a fast source-only pass without C++ compilation:

```powershell
python tools/run_native_backend_checks.py --static-only
```

This is useful during editing, but it does not replace the required gate.

## Windows lifecycle checks

Normal production smoke:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_production_smoke.ps1
```

Repeatable normal start/graceful-stop cycles:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_release_qualification.ps1 -Cycles 25 -RunSeconds 2
```

The release-cycle runner verifies exit code, shutdown deadline, remaining
HallJoy processes, handle count, preservation of the user-state snapshot and
the final zero-continuous-log/crash-only policy. It checkpoints every completed
cycle. It exercises ordinary production operation, creates no stability trace
and does not inject a realtime failure. The final S21 cycle gate is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_release_qualification.ps1 -Cycles 1000 -RunSeconds 1 -ProgressEvery 25
```

One-hour production soak with overlay responsiveness probes:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_long_soak.ps1 -DurationMinutes 60 -SampleSeconds 5 -WarmupSeconds 10 -StartOverlay
```

The soak persists `samples.csv`, checkpoints, before/after user-state hashes,
the bounded stability trace, analyzer output and `summary.json` under
`build/evidence`, outside the build-cleaned `build/output` directory. Leak gates use
a post-startup warm-up baseline. The runner prevents automatic system sleep for
the duration and clears that request on every exit path; manually rebooting or
suspending the machine still invalidates the run. Analyzer `WARN` is acceptable only for the
documented manual-only input/reconnect/mode coverage; analyzer `FAIL`, trace
`ERROR`, trace capping, resource-growth limits or an unresponsive overlay fail
the run.

The analogue simulator is the deterministic lifecycle/fault harness:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_analog_simulator.ps1 -RunSeconds 8
```

Its `-Inject...` switches deliberately create one selected failure and validate
fail-closed containment. An injected timeout is not a normal operating mode and
must never be interpreted as an acceptable production realtime failure.

Storage migration has a separate disposable-root test:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_storage_migration_test.ps1
```

## Hardware qualification

Automated tests can prove parsers, routing, ownership, shutdown contracts and
failure behavior. They cannot prove USB timing or firmware behavior on hardware
that is not connected.

Before release, record the available-device matrix and perform input, reconnect,
held-key unplug/reconnect, graceful shutdown and long-run checks required by
`docs/v1.4/VALIDATION_MATRIX.md`. Aula WIN 60 HE MAX has completed three physical
exclusive protocol proofs and two long claim/runtime runs; the tester confirmed
analogue input. The remaining Aula gate uses the single-file diagnostic schema
v2: hold 10 keys for 10 seconds, release all, then unplug for 10 seconds and
reconnect without closing HallJoy. Its 5-second health windows record real
matrix Hz, latency, active-key distribution, release-to-zero and per-HID maxima.
Return-after-disconnect evidence continues to block release approval.

## Overlay checks

Start the production overlay smoke with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\run_production_smoke.ps1 -StartOverlay
```

The dedicated source checks are part of the unified runner. The browser overlay
still needs a manual visual/input check for layout, live values and reconnect.

## Evidence and limitations

Current evidence lives in `docs/stability/tests/`; current status and remaining
gates live in `docs/v1.4/VALIDATION_MATRIX.md` and
`docs/v1.4/RISK_REGISTER.md`. Files marked historical under `docs/validation/`
describe older packages and are not current release evidence.

Passing static/unit/simulator tests is necessary, not sufficient, for release.
Physical-device compatibility, the one-hour soak, the full 1000-cycle target
and the final hardware matrix remain qualification work until their final runs
are recorded against the release-candidate hash.
