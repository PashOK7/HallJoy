# Optional removal of HallJoy data

Owner decision2026-11-15: uninstall needs a checkbox to remove saved settings,
logs and other HallJoy-generated data; unchecked by default. HallJoy remains
portable and installer packaging remains entirely optional.

The uninstall wizard adds a Personal data page with **Delete settings, profiles,
logs and cached data**. It starts unchecked on every uninstall. Back/Next retains
the choice; the checkbox state feeds the same explicit-consent branch used by
silent `/REMOVEUSERDATA=1`. Silent default and `/REMOVEUSERDATA=0` preserve data;
invalid values abort before deleting anything. The consent text explains that
AppData is shared by other HallJoy copies belonging to the current Windows user.

## Storage inventory and cleanup

- `%LOCALAPPDATA%\HallJoy`: entire dedicated data tree, including settings,
  bindings, game/global profiles, layouts, curves, factory-reset and migration
  backups, recovery/migration internals, reports and extracted runtime cache.
- Current installation folder: known legacy settings/backup names, profile/layout/
  curve folders, migration/reset state, known HallJoy log/crash/qualification files
  and their temporary/atomic-write residues. In this folder, removal matches both
  the known name and the expected file or directory type.
- `%LOCALAPPDATA%\CrashDumps`: `HallJoy.exe.*.dmp` only.
- Current user's TEMP: `HallJoy-ViGEm-<32 hex digits>` extract directories only.
  This matches embedded_vigem_installer.cpp's actual producer, including abandoned
  extracts; other names/files remain. The installed shared driver is untouched.

Sources reviewed: app_paths.cpp, support_log.cpp, debug_log.cpp,
analog_host_client.cpp, embedded_analog_stack.cpp, embedded_vigem_installer.cpp,
factory_reset.cpp, ini_util.cpp, stability_trace.cpp and qualification report code.
Explicit user exports or independent portable directories are not searched/deleted.
No other users' data, global runtime/driver installation, Windows-managed general
caches or unrelated software's temporary files are targeted.

`tools/installer/data_cleanup.nsh` contains the scoped cleanup. Recursive traversal
checks attributes for every entry, unlinks reparse points themselves and never
uses recursive RMDir. Pattern containers cannot be reparse points. Native errors
are captured in the same API call. Program/data roots must be separate, so cleanup
cannot recursively remove the installed app before it finishes; canonicalization
accepts not-yet-created destinations. A locked/denied data file produces failure6,
retaining the installed app, registration and uninstaller for retry. Successful
individual deletions cannot be rolled back. No blind cleanup delays are used.

## Validation and delivery

- 129 installer/uninstaller assertions passed under Windows PowerShell 5.1,
  covering default and explicit preservation, opt-in erase, shared runtime/backup
  cleanup, Unicode names, exact-name and pattern lookalikes, unknown files, independent
  portable copy, child/root junctions, invalid arguments, data-root overlap,
  locked data and retry. Test-only HKCU keys and all paths isolated by TEST_ROOT.
- 20 optional-packaging assertions PASS, including compiler failure and preserved
  ordinary app/old packages. PowerShell parsing and local documentation links PASS.
- Native suite PASS with the updated optional-installer contract audit. Ordinary
  release wrapper PASS:10-family exact image, resources, production profiles,
  layout aliases,92 Supported layouts and18 recovery cases plus repeated startup.
  Candidate matches installed app; no interactive app was closed/replaced.
- No app/driver launch or visual wizard run. Owner evaluates visuals.

Local unsigned package:
`build/packages/windows-20261115-102714-95a368de/HallJoy-1.6.15-Windows-x64-setup.exe`,
5,520,492 bytes, SHA256
`a6c912b639438df66a3735fa0467d0c5d30ace0b26f9a111587d0c9df4c75129`.
SHA256 sidecar and seven-file payload inventory are adjacent. Ordinary application
remains1.6.15.0, SHA256
`6a12acddb9c57adbfc4c996c44be3e9390923294d771b5f8f1c4e1a22adfdb17`.
Source/output manifest and logs: `.local/reviews/pr11-20261115/cleanup-*`.
Final behavior receipt: `build/tests/windows installer 535fa60650be4463994c2569042c099c/cleanup-results.json`.
The initial fixture exposed a reset native GetLastError and missing-directory
canonicalization issue; both corrected before delivery, with regressions passing.
The normal EXE, application source code and BUILD.cmd are unchanged. The earlier
[PR11 integration report](PR11_OPTIONAL_INSTALLER_2026-11-15.md) describes the
initial preserve-only uninstall; this later owner decision supersedes that part.
Backups: `.local/backups/installer-clean-data-20261115/` and
`.local/backups/installer-clean-data-entry-type-20261115/`.
Restore backed-up files and remove only this task's added helper/test/report to
roll back. No GitHub merge, push or release publication is part of this task.
