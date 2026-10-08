# FuryCube M35HE — research (2026-10-07)

Owner request: see what is available for FuryCube M35HE (download firmware as
usual); no build yet. Research files: `.local/research/furycube-m35he-20261007/`
(firmware, updater and web-driver copies stay local).

## Tester log `HallJoy (58).log` (1.6.7, 2026-10-07 13:49 UTC)

- `372E:10A3`, USB product name "M35HE", interfaces 00/01/02.
- The Addressed backend (`addressed-099402`, protocol 3) sees it
  (`present_not_connected`, default 82-key map, 11 failures) but never
  connects: like RK68 HE before 2026-10-02, PID `10A3` is not admitted to the
  exact IPI UUID path (only 105C/106C/10BF/10C0), and the generic path is not
  answered. No analog source.

## Official sources (furycube.com product page "M35HE")

- Firmware: `cdn.shopify.com/.../M35HE_V1.6-_0.01_B08_8K_ARGB_32K_20260630_1.exe`
  (SHA-256 `b011c5a8...8ab7`). Parsed as a ZIP container, never run:
  `BY_UpgradeTool.exe`, `config.json`, `firmware.bin` (164480 bytes, SHA-256
  `8fd4743b...1da8`). Config: BeiYing PixArt 8K, `pass` = UUID
  `110000000065`, `372e:10a3`, upgrade method GEEHY_USB_V2, version 0105.
- Web driver `http://www.he.furycube.com/` (BY "bytech" Vite app): one device
  file per UUID under `brands/furycube/devices/`; `0x110000000065` = PCB RX194,
  37 keycaps with physical IDs and geometry. Desktop driver (pCloud) not taken.

## Firmware audit (same method as the RK68 HE sibling audit)

`extract_audit.audit()` on `firmware.bin`: UUID `0x110000000065`, 372E:10A3,
dispatcher `tbh`, command `94` -> subcommand 2 addressed live-read handler with
the record layout HallJoy parses (`record_layout_ok`), `83` map handler, 108
matrix slots, 37 physical IDs. Handler shape identical to the RX195 (X68)
image `0x110000000061` on the same PID. The web driver's 37 keycap IDs equal
the firmware ID table exactly.

Layout (HallJoy `ipi::factoryHids`): Esc F1-F6 / grave 1-6 / Tab Q W E R T /
Caps A S D F G / LShift Z X C V B / LCtrl, Fn (72), ID 112 (not in the factory
table; between Fn and LAlt, probably Win), LAlt, Space (3u). The live map
(`83 00`) read at admission returns its actual code.

## Conclusion

M35HE is on the BY/IPI read-only path HallJoy already uses (`94 02` live read,
`94 05` calibration, `83 00` map); its own published firmware confirms the
handler. Support needs: admit `372E:10A3` to the IPI UUID path (UUID decides;
10A3 is shared with RX195 X68 / M68 HE UK), a firmware-locked catalog entry
for `0x110000000065` (37 IDs), the factory code of ID 112, and a 37-key
layout from the web driver geometry. Not started (owner: look only).

## Other FuryCube devices in the same web driver

| UUID | PCB | Keys |
| --- | --- | --- |
| 0x110000000046 | RX176 | 64 |
| 0x110000000060 | — | 68 |
| 0x110000000061 | ZHY_195 (RX195 X68) | 68 |
| 0x110000000064 | RX195 UK | 69 |
| 0x110000000065 | RX194 (M35HE) | 37 |
| 0x110000000066 | RX180 | 80 |
| 0x110000000069 | RX180UK | 81 |
| 0x11000000006D / 0x77 | AE68 | 67 / 68 |
| 0x11000000006E / 0x74 | AE75 / AE75 UK | 84 / 85 |
| 0x110000000071 / 0x73 | RX175 / RX175 UK | 67 / 68 |
| 0x110000000076 | MT87 | 87 |
| 0x110000000080 | SG005 | 30 |
| 0x140000000007 | 2829 | 99 |

Retail names (M30HE, M68HE, M68HE v2, M68HE XS) are not yet matched to UUIDs.

## Update (same day): covered by the generic BY path

Owner then asked for generic yellow support of keyboards on known protocols
(`GENERIC_PROTOCOL_SUPPORT_2026-10-07.md`). M35HE (and the other FuryCube BY
UUIDs above) now connect through the generic BY path: UUID outside the
catalog -> IDs, live map and calibration read from the keyboard, yellow
"Keyboard not in the list" notice, default 100% layout (no M35HE layout yet).
ID 112 uses its live code. Not physically tested.

Sheet (after the owner reconnected the Google Sheets connector, 2026-10-07):
live Main!C321 FuryCube / M35HE "Not investigated" -> "Implemented; awaiting
hardware testing"; B321:C321 background/text colors set to the yellow pair
(borders untouched, middle-of-block pattern equal to row 301). Readback: value
and effective yellow verified; `check_keyboard_sheet_structure.py` on a fresh
full native read (`.local/sheet-20261007-native.json`): PASS, 150 blocks, 0
issues. Catalog: `keyboard_support_notices.json` group `GenericProtocol`
(runtime flag from telemetry, no generated condition). M30HE and M68HE stay
"Not investigated" (retail names not matched to UUIDs).
