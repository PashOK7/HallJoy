HALLJOY - WINDOWS X64 BUILD
==========================
Current contract: 2026-09-20, candidate version 1.6.0.

Requirements
------------
Visual Studio 2022 Build Tools with Desktop development with C++ and x64 Clang,
Python 3.12, Git, and PowerShell 5.1 or later. Windows x64 is the supported target.

Full build from repository root:
  BUILD.cmd
Equivalent:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1

After dependencies have been prepared, incremental ordinary delivery:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_release.ps1

Output and replacement
----------------------
  build\bin\Release\x64\HallJoy.exe

Both entry points stage the candidate in build\obj\ReleaseCandidate\x64.
The running delivered application remains open during compilation and candidate
checks. Only successful changed candidates reach the shared replacement script:
it closes the exact target, replaces it and reopens it if previously running.
Failed or unchanged candidates do not close HallJoy. Do not use another delivery
folder to work around a locked EXE. See docs/current/BUILD_REPLACEMENT_LIFECYCLE_2026-09-19.md.

The full build also writes dependency/license/checksum files beside the delivered
EXE. Local runtime settings and unknown files are preserved. Incremental delivery
replaces the EXE; do not assume old sidecar checksums were refreshed by that path.

Validation
----------
The full build verifies locked dependencies, static audits and portable checks.
Incremental delivery builds ordinary Release and runs four linked-image checks:
ATTACK SHARK, MINI60, NA87 and the embedded ViGEm installer resource. These tests
are not physical-device tests and do not install a driver.

ViGEmBus 1.22.0 is embedded and verified; installation requires user action.
The private analogue plugin runtime is bundled; no global SDK install is needed.
Debug|x64 retains symbols/checks with the compatible static release CRT required
by ViGEmClient. See docs/development/TESTING.md for individual test commands.

Logging
-------
Ordinary builds support optional continuous logging (off by default) and mandatory
crash/missing-keyboard/recognized-failure reports even when that option is off.
Dedicated diagnostic builds may force additional telemetry; do not distribute one
as an ordinary release. Raw stack/register memory is excluded from ordinary crash
reports. Detailed diagnostic crash dumps have a different privacy scope.

Current gates and evidence
--------------------------
docs/current/RELEASE_1.6.0_READINESS_2026-09-20.md records the candidate and limits.
SUPPORTED_HARDWARE.md is the current compatibility list. Older v1.4 documents are
historical, not additional hardware or long-duration gates for this release.
The owner checks the UI and approves the candidate before GitHub publication.
