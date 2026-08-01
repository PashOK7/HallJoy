# Native analogue backend contract

## Data boundary

A native backend provides, for each standard HID usage below 256:

- whether the protocol owns that key;
- the latest normalized depth in `[0,1000]`;
- connectivity and generic telemetry.

It does not apply HallJoy curves and does not construct XInput reports.

## Lifecycle boundary

`prepareRouting`
: Startup-only safe capability proof. It may claim an exact HID interface path but
  must not leave an unconfirmed device in a changed state.

`start`
: Opens only the previously validated interface and starts a bounded worker.

`stop`
: Joins the worker, restores any documented temporary state, closes handles, clears
  values and returns whether shutdown completed.

`notifyDeviceChange`
: Schedules reconnect/rediscovery; it must not block the window thread.

## Ownership boundary

Soup/UAP exclusion is an exact normalized HID interface-path fingerprint, published
before the child calls `CreateFileW`. The first protocol that completes a valid proof
owns that interface. A later backend may not steal it, while sibling interfaces with
the same VID/PID remain available unless independently proved and claimed. Protocol
catalog order is therefore classification priority.

Every native enumerator must reject a foreign exact-path claim after SetupAPI returns
the path and before any `CreateFileW`/metadata open. A claim must include the path that
was actually proved. VID/PID remains diagnostic metadata, not the ownership key.

## Realtime boundary

On every meaningful analogue change, call the common realtime wake. The common core
then applies cached curves, bindings and SOCD and uses the fixed-deadline ViGEm
scheduler. A protocol-specific output limiter or ViGEm call is forbidden.

## Telemetry boundary

Generic telemetry must remain safe to read from the UI thread: atomics/snapshots
only, no HID transactions. Device-specific detailed telemetry may coexist, but the
generic fields are mandatory so new modules work in the UI without edits.


## Catalog validity

HallJoy fails closed if a descriptor has an incompatible ABI/size, invalid phase,
unknown flags, missing mandatory callbacks, empty ID/name, duplicate stable ID or
duplicate `NativeAnalogProtocol` identity. The manifest generates both declarations
and entries, so there is no protocol-specific registry include list to maintain.

## Parser boundary

Packet builders/parsers should live in `<slug>_protocol.h/.cpp` and compile without
Windows or HID dependencies. This allows malformed fixtures and normalization to be
tested on Linux CI before hardware or MSVC is available. Transport code may only
publish values after the pure parser has accepted the complete response semantics.
