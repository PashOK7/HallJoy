# WLMOUSE Ying75: experimental support (2026-10-05)

Tester log `HallJoy (51).log` (HallJoy 1.6.7, 2026-10-04 18:21 UTC): USB product
"WLKB YING 75" `36A7:F887` (interfaces 00/01/02) was present; no native backend
knew it, so there was no analog source and the missing-source banner showed.

## Evidence (official vendor files, local in `.local/research/ying75-20261005/`)

- Web Hub https://kb75.wlmouse.gg/ (`index-B5Byzn-K.js`, SHA-256
  `6627840b...`): WebHID filter `{vendorId:13991 (0x36A7), productId:63623
  (0xF887), usagePage:0xFFA0, usage:1}`; packet head 92 (`0x5C`), commands
  `KB2_CMD_SYNC` 1, `KB2_CMD_RM6X21` 18 (travel; reply `0x92` = 146),
  `KB2_CMD_DEFKEY` 43 (factory map), `KB2_CMD_KEY` 35; travel values in um
  (`I/1e3` mm). This is the same JingTai protocol HallJoy already implements
  for IROK MG75 Pro / NA87 Pro / ND63 / Mercury68 and MU68 Pro (`irok-mg75-pro`
  backend, protocol 18). The hub's default device is `1C4F:EE88`, the JingTai
  reference board.
- Firmware `XS117_YING75_App_v1.0.2_20250423b.bin` from the official download
  page (CDN link on wlmouse.com/pages/download; SHA-256 `e7a6583e...`, 122,912
  bytes; not redistributed). Header build stamp 2025-04-23. USB device
  descriptor `36A7:F887` at 0x7608.
  - Windows factory matrix at file offset 0x4B8 (126 x u16, 6x21 slots), the
    same offset as Chilkey Slice75's; macOS copy at 0xC8 differs only by Left
    Win/Alt. 84 keys, Fn = `0xF001`, all unique.
  - Seven switch lookup tables (1024 x u16, 0..max) at 0x1064..0x4064 end at
    3300 um (five) or 3400 um (two). HallJoy uses 3300 um: full travel is
    always reachable; with a 3400 um switch the top ~3% saturates.
- Third-party analysis of the same firmware (github.com/matijuguera/
  wlmouse-ying75-auto-profiler) agrees on the identity, `0x5C` head and the
  XS117 platform; used only as a cross-check.

## Implementation

- `wlmouse_ying75_protocol.h`: exact model (`36A7:F887` + product
  "WLKB YING 75"), factory matrix generated from the firmware bytes, 84 keys,
  range 3300, identity `YING75-36A7-F887`.
- `mg75_pro_backend.cpp`: Ying75 passes the VID/PID prefilter and selects this
  model; the existing session proves the factory map against the keyboard
  (`0x2B` reply must equal the firmware matrix) before any travel is published.
- `keyboard_support_status.h`: classified with the JingTai group (yellow notice
  "Keyboard: hardware testing incomplete", protocol 18 group `Mg75Pro`).
- Layout "WLMOUSE Ying75 ANSI" (84 keys) by `tools/build_wlmouse_layouts.py`:
  the Web Hub's `Keyboard_Layout_width_84` table sums to 16 units in every row
  (compact 75%, no gaps; the alternative K84A table is inconsistent at 17
  units). Rounded hub widths are written as standard 1.25/1.75/2.25/6.25 units;
  rows/keys are asserted equal to the firmware matrix. Automatic selection by
  the verified session identity.
- Sheet: new brand block WLMOUSE / Ying75, Implemented; awaiting hardware
  testing (inserted with the batch planner between WAIZOWL and WOBKEY). Full
  re-read: no existing row changed; structure PASS (150 blocks), notices PASS
  (284), supported layouts PASS (71). Snapshots:
  `.local/research/ying75-sheet-20261005/`.
- Test: `mg75_pro_protocol_test` asserts identity, 84 unique keys, Fn, range.

## Validation

`build_release.ps1` PASS (all gates; layout catalog, yellow audit 284, supported
layouts 71) and `run_native_backend_checks.py --require-compiler` PASS. EXE
SHA-256 `cf3cf355c772c843adb8695404288f0c38bfdb771838041d39bd1f0eb2cd14fe`
(also contains Redragon K686 HE and the Ace 68 Air III change).

## Open

- Which board layout ID the keyboard reports (K84 vs K84A) is not visible
  without the device; the matrix proof at connect protects against a mismatch.
- No physical test. Needs a run of the new build by the tester.

## Tester log 52 (2026-10-05 13:55 UTC, local build with Ying75)

