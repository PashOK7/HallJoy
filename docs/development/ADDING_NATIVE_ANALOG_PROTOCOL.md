# Adding a native analogue keyboard protocol

HallJoy native protocol support is module-based. A protocol module owns discovery,
HID transport, parsing, reconnect and raw-to-milli normalization. The common core
owns curves, bindings, SOCD, UI snapshots, multi-device aggregation and ViGEm.

## Fast path

From the source-package root:

```powershell
python tools/new_native_backend.py `
  --slug foo_matrix `
  --prefix FooMatrix `
  --enum FooMatrix `
  --protocol-value 6 `
  --display-name "Foo Matrix protocol" `
  --start-phase AfterRealtime
```

The generator creates and registers:

- `foo_matrix_protocol.h/.cpp` with a pure parser/normalizer boundary;
- `foo_matrix_backend.h/.cpp` for HID discovery and transport;
- a portable protocol test skeleton discovered by the common test runner;
- a protocol documentation skeleton;
- the `NativeAnalogProtocol` enum entry;
- the one-line catalog entry in `native_analog_backends.def`;
- Visual Studio project/filter entries.

The registry derives the getter declaration directly from the catalog. A protocol
must not add an include, lifecycle call, raw read branch or UI branch elsewhere.

The generated module is inert: it sends no commands and claims no device until its
capability proof and worker are implemented.

After generation, run:

```text
python tools/run_native_backend_checks.py --require-compiler
```

The generated parser test is automatically discovered by filename convention.
Then run `BUILD.cmd` for the full Windows/MSVC and embedded-UAP build.

## The only integration contract

Export one `NativeAnalogBackendDescriptor`. The catalog automatically provides:

- lifecycle ordering relative to UAP, realtime and Raw Input;
- exact HID interface-path exclusion from Soup/UAP after capability proof;
- normalized multi-key reads through `NativeAnalogBackends_ReadMilli`;
- multi-device max aggregation;
- prevention of generic digital fallback for HID usages owned by the native source;
- event-driven curve/SOCD/ViGEm processing when the worker calls
  `RealtimeLoop_NotifyInputChanged()` or `RealtimeLoop_NotifyInputChangedAt()`;
- generic Configuration and Gamepad Tester telemetry;
- device-change notification and reverse-order shutdown.

No new protocol should add device-specific reads to `backend.cpp`, lifecycle calls
to `app.cpp`, or another special ViGEm path.

## Choosing a start phase

- `BeforeUap`: the backend must open/probe the device to prove the protocol before
  UAP starts. SparkLink and Sayo use this phase.
- `AfterRealtime`: startup classification is already complete and the worker only
  needs the common realtime dispatcher. Most polled protocols use this phase.
- `AfterRawInput`: use only when the protocol itself requires target-scoped Raw
  Input registration before activation. MAD68 A0 uses it for state validation.

## Capability-proof rules

A model name, vendor string, VID/PID, usage page or report length is never enough.
A claim is valid only after all relevant invariants are proven:

1. exact plausible HID fingerprint;
2. response framing/length/report ID;
3. checksum/CRC where present;
4. echoed command, offset, count or requested key IDs;
5. plausible analogue range and stable semantics;
6. read-only request or fully documented reversible state transition.

Call `NativeAnalogRouting_Claim(vid, pid, interfacePath, protocol)` only after proof,
using the exact SetupAPI path that was opened and validated. Before opening a path,
reject it if another protocol already owns that exact path. A failed or ambiguous
probe must leave that interface available to UAP; a sibling interface with the same
VID/PID must not be hidden by association.

## Worker requirements

- Publish normalized integer depth `0..1000` only.
- `OwnsHid(hid)` must be true only for mapped, authoritative analogue keys.
- On disconnect, clear owned values and wake realtime immediately.
- Do not allocate, log to files or perform UI work in the hot packet path.
- Do not interpolate, predict, synthesize depth or use digital key state as analogue.
- Bound HID I/O timeouts and make shutdown joinable.
- Keep protocol parser/builders separate from transport when practical.

## UI

Populate `NativeAnalogBackendTelemetry`. A new catalog entry automatically appears
in Configuration and Gamepad Tester. A custom detailed UI section is optional and
must not be required for basic support.

## Required tests

See `PROTOCOL_REVIEW_CHECKLIST.md` and `TESTING_NEW_PROTOCOL.md`. At minimum add:

- pure parser/builder tests with valid and malformed fixtures;
- capability-proof false-positive tests;
- exact interface ownership/UAP exclusion audit, including sibling paths with the
  same VID/PID and enumeration reorder/reconnect generations;
- release-to-zero and disconnect tests;
- scheduler/freshness tests for polled protocols;
- hardware evidence before changing status from experimental to tested.
