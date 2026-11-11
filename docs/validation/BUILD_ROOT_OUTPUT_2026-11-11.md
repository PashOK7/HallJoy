# Final executables directly in build — 2026-11-11

Owner request: keep the finished Windows installer and standalone application
directly in `build`, without navigating compiler/platform or timestamp folders.

## Result

`BUILD.cmd` still runs the source build once, then packages that exact application.
After successful packaging, `tools/publish_windows_outputs.ps1` snapshots the two
executables, verifies their SHA-256 against the package inventory, and places:

- `build/HallJoy-setup.exe` — Windows installation wizard.
- `build/HallJoy.exe` — standalone application.

These stable names are updated on successful builds. Internal artifacts, archived
versioned installers, inventory, hashes and replacement backups stay in subfolders.
The publisher reuses the existing exact-path application replacement helper.
The physical-UAP lifecycle allowlist now also accepts `build/HallJoy.exe` with
its verified worker family. Unknown paths and multiple applications still fail.

The two file replacements are individually atomic, not a cross-file transaction.
An OS failure between replacements is reported as build failure; the completed
archive and existing replacement backups remain available. Compilation, packaging
or manifest-hash failures occur before replacing final files.

## Validation actually performed

- `tools/test_build_windows_installer.ps1`: **29 checks passed**. Real NSIS
  packaging in an isolated checkout; successful default CMD sequencing uses a
  controlled source-stage stub. Covers final hashes, only two root files,
  repeated replacement, corrupt inventory rejection, and preservation after
  source-stage failure. No HallJoy execution or installation in this suite.
  Evidence: `build/tests/build entry a244c2e3f07d42a09e081d8d8771f246/results.json`.
- `tools/test_uap_runtime_session.ps1`: **14 fixture scenarios passed**, including
  acceptance of the new build-root path with five workers. Real processes and
  physical keyboard access were not used by this test.
  Evidence: `build/obj/uap-lifecycle-test-cd9e2c29e9cd4692bae1f46f175be44a/results.json`.
- `BUILD.cmd -PackageOnly`: **exit 0**, used the existing compiled application,
  verified embedded ABI1/licenses/notices/viewer, compiled a fresh installer,
  and created both final files. No C++ recompilation or system installation.
  Log: `build/evidence/build-root-output-2026-11-11/package.log`.
- Read back the two final files and matched both SHA-256 values to the completed
  package inventory. Evidence: `build/evidence/build-root-output-2026-11-11/final-files.json`.
- PowerShell parser accepted all five changed/new scripts. Both working-tree and
  staged whitespace checks passed using `git -c core.whitespace=cr-at-eol diff
  --check` (and `--cached --check`) for this CRLF-preserving repository.

| Final file | Bytes | SHA-256 |
|---|---:|---|
| `build/HallJoy-setup.exe` | 5366224 | `9f21fe722724600027290d0bea9de563e8ab730cbfa2c8a69a7001d286972e97` |
| `build/HallJoy.exe` | 11426816 | `ae645abf20758991eb8042fcc441b04efc9bc3d135dff69e4c1feeb226f9601b` |

Archive: `build/packages/windows-20261111-224444-83b86fd4/` (version 1.6.15).
Installer remains unsigned. No source support changes, commit or remote writes.
Pre-edit backups: `.local/backups/build-root-output-20261111-224012/`.

Updated navigation: root README, Russian README, BUILD_README, WINDOWS_INSTALLER,
and current PROJECT_LAYOUT. Historical dated artifact paths remain historical.

## Follow-up: static documentation gates

The initial placement change missed two existing documentation assertions.
`s20_build_docs_static_audit.py` still required the internal EXE path with literal
backslashes in BUILD_README; `version_identity_static_audit.py` required that
internal path in the public README. The reported S20 failure was reproduced
with the user's Python 3.14.9. After correcting S20, the complete static suite
exposed the second assertion; both were updated to the new delivery contract.

S20 now checks both final paths and the internal compiler output, normalizes
path separators, and reports architecture/logging failures separately. The
version audit retains the project/build target checks and separately checks
the final standalone path in README. BUILD_README explicitly documents the
internal EXE and how PackageOnly refreshes the final pair.

Fresh verification: Python 3.14.9, `tools/run_native_backend_checks.py
--static-only`, exit 0, `native backend checks: static audits passed`.
Log: `build/evidence/build-docs-audit-2026-11-11/static-final.log`.
Working-tree and staged whitespace checks also passed. No C++ compilation,
hardware test or installer rebuild was performed for this documentation/audit fix.
Backups: `.local/backups/build-docs-audit-20261111-224801/`.
