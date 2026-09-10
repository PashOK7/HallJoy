# UAP Provider V2 snapshot design

Date: 2026-08-22

Package: R2-B2. Current status: read-only adapter/projection, one-generation
dual capture, one parent tick capture and live configured-output shadow are
implemented and proven locally through the exact production image. Qualified
publication still consumes the dense compatibility result; observed physical
zero-mismatch evidence remains before any provider route switch.

## New evidence found while preparing the adapter

The planned adapter could not truthfully wrap the existing UAP dense snapshot.
The current private UAP ABI stores `float values[256]`. `Device::update_from_keyboard`
maps Soup keys to the historical Wooting number and explicitly skips every
result above 255. The isolated host independently rejects sparse codes above
255 as well. Therefore UAP Fn (`0x409`), DrunkDeer OEM1/Menu/Fn2 (`0x403`) and
consumer/media codes (`0x3xx`) are lost before an adapter in HallJoy can see
them.

This is not a reason to correlate analog movement with a later Windows digital
keydown. Such learning remains forbidden: it would delay shallow analog travel
until the digital actuation threshold and would fail for Fn/OEM controls that
do not publish an ordinary Windows key.

## Compared implementations

### A. Enlarge the old numeric dense array

Increasing 256 to 2048 would retain a single ambiguous number space, fixed
capacity and fake indexing for consumer/OEM controls. It would also make the
new architecture depend on the compatibility encoding it is intended to
remove. Rejected.

### B. Keep the 256 table and append an extended-key side list

This can recover Fn with a small patch, but creates two authoritative value
planes and makes release, ownership and per-device merge depend on which side
list a key happened to use. It also preserves the collision-prone old codes.
Rejected.

### C. Re-read or infer special keys in the HallJoy parent

Re-reading would create a second hardware acquisition per tick. Digital
correlation would add activation-depth latency and cannot identify non-Windows
keys. Both violate the one-acquisition and no-digital-fallback invariants.
Rejected.

### D. Add one versioned private UAP snapshot and retain the old ABI only as a
compatibility projection

The UAP worker keeps values by Soup key, translates each key once to the shared
`KeyIdentityV1`, and exports one immutable per-device `AnalogProviderV2`
snapshot. Ordinary USB keyboard usages use page `0x07`; media keys use their
real consumer page `0x0C`; Fn/OEM controls use `UapExtended`. The existing
256-key ABI remains temporarily available and is derived from the same worker
sample. Selected.

## Required semantics

1. The worker performs one keyboard acquisition and publishes the legacy and
   V2 views from that same result.
2. Fn, OEM/Menu and media values are retained before any 256-key projection.
3. A V2 sample is published for every key identity the UAP data plane can
   represent, including an explicit zero. `Owned` means the provider owns that
   identity/value cell for the device; it is not proof that a particular
   physical model has the printed key.
4. Physical-layout truth remains separate. Until a family supplies an exact
   capability map, `LayoutProven` stays clear and UI must not claim that the
   superset is the physical keyboard layout.
5. Device identity, duplicate-safety, VID/PID and usage metadata are copied
   without inventing an interface or layout proof.
6. Device and sample required counts are reported before truncation. A short
   buffer cannot be marked complete.
7. Parent/child transfer is copied under the existing snapshot publication
   transaction. The parent exposes a read-only capture API; `Backend_Tick`
   continues using the old path in this package.
8. No realtime allocation, device I/O, wait, string lookup or digital-key
   correlation is introduced.

## Generation rules

- provider generation is bound by the parent to the isolated-host launch
  nonce, so a restarted child cannot continue an old generation;
- sample generation advances for each accepted worker publication;
- value generation advances only when a retained value changes;
- ownership generation advances for provider topology changes;
- timestamps are monotonic source timestamps, not wall-clock labels.

The first implementation may still transfer through the existing fixed V10
mapping, but its capacity must be explicit and large enough for every currently
representable UAP device/key pair. The later IPC-V2 package replaces that
transport; it must not change the semantic snapshot.

