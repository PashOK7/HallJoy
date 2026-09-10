# AnalogProviderV2 foundation design

Date: 2026-08-22

Package: R2-A/B1. Risks: `HJ-V14-P0-001`, `HJ-V14-P1-011`,
`HJ-V14-P1-016`, `HJ-V14-P1-017`, `HJ-V14-P1-020`, and foundation work for
`HJ-V14-P1-021..023`.

Status before implementation: design selected; production routing unchanged.

## Problem and evidence boundary

The current common path uses one unversioned `uint16_t` number for several
different concepts:

- USB HID keyboard usages below 256;
- Soup/UAP extended key codes such as `0x403` and `0x409`;
- HallJoy mouse pseudo-codes `240..246`, which occupy the same numeric space as
  possible keyboard-page usages;
- native provider values quantized to integer milli before curves and XUSB.

The UAP host already publishes coherent float snapshots, but the core turns
them back into per-key compatibility calls. Native providers use a different
`getMilli` ABI. Capacity, completeness, source ownership, freshness and value
generations are not one common contract.

This package does not claim a physical defect or change a protocol parser. It
establishes a production-compiled, portable contract and negative tests before
any route is migrated.

## Required invariants

1. A key identity always carries a schema version and namespace.
2. USB keyboard, USB consumer/media, UAP extended and HallJoy semantic inputs
   cannot collide merely because their numeric usage/code is equal.
3. Provider snapshots preserve normalized float plus optional exact raw
   numerator/domain until the curve/output boundary.
4. Every snapshot names provider, sample, value and ownership generations and
   their timestamps independently.
5. Full authoritative release-to-zero is representable; a changed-only list is
   never the source of truth.
6. Device/sample capacity is explicit. Truncation cannot masquerade as a
   complete authoritative snapshot.
7. Duplicate `(device, key identity)` samples are rejected; the same key from
   two distinct devices is allowed for a later explicit merge policy.
8. This package adds no binary-keydown identity learning and no realtime wait,
   allocation, I/O or compatibility fallback.

## Compared approaches

### Option A — keep one integer and enlarge arrays

This has the smallest diff, but preserves the root collision between usage
pages, UAP keys and synthetic inputs. It also keeps fixed-capacity bitmasks and
does nothing for precision, ownership or IPC generations. Rejected.

### Option B — big-bang replacement of every provider, binding, UI and profile

This can reach the right final shape, but changes all protocol routes and all
persistence surfaces before there is a characterization oracle or broad
hardware matrix. Failure attribution and rollback would be unacceptably wide.
Rejected.

### Option C — versioned contract, verified adapters, then route-by-route removal

Introduce a strict common identity/value/snapshot contract beside the current
route. First prove type, validation, capacity and precision invariants. Then add
read-only UAP and native adapters, prove old/new XUSB equivalence on existing
vectors, switch one provider family at a time, and delete each legacy surface
only after its production-linked gate passes. Selected.

### Option D — strings or UUIDs as realtime key identity

Strings make persistence readable but add parsing, allocation and comparison
cost to the high-rate path. Random UUIDs are stable only with an additional
registry and obscure standard HID semantics. Both may be control-plane labels,
not the canonical realtime identity. Rejected.

## Selected type boundary

`KeyIdentityV1` is a small POD value:

```text
schemaVersion + namespace + usagePage/controlSet + usage/control
```

Namespaces initially cover USB HID usages, Soup/UAP extended codes and
HallJoy-defined semantic inputs. USB Menu is page `0x07`, usage `0x65`; media
keys use page `0x0C`; DrunkDeer Fn remains UAP code `0x409`; its physical
Menu/Fn2 control remains the proven UAP OEM1 code `0x403`. Labels do not forge
another identity.

`AnalogSnapshotHeaderV2`, `AnalogDeviceV2` and `AnalogSampleV2` are also POD.
The header reports provider/sample/value/ownership generations, timestamps,
counts, capacities, required counts and complete/truncated state. A sample
reports device index, full key identity, normalized float, optional raw
numerator/domain and explicit owned/valid/fresh flags.

This is the semantic data contract, not yet the final shared-memory layout.
IPC V2 will later place these records into separately versioned data sections
with negotiated mapping capacity and read-only parent payload access.

## First gate

The portable contract test must prove:

- HID `0xF0`, HallJoy semantic code `0xF0`, UAP Fn/OEM1, USB Menu and USB
  consumer media identities are distinct and invalid combinations fail closed;
- 12-bit adjacent raw levels survive even when both would become legacy milli
  zero;
- explicit zero is a valid owned/fresh release sample;
- 1/8/16/32-device complete snapshots validate;
- insufficient capacity and false complete/truncated combinations fail;
- same-device duplicate keys fail while the same key on distinct devices is
  accepted;
- same-generation counters cannot regress and a new provider generation must
  restart its sub-generations explicitly.

## Migration and rollback

R2-A/B1 adds types, validation and tests only. No UAP/native/backend/profile/UI
route changes. Its rollback boundary is therefore the new module, tests,
project entries, runner entry and this decision.

Next packages:

1. UAP read-only adapter acquires one existing dense generation per tick and
   emits this contract without changing parser bytes.
2. Native compatibility adapter converts existing milli values while retaining
   an explicit `legacy-quantized` flag; family migration later removes it.
3. Deterministic old/new XUSB equivalence and source-ownership tests gate the
   first production route switch.
4. IPC V2 and native process containment follow only after the in-process
   contract is proved.

No production support claim, user EXE or hardware request belongs to R2-A/B1.
