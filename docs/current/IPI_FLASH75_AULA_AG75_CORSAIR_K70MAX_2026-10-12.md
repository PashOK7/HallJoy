# IPI Flash75 Pro, AULA AG75, Corsair K70 MAX: research (2026-10-12)

Owner request: examine three models from user requests (Discord: "AG75 or IPI Flash 75
Pro"; GitHub issue #8: Corsair K70 MAX), download their firmware and decide whether
HallJoy can support them. Research only; no code change yet. Local files:
`.local/research/ipi-ag75-k70-20261012/` (bundles, catalogs, firmware stay local).

## Summary

| Model | Platform | Analog path | Firmware | Verdict |
| --- | --- | --- | --- | --- |
| IPI Flash75 Pro (Gemini tri-chip) | SparkLink V2, `1CA6:1A08/1A0F/1A10/1A11`, FFB0:1 | same `04 03 01` route read as supported SparkLink V2 | not served without the keyboard's board ID | admission only (yellow) |
| IPI Flash75 (Deep Ocean / Thunder V2) | BY, `372E:10F8/10F9` (IPI routing table) | BY `94 02` | not on hubx | already covered by the generic BY path |
| AULA AG75 HE | SparkLink V2, `38A6:28DE/28DF/28E0/28E1/28F0/28F1/28F2`, FFB0:1 | same route read | not published; reset archive has no AG75 | admission only (yellow), needs VID `38A6` |
| Corsair K70 MAX | Corsair Bragi, `1B1C:1BC0` | see Corsair section | `K70MAX_1.17.96.zip` downloaded | see Corsair section |

## IPI Flash75

- IPI sells FLASH 75 in two versions (ipigame.net): "Deep Ocean Balanced" (Thunder V2,
  0.125 ms, RT 0.01 mm) and "Gemini Tri-Chip" (Kailh Seven-Star, 0.08 ms, RT 0.001 mm);
  retailers name the Gemini version "Flash75 Pro". Not in the hubx IPI catalog
  (`api.hubx.pro/v1/device/devices`, tenant 6: 13 keyboards, all 372E BY; new "+"
  revisions on `372E:10E0`: AURORA65+ `…6A`, AURORA 75 PRO+ `…5D`, QBZ65+ `…5B`,
  flash68+ `…5F`).
- IPI driver routing table `https://www.ipigame.cn/api/recognition-rules` (official):
  `1CA6:1A0F`, `1A10`, `1A11` -> `https://dc.ipigame.cn/` (updated 2026-10-09);
  `372E:10F8`, `10F9` -> `qbz.ipigame.cn` (BY); `1C4F:EE88`, `1CA3:0501`, `1CA5:0F01`,
  `1CA2:0303`, `1CA6:1A08`, `1CA2:0405` -> SparkLink PlayJoy; keyword `shan` -> `sp.ipigame.cn`.
- `dc.ipigame.cn` device filter (`device-CnP0jI_U.js`): `1CA6` PIDs `1A08`, `1A11`,
  `1A0F`, `1A10`, usage page `0xFFB0`, usage 1. Command enums are identical to the
  SparkLink V2 app HallJoy follows (Device=1/DeviceInfo=2, LayoutAndKey=3/GetKeyLayout=1,
  Performance=4/AxisData=3/Route=1, `sendReport(0, …)`); `getRouteData` parses
  little-endian 16-bit values from byte 4, as HallJoy does.
- Official SparkLink keyboard list (`getKeyboardListEncrypt`, decrypted 2026-10-12):
  `1A0F`, `1A11` = `public` (same class as all 23 admitted SparkLink PIDs that are
  listed), `1A10` = `public` + `shuangzixinAndPublic`, `1A08` = `shuangzixin`
  (双子芯, "Gemini" chip). The PlayJoy v2 app handles all classes with one device class.
- The dc site names no model per PID (it shows the USB product string). Which PID is
  the Flash75 Pro is not established; a tester log (`hid.candidate name=`) settles it.
- Firmware: `api.sparklinkplayjoy.com/api/v1/getKeyboardFirmware` needs the keyboard's
  32-bit board ID from the `01 02` reply (bytes 4..7); with a placeholder it answers
  415 "firmware not uploaded" for every PID. HallJoy logs the board ID only in the
  debug log (`[backend.spark] device_info … board=`), not in the ordinary support log.
- BY version: `372E:10F8/10F9` are BY PIDs outside the hubx catalog; the generic BY
  path (GENERIC_PROTOCOL_SUPPORT_2026-10-07.md) already asks any 372E keyboard for its
  UUID and reads keys/calibration from it. hubx firmware scan (`0x1100…00–FF`,
  `0x1200…00–3F`, 320 requests) found the same 26 UUIDs as on 2026-10-02, none for Flash75.

## AULA AG75

- AULA web-driver page lists "AG 60 HE" and "AG 75 HE" with `https://heu.aulacn.com`
  (cookie challenge, then a SparkLink-template app). Device filter (`device.js`):
  VID `1CA6` PIDs `083E 0839 083C 0836 0834 0814`, VID `38A6` PIDs `28D0–28E4`, `271C–271E`,
  `28F0–28F2`, all FFB0:1. The app's `isAG75` getter: VID `38A6`, PIDs `28DE 28DF 28E0 28E1
  28F0 28F1 28F2` (used for the lighting form only).
- Same command enums and identical `getRoute`/`getRouteData` as the IPI and PlayJoy apps.
- SparkLink list classes: `28DE` public, `28E1` shuangzixin, `28DF 28E0 28F0 28F1 28F2`
  `other` (FFB0:1 like the rest; served by the brand hub, not by the PlayJoy app).
- HallJoy admits SparkLink only for VID `1CA6` (`sparklink_model_profiles.h`), so the
  AG75 needs a new VID in the exact identity list.
- Firmware: AULA "Magnetic Keyboard Reset Firmware.zip" (aulastar.com /other/, 2026-04-23,
  SHA-256 0c5263d7…c403) holds F75 HE, HERO 68 HE/Ultra, HERO 75 HE, HERO 84 HE,
  MINI 60, WIN60/WIN68 updaters; no AG75. SparkLink firmware API (AULA key) needs the
  board ID as above.
- Sheet: AULA AG60/AG63/AG75/BOX63 rows are "Research incomplete" (snapshot 2026-10-11).

## Corsair K70 MAX

- Corsair firmware storage (`/firmware-storage/firmware/public/mapping.json`, used by
  the official web Firmware Update Utility): "K70 MAX" = `1B1C:1BC0`, protocol `bragi`,
  package `K70MAX_1.17.96.zip` (application 1.17.96, needs iCUE >= 5.27), SHA-512 equals
  the manifest. Contents: `Aldaris_App_v1.17.96.bin` (220,672 bytes, not encrypted:
  Cortex-M vector table, SP 0x20030000, reset 0x00010215, build "Mar 27 2025") and its
  JSON (protocol-version 154).
- Corsair Web Hub supports VANGUARD PRO 96 and MAKR (PRO) 75 magnetic boards, not the
  K70 MAX. Its Bragi notification enum has `AnalogKeyPressChange = 12`, but the Web Hub
  never subscribes to it (key test uses browser key events).
- Source: official Corsair firmware storage (`/firmware-storage/firmware/public/fw/`);
  the zip matches the manifest SHA-512, the image matches its JSON SHA-512
  (image SHA-256 e6bd2b3a…fb22).
- Web Hub key test and calibration use browser key events and `calibrationControl`
  (start, MagneticSwitch); `KeyActualService` is actuation settings. No public Corsair
  client reads live per-key depth. No community project (ckb-next, OpenRGB) supports
  K70 MAX analog data; ckb-next has an open K70 MAX request.
- First static pass of the image (capstone, Thumb, base 0x10000; unoptimised -O0 code,
  no symbols): actuation values are clamped to 1..40 (0.1 mm steps, 4.0 mm); no emitter
  of notification 12 identified yet. Establishing whether the K70 MAX sends
  `AnalogKeyPressChange` (and what enables it) needs the Bragi command dispatcher and
  notification path reversed; that is a separate, larger task.
- Verdict (updated after the firmware reverse below): the K70 MAX firmware 1.17.96 has no
  live analog key reader and no analog notification; see "firmware reverse result". Not
  suitable for HallJoy; no support work started.

## Next steps (proposed)

1. SparkLink admission for the IPI and AULA PIDs above (exact identities, yellow notice,
   VID `38A6` added to the SparkLink identity check), same pattern as Everglide SU75 Pro.
2. Log the SparkLink board ID in the ordinary support log, so a tester log allows the
   firmware request.
3. Ask the Discord tester for HallJoy.log with the keyboard connected (model PID).
4. Corsair: done, see "firmware reverse result" (no analog path in K70 MAX 1.17.96).

## Corsair K70 MAX: firmware reverse result (2026-10-12)

Image `Aldaris_App_v1.17.96.bin` (220,672 bytes), load base `0x10000` (address = file offset +
`0x10000`). Evidence: reset vector `0x10215` is Thumb code at file offset `0x214` (`cpsid i;
ldr r0,[pc,#0x10]`); vector entries point to `ldr r0,[pc,#0x100]; bx r0` stubs. Two earlier
drafts used base 0; that was wrong. The USB stack itself is in on-chip ROM (the application calls
it through a table at `0x13001100`), so the application only exposes class callbacks. Static
analysis plus a unicorn run of the boot path (descriptor unpacking) were used; no hardware.

HID interfaces. Descriptors are LZ-unpacked into RAM at boot (init table `0x456F8`, unpacker
`0x10664`; plain bytes also visible in flash at `0x45915`/`0x45B04`). Four interfaces:

| Interface | Descriptor | Object | Role |
| --- | --- | --- | --- |
| keyboard | report IDs 8 (13 keys), 1 (104-bit bitmap + modifiers), 2 (consumer) | `0x20010FC8` | normal typing |
| vendor `FF42:1` | 128-byte input and output | `0x20010FD0` | Bragi commands |
| vendor `FF42:2` | 64-byte input only | `0x20010FD4` | device notifications (EP3) |
| mouse | 8 buttons, 16-bit X/Y, wheel | `0x20010FCC` | pointer |

Bragi command path (v1.0 header, 2 bytes). Class callback `0x180C4` re-arms OUT, copies the 128
bytes from `0x2000FD00` into a 15-slot request ring at `0x2000F500` (index byte `0x20011F1E`).
`0x24084` pops a slot and calls the dispatcher `0x28AAC` (`byte1 & 0x3F`, 18-entry `tbh`). The
reply is built in `0x2000F480` as `[idx | status<<4, cmd, status, data]` and sent by `0x2FEA0`.
Commands: 1 set property (`0x1DEF8`), 2 get property (`0x1D78C`), 3 get multiple, 4 mode/sub-ops
(`0x1DD00`, drives the state variable `0x20011E50`), 5 close handle (`0x3E36C`), 6 write
(`0x41A08`), 7 continue write (`0x40F20`), 8 read (`0x3A99C`), 9 probe (`0x292EC`), 10 reset,
11/12 file operations (`0x1F37C`/`0x3B07C`), 13 open handle (`0x393E0`), 15 refresh, 18 mode check;
14, 16, 17 and above 18 answer status 4. Handles are internal files named `ID%04X.hex` (profiles,
lighting, actuation settings), not live data. The property getter covers IDs 1..255 but only 31
are implemented, all configuration or status bytes from the settings struct at `0x2000CDFA`
(plus `0x3D`, a timer value). None returns key position.

Notification channel (64 bytes, EP3, sender `0x11DA0`, called from the task at `0x23E10` and
`0x23E56`). Exactly two packet buffers exist, `0x2000EB6E` and `0x2000EBAE`, with ready flags at
`0x2000EBF3` and `0x2000EBF4`:
- buffer 1: ID 2 `KeyPressStateChange`: a bitmap of currently pressed keys (byte `2 + key/8`,
  bit `key%8`; set/clear helpers `0x22FC8`, `0x23020`, `0x23080`, `0x230D8`), digital state after
  the firmware's actuation threshold; ID 1 `PropertyValueChange` (builder `0x2313E`, property `0x76`).
- buffer 2: ID 3 `CalibrationProgress` (builder `0x23D40`; per-key byte pairs packed by the
  calibration state machine at `0x242xx`-`0x24Axx`, state `0x20011E50` values `0x0F`..`0x1E`).
No code writes a notification ID 12: every write to the ID bytes (`+0x87`, `+0xC7`) stores 1, 2
or 3, and the send function is only ever called with these two buffers.

Result. K70 MAX firmware 1.17.96 provides no live per-key analog depth over USB in normal
operation: only digital pressed/released bits (all keys at once, at the firmware actuation
point) and calibration-time progress. `AnalogKeyPressChange` (12) is defined in the Web Hub
enum for other Corsair products but is not implemented here. This does not meet the HallJoy game
requirement (analog travel of all keys at once, no calibration mode, no single-key limit).

Limits: static analysis of the application image only; the ROM is not available, so an
emitter that writes the buffers through pointers computed elsewhere is not excluded with
certainty, but none of the 55 references to the mode variable, the 73 references to the
buffer base or the sender's single call site points to one. Scripts were scratch files
(not kept). Re-check if Corsair publishes a newer K70 MAX firmware. The Web Hub also lists
VANGUARD PRO 96 and MAKR (PRO) 75 magnetic boards with separate firmware that may differ.
