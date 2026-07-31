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
4. verifies common-pipeline reports, accepted ViGEm changes, graceful shutdown,
   and absence of remaining processes;
5. rejects missing trace evidence or any `ERROR` event.

`-SkipBuild` reuses the existing simulator executable. `-RunSeconds` must be at
least 7 so every deterministic phase runs.

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
