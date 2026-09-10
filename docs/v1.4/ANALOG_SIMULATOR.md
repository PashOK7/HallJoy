# HallJoy v1.4 analog simulator

## Purpose

The simulator is a development-only verification target for workstations
without an analog keyboard. It drives the same native aggregation, curve, SOCD,
report builder, ViGEm scheduler, telemetry, and shutdown path as a real native
backend.

It is not a protocol emulator. It does not open HID devices, advertise a
VID/PID, claim native routing, or prove MAD68, Hex80, Addressed, SparkLink,
Sayo, or UAP hardware compatibility.

## Run

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\run_analog_simulator.ps1
```

The runner:

1. builds `HallJoyV14Simulator.exe` with `HallJoyAnalogSimulator=true`;
2. launches it with the exact opt-in argument
   `--halljoy-simulate-analog=script`;
3. exercises ramp, hold, release, opposing axes, diagonal, disconnect,
   reconnect, source fault, and recovery;
4. verifies common-pipeline reports, child-acknowledged publication through the
   isolated production ViGEm route, qualified neutral/remove shutdown, and
   absence of remaining processes;
5. rejects missing trace evidence or any `ERROR` event.

`-SkipBuild` reuses the existing simulator executable. `-RunSeconds` must be at
least 7 so every deterministic phase runs.

The legacy ViGEm regression switch names are retained so old commands remain
reproducible, but their target assertions follow the F3 architecture. The
runner can inject a child exit or a child that stops making progress, then
requires publication disable/quiescence, predecessor reap, exactly one Ready
replacement, clean final neutral/remove, exit 0 and zero survivor:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\run_analog_simulator.ps1 `
  -InjectVigemUpdateStall -RunSeconds 8
```

The same routed recovery contract is exposed by
`-InjectVigemOutputCppFault`, `-InjectVigemOutputInvalidThreadHandle` and
`-InjectVigemOutputWakeCloseUse`. These are process/lifecycle fault injections,
not physical driver or keyboard evidence.

The SparkLink service-stop regression is available as a simulator-only
lifecycle injection:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\run_analog_simulator.ps1 `
  -InjectSparkShutdownRace -RunSeconds 7
```

It starts a cooperative synthetic Spark worker, closes the outer service gate,
joins the worker, then explicitly probes the hotplug tick. PASS requires no
reconnect after service stop and a normal process exit. This proves lifecycle
ordering only; it does not prove SparkLink hardware behavior.

## All-keyboard shutdown matrix

To verify bounded containment for every production route with one simulator
build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\tools\run_keyboard_shutdown_matrix.ps1 -RunSeconds 7
```

The runner executes a normal control, permanent-stop injections for all six
native catalog routes, a permanent private UAP/Soup child-unload stall and the
independent application watchdog. It requires exact trace/exit evidence,
copies and hashes each trace, rejects surviving HallJoy processes and writes
`summary.json` below `build/evidence/keyboard-shutdown-matrix`.

Every injected hang is simulator-only. The summary deliberately records
`hardware_verified=false`; this matrix proves containment behavior, not that an
unavailable keyboard's protocol or physical input works.

## Isolation contract

- Simulator sources are excluded from ordinary MSBuild targets.
- The catalog entry and implementation require `HALLJOY_ANALOG_SIMULATOR`.
- Runtime activation requires the exact script argument.
- Test telemetry and trace events contain `simulated=1 hardware=0` or
  `SIMULATED / NOT HARDWARE`.
- The target applies temporary in-memory WASD-to-left-stick bindings. It does
  not persist them.
- Simulator PASS is common-pipeline evidence only and cannot close a hardware
  gate.

## Isolated synthetic input (2026-09-05)

Use `-IsolateSyntheticInput` when testing the scripted pipeline on a workstation
with real analog hardware attached. This opt-in owns the four synthetic WASD
keys even in scripted disconnect/fault phases, so a physical UAP/native key
cannot override the required zero. Curves, report construction and the isolated
ViGEm transport are unchanged. The ordinary simulator still tests aggregation
with real sources; an isolated run is explicitly not multi-device aggregation
or hardware evidence. Storage-migration tests select the isolated mode.
