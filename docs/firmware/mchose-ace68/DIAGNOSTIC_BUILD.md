# MCHOSE Ace 68 diagnostic build

This is an evidence-collection image, not normal HallJoy support. It owns no
keys and publishes no analogue values to a controller. It opens only the exact
Ace 68 I M HUB interface: `VID 41E4`, `PID 2114`, `0001:0000`, 64-byte IN/OUT.

It logs the matching-device interface inventory, every complete 64-byte vendor
report, all transmitted frames, `AA`/`AB` replies, and for each `A0` packet the
three-byte descriptor plus BE16 bytes `4..5` and its observed min/max. It also
records Raw Input press/release events only when the Windows raw-device path is
`VID_41E4&PID_2114`; these timestamps establish descriptor-to-key correlation.

It first captures 2.5 seconds on a read-only vendor handle, then another 2.5
seconds after opening a read/write handle. This distinguishes a missing stream
from a stream affected merely by opening the interface. It never enters
calibration and never sends `A8`, `A9`, or `B1`. Its active stage uses only the
official M HUB getter requests: information (`03`), base settings (`04`),
function configuration (`05`), key matrix (`08`), and profile-0 trigger data
(`A0`). These are canonical zero-payload `55` requests, first via `WriteFile`
and then, only if necessary, `HidD_SetOutputReport`. It issues no feature
reports, opcode scans, configuration writes, update/flash command, or raw/XOR
variant. `AB` or a mismatched reply stops the remaining active probes
immediately but does not stop capture: the full five-minute raw observation
still lets the tester press individual keys and chords.

Build from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_mchose_ace68_diagnostic.ps1
```

Run `src\HallJoyProject\x64\MchoseAce68Diagnostic\HallJoy-MCHOSE-Ace68-Diagnostic.exe`.
Do not hold keys until the log records `official_read_complete`; then make deliberate
press/release and chord sequences for 10--15 seconds. Return the entire
`HallJoyStabilityTrace.log` beside the executable (and
`HallJoyStabilityTrace.previous.log` too, if it exists), plus the executable
SHA-256. `HALLJOY_SINGLE_LOG_DIAGNOSTIC` deliberately routes diagnostic output
into this stability-trace file; `HallJoyDiagnostic.log` is not produced.
