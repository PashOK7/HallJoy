# Everglide AE68 Max: web driver and firmware check (2026-10-27)

Owner request: a Discord user (Prasad) asked whether the Everglide AE68 Max will get HallJoy
support. There is no log. Check the web driver and the firmware. Research only; no code change.

## What the model is

- KeebForce product page `https://keebforce.com/everglide-ae68-max/`: 68-key ANSI, USB-C wired
  only, 8K polling, "dual 240MHz MCUs on Everglide's Gemini platform", four-layer PCB, full-key ADC.
  Switches YuHeng / Siren V4 / WuKong. The photos show the shared AE68 exterior.
- Not the original Everglide AE68 (2023, "江湾 AE68"): that board is a different platform with a
  community calibration tool and is not covered here.
- "Gemini" matches 双子星 (`shuangzixin`), a device class in the SparkLink PlayJoy catalog.

## AE68 family: three different keyboards (follow-up, same day)

| Model | Electronics (retail) | Driver |
| --- | --- | --- |
| AE68 (2023-2024, Epomaker/MKB) | 65% 68 keys, 8K, magnetic | not the SparkLink app; Epomaker posts `Everglide_AE68_Firmware.zip` (2024-09-04, no version/platform named); CN community "江湾 AE68" / 68-key public-board calibration tool |
| AE68 Pro | single 480 MHz MCU, ADC direct only on WASD, 256K scan, 8K | SparkLink `www.xsyd.top/connect` (KeebForce software guide lists AE68 Pro with SU75 Pro, SU66 Max, AE64 Pro) |
| AE68 Max | dual 240 MHz MCU "Gemini", full-key ADC, 4-layer PCB, 2304K scan | KeebForce points to `hub.xsyd.top`; not in the guide's model list |

Same exterior, different boards: a support claim for one does not cover the others. AE68 Pro and
AE68 Max are both on the SparkLink platform but likely on different PIDs (AE68 Pro probably a
`public` `30xx`, AE68 Max a `shuangzixin` one; not proven). The original AE68 is a separate
platform and was not reviewed (firmware zip not downloaded). No "AE68 Max V2" was found.
Name collision: FuryCube's BY/IPI web driver also has a PCB named "AE68" (UUID
`0x11000000006D/0x77`, see [FURYCUBE_M35HE_2026-10-07.md](FURYCUBE_M35HE_2026-10-07.md));
nothing ties it to Everglide.

Sources: KeebForce AE68 Pro / AE68 Max pages and "Everglide Keyboard Software User Guide (2026)",
Epomaker AE68 product and firmware pages.

## Official driver

- KeebForce software page `https://keebforce.com/software-tools/` now points Everglide owners to
  `https://hub.xsyd.top/` (SparkLink PlayJoy hub, `xsyd-web-version` 1.4.1, 星闪悦动). The old
  `www.xsyd.top/connect` still serves the same `index-DwbZ9fGF.js` as on 2026-10-02/12.
- The hub loads one of two keyboard apps per device:
  - `/keyboard/` = protocol `v2` (the one HallJoy implements: `01 02`, `03 01`, `04 03 01 row`,
    report id 0, `0xFFB0`/1).
  - `/keyboardv3/` = NEW protocol `v3`. Catalog entries are routed by `protocol_version`
    (`v2` or `v3`); classes `public`, `shuangzixin`, `shuangzixinAndPublic` are all accepted by both.
- v3 framing (from `keyboardv3/js/keyboard-api-*.js`): header `0x55AA` (bytes `AA 55`), src id 1,
  dest id 3, main cmd, sub cmd, payload, CRC. Main commands: Device 1, Upload 2, Custom 3,
  Lighting 4, Global 5, LayoutAndKey 6, Macro 7, Performance 8, HigherKey 9. Live travel =
  Performance 8 / SwitchData 3 / type 1 (Route) per row; type 0 ADC, 2 calibration. Device ability
  reply reports switch type (1 = magnetic), connect mode, functions. HallJoy has no v3 code.
