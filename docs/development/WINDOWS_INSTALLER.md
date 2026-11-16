# Optional Windows installer

HallJoy remains a standalone portable EXE. `BUILD.cmd` and
`tools/build_release.ps1` produce `build/bin/Release/x64/HallJoy.exe`.
They do not require NSIS, package an installer or create a second application copy.
No setup is required to run HallJoy. Personal-data mode remains unchanged:
normal local data, or a `HallJoy.portable` marker for data beside the EXE.

## Optional packaging

After a normal application build, explicitly run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_windows_installer.ps1 -CheckOnly
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_windows_installer.ps1
```

Only this optional command requires NSIS 3, Python3.10+ x64 and matching embedded
runtime/license inputs. It compiles a new setup package from the existing EXE;
it does not rebuild, launch, replace or close HallJoy. No tools are downloaded or
installed by this command. `-CheckOnly` checks prerequisites without packaging.
Use `-PythonPath <python.exe>`, `-NsisPath <makensis.exe>` or `NSIS_EXE` for tools
outside PATH. `-OutputDirectory <new-directory>` selects a new package directory.
Incremental application builds accept their existing supported C++ compiler and
cached runtime; they do not require Clang solely for tool discovery.

Each successful invocation writes a unique directory under `build/packages/`:
`HallJoy-<version>-Windows-x64-setup.exe`, its `.sha256` and JSON payload inventory.
Existing output directories are refused. There is no stable app/setup output pair
in build's root, so failed setup publication cannot leave two different generations
there. Previous packages and the ordinary EXE stay unchanged on packaging failure.
Internal staging files remain under `build/obj/windows-installer/` for inspection.
The EXE is staged and hashed, and its embedded runtime/license bytes are verified.

## Installation and removal

The optional wizard includes welcome, license, data/permissions, destination,
components, installation and completion pages. It uses the existing HallJoy icon.
It defaults to `%LOCALAPPDATA%\Programs\HallJoy`, registers per-user uninstall,
creates Start menu shortcuts and optionally a desktop shortcut. Setup does not
launch HallJoy, install ViGEmBus, modify keyboard firmware or create autostart.
The installer is unsigned; a checksum is not a publisher signature.

Installation is separate from portable use. Setup creates no portable marker.
Uninstall offers an unchecked-by-default **Delete settings, profiles, logs and
cached data** checkbox. Leaving it unchecked preserves all personal data.
Selecting it removes the current Windows user's entire `%LOCALAPPDATA%\HallJoy`
data folder (including backups, recovery state and runtime caches), known legacy
settings/logs/dumps beside this installed EXE, HallJoy WER dumps and abandoned
HallJoy-owned temporary driver extracts. Other portable folders, unknown files
beside the installed program, unrelated dumps/temp files and the shared ViGEmBus
driver are preserved. Data shared with other HallJoy copies is explicitly named
in the consent page. Cleanup matches known entries beside the EXE by name and
file or directory type. Program/data directory overlap is refused.

Silent uninstall also preserves data unless explicitly passed `/REMOVEUSERDATA=1`;
`/REMOVEUSERDATA=0` keeps it, invalid values abort. Cleanup does not follow directory
junctions or symbolic links. If a selected data file cannot be removed, uninstall
reports failure and retains the app, registration and uninstaller for retry;
completed deletions are not rolled back. Close other copies before data cleanup.
See [scope and test evidence](../current/WINDOWS_UNINSTALL_DATA_2026-11-15.md).
Close an installed copy before upgrading.
In-place installed upgrades are not a multi-file transactional installer; retain
configuration backups. This does not affect portable application publication.

## Validation

`tools/test_build_environment.ps1` checks optional compiler discovery.
`tools/test_portable_helpers.ps1` uses fake network responses and private files.
`tools/test_build_windows_installer.ps1` packages a supplied existing EXE in a
private checkout and verifies success/failure, hashes and preserved source/output.
`tools/test_windows_uninstall_data.ps1` extends the installer suite with default/
explicit-preserve, full cleanup, junction, locked-file/retry and path-overlap cases.
`tools/test_windows_installer.ps1` exercises the real recipe with TEST_ROOT paths,
test-only HKCU keys and shortcuts; it never launches HallJoy. Owner visual review
is separate; no wizard visual automation is required for normal packaging.

Contribution adapted from PR#11 (`d9911`, head2bc4e5a). Source/runtime changes,
mandatory installer BUILD behavior, duplicate EXE publisher and unused alternate
branding assets are outside this adaptation. See the [current integration report](../current/PR11_OPTIONAL_INSTALLER_2026-11-15.md).
