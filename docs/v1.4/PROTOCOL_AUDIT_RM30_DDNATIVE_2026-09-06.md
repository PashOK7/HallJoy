# RM-30-DDNATIVE — DrunkDeer native diagnostic protocol review

Date: 2026-09-06

## Scope and wire handling

The native DrunkDeer route is a diagnostic-only target, not a production
support claim and not a substitute for the embedded UAP route. It uses one
serialized tracking transaction per frame and accepts a complete 6×21 matrix
only after all three response chunks are present, uniquely numbered and
correlated. Arrival order is not trusted; duplicate, impossible or unrelated
chunks discard the incomplete frame.

The parser keeps the raw `0..40` domain separate from published normalized
values. It preserves the extended key domain for Fn/Menu/OEM rather than
silently coercing it into an 8-bit ordinary HID usage.

## Layout and diagnostics

- Product `352D:2382` selects the independently pinned full G65 ANSI map.
  Other candidate products use only the explicitly labelled diagnostic generic
  map, never inherit the G65 physical layout.
- The diagnostic records report headers, raw cell change/release evidence and
  payload offsets, but Windows raw keyboard data is logger-only: it cannot
  rewrite the analogue map or claim input ownership.
- Transport errors have bounded same-handle retries, definitive-disconnect
  classification, neutralization and reconnect handling. The protocol lock
  keeps requests from overlapping.

The portable protocol test verifies reordered chunks, duplicate/impossible
chunks, map identity and G65 physical fixture cells; static audits retain the
diagnostic-only/catalog boundary. No executable or HID session was run for this
review. Real-device evidence is still required before a native production route
could be considered.