## Gates before any route switch

- portable identity mapping covers USB keyboard, USB Menu/Context, consumer
  media, Fn and every current OEM code;
- a complete zero after a nonzero value remains present and authoritative;
- legacy ordinary-HID projection is exactly equivalent for deterministic
  vectors, while the V2 view additionally retains special keys;
- two equal keyboards preserve separate device/sample ownership;
- short device/sample buffers report required capacity and never claim
  completeness;
- malformed, duplicate or generation-regressed snapshots fail closed;
- the official UAP build, native suite and MSVC Release x64 build pass;
- static gates prove `Backend_Tick` was not switched by R2-B2.

## Rollback boundary

R2-B2 may add the private UAP export, shared-memory copy, read-only parent
capture and tests. It may not remove the old ABI or select V2 for controller
output. If any comparison fails, the whole package can be removed without
changing the currently qualified provider route.

## 2026-08-22 implementation checkpoint

The private ABI1 worker now retains the complete Soup-key value array before
the old 256-code projection and exports the V2 device/sample view. The isolated
host copies it inside the same shared publication transaction and the parent
offers a validated read-only capture. `Backend_Tick` does not call that capture.

The production-linked projection gate covers two devices, USB Menu, Consumer
media, UAP Fn, an explicit all-zero release, duplicate/NaN rejection and exact
ordinary-page legacy projection. It also found and closed a real capacity bug:
the pinned owner window used to forget the registry's original device count.
The snapshot now retains `required_count`, reports the effective captured
capacity rather than the caller's potentially larger buffer, and can never
label internal 8-device truncation complete.

The exact newly built ABI1 DLL passed the V2 export at runtime with one locally
connected device and 127 samples. The gate validates structure sizes,
generations, namespaces, unique device/sample identities, finite values,
completeness/capacity and inactive-state behavior before initialise and after
bounded unload.

## 2026-08-22 same-generation dual-capture checkpoint

Three implementations were compared for the next boundary. Retrying two
independent exports until their counters happened to match still permits ABA
and does not prove one pinned source. Replacing the legacy route immediately
with a V2-derived dense table removes the hardware-safe rollback oracle before
the configured report is characterized. The selected implementation pins the
device owners once, takes their locks once and produces both the V2 snapshot and
the ordinary-HID dense compatibility view inside one private UAP call.

The private ABI gate reconstructs all 256 dense values independently from the
returned namespaced V2 samples and compares every value for every captured
device. This first failed on a real condition: a newly discovered device exposed
topology while `snapshot_generation` and `snapshot_timestamp_us` were still
zero, allowing constructor data to be labelled fresh. Dual capture now rejects
that state and waits for the first real hardware acquisition.

The isolated host resolves the dual export as mandatory, validates V2 and dense
metadata plus complete ordinary-HID equality, and publishes both views under
one IPC V12 transaction with an explicit coherence bit. The parent rejects V2
capture unless that bit is present. If a future plugin violates this contract,
V2 fails closed while the child deliberately falls back to the existing legacy
export; current production input therefore remains available and unchanged.

The exact production `HallJoy.exe` now has a hidden bounded self-test that starts
the actual isolated child and accepts success only after the parent captures a
coherent, authoritative, non-empty V2 snapshot. The final image passed with SHA-
256 `CE26D65EFBF0DBB354B36AF8FC6BDD13B7C29F369775957545003462C18987F2`.
Ordinary startup/WM_CLOSE also passed without a continuous log, crash report or
new exact-image survivor.

This proves the acquisition and transport generation boundary, but not final
configured XUSB equivalence. The next rollback-separated package must run the
actual bindings, curves and complete report builder in read-only shadow and
compare every XUSB field without submitting the shadow result. Provider V2
remains unselected for configured mapping until that test passes.

## 2026-08-22 parent tick-capture checkpoint

