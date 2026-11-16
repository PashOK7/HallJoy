# Pull request 11 review (2026-11-15)

Follow-up: [locally corrected optional-installer adaptation and tests](PR11_OPTIONAL_INSTALLER_2026-11-15.md).
The findings below apply to the original PR head; GitHub was not modified.

PR: https://github.com/PashOK7/HallJoy/pull/11
Head reviewed: `2bc4e5a9d0e1ee16e45b7c34e10182839c44373e`.
Base: `323774e4544326b8aeb7560a0f173b1ff6fc68e5`.
All58 changed files were acquired from that immutable head and their Git blob
SHA1 values independently verified against the GitHub file records. Working
project code and the installed ordinary EXE were not changed by this review.

## Malicious-code review

No affirmative evidence of a malicious insertion was found in the reviewed head.
This is a scoped source/resource review, not a guarantee about every vulnerability
or a certification of a future executable. No antivirus scan was run: the owner
clarified that code review of the entire PR was wanted.

Coverage: all modified C++, vcxproj/build/static-audit changes, CMD wrappers,
PowerShell functions and call sites, NSIS installer/uninstaller, tests and test
here-strings, docs/translations and the five added image/vector resources.
All PowerShell files were parsed without executing them, and command ASTs and
network/dynamic-execution sites were inspected. No bidirectional Unicode control
characters were found. No new executable/DLL, submodule or symlink was introduced.

- Runtime C++ changes are a local-variable rename, a16-bit PID cast, and DPI-aware
  test-fixture sizing/error reporting. No new runtime network/process launch path.
- The new updater explicitly downloads only the official upstream GitHub stable
  asset, checks its SHA256 and constrains the download URL. No hidden download,
  user-data upload, credential harvesting, persistence/autostart, security-product
  disabling or arbitrary remote command evaluation was found.
- NSIS contains per-user HKCU uninstall registration, shortcuts, file copies and
  deletion of enumerated owned files. It has no new driver/service installation,
  autorun registration, network access or payload-execution command.
- Dynamic PowerShell calls target visible local build/check helpers or explicit
  installed tools. The new user32 Add-Type block is a wizard-test probe; it only
  locates the test process's dialogs/controls and advances wizard pages. Test
  stubs/mocks were inspected separately from production paths.
- PNG chunk boundaries/CRCs/IEND, ICO entry bounds/end, and BMP header were checked
  without rendering or executing assets. SVGs have no script, event-handler,
  foreignObject or external-resource nodes. The added Driving Engineer icon
  resources are currently unused leftovers; the installer stages the existing
  HallJoy icon.

## Findings

### P2: Incremental builds now require Clang unnecessarily

`tools/build_release.ps1:7` calls Initialize-HallJoyBuildEnvironment with
`-RequireClang` before consulting the already verified dependency/runtime cache.
An existing working MSVC + MinGW/g++ environment with cached UAP can run the
previous incremental build, but now fails immediately if Clang is absent. The
portable test runner explicitly accepts g++ as well as clang++. Require Clang
only when rebuilding the Clang-dependent runtime/full source tree, or accept
an available supported portable compiler for the incremental path. Static
control-flow finding; no toolchain was installed/removed to reproduce it.

### P2: Publishing the application and setup is not failure-atomic

`tools/publish_windows_outputs.ps1:35-40` replaces/restarts the application before
replacing the setup. The setup lock probe at31-33 closes its handle, so it cannot
exclude a subsequent lock. If a user starts the old installer during application
publication, setup replacement fails and there is no rollback of the application.
The stable pair then contains a new app and old setup despite the overall command
failing. Add rollback of the already-published output or equivalent coordinated
publication, and test a failure after the first replacement.

Reproduced using the exact PR publisher in a private file-only fixture. Its
application-publish helper was replaced with a reviewed stub that copies a text
fixture and acquires a setup read lock after preflight. No actual executable was
launched, no installed application was closed and no registry/installer change
was made. Result: Replace reports sharing violation; app remains new; setup
remains old. Evidence: `.local/reviews/pr11-20261115/proof-publish-pair/result.txt`.

## Scope and validation gaps

The PR description is stale: it says C++, the original build entry and release
workflow remain unchanged, but58-file head changes those surfaces. The actual
runtime C++ edits appear benign. Making NSIS mandatory for BUILD.cmd, adding two
build-root delivery files and replacing the established ordinary-path convention
are product/workflow changes rather than merely an optional installer. Recommend
keeping packaging optional and separating translation/build-environment/C++ fixes
from the installer contribution before accepting the whole patch.

As of the final live check the head is unchanged. GitHub Actions run
https://github.com/PashOK7/HallJoy/actions/runs/43109035351 is `action_required`;
there is no completed green check run for this PR head. It was not approved or
rerun by the agent. Author-reported installer/portable checks were not independently
rerun in full; no complete PR source build or installer execution was performed.

Local immutable inputs, AST/network index and resource structure results:
`.local/reviews/pr11-20261115/`. Only the file-only publisher failure reproduction
executed a PR production script, after its full source and local dependencies
were reviewed. Main application/configuration files were untouched.
