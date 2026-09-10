# RM-30-UAPMADLIONS protocol audit — 2026-09-06

## Scope

This card covers the embedded UAP poll reader for the admitted Madlions
`373B` `FF60:0061` layouts. It retains the existing SafeHID request/response
transport: a 33-byte `02/96/1C` request contains a four-position offset and the
reader requires at least 27 reply bytes before extracting four BE16 travel
fields. The native MAD68 route is separate; its evidence is not copied onto the
UAP route.

## Persistent late-chunk failure correction

The previous per-device `consecutive_failed_reports` counter was reset after
every successful chunk. A damaged late chunk could therefore fail once per
round while earlier successful chunks reset the counter, preventing the
eight-failure disconnect threshold forever.

The reader now keeps a bounded 64-slot failure count keyed by the requested
four-key chunk (`offset / 4`). A successful reply resets only its own slot; a
short/failed reply increments only its own slot, clears that chunk's cached
values immediately, marks this poll failed for pacing, and fails closed after
eight failures of that same chunk. This makes the failure decision independent
of earlier successful chunks and preserves the existing isolated-host recovery
boundary.

```
02/96/1C + offset -> SafeHID serialized transaction -> reply length >= 27
  -> four BE16 travels -> per-key cache/publication
  -> failure: per-chunk counter + immediate local neutralization
```

## Remaining evidence limits

The reply header/echo/status and record metadata still have no captured
firmware-backed correlation contract in this repository, so this card does not
invent byte checks. Likewise, MAD68R still lacks an independent layout proof,
and an incremental polling cache is not a firmware full-snapshot/release
protocol. Those are explicit hardware/protocol evidence gates, not grounds to
accept an arbitrary queued 27-byte response as proven.

Parser/static checks cover layout bounds, short replies, per-chunk persistence,
and bounded SafeHID ownership. No executable, HID session, firmware action or
ROG Azoth diagnostic ran in this review.
