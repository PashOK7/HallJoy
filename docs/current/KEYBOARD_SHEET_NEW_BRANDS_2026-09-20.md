# Additional magnetic keyboard brands — 2026-09-20

> Follow-up owner decision: Pwnage Zenblade 65 V2 now uses the red status `Research blocked: V2 firmware unavailable` (Main C280). Its firmware has not been recovered; the reviewed host software exposes no live-depth getter. This is a research blocker, not proof that analog export is impossible. The original Zenblade 65 remains separate: its V0022 firmware was recovered. The red conditional rule includes the new exact status, and C280 validation accepts it. No other model status changed.

## Result

Added 38 model records across seven new brands to the [canonical Google Sheet](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit). Main now occupies A1:C343: 304 records, 39 brands, 38 blank spacer rows. All 266 prior records and statuses are preserved.

Every addition uses **Not investigated**. Manufacturer evidence establishes magnetic sensing, not an accessible USB depth protocol or HallJoy support. No runtime changes or hardware tests were performed. This batch does not establish worldwide or per-brand historical completeness.

## Source-backed additions

| Brand | Model | Manufacturer source |
| --- | --- | --- |
| Womier | M68 HE | [Source](https://womierkeyboard.com/collections/hall-effect-keyboards) |
| Womier | M68 HE V2 | [Source](https://womierkeyboard.com/collections/hall-effect-keyboards) |
| Womier | M68 HE Pro | [Source](https://womierkeyboard.com/collections/hall-effect-keyboards) |
| Womier | SK61 HE | [Source](https://womierkeyboard.com/collections/hall-effect-keyboards) |
| Womier | SK75 TMR | [Source](https://womierkeyboard.com/collections/hall-effect-keyboards) |
| Womier | WD75 HE | [Source](https://womierkeyboard.com/collections/hall-effect-keyboards) |
| YUNZII | RT68 | [Source](https://www.yunzii.com/collections/magnetic-keyboard) |
| YUNZII | RT75 | [Source](https://www.yunzii.com/collections/magnetic-keyboard) |
| YUNZII | RT75 Pro | [Source](https://www.yunzii.com/collections/magnetic-keyboard) |
| YUNZII | RT80 | [Source](https://www.yunzii.com/collections/magnetic-keyboard) |
| IQUNIX | EZ60 | [Source](https://iqunix.com/pages/iqunix-ez60-ez63-magnetic-switch-gaming-he-keyboard) |
| IQUNIX | EZ63 | [Source](https://iqunix.com/pages/iqunix-ez60-ez63-magnetic-switch-gaming-he-keyboard) |
| LUMINKEY | Magger60 HE | [Source](https://www.luminkey.com/collections/hall-effects-magnetic-keyboards) |
| LUMINKEY | Magger68 HE Performance | [Source](https://www.luminkey.com/collections/hall-effects-magnetic-keyboards) |
| LUMINKEY | Magger68 HE Professional | [Source](https://www.luminkey.com/collections/hall-effects-magnetic-keyboards) |
| LUMINKEY | Magger68 HE Plus | [Source](https://www.luminkey.com/collections/hall-effects-magnetic-keyboards) |
| LUMINKEY | Magger68 HE Ultra | [Source](https://www.luminkey.com/collections/hall-effects-magnetic-keyboards) |
| LUMINKEY | LUMINKEY65 HE Ultra | [Source](https://www.luminkey.com/collections/hall-effects-magnetic-keyboards) |
| LUMINKEY | LUMINKEY75 V2 HE | [Source](https://www.luminkey.com/collections/hall-effects-magnetic-keyboards) |
| LUMINKEY | Titan75 HE | [Source](https://www.luminkey.com/collections/hall-effects-magnetic-keyboards) |
| Meletrix | BOOG75 | [Source](https://meletrix.com/pages/boog75) |
| Meletrix | Zoom75 HE | [Source](https://meletrix.com/pages/boog75) |
| Meletrix | Zoom65 V3 HE | [Source](https://meletrix.com/products/zoom65-v3-he-module) |
| Arbiter Studio | Polar 65 HE | [Source](https://arbiterstudio.com/collections/keyboards) |
| Arbiter Studio | Polar 65 Pro | [Source](https://arbiterstudio.com/collections/keyboards) |
| Arbiter Studio | Polar 75 Pro | [Source](https://arbiterstudio.com/collections/keyboards) |
| Arbiter Studio | Polar 65+ HE | [Source](https://arbiterstudio.com/collections/keyboards) |
| Arbiter Studio | Polar 75+ HE | [Source](https://arbiterstudio.com/collections/keyboards) |
| Arbiter Studio | ARC 65 HE | [Source](https://arbiterstudio.com/collections/keyboards) |
| Glorious | GMMK 3 HE 65% | [Source](https://www.gloriousgaming.com/pages/hall-effect-technology) |
| Glorious | GMMK 3 Pro HE 65% | [Source](https://www.gloriousgaming.com/en-ca/pages/guide-gmmk3-pro-he) |
| Glorious | GMMK 3 Pro HE Wireless 65% | [Source](https://www.gloriousgaming.com/en-ca/pages/guide-gmmk3-pro-he) |
| Glorious | GMMK 3 HE 75% | [Source](https://www.gloriousgaming.com/pages/hall-effect-technology) |
| Glorious | GMMK 3 Pro HE 75% | [Source](https://www.gloriousgaming.com/en-ca/pages/guide-gmmk3-pro-he) |
| Glorious | GMMK 3 Pro HE Wireless 75% | [Source](https://www.gloriousgaming.com/en-ca/pages/guide-gmmk3-pro-he) |
| Glorious | GMMK 3 HE 100% | [Source](https://www.gloriousgaming.com/pages/hall-effect-technology) |
| Glorious | GMMK 3 Pro HE 100% | [Source](https://www.gloriousgaming.com/en-ca/pages/guide-gmmk3-pro-he) |
| Glorious | GMMK 3 Pro HE Wireless 100% | [Source](https://www.gloriousgaming.com/en-ca/pages/guide-gmmk3-pro-he) |

## Interpretation and exclusions

- Womier M68 HE Pro's title says 60%, while its detailed specification says 67 keys plus knob and ANSI 65%. The catalog omits a percentage rather than propagating this discrepancy. Its detailed page explicitly specifies Haimu Ice Jade magnetic switches: https://womierkeyboard.com/collections/hall-effect-keyboards/products/womier-m68he-pro-60-mechanical-gaming-keyboard
- Womier SK75 TMR and Glorious HE boards can accept conventional switches; this does not make conventional switches analog. Ordinary SK75 and non-HE GMMK models are excluded.
- Meletrix Zoom65 V3 HE denotes the keyboard configured with the optional HE module; the ordinary mechanical board is not claimed magnetic. Flux60 was found as a module, so no separate complete keyboard was inferred.
- LUMINKEY65 HE Ultra is a manufacturer-listed magnetic barebones board. Separate colors, regional layouts and prebuilt/barebones duplicates were not added.
- Arbiter Studio's TENKO-marked Polar+/ARC products are cataloged under their manufacturer/store brand. Cosmetic collaborations and switch-only Polar 65 Dual Rail editions were not duplicated.
- YUNZII RT80 duplicate listings and RT75 JIS were not duplicated. MADLIONS collaboration listings were not added as new YUNZII hardware.
- Glorious sizes and wired/wireless Pro variants have distinct product configurations and are separate records.
- Ducky One X search results identify inductive sensing, so it was not silently classified as Hall/TMR magnetic. VXE V75X magnetic charging-dock wording likewise is not evidence of magnetic key sensing.
- Other candidates such as AJAZZ and additional IQUNIX models remain for a future primary-source pass; this is not a finding that they lack magnetic products.

## Verification and recovery

Native Sheets API readback confirms exact intended values, preservation of all 266 existing records, 38 gray additions with strict status validation, 39 medium bordered blocks and hidden repeated brand names. No cell notes were introduced. Existing conditional rules, header and widths remain unchanged. No browser automation or visual run.

Before-state: `.local/backups/google-sheet-before-new-brands-20260920.json`.
Machine-readable additions: [source manifest](../research/keyboard-sheet-new-brands-20260920.json).