- `irok-mg75-pro observation=present_not_connected present=1 connected=0`,
  failures 1 -> 9 in 8 s (the worker retries about once a second): the new
  admission finds the keyboard, but every session fails before connecting.
  The support log did not say which step; the phase went only to the debug log.
- Formats of all three requests match the official Web Hub byte for byte:
  checksum (`53 + head + len + cmd + last payload byte`), factory map `0x2B`
  (rows at payload 1/23, keys at 2..22/24..44), assignments `0x23` (key, layer,
  value LE), travel `0x12` (payload 1 = 2, 63 LE values from payload 2). The
  difference is strictness: HallJoy required status 0, a matching checksum and
  length 128 for the travel reply; the official client checks none of these and
  always reads three reports for a travel reply (it overrides the length byte).
  The firmware's command handlers are in its LZ-compressed block, so the exact
  reply bytes were not derived statically.

### Changes

- Ying75 sessions use the official client's rules (`Frame::lenient`): header
  `0x5C` and command echo still required; no status/checksum check; a travel
  reply is 132 bytes from three reports regardless of its length byte.
- Stronger identity for Ying75 instead: the factory map (`0x2B`, read by the
  official client too) must equal the firmware matrix (or its macOS variant with
  Left Win/Alt swapped) before anything is published.
- Support log: `mg75.admission_failed` = phase (1 open, 2 factory read,
  3 travel, 4 assignments, 5 factory mismatch) << 56 | exchange stage (1 write,
  2 timeout, 3 read, 4 rejected) << 48 | reject cause (1 report, 2 header,
  3 length, 4 command, 5 status, 6 checksum, 7 overflow) << 40 | command << 32 |
  reply length byte << 24 | reply command << 16 | reply status << 8 | reports;
  Win32 detail. Logged once per distinct failure.
- Tests: `mg75_pro_protocol_test` covers strict reject causes, lenient travel
  parsing, command echo in lenient mode and the Ying75 Windows/macOS factory
  proof. `build_release.ps1` and native suite PASS. EXE SHA-256
  `f70dd8d875afdbade4ccb9c4746fdd9aee39bf78b902e5037f23775e4f34be75`.
- If the next log still shows `connected=0`, `mg75.admission_failed` names the
  step and the reply header.

## Tester log 54 (2026-10-05 14:47 UTC, build f70dd8d8)

- `mg75.admission_failed value=72057594037927936 error=32`: phase 1 (open),
  Win32 32 `ERROR_SHARING_VIOLATION`; failures 12, all identical (repeats are
  suppressed). The protocol was never reached: the exclusive open failed
  because another handle with read/write access was open on the vendor
  interface, already at the first attempt (before HallJoy's UAP child started).
- Not HallJoy's UAP child: Soup opens every HID interface only briefly during
  its once-a-second discovery and does not treat `36A7:F887` as an analogue
  keyboard. Likely holders: the WLMOUSE Web Hub tab (WebHID keeps the device
  open while connected) or a background RGB/peripheral service.
- Fix (Ying75 only): after a sharing violation the session opens the interface
  shared, like the official client (`mg75.shared_open` logged once). Before
  every request a shared session drains pending reports; any reply it did not
  ask for (`5C`, response bit) ends the session with
  `mg75.admission_failed` stage 5, because a travel reply carries no half id
  and another program's reply could be taken for ours.
- `build_release.ps1`, diagnostics gate and native suite PASS. EXE SHA-256
  `2aaf8fbb217bfeb63548fbfe4058c614eab1247dd0502ef1233b760428c25d7c`.

## Supported (2026-10-05)

Tester log 55 (older local build, Web Hub tab closed): `irok-mg75-pro connected
36A7:F887`, 84 mapped keys, 7,182 travel updates in 15 s, 0 failures. All
criteria met (working path, built-in layout with automatic selection,
`supported_layouts.json`, checks): Supported under the owner rule.
- `supported_layouts.json`: WLMOUSE / Ying75 -> "WLMOUSE Ying75 ANSI".
- Notice: `Mg75Pro` group excludes token `E6D996D6FD1177EC` with PID 0xF887
  (`confirmed_devices`); `ClassifyFrozen` no longer flags it; tests assert both.
- Sheet Main!C775 Implemented -> Supported (green), full re-read: only row 775
  changed; structure PASS (150 blocks), notices PASS (283), supported 72.
  Snapshots `.local/research/ying75-supported-sheet-20261005/`.
- README Supported list and SUPPORTED_HARDWARE row updated. Known limit: the
  WLMOUSE Web Hub tab holds the interface; HallJoy now opens it shared then.
