# Native protocol architecture overview

## One-way data flow

```text
HID/vendor transport owned by protocol module
        ↓ validated raw sample
protocol parser + raw-to-milli normalization
        ↓ 0..1000 and authoritative HID ownership
NativeAnalogBackendDescriptor catalog
        ↓ max aggregation across real analogue devices
HallJoy curve cache → bindings → SOCD
        ↓ changed XInput report
common fixed-deadline ViGEm scheduler
```

A protocol module never calls ViGEm, applies user curves, reads digital key state as
analogue, or edits the keyboard UI.

## Startup and ownership flow

```text
NativeAnalogBackends_Reset
        ↓
PrepareRouting for every catalog entry
        ↓ exact VID:PID claims only after capability proof
BeforeUap start phase
        ↓
UAP receives complete native exclusion set before Soup CreateFileW
        ↓
common backend/realtime starts
        ↓
AfterRealtime phase
        ↓
Raw Input UI/diagnostic registration
        ↓
AfterRawInput phase
```

Catalog order is classification priority. A claim is first-proof-wins; a later
backend cannot steal the same exact VID/PID.

The native registry owns a monotonic generation for every catalog entry.
Lifecycle mutations are serialized and bound to the first owner thread. A
backend stop callback receives that generation and returns `StopResult`; only a
confirmed `Joined` result permits replacement. Timeout, fault, wrong-thread, or
generation mismatch remains visible through `NativeAnalogBackends_GetLifecycle`
and blocks unsafe restart instead of being cleared by reset.

## Files a new protocol owns

Generated default:

```text
HallJoy/<slug>_protocol.h/.cpp   pure packet builders/parsers and normalization
HallJoy/<slug>_backend.h/.cpp    enumeration, safe proof, HID worker, snapshots
Tests/<slug>_protocol_test.cpp   portable parser fixtures
Docs/protocols/<SLUG>_PROTOCOL.md evidence and safety contract
```

Central registration consists of:

1. one unique `NativeAnalogProtocol` value;
2. one line in `native_analog_backends.def`.

`tools/new_native_backend.py` performs both edits and adds the Visual Studio project
entries. The registry derives getter declarations directly from the manifest, so no
protocol-specific include or lifecycle edit is required.

## Files a new protocol must not edit

Normal protocol additions must not add device-specific code to:

- `app.cpp`;
- `realtime_loop.cpp`;
- the common read path in `backend.cpp`;
- `keyboard_subpages.cpp` or `keyboard_render.cpp`;
- `vigem_output_scheduler.h`;
- UAP/Soup exclusion code.

A requested edit to one of these files indicates either a missing generic contract
feature or an attempted protocol-specific shortcut. Extend the contract first.

## Descriptor responsibilities

The descriptor declares:

- stable ASCII ID and display name;
- protocol identity;
- startup phase;
- transport/safety flags;
- capability routing callback;
- lifecycle callbacks;
- authoritative HID ownership and normalized reads;
- lock-free/snapshot UI telemetry.

At startup HallJoy validates every descriptor and rejects duplicate IDs, duplicate
protocol identities, unknown flags, invalid phases or missing mandatory callbacks.

## Choosing an implementation reference

- Asynchronous stream plus reversible control: `mad68pr_backend.cpp`.
- Full matrix polling: `hex80_backend.cpp`.
- Priority/addressed polling: `addressed_analog_backend.cpp`.
- Backend-integrated row/depth polling: `backend_sparklink.inc` and `backend_sayo.inc`.

New code should generally use standalone `<slug>_backend.cpp` rather than adding
another `.inc` implementation to `backend.cpp`.
