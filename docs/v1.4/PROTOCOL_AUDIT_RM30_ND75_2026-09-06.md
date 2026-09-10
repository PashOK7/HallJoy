# RM-30-ND75 — IROK ND75 M484 experimental protocol review

Date: 2026-09-06

## Admission and protocol separation

The ND75 candidate is intentionally separate from W669 despite the shared
`FF1B:0091` / report-ID-1 envelope. It requires exact `0416:7372`, an `0D`
identity response with controller `M484`, product `X86HERGB`, and a known `V1.`
firmware family, followed by the asymmetric device `21/04` capability response.
There is no soft identity or capability fallback.

Host requests use command `29` on channel `18`: RAM subscription `29/18/02`
and matching unsubscribe `29/18/03`. The device emits live `21/01` events. The
81-key, 6×22 map and subscription mask are pinned from the official
`KeyInfo_X86HERGB.config`; map/capability/event parsers enforce bounds and
normalise the proved raw domain `0..40` only after validation.

## Lifecycle and release scope

- A session re-proves its firmware identity after routing claim, publishes the
  known ownership set only after proof, clears it on removal/failure, and
  returns through bounded reconnect handling.
- Stop cancellation, read-error threshold and matching unsubscribe are explicit
  and covered by the portable/static checks.
- `HALLJOY_IROK_ND75_EXPERIMENTAL` keeps the backend out of the ordinary build.
  The owner-validation procedure remains the only path to collect physical
  input, release, coexistence and hotplug evidence.

No production scope was widened, and no executable or live HID command was run
during this review.
