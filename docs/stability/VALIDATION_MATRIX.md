# HallJoy stabilization validation matrix

Baseline date: July 30, 2026

## Purpose

This matrix separates checks available for every package from checks requiring
specific physical hardware. Missing hardware never permits a Verified claim,
but it does not block a narrowly bounded lifecycle package whose protocol and
normal hot path are proven unchanged.

## Validation levels

### V0: portable reproducibility

Required for every archive: complete native/static runner, GCC and Clang,
strict warnings for new tests, applicable ASan/UBSan, affected-area audits,
source/hot-path comparisons, manifest, and a repeated gate after clean unpack.

### V1: shared Windows/MSVC gate

Required before runtime acceptance: clean x64 Release build, HallJoy startup,
ViGEm smoke, normal restart/close, and no new warning, crash, hang, surviving
process, or obvious regression.

### V2: available SparkLink hardware gate

SparkLink/Irok was the continuously available physical gate. From S02V1 onward,
acceptance used a bounded trace bundle and deterministic analyzer rather than an
oral report. The scenario covered cold start, analogue and ordinary HID input,
ViGEm, every poll mode, row-limit changes, hotplug/reconnect, repeated lifecycle,
unplug under active input, and neutral release. A package changing SparkLink did
not advance without V1 and V2.

### V3: deferred device-owner gate

MAD68, Hex80, Addressed, and Sayo required owners of those devices. A package
could remain `Implemented / device gate deferred` only when protocol parser,
packet layout, HID mapping, command bytes, and polling body were unchanged; V0
passed; shared Windows plus available SparkLink regression passed; and the
deferred gate was recorded in both stage result and risk register.

## Pre-final external package

Device owners were to test the same immutable binary package and manifest. The
minimum scenario was clean start, ten connect/disconnect/reconnect cycles,
multi-key holds and releases, unplug under active input, close with the device
connected, restart after abnormal exit, at least 30 minutes of ordinary use, and
no stuck value, lost ordinary HID, crash, or hang. MAD68 additionally required
safe A8/A9 lifecycle and no unnecessary commands; Hex80 required full-matrix
publication and GET-only routing proof.

## Status rules

- `Implemented`: code and local gates complete; mandatory device evidence absent.
- `Verified on SparkLink`: V0, V1, and V2 passed; no implication for another
  protocol family.
- `Verified`: every gate required by the affected area passed.
- `Deferred`: work or evidence postponed; never a positive result.

## Recorded package status

- S02A/S02A.1: Windows build and shared runtime smoke passed.
- S02B.1: MAD68/Hex80 exception boundaries passed V0 and shared SparkLink
  regression; matching hardware remained deferred.
- S02B.2: SparkLink C++/SEH boundary passed V0, V1, and V2.
- S02B.3: Addressed boundaries passed V0 and shared Windows/SparkLink smoke;
  Addressed hardware remained deferred.
- S02V1: local trace infrastructure passed; machine-readable Windows/SparkLink
  evidence was the next acceptance gate.
- V14-12B/S06: Addressed ownership, cancellation, MSVC, simulator timeout,
  official build, and Irok regression passed; physical Addressed remained
  `Implemented / device gate deferred`.
- V14-12C/S07: Hex80 ownership/publication/containment, 40 audits, 26 tests,
  MSVC, simulator timeout, build, and Irok regression passed with protocol files
  unchanged; physical Hex80 remained deferred.
- V14-12D/S07: MAD68 persistent-read ownership, no post-stop A8, final A9,
  containment, 41 audits, 26 tests, MSVC, simulator, build, and Irok regression
  passed; physical MAD68 remained deferred.
- V14-12E/S21 pilot: 25/25 production `WM_CLOSE` cycles passed in 138-326 ms,
  all 11 user-state files remained unchanged, and no handle trend accumulated.
- V14-12F/S18: automatic privileged installation was structurally removed;
  pinned manual ViGEm 1.22.0 policy, 43 audits, 27 tests, MSVC/build, and post-
  build Irok 3/3 passed.
- V14-12G/S20: shipped documentation/build commands, unified Addressed audit,
  historical labeling, x64-only W4 configurations, 44 audits, 27 tests,
  zero-unexpected-warning build, and Irok 3/3 passed.
- V14-12H/S21: persistent lifecycle and soak automation passed a one-minute
  overlay pilot with stable handles, lower private bytes, 234,846/234,846 Spark
  queries, no trace error/process survivor, and immutable state. Post-build Irok
  3/3 passed.
- V14-12I/S21: 1000/1000 production cycles passed for executable SHA-256
  `6A2E82709F6FC6B652ECAEA657BA4FBD1544B0832934865779D9FF7F0306D97F`.
  Every cycle exited zero with no survivor/error/cap/state mutation; all trace
  hashes matched. Shutdown min/average/p50/p95/p99/max was
  `101/277.1/250/430/1315/2662 ms`; maximum handles 217. Spark totals were
  1,598,879 = 1,598,454 success plus 425 expected shutdown cancellations.
- V14-12K/S21: the attended run lasted 3,604.553 seconds with 703 resource
  samples, overlay 12/12, 92 ms shutdown, no survivor, and unchanged state. It
  exposed two false stale/reconnect events caused by unsigned age underflow
  across 14,138,221 queries. A saturating-age correction passed 46 audits,
  28 portable tests, official build, and a targeted two-minute production run
  with 464,905/464,905 successful queries and zero stale/reconnect.

The corrected V14-12K executable recorded SHA-256
`81609DC44D12F7DF44C2A7D801D8992CBFDCB45221F07AE041ED4711F4EB840C`.
The deterministic narrow correction did not invalidate the already completed
1000-cycle gate. External Aula and other device-owner requirements remained
separate until their own acceptance records.
