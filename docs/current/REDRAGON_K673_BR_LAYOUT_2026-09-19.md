# Redragon K673RGB-M ABNT2 layout — 2026-09-19

## Resolution

The BR position-100 International1 (0x87, DOM IntlRo) assignment is not evidence
of a missing Right Shift. The manufacturer's exact ABNT2 product image shows a
wide /? key in that location and no right Shift. The prior deferral incorrectly
assumed a conventional right Shift. Keep the existing proven analog factory map.

Official product: https://novo.redragon.com.br/produto/ucal-magnetico
Part K673RGB-M, EAN 6950376733566, explicitly ABNT2.
Exact ABNT2 image: https://static.wixstatic.com/media/71a6c2_0bc51e6aa4cc4f0f889739d7d0b878d4~mv2.png
The same page's general banner (21c3e233823643c589f9e41e95bd0662) shows a
right Shift and different legends. It is not the source for the BR transcription.
This source discrepancy is recorded here rather than silently substituting UK.

The live official profile was re-fetched on 2026-09-19 and remains byte-identical:
https://www.illumipc.com/config/keys/7272BRHEXYXK673JCARGB.json
SHA256 e2ed942977639d2c925c083f186f8285045c212238bd54ca3dfa462b388e85b5.
Its type=uk selects compound Enter rendering; it does not mean UK identity.
Its Japanese IntlRo caption is a generic DOM/HID name, not proof of a JIS board.

## Implementation

- Redragon / K673RGB-M / ABNT2, 81 published analog keys; 83 vendor records.
- Preserve matrix position 100 -> 0x87; normalized English label `Intl /`.
- No fabricated Right Shift. Fn (121) and encoder (15) remain omitted because
  the existing backend publishes no keyboard depth channel for either control.
- Use vendor coordinates and CSS compound Enter contour; all positions checked
  against the existing native BR factory map.
- Exact verified W669 product 7272BRHEXYXK673JCARGB selects the preset. UK and
  US retain distinct tokens; no VID/PID-only inference.
- ABNT2 is an explicit catalog/picker suffix, grouped with ISO under K673RGB-M.
- Native protocol, factory map, depth scale and user settings are unchanged.
- This supersedes the BR deferrals in REDRAGON_LAYOUTS.md,
  REMAINING_LAYOUTS_2026-09-14.md and the older owner context.

## Evidence and validation

Cached manufacturer images are in docs/research/redragon-layout-sources.

k673-br-official-0.png SHA256 0f76365fcb17008b614af091cfc3a86a99ab16401274cea8b28f82cc79161d76.

k673-br-official-1.png SHA256 e5a41afa4dba05b51d02287939218ce9cc4214234374b7117d04c793398c8bc9.

Python layout suite (20 tests), Redragon pipeline check and focused native
identity/W669 protocol tests pass. Production-linked profile/catalog checks and
release delivery evidence follow below. No physical keyboard or application
visual test was performed; owner evaluates application appearance.

Backups: .local/backups/k673-br-before.zip and
.local/backups/layout-integrate-hz71dy61. Evidence:
.local/k673-br-native-tests.txt, .local/k673-br-profile-tests.txt.

## Final delivery

Production-linked profile, picker, catalog and 16 recovery scenarios: PASS.
Compiled inventory: 102 source / 83 visible variants.
Release build and four candidate self-checks: PASS; ordinary EXE installed through
tools/build_release.ps1. Existing optional ViGEm PDB warning only.
Evidence: .local/k673-br-release-build.txt. No GitHub publication.

Path: build/bin/Release/x64/HallJoy.exe
Size: 9375232 bytes
SHA256: 5d77764e4e6b47bf2961f6f840da65be754181bdeb0ec000930527dd7d2c14a3
