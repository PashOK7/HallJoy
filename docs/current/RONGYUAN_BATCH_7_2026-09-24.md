# RongYuan batch 7 — complete catalog pass, 2026-09-24

LOCAL ONLY. No GitHub publication, device flashing, physical tests or firmware emulation.

## Completed result

Added 40 experimental model/variant labels plus six revisions of existing models: 50 exact USB board/VID/PID profiles across 28 brands. RongYuan stream now covers 168 model labels / 251 revisions. Whole application: 228 yellow entries, 63 green entries. G84 HE JIS is counted as a distinct regional variant here, not an entirely different product family.

Detection, analog decoding, factory-position bindings and virtual-gamepad output are enabled through the existing native backend. These are not disabled placeholders. Manual layouts remain available; this packet does not invent automatic geometry for every model. USB only: no claim for Bluetooth or wireless receivers merely because the keyboard supports them.

Yellow reflects unverified physical behavior, firmware/switch variation and (where recorded) provisional normalization. It is not proof of a real-device test. Readings still use the established stream protocol; nominal ranges are per exact profile, not a global clamp or changed sensor resolution.

## Sources and protocol review

Reviewed the pinned WOMIER 3.2.15 OEM catalog, model loaders and classes as data; vendor JavaScript was not executed. The catalog SHA256 is `27229ea0331f511aaa321fc742605c36bc30705e616517709a8a8b1e8ee72c93`; technical archive SHA256 is `2066de9d59d41e6c17654df1e99ee9c6f919fe2d70fc868444aa93d77c43b5ce`.

Each exact model class, matrix, admission record and source hash is pinned in `docs/research/rongyuan-stream/`. `profiles.json` and `admission_sources.json` preserve the evidence. This packet is based on driver implementation and exact identities; it does not claim an independently downloaded and emulated firmware image for each model.

Three additional parents inherit analog/transport behavior from the already reviewed `7a5b12c9.js`:

- `631ebd97.js`: firmware-upgrade wrapper only.
- `3f643f35.js`: boot query and firmware-upgrade wrapper only; updater has its own CRC/boot operations.
- `9b8d4f8a.js`: Nordic boot constants/query and firmware-upgrade wrapper only.

HallJoy does not call those upgrade/boot operations. Parent bytes are pinned. NOS C800 ALU class `d9073e21.js` additionally changes LED constants, `setLightSetting` and `getSideLightSetting`; exact SHA256/parent review is in `reviewed-overrides.json`. This is not a general permission for similarly named methods: a modified class is rejected by regression tests. Other unknown overrides continue to be held.

OEM-specific names (including IDJ, JINGSU, Koda, Funbey, Ninjadog, PSYCommu) are admitted only for their explicit branded catalog record, known stream parent and complete factory matrix. Independent retail documentation was not found for every such name; do not expand them to related brands or renamed products. Ninjadog's earlier deferral is superseded for exact board3076 after review of its explicit Varna Atlas record; no tester result was received.

Primary retail corroboration used where available:

