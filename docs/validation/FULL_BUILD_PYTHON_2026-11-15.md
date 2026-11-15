# Full BUILD.cmd Python discovery follow-up

Date: 2026-11-15. Branch: `fix/windows-build-portability`.

## Reproduced failure

The user ran BUILD.cmd and reported Windows' "Python was not found" message,
followed by `Native backend static checks failed: 9009` at build.ps1's first
Python invocation. `Get-Command python.exe` selected the WindowsApps alias.
An installed x64 Python 3.12 interpreter was available through uv.

The previously verified incremental build used build_environment.ps1, but the
full build.ps1 did not call it. Fixes to incremental discovery alone therefore
did not cover the user's full entry point.

## Changes

- Initialize the existing shared Python/Clang discovery at the beginning of
  build.ps1, before any Python invocation. Full dependency builds require x64
  Clang; ordinary incremental discovery remains compiler-optional.
- Accept explicit `-PythonPath` and a read-only `-CheckOnly` mode for that
  environment discovery. The latter does not claim to validate every dependency.
- Forward BUILD.cmd arguments and return without pausing for argument-driven
  calls. Double-click builds retain the original completion/error pause.
- Add test_full_build_environment.ps1 to CI. It invokes the actual CMD → Windows
  PowerShell entry in a private fixture with an invalid WindowsApps alias first
  on PATH, checks explicit Python validation and verifies that CheckOnly creates
  neither build output nor a dependency cache.
- Remove two existing compiler warnings rejected by the full builder's warning
  gate: rename a shadowing telemetry local and explicitly narrow a PID whose
  production producer is the 16-bit HIDD_ATTRIBUTES::ProductID. No warning gate
  was weakened and no protocol admission or hardware-support status changed.

## Verification

- The new entry regression failed before the entry-point fix and passed afterward:
  seven assertions, Windows PowerShell 5.1, including the real BUILD.cmd child.
- `tools/build.ps1 -CheckOnly` selects the installed Python and x64 Clang.
- Optional-installer/S20 documentation audits and modified PowerShell syntax PASS.
- A real full-build run passed the formerly failing Python/native stage and built
  the pinned UAP. It then correctly stopped because the user's portable HallJoy
  was still running. Its exact path and child roles were verified, its running
  state recorded, and that copy was closed for the required isolated UAP check.
- The concluding full-build run completed with exit 0: all native checks,
  PRIVATE_UAP_ABI_RUNTIME, profile/layout/recovery checks, complete MSVC Rebuild,
  embedded ViGEm/ABI/license checks and atomic local EXE publication passed.
  Only the explicitly allowed vendor LNK4099 remained; unexpected warnings: zero.
  Evidence: `build/evidence/full-build-python-2026-11-15-final.log`.
- The exact delivered EXE also passed `--halljoy-require-full-catalog` (exit 0).
  The previously running portable copy was restarted afterward at its original
  path and working directory, with its original normal window style.

Final ordinary output: `build/bin/Release/x64/HallJoy.exe`, version `1.6.15.0`,
11,426,816 bytes, SHA-256
`9fd25d835fa67122c9bd699bc20b9e9bf463679ea32aaaf87080834bfad87890`.
Readback metadata is retained in
`build/evidence/full-build-python-2026-11-15-artifact.json`.

The physical-UAP process-isolation requirement remains in the full builder.
Users rebuilding dependencies must close other HallJoy instances for that stage.
The discovery fix does not alter Windows App execution aliases, permanent PATH,
PowerShell policy, firmware or shared driver installation.

## Follow-up: CheckOnly and the physical-UAP message

The owner supplied a transcript with `FULL_BUILD_ENVIRONMENT=PASS`, followed by
plugin-build output and a refusal because HallJoy was running. That continuation
could not be reproduced on the current files. The owner subsequently confirmed
that the plugin-build output and physical-UAP refusal came from the previous
full build. The transcript therefore combines separate invocations: CheckOnly
succeeded; the earlier full build stopped because HallJoy was still running.

Fresh reproduction of `.\BUILD.cmd -CheckOnly` returned **0** and stopped at the
environment PASS. The seven existing entry-point assertions also passed. The
PowerShell-prompt invocation was then repeated with an explicit uv-managed
**Python 3.14.9**, matching the version in the owner's transcript: exit **0**;
five watched UAP/runtime/ordinary/portable artifacts retained their hashes,
sizes and modification timestamps. Evidence:
`build/evidence/checkonly-python3149-2026-11-15.json`.

The entry regression now additionally exercises PowerShell -> BUILD.cmd ->
Windows PowerShell, requires exactly one environment PASS and verifies no build
or dependency-cache output. The expanded regression passed **10 checks**, exit
**0**; output is retained in
`build/evidence/checkonly-entry-regression-2026-11-15.log`.
The physical-UAP gate is unchanged: it belongs to
the full dependency build, requires all HallJoy processes to be closed and is
not executed by CheckOnly. At this point the build guide explained the tray Exit step and
the distinction from the incremental build. No application rebuild is required
for these test/documentation changes.

## Subsequent full-build lifecycle fix

After a repeated full-build refusal with the personal HallJoy copy still running,
the UAP stage gained scoped automatic shutdown and restoration for verified
current-checkout/current-session app paths. The global and leaf isolation guards
remain active. A full build with Python 3.14.9 and the same six processes running
then completed with exit **0**, and the original app was restored. See
[full-build lifecycle verification](FULL_BUILD_UAP_LIFECYCLE_2026-11-15.md) for the
16 process-lifecycle assertions, unchanged CheckOnly contract and artifact hashes.

## Tester feedback recorded separately

The user reports that the TK75 HE V2 ISO UK / Magnetic Phoenix V3 works smoothly
for W/A/D and simultaneous W+D in Gamepad Tester and returns to neutral on release.
This partial functional result is recorded in the
[GamaKay history](../current/GAMAKAY_2026-10-23.md). It does not establish complete
ISO geometry, reconnect, typing or all-key coverage and does not promote board
3138's Experimental status.
