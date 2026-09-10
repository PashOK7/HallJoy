# Permanent Discord entry in Global settings

## Text alignment correction (2026-09-09)

The title and description now form one vertically centred block. Previously,
independent fixed rectangles left unused space below the description and made
the pair sit above the button centre. DrawText measures the current selected font
and wrapped description; the pair is centred inside symmetric card padding.
Narrow cards centre text in the area above the stacked button. Measurement-only
rectangle width changes are discarded before painting. Existing font, colours,
card/button sizes and interaction remain unchanged; no new timer or HWND.
Backup: `.local/backups/discord-alignment-20260909/` (source and previous EXE).
Validation: Release x64 build and complete static suite PASS; source regression
guards paired measurement/centring and stacked bounds. No visual checks.
The release hashes below describe earlier checkpoints, not this correction.

Always-visible community card at the top of Global settings, before Global profile, independent
of keyboard detection. Title: HallJoy on Discord; brief community description;
compact Join Discord button with a subdued blue/violet accent and ordinary
hover/pressed states. Uses the existing retained surface and font, with a
stacked button at narrow widths. No animation, timer, QR duplication or automatic
network request. Clicking opens the existing HTTPS invite through Windows;
launch failure displays the invite in an error dialog.

Owner refinement: card width is capped at 480 logical pixels, aligned left,
and constrained to available content width. Below 440 logical pixels the button
stacks under the text. Existing text, heights and interaction are preserved.
Checkpoint: `.local/backups/discord-compact-20260909/`.

The invite now lives in community_links.h and is shared with the existing
missing-keyboard banner (including its clipboard operation). The banner QR is
unchanged and still encodes that exact URL. Regenerate QR if the invite changes.
Source/previous EXE checkpoint: `.local/backups/discord-settings-20260909/`.
The source audit checks both consumers reference the shared invite and the new
card is hit-testable. No settings/schema, keyboard or runtime changes are made.
X68HE research is waiting for the owner's log/version; no support enabled.

Validation: complete static audit suite and Release x64 build PASS. No visual
inspection or external browser launch was performed. Deployed release SHA256:
`3ACED5511A7B50671D28C3DFE3C45E6DDC4C8F98E2AABF1CD60289F3590F873C`.

Top placement retains the compact geometry and derives width directly from the
page dimensions, without depending on controls laid out later. The static audit
guards placement before Global profile. Previous executable checkpoint:
`.local/backups/discord-top-20260909/`.
