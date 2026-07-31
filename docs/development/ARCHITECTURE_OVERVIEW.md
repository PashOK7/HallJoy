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

The realtime loop follows the same lifecycle vocabulary. Its worker owns and
releases MMCSS, waitable-timer, and multimedia-period resources. Stop wakes the
address wait and joins for a bounded interval; only a confirmed join closes the
thread HANDLE. An incomplete join retains ownership and poisons restart. Final
application shutdown then avoids destroying backend state that a live
`Backend_Tick` could still access.

The diagnostic writer follows the same rule. Start and shutdown are serialized
around one generation. Shutdown first closes the producer gate, wakes the
writer and waits for its bounded drain. A confirmed join transfers HANDLE,
event, file and queue ownership back to the caller for cleanup. Timeout retains
those resources, poisons restart and requires process-level exit without CRT
destruction so no live writer can access destroyed storage.

The loopback overlay server also has a serialized generation. Stop closes the
listen socket to wake `accept` and shuts down the active client socket to wake
`recv`. The worker HANDLE and WSA ownership are released only after confirmed
completion. An incomplete join retains ownership, poisons restart and prevents
teardown of backend, settings, logging and other state that the worker could
still read. HTTP framing and connection concurrency remain separate concerns.

SparkLink has an inner worker generation because device hotplug can restart its
poller while the outer native-registry generation remains active. That outer
generation represents the hotplug service even when no device exists at initial
startup, guaranteeing final stop for a worker connected later. Start/stop is
serialized, but final stop acquires that lock with a bound so a synchronous HID
probe cannot make shutdown unbounded. Stop signals the event and cancels HID
I/O before joining. Only confirmed completion releases thread, HID and event
HANDLEs; join or lock timeout blocks both inner restart and outer registry reuse.

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
