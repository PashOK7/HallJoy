# Public analog keyboard catalog: regional expansion — 2026-09-20

## Outcome
Main A1:C513 contains 440 records / 73 brands / 72 blank spacer rows.
Added 21 records: IROK 16, VTER 2, Durgod 1, MelGeek 1, Neo 1.
All new records use the existing allowed status Not investigated.
Analog sensing is confirmed by product/support material; usable USB depth protocols and HallJoy support are not established.

## Per-record evidence
- **VTER Fighting68**: Manufacturer download page supplies magnetic-switch Rapid Trigger firmware. [Source](https://www.vtergaming.com/download/fighting68/).
- **VTER Ato87**: Manufacturer download page supplies magnetic-switch Rapid Trigger firmware. [Source](https://www.vtergaming.com/download/ato87/).
- **Durgod K100 HE**: Gateron 20T magnetic switches and adjustable actuation; Icy/Silvery colors consolidated. K100w uses conventional Smoothie switches and is excluded. [Source](https://www.durgod.com/product/k100-silvery/).
- **MelGeek Centauri60**: Manufacturer lists Centauri60 and Centauri80 Hall Effect variants; Centauri80 already in catalog. [Source](https://www.melgeek.com/products/centauri-hall-effect-gaming-keyboard).
- **Neo Neo65 Sonic HE+**: Qwertykeys manufacturer listing confirms complete prebuilt Hall Effect keyboard; separate module SKU not duplicated. [Source](https://www.qwertykeys.com/collections/neo-product-collection/products/neo65-sonic-he-hall-effect-custom-keyboard).
- **IROK Mars68 SE**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mars68).
- **IROK Mars68**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mars68).
- **IROK Mars68 Pro**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mars68).
- **IROK Mars68 Wireless**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mars68).
- **IROK Mars68 Pro Wireless**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mars68).
- **IROK Mars75**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mars75).
- **IROK Mars75 Pro**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mars75).
- **IROK Mercury68 Lite**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mercury-68).
- **IROK Mercury68 SE**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mercury-68).
- **IROK Mercury68**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mercury-68).
- **IROK Mercury68 Pro**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mercury-68).
- **IROK Mercury68 Max**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-mercury-68).
- **IROK ND63**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-nd63).
- **IROK ND63 Pro**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-nd63).
- **IROK ND63 Max**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-nd63).
- **IROK ND63 Ultra**: IROK Argentina regional catalog explicitly names this Hall Effect variant; USB depth protocol not investigated. [Source](https://irokgaming.com.ar/products/irok-nd63).

## Identity decisions and deferred candidates
- IROK evidence is the regional IROK Argentina catalog, not a firmware investigation. Retain regional model names. Mercury68 means the Standard variant; do not duplicate as Mer68 without resolving aliases.
- Mars68 Wireless and Pro Wireless are separately named products in that catalog. Colors are not separate rows.
- Durgod K100 Icy and Silvery are consolidated as K100 HE. Manufacturer comparison explicitly distinguishes conventional K100w: https://www.durgod.com/product/k100-icy/
- Neo is the product brand sold by Qwertykeys. The Neo65 Sonic HE+ page offers a complete prebuilt board and a separate module; only the keyboard is included.
- VTER Jeet65/Pro deferred: official support mixes single-mode and QMK tri-mode variants. Downloaded page identifies a mechanical tri-mode firmware; exact magnetic/Pro/Plus naming needs clarification before adding separate rows. Official source: https://www.vtergaming.com/download/jeet65/
- GEONWORKS Venom results were PCB/module listings, not a newly verified complete keyboard. No rows added from those.
- REDMAGIC Nova magnetic tablet attachment and ordinary TTC Speed Silver mechanical keyboard are not analog-sensing evidence.
- Everglide SU75 family, JamesDonkey R3 TMR/A3 HE and Lenovo Legion R7 RT75 remain candidates requiring clearer primary product evidence. Everglide.co currently returns a directory index; its cached store claim is not sufficient.
- No claim of global or per-brand completeness.

## Verification
Fresh pre-write range matched baseline. Backup:
.local/backups/google-sheet-before-regional-expansion-20260920.json
Readback verified every written value and per-row validation, preservation of all 419 prior records,
73 brand-block outer borders and all 21 gray status fills.
Metadata, six conditional rules and grid dimensions unchanged.
Pwnage Zenblade 65 V2 retains its special status/validation at C402.
Owner evaluates visuals; API formatting checks used without browser automation.
No HallJoy runtime, firmware, build or support-status changes.
