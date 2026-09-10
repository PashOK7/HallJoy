# DrunkDeer layout and model batch — 2026-09-09

Current implementation supersedes the investigation-only status in
`DRUNKDEER_LAYOUT_IDENTIFICATION.md`. Single-brand batch; no new vendor/model
families beyond the DrunkDeer identities already declared by embedded UAP.

## Catalog

Seven physical layouts: A75 ANSI, A75 Pro, A75 ISO, G60 ANSI, G65 ANSI,
G75 ANSI and G75 JIS. Shared by main preview, Layout editor and Input Overlay.
No UK/FR/DE duplicate categories; English labels. ISO/JIS Enter uses the existing
compound contour in all consumers. No image widget or custom Enter renderer.

`tools/build_drunkdeer_layouts.py` generates both the UI geometry header and
126-cell physical tracking maps. Inputs are the parsed Antler factory/preview
audit and the frozen CSS rectangle extraction in
`docs/research/drunkdeer-layout-sources/rectangles.json`. Source hashes are kept
in each review; reproducible INI exports are in `docs/exports/drunkdeer`.
Normalize ordinary key height to 42 px with ONE scale for both axes, rounding
absolute edges. Do not reflow rows. Validate actual contours for overlaps,
unique identities, dimensions and missing keys. Compound notch widths follow
the straight edges of vendor transparent assets (46/164 ISO, 52/220 JIS).

Tracking uses preview offsets, because Antler's B7 handler uses those offsets
directly. Explicit reviewed exceptions to stale factory defaults: End at 99
for A75/Pro/ISO; ISO # at 75 and non-US backslash at 85. End=99 also matches the
previously shipped A75 decoder. Factory keyIndex typo at unused slot 19 on
G75/JIS does not alter array positions. G65 matches the full official factory
array; owner approved navigation offsets 35/56/77/98. Preserve all 45 earlier
physically observed G65 cells, including arrows, modifiers and extended Fn/Fn2.

## Runtime identification and analogue maps

The existing UAP device owner sends the official read-identity report
`04 A0 02 00 ...` only after stable-ID deduplication and before starting its
worker. Existing stable-ID seeds are unchanged. Request is restricted to the
five known PIDs. It uses the same handle and named mutex as analogue reads,
bounded mutex acquisition (100 ms) and bounded write/read transactions
(100 ms each), never UI-thread HID or repeated per-frame identification.
No calibration, firmware write or change to letter output.

Accept only an exact 64-byte response, expected report/command/status, known
signature AND matching PID. Verified name and flag travel through the existing
telemetry structure; ABI structure size/version unchanged. First-run selection
requires that flag, exact model/PID and 6x21 transport matrix, plus existing
fresh/coherent one-device one-shot policy. Device names alone do not select a
layout. Saved/manual selections, independent overlay choice and later hotplug
remain authoritative.

Verified models select their own generated analogue map. Raw travel scaling
and B6/B7 framing remain unchanged; this batch does not claim new high-precision
firmware scaling. If optional identity is unavailable, preserve the previously
shipped decoder for compatibility, WITHOUT setting a verified model or choosing
an A75 layout. This is deliberately not a claim that the legacy generic map
becomes exact for unknown firmware.

Append three Soup identities (non-US hash, Ro, Yen), preserving every existing
enum value. Add their exact bidirectional HID mappings. Both legacy dense and
provider-v2 paths therefore retain distinct ISO/JIS keys without aliases or
OEM-code substitution. Seven Soup overlay files are hash-locked. The dependency
audit now verifies the actual complete file set and normalized contents instead
of merely asserting that there are five files.

## Existing files and tests

A75 Pro remains catalog index zero; G65 stays index one. On load, upgrade an old
default only when ALL effective geometry, key identities, labels and spacing
still match the original. Any user edit preserves that file. Use the existing
atomic layout writer, no migration sidecars or extra backup spam. New layouts
use normal existing preset creation. Backup is
`.local/backups/drunkdeer-integration-20260909/` (release, source, old runtime DLLs).

Checks include generated-output reproducibility, source parser rejection,
all 126 G65 cells versus official factory data, all seven model signatures and
PID/name rejection, full Soup HID roundtrip for every generated key,
production-linked first-run selection for seven models, no-name-only selection,
one-shot policy and preserving edited legacy layouts. Existing editor event
test was corrected to resolve sorted combo rows by catalog identity: adding
models exposed its old assumption that row 1 was always catalog entry 1.

No visual test or physical DrunkDeer test is claimed. Owner evaluates appearance.
Connected-device private UAP ABI gate passed with the existing keyboard;
legacy/provider-v2 projections agreed. Release/simulator builds pass with the
existing optional ViGEm PDB warning. Final test/deployment evidence follows below.

## Delivered

Release SHA256: `F9FF13072B96C85FC0078EB5B0A45B98D9BAE6599FF14844E75914102213680A`.
Installed at `build/release/HallJoy.exe`, dependency lock and SHA256SUMS updated,
ordinary HallJoy restarted through Explorer. Embedded ABI1 resource SHA256 and
extracted runtime agree with the rebuilt plugin:
`196987606A83E7D4DD2D9F7FA253C209B70572331A65CC178C1DAA1E40A98FAE`.

All static and portable C++ checks PASS (source inventory 521); 11 DrunkDeer
Python tests, 3 Lemokey tests, 16 importer tests and both generated catalog
checks PASS. Lemokey geometry is unchanged; its source-review hashes were
refreshed because the shared Soup source changed. Soup key roundtrip PASS for
all seven maps. Private ABI live gate PASS: one device, equivalent legacy and
provider-v2 snapshots. Full Windows profile/editor/picker tests and rejected
startup file-preservation tests PASS:
`%TEMP%/HJProfileTest-9f03c11b2fcd4db0a6462eca3c9bbc62`.
The event suite exercises all seven DrunkDeer model selections and both compound
Enter variants; count assertions now follow the actual filtered catalog.

Owner check: select Brand **DrunkDeer** in Global settings or Input Overlay;
test the seven models, including ISO/JIS Enter. This completes this brand's
implementation batch. Wait for owner feedback before starting another brand.
