# Madlions Titan68 Turbo diagnostic build

This is an isolated, evidence-collection executable, not normal HallJoy support.
It accepts only the exact official V2 USB vendor interface: VID:PID `28E9:31FD`,
top-level usage `FF87:0020`, 64-byte input/output reports, input report IDs `06`
and `07`, and output report ID `06`.

## What it does

1. Passively listens for 750 ms.
2. Sends the firmware-required RAM-only sequence `06 37 00 00 01 39 00 00 01`, then
   `06 36 00 00 01 38 00 00 01`.  The first command establishes the ID-07-producing
   mode; the second selects the travel-test behavior.
3. Records 10 seconds of all HID input frames and correlates target-scoped physical keys.
4. Always issues `06 36 00 00 01 37 00 00 00` after a successful `0x37` entry. It
   deliberately never sends `0x37,00`: that firmware exit calls the persistent-write
   primitive.
5. Passively listens for a final 750 ms and writes a summary.

The diagnostic does not publish game input. Its only calibration-family command is the
non-writing entry `0x37,01`; it sends no `0x37,00`, firmware-update command, reset,
persistent-setting write, or feature report.
It performs one probe attempt per launch; a missing acknowledgement is recorded but
never causes an automatic retry.

## Build and verify

From the repository root:

```powershell
.\tools\build_titan68_turbo_diagnostic.ps1
```

The script first runs `src/HallJoyProject/tests/titan68_turbo_diagnostic_static_audit.py`,
then rebuilds the isolated x64 target with one compiler process to avoid PDB contention.
The output is:

`src\HallJoyProject\x64\Titan68TurboDiagnostic\HallJoy-Madlions-Titan68-Turbo-Diagnostic.exe`

The 2026-09-05 verified x64 build is SHA-256
`8DC2269B540FCDD26A879B3241221272BA13FFAF03190863F10AEFD72C9602E4`.
It is not code-signed; verify this hash when transferring it to a tester.

## Tester procedure

1. Close any Madlions/FGG web driver and launch the diagnostic normally.
2. Before the ten-second active window, type a few ordinary characters; during
   it, slowly press and release `W`, `A`, `S`, `D`, and type several letters in
   another application. This is the normal-HID coexistence measurement.
3. Let the program close normally. If it is closed during the initial passive
   window, it sends no control frame at all. Do not unplug the keyboard or kill
   the process during the active window, so `0x36,00` can be sent.
4. After the diagnostic has closed, physically reconnect the keyboard once.
   This clears the RAM-only `0x37,01` state without ever issuing its flash-writing
   `0x37,00` exit.
5. Send back `HallJoyStabilityTrace.log` located next to the executable. It
   contains the exact transmitted frames, every received HID frame, raw-key
   timestamps and the decoded `raw12` values; it intentionally contains no
   keyboard path or serial number.

Use `tools\analyze_titan68_turbo_trace.py` after receiving the file; its
read-only verdict and limitations are documented in `TRACE_ANALYSIS.md`.

## Evidence boundary

Static firmware analysis proves that the firmware emits three-byte USB-IN records on
endpoint `0x82` with report ID `07`, key index, and paired six-bit fragments that form a
12-bit raw Hall value. Static analysis proves that `0x37,01` copies the prior calibration
image into RAM and that `0x36` toggles a separate volatile mode bit; `0x37,00` is excluded
because it reaches the persistent-write primitive. Runtime evidence from this executable is
still required to prove simultaneous ID-07 and ordinary typing for a retail keyboard.
