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
        ↓ non-blocking complete newest-state batch
latest-value mailbox → dedicated ViGEm output worker → driver
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
common backend initialization completes
        ↓
Raw Input UI/diagnostic registration
        ↓
dependent-start transaction: realtime → AfterRealtime
        ↓
validate Raw Input prerequisite → AfterRawInput → publish ready
```

Catalog order is classification priority. A claim is first-proof-wins; a later
backend cannot steal the same exact VID/PID.

Dependent startup is transactional. Optional protocol families that are absent
are recorded as unavailable, while a present-device failure or rejected
lifecycle generation aborts the transaction. Rollback follows reverse
acquisition order (`AfterRawInput`, `AfterRealtime`, realtime, backend). It stops
at the first unconfirmed join and retains all lower-level ownership so a live
worker cannot observe destroyed state.

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

Realtime input notifications are durable process-lifetime sequence increments.
The worker acquire-observes pending input before every `WaitOnAddress` and only
marks the exact observed sequence consumed after its tick. A notification before
start, across restart, or during the observe/consume window therefore remains
pending. Address wakes are a latency optimization, not the correctness carrier.
Curve settings use the same publication principle: writers release-publish a
generation after atomic field updates and thread-local curve caches
acquire-observe the generation before rebuilding their snapshots.

ViGEm client and target creation is a bounded startup responsibility. Once the
output generation starts, only its dedicated worker may call runtime update,
reconnect or destruction APIs. Realtime uses a try-only mailbox operation and
never waits across driver I/O. Pending multi-pad masks merge, while all report
payloads are refreshed from the newest complete snapshot. Output shutdown has a
three-second join bound; incomplete completion retains its HANDLEs and driver
ownership, blocks dependent backend teardown and requires process containment.

The diagnostic writer follows the same rule. Start and shutdown are serialized
around one generation. Shutdown first closes the producer gate, wakes the
writer and waits for its bounded drain. A confirmed join transfers HANDLE,
event, file and queue ownership back to the caller for cleanup. Timeout retains
those resources, poisons restart and requires process-level exit without CRT
destruction so no live writer can access destroyed storage.

The loopback overlay server also has a serialized generation. Its accept owner
delegates sockets to a fixed table of 16 independent client workers. Stop closes
the listen socket to wake `accept`, shuts down every client to wake `recv` and
joins all client workers before releasing the accept HANDLE or WSA ownership.
An incomplete join retains reachable socket/thread ownership, poisons restart
and prevents teardown of state the workers could still read. Each connection
uses bounded incremental HTTP framing; one-shot telemetry and error responses
close immediately, while `/state` may retain keep-alive. Each server generation
also owns a 128-bit session cookie. Protected routes require it, browser origins
must exactly match the bound IPv4 loopback origin, and wildcard CORS is absent.

SparkLink has an inner worker generation because device hotplug can restart its
poller while the outer native-registry generation remains active. That outer
generation represents the hotplug service even when no device exists at initial
startup, guaranteeing final stop for a worker connected later. Start/stop is
serialized, but final stop acquires that lock with a bound so a synchronous HID
probe cannot make shutdown unbounded. Stop signals the event and cancels HID
I/O before joining. Only confirmed completion releases thread, HID and event
HANDLEs; join or lock timeout blocks both inner restart and outer registry reuse.
Final registry stop first closes a separate outer service gate. Worker start,
hotplug reconnect and connection publication all honor that gate, so a realtime
tick that overlaps application shutdown cannot create a new poller after the
registry has begun stopping SparkLink.

Sayo applies the same outer-service/inner-generation split to a reader group.
Every active reader observes one shared stop event, and shutdown issues
`CancelIoEx` for every HID handle before one bounded `WaitForMultipleObjects`
group join. This is one three-second deadline, not three seconds per reader.
The shared event, reader records, thread and HID HANDLEs remain owned until the
entire group completes; an incomplete group poisons inner and outer restart.
Published Sayo values are neutralized before cancellation and once more after
confirmed completion to cover a final in-flight reader publication.

Each Sayo reader also has two exception boundaries: the common allocation-free
C++ barrier and an outer Win32 SEH wrapper. C++/SEH faults retain fixed
per-reader diagnostics, neutralize publication and signal the shared group stop
event. Completion is published for normal and exceptional exits. Startup cannot
publish a group whose reader already exited or faulted, and loss of the final
live reader clears connected state without waiting for the hotplug watchdog.

The parent side of the isolated analog host owns its snapshot bridge,
supervisor, shared mapping, events and child job as one generation. Startup is
published only after both parent workers exist. Partial-start rollback joins an
already created worker before releasing IPC. Shutdown first requests a bounded
group join, then terminates the isolated child job and retries the parent join.
If either parent worker still survives, all reachable HANDLEs and IPC remain
owned, restart is permanently blocked for that process, and application
shutdown uses process-level containment instead of destroying dependent state.
The bridge and supervisor enter through allocation-free C++ barriers inside
Win32 SEH wrappers. Their fault paths neutralize the shared snapshot, signal
stop and permanently reject restart. The isolated child entry has the same
C++/SEH publication boundary. A child must be assigned to the owned job, and
its process HANDLE is closed only after confirmed completion; timeout retains
the HANDLE and forbids a replacement child.

Inside that child, private UAP exports use a common catch-all C ABI barrier and
scope-bound Soup mutex guards. The initialized state is generation-truthful and
pointer/length arguments are validated before access. Unload snapshots workers
under the devices mutex but releases it before cancel or bounded join. A join
timeout retains plugin ownership and poisons restart; the child host then exits
the disposable process without calling `FreeLibrary`, leaving parent/job
containment to confirm process completion before any replacement child starts.

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
