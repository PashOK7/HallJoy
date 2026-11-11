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

Windows tool discovery (2026-11-11)
---------------------------------
BUILD.cmd and the internal PowerShell build stages use
tools/build_environment.ps1. Python must be a working x64 Python 3.10+.
Discovery checks python.exe on PATH (excluding Microsoft Store aliases), the
Windows py launcher and installed uv-managed interpreters. Python 3.12 is
preferred among uv installations; no interpreter is downloaded automatically.
An explicit -PythonPath takes precedence and invalid explicit paths fail closed.
Source builds discover x64 clang.exe AND clang++.exe through PATH, Visual
Studio's vswhere or Program Files/LLVM. Only the build process PATH is changed;
child checks and dependency builders inherit it. User/system PATH is untouched.

Read-only tool checks, including from a normal Windows terminal:
  BUILD.cmd -CheckOnly
BUILD checks installer prerequisites, including Visual Studio C++, Git and NSIS.
This check does not compile or create a package.
Use BUILD.cmd -PythonPath "C:\path with spaces\python.exe" for an explicit Python.
Double-clicking BUILD.cmd builds the full application and Windows setup package.
Its final results are build/HallJoy-setup.exe and build/HallJoy.exe.
This is the single public build command: compile the application once, then
package that exact EXE. No separate application/Windows CMD wrappers are needed.
Argument-driven invocations return immediately with the build-stage exit code.
The full build temporarily closes one verified local HallJoy application and its
known workers only for the physical-UAP check, then restores that same executable
even if the check fails. Supported locations: .local/portable/HallJoy.exe,
build/HallJoy.exe and
build/bin/Release/x64/HallJoy.exe in this checkout, in the current Windows session.
Other paths, multiple applications or unrecognized arguments stop the check
without closing anything; those instances must be closed manually. Device
isolation remains mandatory. The incremental release build keeps the application
open until its normal atomic replacement step.
If Clang is absent, add C++ Clang Compiler for Windows through Visual Studio
Installer. This is a build-machine prerequisite, not a HallJoy runtime dependency.
Regression check: powershell -NoProfile -File tools/test_build_environment.ps1
UAP lifecycle check: powershell -NoProfile -File tools/test_uap_runtime_session.ps1

Read-only entry: python tools/project_context.py --check

Ordinary source change - each once, with the test cache enabled:
  python tools/run_native_backend_checks.py --require-compiler
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_release.ps1
Never HALLJOY_NO_TEST_CACHE=1; do not repeat PASS without a relevant change.
research_reference_checks.py --record only follows a changed pinned source.

Full dependency rebuild and installer:
  BUILD.cmd
Package an already verified EXE without recompiling:
  BUILD.cmd -PackageOnly
Internal application stage (for development/diagnosis):
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1
The full entry already runs unified checks. Do not run both workflows just
to repeat validation.

Output and replacement
----------------------
  build\HallJoy-setup.exe
Application binary (starts HallJoy directly, not the installer):
  build\HallJoy.exe

Only these two final files are placed in build's root, after successful packaging
and hash verification. The stable names are replaced on subsequent successful
builds. Versioned packages, SHA-256 records and inventory remain under
build\packages\windows-<timestamp>-<id>; compiler output stays under build\bin
and build\obj. Internal application-only commands do not refresh the final pair.
Their internal EXE is build\bin\Release\x64\HallJoy.exe. Run BUILD.cmd -PackageOnly
after an internal application build to package it and refresh both final files.

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