- [Womier SK61 HE](https://womierkeyboard.com/products/womier-sk61-he-wireless-magnetic-keyboard): exact name and 3.3mm actuation limit.
- [Nova Gaming products](https://www.novagaming.fr/produits) and [downloads](https://www.novagaming.fr/download): GK505 Eon magnetic model and driver; retain its factory regional map.
- [CHERRY K5 Pro TMR Compact](https://www.cherry.de/en-gb/product/k5-pro-tmr-compact): exact product; OEM range3.5mm. Existing Sheet K5 Pro TMR row was clarified, not duplicated.
- [MonsGeek M3 V5 HE](https://www.monsgeek.com/keyboard/m3-v5-he-fully-assembled/): magnetic edition, not ordinary mechanical M3.
- [GAMEPOWER Nexa HE60](https://gamepowerpc.com/keyboard/nexa-he60): exact1K model. Use OEM3.3mm normalization; marketing4mm is not treated as a measured output range.
- [HAWK support](https://www.hawkgaming.com.tr/tr/destek/): HK550 and HK610S magnetic editions.
- [DARKFORCE Fib-68 manufacturer design entry](https://ifdesign.com/en/winner-ranking/project/darkforce-fib-68-magnetic-keyboard/745453): magnetic model identity.
- [Fury Kanabo K6 announcement](https://pl.fury-zone.com/press/premiera-fury-kanabo-k6-bron-dla-graczy-ktorzy-nie-uznaja-kompromisow): magnetic product and4mm travel.
- [Titan Nation PCB catalog](https://www.titannation.cn/sys-col-120/): TITAN60 PCB. Its factory matrix contains60 ordinary key positions despite a Common68 layout label; the previous size-based hold is superseded by the matrix review. Storm68 retains exact TITANHUB OEM scope.
- [Valkyrie](https://www.valkyrie.com.cn/): VK99 Gaming Naruto magnetic edition; ordinary VK99 board2410 remains excluded.
- [Astromeda official shop](https://shop.mining-base.co.jp/collections/gaming-keyboards) and [driver page](https://shop.mining-base.co.jp/pages/keyboard-application): Mining Base ownership / qmk.top driver. AMGK80-001 is an existing model; only its extra revision is new.
- [NOS C800](https://nosgg.com/products/nos-c800-mini-keyboard-magnetic-65-rgb-black-hall-effect) corroborates the product family, not every ALU revision. Only explicit C800 ALU UK OEM board3663 is added.

## Exact affected models

Six existing model labels receiving another revision: ASTROMEDA AMGK80-001, EPOMAKER HE60 Wired, EWEADN ZAP87 HE, FL ESPORTS GP75 HE, MonsGeek M2 V5 HE and M3 V5 HE.

Internal identifiers below are evidence, not user-facing names.

| Brand | Model / variant | Board revisions | Range, micrometres |
|---|---|---|---|
| ANGRYSHARK | Final 75 | 3089 | 4000 |
| ASTROMEDA | AMGK80-001 | 3389 | 4000 |
| CHERRY XTRFY | K5 Pro TMR Compact | 2626 | 3500 |
| DARKFORCE | Fib(68) | 3055 | 4000 |
| E7 | 68 PRO V2 | 3383 | 4000 |
| EPOMAKER | G84 HE JIS | 3703 | 4000 |
| EPOMAKER | HE60 Lite | 3727, 3759 | 3300 |
| EPOMAKER | HE60 Wired | 3746 | 3400 |
| EWEADN | SMART 875 HE | 2637 | 4000 |
| EWEADN | V99 (magnetic version) | 2279 | 4000 |
| EWEADN | ZAP68 SE | 3225 | 4000 |
| EWEADN | ZAP87 HE | 2527 | 4000 |
| FL ESPORTS | D75 HE | 3542 | 4000 |
| FL ESPORTS | D98 HE | 3556 | 4000 |
| FL ESPORTS | FL750 (magnetic version) | 3003, 3524 | 4000 |
| FL ESPORTS | Flame65S | 3547 | 4000 |
| FL ESPORTS | GP75 HE | 2869 | 4000 |
| FL ESPORTS | GP87 HE | 3007, 3539 | 4000 |
| Funbey | AST V68 | 3292 | 3300 |
| Funbey | Coke V68 | 3291 | 3300 |
| Fury | Kanabo K6 | 2763 | 4000 |
| GAMEPOWER | Nexa HE60 1K | 2406 | 3300 |
| HAWK Gaming | HK550 | 3760 | 4000 |
| HAWK Gaming | HK610S | 3677 | 3300 |
| IDJ | H60HE | 3078, 3079 | 4000 |
| JEDEL | KL166 | 2729 | 3300 |
| JINGSU | KA67 | 3106 | 4000 |
| JINGSU | KB98 | 3520 | 4000 |
| JINGSU | KCC04A | 3193 | 4000 |
| JINGSU | KE87 | 3711 | 3300 |
| Koda | A68 | 3244 | 4000 |
| MageGee | MK-BOX (magnetic version) | 2311 | 3300 |
| MechLands | M75 | 2496 | 4000 |
| MonsGeek | M2 V5 HE | 2601 | 4000 |
| MonsGeek | M3 V5 HE | 2585 | 4000 |
| Ninjadog | Varna Atlas | 3076 | 4000 |
| NOS | C800 ALU (UK) | 3663 | 4000 |
| Nova Gaming | GK505 Eon | 3302 | 4000 |
| PSYCommu | PSY P1 | 3425 | 4000 |
| SALPIDO | SHOT209 | 2932 | 3300 |
| Titan Nation | Storm68 | 2816 | 3400 |
| Titan Nation | TITAN60 PCB | 2220 | 3400 |
| Valkyrie | VK 99 Gaming (Naruto) | 2831 | 4000 |
| Womier | SK61 HE | 3708 | 3300 |
| XINMENG | X87 TMR | 3486 | 4000 |
| XINMENG | X98 V3 (magnetic version) | 3277 | 4000 |

Ranges: OEM travel.max is used for3.3/3.4/3.5mm profiles; Womier SK61 HE uses its official3.3mm maximum. Fury4mm has official corroboration. Other4mm entries remain provisional and are marked experimental. Changing switches may require revisiting the range; no hardcoded user calibration was added.

## Whole-catalog disposition

[Machine-readable review ledger](../research/rongyuan-stream/catalog-review-20260924.json) covers every one of the479 records, preserving record order, IDs, source/loader evidence and reasons:

- 201 revisions were already admitted to this stream backend.
- 50 revisions were added in this packet.
- 149 compatible source records still need a unique retail-model mapping.
- 44 records are held for technical/source/identity reasons, including placeholders, missing loaders, hybrid/mechanical configurations and different parent families.
- 35 records reference another backend and are not counted as new integrations. A source-code reference is not independently asserted to prove every hardware revision.

These are records, not479 distinct retail keyboards. A matching layout/class does not prove an OEM alias. Specific unresolved examples: GamaKay LK75 has a mechanical retail namesake; TK75 revisions cannot be automatically named HE V2; Akko Mineral/Gem codes are not retail names; VGN8640/8670/Neon needs exact HE/HE+/Pro/Ultra mapping. Examined VGN overrides concern side LEDs, not a discovered analog failure. AttackShark K13 is a36-position device whose retail relation to M36 HE remains unresolved. YC500 is a different unreviewed family for this packet. No physical tester is imposed as the prerequisite for resolving these holds.

A separate new lead from this pass: EWEADN's [DK68 HE product page](https://www.eweadn.com/ja/collections/キーボード/products/eweadn-dk68-he-magnetic-switch-keyboard) explicitly lists a SparkLink MCU. That alone does not identify its analog endpoint, board identity or map; do not equate it with the unrelated osd DK68 HE records or relabel it yellow yet.

## Automation and verification

`tools/rongyuan_batch.py` prepared and applied the packet after complete hash preflight. Backup: `.local/backups/before-rongyuan-batch7.zip`; pre-review tool/source-lock backup: `.local/backups/before-batch7-protocol-review.zip`. Superseded initial package `.local/rongyuan-batch7-package.json` was never applied; the final package is `.local/rongyuan-batch7-final-package.json`.

New `tools/plan_keyboard_sheet_batch.py` plans native insertions/status changes/explicit renames, sorts new models naturally, keeps separators and recomputes outlines before sending anything. It refuses damaged input structure, duplicate pairs/intents, invalid statuses and colliding renames. It detaches interned formats before editing cells. It performs no network writes and must be followed by fresh live comparison, application and readback.

Validation:

- Source/profile audit PASS:251 revisions /168 labels; every compiled matrix byte and identity matches pinned evidence.
- Native protocol regression PASS:251 profiles,4898 captured frames plus full-map generated cases; stationary holds, releases, aliases, disconnects, malformed frames, precision, identity and notices.
- Ordinary Release and all six linked-image gates PASS, diagnostic flags disabled.
- EXE: `build/bin/Release/x64/HallJoy.exe`; SHA256 `36fb11b45fe54cf5e82d73213344655c818e5b8684c912a07d1cef59c34625e2`.
- Generic automation regressions PASS: 11 batch-generator tests, 4 native Sheet planner tests and 2 sparse Sheet structure tests (17 total). No new device-specific runtime tests.

## Live Sheet synchronization

[HallJoy supported keyboards](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit), Main, sheetId0. Fresh before-snapshot backed up, native data re-read and compared immediately before applying212 requests in one coherent batch.

37 model rows and16 brand separators inserted. Three gray rows became yellow: CHERRY XTRFY K5 Pro TMR, Valkyrie VK99 Gaming (Naruto), Womier SK61 HE. CHERRY name clarified to K5 Pro TMR Compact. Six already-yellow models retained status while gaining revisions. Final grid1259 rows;632 model rows:228 yellow,63 green,337 gray,4 red.

Independent readback: all228 yellow rows match runtime notices; structure PASS146 blocks, zero issues. All1206 prior rows were compared: unchanged values outside authorized name/status edits, validation, non-border formatting, heights, unaffected effective colors, column widths and notes. Conditional rules preserve semantics with Google's expected coordinate shifts. Existing outlines changed only as required by inserted rows. No comments/notes added. This was native/API structural verification, not an owner visual acceptance claim.

Snapshots/plan/evidence: `.local/rongyuan-batch7-sheet-before.packed.json`, `rongyuan-batch7-sheet-plan.json`, `rongyuan-batch7-sheet-after.packed.json`, `rongyuan-batch7-sheet-yellow.json`, `rongyuan-batch7-sheet-verification.json`.
