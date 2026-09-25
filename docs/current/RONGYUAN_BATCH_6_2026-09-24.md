# RongYuan sixth support batch — 2026-09-24

## Result

11 models / 18 exact revisions / 8 brands enabled locally over wired USB.
Complete detection, independent analog input, bindings and virtual gamepad path;
yellow runtime notices for untested hardware and remaining switch-range nuances.
Manual layouts remain available. No hardware test claim, firmware flashing,
firmware emulation or forced logging. Keychron onboard unchanged. No publication.

| Brand | Model | Board | VID:PID | Source | Parent | Range, um |
|---|---|---|---|---|---|---|
| ATWO | GK7 MX | 3475 | 3151:5030 | 7561aea9.js | 7a5b12c9.js | 3300 |
| HAVIT | KB900L | 3570 | 3151:5029 | 7df3584d.js | 7a5b12c9.js | 3300 |
| HAVIT | KB904L | 3588 | 3151:5029 | 254f7ac4.js | 7a5b12c9.js | 3300 |
| UluGames | Howl 75 | 3437 | 3151:5029 | 5437795b.js | 4796d290.js | 4000 |
| UluGames | Howl 75 | 3403 | 3151:5029 | 362e6d41.js | 4796d290.js | 4000 |
| GamePro | MK160B MAX | 2930 | 3151:502D | 9acee80b.js | 7a5b12c9.js | 4000 |
| LOMZ | 75S | 2737 | 3151:5029 | 6d3a1aeb.js | 7a5b12c9.js | 4000 |
| LOMZ | 75S | 2828 | 3151:502D | 58dc141a.js | 7a5b12c9.js | 4000 |
| M4G | MAG 68 HE | 3017 | 3151:5029 | fedf7ba9.js | 7a5b12c9.js | 4000 |
| Fuego | GKB904 | 3439 | 3151:502D | 7328dcbc.js | 7a5b12c9.js | 3300 |
| XINMENG | Beat65 | 2326 | 3151:502D | 6e63726e.js | 7a5b12c9.js | 4000 |
| XINMENG | Beat65 | 2436 | 3151:502F | 6e63726e.js | 7a5b12c9.js | 4000 |
| XINMENG | Beat65 | 2535 | 3151:502D | 6e63726e.js | 7a5b12c9.js | 4000 |
| XINMENG | Beat68 | 2589 | 3151:502D | 5cd8e441.js | 7a5b12c9.js | 4000 |
| XINMENG | Beat68 | 2590 | 3151:502F | 35e88ab1.js | 7a5b12c9.js | 4000 |
| XINMENG | Beat68 | 2680 | 3151:502F | 35e88ab1.js | 7a5b12c9.js | 4000 |
| XINMENG | Beat75 | 2770 | 3151:5030 | 14698b15.js | 4796d290.js | 4000 |
| XINMENG | Beat75 | 2797 | 3151:502D | 14698b15.js | 4796d290.js | 4000 |

## Evidence and boundaries

Pinned Womier3.2.15 vendor records establish exact identities, magnetic status,
class loaders and factory matrices. All selected classes pass the known-body
review and inherit the byte-pinned shared command1B/report5 implementation.
Howl75 and Beat75 use the previously reviewed4796d290 auxiliary USB-version
reader subclass; its analog behavior is unchanged. Detection uses the exact
board/VID/PID tuple; shared USB IDs do not admit unrelated devices.

ATWO and HAVIT brand/model names are corroborated by their official product
catalogs; UluGames names Howl75 on its own product page. LOMZ official site
corroborates the brand's magnetic keyboard family; exact75S naming comes from
its OEM records, including LOMZHUBWEB. GamePro MK160B MAX, M4G MAG68HE, Fuego
GKB904 and XINMENG Beat65/68/75 identities are grounded directly in the OEM
company/displayName/internal-name records. Do not describe these as independently
downloaded firmware images. Third-party search snippets were not used as proof
of protocol behavior. No extension to GamePro mechanical MK160B/Pro, LOMZ75X,
XINMENG Ultra/Elite variants or other namesakes.

