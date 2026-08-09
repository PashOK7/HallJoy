# S06: Addressed overlapped-I/O ownership

- Date: August 1, 2026
- Package: V14-12B
- Status: `Implemented / Addressed hardware gate deferred`

## Corrected risk

The old shutdown path could call `CancelIoEx`, wait briefly, and then close the
HID handle from the owner thread while the reader's stack buffer and
`OVERLAPPED` structure were still live. `CancelIoEx` requests cancellation but
does not prove terminal completion, so the forced close created uncertain
behavior and a possible use-after-lifetime condition.

The HID handle is now closed only by reader-side RAII after `CancelAndDrain`.
If the driver stalls, the reader and the complete session stack remain alive in
the main worker. External shutdown waits no more than 3000 ms for the native
main-worker handle. On timeout it retains all generation resources, returns
`TimedOut`, poisons the registry, and stops dependent application teardown.

## Invariants

- the owner may call `CancelIoEx` but may not close the reader's HID handle;
- the buffer, event, and `OVERLAPPED` outlive terminal completion;
- a late read after stop cannot publish non-neutral analogue state;
- threads, events, and claims are released only after confirmed worker exit;
- timeout forbids restart and selects immediate process containment;
- packet builders, command bytes, and polling cadence are unchanged.

## Validation

- targeted ownership/containment static audit: PASS;
- complete gate: 39 static audits and 26 portable C++ tests, PASS;
- Addressed translation unit under MSVC 19.44 `/W4 /WX`: PASS;
- simulator-only Addressed stop timeout: PASS, expected exit code 2;
- official `BUILD.cmd`: PASS, zero errors, allowlisted `LNK4099` only;
- `MakePacket`, `MakePollPacket`, and the session polling core: token-identical;
- 15-second Irok MG75 Max production smoke: 57,161 successful routes,
  cancellation during shutdown, clean exit 0, zero trace errors;
- all 11 user files remained unchanged.

No physical Addressed device was available. The correction is accepted as
implementation-tested, but hardware-verified status remains deferred to an
external device-owner gate.
