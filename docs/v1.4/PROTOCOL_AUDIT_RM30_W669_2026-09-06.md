# RM-30-W669 — Aula Standard / W669 live-stream protocol review

Date: 2026-09-06

## Admission and wire contract

The W669 route is independent of Aula 6×21. It requires `VID 2E3C`, vendor
usage `FF1B:0091`, a 64-byte report with report ID `01`, and uses the following
bounded protocol:

| Operation | Request | Required response / rule |
|---|---|---|
| Firmware product | `01 0D` | parse the exact product from the returned device-info CSV before selecting a known factory map |
| Travel capability | `01 21 … 04` | matching subtype `04`, valid processed maximum/unit/format |
| Dynamic map fragments | `01 18 80` | ten bounded fragments; zero records inherit only an already selected exact factory map |
| Live subscription | `01 21 … 18 02 <6-column mask>` | RAM-only subscription, then publish only valid `21/01` live events |
| Poll-rate observation | `01 21 … 0A` | optional read-only telemetry; code zero remains “firmware default”, not a fabricated rate |

Known SI2825, SI2828, SI2851 and the three exact Redragon K673 firmware
products select distinct pinned maps. An unknown `0D` product never inherits a
layout by PID, product-name substring or key count. In particular, a generic
`K673RGB-M` HID descriptor is not an admission fallback.

The reader intentionally does not request/publish `21/0E` sensor-domain
snapshots: physical evidence showed that this domain is not the processed
live-travel domain. The runtime allow-list has no firmware/calibration, map,
lighting or profile mutation.

## Current safety boundary and unresolved evidence

Packet identity, length, row/column bounds, processed-scale normalization,
factory-map selection, runtime re-proof, stop cancellation and final ownership
neutralization are covered by the current tests. The static review also confirms
that an ordinary read timeout is preserved as an idle timeout rather than being
miscounted as a transport failure.

There is nevertheless an unresolved transport fact that source inspection
cannot safely guess: after the RAM subscription, the device is an event stream
and an idle keyboard may legitimately produce no `21/01` reports. Therefore a
blind “no reports for N seconds” disconnect would periodically clear/reconnect
a healthy untouched keyboard. Conversely, the present reader has no
firmware-proven independent liveness response while that stream is idle, so a
lost release can remain published until real removal, a later event, or worker
shutdown.

Do not add a time-based neutralization/reconnect or concurrent recovery request
without real-device proof of an idle-safe, correlated heartbeat/recovery
transaction and its interaction with the single live-stream owner. That would
replace a known limitation with unverified behavior on physically working
keyboards. This remains the explicit physical-evidence gate for W669, recorded
as `HJ-V14-P0-004` / `HJ-W669-P1-003` in the risk register.

## Verification

`aula_w669_protocol_test.cpp` verifies exact product selection, 6×22 bounds,
fragment assembly/inheritance, subtype filtering, scale conversion, poll-code
handling and malformed report lengths. `aula_w669_backend_static_audit.py` and
the complete static native suite passed on 2026-09-06. No HallJoy executable or
live HID command was run for this review.
