# RM-09 — Native / UAP common V2 snapshot review

## Decision

The existing `halljoy::analog_provider_v2` contract is the one common snapshot
format.  Native protocols must adapt into it; RM-09 does not introduce a second
native-only DTO and does not change the qualified output path.  Source
arbitration remains RM-11 work: a V2 snapshot preserves provenance first, then a
separately characterized policy may combine sources.

## Native-to-V2 mapping

| V2 field | Native adapter rule |
|---|---|
| provider ID | Stable, non-zero ID derived from the descriptor's ASCII ID. |
| device ID | Stable composition of provider ID, exact interface fingerprint, VID and PID.  VID/PID alone are never an identity. |
| exact interface ID | The normalized complete HID interface-path fingerprint already used by the native/UAP ownership registry. |
| topology | One `AnalogDeviceV2` for every connected exact interface exposed by a backend. |
| samples | One `(device, key)` sample.  `Owned`, `ValueValid` and `Fresh` are set only when the backend proves them. |
| raw scale | A legacy milli backend uses `ValueFromLegacyMilli`: `rawDomain=1000` and `LegacyQuantized` remain visible.  No raw bits are reconstructed. |
| capacity | `required*Count` is the full count before truncation; `Truncated` is set exactly when caller storage is insufficient. |
| generation/time | A backend publishes only an even, complete local publication generation.  Header time is the monotonic completion time of that publication. |
| completeness | A row-polled device is deliberately not marked `Complete` until it can prove a coherent whole-device pass.  Fresh independent rows may still be represented as samples, as required by RM-09. |

## SparkLink pilot

SparkLink is the pilot because its layout is capability-proven, its claimed HID
path is exact, and RM-04 already characterizes per-row freshness.  The adapter
exports its current per-key normalized values as explicit legacy-milli samples.
It must use a publication seqlock so it never mixes a layout or row-freshness
transition into one V2 snapshot.  A missing/disconnected Spark device publishes
zero devices and zero samples; it cannot erase another device because no merge
occurs in the adapter.

## Compatibility and non-goals

`NativeAnalogBackends_ReadMilli` remains unchanged for the legacy qualified
route.  The new registry V2 read is opt-in and source-preserving; it is not an
early-max replacement.  Consequently no product behavior changes before RM-11.
Hardware qualification is still required for Spark transport timing and for any
additional protocol family.

## Negative oracle

A synthetic pair of equal HID usages with different exact-interface IDs must
produce two devices and two samples.  Removing one input must retain the other
fresh sample.  The portable test covers that adapter property; it is not a
claim of two-device Spark runtime support.
