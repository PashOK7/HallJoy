# Public keyboard catalog expansion — 2026-09-20

> Follow-up: [magnetic-model audit and owner corrections](KEYBOARD_SHEET_MAGNETIC_AUDIT_2026-09-20.md) supersede the original research statuses below. Ace 68 / TITAN 68 Turbo now No usable analog found; all frozen models now Research frozen; tester needed, including MG75 V2. The table below preserves the initial expansion snapshot.

## Scope and result

The owner requested continuing the all-brand magnetic-keyboard catalog and adding previously investigated unusable analog routes, including IO. This is a catalog update, not a HallJoy implementation or hardware-test claim.

[Canonical Google Sheet](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit), Main sheetId 0: **266 model records across 32 brands**, A1:C298 including header and 31 brand spacers. Added 149 records; preserved all 117 existing records and their statuses. Earlier passes: [ATK](ATK_SHEET_CATALOG_2026-09-20.md), [ATTACK SHARK](ATTACK_SHARK_SHEET_CATALOG_2026-09-20.md).

This is not yet an exhaustive worldwide catalog. Regional brands, discontinued models and exact revision aliases require further source review. Existing optical-keyboard entries remain; additions include magnetic HE and TMR models. Colorways, regional layouts and barebones variants are not automatically separate records. Official announced/preorder models can be included; presence is not a shipping or compatibility claim.

## Status interpretation

- Not investigated: verified model existence, no model-specific HallJoy investigation recorded in this pass.
- Research incomplete: prior work exists, but suitable gameplay support has not been established.
- Research frozen: disabled research retained, not enabled by this catalog update.
- No usable analog found (FW 1.17): IO Type 84 Magnetic. The reviewed firmware provides a selected/deepest-key feed, not the required independent multi-key feed. This does not prove the absence of every undocumented mechanism or permanent impossibility in future firmware.
- Existing supported and implemented/untested statuses remain unchanged.

Research incomplete/frozen use neutral gray alongside Not investigated. The scoped IO Type 84 result uses red. No source notes are placed in cells; provenance is retained in the [addition manifest](../research/keyboard-sheet-expansion-20260920.json).

## Research distinctions

IO Type 68 Magnetic has only preliminary comparison evidence, so it is Research incomplete, not assigned the Type 84 conclusion. See [IO deep review](../firmware/io-type84-magnetic/DEEP_REVERSE.md).

Pwnage Zenblade 65 and V2 remain Research incomplete; a missing V2 firmware or missing host-side getter is not proof of impossibility. See [Pwnage review](../v1.4/PWNAGE_ZENBLADE_65_V2_PROTOCOL_RECON.md).

MCHOSE Ace68 remains Research incomplete. Internal Ace68 Pro / Ace68-II identifiers must not be treated as separately verified retail names without evidence. Ace68 V2 is a catalog model, with no inferred USB identity. See [Ace68 assessment](../firmware/mchose-ace68/PROTOCOL_ASSESSMENT.md) and [other revision review](../firmware/mchose-ace68-pro/DEEP_REVERSE.md).

IROK NA87 Pro / ND75 and ASUS ROG Azoth 96 HE remain Research frozen. IROK MG75 V2 remains Research incomplete, not permanently unsupported; its existing README omission is unchanged. See [MG75 review](MG75_PRO_V2_REVIEW_2026-09-19.md).

MADLIONS TITAN68 Turbo has a diagnostic transport candidate but no established gameplay integration; Research incomplete. Newly listed Keychron models do not change the accepted custom-firmware support for previously implemented models. No additional ATTACK SHARK protocol investigation was performed.

## Data and presentation verification

Native Sheets batch update only; no browser automation. Before-state backup: .local/backups/google-sheet-before-catalog-expansion-20260920.json.

Readback verified 266 records, all 117 old records preserved exactly, 32 complete outer brand borders, 31 spacers, repeated raw brand values with conditional visual hiding, effective red/gray IO colors, and zero cell notes. Strict status validation was extended with the three research statuses. Header, column widths, three-column structure and existing supported status colors retained. Borders are static and must be regenerated after future structural edits. Consumers must skip rows with empty model values and read raw brand values.

## Added records and provenance

