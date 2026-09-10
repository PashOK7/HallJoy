# RM-10 — numerical precision review

## Current evidence

The common V2 adapter correctly labels the existing native value as
`LegacyQuantized` with a `[0,1000]` domain.  This is an honest compatibility
representation, not a claim that SparkLink hardware is only 1001 levels.

SparkLink receives a 16-bit `routeRaw` word, but the current normalization does
not have a firmware-proven physical denominator.  It starts with an observed
maximum of 3500, updates that value only for readings up to 5000, and divides by
the observed value.  Therefore 3500/5000 are implementation guardrails, not
evidence of sensor raw min/max or a stable device scale.

## Decision

Do not propagate `routeRaw` as V2 `rawNumerator/rawDomain` yet: naming the
observed maximum as a raw domain would fabricate precision and can make the same
physical travel map differently before and after a newly observed peak.  Keep
the RM-09 legacy adapter unchanged until a packet trace or firmware/documented
protocol establishes encoding, zero, maximum, signedness and stable scale.

## Required evidence to reopen the Spark raw path

1. Captures covering rest, near-zero, multiple travel points, bottom-out and
   release for at least two keys and reconnect.
2. Proof whether `routeRaw` is travel, filtered travel, calibration-relative
   travel, or another transport quantity; record byte order and valid range.
3. A stable domain/zero rule, or an explicit proof that values are only
   per-scan normalized and thus must remain legacy quantized.
4. A hardware near-zero/output comparison after the existing 0..1000 route is
   preserved as the control case.

No raw-path code is changed by this review.  This is a real hardware-evidence
gate, not a reason to invent a conversion.
