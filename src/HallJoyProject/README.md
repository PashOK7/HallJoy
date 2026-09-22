# HallJoy source tree

Windows x64 C++ application, tests and project tools. Current product/build
status is maintained in the [root README](../../README.md),
[hardware table](../../docs/SUPPORTED_HARDWARE.md) and
[documentation index](../../docs/README.md).

## Build

Run `BUILD.cmd` from the repository root for a full build. With dependencies
already prepared, use `tools/build_release.ps1` from that root for incremental
ordinary delivery. Output: `build/bin/Release/x64/HallJoy.exe`.
See the [canonical build guide](../../docs/development/BUILD_README.txt).
Compilation and candidate checks precede any closure of the delivered app.

## Runtime

HallJoy converts real analogue depth into up to four virtual Xbox controllers.
Digital key presses do not synthesize analogue travel. Native backends and the
bundled private analogue runtime supply depth; no separate SDK install is needed.
The embedded ViGEmBus installer runs only after user action.

Data defaults to `%LOCALAPPDATA%/HallJoy`. An explicit `HallJoy.portable` marker
beside the EXE selects writable portable storage. Built-in layouts are embedded;
user edits and custom layouts are separate saved presets.

## Verification and maintenance

`HallJoy/` contains application sources; `tests/` contains component/static checks.
Use the [testing guide](../../docs/development/TESTING.md) and latest review records
in the documentation index. The [release readiness record](../../docs/current/RELEASE_1.6.0_READINESS_2026-09-20.md)
separates software verification from hardware evidence and pending owner review.
Older audits are historical; do not treat their unresolved gates as current policy.
