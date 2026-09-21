# DAREU, Higround, Keydous and SIKAKEYB catalog — 2026-09-20

## Result

Added 26 records to the [canonical spreadsheet](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit): DAREU 11, Higround 1, Keydous 6, SIKAKEYB 8. Main is now A1:C373, 330 records, 43 brands, 42 spacer rows. Every addition is Not investigated. All previous 304 records and statuses are preserved, including the red Pwnage V2 firmware blocker (now C301).

Manufacturer sources establish magnetic sensing/configurations, not HallJoy protocol compatibility. No runtime changes or hardware tests.

## Sources

| Brand | Model | Source |
| --- | --- | --- |
| DAREU | A75 HE | [Manufacturer](https://dareu.com/collections/magnetic-keyboard) |
| DAREU | EK75 RT | [Manufacturer](https://dareu.com/collections/magnetic-keyboard) |
| DAREU | EK75RT 8K | [Manufacturer](https://dareu.com/collections/magnetic-keyboard) |
| DAREU | A68 | [Manufacturer](https://dareu.com/collections/keyboard) |
| DAREU | COOL60 | [Manufacturer](https://dareu.com/collections/keyboard) |
| DAREU | EK60 HE | [Manufacturer](https://dareu.com/collections/keyboard) |
| DAREU | GT87 TMR | [Manufacturer](https://dareu.com/collections/keyboard) |
| DAREU | ULTRA 75 | [Manufacturer](https://dareu.com/collections/keyboard) |
| DAREU | COOL68 | [Manufacturer](https://dareu.com/products/dareu-cool68) |
| DAREU | COOL68 Pro | [Manufacturer](https://dareu.com/products/dareu-cool68-pro) |
| DAREU | Ultra 68 (magnetic version) | [Manufacturer](https://dareu.com/products/dareu-ultra-68%E4%B8%A8dual-mode-gaming-keyboard-magnetic-inductive-switches-custom-display-8k-polling-rate) |
| Higround | Basecamp 65HE | [Manufacturer](https://higround.co/products/performance-base-65-keyboard-retrosuper) |
| Keydous | NJ80-CP V3 HE | [Manufacturer](https://keydous.store/collections/magnetic-swich-keyboards) |
| Keydous | NJ81-CP V3 HE | [Manufacturer](https://keydous.store/collections/magnetic-swich-keyboards) |
| Keydous | NJ98-CP V4 HE | [Manufacturer](https://keydous.store/collections/magnetic-swich-keyboards) |
| Keydous | NJ98-CP V4S TMR | [Manufacturer](https://keydous.store/collections/magnetic-swich-keyboards) |
| Keydous | NJ81 MAX-CP TMR (announced) | [Manufacturer](https://keydous.store/products/keydous-nj81-max-cp-tmr) |
| Keydous | NJ98-CP V3 | [Manufacturer](https://keydous.oss-cn-shenzhen.aliyuncs.com/%E8%AF%B4%E6%98%8E%E4%B9%A6/NJ98-CP%20V3%20%E8%AF%B4%E6%98%8E%E4%B9%A6%EF%BC%88%E4%B8%A4%E9%A1%B5%EF%BC%89.pdf) |
| SIKAKEYB | Castle HM66 | [Manufacturer](https://sikakeyb.com/collections/magnetic-keyboards) |
| SIKAKEYB | Castle HM80 | [Manufacturer](https://sikakeyb.com/collections/magnetic-keyboards) |
| SIKAKEYB | Castle HM80 HE | [Manufacturer](https://sikakeyb.com/collections/magnetic-keyboards) |
| SIKAKEYB | Castle CK75 / CK75 US | [Manufacturer](https://sikakeyb.com/collections/magnetic-keyboards) |
| SIKAKEYB | FNP75 HE | [Manufacturer](https://sikakeyb.com/collections/magnetic-keyboards) |
| SIKAKEYB | FORT75 | [Manufacturer](https://sikakeyb.com/collections/magnetic-keyboards) |
| SIKAKEYB | FORT68 | [Manufacturer](https://sikakeyb.com/products/sikakeyb-sakura-gift-box-gt68-ultra-magnetic-keyboard) |
| SIKAKEYB | Sakura Limited Edition (GT60 Pro) | [Manufacturer](https://sikakeyb.com/products/sikakeyb-sakura-limited-edition-magnetic-keyboard) |

## Findings and scope limits

- Reviewed current manufacturer catalogs, product pages and relevant manuals. Historical/global completeness is not claimed.
- DAREU Thunder 68 has inductive switches despite appearing in its magnetic category; excluded from this magnetic batch. Ultra 68 has magnetic and inductive options; the added row explicitly covers the magnetic version. COOL98, A104 Master and ordinary mechanical boards were not inferred magnetic from SOCD or polling rate.
- Higround Performance 65 / Performance Basecamp 65HE / Basecamp 65HE are represented by Basecamp 65HE. Licensed graphics and colorways do not create new model rows. Ordinary Basecamp, Summit and 75+/96+ were not inferred HE.
- SIKAKEYB has duplicate HM80 HE product pages. CK75 and CK75 US are shown together without separate regional rows. FORT68 uses GT68 Ultra wording in the product title; it is not counted twice. Sakura Limited Edition is a complete GT60 Pro keyboard, unlike a standalone case or PCB module.
- Keydous NJ81 MAX-CP TMR is explicitly labeled (announced): manufacturer page directs to Kickstarter launch notifications. Shipping or physical availability is not asserted.
- Keydous NJ98-CP V3 is confirmed by its manufacturer manual, which explicitly permits magnetic and mechanical switches. TMR/hybrid compatibility never implies analog sensing for installed conventional switches.
- Older NJ98-CP V2 and V3 store URLs now redirect to the V4 product. A redirect is not revision-specific proof. V3 was independently confirmed by its manual; older NJ80/NJ81/NJ98 revisions remain a historical follow-up rather than guessed additions.
- New status values use the existing strict Not investigated option. Existing per-row validation travels with its model, including Pwnage's extra option.

## Verification

Fresh native API read before write matched the snapshot; unused extension was empty. Backup: `.local/backups/google-sheet-before-four-brands-20260920.json`.

Readback verified all intended values and per-row validation, 26 neutral gray new records, 43 medium bordered brand blocks, preserved Pwnage red status, and the original 304 records. Conditional rules, column widths and header were not edited. No cell notes introduced; no browser automation or visual run.

[Machine-readable provenance](../research/keyboard-sheet-four-brands-20260920.json).

