# RM-30-SAYO — SayoDevice O3C protocol review

Date: 2026-09-06

## Scope

The enabled route is a native O3C session, not a generic promise for all USB
devices carrying `VID 8089`. PID `0009` is physically evidenced. A different
Sayo PID needs the existing bounded, read-only `0x22` capability response
before it is retained. This review preserves automatic user-letter association;
that behavior is intentional and separately audited.

## Wire contract

| Traffic | Admission / framing | Decoding and failure behavior |
|---|---|---|
| Keyboard | report starts `00`, Generic Desktop keyboard interface, at least 8 bytes | boot usages are read only; exactly one newly pressed usage may be associated with one unambiguous physical edge |
| Physical edge | report `21`, at least 12 bytes | byte 8 is `10` down or `11` up; byte 9 must be index `0..15`; other shapes are ignored |
| Depth poll | writable 1024-byte output interface | only observed prefix `22 12 3C 13 05 00 15 01` is sent; no firmware/profile/calibration operation is used |
| Depth response | report `22`, at least 14 bytes | three little-endian values at offsets 8, 10 and 12; probe rejects values over twice the captured 4000 domain |

Depth is published as `raw * 1000 / 4000`, clamped to 0..1000, with values
below 4 neutralized. The 4000 denominator is capture-derived, not a universal
firmware scale claim. The depth report is refreshed every 8 ms only on a
writable 1024-byte interface; a non-writable session can still consume an
externally produced stream.

## Identity and lifecycle boundaries

- The discovery maximum is eight HID interfaces, which may be parts of one O3C
  session. Every selected interface is exact-path claimed only after the
  selected PID/proof decision.
- A normal keyboard report remaps an index only if one physical edge remains
  down within the 80 ms association window. Multiple candidates, split reports,
  repeat reports, timeout and reconnect fail closed; fallback F/G/H exists only
  until an unambiguous real mapping arrives.
- A fresh depth value is at most 160 ms old. A later down edge uses the existing
  binary-full fallback only when depth is stale; release always publishes zero.
- Reader failure signals the shared stop boundary and neutralizes output. Group
  stop cancels every handle before one bounded join; incomplete join retains the
  group and blocks restart.

## Explicit non-promotion

The current model aggregates a single O3C session. Two separate physical O3C
devices do not have per-device edge/depth/mapping state, so their automatic
letter identities intentionally remain unsupported rather than being guessed
from a shared candidate pool. Implementing independent simultaneous O3C support
requires a new per-physical-device model and real dual-device captures; it is
not a safe local parser change. This review therefore records the limit rather
than misrepresenting the existing reader-interface count as multi-keyboard
support.

## Verification

- `sayo_letter_matcher_test.cpp` covers normal ordering, cross-report ambiguity,
  repeats, held-neighbor timeout, reconnect reset and intentional same-letter
  mappings.
- Sayo letter-matching, exception-boundary and cooperative-shutdown static
  audits passed as part of the full static native-backend suite.

No executable, HID device, controller output or diagnostic image was started.
Physical timing, post-change O3C reconnect and separate-device behavior remain
hardware evidence gates.
