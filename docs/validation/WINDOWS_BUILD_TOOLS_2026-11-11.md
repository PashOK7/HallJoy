# Windows build-tool discovery — 2026-11-11

Final outcome: the source-built HallJoy 1.6.15.0 passed the ordinary release gates
and was placed at `build/bin/Release/x64/HallJoy.exe`. `Build-Windows.cmd -PackageOnly`
then produced a matching local installer. Neither the installer nor a remote
publication was launched. See final evidence and hashes below.

## Reported failure and cause

`BUILD.cmd` invoked `python` directly. On this workstation, that name resolved to
the Microsoft Store execution alias and returned 9009. A working uv-managed
Python 3.12.15 x64 was already installed but absent from PATH. The installed
Visual Studio Community 2022 instance had MSVC, but no Clang component.
The installer entry detected the alias and missing Clang, but did not discover
the existing Python interpreter.

After the tool checks and runtime build passed, the incremental release and
profile-test builders exposed a second discovery defect: this Visual Studio
installation returns no result for `vswhere -requires Microsoft.Component.MSBuild`,
but returns its working x64 MSBuild for
`-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64`. Those two scripts
now use the same C++ component query as the full source/dependency builders.

The MSBuild project's encoding gate also invoked `py` directly, although the
launcher was not installed. `HallJoy.vcxproj` now invokes `python`, inheriting
the interpreter selected by the build environment. A focused MSBuild
`/t:CheckSourceEncoding /p:Configuration=Release /p:Platform=x64` run passed
with `SOURCE_ENCODING_STATIC_AUDIT=PASS files=892` before retrying compilation.

Packaging additionally reproduced a PowerShell 7 -> CMD -> Windows PowerShell 5.1
module-path issue: diagnostics inside the failed child showed PSHOME pointing to
Windows PowerShell 5.1, but Utility loaded from the user's PowerShell 7 directory
(version 7.0.0.0, no exported Get-FileHash function). The shared environment and
packager now import Utility from their own `$PSHOME/Modules` explicitly. The CMD
regression fixture also computes a real hash in that nested host. No global
PSModulePath or execution-policy setting was changed. Evidence: `package-diagnosis.log`.

## DPI-dependent test failure exposed by the first successful compilation

The release candidate compiled and passed its linked-image gates, but the profile
transaction suite returned `profiles page browse/scroll/selection tests failed`.
Targeted simulator-only diagnostics narrowed it to
`profiles selector sizing failed: compact=0 icon=0 scaled100=200`.
The production selector compares DPI-scaled mode thresholds, while its test used
fixed 190/80 physical-pixel widths. At the observed 200% scale, neither fixture
provided the intended logical space. `ProfilesPage_Test` now scales these two
fixture widths and retains precise compact/icon failure diagnostics after window
cleanup. Changes are inside `HALLJOY_ANALOG_SIMULATOR`; production layout and
keyboard behavior are unchanged. Evidence: `profile-diagnosis.log` and
`profile-fixed.log` in the build evidence directory.

## Changes

- `tools/build_environment.ps1` shares executable discovery and probes the
  selected Python and Clang executables. Store aliases are excluded; PATH,
  the Python launcher and uv-managed installations are considered. Explicit
  `-PythonPath` is authoritative and fails if invalid.
- `tools/build.ps1` checks the environment before native checks and supports
  `-CheckOnly`. `tools/build_release.ps1` uses the same Python/Clang discovery.
- `tools/build_windows_installer.ps1` uses the shared helper, retaining its
  Visual Studio, Git and NSIS preflight. `-PackageOnly` does not require Clang.
- `BUILD.cmd` forwards arguments, preserves failure codes and pauses only on
  the normal double-click invocation. It does not label a tool check as a built EXE.
- The CMD exit-contract audit and isolated installer test include the updated
  entry contract/helper. `tools/test_build_environment.ps1` exercises the real
  interpreter/compiler and the actual CMD wrapper with an isolated build stage.
- Build and installer guides describe discovery, explicit overrides and tool checks.

On this computer the existing Visual Studio installation was modified through
the Microsoft installer, adding only requested component
`Microsoft.VisualStudio.Component.VC.Llvm.Clang` and its required dependencies,
with `--quiet --norestart`. The elevated installer returned 0; installed Clang
reports 19.1.5. Python was not reinstalled. System/user PATH was not changed by
the build scripts; only process PATH is prepended and inherited by child builds.

