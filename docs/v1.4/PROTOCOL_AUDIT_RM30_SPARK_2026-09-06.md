# RM-30-SPARK — SparkLink / XD row protocol review

Date: 2026-09-06

## Scope and evidence state

This reviews the enabled `sparklink` native family in
`backend_sparklink.inc` and its registry adapter in `backend.cpp`. It is a
current source and fake-transport review, not new physical certification.
Real multi-device, held-key/release and reconnect evidence remains pending.

## Wire contract

| Stage | Request | Required response correlation | Decoding / boundary |
|---|---|---|---|
| Identity | `01 02` | bytes `01 02`, 64-byte payload | device type must be `01`; logged identity is not itself a route claim |
| Layout row | `03 01 00 row` | bytes `03 01 00 row`, 64-byte payload | 21 little-endian 16-bit entries from offset 4; only `1..255` becomes a HID usage |
| Live row | `04 03 01 row` | bytes `04 03 01 row`, 64-byte payload | 21 little-endian 16-bit values from offset 4; requested row is correlated before commit |

The endpoint needs a valid HID capability record, matching vendor usage score,
and input/output reports of at least 64 bytes. An exact SparkPlayJoy 6x21
identity is excluded before opening so this older family cannot probe it.
Routing is claimed only after the synchronous device-info proof succeeds.

## Publication and failure contract

- Layout is bounded at 8 rows × 21 columns. Unknown or extended layout codes
  become unmapped (`0`) rather than aliases into another key.
- Each row owns its own values and timestamp. A row is fresh only after a
  successful request and for at most 2160 ms (8 × (250 ms transaction timeout
  + 20 ms safe poll interval)). A never-seen row is not fresh.
- Aggregate HID value is the maximum across only fresh row owners. On expiry,
  row-limit reduction or reset, only the affected row is removed; a duplicate
  HID in another fresh row is retained.
- A stale/short/wrong-command/wrong-row response is ignored until the bounded
  transaction deadline; repeated failures end the worker and publish zero.
  Stop signals the worker, cancels pending I/O, then uses the common bounded
  join policy. An unconfirmed stop retains resources and blocks restart.
- The dead `if (false)` burst experiment is not reachable and remains owned by
  RM-35. It was neither enabled nor treated as protocol evidence.

## Scale and qualification limit

Transport values are 16-bit. The legacy output is normalized against a
per-key observed range constrained to 3000..5000; it is deliberately not
presented as a firmware-authenticated raw domain or precision guarantee.
The V2 snapshot consequently labels this route as a partial legacy-milli
source rather than a complete raw snapshot.

## Verification performed

- `sparklink_row_freshness_test.cpp`: never-seen, exact expiry boundary, stale
  row neutralization and fresh duplicate-HID preservation.
- `sparklink_hotplug_age_test.cpp`: monotonic-age underflow/race shape.
- SparkLink row-freshness, hotplug-age, exception-boundary and cooperative-stop
  static audits, plus the full static native-backend suite, passed.

No HallJoy executable, HID traffic, controller output or diagnostic image was
started during this review.
