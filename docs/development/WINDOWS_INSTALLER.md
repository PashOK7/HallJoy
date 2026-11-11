# Windows installer

`BUILD.cmd` produces a Windows setup executable. This NSIS package provides
a per-user installation, Start menu entry, optional desktop
shortcut and Windows uninstall registration. It uses the existing HallJoy name
and icon and does not modify the application binary or keyboard support.

## Build

From the repository root, double-click **`BUILD.cmd`**. It checks the
build tools, runs the existing full source/dependency build and packages its
result. Open **`build/HallJoy-setup.exe`** to install, or **`build/HallJoy.exe`**
to run the application directly.
The builder does not start the installation automatically.

During the physical-UAP check, the full build temporarily closes one verified
HallJoy application from this checkout's `.local/portable`, `build` or
`build/bin/Release/x64`, including its known worker processes. It restores the
same application after the check, including on failure. Other paths, multiple
applications and unknown process roles require manual closure; they are not
terminated automatically. See [the build lifecycle details](BUILD_README.txt).

After successful packaging, the builder verifies both EXEs against the package
inventory and places only `HallJoy-setup.exe` and `HallJoy.exe` directly in `build`.
These stable filenames are replaced on subsequent successful builds; the console
prints both paths. Each file is replaced atomically, with backups kept internally.
A locked existing setup is rejected before replacing the application. This is
not a transaction across both files: an OS error between replacements can leave
different generations, in which case the command fails and retains the archive.
Each invocation also retains a versioned package, SHA-256 sidecar and JSON
inventory under `build/packages/windows-<timestamp>-<id>/`. If compilation or
packaging fails, the previous final files remain; an old EXE is never packaged
as a new source build. Intermediate compiler and staging files stay in subfolders.

Build-machine prerequisites: Visual Studio 2022 Desktop development with C++
(MSVC x64, Windows SDK and C++ Clang tools for Windows), Git, Python 3.10+ and
NSIS 3. See the existing [source-build requirements](BUILD_README.txt).
The full build fetches its pinned dependency sources through the existing build
scripts; it does not install system tools. These developer tools are **not**
required on a computer that only installs the finished package.

The builder detects existing uv-managed Python as well as PATH/py installations,
and ignores Windows Store execution aliases. Clang and clang++ are discovered
through Visual Studio or PATH. The selected paths are printed before checks and
inherited by the source build; no persistent user/system PATH change is made.
See [the shared tool-discovery guide](BUILD_README.txt) if a prerequisite is missing.

```powershell
.\BUILD.cmd -CheckOnly
.\BUILD.cmd -PackageOnly
```

`-CheckOnly` reports missing tools before compilation without creating a package.
It is a prerequisite check, not a source-build or embedded-resource validation.
`-PackageOnly` explicitly packages `build/bin/Release/x64/HallJoy.exe` from an
earlier build and still verifies the embedded resources. No compiler is needed
for this mode. Optional `-PythonPath <python.exe>`, `-NsisPath <makensis.exe>` and
`-OutputDirectory <new-folder>` arguments are forwarded to the PowerShell entry.
`-OutputDirectory` selects the package archive location; the final two EXEs still
go directly into `build`. `-PackageOnly` also refreshes these final files.
NSIS is found via `NSIS_EXE`, standard installation folders, PATH or the existing
local Tauri NSIS cache. PATH changes apply only to the build process.

`BUILD.cmd` is the single public build entry. It builds the standalone EXE once,
then packages those same bytes. `build/bin/Release/x64/HallJoy.exe` is the internal
application output. Use the final `build/HallJoy.exe` for direct launch or
`build/HallJoy-setup.exe` for the installation wizard. Internal application-only
scripts do not refresh this final pair; use `BUILD.cmd -PackageOnly` afterwards.
The PowerShell files under `tools/` implement the stages of this one workflow.

The wizard includes welcome, license, data/permissions notice, destination,
components (optional desktop shortcut), installation and completion pages.
Program files default to `%LOCALAPPDATA%\Programs\HallJoy`; normal personal
settings/profiles/layouts are stored separately in `%LOCALAPPDATA%\HallJoy`.
The embedded analog runtime is cached on demand under
`%LOCALAPPDATA%\HallJoy\Runtime\v<version>`.
Setup registers removal in Windows Installed apps and creates `Uninstall.exe`.
The packaged `PRIVACY_AND_SECURITY.txt` explains input access, local reports,
driver permissions and preservation of personal data on uninstall. The setup
package is unsigned; no signing certificate or publisher identity is implied.

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
`tools/test_windows_installer_wizard.ps1 -InstallerPath <test setup.exe>` separately
walks the five pre-install wizard pages through the spawned process's own controls.
It verifies the data/permissions page and stops before clicking Install.

The build entry has a separate isolated packaging test. Supply an EXE and the
matching runtime from a local build; it verifies preflight, resource mismatch,
exit statuses, output hashes and preservation across repeated invocations:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test_build_windows_installer.ps1 -PythonPath '<python.exe>' -NsisPath '<makensis.exe>' -ExecutablePath '<HallJoy.exe>' -RuntimePath '<universal_analog_abiv1.dll>'
```

This focused test does not compile the C++ application, launch HallJoy or install
the resulting package. Output is isolated under `build/tests/build entry <id>/`.
