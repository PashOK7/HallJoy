# RongYuan third large support batch — 2026-09-24

## Result

22 models / 33 revisions / 7 brands added locally. Experimental yellow support
is enabled for wired USB detection, independent per-key analog, bindings and
virtual gamepad output. Manual layout selection remains available. No physical
hardware testing, firmware emulation, flashing or GitHub publication occurred.
K4 onboard and ordinary logging policy remain unchanged.

## Exact profiles

| Brand | Model | Board | VID:PID | Source | Factory layout | Precision enum |
|---|---|---|---|---|---|---|
| EWEADN | DEEP68 HE | 2578 | 3151:5025 | 454c6478.js | c.Common68_FreeWolfF68 | False |
| EWEADN | DEEP68 HE | 2710 | 3151:5030 | 6d1bc62f.js | c.Common68_FreeWolfF68 | False |
| EWEADN | DEEP68 HE | 2711 | 3151:5029 | fff5c86f.js | c.Common68_FreeWolfF68 | False |
| EWEADN | DEEP68 HE | 2955 | 3151:5029 | eea5f3aa.js | c.Common68_ZAP68 | False |
| EWEADN | DEEP80 HE (magnetic version) | 2574 | 3151:5030 | 2d590c99.js | c.Common84_x110_Deep80 | False |
| EWEADN | DEEP80 HE (magnetic version) | 2652 | 3151:5030 | 09265330.js | c.Common84_x110_Deep80 | False |
| EWEADN | DEEP80 HE (magnetic version) | 2653 | 3151:5029 | f65be045.js | c.Common84_x110_Deep80 | False |
| EWEADN | DEEP80 Pro HE (magnetic version) | 2906 | 3151:502D | c492da48.js | c.Common84_x110_Deep80 | False |
| EWEADN | ZAP68 HE | 2348 | 3151:502D | 14f3d6e4.js | c.Common68_ZAP68 | False |
| EWEADN | ZAP68 HE | 2426 | 3151:5030 | f02e8de1.js | c.Common68_ZAP68 | False |
| EWEADN | ZAP68 HE | 3035 | 3151:5029 | f467984f.js | c.Common68_ZAP68 | False |
| EWEADN | ZAP68 HE | 3036 | 3151:5030 | 72b90ba4.js | c.Common68_ZAP68 | False |
| EWEADN | ZAP68 Ultra HE | 2554 | 3151:5029 | f1752204.js | c.Common68_ZAP68 | False |
| EWEADN | ZAP68 Ultra HE | 2510 | 3151:5030 | 9d4568f1.js | c.Common68_ZAP68 | False |
| EWEADN | ZAP87 HE | 3011 | 3151:5030 | 10567eac.js | c.Common87_MK25022B | False |
| EWEADN | SEEK75 | 2799 | 3151:5030 | 56246b54.js | c.Common81_beat75_v2 | False |
| Valkyrie | VK Mag75 | 2246 | 374A:A216 | bb3dfbdf.js | c.Common79_VKMAG75 | False |
| Valkyrie | VK Mag75 Lite | 2234 | 374A:A213 | c859c43d.js | c.Common80_FQ_X78 | False |
| Valkyrie | VK Mag75 Pro | 2227 | 374A:A216 | bb3dfbdf.js | c.Common79_VKMAG75 | False |
| Valkyrie | VK Mag75 Max | 2398 | 3151:5030 | ea87bd0d.js | c.Common79_VKMAG75 | False |
| Valkyrie | VK Mag68 | 2320 | 374A:A225 | 14847152.js | c.Unknown | False |
| Valkyrie | VK Mag68 | 3390 | 374A:A232 | 667783ef.js | c.Common68_FreeWolfF68_v2 | True |
| Valkyrie | VK Mag68 Max | 3194 | 374A:A233 | 655f8c4a.js | c.Common68_FreeWolfF68_v2 | True |
| Valkyrie | VK NB68 | 2560 | 374A:A228 | 8e7e1202.js | c.Common68_FreeWolfF68 | False |
| Valkyrie | VK NB68 Max | 2757 | 374A:A236 | 4068b246.js | c.Common68_FreeWolfF68 | False |
| Valkyrie | VK NB68 Max | 3111 | 374A:A236 | 38eb1359.js | c.Common68_FreeWolfF68_v2 | True |
| FREEWOLF | F68 | 2634 | 3151:5030 | d2c8ac09.js | c.Common68_FreeWolfF68 | False |
| FREEWOLF | F68 PRO | 2594 | 3151:5030 | cc66dead.js | c.Common68_FreeWolfF68 | False |
| Oniverse | Maegnus | 3267 | 3151:5029 | 749ae2c5.js | c.Common67_SG9004UK | False |
| GAMEBOOSTER | RAPID HE | 3278 | 3151:5029 | 46e0375c.js | c.Common82_KD82 | False |
| Rampage | KAISEL | 3114 | 3151:5029 | 21985709.js | c.Common68_SG8905 | False |
| Rampage | ZENITH PRO | 2310 | 3151:502D | e774bbc7.js | c.Common82_SG9000 | False |
| Blackstorm | Renegade HE | 3355 | 3151:5029 | 6b5641b9.js | c.Common67_ka67UK | False |

