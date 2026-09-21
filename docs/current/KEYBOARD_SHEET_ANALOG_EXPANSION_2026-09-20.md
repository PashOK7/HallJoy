# Analog catalog expansion — 2026-09-20

## Owner decision

The owner explicitly approved including ALL analog keyboards, not just Hall Effect/TMR magnetic models. Inductive sensing must be identified as such, not mislabeled magnetic.
This scope decision does not establish a usable USB analog protocol or HallJoy support.

## Result

Main A1:C451: 391 model/configuration records, 60 brands, 59 blank separators.
Added 14 entries, all Not investigated: ARDOR GAMING 2, Royal Kludge 5, Rapoo 4, Valkyrie 2, Thunderobot 1.
[Per-model source manifest](../research/keyboard-sheet-analog-expansion-20260920.json).

Rapoo A67, A83, ESK900-75RT and ESK900-83RT explicitly use inductive sensing. Manufacturer pages describe adjustable Rapid Trigger and depth precision. Sheet names include (inductive).
Royal Kludge C68/C84/C87/C96/C98 HE are explicitly listed as magnetic in the official store catalog. Do not transfer this to ordinary RK/R/S-series mechanical keyboards.
Valkyrie VK Mag75 and VK99 Gaming Naruto have explicit magnetic manufacturer driver listings. VK75 and VK99 mechanical models are different. Pro/Lite revisions of Mag75 remain a follow-up naming question, not assumed identical hardware.
Thunderobot VIC68 is present in the official global catalog; the image-only product description is corroborated by magnetic Gateron switch retail specifications. VIC68 SE and KT78 remain candidates requiring a separate source check.
ARDOR Radiant and Viper have explicit manufacturer magnetic specifications.
Rapoo 9560M's magnetic mouse scroll wheel and V500PRO's magnetic removable faceplate do not establish analog keyboard switches; excluded.
Colorways, regional layouts and accessory bundles are not additional records.

## Verification and preservation

Backup: .local/backups/google-sheet-before-analog-expansion-20260920.json.
Fresh read matched baseline before writing. Native API readback verified all 377 existing triples unchanged, every per-row validation retained, all 14 intended additions gray, and medium outer borders for all 60 brand blocks.
Pwnage V2 status is unchanged at C347; special validation retained.
Header, widths and conditional rules retained; single blank row between brands.
No runtime or build changes; no completeness claim.
