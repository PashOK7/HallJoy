# AJAZZ AK680 V2 HE — firmware search, 2026-10-17

Owner request: a user asked for AK680 V2 support; check whether the keyboard
provides an analog stream, preferably by downloading and analysing the
firmware. Status unchanged: **not supported** (same Sonix family as the red
AK820 MAX HE `0C45:80B1`, see `AJAZZ_AK820MAX_SONIX_80B1_2026-10-08.md` and
GitHub #9).

Local evidence: `.local/research/ajazz-ak680v2-20261017/`.

## Firmware: not published anywhere official

- Driveall web configurator `https://ajazz.driveall.cn/` (new bundle
  `assets/layout-classic-0DHAQysC.js`, SHA256 `7edaa333...142638d`). OTA list
  is `${origin}/update.json`; for AJAZZ it is a 0-byte file
  (Last-Modified 2025-04-16). The old API
  `cp.driveall.cn/api/device/firmware/index` returns "数据为空" for
  `supplier_name=ajazz`, PIDs 32956/32961/32962 (decimal and hex, with and
  without product name).
- Other Driveall suppliers' `update.json` (xinmeng, langtu) list only Windows
  ISP updater EXEs for their own models (S75/M75/X98, MC68/BOOM68/T87). None
  is an AK680 V2 build; a sibling binary would not prove this firmware's
  behaviour, so nothing was downloaded.
- ajazz.net `pages/ajazz-drivers`: AK680 V2 has no entry; AK680 entries are
  the RongYuan AK680 MAX 8+1K/8+8K drivers (another platform) and the
  "AJAZZ AK680 Max Online Driver" link to Driveall.
- Web search (English and Chinese): reviews only; AK680 V2 is web-driver only.

Without a binary there is no static analysis. Reading the flash out of the
keyboard would need the hardware and a Sonix ISP/debug path; not done.

## What the configurator says about AK680 V2

Three Driveall configs named "AJAZZ AK680 V2" (`connectType: USB`, routes
customKeys/lighting/macro/performance/advancedKeys/settings, advanced keys
RS/SOCD/DKS/MT/TGL, report rates 1000/4000/8000):

| USB id | max trigger | note |
|---|---|---|
| `0C45:80BC` (32956) | 3.4 mm | GitHub #9 reporter's unit, firmware 11.01 |
| `0C45:80C1` (32961) | 3.3 mm | PID also used by an unrelated "AJAZZ KEYBOARD" TFT config |
| `0C45:80C2` (32962) | 3.3 mm | |

All three use the same Sonix AA/55 command set as AK820 80B1 and AULA MINI60;
the bundle has no per-PID protocol branch. Command table unchanged:
`SET_SIMULATION_TEST_ON/OFF` 0x66/0x67 (the `55 FB` stream, decoded only for
the single "test trip" readout), `GET_MAGNETIC_AXIS_STATUS` 0x68 (called only
on frameVersion 1 inside calibration V2), `GET_MAGNETIC_AXIS_KEY_STATUS` 0x60
(declared, never called). No new multi-key read appeared.

## Conclusion

- The keyboard does have a live analog source: the 0x66 stream reports real
  per-key travel (GitHub #9 measurements on 80BC: ~1 kHz, 0.01 mm).
- It is not a usable multi-key source on firmware 11.01: the stream follows
  one key and a key that loses the stream never gets its release; 0x68 and
  0x60 answer with an echo (80B1 log and #9 measurements agree).
- The owner's rule (no analog from key events, 2026-10-15 #9 reply) still
  applies, so nothing changes without new evidence.
- Open point: the requesting user's unit may be 80C1 or 80C2 or have newer
  firmware. A HallJoy.log from a current build shows the USB id and product
  string only (no backend claims 80BC/80C1/80C2); device info, frame_version
  and the 0x68 reply would need a probe build like the 80B1 one. Only a working 0x68/0x60 or
  a stream that reports every held key would reopen the question.

## Sheet gap

Main!A12:C12 AJAZZ / "AK680 V2" is still `Not investigated` (read 2026-10-17).
The 2026-10-15 GitHub #9 decision ("this firmware family stays unsupported")
only touched the AK820 80B1 row (A18); the AK680 V2 row was never updated.
Owner approved (2026-10-17): C12 -> `No usable analog found` (red), same as
the 80B1 row. Planned offline with `plan_keyboard_sheet_batch.py` (package
`.local/ak680v2-sheet-package-20261017.json`, plan `...-plan-20261017.json`:
value-only write C12 + base red colors B12/C12, 3 requests), applied in one
revision-guarded batch. Backup/before snapshot
`.local/sheet-native-20261017-before-ak680v2.json` (also in
`.local/backups/ak680v2-sheet-20261017/`). Fresh full native read
`.local/sheet-native-20261017-after-ak680v2.json`: the only differences are
row 12 B/C format and C value; C12 dropdown kept, effective background red.
`check_keyboard_sheet_structure.py` PASS (150 blocks, 0 issues),
`check_keyboard_sheet_presentation.py` PASS (0 issues), `support_notice_catalog.py --sheet`
PASS (287, rows file `.local/sheet-rows-20261017-after-ak680v2.json`),
`check_supported_layouts.py --sheet` PASS (73). No notes or comments.

No code change. No hardware or visual test by the agent.