Primary public corroboration:
- https://en.atwo.co.kr/category/wired-keyboard/107/ (GK7 MX magnetic model)
- https://havitsmart.com/collections/gaming-keyboards (KB900L and KB904L magnetic models)
- https://ulugames.com.tr/products/ulugames-howl-75-manyetik-switch-klavye (Howl75)
- https://lomzsport.com/ (brand magnetic family; exact75S from pinned OEM records)

ATWO3475, HAVIT3570/3588 and Fuego3439 explicitly set travel.max3.3mm in the
vendor record, used as3300um normalization. Other models use provisional4000um,
not measured physical stroke or effective resolution. Tri-mode variants are
admitted only through the wired USB backend. Stationary deltas retain values
until explicit release or disconnect; no polling expiry introduced.

Beat68 revision2671 was deliberately excluded: its switch list includes
mechanical switches and its internal name says Elite. A new generic inventory
hold now catches this clue automatically. No claim that all switches/positions
on that revision are analog. Other selected Beat68 revisions lack that option.
The auto inventory is a candidate filter, never a support conclusion by itself.

Deferred: Ninjadog Varna Atlas and MiningBase AMGK80-001 lack sufficiently clear
retail mapping in the sources checked; MiningBase may alias existing ASTROMEDA.
TITAN60 has a name/map-size ambiguity, Storm68 retail scope unclear. Existing
VGN/KEYCOOL/hybrid/placeholder holds remain. No gray row was promoted by name alone.

## Automation and checks

Used tools/rongyuan_batch.py with reviewed manifest and guarded backup/apply.
Fixed generator ordering when appending models to an existing README brand.
Added mechanical-switch-option hold. Pipeline9 tests PASS. Generic sheet
regressions2 PASS: omitted empty cells allowed, missing model status rejected.
Google omits trailing empty CellData, so the structure auditor now pads these
before checking; this does not hide missing values in populated model rows.

Source audit PASS128 RongYuan models /201 revisions. Native protocol regression
PASS all profiles, factory mappings, identities, independent values, alias
merging, stationary hold, release, disconnect, malformed data and precision;
4898 existing captured frames are not captures from newly added devices.
Ordinary Release plus all six linked-image checks PASS. Existing ViGEm PDB
linker warning remains nonfatal. Installed EXE:
`build/bin/Release/x64/HallJoy.exe`
SHA256: a576807f529a71065384dab4a2e0b6351e18e619ec9dec73db884c6cdb63681c
Build log: `.local/rongyuan-batch6-build.log`.

README, hardware inventory, runtime notice catalog and next release notes updated.
Live Sheet Main received one atomic67-request batch following a fresh unchanged
native-snapshot check. Added11 model rows and7 separator rows, explicit32px model
heights/16px separator heights and complete affected-block border calculation.
Strict existing dropdown permits `Implemented; awaiting hardware testing`.
No cell comments/notes or new validation choices.

Readback: all707 preexisting returned rows retain values, validation, colors and
formatting except authorized XINMENG block borders. Column widths unchanged.
All188 yellow models match runtime; structure PASS130 blocks,0 issues.
Totals595 models:63 green /188 yellow /340 gray-research /4 blocked.
Grid1206 rows. Metadata verification only; owner handles visuals.

Artifacts:
- `.local/rongyuan-batch6-reviewed.json`, `.local/rongyuan-batch6-package.json`
- `.local/backups/before-rongyuan-batch6-20260924.zip`
- `.local/backups/sheet-before-rongyuan-batch6.packed.json`
- `.local/rongyuan-batch6-sheet-requests.json`
- `.local/rongyuan-batch6-sheet-native.packed.json`
- `.local/rongyuan-batch6-sheet-yellow.json`
