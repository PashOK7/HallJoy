# HallJoy testing

Run commands from the repository root. Current candidate evidence and pending
owner review are recorded in [release readiness](../current/RELEASE_1.6.0_READINESS_2026-09-20.md).
Choose checks appropriate to the change. Do not run speculative long-duration
soaks without a concrete failure hypothesis or user report.

## Full source / dependency validation

```powershell
python tools/run_native_backend_checks.py --require-compiler
BUILD.cmd
```

The full build repeats the automated gate, validates locked dependencies and
builds the private runtime and Windows x64 application. It delivers
`build/bin/Release/x64/HallJoy.exe`. For source-only inspection:

```powershell
python tools/run_native_backend_checks.py --static-only
```

This source-only check does not substitute for compiling changed code.

## Incremental ordinary candidate

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_release.ps1
```

This compiles and runs four linked-image checks (ATTACK SHARK, MINI60, NA87,
embedded ViGEm installer) before replacing the delivered executable. It does
not run every component suite or test hardware. The build leaves a running
HallJoy open until a changed candidate has passed; see the
[replacement lifecycle](../current/BUILD_REPLACEMENT_LIFECYCLE_2026-09-19.md).

## Targeted regression evidence

- [Input lifecycle](../current/INPUT_LIFECYCLE_REVIEW_2026-09-20.md): Pause and
  failed Resume stop the producer before resetting shared state.
- [Profile persistence](../current/PROFILE_PERSISTENCE_REVIEW_2026-09-20.md):
  isolated save/load, interrupted writes, concurrency and startup recovery.
- [Automatic layout](../current/AUTOMATIC_LAYOUT_REVIEW_2026-09-20.md): coherent
  snapshots, multiple devices, freshness, reconnect and manual-mode isolation.
- [Logging privacy](../current/LOGGING_PRIVACY_REVIEW_2026-09-20.md): support-log
  exclusions, burst handling, mandatory incident policy and ordinary crash memory.
- [Background work](../current/BACKGROUND_WORK_REVIEW_2026-09-20.md): UI snapshot
  reuse and regression checks. Short process samples are not leak proof.

Useful narrow checks:

```powershell
python src/HallJoyProject/tests/support_log_static_audit.py
python tools/generate_attackshark_family.py --check
```

The latter verifies existing generated profiles; it is not new firmware research.

## Hardware and visual limits

Software tests verify parsers, routing, ownership and failure handling. They do
not establish physical USB timing, every firmware revision or gameplay behavior.
Record actual tester evidence per model in the [hardware table](../../SUPPORTED_HARDWARE.md).
Experimental support remains explicitly labelled; untested devices do not acquire
confirmed status merely because a software suite passed.

The owner evaluates visual behavior. The agent performs code and automated checks,
not visual runs. ATTACK SHARK work is paused awaiting the tester; its reported
Forza/Block Bound Keys issue has no confirmed cause. Do not change functioning
blocking or scaling behavior speculatively.

Optional continuous logging is distinct from mandatory crash/missing-keyboard/
recognized-failure reports. Enable logging OFF must not suppress those reports.
See the [support guide](../SUPPORT_REPORT.md).

## Historical procedures

The [previous testing guide](../archive/TESTING_BEFORE_2026-09-20.md) preserves
older S21 cycle targets, soak procedures and v1.4 hardware gates. Those are not
current release requirements. Long checks require a specific reason; elapsed
duration alone is not proof of stability.
