# Optional Windows installer validation — 2026-11-11

Base: `323774e4544326b8aeb7560a0f173b1ff6fc68e5` (HallJoy 1.6.15).
Scope: additive NSIS packaging, isolated integration tests and documentation.
No application C++, protocol, support-status, release-automation or driver changes.

## Packaging and integration

`tools/package_windows_installer.ps1` completed with NSIS 3 and `/WX`.
The input was the official HallJoy 1.6.15 executable, SHA-256
`59b3886d9c99e76131b0629e25acd2477144771216dc0cc00aaa57d6e8e11139`,
previously checked against the official release digest. This was a packaging
test with a released binary, not a fresh application build from this checkout.

The fresh clone had no built analog runtime. For this packaging test only,
resource 131 was read from that EXE using `LoadLibraryExW` with
`LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE` and used as a temporary
runtime fixture. The existing `verify_embedded_licenses.py` then passed its exact
runtime/license/notices comparison and license-viewer resource check.
The temporary runtime fixture was removed after testing; no application or
keyboard code was executed to extract it. This is not a provenance verification
of a locally compiled runtime or a substitute for the normal build gates.

Output: `build/packages/HallJoy-1.6.15-Windows-x64-setup/HallJoy-1.6.15-Windows-x64-setup.exe`.
Measured size: 5,364,844 bytes (`Get-Item`). SHA-256 (`Get-FileHash`):
`807f8233d68d5f246fddc573435d9d2d2b489217385849a153045d69ab37432e`.
Authenticode: `NotSigned`. The checksum and JSON sidecars matched this file.

`tools/test_windows_installer.ps1` passed **42 assertions**, as counted from
`checks` in `build/tests/windows installer 1c4ebaf23c0a42e980d30349a713def3/results.json`:

- install into a path containing spaces and verify all six payload files;
- validate Start menu and optional desktop shortcut target/icon;
- reject a foreign nonempty directory and a second registered installation path;
- reject replacement of a locked EXE, retaining its bytes;
- reinstall and uninstall while preserving user-created settings and profiles;
- preserve a foreign desktop shortcut even when the desktop component is selected;
- create and remove the installer's own desktop shortcut in a fresh installation;
- remove package files, test menu shortcuts and isolated test registration.

Additional negative checks rejected an existing package output directory and a
1.6.14 EXE with a mismatched version. Both left the existing installer unchanged.
The two new PowerShell scripts parsed successfully under the PowerShell parser.

## Project checks and limitations

`python tools/run_native_backend_checks.py --require-compiler` completed its
static audits and seven project-context unit tests, then exited with failure
because no `g++` or `clang++` was available. Portable C++ tests did not run.
Log: `build/evidence/installer-native-checks.log` (local only).

`tools/build_release.ps1` failed its initial runtime provenance gate: the fresh
checkout had no `build/runtime/universal_analog_abiv1.dll` or associated build
outputs. The full HallJoy application was not compiled or qualified here.
The default Python command was an unavailable Windows Store alias; an existing
bundled Python was used through a process-local PATH, without installing software
or changing persistent system settings.

No physical keyboard/game testing, graphical wizard review, clean-Windows
installation, signing, release upload or pull request was performed. Existing
standalone distribution and release automation remain unchanged. The maintainer
still needs the normal build environment and full build checks before release.
