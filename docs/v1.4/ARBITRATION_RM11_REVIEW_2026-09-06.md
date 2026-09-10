# RM-11 — explicit input arbitration

## Characterized policy retained

`Arbitrate` receives already-acquired source states; it does no HID/SDK/mouse
I/O and does not choose device enumeration order.

| Key/source state | Result |
|---|---|
| Standard HID, fresh owned native + fresh owned provider | Maximum normalized value; both source bits are retained. |
| Extended key, fresh owned native + provider | Native only; no duplicate SDK read is introduced. |
| Native owned zero | Zero remains a valid native result and blocks digital fallback. |
| No native owner, provider owned zero, fallback enabled | Digital may contribute, preserving the prior fallback behavior. |
| Stale/unavailable state | It contributes nothing. |
| Mouse pseudo-key | It remains outside keyboard-source arbitration. |

The old direct `max` in the qualified path is replaced by this function.  The
shadow wrapper calls the same function with no digital source, preserving its
deliberate no-training/no-fallback boundary.

## Provenance boundary

RM-09 preserves exact device identities in the native V2 endpoint.  The current
qualified legacy route still consumes `NativeAnalogReadResult`, whose internal
catalog has historical per-key max semantics.  RM-11 makes the next aggregation
boundary explicit but does not falsely claim that the legacy read regained that
lost identity.  Switching the qualified consumer to the V2 native snapshots is
an integration task after Provider V2 promotion/qualification; no user-visible
merge policy changes here.

## Verification

The portable test covers standard max, extended native authority, owned-zero
fallback blocking, stale rejection, V2 multi-device projection and controller
frame comparison.  Hardware confirmation of actual concurrent physical devices,
disconnect and hotplug remains pending.
