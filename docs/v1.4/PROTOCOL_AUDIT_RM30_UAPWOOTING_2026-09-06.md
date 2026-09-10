# RM-30-UAPWOOTING — embedded UAP Wooting protocol review

Date: 2026-09-06

## Admission and decoding

The pinned private UAP recognizes modern Wooting vendor `31E3` on usages
`FF54` (V1 report form) and `FF53` (V2), plus explicitly named legacy
`03EB:FF01/FF02` V1 devices. V1 parses big-endian scancode/value triples with
the established 8-bit scale. V2 parses four-byte matrix/scancode/packed-value
records with the 10-bit scale. Both preserve the full supported key domain,
including media and OEM/Fn codes; unknown codes are ignored rather than
relabelled as ordinary keys.

## Snapshot and disconnect contract

Each device builds a fresh dense and provider-key snapshot per successful
read. Values are finite-clamped to `[0,1]`, repeated key observations merge by
maximum only within that exact device snapshot, and absent keys become explicit
zeroes in the next full snapshot. An empty Wooting report marks the device
disconnected; the same update publishes the empty snapshot, while the host's
owner-pinned registry/removal flow prevents a removed device from retaining a
stale published value.

The UAP's V2 export is an immutable, device-identified snapshot. It does not
claim model-specific physical layout proof merely because a Wooting-compatible
device is decoded. Multi-device ownership and disconnect behavior still require
physical qualification; no executable or HID command was run in this review.
