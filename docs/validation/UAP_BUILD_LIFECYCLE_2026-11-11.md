# Full-build physical-UAP lifecycle, 2026-11-11

## Cause and scope

The reported full build completed the plugin but stopped before the physical-UAP
ABI check because HallJoy was already running. The six reported PIDs belonged to
one `.local/portable/HallJoy.exe` application and its five direct workers, rather
than six independent application windows. Compiler warnings in the portable test
fixtures were not the failure that stopped this build.

`tools/build.ps1` now runs this check through `tools/uap_runtime_session.ps1`.
It accepts one argument-free application in the current Windows session, at
either `.local/portable/HallJoy.exe` or `build/bin/Release/x64/HallJoy.exe` inside
this checkout. Every other process must have that exact executable path, be a
direct child of the main process, and have a recognized internal worker role.
Native image-path queries verify executable identity. A second process snapshot,
including process creation times, must match before shutdown begins.

The existing exact-path close helper handles the shutdown. The UAP callback runs
only after process enumeration is empty. The Python ABI check retains its own
independent checks before opening hardware. A `finally` block restores the same
executable and window style after success or failure; its SHA-256 must still match.
An inactive application is not launched. Foreign paths, multiple main processes,
unknown roles and other Windows sessions are rejected before shutdown.

The full-build retry also exposed a PS7 -> CMD -> Windows PowerShell 5.1 module
autoload failure for `Get-AuthenticodeSignature`. The shared build environment
now imports Security from the current host's `$PSHOME`, as it already did for
Utility. No persistent PATH or module-path settings are changed.

## Focused checks

- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/test_uap_runtime_session.ps1`:
  PASS, 13 scenarios. Five lifecycle cases cover one app with five workers on
  success, callback failure, partial shutdown failure, surviving processes and
  inactive state. Eight rejection cases cover a foreign path, foreign session,
  unknown role, multiple main processes, unrelated worker, missing command line,
  enumeration failure and process-snapshot changes. Process boundaries are
  controlled fixtures, not real application shutdowns.
- `tools/test_build_environment.ps1`, invoked from PowerShell 7: PASS, 8
  assertions. The actual CMD wrapper launches Windows PowerShell 5.1, hashes a
  fixture, verifies the signature of that host's `powershell.exe`, forwards spaced
  arguments and propagates both success and injected failure exit codes.
- `git -c core.whitespace=cr-at-eol diff --check` and the corresponding
  `--cached --check`: PASS. The repository preserves CRLF bytes with `* -text`.

Lifecycle fixture evidence:
`build/obj/uap-lifecycle-test-8417c564d0354ee28ea3aa8e89bda482/results.json`.
Full-build logs and process/hash snapshots:
`build/evidence/uap-build-lifecycle-2026-11-11/`.
Pre-edit backups:
`.local/backups/uap-build-lifecycle-20261111-220816/`.

The first full attempt passed the native suite and UI audit, then exposed the
Security module loading failure before reaching the new shutdown code. Its log
is retained as `full-build.log` with exit code 1.

The next attempt (`full-build-retry.log`) completed the real close / UAP PASS /
restore sequence, plus profile/layout/recovery checks. Production compilation
then reported two pre-existing warnings in `irok_na87_backend.cpp`, and the strict
warning gate correctly refused to package that candidate. The telemetry counter
is now named `liveEventCount` to avoid shadowing a packet-validity boolean. The
USB product ID conversion is explicit; production writes originate from the
16-bit `HIDD_ATTRIBUTES::ProductID`. These changes preserve the existing values
and behavior. The production warning allowlist was not broadened.

## Final verification

`Build-Windows.cmd -OutputDirectory <new package directory>` completed with exit
code 0; see `full-build-final.log` and `exit-code-final.txt`. It passed the native
suite, UI audit, physical-UAP ABI check, production-linked profile/layout/recovery
tests, production build, linked-image checks and embedded-resource verification,
then created the installer. The production warning baseline was `LNK4099` only,
with zero unexpected warnings. The exact resulting EXE also passed
`--halljoy-require-full-catalog` with exit code 0.

The real UAP check reported zero connected UAP devices; this is an ABI/lifecycle
result, not a physical keyboard qualification. The installer is unsigned and
was built and hash-verified, not installed.

The build logged immediate application restoration. A later process inventory
found no HallJoy process; the cause was not established. The original portable
copy was reopened, and a dedicated real failure probe then exercised the
production lifecycle with an injected callback exception (without opening HID).
It preserved the expected failure, restored the application, and the restored
main PID 74044 plus five workers survived the Windows PowerShell process exiting.
A separate subsequent process query confirmed the same six PIDs. Running the
full-catalog check also preserved them. Evidence: `real-failure-restore.log`,
`failure-restored-processes.json`, `processes-after.json` and `artifacts.json`.
The original portable EXE SHA-256 remained unchanged.

Final artifacts:

- `build/bin/Release/x64/HallJoy.exe`, version 1.6.15.0:
  `91f5c86a8ddae47baf5cb94c8af43358f97f58857519055238849898a77a19f1`.
- `build/packages/windows-20261111-221713-uap-lifecycle/HallJoy-1.6.15-Windows-x64-setup.exe`:
  `dc39bc7fc7547aecf636a62abdda2b03186b7040122ad2c44c3cdf785c8f136e`.

The installer JSON inventory and SHA-256 sidecar match its actual bytes; its
recorded application payload matches the final EXE above. No commit, remote
repository write, installation or keyboard-support status change was performed.