## Admission evidence

The pinned Womier 3.2.15 OEM records associate each exact board/VID/PID with a
magnetic flag, class loader and complete factory matrix. 32 additional source
chunks and their SHA256 values retained under docs/research/rongyuan-stream.
The 33 profiles use the reviewed report 5 / command 1B analog stream through
backend 22. Three Valkyrie revisions (3111, 3194, 3390) inherit precision-enum
parent 60ee4367.js; all others inherit 7a5b12c9.js. Existing physical-slot
publication, negotiated units, delta retention, alias max merge and release
handling are reused. No configuration or calibration writes were added.

EWEADN ZAP68 board2348 stores its matrix in a local literal constant instead
of directly in the class. The audit now resolves that literal safely without
executing vendor JavaScript. It compares all 512 bytes with runtime data.

The previous exploratory override regex only recognized method syntax and
could miss arrow-function class fields (observed while inspecting NOS C800 ALU).
The permanent source audit now strips only factory-matrix data and reviewed
lighting constants; any remaining class behavior fails admission. It covers
all 146 existing/new revisions, not just this batch. Rampage KAISEL differs only
in lighting constants; there is no protocol-method override. This strengthens
the research gate without changing the runtime protocol.

All new ranges remain explicitly provisional 4000um. This is normalization,
not a claimed measurement of a switch's sensor endpoint or effective precision.
Firmware versions, switch-specific ranges and real hardware behavior remain
reasons for yellow status. Automated tests are not physical-device validation.

## Identity and retail scope

- EWEADN DEEP68, DEEP80, DEEP80 Pro, ZAP68 and ZAP68 Ultra retain the existing
  Sheet HE/magnetic-version labels. Do not imply mechanical siblings, DEEP80 Max,
  ZAP68 Pro or DEEP68 Pro are covered by a similar name. SEEK75 and ZAP87 use
  explicit magnetic OEM records. Only board3011 is admitted for ZAP87 here.
- Valkyrie uses the OEM company VKMS. The official Valkyrie site identifies the
  VK MAG75 magnetic family. MAG75/Lite/Pro/Max and MAG68/Max/NB68/Max are separate
  profile groups; exact board identity selects each revision despite shared PIDs.
  Existing Sheet label VK Mag75 retained. No VK99 or Cyber68 claim added.
- FREEWOLF F68 and F68 PRO use separate wired/tri-mode OEM identities; HallJoy
  support here requires a USB cable for both.
- Oniverse Maegnus uses the pinned UK physical matrix and French retail AZERTY
  legends. HallJoy follows physical HID positions, not translated keycap text.
- GAMEBOOSTER RAPID uses the Turkish-Q factory profile; no automatic claim for
  other regional revisions. Retail RAPID HE name retained, no monitor alias.
- Blackstorm Renegade uses its UK/ISO factory profile; retail HE model is Nordic.
  Regional keycap legends do not change the pinned HID map.
- Rampage KAISEL and ZENITH PRO have separate exact OEM records. Official Rampage
  listings corroborate magnetic keyboard product names.

