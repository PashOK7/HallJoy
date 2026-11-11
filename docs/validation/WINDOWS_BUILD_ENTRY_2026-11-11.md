# Windows installer build entry — 2026-11-11

## Change

`Build-Windows.cmd` calls `tools/build_windows_installer.ps1`. The default path
checks prerequisites, runs the existing `tools/build.ps1` in a child process,
then calls the existing verified NSIS packager. A failed source build stops before
packaging. `-PackageOnly` explicitly uses an already built executable; `-CheckOnly`
does not compile or package. Each build gets a new directory under `build/packages`.
The original `BUILD.cmd`, installer recipe and application code are unchanged.

MeetScribe Lite was inspected only as a workflow example; it was not edited.
Existing staged HallJoy changes and all personal portable settings were retained.
A verified pre-edit document backup remains in `.local/backups/before-build-entry-*`.

## Fresh evidence

- Full-build prerequisite checking found Visual Studio C++ and NSIS, but reported
  missing Clang / a working compiler for portable C++ checks. The machine's Python
  command is a Windows Store alias, so validation used an explicit existing
  Python executable through `-PythonPath`. No system tool was installed.
- `tools/test_build_windows_installer.ps1`: **15 assertions passed**. It checks
  missing input, read-only preflight, mismatched embedded runtime rejection,
  real NSIS packaging, final hashes/inventory, existing-output rejection and
  preservation of both results from repeated builds. Paths include spaces.
- Results: `build/tests/build entry 79dec872e0d04eac918d2ea06dbe251b/results.json`.
  Process logs are retained beside that file. No HallJoy executable was launched.
- The actual `Build-Windows.cmd -PackageOnly -CheckOnly` entry also passed with
  exit code 0 in that isolated checkout, including argument paths with spaces.
  PowerShell syntax parsing, `git diff --check`, `git diff --cached --check`,
  publication-input checks and `tools/project_context.py --check` passed.
- The fixture was the official HallJoy 1.6.15 EXE with its matching extracted
  resource, not a fresh source build. The produced setup SHA-256 is
  `807f8233d68d5f246fddc573435d9d2d2b489217385849a153045d69ab37432e`,
  identical to the previously validated installer package.
- NSIS required execution outside the tool sandbox: inside it Windows rejected
  the process with an illegal system DLL relocation error. The successful runs
  used the same local NSIS binary and only isolated build/test directories.

## Limits

The full source-build path remains unverified on this computer because the
prerequisite check fails. It was not replaced by packaging a downloaded EXE
silently. A configured Windows build machine must validate that path.
No long native suite was repeated for this wrapper-only change; the earlier
missing-compiler and runtime-provenance limits still apply. The unchanged
installer integration results are recorded in
[Windows installer validation](WINDOWS_INSTALLER_2026-11-11.md).

There was no visual wizard review, physical keyboard/game test, code signing,
commit, push or remote repository update. A checksum is not a publisher signature.
