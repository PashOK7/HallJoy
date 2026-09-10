# UAP Provider V2 negotiated-capacity producer design

Date: 2026-08-23

Package: R2-B2h. Production route before and after this package:
`dense_compatibility`. Provider V2 configured-output equality is physically
qualified, but route promotion remains blocked by the fixed eight-owner source
window and the fixed monolithic IPC payload.

## Problem boundary

The Provider V2 ABI already supports a zero-capacity demand query followed by a
caller-sized snapshot request. The implementation does not honour that contract:
it pins at most `HallJoyPluginTelemetry::kMaxDevices` (eight) internally even
when the caller supplies exact larger buffers. It reports the original registry
demand, so truncation is honest, but the ninth owner can never become part of a
complete captured generation. Replacing `frames.qualified` with `frames.shadow`
would therefore promote a knowingly capacity-limited producer.

The current IPC also embeds eight dense/provider devices and 2,048 samples in
one bidirectional `SharedState`. R2-B2h does not pretend that changing only the
producer completes `HJ-V14-P1-020/021`; it establishes the variable producer
that the following split data-plane package requires.

## Compared implementations

1. Promote the already qualified V2 frame immediately. Rejected: field equality
   does not remove the hidden source capacity or the compatibility fallback.
2. Raise the constant from eight to 32/64. Rejected: this only moves the cliff,
   keeps caller capacity fictional and leaves the monolithic IPC unchanged.
3. Replace producer, IPC and production routing in one change. Rejected for this
   package: it combines owner pinning, mapping lifetime, access direction and
   output semantics before the variable producer has its own negative oracle.
4. Make the existing two-pass Provider V2 ABI real first, then split IPC and
   promote the route in rollback-separated packages. Selected.

## Selected producer contract

- A zero-capacity call obtains the exact current device/sample demand and still
  verifies that every captured owner has published a real first sample.
- Caller-sized storage controls copied output, not internal source visibility.
  The producer pins the whole registry generation or returns no snapshot.
- Reusable thread-local storage may grow only before the registry and per-device
  locks are acquired. No allocation occurs while either lock class is held.
- Ref-counted owner pins are released on every return/exception; reusable slots
  may retain capacity but never retain removed devices.
- A topology change between demand observation and pinning is retried a bounded
  number of times, then fails closed. Ownership generation is checked before and
  after the per-device lock set.
- Resource ceilings are defensive admission limits, not silent truncation. A
  demand outside the documented ceiling returns failure and remains observable
  in the header where possible.
- The same-generation dual export uses the caller's negotiated device/sample/
  dense capacities and continues comparing every captured ordinary HID cell.
- Existing eight-slot IPC and dense production output remain unchanged in
  R2-B2h. No digital correlation, runtime key learning or input fallback is
  introduced.

## Required gates

- portable owner-pin tests capture 12 owners completely, retain exact removal
  lifetime and prove caller storage is supplied outside the registry lock;
- production projection test builds an authoritative 12-device generation and
  retains the existing explicit 2-of-12 truncation oracle;
- static audit rejects fixed `PinOwners<kMaxDevices>` in the Provider V2 builder
  and requires allocation-before-lock plus RAII pin/lock release;
- the exact DLL runtime sizes both provider-only and dual buffers from the
  zero-capacity demand and reports `negotiated_capacity=1`;
- full UAP, static/portable, MSVC and ordinary production smoke gates pass;
- current output route remains statically dense compatibility.

## Next package

R2-B2i separates high-rate Provider V2 data from control/health state, negotiates
the data mapping from the producer demand, gives the parent a read-only view and
binds mapping lifetime to the isolated-host generation. Only after malformed,
resize, topology-change, restart and exact-EXE gates pass may R2-B2j select the
Provider V2 frame and remove the dense compatibility runtime fallback.

## Implementation checkpoint

R2-B2h is complete. The producer queries exact owner demand, grows reusable
storage before registry/device locks, pins the full owner generation into
caller-provided storage, takes a caller-provided reverse-release lock set and
clears every shared owner reference through a move-only lease. Four bounded
topology attempts and defensive ceilings of 4,096 devices / 1,048,576 samples
fail closed instead of publishing a partial authoritative generation.

All required gates above pass, including 12-owner pinning, complete 12-device
projection, exact private-DLL negotiated ABI sizing, official Release x64 build,
exact dual capture and ten-second startup/shutdown. Evidence is recorded in
`docs/stability/tests/V14_R2_B2H_UAP_NEGOTIATED_CAPACITY_2026-08-23.txt`.
No production route or fixed SharedState claim changed.
