# YUNZII RT75 Pro — tester log, approximate layout, Supported (2026-10-28)

## Tester evidence

HallJoy 1.6.11 support log (user "Klutch", 2026-10-28): the RT75 Pro connects
through the `rongyuan-stream` backend and streams live analog.

- USB `3151:5030`, board **2865** (`evidence.rongyuan reader board=2865 pid=5030`).
- `rongyuan-stream`: `observation=connected`, `present=1`, `connected=1`,
  `mapped_keys=81`, `updates` climbing `0 → 1570 → 2507 → 3564`, `failures=0`,
  `age_valid=1`. The user confirmed "seems to be working just fine".

This is the tester confirmation the model lacked. The board was already in the
rongyuan-stream model list (it worked in the log); the only thing missing for
Supported was a built-in layout.

## Why the exact vendor geometry is unavailable

Board 2865 (and 3755) use `keyLayout:u.Common81_RT75` / displayName "RT75PRO".
Its key-mapping component is `Keyboard_81_RT75_KeyMappings_RT`, 81 keys. In the
archived RongYuan desktop client it is loaded as an ESM module (`import.meta.url`)
from `figma组件_Keybord_unsafe/keyboards_keyMappings_rt/…`, a directory not
captured in any local client copy, so the plain SVG cannot be extracted (this is
the `plain key-mapping SVG absent or ambiguous` hold in the rongyuan inventory).
The CN/IOT clients carry only a different `yc3121_rt75*` object (85 keys, non-PRO)
whose key set does not match board 2865. The firmware exposes the electrical
matrix, not visual key coordinates, so geometry cannot be read from the device.

## Approximate layout (owner-approved 2026-10-28)

Board 2865's 81-key factory set equals a standard rongyuan 75% ANSI **minus the
Home key (HID 74)**. Dozens of bundled rongyuan 75% presets differ from it by
exactly that one key, and all share byte-identical geometry. Board 3755 has the
same 81-key set as 2865.

`tools/build_yunzii_rt75pro_layout.py` derives the preset deterministically:

- Geometry adapted from the identical-key-set sibling **Rampage ZENITH PRO**
  (`ry_layout_2310`) with the Home key removed.
- The resulting 81-key set is verified equal to the reviewed factory matrix of
  boards 2865 and 3755 in `profiles.json`.
- Positions are borrowed, not vendor-exact; no visual or physical test is claimed.
- Boards 3100 (`Common82_MK825`, 84 keys) and 2445 (`Common82_B75PRO`, 86 keys)
  are different layouts and are not covered.

Output: `docs/research/rongyuan-layouts/ry_layout_2865.json`
(name "YUNZII RT75 Pro ANSI", products `RY1B-2865`, `RY1B-3755`).

## Integration

- `tools/layout_catalog.json`: new brand **YUNZII**, model "RT75 Pro" (ANSI),
  source `ry_layout_2865.json`, count 81, products `RY1B-2865`, `RY1B-3755`.
- `tools/layout_pipeline.py integrate` regenerated `identities.inc` / `identities.h`
  (`RY1B-2865` and `RY1B-3755` → "YUNZII RT75 Pro ANSI"), `presets.inc`,
  `layouts.h` and the layout-pipeline exports.
- `docs/development/supported_layouts.json`: YUNZII RT75 Pro → "YUNZII RT75 Pro ANSI".
- `tools/research_reference_checks.py --record` re-recorded the manifest after the
  `layout_catalog.json` change (all original build-script audits passed).

## Checks

- `layout_pipeline.py check YUNZII`: PASS (81 keys, changed=0 after integrate).
- `build_yunzii_rt75pro_layout.py --check`: PASS.
- `run_native_backend_checks.py --require-compiler`: all static and portable C++
  tests passed.
- `build_release.ps1` (full catalog + profile transaction tests): PASS
  (`PROFILE_TRANSACTION_WINDOWS_TEST=PASS`, `LAYOUT_CATALOG=PASS`,
  `SUPPORTED_LAYOUTS=PASS models=83 catalog=True`, verified EXE installed). Run
  twice: once after the layout integration, once after the notice-catalog change.
- `check_supported_layouts.py --sheet`: PASS (`models=83 sheet=True`), the Sheet's
  Supported rows equal the listed models.

## Notice catalog

YUNZII RT75 Pro was an "Implemented; awaiting hardware testing" entry in
`keyboard_support_notices.json`; removed, and `support_notice_catalog.py --write`
regenerated `support_notice_catalog.h` (282 models). The release build does not
reject the header (consistent).

## Documentation + Sheet sync

- `docs/SUPPORTED_HARDWARE.md`: RT75 Pro moved from the experimental "Local
  additions" table to a Supported row (boards 2865/3755; approximate-layout note;
  3100/2445 noted as different revisions).
- `README.md`: RT75 Pro moved from "Experimental support" to the Supported list.
- Google Sheet Main!C813, YUNZII RT75 Pro: "Implemented; awaiting hardware testing"
  → "Supported". Readback confirms value "Supported", green conditional fill
  (RGB 0.659/0.867/0.710 on B813:C813), dropdown validation preserved, neighbours
  unchanged. Separate rows "RT75" and "RT80" ("No usable analog found") are
  different models and were left unchanged.