Primary web corroboration checked during the batch (protocol evidence remains
local source-pinned OEM code, not third-party compatibility lists):
- https://www.valkyrie.com.cn/ — VK MAG75 magnetic family and official driver.
- https://www.eweadn.com/pages/eweadn-drivers — legacy magnetic families.
- https://www.eweadn.com/collections/gaming-series — DEEP68/80 and ZAP68 HE.
- https://www.eweadn.com/pages/driver-download — distinguish legacy and new HUB.
- https://www.eweadn.com/products/eweadn-smart875-aluminum-mechanical-keyboard —
  conflicting mechanical SMART875 product, excluded pending identity resolution.
- https://www.oniverseofgamers.com/maegnus — wired magnetic/AZERTY model.
- https://www.rampage.com.tr/en — KAISEL and ZENITH PRO magnetic listings.
- https://www.e-vektron.com/marka/gamebooster — official distributor RAPID HE listing.
- https://www.verkkokauppa.com/fi/product/1007867/Blackstorm-Renegade-HE-RGB-pelinappaimisto-musta — own-brand HE product.

## Deferred, not silently promoted

- EWEADN SMART875 board2637 conflicts with a mechanical/VIA retail namesake;
  resolve the precise magnetic SKU before adding it. K68 board2301 is not
  automatically DK68. ZAP87 board2527 has a Common104 layout expression and
  differs from3011; inspect its exact retail/revision scope first.
- Valkyrie VK99 board2410 / VK99 Gaming2831: mechanical/magnetic retail ambiguity.
  Cyber68 boards3250/3289 were inspected but retail naming remains uncorroborated.
- MechLands M75 board2496: do not promote the ordinary mechanical M75 by name.
- NOS C800 ALU3663: class has lighting arrow-method overrides; not data-only.
  Needs separate review of the overrides and retail mapping, not blanket rejection
  of eventual compatibility.
- Previous deferred records remain in RONGYUAN_BATCH_2_2026-09-24.md and
  RONGYUAN_BATCH_2026-09-24.md. IO Type68 Wireless stays catalog-only as requested.

## Verification and delivery

Source audit PASS: RongYuan 96 models / 146 revisions.
Generic full-map regression PASS: all revisions, 4898 earlier captured frames,
stationary hold/release, duplicate HID aliases, disconnect, malformed packets,
precision, identity matching and notices. New models were exercised synthetically,
not recorded from physical keyboards.
Release build and all six linked executable gates PASS.
Build log: .local/rongyuan-batch3-build.log
Installed EXE SHA256: 7878cb3fa33ac04fb1d0e480afdbeef76fb19f457afd14f044bc56c25de30f24

README, hardware table, next release notes and generated notice catalog agree.
Live Sheet Main updated by one 52-request transaction after exact fresh snapshot
comparison. Six Not investigated statuses became Implemented; awaiting hardware
testing, 16 models and five separators inserted. All 547 old model rows retain
values (except those six statuses), validation and non-border formatting; unrelated
colors and column widths unchanged. No comments/notes. Readback confirms all156
yellow rows match runtime. Structure audit PASS107 blocks, zero issues: continuous
outer borders, heights, validation and blank-row dropdowns checked through native
API metadata. No GUI visual test per owner instruction.

Final affected rows:
Blackstorm C148; EWEADN C241:C242, C244, C248, C250, C252:C253;
FREEWOLF C265:C266; GAMEBOOSTER C279; Oniverse C504;
Rampage C521:C522; Valkyrie C608:C615.

Current Sheet totals: 563 models; 156 yellow, 63 green, 340 gray and 4 red.

Backups/evidence:
- .local/backups/before-rongyuan-batch3-20260924.zip
- .local/backups/check_rongyuan_before_batch3.py
- .local/backups/sheet-before-rongyuan-batch3.packed.json
- .local/rongyuan-batch3-sheet-native.packed.json (Main!A1:C1151)
- .local/rongyuan-batch3-sheet-yellow.json
- .local/rongyuan-batch3-plan-20260924.json
- .local/rongyuan-batch3-test.exe
