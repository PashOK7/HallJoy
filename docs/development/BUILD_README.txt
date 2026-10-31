HALLJOY - WINDOWS X64 BUILD
==========================
Current workflow: 2026-10-31. Source version: src/HallJoyProject/HallJoy/version.h.
Run from HallJoy-main, not from the outer session folder.
Windows x64 is the supported target.

Requirements
------------
Visual Studio 2022 C++ x64 tools / MSBuild, LLVM clang++ for portable checks,
Git, PowerShell 5.1 or later and Python. Local Python 3.10.9 is the verified
existing environment; hosted Windows CI selects Python 3.12. Firmware research
has separate environment locks. Do not replace a working runtime or install
global packages merely to match an old guide.

Read-only entry: python tools/project_context.py --check

Ordinary source change - each once, with the test cache enabled:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_release.ps1
Never HALLJOY_NO_TEST_CACHE=1; do not repeat PASS without a relevant change.
research_reference_checks.py --record only follows a changed pinned source.

Full dependency rebuild, when needed:
  BUILD.cmd
Equivalent:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
The full entry already runs unified checks. Do not run both workflows just
to repeat validation.

Output and replacement
----------------------
  build\bin\Release\x64\HallJoy.exe

Candidates stage in build\obj\ReleaseCandidate\x64. The installed app stays open
through compilation and checks. The shared publisher closes only the exact target,
atomically replaces it and restores it if previously running. Failed/unchanged
candidates do not close it. Closing HallJoy is authorized; elevated handling is
in AGENTS.md. Do not create other delivery folders to avoid a locked EXE.
Preserve profiles and unknown files. See the build/replacement lifecycle in
  docs/current/BUILD_REPLACEMENT_LIFECYCLE_2026-09-19.md
  docs/current/BUILD_LIFECYCLE_2026-09-27.md

Validation
----------
Current scripts are authoritative. build_release.ps1 checks dependencies,
identities, notices and diagnostics; builds the complete ordinary native catalog;
runs linked-image self-tests, embedded-license checks and production-linked
profile/layout/startup recovery tests before replacement. Independent full-catalog
and Supported-layout gates are mandatory. These checks are not hardware tests,
driver installation or visual acceptance.
ViGEmBus 1.22.0 is embedded and verified; installation requires user action.
Private analog runtime is embedded; no global SDK install is needed.
Debug|x64 retains debugging with the compatible static release CRT.
The full build also updates package/license/checksum sidecars. Incremental EXE
replacement alone does not prove old sidecar hashes were refreshed.

Logging
-------
Ordinary builds support optional continuous logging (off by default), plus required
crash/missing-source/recognized-failure reports even when it is off. Limited or
unstable warnings do not themselves authorize forced logging. Tester delivery:
one ordinary EXE, one normal HallJoy.log through Open log.

Release and CI
--------------
Publication needs owner authorization. Write docs/releases/RELEASE_NOTES_v<version>.md:
  python tools/release.py <version> "v<version>: title" --publish
The command uses a fresh cleaned-main clone, publication/private-data checks,
one native suite/build, packaging and server-side asset hashes. Without --publish
it still prepares/builds local files, but does not push. Do not initialize Git
in this extracted workspace or push a stale mirror.
GitHub checks run on Windows for push/PR; the separate production build job is
manual workflow_dispatch. CI is the clean-checkout run; do not duplicate it locally.
See docs/development/TESTING.md and current OWNER_CONTEXT before delivery.
