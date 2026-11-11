# Installer as the default build result, 2026-11-11

## User-visible contract

`BUILD.cmd` now invokes `tools/build_windows_installer.ps1`: a normal invocation
builds the application and pinned dependencies, then packages the resulting
Windows setup executable. Following the owner's consolidation request, the
duplicate `Build-Windows.cmd` and `Build-App.cmd` wrappers were removed.
`BUILD.cmd` is the single public entry and packages the same EXE it builds. Arguments,
failure exit codes and the no-arguments completion pause are preserved.

The setup EXE is an NSIS wizard, separate from the `HallJoy.exe` application.
It includes welcome, license, data/permissions, destination, component selection,
installation and completion pages. The new informational data page is not an
additional license acceptance. Its text is also installed as
`PRIVACY_AND_SECURITY.txt` and removed with the other owned program files.

The existing per-user installation behavior was retained and exercised:

- Program files: `%LOCALAPPDATA%\Programs\HallJoy` by default.
- Settings/profiles/layouts: `%LOCALAPPDATA%\HallJoy`.
- Embedded analog runtime cache: `%LOCALAPPDATA%\HallJoy\Runtime\v<version>`,
  materialized by the application when needed (`embedded_analog_stack.cpp`).
- Start menu shortcuts, optional desktop shortcut, Windows uninstall registration
  and `Uninstall.exe`.
- Reinstallation preserves personal files; uninstall removes named program files
  and owned shortcuts while preserving personal data, unknown files and the
  shared ViGEmBus driver. Driver installation needs its own explicit action.

## Verification

- `build_exit_contract_static_audit.py`: PASS, including installer-default and
  application-only routing.
- Before consolidation, `tools/test_build_environment.ps1` passed 12 assertions
  across three wrappers. The current test targets only the retained `BUILD.cmd`.
- `tools/test_windows_installer.ps1`: PASS. Actual NSIS install, reinstall,
  uninstall, locked/nonempty destination rejection, shortcut ownership,
  registration, payload hashes and personal-file preservation. All files and
  shortcuts were isolated under `build/tests/windows installer
  dbce16a0b0b44d4083107342ec57b5c3`; registry writes used its unique
  `HKCU\Software\HallJoyInstallerTests` key, removed by uninstall.
- `tools/test_windows_installer_wizard.ps1`: PASS. The actual spawned test wizard
  reached all five pre-install pages through its own Win32 controls. It verified
  the informational continuation text and stopped before clicking Install.
  The probe waits for the page text to finish changing before advancing.
- `tools/test_build_windows_installer.ps1`: PASS, including default `BUILD.cmd`
  source-stage invocation, failure blocking packaging, successful packaging,
  payload hashes, resource-mismatch rejection and old-package preservation.
  The source compiler stage was a controlled stub; resource verification and
  NSIS packaging were real. An initially incomplete fixture lacked the Python
  prerequisite module; the fixture was corrected and the complete test passed.
- `BUILD.cmd -PackageOnly`: PASS, real final package created from the existing
  application; its embedded resources matched the checkout. C++ compilation
  was not repeated for this packaging-only change.
- Exact packaged application `--halljoy-require-full-catalog`: PASS, exit 0.
- Working-tree/index whitespace checks: PASS with `core.whitespace=cr-at-eol`.

Logs and final hashes: `build/evidence/installer-default-2026-11-11/`.
Final default-entry test results:
`build/tests/build entry ba923e7271c549ba8a3da4597fbed12f/results.json`.
Pre-edit backups: `.local/backups/installer-default-20261111-222553/`.

## Result and limits

Installer:
`build/packages/windows-20261111-223216-757e8d2d/HallJoy-1.6.15-Windows-x64-setup.exe`

SHA-256: `ce2c9be1fef21564770e32a48c4d6e563670d04611d89af52d08d2538e8637aa`.
Packaged application SHA-256:
`7abda56e274947aa1d2b62703260a3c73e34a2e9b0e16c72b9a68f09269cdd3b`.
Both match the actual package/inventory/sidecar verification.

The installer is unsigned. No signing identity is claimed and no Windows
security settings are disabled. The regular installed application, real desktop
shortcuts, personal data and shared driver were not changed by the isolated
tests. No commit or remote repository write was performed.

The reported C4456/C4244 sites remain corrected in the current source
(`liveEventCount` and explicit 16-bit product-ID conversion); those changes are
documented in the preceding UAP build lifecycle report.

## Single-entry consolidation

The owner requested one command for the application-to-installer chain.
`Build-App.cmd` and `Build-Windows.cmd` were removed after hash-verified backups
under `.local/backups/single-build-entry-20261111-223619/`. Current English and
Russian guides point to `BUILD.cmd`. Internal PowerShell stages remain reusable
implementation details; the application is built once and those bytes are
packaged. The existing setup artifact above remains valid and unchanged.

Fresh verification after removing the wrappers:

- Build exit/routing audit: PASS; it also checks that the duplicate wrappers are absent.
- Build environment/CMD behavior: PASS, 8 assertions.
- Installer pipeline integration: PASS, 20 assertions. The test uses a controlled
  source-stage exit and actual NSIS packaging; it proves source failure blocks
  packaging, success produces one setup, and packaged EXE bytes are unchanged.
- Current-guide reference scan: no instructions to run the removed wrappers.
- Root build CMD inventory: only `BUILD.cmd`; whitespace checks pass.

Evidence: `build/evidence/single-build-entry-2026-11-11/pipeline.log` and
`build/tests/build entry da9c2eb8da7b4dbbaf1aae5f38979ce7/results.json`.
C++ compilation and installer lifecycle tests were not repeated: their code and
payload did not change in this consolidation.
