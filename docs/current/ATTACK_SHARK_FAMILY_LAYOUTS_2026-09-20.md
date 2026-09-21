# ATTACK SHARK family layouts — 2026-09-20

## Owner scope and delivered result

Owner approved the layout batch, then explicitly deferred remap readback.
All 37 enabled RY5088 revisions now have exact native automatic layout tokens.
The catalog has 16 visible ATTACK SHARK ANSI geometry groups, including the
98-key and 102-key number-pad layouts. There are 15 new reviewed source reports;
two share geometry with existing Pro entries and merge with them. Overall
production catalog: 121 source variants / 98 visible variants (previously 106/85).

Exact native identification remains required. USB VID/PID or product captions
alone never select these layouts. Multi-device/manual fallback behavior remains
unchanged. Factory assignments are used; no new HID commands, remap readback,
calibration mode, digital-to-analog inference or travel normalization changes.
The existing experimental testing banner remains.

Legacy X65/X68/X82 Pro tokens and names remain valid. X68 Pro merges with X68 HE
and R68 HE; X82 Pro merges with X82 HE. Edited legacy presets remain independent.
Other identical ATTACK SHARK geometry groups use one canonical source report
with multiple exact dev_id selectors. UI names use `/`; filesystem names retain
safe `+` separators. No cross-brand geometry merges.

## Exact-source chain

`tools/extract_attackshark_family_layouts.py` resolves each manufacturer registry
entry's keyLayout enum, its RT component, the imported (or inline) KeyMappings
component, and the default SVG. The 37 revisions resolve to 16 vendor schemes.
All links and SVG files are pinned under `docs/research/attackshark-layout-sources`.
The extraction does not execute the vendor application or infer geometry from
USB IDs, model names, matrix slots or unrelated keyboards. Consumer/encoder
controls are omitted; they do not have ordinary keyboard-depth assignments.
SVG artwork remains research evidence and is not embedded into the executable.

`tools/build_attackshark_family_layouts.py --check` validates reports/catalog and
the exact native token aliases. `tools/layout_pipeline.py check "ATTACK SHARK"`
validates source locks, unique HID labels, non-overlapping geometry, registrations
and generated files. The model/profile manifest independently supplies factory
maps for geometry-to-native coverage checks.

## Reviewed discrepancies

- Beat75 dev2633: shared 81-key SVG labels Right Alt (usage230), but the exact
  factory map has Right Control (usage228) at that position. Only this model's
  layout is corrected to Ctrl. X85 Ultra uses the same base SVG with real Right
  Alt and stays a separate layout. Raw extraction and correction are both stored
  in its source report; backend matrices were not changed.
- Several compact profiles contain non-US usages50/100 beyond the shown ANSI
  geometry. dev2901/dev2902/dev3650 additionally have F1–F12 records despite the
  compact SVG. These are recorded in extraction/report discrepancies, not drawn
  as invented physical keys or removed from native protocol maps.
- dev2552 has an additional Insert record absent from its 82-key SVG.
- Vendor scheme names and retail names are not authoritative key counts:
  Common68_ZAP68 shows67 keyboard keys, Common89_SG8967 shows87, and
  Common81_MK830 shows80. Geometry follows the actual linked SVG. All displayed
  keys have a corresponding exact-profile native factory usage after the Beat75
  correction. Omitted media/encoder controls are explicitly listed in reports.

## Visible groups

| Model group | Displayed keys |
|---|---:|
| R98 GT / R98 HE / R98 Pro / R98 Ultra | 98 |
| X65 HE | 67 |
| R68 HE / X68 HE / X68 Pro HE | 66 |
| Beat75 | 81 |
| X87 Ultra | 84 |
| X82 HE / X82 Pro HE | 83 |
| X68 MAX / X68 Ultra | 68 |
| X85 Ultra | 81 |
| R86 Pro HE | 87 |
| R82 HE / R82 Pro HE | 80 |
| K85 / K85 Pro HE | 82 |
| X60 HE | 61 |
| X96 HE / X98 HE | 102 |
| R85 HE / R85 Ultra | 79 |
| X820 Pro | 80 |
| X65 Pro HE | 66 |

## Verification and delivery

- Exact source extraction: PASS, all37 revisions /16 vendor schemes.
- Generated reports, locked sources and layout pipeline: PASS.
- Portable C++ layout test: PASS, all37 native tokens resolve to real presets;
  every visible HID exists in its exact factory map; unique usages and existing
  Pro geometry/identity tests remain valid.
- Production-linked profile/catalog tests: PASS, every generated identity,
  automatic/manual/multi-device transitions, merged-name migrations and edited
  legacy precedence. No cross-brand merges or unmerged same-brand duplicates.
- Profile startup recovery: PASS,16 scenarios and repeated startup.
- Ordinary Release build and Shark/MINI60/NA87/embedded-ViGEm checks: PASS.
- Owner evaluates visuals; no agent visual test, physical keyboard test or
  GitHub publication. Existing compiler/PDB warnings did not prevent the build.

Evidence: `.local/attackshark-layout-profile-tests.txt` and
`.local/attackshark-layout-release-build.txt`.
Profile inventory: `C:/Users/PC/AppData/Local/Temp/HJProfileTest-374f724108ad4966808dcfd4b808d804/layout-catalog-audit.json`.
Backup: `.local/backups/attackshark-layouts-before-20260920.zip`, plus the layout
pipeline's own checked backup. Build lifecycle stages and tests the candidate
before replacing the ordinary target.

Delivered: `build/bin/Release/x64/HallJoy.exe`.
SHA256: `26c4ff4733fc258ad721abfde99d6709f49d54fd1f5f1a9f04e8e4025c6a5cd4`.

Parameter-read investigation was not integrated: the client's model-specific
travelSetting limits are settings metadata, not established physical full-travel
endpoints. Existing provisional3.5mm scaling remains documented in the family
support note. User-requested remap work is deferred.
