# RM-12 — Provider V2 promotion/removal gate

## Existing qualification infrastructure

The dedicated `HallJoyProviderV2Qualification` build writes a fail-closed
transactional report.  It requires one backend generation, same-transaction
dense/V2 capture, parent read-only plane, sufficient negotiated capacity,
configured-field activation and release, 1000 eligible/matched reports, 100
unique sample generations, no frame mismatch, no curve mutation, no digital
fallback and bounded V2 unavailability.

Its report explicitly records `production_route=provider_v2_authoritative` and
`legacy_dense_shadow_submitted_to_vigem=0`.  The ordinary route still retains
dense capture and a second shadow calculation to establish that equality; this
is intentional until the separate qualification artifact provides a PASS.

## Decision

Do not remove dense capture, V2 lease checks, or shadow work based on portable
tests.  No current PASS artifact/report for this worktree and real UAP device
was supplied, and running the qualification image would initialize input/output
while the user is gaming.  The removal gate remains unmet.

## Required promotion evidence

1. Build the dedicated qualification variant, recording source/artifact hash.
2. Run it on a real UAP device through required activation/release coverage.
3. Preserve its finalized `PASS` report plus timing/telemetry evidence.
4. Only then remove the ordinary-path shadow in a separate change with an
   explicit compatibility/emergency consumer ledger and a post-removal hardware
   comparison.

No production route was switched by this review.
