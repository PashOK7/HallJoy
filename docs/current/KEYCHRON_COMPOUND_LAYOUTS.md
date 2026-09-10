# Keychron catalog and compound keys — 2026-09-09

Owner approved compound-key support during the request to remove Imported from
display names and import the remaining available Keychron layouts. No analogue
protocol changes, firmware flashing, HID discovery or user-settings migration.
Backup: `.local/backups/keychron-catalog-20260909/` (source and previous EXE).

## Implemented catalog: 36 variants

| Family | Variants |
| --- | --- |
| K2 HE, K3 HE, K4 HE, K8 HE | ANSI, ISO, JIS each |
| K10 HE | ANSI, ISO |
| Q1 HE, Q3 HE, Q5 HE, Q6 HE | ANSI, ISO, JIS each |
| Q12 HE | ANSI, ISO |
| Q1 HE 8K, Q3 HE 8K | ANSI, ISO, JIS each |
| Q5 HE 8K, Q6 HE 8K | ANSI each |

Two previously shipped K4/Q1 ANSI arrays retain their storage identities. Thirty-four
new arrays, preset registry and first-run identities are generated together by
`tools/build_keychron_layouts.py`. UI uses display names without Imported. No
renaming/removal of user files; saved references, user edits and catalog indices
of existing presets are preserved. Exact first-run matching uses VID/PID, usage
page/usage and the complete matrix dimensions; saved/manual selection still wins.

## Source and reproducibility

- Launcher flat JSON: `https://launcher.keychron.com/static/device/{vpid}/json/v3.json`.
- Original download index:
  https://www.keychron.com/pages/firmware-and-json-files-of-the-keychron-he-series-keyboards
- QMK commits: `bc56b3c611dcc1a8ed9a2acb8bdc4da5e1a80c27` and
  `9ada9b7baecb9591c469b9b068146ac5891a480a` (official Keychron/qmk_firmware).
- Eleven factory tables extracted from official K3 HE / HE 8K firmware, not
  copied from similarly named ordinary HE boards. Different matrix dimensions
  and several moved matrix positions were found. `keychron_firmware_keymap.py`
  pins every image hash and table offset; validates product identity, base-table
  anchor, every occupied key, every empty matrix cell and unknown actions.
  QMK keycodes: `KC_AUDIO_MUTE=0x00A8`, `QK_MOMENTARY=0x5220`,
  `QK_UNDERGLOW_MODE_NEXT=0x7821`, verified against pinned `quantum/keycodes.h`.
- Source JSON/C and the eleven images plus manufacturer metadata (including
  original download URLs) are retained in `docs/research/keychron-catalog-sources/`.
  `sources.json` is the original ZIP-download manifest; its `.local` paths describe
  the download stage, not the inputs used by the offline generator.
- Per-layout `docs/exports/keychron-he/*-review.json` records all source hashes,
  coordinates, original key actions, exclusions and physical-code overrides.
  Generated UTF-16 INIs are offline exports, not extra copies in the user's folder.
- QMK source code and downloaded JavaScript/firmware are never executed.
  Knob push actions are excluded, not misrepresented as keyboard HID mute keys.
  Fn/RGB/assistant/OEM keys use existing HallJoy physical-code conventions;
  this is not a claim of hardware testing of each custom firmware.

Reproduce without network or devices:

```powershell
py tools/build_keychron_layouts.py --check
py -m unittest discover -s tools/tests -p test_layout_import.py
py -m unittest discover -s tools/tests -p test_keychron_catalog.py
py -m unittest discover -s tools/tests -p test_compound_overlay.py
```

`--update` is an explicit maintainer regeneration operation, after backup; it
snapshots existing generated contents and refuses concurrent changes before writes.
Default generation refuses overwrites. No runtime importing/downloading added.

## Compound geometry

One KeyDef, one HID, one history entry. `notchW/notchY` describe a bounding
rectangle with its bottom-left corner removed. Zero/zero is a legacy rectangle;
otherwise `0 < notchW < width` and `0 < notchY < height`. All observed ISO/JIS
Enter shapes use this outline (12 px inset, 40 px top arm at default scale).
Other polygons remain explicitly unsupported, not flattened to rectangles.

- Optional `NotchWn/NotchYn` INI fields, validated on load, atomic write validation
  and draft commit. Old files without fields remain rectangular. Invalid shapes
  cannot overwrite a good file. Older EXEs do not understand the new outline;
  do not use them to edit compound presets.
- Editor: contour fill/analogue clipping, shape-aware hit tests, grips exclude
  the absent corner; width/height resize preserves and validates the inset.
  Selected compound keys expose pixel-exact Inset and Top arm fields, using the
  existing dimension row. Shape changes participate in dirty comparison, grouped
  undo/redo, duplicate, save/discard and snapshot ownership.
- Main preview: Win32 polygon region created only on layout/shape changes, not
  on ticks. It clips the actual key HWND and its input region. Remap proximity
  uses the two real arms, not the missing corner. Labels/indicators fit the leg.
- Overlay: outline and fill share a canvas path. Geometry invalidation and the
  bounded sprite-cache key include notch dimensions. No additional timer, network
  request, frame loop or per-tick window-region allocation.

## Not finished: no invented mappings

- K6 HE ANSI/ISO, Q2 HE ANSI, Q4 HE ANSI and Q2 HE 8K ANSI: need the actual
  custom-firmware physical identity for the second Fn. Current UAP enum has one
  Fn; mapping both to it would create a collision. K6 ISO also lacks a matching
  factory source in the checked QMK branches. Owner asked for the conversion
  tool name/source; do not re-open the question of whether analogue exists.
- Q0 HE: firmware downloaded; factory table at image offset `0x20d20` found,
  including four macro keys and Fn. Need verified physical identities for the
  macro keys, rather than assuming macro actions are HID usages.
- J12 HE, J14 HE, Q16 HE 8K, C0 HE 8K: complete exact source set not found yet.
  Their presence in a store catalog is not sufficient to invent IDs or geometry.
- Additional regional 8K variants are not inferred from the ordinary HE model.
  Official page contains copied/mislabeled links; Q12 ISO was instead recovered
  from its real QMK PID and corresponding Launcher JSON. K3 JIS was discovered
  through the manufacturer API despite omission from the original page.

## Validation

Code-only: native and simulator builds; all static native audits; portable editor
math/history and per-identity matching; importer fixture/negative tests; complete
catalog source counts and unique codes; firmware tamper rejection; production
overlay JavaScript syntax and executed outline path. Windows private-desktop
tests exercise actual edit notifications, undo, scaled HWND regions, save/load,
invalid-shape save rejection, existing configuration transactions and Brand/Model.
No screenshots or visual approval claimed. Native release hash is recorded in
`build/release/SHA256SUMS.txt` after the final build.

Final native EXE: `1618566F32DA2FCDFAC83E9E54294EF6630441A8980AB593EDD6109C52F34F91`.
Profile/event suite PASS: `HJProfileTest-64031236fd524ad8a7c00df68fb7c458`
in the Windows temporary directory. 21 Python importer/catalog/overlay tests
PASS; native and simulator builds PASS (existing ViGEm PDB warning only).
