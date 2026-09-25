# All experimental models: easy layout batch — 2026-09-24

Owner clarified that the task covers ALL251 current yellow models, not just
IROK. Keep only straightforward source-proven geometry; incomplete models may
retain manual generic layout selection. No guessed layout from key count.

## Implemented

41 additional models /26 brands:39 RongYuan models covering56 exact board
revisions plus Neo65 Sonic HE+ ANSI and Redragon K617 HE ANSI. These form34 new
presets because identical geometry is grouped within each brand. Exact runtime
identity tokens drive automatic selection. Neo identity strings are normalized
to the existing uppercase catalog convention; no polling or firmware changes.
ISO Neo65 and Brazilian K617 shapes are deliberately deferred in this simple
rectangular batch. Their analog support remains enabled and unchanged.

The archived official ATTACK SHARK OEM client contains other brands' registry
entries. Exact board/VID/PID and keyLayout are cross-checked against the separately
pinned WOMIER admission catalog. The extraction follows the actual component to
its SVG, and admits only exact full factory HID-set matches and non-overlapping
rectangular geometry. No remapped HID substitution to force a match. Vendor code
is parsed as evidence, never executed. Earlier blanket wording that RongYuan
geometry was absent was incorrect: the WOMIER archive lacks many components,
but the other official client provides this ready subset.

| Brand | Models included (ANSI; exact admitted revisions only) |
| --- | --- |
| AJAZZ | AK680 MAX HE, ALUX60, ALUX68 AIR, ALUX68 PRO, NS67, NS67 PRO |
| Akko | MOD007B V3 HE, MOD007S V3 HE, Ray68 HE |
| ASTROMEDA | AMGK80-001 |
| ATWO | GK7 MX |
| EPOMAKER | HE60 Lite, HE60 Wired, HE65 Mag, HE68 Lite |
| EWEADN | SEEK75, ZAP68 SE, ZAP87 HE |
| FL ESPORTS | MK870 HE, X80 HE |
| Funbey | AST V68, Coke V68 |
| Fury | Kanabo K6 |
| GamaKay | TK75 HE |
| GamePro | MK160B MAX |
| Koda | A68 |
| KYSONA | KM82 HE |
| MAMBASNAKE | M82 HE |
| MEETION | Magic A75 |
| MICROPACK | K-68M |
| MonsGeek | FUN68 HE |
| Neo | Neo65 Sonic HE+ |
| Rampage | ZENITH PRO |
| Redragon | K617 HE |
| SAVIO | ASTRAL |
| Skyloong | GK61 HE |
| Sunsonny | N-J100 |
| UluGames | Howl 75 |
| Womier | SK61 HE |
| XINMENG | Beat75, X87 TMR |

## Full-catalog result and intentional gaps

The reproducible `tools/audit_yellow_layouts.py` checks every experimental entry
against the production-compiled TSV and exact RongYuan tokens. Final snapshot:
`docs/research/yellow-layout-coverage-20260924.json`.

- 56 models have named presets outside the RongYuan stream group (regional
 coverage/autoselection is not inferred merely from a matching name).
- 35 RongYuan models have presets for all currently admitted revisions.
- 6 RongYuan models are only partially covered: EPOMAKER HE68 Lite/HE60 Wired,
 Akko MOD007B V3 HE/MOD007S V3 HE, ASTROMEDA AMGK80-001, EWEADN ZAP87 HE.
- 154 models remain without an exact named preset. Analog support is unchanged.
 Thus97 models have at least one model-specific preset, not97 fully covered
 regional families. The two new ANSI-only models also retain regional gaps.

Skipped RongYuan revisions are recorded individually in
`docs/research/rongyuan-layouts/inventory.json`: missing/unbundled components,
factory HID mismatch, changed cross-client layout or absent exact identity.
Do not override those gates to increase the count. Remaining EWEADN SparkLink
geometry uses runtime layout plus model-dependent row/spacing transformations
(module16139 and Gt renderer in the pinned HUB); it is not a ready literal
per-model preset. Other IROK V2 selectors use different addressing/inheritance;
the V1 extractor cannot simply be applied to them. Tartarus has protocol/key
identity evidence but no pinned complete physical geometry. These remain held,
not claimed impossible or unsupported.

## Validation and maintenance

Generator checks are wired into the native static audit. Production-linked
profile tests now always write a coverage report for ALL current yellow models;
missing layouts are allowed and visible, not silently equated to full UX.
The K673 regional regression now selects its actual K673 models instead of
assuming the Redragon brand can never gain a fourth layout.

Source/geometry extraction PASS. Production-linked catalog/remap tests and
18 recovery scenarios PASS. Full static suite and final ordinary Release with mandatory gates PASS.
Compiled catalog:183 source variants /151 visible groups. All149 prior presets
retain identical key geometry. EXE SHA256: fa0c8b0f70dbba987afa691c21ad6e9560f687090579e22b621272a135d92588.
No visual/hardware testing or GitHub publication. No analog support status
change: README yellow list, runtime notices and Sheet statuses are unchanged;
no Sheet mutation or fresh Sheet-sync claim in this layout-only task.

Backups: `.local/backups/all-yellow-layouts-20260924-181616.zip`,
`.local/backups/all-yellow-layout-followup.zip`, automatic `layout-integrate-*`.
Evidence: `.local/all-yellow-layout-profile-final-20260924.log`,
`.local/all-yellow-layout-static-final-20260924.log`,
`.local/all-yellow-layout-release-final-20260924.log`.