| Brand | Model | Status | Source |
| --- | --- | --- | --- |
| AULA | AG60 | Not investigated | https://www.aulastar.com/magnetic-keyboard/ |
| AULA | AG75 | Not investigated | https://www.aulastar.com/magnetic-keyboard/ |
| AULA | F75 HE | Not investigated | https://www.aulastar.com/magnetic-keyboard/ |
| AULA | HERO 68 HE | Not investigated | https://www.aulastar.com/magnetic-keyboard/ |
| AULA | HERO 68 HE Ultra | Not investigated | https://www.aulastar.com/magnetic-keyboard/ |
| AULA | HERO 75 HE | Not investigated | https://www.aulastar.com/magnetic-keyboard/ |
| AULA | HERO 99 HE | Not investigated | https://www.aulastar.com/magnetic-keyboard/ |
| AULA | MINI 60 HE | Not investigated | https://www.aulastar.com/magnetic-keyboard/ |
| DrunkDeer | A75 Ultra | Not investigated | https://drunkdeer.com/products.json?limit=250 |
| DrunkDeer | X60 HE | Not investigated | https://drunkdeer.com/products.json?limit=250 |
| DrunkDeer | X60 Future | Not investigated | https://drunkdeer.com/products.json?limit=250 |
| Keychron | C0 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | C3 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | C4 HE 8K | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | J2 HE 8K | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | J4 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | J5 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | J8 HE 8K | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | J12 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | J14 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | J15 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | K6 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | Q0 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | Q2 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | Q2 HE 8K | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | Q4 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | Q16 HE 8K | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | V1 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | V6 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Keychron | V6 Ultra Hybrid 8K | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Lemokey | L0 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Lemokey | L1 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Lemokey | L5 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Lemokey | P2 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| Lemokey | P3 HE | Not investigated | https://www.keychron.com/products.json?limit=250 |
| NuPhy | BH65 | Not investigated | https://nuphy.com/pages/user-manual |
| NuPhy | Field75 HE | Not investigated | https://nuphy.com/pages/user-manual |
| NuPhy | Field75 HE V2 | Not investigated | https://nuphy.com/pages/user-manual |
| NuPhy | Halo65 HE | Not investigated | https://nuphy.com/pages/user-manual |
| NuPhy | WH80 | Not investigated | https://nuphy.com/pages/user-manual |
| IO | Type 84 Magnetic | No usable analog found (FW 1.17) | docs/firmware/io-type84-magnetic/DEEP_REVERSE.md |
| IO | Type 68 Magnetic | Research incomplete | docs/firmware/io-type84-magnetic/DEEP_REVERSE.md |
| Pwnage | Zenblade 65 | Research incomplete | docs/v1.4/PWNAGE_ZENBLADE_65_V2_PROTOCOL_RECON.md |
| Pwnage | Zenblade 65 V2 | Research incomplete | docs/v1.4/PWNAGE_ZENBLADE_65_V2_PROTOCOL_RECON.md |
| IROK | NA87 Pro | Research frozen | docs/current/FROZEN_SUPPORT_NOTICES_2026-09-15.md |
| IROK | ND75 | Research frozen | docs/current/FROZEN_SUPPORT_NOTICES_2026-09-15.md |
| IROK | MG75 V2 | Research incomplete | docs/current/MG75_PRO_V2_REVIEW_2026-09-19.md |
| MCHOSE | Ace 60 | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | Ace 60 Pro | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | GOD 60 | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | Ace 68 V2 | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | Ace 68 Air | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | Ace 68 GT | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | Ace 68 Turbo | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | Jet 75 | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | Ace 75 | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | Mix 87 | Not investigated | https://support.mchose.store/hc/en-us/articles/51976583851028-MCHOSE-Compliance-and-Documents-of-Conformity |
| MCHOSE | Ace 68 | Research incomplete | docs/firmware/mchose-ace68/PROTOCOL_ASSESSMENT.md |
| ASUS ROG | Azoth 96 HE | Research frozen | docs/current/FROZEN_SUPPORT_NOTICES_2026-09-15.md |
| ASUS ROG | Falchion Ace HFX | Not investigated | https://rog.asus.com/us/keyboards/keyboards/pbt-keycaps/rog-falchion-ace-75-he/overview/rog-keyboard-switch/ |
| ASUS ROG | Falchion Ace 75 HE | Not investigated | https://rog.asus.com/us/keyboards/keyboards/pbt-keycaps/rog-falchion-ace-75-he/overview/rog-keyboard-switch/ |
| ASUS ROG | Falcata | Not investigated | https://rog.asus.com/us/keyboards/keyboards/pbt-keycaps/rog-falchion-ace-75-he/overview/rog-keyboard-switch/ |
| Logitech G | PRO X TKL RAPID | Not investigated | https://www.logitechg.com/en-in/shop/p/pro-x-tkl-rapid |
| Corsair | K70 MAX | Not investigated | https://www.corsair.com/us/en/c/keyboards/mgx-switch-keyboards |
| Corsair | K70 PRO TKL | Not investigated | https://www.corsair.com/us/en/c/keyboards/mgx-switch-keyboards |
| Corsair | VANGUARD PRO 96 | Not investigated | https://www.corsair.com/us/en/c/keyboards/mgx-switch-keyboards |
| SteelSeries | Apex Pro Gen 3 | Not investigated | https://steelseries.com/press/152-steelseries-expands-its-apex-pro-gen-3-series-keyboard-lineup |
| SteelSeries | Apex Pro TKL Gen 3 | Not investigated | https://steelseries.com/press/152-steelseries-expands-its-apex-pro-gen-3-series-keyboard-lineup |
| SteelSeries | Apex Pro TKL Wireless Gen 3 | Not investigated | https://steelseries.com/press/152-steelseries-expands-its-apex-pro-gen-3-series-keyboard-lineup |
| SteelSeries | Apex Pro Mini Gen 3 | Not investigated | https://steelseries.com/press/152-steelseries-expands-its-apex-pro-gen-3-series-keyboard-lineup |
| SteelSeries | Apex Pro TKL | Not investigated | https://support.steelseries.com/hc/en-us/sections/9637593545229-Apex-Pro-TKL-2023 |
| SteelSeries | Apex Pro TKL (2023) | Not investigated | https://support.steelseries.com/hc/en-us/sections/9637593545229-Apex-Pro-TKL-2023 |
| SteelSeries | Apex Pro TKL Wireless (2023) | Not investigated | https://support.steelseries.com/hc/en-us/sections/9637593545229-Apex-Pro-TKL-2023 |
| SteelSeries | Apex Pro Mini | Not investigated | https://support.steelseries.com/hc/en-us/sections/7087574605325-Apex-Pro-Mini-Mini-Wireless |
| SteelSeries | Apex Pro Mini Wireless | Not investigated | https://support.steelseries.com/hc/en-us/sections/7087574605325-Apex-Pro-Mini-Mini-Wireless |
| MonsGeek | M1 HE | Not investigated | https://www.monsgeek.com/download/ |
| MonsGeek | M1W-SP HE | Not investigated | https://www.monsgeek.com/download/ |
| MonsGeek | MG75S HE | Not investigated | https://www.monsgeek.com/download/ |
| MonsGeek | M1 V5 TMR | Not investigated | https://www.monsgeek.com/download/ |
| MonsGeek | M3 V5 HE | Not investigated | https://www.monsgeek.com/download/ |
| MonsGeek | FUN68 HE | Not investigated | https://www.monsgeek.com/download/ |
| MonsGeek | Verve68 HE | Not investigated | https://www.monsgeek.com/download/ |
| MonsGeek | FUN60 Pro | Not investigated | https://www.monsgeek.com/monsgeek-magnetic-switches-knowledge-base/ |
| MonsGeek | FUN60 Max | Not investigated | https://www.monsgeek.com/monsgeek-magnetic-switches-knowledge-base/ |
| MonsGeek | FUN60 Ultra | Not investigated | https://www.monsgeek.com/monsgeek-magnetic-switches-knowledge-base/ |
| MonsGeek | FUN60 Ultra TMR | Not investigated | https://www.monsgeek.com/monsgeek-magnetic-switches-knowledge-base/ |
| MelGeek | MADE68 | Not investigated | https://www.melgeek.com/collections/magnetic-gaming-keyboard |
| MelGeek | MADE68 Air | Not investigated | https://www.melgeek.com/collections/magnetic-gaming-keyboard |
| MelGeek | MADE68 Pro | Not investigated | https://www.melgeek.com/collections/magnetic-gaming-keyboard |
| MelGeek | MADE68 Ultra | Not investigated | https://www.melgeek.com/collections/magnetic-gaming-keyboard |
| MelGeek | MADE68 Ultra V2 | Not investigated | https://www.melgeek.com/collections/magnetic-gaming-keyboard |
| MelGeek | MADE84 Pro | Not investigated | https://www.melgeek.com/collections/magnetic-gaming-keyboard |
| MelGeek | MADE84 Ultra | Not investigated | https://www.melgeek.com/collections/magnetic-gaming-keyboard |
| MelGeek | Centauri 80 | Not investigated | https://www.melgeek.com/collections/magnetic-gaming-keyboard |
| EPOMAKER | HE30 | Not investigated | https://epomaker.com/pages/he-series |
| EPOMAKER | HE68 Lite | Not investigated | https://epomaker.com/pages/he-series |
| EPOMAKER | HE75 V2 TMR | Not investigated | https://epomaker.com/pages/he-series |
| EPOMAKER | HE80 | Not investigated | https://epomaker.com/pages/he-series |
| EPOMAKER | G84 HE | Not investigated | https://epomaker.com/pages/he-series |
| MADLIONS | TITAN 68 Turbo | Research incomplete | docs/firmware/madlions-titan68-turbo/STATIC_AUDIT.md |
| MADLIONS | MAD60 V2 | Not investigated | https://madlionskeyboard.com/ |
| MADLIONS | MAD60 Pro | Not investigated | https://madlionskeyboard.com/ |
| MADLIONS | MAD LIGHT 60 HE | Not investigated | https://madlionskeyboard.com/ |
| MADLIONS | MAD LIGHT 60 quattro | Not investigated | https://madlionskeyboard.com/ |
| MADLIONS | TITAN75 Turbo | Not investigated | https://madlionskeyboard.com/ |
| MADLIONS | FIRE68 Ultra | Not investigated | https://madlionskeyboard.com/ |
| MADLIONS | NANO68 Pro | Not investigated | https://madlionskeyboard.com/ |
| Redragon | K556 HE | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Redragon | K580 HE | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Redragon | K585 HE | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Redragon | K617 HE | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Redragon | K618 HE | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Redragon | K683 (M61) | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Redragon | K686 HE | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Redragon | K736 Pro | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Redragon | K745 HE | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Redragon | K769 HE | Not investigated | https://redragonshop.com/collections/magnetic-switch-keyboards |
| Akko | 5075 V3 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | 5087 V3 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | 5075 V5 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | 5075 V5 TMR | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | 5087 V5 TMR | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | MOD68 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | MOD 007B HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | MOD 007 HE Year of Dragon | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | MOD 007 V5 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | Ray68 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | Shine60 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Akko | TAC75 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| MonsGeek | M1 V5 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| MonsGeek | M2 V5 HE | Not investigated | https://akkogear.eu/collections/akko-he-keyboards |
| Varmilo | Victory65 | Not investigated | https://varmilo.com/pages/keyboard-model |
| Varmilo | Victory75 | Not investigated | https://varmilo.com/pages/keyboard-model |
| Varmilo | Muse65 HE | Not investigated | https://varmilo.com/pages/keyboard-model |
| Varmilo | Muse75 HE | Not investigated | https://varmilo.com/pages/keyboard-model |
| Varmilo | Minilo75 HE | Not investigated | https://varmilo.com/pages/keyboard-model |
| Turtle Beach | Vulcan II TKL Pro | Not investigated | https://eu.turtlebeach.com/collections/tkl-keyboards |
| Turtle Beach | Command Series KB7 | Not investigated | https://eu.turtlebeach.com/collections/tkl-keyboards |
| NZXT | Function Elite MiniTKL | Not investigated | https://nzxt.com/products/function-elite-minitkl-lift-elite-wireless |
| Endgame Gear | KB65HE | Not investigated | https://img.endgamegear.com/assets/8/b/d/9/8bd91db974058fc6872a30cfc068ef094392ef51_KB65he_8k_endgamegear_de_en_fr_es_it_sv_pl_du_pt_fi_hu.pdf |
| Endgame Gear | KB65HE 8K | Not investigated | https://img.endgamegear.com/assets/8/b/d/9/8bd91db974058fc6872a30cfc068ef094392ef51_KB65he_8k_endgamegear_de_en_fr_es_it_sv_pl_du_pt_fi_hu.pdf |
| Wooting | 80HE+ | Not investigated | https://wooting.io/ |
| GamaKay | TK68 HE | Not investigated | https://gamakay.com/en-gb/products/gamakay-tk75-he-hall-effect-wireless-custom-keyboard |
| GamaKay | TK75 HE | Not investigated | https://gamakay.com/en-gb/products/gamakay-tk75-he-hall-effect-wireless-custom-keyboard |
| GamaKay | TK75 HE V2 | Not investigated | https://gamakay.com/en-gb/products/gamakay-tk75-he-hall-effect-wireless-custom-keyboard |
| GamaKay | NS68 | Not investigated | https://gamakay.com/en-gb/products/gamakay-tk75-he-hall-effect-wireless-custom-keyboard |
| GamaKay | TK75 TMR | Not investigated | https://gamakay.com/en-gb/blogs/news/tk75-buying-guide-socd-vs-he-v2-vs-tmr |
| CHERRY XTRFY | MX 8.2 Pro TMR Wireless | Not investigated | https://www.cherry.de/en-gb/company/news/press/article/cherry-xtrfy-introduces-magnetic-switch-keyboards-with-tmr-technology |
| CHERRY XTRFY | K5 Pro TMR | Not investigated | https://www.cherry.de/en-gb/company/news/press/article/cherry-xtrfy-introduces-magnetic-switch-keyboards-with-tmr-technology |
