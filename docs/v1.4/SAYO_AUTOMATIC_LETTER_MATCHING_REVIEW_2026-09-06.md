# RM-03 — Sayo automatic letter matching review

Date: 2026-09-06. Scope: production Sayo O3C automatic letter association.
This review does not change the user's no-wizard product contract, HID
admission, depth normalization, polling protocol, or default F/G/H fallback.

## Evidence and defect

`backend_sayo.inc` accepts `0x21` physical edges independently of normal
keyboard reports. Before this package it retained just one global pending index.
Consequently the production sequence `A down`, `B down`, then a normal keyboard
report with one newly pressed letter A replaced the pending candidate with B and
could map B to A. The existing `addedCount == 1` check only rejects multiple
letters in one keyboard report; it cannot reject multiple physical candidates
spread over reports.

The old-bug oracle is therefore: two still-valid physical candidates plus one
new HID letter must change neither mapping. An unambiguous later press must
still learn automatically, without an activation-point delay.

## Options considered

| Option | Correctness / compatibility assessment |
|---|---|
| Keep one newest pending index | Rejected: directly causes the cross-report mis-association above. |
| Require all other keys to be released before learning | Rejected: makes normal rollover unnecessarily slow and changes working UX. |
| Track every live candidate in the match window and accept only one | Chosen: preserves automatic mapping and early depth, while refusing only information that cannot identify a physical key uniquely. |
| Add a manual mapping wizard or persist learned identity | Rejected by product contract; neither solves the temporal ambiguity safely. |

## Chosen invariant

The production-used `SayoLetterMatcher` records a candidate per physical index.
On a keyboard report with exactly one newly added HID letter it returns an index
only when exactly one candidate is still down and inside the 80 ms match window.
Two or more candidates leave all mappings unchanged. Release, reset, and timeout
remove candidates. A later isolated edge is learned normally. The matcher is
guarded with the Sayo mapping mutex because reader interfaces can report edges
and keyboard reports concurrently.

This is device-session state only; reconnect/reset clears it. Sayo's current
aggregation has one global O3C session rather than a per-physical-device output
model, so two simultaneous O3C devices remain unsupported for mapping identity:
their overlapping candidates intentionally fail closed instead of cross-learning.

## Separate stale-depth observation

`1000` is currently an explicit temporary digital fallback only after a down
edge when no fresh `0x22` depth response exists. A fresh depth packet immediately
overwrites it with measured normalized travel; an up edge publishes zero. That
policy is not evidence that `1000` is a measured analogue value, so it is not
changed in RM-03. Its freshness contract is reviewed separately from letter
matching.

## Evidence boundary

The portable state-machine test covers single press, reverse order, split and
combined keyboard reports, auto-repeat, held neighbour, duplicate HID letter,
timeout, release/reconnect reset, and the old-bug sequence. It executes the
same header used by production Sayo code. Static wiring confirms the backend
does not retain the single global pending index. Hardware timing and the
physical multi-device policy remain separate evidence.
