# Native keyboard update rate in Gamepad Tester — 2026-10-22

Owner report: a user's Gamepad Tester screenshot (EPOMAKER HE108, `rongyuan-stream`, 3151:5030) showed
`0.0 Hz` next to `update age 94 ms`. The age was correct (keys held still). The rate was wrong.

## Cause

The "Native protocol ..." line printed each backend's own `updateHz10`. Backends fill it differently (average
interval, matrix rate, own counters), and `rongyuan-stream` derives it from `g_avgUs`, which nothing in that
backend writes, so it was always 0. Every backend does count `successfulUpdates`.

Audit of all native backends (2026-10-22): the old line was always `0.0 Hz` for every backend that never sets
`updateHz10`: attackshark-pro, aula-mini60-he-pro (incl. Alumix 104), aula-hero84he, keychron-k4-onboard,
logitech_rapid, mad68-dual-trial, mchose_jet75, mchose_mix87, neo65, redsquare-alumix68, steelseries_apex,
razer-tartarus and the diagnostic backends (Titan68, ACE68), plus rongyuan-stream (set from an unwritten value).
Backends with their own working rate: aula-w669, aula 6x21 (win60he), DrunkDeer, IROK NA87/ND75, MADE68 Air,
MG75 Pro/JingTai, rongyuan-snapshot, Slice75, Azoth96. The detailed protocols (mad68-a0, hex80, addressed,
sparklink, sayo) have their own lines and are unchanged. All of these count `successfulUpdates` with a real
counter, so the measured rate now covers every protocol on the generic line.

## Change

- `update_rate_meter.h` (`halljoy::UpdateRateMeter`): rate from the successful-update counter, 0.5 s windows,
  works with irregular sampling and counter restarts. "now" = last window; "max" = 99th percentile (nearest
  rank) of the windows that carried updates, over the last 1024 such windows: the top 1% of windows is excluded.
- `backend.cpp` `MeasureNativeUpdateRates`: one meter per native catalog entry, observed on every telemetry
  collection (UI timer and support log), reset while the entry is not connected. Results in
  `BackendNativeProtocolTelemetry::measuredHz10/measuredPeakHz10`.
- Gamepad Tester line: `... | active N | updates now X Hz, max Y Hz`.
- Backends' own `updateHz10` is unchanged (still used in other lines and logs).

Meaning for users: a change-only stream (RongYuan and others) sends nothing while keys are still, so "now" is
0 Hz then; "max" shows the rate the keyboard reaches while keys move. For polled backends the rate is the
poll/update rate of HallJoy's reads.

## Validation (agent, no hardware, no visual check)

- `update_rate_meter_test.cpp` (new, in `run_native_backend_checks.py`): 1000 Hz at 16 ms sampling, still keys,
  top-1% burst excluded, 2 s gap, counter restart, short windows.
- `run_native_backend_checks.py --require-compiler` EXIT=0, `build_release.ps1` EXIT=0. EXE SHA256
  `28914c881bb49c705842bb941b0f0cd19183757df2527b4ccfc9235b82208721` (local, not released; also contains the
  red unsupported-keyboard banner and the Attack Shark probe fix). Backup `.local/backups/update-rate-meter-20261022/`.
- Not checked: the line on screen and real rates on hardware.
