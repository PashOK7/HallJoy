# Remap: repeatable empty-profile binding hint

Clicking a physical-key preview button in Remap with no bindings in any of the
four pad slots demonstrates left-stick-up moving to W, regardless of the clicked
key. All stored axes,
triggers and buttons count, including keys absent from the visible layout and
inactive slots. Mouse pseudo-keys and other tabs do not trigger it.

The hint starts after the native button releases mouse capture. It copies the
first fully visible left-stick-up palette icon; it never moves the original, binds, saves,
captures input, scrolls the page or simulates a drop. No visible source means no
demonstration (also skipped in custom layouts without W). Repeated preview clicks
and their native mouse capture neither cancel nor restart playback; after playback
it may run again without a persistent display limit. Owner approved keeping this
behavior permanently while the profile has no bindings (2026-09-09).

A separate timer exists only for the unchanged 1.8-second sequence: fade in, quintic smoothstep
flight, hold and fade out. The cached glyph uses the existing nonactivating,
mouse-transparent layered ghost, exclusively while no drag/post-animation owns
it. Real palette interaction cancels the hint before acquiring that ghost.
Tab hide, resize, scroll, focus loss, non-preview capture, geometry changes, new bindings and
destruction stop it. No full-page animation repaint is added.

Checkpoint: `.local/backups/remap-hint-20260909/` (source and previous EXE).
Refinement checkpoint: `.local/backups/remap-hint-refine-20260909/`.
Motion and fades now have zero endpoint velocity and acceleration; no timing change.
Tests: portable production motion-function boundary/monotonicity tests and static
event-wiring/read-only guards. Visual evaluation remains with the owner.

Validation: complete static suite, compiled motion tests and Release x64 build
PASS. No visual inspection or changes to the user's bindings were performed.
Deployed SHA256:
`1E6B4DC70F2E937EC678E8C43C1E102842E7419133165FA655BA96803FA8B4E9`.
