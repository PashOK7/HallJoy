# PR #11 Actions review and Windows build fixes

Date: 2026-11-15. Scope: investigate the three reported Actions runs, verify the
accepted optional-installer contribution and fix reproduced local build failures.

## Remote findings

Fresh public GitHub API reads at 12:31 UTC confirmed that
[PR #11](https://github.com/PashOK7/HallJoy/pull/11) was merged at
07:59:33 UTC (08:59:33 Europe/Paris). The merge commit is
`35ba83ac1f42fe849bd896721503a880a8cf4c61`; the accepted PR head is
`ab97588c3f9f71aca84c7016db1ccffe63332bd7`. The API reported no conversation or
inline review comments. The accepted PR head and merge commit have identical trees.

| Run | Observed result | Meaning |
| --- | --- | --- |
| [43692130366](https://github.com/PashOK7/HallJoy/actions/runs/43692130366) | Success, attempt 2 | `checks` passed; `windows-release` was skipped by its manual-only condition. |
| [43107826250](https://github.com/PashOK7/HallJoy/actions/runs/43107826250) | Failure | GitHub's annotation says approval was required and expired before approval. No executed test failure is shown. |
| [43109035351](https://github.com/PashOK7/HallJoy/actions/runs/43109035351) | Failure | The same approval-expiration annotation; this is not evidence of a compilation failure. |

The successful run also has a Node.js 20 deprecation warning for the pinned
checkout/setup-python actions. It did not fail the checks. The action versions
were not changed in this task.

PashOK7's accepted follow-up commit keeps ordinary builds EXE-only, makes installer
packaging explicit and adds unchecked-by-default personal-data cleanup on
uninstall. Those decisions remain intact.

## Reproduced defects and changes

1. `build_release.ps1` could not find the portable C++ compiler when Visual Studio
   Community's Clang was installed outside PATH. `find_cxx()` knew only three fixed
   Windows locations. It now queries the installed `vswhere` for x64 Clang after
   explicit CXX, PATH and existing fixed-path candidates. Discovery is bounded to
   ten seconds, requests UTF-8 output and checks candidate existence. No compiler
   is downloaded and no persistent environment setting changes.
2. Release and profile-test scripts selected installations with
   `Microsoft.Component.MSBuild`. On the inspected VS Community installation that
   query returned no result despite an installed amd64 MSBuild. Selecting
   `Microsoft.VisualStudio.Component.VC.Tools.x86.x64`, as `build.ps1` already does,
   finds the installed compiler toolchain in both scripts.
3. MSBuild's source-encoding target called `py`, which is absent in this uv-managed
   Python environment. The build environment already places the resolved Python
   on PATH. The target now uses `python`; running the actual CheckSourceEncoding
   target passed for 894 files.
4. The production-linked profile suite failed its selector sizing fixture. The
   production selector compares DPI-scaled widths, while its test used unscaled
   190/80-pixel offsets. Those two fixture offsets now use the same scaling helper.
   This change is inside `HALLJOY_ANALOG_SIMULATOR`; ordinary UI behavior is
   unchanged. The profile suite, layout audits and startup recovery then passed.

`tools/tests/test_compiler_discovery.py` adds seven Windows regression tests and
is wired into the existing Actions checks job. Before the fix, the two tests for
Visual Studio discovery failed with `None` instead of the available compiler.
All seven passed afterward. Tests cover CXX/PATH priority, custom/Unicode paths,
missing/stale candidates, unavailable discovery, process errors and timeout.

## Verification

Commands ran locally with the normal exact-input native test cache enabled.
Win32 private-desktop/process tests and NSIS required execution outside the agent
sandbox. The initial sandbox-only failures are preserved in the local logs;
the same checks subsequently passed outside that restriction.

| Check | Result |
| --- | --- |
| `python -m unittest discover -s tools/tests -p test_compiler_discovery.py -v` | PASS, 7 tests; real VS Community Clang discovery also succeeded without setting CXX. |
| `python tools/run_native_backend_checks.py --require-compiler` | PASS, all static and portable C++ checks. |
| `tools/test_portable_helpers.ps1` | PASS, 25 assertions, fake network and isolated settings. |
| `tools/test_build_environment.ps1` | PASS, including compiler-absent incremental discovery. |
| `tools/test_build_windows_installer.ps1` with the existing local EXE/runtime and NSIS | PASS, 20 assertions, real packaging and failure preservation. |
| `tools/test_windows_uninstall_data.ps1` with an isolated payload | PASS, 129 cumulative assertions including the 44 base installer assertions; consent, junctions, lookalikes, locked-file failure and retry. |
| `tools/run_profile_transaction_tests.ps1` | PASS, profile transactions, compiled layouts, Supported layouts and 18 recovery scenarios plus repeated startup. |
| `tools/build_release.ps1` after all fixes | PASS, exit 0; full candidate self-tests, embedded licenses, profiles/layouts/recovery and local atomic EXE replacement. See `build/evidence/actions-review-2026-11-15-release-pass.log`. |
| Optional-installer and S20 build-documentation static audits | PASS. |
| Modified PowerShell parsing, publication-input check and project-context check | PASS. |

Final local artifact: `build/bin/Release/x64/HallJoy.exe`, version `1.6.15.0`,
11,426,816 bytes, SHA-256
`cea785b498b715f9a010fa0f8d312df82c915f6a23ae90757568a070e5222c2d`.
These values were read from the published local EXE and are recorded in
`build/evidence/actions-review-2026-11-15-artifact.json`.

Local machine-readable GitHub evidence is retained at
`build/evidence/actions-review-2026-11-15-github.json`. Native, packaging,
uninstall, profile and release logs use the same date/prefix under `build/evidence`.
Fixture JSON inventories record assertion counts; the uninstall count is
cumulative, not 129 additional assertions on top of the base 44.

## Limits and handoff

The packaging/installer suites used the pre-existing local 1.6.15 executable with
verified matching embedded resource inputs; they did not launch that application
or install its driver. Fresh application validation is the release run above.
No physical keyboard, gameplay or visual acceptance claim is made. Keyboard
support and restrictions were not changed, so no support-status Sheet update was
needed.

The original incremental-build review exposed existing backend warnings
C4456/C4244 and the missing vendor ViGEmClient PDB warning LNK4099. They were left
unchanged at that stage. The later
[full-build follow-up](FULL_BUILD_PYTHON_2026-11-15.md) addresses the two compiler
warnings because the full builder's warning gate rejects them; vendor binaries
remain unchanged.

Before local synchronization, the clean branch was at `2bc4e5a`. A verified bundle
and SHA-256 manifest are retained under
`.local/backups/2026-11-15-actions-review/`. The current working tree is based on
the accepted merge. No new commit was created. No push, PR/comment change,
release publication or remote-ref modification was performed. These follow-up
changes remain local for Denis to review and publish manually.

Suggested follow-up title: `Fix Windows toolchain discovery and DPI-sensitive profile test`.