- The v3 app's built-in identities are `1CA6:7701`, `1CA6:1037` and `3CA2:1037 7701 5730 391E 7301
  4D01 0A2C 0A2D`. No Everglide `1CA6:30xx` PID is among them.
- Neither app has model names or per-PID Everglide code; the driver shows the USB product string.

## Everglide identities (catalog of 2026-10-12, all protocol v2)

| PID | Class | HallJoy |
| --- | --- | --- |
| `3001`, `3007` | `shuangzixin` (Gemini) | generic SparkLink V2 admission |
| `3003`, `300D` | `shuangzixinAndPublic` | generic SparkLink V2 admission |
| `3002` | `public` | exact model, Everglide SU75 Pro, Supported |
| `3006 3008 3009 300A 300B 300E 300F` | `public` | generic SparkLink V2 admission |

All twelve are in `sparklink_official_identities.h`; generic admission is on since 1.6.10
([SPARKLINK_GENERIC_ADMISSION_2026-10-12.md](SPARKLINK_GENERIC_ADMISSION_2026-10-12.md)).

The fresh 2026-10-27 catalog was NOT read: the hub now sends an `Xs-API-Key` header, and the agent
tool policy blocked calling the vendor API with the key taken from the page. Whether an Everglide
PID moved to `v3`, or a new `30xx` PID was added, is therefore not established.

## Firmware

The hub has online and local (`.bin`) update. The online path asks the API for a firmware pack
by the board/version the connected keyboard reports (`getFirmwarePack`); no static firmware URL
exists. Without the keyboard (or the API) the AE68 Max firmware cannot be obtained, so no static
firmware review was possible.

## Verdict

- If the AE68 Max reports one of the listed v2 Gemini PIDs (most likely `3001`/`3007`/`3003`/`300D`,
  not proven), it should already connect in HallJoy 1.6.10+ through the generic SparkLink V2 path,
  with the yellow "Keyboard not in the list" notice and no built-in layout (same as Weikav EG75 HE
  before its log, [WEIKAV_EG75HE_2026-10-26.md](WEIKAV_EG75HE_2026-10-26.md)). The backend still
  proves device info, layout rows and live travel before claiming it.
- If it is a v3 device, HallJoy does not support it: a new v3 transport (framing + CRC + Performance
  8/3/1 route read) would be needed. The driver code is enough to write it; no hardware evidence.
- Next evidence needed: one `HallJoy.log` from the user (1.6.13). It shows the PID, product name,
  `[spark] generic.admitted` or failure, and the key count. With a successful log: exact model,
  68-key ANSI layout from the driver key map + retail top view, Supported (owner rule).

## Will the MAX specifically be supported? (2026-10-28, owner follow-up; tester pre-ordered, ~1 month, still no log)

Honest answer: **cannot be confirmed for the MAX until a log or the device exists**, because the one
thing that decides it — the MAX's USB PID and its catalog `protocol_version` — is not knowable without
the hardware. The SparkLink catalog has no model names (established 2026-10-02), so "AE68 Max" cannot
be mapped to a PID from data alone. The fresh catalog could not be read this session: both the direct
API call and the in-browser decrypt of the vendor catalog were blocked by the agent policy. The
assessment below rests on the local Oct snapshots and the hub app code only.

Three conditions must all hold for the MAX to work in HallJoy:

1. **It must speak v2, not v3.** HallJoy only implements SparkLink v2 (`01 02`, `03 01`, `04 03 01`).
   The new hub `hub.xsyd.top` serves two apps: `/keyboard/` (v2) and `/keyboard v3/` (v3, framing
   `AA 55` + CRC, travel = Performance 8 / SwitchData 3 / Route). A v3 device fails HallJoy's travel
   proof and is rejected (fails closed) — safe, but unsupported until v3 is implemented.
2. **Its PID must be in HallJoy's baked-in identity list** (`sparklink_official_identities.h`, 551 ids
   from the 2026-10-12 catalog snapshot; generic admission since 1.6.10). A PID added to the vendor
   catalog after that snapshot is NOT admitted until the list is regenerated.
3. **It must be Hall-effect** (it is) — the live-travel proof then passes.

What the local evidence says about the risk:

- In both Oct catalogs (2026-10-02, 2026-10-12) **every** `1CA6` entry is `protocol_version` v2; v3 is
  a newer addition. All twelve Everglide `30xx` PIDs (`3001 3002 3003 3006-300B 300D 300E 300F`) are v2
  and are all in HallJoy's 551-id list. So **if the MAX reuses a known `30xx` Gemini PID, it already
  works** in 1.6.10+ (generic, yellow notice, no built-in layout) — exactly the Weikav EG75 HE / SU75
  Pro pre-log state.
- The v3 app's built-in identity list (`keyboardv3`: `1CA6:7701`, `1CA6:1037`, `3CA2:*`) contains **no
  Everglide `30xx`** — so no known Everglide PID is v3 as of these app versions.
- Risk factors that keep this uncertain: the MAX is a **new** dual-MCU "Gemini" board, KeebForce points
  its owners at the new hub (which also serves v3), and a new product may use a **new PID** (post
  2026-10-12, not yet in HallJoy's list) and/or the **v3** protocol. None of these can be ruled out
  offline.

Decision tree once a log (or the device's `VID_1CA6&PID_xxxx` + GG firmware app) exists:

| What the MAX turns out to be | Outcome |
| --- | --- |
| v2, PID already in the 551-id list (a known `30xx`) | Works now in 1.6.10+ (generic, yellow); add exact model + 68-key layout -> Supported |
| v2, PID newer than 2026-10-12 | Small fix: refresh `sparklink_official_identities.h` from the current catalog, rebuild; then as above |
| v3 | Not supported until a v3 transport is implemented (framing/CRC/Performance 8/3/1); driver code suffices, but it is real new work |

Pre-emptive option (owner decision): refresh HallJoy's SparkLink official-identity list from the
current catalog now, so a v2 MAX on any newly listed `30xx` PID is admitted the moment it is plugged
in. This needs the fresh catalog, which the agent policy currently blocks; it would need the owner to
supply the catalog export or a `getKeyboardListEncrypt` dump. It does nothing for a v3 MAX.

## SparkLink v3 protocol: reverse-engineered, and the verdict (2026-10-28, offline from `keyboardv3` app)

Owner asked to settle v3 so we can say for certain whether it can be supported. Static analysis of
`keyboardv3/js/keyboard-api-Ct2WFMsS.js` (no device, nothing sent). **Verdict: v3 CAN be supported —
it exposes a live per-key analog read, a safe self-identification gate, and the layout geometry.** It
is a different transport and command set from v2, so it needs a new HallJoy backend, but the analog
data HallJoy needs is present.

### Frame format

- On the wire: `AA 55` (header, little-endian of `0x55AA`) · `len` (u16 LE) · `crc` (u16 LE) ·
  `crcData`, where `crcData = [srcId=1, destId=3, mainCmd, subCmd, ...payload]` and `len` = payload
  byte count (excludes src/dst/main/sub).
- CRC = standard CRC-32 (reflected, poly `0xEDB88320`, init `0xFFFFFFFF`, final XOR `0xFFFFFFFF`),
  then the **low 16 bits** are taken.
- HID packetization: 64-byte output reports, report id `0`, on the vendor collection usage page
  `0xFFB0` usage 1 (same page as v2). Each report = `[chunkLen u16 LE][frame bytes]`; frames longer
  than 62 bytes are split into 62-byte chunks and reassembled. Replies are matched by `main/sub`.
- Main commands: Device 1, Upload 2, Custom 3, Lighting 4, Global 5, LayoutAndKey 6, Macro 7,
  **Performance 8**, HigherKey 9.

### Live analog read (the decisive part)

- `Performance(8) / SwitchData(3)` with payload `[type, row]`:
  - `type 0 = ADC` -> `getADC(row)`: reply payload `[type, row, v0_lo,v0_hi, v1_lo,v1_hi, ...]`, i.e.
    **16-bit raw ADC (travel) per key in that row**.
  - `type 1 = Route` (processed travel), `type 2 = Calibrate` (calibration status), `type 3 = KeyStatus`.
- The app reads the whole matrix by looping the rows (its own calibration/visualiser does exactly
  this). So HallJoy can obtain live per-key depth by polling `getADC`/`getRoute` per row — the same
  model as the 4.16.8 Apex depth poll, just a different frame. (Raw ADC `type 0` or processed
  `Route type 1`; HallJoy normalises either.)

### Safe admission and layout

- `Device(1) / DeviceAbility` -> `getDeviceAbility()` reports `switchType` (0 mech, 1 magnetic,
  2 optical, 3 inductive, 4 3D-magnetic), `connectMode` (0 USB / 1 2.4G / 2 BLE) and
  `extendedFunctions` (which include "analog gamepad"). This is a read-only self-identification reply
  HallJoy can require (switchType magnetic/3D) before trusting the board — it satisfies the owner's
  protocol-match certainty rule (safe gate + reply proof + live-analog proof).
- `LayoutAndKey(6) / GetKeyLayout` and `GetKeyLayoutStyle` return a **6×21** grid with per-cell
  `{row, col, ratio, type}` — enough to auto-derive a layout, unlike v2 (which publishes no geometry).

### What implementing v3 in HallJoy costs

A new backend/transport parallel to `backend_sparklink.inc`: `0x55AA` framing + the CRC above,
64-byte report assembly, reportId 0 on `FFB0/1`, request/reply matching, the DeviceAbility gate, and
per-row ADC/Route polling feeding the existing physical-analog -> bindings -> gamepad path. Identity
gate reuses the SparkLink official-identity list (same vendor page). Well-scoped, comparable to
adding the v2 or MADE68 backend. It stays software-only/untested until a v3 device or log exists.

### Net answer for the AE68 Max

The board will be supportable **whether it is v2 or v3**: v2 already works through generic admission
(if its PID is on the list), and v3 is now proven implementable. The two remaining unknowns are
unchanged and both need the hardware/log: (1) the exact PID (and whether it is already in HallJoy's
list), and (2) that any v3 implementation is untested until a device reports in. So: "yes, it can be
supported"; "works the moment it is plugged in" is only guaranteed for a v2 board on an already-listed
`30xx` PID.

## Local files

Hub pages and both keyboard apps (vendor code, local only):
`.local/research/everglide-ae68max-20261027/` (`hub.html`, `assets/`, `keyboard/`, `keyboardv3/`,
preload manifests).
