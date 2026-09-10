# RM-30-RESEARCH protocol audit — 2026-09-06

## MCHOSE Ace 68

The Ace 68 implementation is an isolated diagnostic for exactly
`41E4:2114`, Generic Desktop `0001:0000`, and 64-byte input/output reports.
It records complete transport frames and `A0` descriptor/raw observations but
never claims ownership of gameplay input. Its firmware-derived read allow-list
is `03`, `04`, `05`, `08`, and `A0` under the official `55` framing; calibration,
activation and feature-report paths are absent. The read-only baseline is
separate from the writable diagnostic transport so a denied write is not
misreported as an analogue result.

An `A0` marker or 0..1600-looking value is not a key map. The first three
descriptor bytes remain evidence to capture and correlate with the exact
firmware/model, not bytes to translate through digital keyboard events. Static
audit verifies exact admission, allow-list, full-frame logging and the
transport-only routing boundary. No production support is promoted.

## Madlions Titan68 Turbo

The Titan68 Turbo diagnostic is independently admitted only as paired sibling
interfaces of `28E9:31FD`: control `FF87:0020`, report 06 / 64 bytes; stream
`FF88:0021`, report 07 / 3 bytes. It reads mapping through `12`/`16`, sends the
reviewed volatile simulation pair `36:01` then mandatory `36:00`, and treats
report-07 raw12 data as visual diagnostic input only. Firmware/trace evidence
does not authorize calibration (`37`), persistent write, firmware controls, or
gameplay ownership.

The source also records a decisive product boundary: observed stock `36` stream
activity did not establish simultaneous normal keyboard typing. The diagnostic
and any future experimental image must remain separate until a real-device
procedure proves simultaneous ordinary HID, analogue movement, release,
reconnect and restoration. Historical text that described a `37:01` diagnostic
sequence is superseded by the current source/static allow-list: it is not sent.

## Result

Both routes remain evidence-gathering diagnostics. No opaque byte was promoted
to a semantic key by digital correlation, and no executable, HID session,
firmware action or ROG Azoth diagnostic ran during this card.