The next audit found a deeper parent-side coherence gap. Ordinary production
did not use its diagnostic full-buffer preference: each bound UAP key could call
the legacy read API separately. Although every call read valid shared memory,
the analog host could publish between calls, so one controller report could mix
keys from adjacent generations.

Three responses were compared. Reading dense and V2 separately and comparing
counters retains an ABA window. Capturing V2 only while leaving qualified keys
on per-key calls produces an invalid comparison oracle. Replacing qualified
output with V2 immediately removes the rollback route before configured-output
proof. The selected parent API copies publication metadata, aggregate dense,
per-device dense and optional V2 under one `snapshotSequence` transaction.

The qualified UAP path now consumes only that captured dense compatibility
plane for the complete tick. It does not consume V2 identities or samples. If a
stable parent capture is unavailable, the old analogue per-key read remains an
availability fallback; no digital event is used to correlate, learn or trigger
an analogue key. A missing or invalid V2 plane disables only future shadowing
and does not discard a valid dense capture.

One shared validator is used before child publication and again after parent
copy. It checks every per-device ordinary-HID projection, aggregate max merge,
active counts, finite range and publication identity. Portable negative tests
corrupt the aggregate, a device plane and V2 equality independently. The exact
production EXE proves a non-empty coherent capture through the real child and
ordinary startup/shutdown remains clean. Live configured V2 shadowing is still
the next separate package.

## 2026-08-23 live configured Provider V2 shadow checkpoint

The parent now projects the V2 plane from the same immutable tick snapshot into
a bindable raw map with ownership independent of value. USB keyboard-page and
supported UAP extended identities are mapped explicitly; consumer and semantic
namespaces are ignored rather than aliased. Duplicate identities from multiple
devices merge by maximum value. The qualified compatibility view remains the
production oracle.

Qualified and shadow routes reuse the same native-key cache, curve definition
and generation, bindings/settings capture and mouse sample. They invoke one
configured neutral-frame builder with separate conflict state. Digital fallback
cannot contribute to, train or trigger the shadow and instead disqualifies the
tick. Invalid/unavailable V2 and a concurrent curve mutation likewise skip the
comparison and resynchronize shadow history to qualified history.

Every eligible frame compares the full button mask, two triggers and four stick
axes. Bounded atomic telemetry records equality, per-field mismatches and skip
causes without creating continuous logs. The XUSB adapter is called only for
the qualified frame, so shadow output cannot be submitted to ViGEm.

The new production-linked projection tests, full static/portable suite,
official Release x64 build, exact real-child capture and ordinary startup/close
smoke pass. The resulting local artifact is not promoted. These checks prove
the shadow implementation but do not manufacture physical equality evidence;
counter observation on representative UAP hardware and a separate promotion
gate remain mandatory.

## 2026-08-23 negotiated-capacity producer checkpoint

Physical configured equality did not erase a deeper capacity defect: the UAP
registry is dynamic, but the Provider V2 builder still pinned at most eight
owners. R2-B2h replaces that internal fixed window with a genuine demand/exact-
capacity capture. Reusable pin, projection and lock storage grows before locks;
one complete ownership generation is pinned; all device locks unwind in reverse
order and all retained owner references are cleared on every exit. Topology
movement receives bounded retry and then fails closed.

Portable tests now capture 12 owners and a complete authoritative 12-device
generation while preserving the explicit 2-of-12 truncation rejection oracle.
The exact private DLL sizes its ABI buffers from the zero-capacity demand and
reports `negotiated_capacity=1`. Full static/portable checks, official MSVC
Release build, exact dual capture and ordinary lifecycle smoke pass.

This closes the producer-side part of the capacity risk only. The live
analog-host `SharedState` remains an eight-slot monolith and production remains
on the qualified dense route. The next package is a separately owned,
capacity-negotiated, parent-read-only Provider V2 data plane; route selection
and dense-fallback removal remain a later package.
