# Optional Windows installer

The standalone `HallJoy.exe` remains the normal portable download. This optional
NSIS package adds a per-user installation, Start menu entry, optional desktop
shortcut and Windows uninstall registration. It uses the existing HallJoy name
and icon and does not modify the application binary or keyboard support.

## Build

From the repository root, double-click **`Build-Windows.cmd`**. It checks the
build tools, runs the existing full source/dependency build and packages its
result. Open the resulting `HallJoy-<version>-Windows-x64-setup.exe` to install.
The builder does not start the installation automatically.

Each successful invocation creates a fresh `build/packages/windows-<timestamp>-<id>/`
folder with the installer, SHA-256 sidecar and JSON inventory. The console prints
the exact path. Earlier installers are retained, including repeated builds of
the same version. If compilation fails, an old EXE is never packaged as a new build.

Build-machine prerequisites: Visual Studio 2022 Desktop development with C++
(MSVC x64, Windows SDK and C++ Clang tools for Windows), Git, Python 3.10+ and
NSIS 3. See the existing [source-build requirements](BUILD_README.txt).
The full build fetches its pinned dependency sources through the existing build
scripts; it does not install system tools. These developer tools are **not**
required on a computer that only installs the finished package.

```powershell
.\Build-Windows.cmd -CheckOnly
.\Build-Windows.cmd -PackageOnly
```

`-CheckOnly` reports missing tools before compilation without creating a package.
It is a prerequisite check, not a source-build or embedded-resource validation.
`-PackageOnly` explicitly packages `build/bin/Release/x64/HallJoy.exe` from an
earlier build and still verifies the embedded resources. No compiler is needed
for this mode. Optional `-PythonPath <python.exe>`, `-NsisPath <makensis.exe>` and
`-OutputDirectory <new-folder>` arguments are forwarded to the PowerShell entry.
NSIS is found via `NSIS_EXE`, standard installation folders, PATH or the existing
local Tauri NSIS cache. PATH changes apply only to the build process.

The existing `BUILD.cmd` remains the application's original build entry.

### Direct packaging for maintainers

Requirements: Windows x64, PowerShell 5.1+, Python from the normal build environment,
NSIS 3 and a completed HallJoy release build with matching `build/runtime` inputs.
No compiler, dependency, driver or binary is downloaded by the packaging script.

After the normal [release build](BUILD_README.txt):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/package_windows_installer.ps1 -NsisPath 'C:\Program Files (x86)\NSIS\makensis.exe'
```

NSIS can also be located through `NSIS_EXE`, its standard installation directory
or PATH. `-ExecutablePath` accepts another locally prepared HallJoy build; its
version must match `version.h`, and its embedded license, notices and analog
runtime must pass the existing `verify_embedded_licenses.py` gate. The packager
does not launch HallJoy. It preserves the release's four-part version, including
nonzero hotfix numbers, and normalizes a final `.0` in the output filename.

Output: `build/packages/HallJoy-<version>-Windows-x64-setup/`, containing
`HallJoy-<version>-Windows-x64-setup.exe`, a SHA-256 sidecar and a JSON inventory.
An existing output directory is rejected. Use `-OutputDirectory` with a fresh
directory for another candidate. NSIS warnings fail the build. The verified
payload stays in the `PAYLOAD=` directory printed by the command.

The installer is unsigned. Signing and release upload remain separate maintainer
actions; the existing ZIP packager and release automation are unchanged. When
distributing a package, provide the corresponding source and dependency sources
under their existing license terms, just as for the standalone application.
The package is not an alternative source distribution or a source-license waiver.

## Installation behavior

- Installs for the current user under `%LOCALAPPDATA%\Programs\HallJoy` by default.
- Adds Start menu shortcuts and an optional desktop shortcut using `HallJoy.ico`.
- Ships `LICENSE`, `THIRD_PARTY_NOTICES.md`, `dependency-lock.json` and local help.
- Shows the project's existing license in the installation wizard before the
  directory/component choices; no additional license agreement is invented.
- Does not install drivers, launch HallJoy, add autostart or contact the network.
- Uses normal `%LOCALAPPDATA%\HallJoy` storage; portable settings are not bundled
  or migrated. ViGEmBus is installed separately through the existing HallJoy UI.
- Rejects unrelated nonempty folders, locked HallJoy executables and a second
  installation location while another one is registered for the current user.
- Supports reinstalling in the same folder. Uninstall removes only named package
  files and owned shortcuts; settings, profiles and the shared driver remain.

## Focused integration test

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test_windows_installer.ps1 -PayloadPath '<PAYLOAD directory printed by the packager>' -NsisPath 'C:\Program Files (x86)\NSIS\makensis.exe'
```

The test recompiles the production recipe with a unique test registration below
`HKCU\Software\HallJoyInstallerTests`. Files and all shortcut locations stay under
`build/tests/windows installer <id>`. It exercises install, rejected destinations,
locked-file rejection, reinstall, payload hashes, desktop shortcut creation and
preservation, and uninstall with user files retained. It does not launch the
application or driver installer, or change real Start menu/desktop shortcuts.
Results remain in `results.json`; these tests are not hardware or visual approval.

The build entry has a separate isolated packaging test. Supply an EXE and the
matching runtime from a local build; it verifies preflight, resource mismatch,
exit statuses, output hashes and preservation across repeated invocations:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test_build_windows_installer.ps1 -PythonPath '<python.exe>' -NsisPath '<makensis.exe>' -ExecutablePath '<HallJoy.exe>' -RuntimePath '<universal_analog_abiv1.dll>'
```

This focused test does not compile the C++ application, launch HallJoy or install
the resulting package. Output is isolated under `build/tests/build entry <id>/`.
