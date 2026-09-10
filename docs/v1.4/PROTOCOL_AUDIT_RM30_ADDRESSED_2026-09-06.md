# RM-30-ADDRESSED — Addressed Analog `09/94/02` protocol review

Date: 2026-09-06

## Evidence and supported behavior

This is a physically tested, working route. It is admitted only after the
exact `FF60:0061` HID fingerprint, 64-byte checksum validation and a correlated
`09 94 02` response for explicitly requested key IDs. It is intentionally a
one-active-device session; a matching VID/PID or title never claims an endpoint.

## Command contract

| Command | Purpose and validation |
|---|---|
| `09 83 00` | read-only dynamic key-ID to HID map; malformed/partial data cannot create an out-of-range mapping |
| `09 94 02` | addressed live-value request, at most nine unique key IDs; response must have matching command, requested IDs, record length and checksum |
| `09 98 02` | one established session-start command that disables the legacy last-key diagnostic mode before normal polling |

`09 98 02` is retained by explicit owner confirmation and historical protocol
documentation. It is issued only after the endpoint has passed the normal
fingerprint/proof path; it is not a calibration, firmware, layout, lighting or
profile write and it does not broaden device admission.

## Parser and lifecycle contract

- Frames are exactly 64 bytes (with an optional leading report-ID byte) and
  must have an `FF` checksum. Map and live records are length-bounded six-byte
  entries; duplicate/unrequested IDs, implausible values and late replies are
  rejected rather than attributed to a newer request.
- The pure scheduler chooses no more than nine unique keys, gives bindings and
  active movement priority, and reserves background service so the matrix does
  not starve under heavy bindings/chords.
- A timed-out request clears its pending token. The reader then classifies an
  arriving old reply as late, not as success for the next plan. Per-key samples
  expire after 500 ms and release/stop neutralizes publication.
- Reader cancellation is owned by the reader lifetime; the main worker joins it
  before closing dependent state. An incomplete outer stop retains resources
  and blocks unsafe restart.

## Verification

The addressed scheduler test covers bound/active/background fairness, full
matrix chord pressure and the nine-key bound. The exception/lifecycle static
audit and full native static suite passed. No live device or executable was run
for this review; existing physical confirmation remains the product evidence.
