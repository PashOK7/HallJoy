# PR11: optional installer and retained portable delivery

Owner decision, 2026-11-15: fix the review findings; an installer must be entirely
optional and HallJoy must remain portable. This is a local adaptation of
[PR11](https://github.com/PashOK7/HallJoy/pull/11), reviewed head
`2bc4e5a9d0e1ee16e45b7c34e10182839c44373e` by d9911.
The [original review](PULL_REQUEST_11_REVIEW_2026-11-15.md) describes the original
head. The initial adaptation and tests below predate publication of the source
changes. See the merge record at the end of this document.

## Corrections

- Incremental `build_release.ps1` discovers Python without RequireClang. Existing
  MSVC builds with g++ for portable checks and cached UAP remain supported. The
  compiler-absent regression checks this independently from the installed tools.
- `BUILD.cmd` is byte-identical: SHA256
  `abf1b0e36d97cdf435c428ebd1127bbfb2d80b70431ab762816ae27cd988a432`.
  It still builds `build/bin/Release/x64/HallJoy.exe`, without NSIS/setup.
- `build_windows_installer.ps1` is a separate package-only entry point. It reads
  the existing ordinary EXE, verifies embedded runtime/legal inputs and compiles
  into a new directory under `build/packages`. It never rebuilds, republishes,
  closes or launches the ordinary app. Nothing downloads or installs NSIS.
- The PR's two-output publisher is deliberately not integrated. There is no
  second build-root HallJoy.exe/setup pair that can become inconsistent. A failed
  compiler or an occupied output does not change the ordinary EXE or old setup.
  This replaces the original failure-prone paired-publication design.
- Portable Start/Update helpers are optional; no helper is required to run EXE.
  Update accepts the official stable upstream asset plus SHA256, refuses foreign
  folders/running copies and preserves user files. Network cases are mocked.
- English guides and nine translated quickstarts are included. Russian build
  instructions were corrected to ordinary EXE-only builds; NSIS badges removed.
- PR runtime C++, project-file, full-build and DPI-test edits were not applied;
  existing keyboard/MIDI work is retained. Unused alternate branding resources
  and visual wizard automation were not added. Existing icon is used.

See [commands, requirements and installer behavior](../development/WINDOWS_INSTALLER.md).
The durable portable/optional rule is recorded in AGENTS.md and OWNER_CONTEXT.

## Verification

- PowerShell AST parse: ten changed/new scripts PASS.
- Build environment: explicit Python, invalid Python, Store alias, compiler-absent
  incremental setup and explicit compiler requirement PASS.
- Portable helper tests:25 PASS, no real release download or application launch.
- Optional packaging:20 PASS, including real NSIS compilation, exact payload
  hashes, missing EXE, incorrect embedded runtime, occupied output, compiler
  failure after preflight, repeated packages and unchanged ordinary EXE.
- Real installer/uninstaller behavior:44 PASS using isolated TEST_ROOT paths,
  test-only HKCU keys and shortcuts. Installation/reinstallation/locked EXE,
  foreign-folder/shortcut preservation, per-user registration and removal tested.
  Settings/profiles survive reinstall and uninstall. App/driver not launched.
- Native suite PASS (including the optional-installer policy audit). Ordinary
  release wrapper PASS:10-family exact-image gate, legal resources, production
  profile transactions, layout aliases,92 Supported models and18 startup-recovery
  scenarios plus repeated startup. Candidate equals installed EXE; app untouched.
- Compiler discovery also passes under Windows PowerShell5.1 (CI host). Local
  links in15 guides PASS; project context PASS within the startup byte budget.
- Owner visual review and physical keyboard testing were not performed; no
  runtime/keyboard-support change is claimed.

Evidence under `.local/reviews/pr11-20261115/`; fixture results under `build/tests/`.
Backups: `.local/backups/pr11-optional-portable-20261115/`. Restore backed-up bytes
and remove only adaptation-added files from the integration manifest to roll back;
keep all prior keyboard work and ordinary EXE. Snapshot comparisons guard writes.
The pre-existing OWNER_CONTEXT exceeded its20KiB startup budget; stale baseline
and duplicate historical summaries were shortened, with detailed linked records
and exact backup bytes retained. The decision to use1.6.15 supersedes its stale
1.6.14 baseline paragraph.

## Local optional artifact

`build/packages/windows-20261115-095642-c8d64232/HallJoy-1.6.15-Windows-x64-setup.exe`
(5,517,851 bytes), SHA256
`d944196116d7e5043074186333f3479587f1d5de138d5fd132f442547962d80e`.
Its seven-file JSON inventory and SHA256 sidecar are adjacent. Embedded application
is the unchanged ordinary1.6.15.0 EXE (11,879,936 bytes), SHA256
`6a12acddb9c57adbfc4c996c44be3e9390923294d771b5f8f1c4e1a22adfdb17`.
This unsigned setup is local only; existing releases/assets were not modified.

NSIS3.13 was downloaded using the official Microsoft winget manifest's upstream
SourceForge URL, verified as SHA256
`db9a1038bbc0e53fef24f423fd609979bd578e942f1be1198db186d1c42652a3`,
and extracted locally with existing7-Zip. Its installer was not run; no persistent
system tool/environment change. Acquisition manifest and tools remain under
`.local/tooling/nsis-3.13/`, outside publication inputs.

## GitHub merge, 2026-11-15

The maintainer added `ab97588` as a fast-forward update to the PR branch.
The final diff changes 38 files relative to the previous main. It contains
the installer, portable helpers, guides and checks. BUILD.cmd, the full
application build and C++ sources match the previous main. The optional
uninstall cleanup follows [the later owner decision](WINDOWS_UNINSTALL_DATA_2026-11-15.md).
The staged publication policy, whitespace, local links and private-data scan
passed. The clean clone packaged the verified official 1.6.15 EXE and passed
129 isolated install/uninstall assertions. The package was a local fixture.
[PR CI](https://github.com/PashOK7/HallJoy/actions/runs/43692130366) passed.

GitHub merged [PR #11](https://github.com/PashOK7/HallJoy/pull/11) with a merge
commit and preserved d9911's three commits. Verified main tree identity with
the tested head. Merge commit `35ba83ac1f42fe849bd896721503a880a8cf4c61`.
The local release baseline now points to the merged main and the released
version remains 1.6.15. The local workflow and test formatting were synchronized.
Reviewed local overlays in AGENTS.md, README.md, docs/README.md,
WINDOWS_INSTALLER.md and build_release.ps1 preserve in-progress work and notes.
Backup hashes are under `.local/backups/pr11-merged-baseline-20261115/`.

