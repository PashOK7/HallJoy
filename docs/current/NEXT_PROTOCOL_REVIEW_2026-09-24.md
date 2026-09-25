# Protocol follow-up — 2026-09-24, local only

## Completed integration

Game Arena GKX68 MAGNUM, experimental wired USB, two exact OEM revisions:

| Board | VID:PID | Source | Reviewed parent |
|---|---|---|---|
|2628|3151:5030|1bca065b.js|7a5b12c9.js|
|2790|3151:502C|1524e8dd.js|631ebd97.js|

The [own-brand product page](https://gamearena.ly/%D8%AC%D9%83%D8%B368G) identifies Game Arena GKX68 MAGNUM as a 68-key magnetic keyboard. Both exact OEM records name GKX68MAGNUM/GKX68 and provide complete factory matrices. The second record's internal YC3121 name does not make it the legacy CommonKbYc500 variant: its actual source inherits the reviewed stream parent.

Enabled ordinary native identification, factory mapping, independent analog input, bindings and gamepad output; yellow runtime notice. Range is provisionally4mm, not a measured switch endpoint. Manual visual layout selection may be needed. No hardware-test claim, calibration write, diagnostic build, or GitHub publication.

Source bytes/hashes are pinned under `docs/research/rongyuan-stream/`; catalog dispositions updated. Application package `.local/gamearena-prepared-20260924.json`; backup `.local/backups/before-gamearena-20260924.zip`. README, hardware evidence and next patch notes updated by the guarded batch tool.

## Verification

- Source audit PASS:169 model labels /253 exact revisions.
- Native stream regression PASS:253 revisions,4898 captured-frame fixtures; stationary hold, release, aliases, disconnect, malformed frames, precision, identity and notices. These are offline fixtures, not new hardware captures.
- Ordinary Release and six linked-image gates PASS. Build log `.local/gamearena-release-20260924.log`.
- Installed `build/bin/Release/x64/HallJoy.exe`: SHA256 `d71611a705000e9a390abfd98c09b7bbf4ed01db276872e6001621b8e18927b6`.
- The first ad-hoc test invocation omitted `keyboard_support_status.cpp` and failed to link; corrected invocation used the existing native runner's full source list and passed. No application defect was inferred from that harness error.

## Live Sheet synchronization

[HallJoy supported keyboards](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit), Main, sheetId0. Fresh native read, unchanged-snapshot comparison, six-request batch added Game Arena/GKX68 MAGNUM at A328:C328 plus a separator. Status `Implemented; awaiting hardware testing` is in the preserved dropdown.

Independent full readback:645 model rows,246 yellow,63 green,332 gray,4 red;147 brand blocks. Structure and base/effective colors PASS; all246 yellow entries reconcile with runtime catalog. All1271 previous rows' native cell data and row metadata are exactly preserved after index translation; column widths and conditional-rule semantics unchanged. No comments or notes. API checks, not owner visual acceptance. Snapshots/plan `.local/gamearena-{before,after}-20260924.packed.json`, `.local/gamearena-sheet-plan-20260924.json`.

## Research findings to retain for the next batch

### Legacy RongYuan CommonKbYc500

Pinned original WOMIER3.2.15 archive already used by the catalog pipeline contains:

- `2e81aae0.js`: SHA256 `cd90ed0ba676413dcf5be8f1606d48e18ad6cbe8ceb8489216b55a812c27cba7`.
- Generic base `15394fe0.js`: `72c350cb9902a3386dbb9374f4951b3202f84839e2c30c78077f7a4eb165c2ba`.
- YUNZII RT75 factory class `9f7a626a.js`: `50130b4917340a10595d1f87f30811107c79f557a19697b1e0cc4ffa6da3abad`.

Stream enable is also0x1B, but this is insufficient for compatibility. Profile read is0x85, magnetic profiles divide raw profile by4; key-map read is0x89 with layer*4+Fn and eight pages. Depth units are10/mm; vendor scalar UI reads byte2 only for this scale. Do not copy the modern two-byte sample decoder or assume byte4 is an independent key slot without evidence.

`_getMagnetismCalibrationValue` reads0x9E/0x9C and computes abs(difference)/400; `_getJiaoZhunXinXi` displays this in the calibration UI. This does not establish a live, multi-key analog interface outside calibration. Commands0x1C/0x1E change calibration and must not be used to make a speculative integration work.

Next useful evidence: actual firmware handlers/report schema establishing independent key identity and normal-mode live values. Holds include legacy RT75/RT80/RT68, PIIFOX ER75/WALKER75 and strayfe RxD60 HE revisions. Do not mark these yellow from the shared0x1B command alone.

### FL ESPORTS SparkPlayJoy

Official software page links FL750HE/GEO65HE/GEO75HE to `https://fl.sparklinkplayjoy.com/`. Downloaded, not executed: `.local/research/next-protocols-20260924/fl-index-B_l-bfBe.js` and `fl.html`. The client uses legacy pageFFA0/usage1, command0x12 RM6x21,0x23 key assignments and0x2B factory keys, related to the existing AULA legacy backend rather than SparkLink V2.

USB filters alone do not map those retail models to exact board IDs. Device names and firmware updates are queried at runtime; updater endpoint is `/api/keyboard/getUpdate.php?BoardID=...`. No verified offline board-ID/retail mapping found. Next step is exact board identity/firmware evidence, not guessed board prefixes or wholesale admission of all filter PIDs.

### IROK JingTai V1

Existing MG75 Pro integration is exact-model scoped, with firmware-derived6x21 factory matrix and fixed3.5mm normalization; its map must not be reused for all V1 group members. Official group inventory includes NA87 Pro, ND63, IYX MU68 variants and Polar75. Review each class's layout/read behavior or implement a proven dynamic factory-map path before promotion. No new V1 support claimed in this follow-up.

### Remaining catalog scope

Whole479-record RongYuan ledger now has201 prior stream records,50 batch7 records,2 Game Arena records,147 unresolved retail mappings,44 technical holds and35 other-backend references. These are revision counts, not147 easy-to-add keyboards. Many remaining labels are controller codes, mechanical namesakes or ambiguous retail variants. Batch7's explicitly branded OEM admissions remain valid; do not impose a new universal physical-tester requirement.