Official component and installer references:
[Clang in Visual Studio](https://learn.microsoft.com/cpp/build/clang-support-msbuild?view=msvc-170),
[command-line installation](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio).

## Verification

- `BUILD.cmd -CheckOnly`: exit 0, existing Python 3.12.15 and Visual Studio Clang detected.
- `Build-Windows.cmd -CheckOnly`: exit 0 in the normal user environment, including NSIS.
- `tools/test_build_environment.ps1`: eight assertions passed in Windows PowerShell 5.1 and PowerShell 7.
  Cases cover alias exclusion, explicit interpreter, missing/alias override rejection,
  inherited executable lookup, spaced CMD arguments and build-stage failure propagation.
- PowerShell parser accepted all changed build scripts.
- `python -B tools/project_context.py --check`: PASS.
- Working and staged diffs passed whitespace checks with `cr-at-eol`, preserving
  the existing CRLF files under the repository's `* -text` attribute.

The first source-build attempt found an obsolete static assertion requiring an
unconditional CMD pause. The assertion now checks the argument-aware pause and
exit contract; the second attempt proceeds through the full existing native suite.
Full build evidence is retained under `build/evidence/build-tool-discovery-2026-11-11/`.
The native suite passed (`native backend checks: all static and portable C++ tests passed`)
and the pinned Sun/Soup/UAP build completed. The full wrapper then stopped at its
existing physical-UAP isolation gate because portable HallJoy was running.

Read-only process inspection established one interactive application and five
internal children, all from `.local/portable/HallJoy.exe`. The temporary local
continuation refuses multiple main processes, foreign executable paths or unknown
child roles; it closes that one application using the existing identity-checked
helper and restores it in `finally`. Automatic approval initially rejected the
unqualified six-process closure and accepted it after this evidence and guard.

The just-built UAP passed `PRIVATE_UAP_ABI_RUNTIME` (no UAP devices were detected;
this is not a physical keyboard compatibility result), and its DLL/MAP/provenance
were copied to the normal runtime paths after backing up existing files. Work
continued through the documented incremental release entry rather than repeating
the already-passed full native suite and dependency build.
The first incremental attempt passed its support-diagnostics contract but stopped
at the MSBuild lookup described above; the previous portable application was
restored successfully by the `finally` block. A subsequent incremental attempt
uses the corrected query without closing the portable application again.

## Final evidence

- `tools/run_profile_transaction_tests.ps1` after the DPI fixture fix: exit 0;
  profile transactions, layout catalog, Supported-layout coverage and startup
  recovery (18 scenarios plus repeated startup) passed (`profile-fixed.log`).
- `tools/build_release.ps1`: exit 0 (`release-final.log`). This includes the
  full-catalog/linked-image gates, embedded-resource validation, profile/layout
  tests and atomic placement of the final EXE. The known ViGEmClient LNK4099
  missing-PDB warning was emitted; no claim of a warning-free build is made.
- `Build-Windows.cmd -PackageOnly`: exit 0 (`package-verified.log`), using this
  source-built EXE and matching UAP resources, not a downloaded release fixture.
- SHA-256 inventory and sidecar match the installer; the package's HallJoy.exe
  payload hash matches the final EXE (`artifacts.json`).
- `tools/test_build_windows_installer.ps1`: exit 0, all 15 assertions passed
  (`installer-regression.log`): missing inputs, read-only preflight, mismatched
  resource rejection, real packaging, matching hashes/inventory, refusal to
  overwrite an existing output, preservation of repeated results and input EXE.
  Fixture results: `build/tests/build entry e11c170865744aa2a81867348410b9fe/results.json`.

Final files:

- `build/bin/Release/x64/HallJoy.exe` — SHA-256
  `59beed5c6bc9271d7f0ccb51e24e0fe2284ff6aebb49444a3ab90eff2c2ffe27`.
- `build/packages/windows-20261111-220336-954f44a8/HallJoy-1.6.15-Windows-x64-setup.exe`
  — SHA-256 `6cddde36ee90fed9c34734d2a5b2481eb03a119ffb9aef9abb2d1be2d621fa45`.

The installer is unsigned. Its installation UI and physical keyboard behavior
were not tested. The original portable HallJoy was restored after the isolated
UAP check; it was not replaced with the new build. At this stage the full build
still required manual closure. The subsequent
[UAP lifecycle fix](UAP_BUILD_LIFECYCLE_2026-11-11.md) supersedes that restriction
for one verified local application and restores it automatically after the check.

NSIS detection inside the restricted tool sandbox used a different LOCALAPPDATA
and reported it missing; the normal-user preflight above passed with the existing
NSIS installation. This was not treated as a missing system installation.

Pre-edit backups of the original entry scripts and guides, with SHA-256 manifest:
`.local/backups/build-tool-discovery-20261111-214149/`.
Restore individual files from that snapshot only after checking for later edits.
No commit, remote write, HallJoy installation or keyboard-support change is part of this task.
