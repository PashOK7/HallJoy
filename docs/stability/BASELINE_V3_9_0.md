# HallJoy v3.9.0 pre-stabilization baseline

Baseline recorded: July 30, 2026.

## Original package

- Archive: `HallJoy_UniversalAnalog_v3_9_0_EXTENSIBLE_FINAL_Source(1).zip`
- SHA-256: `1fad157abac5ad0d39b8be13095f2c274d235075eebb4baf50e957a3413ae8a1`

Stage 01 did not modify production code. The original package manifest is
stored in `baseline/ORIGINAL_CLEAN_PACKAGE_MANIFEST_V3_9_0.json`, and hashes of
all source files taken before the documentation was added are stored in
`baseline/ORIGINAL_SOURCE_SHA256SUMS.txt`.

## Reproducible baseline validation

Run from the project root:

```text
python tools/run_native_backend_checks.py --require-compiler
```

Result on July 30, 2026: PASS.

The baseline covered:

- eight Python static audits;
- `addressed_poll_scheduler_test`;
- `hid_io_operation_lifecycle_test`;
- `vigem_output_scheduler_test`;
- `native_analog_backend_contract_test`;
- `hex80_protocol_test`;
- `mad68pr_protocol_test`.

The complete output is in `baseline/PORTABLE_CHECKS_2026-07-30.txt`.

## What the baseline does not prove

The original local environment did not cover:

- an MSVC Windows Release x64 link;
- real HID, ViGEm, Raw Input, WinHTTP, WinTrust, or Job Object behavior;
- hardware hotplug, reconnect, or sleep/resume;
- Windows API fault injection;
- Application Verifier or PageHeap;
- long soak tests.

Each Windows-dependent package therefore has a separate mandatory Windows gate.
A correction can be considered implemented in code before that gate passes, but
not fully validated on the platform.

## Invariants

1. MAD68, Hex80, and Addressed packet formats and semantics do not change
   without separate protocol evidence.
2. Native/UAP arbitration remains fail-closed and occurs before UAP opens HID.
3. Digital HID input never becomes fabricated analogue depth.
4. Multi-device aggregation retains maximum-value semantics across genuine
   analogue sources.
5. Curves, SOCD behavior, UI telemetry, and ViGEm consume the same values for
   the same input.
6. No stage may report failed startup, shutdown, or persistence as success.
7. A timeout is not successful completion.
8. Performance is evaluated together with correctness and shutdown safety.
