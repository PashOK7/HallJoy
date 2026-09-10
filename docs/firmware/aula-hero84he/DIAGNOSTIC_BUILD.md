# AULA HERO84 HE diagnostic build and physical run

This is an evidence collector for the exact wired HERO84 HE family identity
`372E:103E`, vendor interface `FF60:0061`, report ID `09`, and UUID
`18691697672197`.  It is not production HallJoy support and it never creates
gamepad input from the sampled data.

Build it from the repository root:

```powershell
.\tools\build_aula_hero84he_diagnostic.ps1
```

The expected output is
`src\HallJoyProject\x64\AulaHero84HeDiagnostic\HallJoy-AULA-HERO84HE-Diagnostic.exe`.
The current audited build (2026-09-01) is SHA-256
`403026295C2B9A866D61B2306F2D2EAD2DB52724663F339AD0364BAF547696A4`.

## Test procedure

1. Connect only the keyboard under test directly over its normal wired USB
   connection.  Do not open AULA HUB or another keyboard configuration program.
2. Place the EXE in a writable folder and launch it normally.  It performs one
   official identity read before the private UAP can open HID, then waits for
   keyboard Raw Input registration.  It sends no active analogue command if
   that registration does not succeed within ten seconds.
3. Once the HallJoy window is visible, for the next 10–15 seconds deliberately
   press and release `W`, `A`, `S`, and `D` several times.  Include one slow
   press, one full press, a short `W+A` chord, and normal typing in another
   text field before/during/after.  Do not hold every key or change keyboard
   settings during the run.
4. Close the program after the run finishes.  Return the entire
   `HallJoyStabilityTrace.log` beside the EXE (and
   `HallJoyStabilityTrace.previous.log` if present), plus the EXE SHA-256.

The active trace must contain, in order:

- `[aula.hero84.prepare] identity_proven`;
- `[aula.hero84.raw_input] registered=1`;
- `[aula.hero84.admission] accepted`;
- `[aula.hero84.mapping]` records for physical positions `001E`, `002B`,
  `002C`, `002D` (`W/A/S/D`);
- five `[aula.hero84.stage]` begin/complete pairs through `1000Hz`; and
- one `[aula.hero84.summary]`.

Any `reject`, `semantic_reject`, timeout, missing stage completion or keyboard
input interruption is useful negative evidence.  Do not rerun with different
commands: send the log unchanged.  The program’s only possible transmit frames
are `82/01`, `83`, and the firmware-derived read-only candidate `94/02`.
