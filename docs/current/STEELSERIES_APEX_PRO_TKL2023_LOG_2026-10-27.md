# SteelSeries Apex Pro TKL 2023: user log (2026-10-27)

Discord user: "using apex pro TKL 2023 wired, which is listed as experimental on github, any idea
how to get it to be recognized?" Screenshot: red "No supported analogue keyboard detected" banner.
Log `HallJoy (24).log` (HallJoy 1.6.13.0, 2026-10-27 19:53 UTC). Analysis only; no code change.

## Log

- HID candidates: `1038:1628` "SteelSeries Apex Pro TKL 2023" on interfaces 00-04; also
  `1038:1850` Aerox 5, `1038:12E0` Arctis Nova Pro Wireless, LianLi, ITE (unrelated).
- `apex.backend_start value=2 error=0`; `native index=17 id=steelseries_apex observation=not_present
  vid=1038 pid=0000` in every snapshot, no failures, no access busy. The backend never selected the
  keyboard: its allowlist is `1610`, `1614`, `1640` only.
- `support.banner value=1` = the generic red banner (correct for an unadmitted keyboard).

## Why

The GitHub "experimental" entry is the ORIGINAL Apex Pro TKL (`1038:1614`, firmware 4.16.8). The
TKL 2023 is a different board: GG bundle `272111144` (`apex_pro_tkl_2022`), firmware 1.19.7, the
modern command table without the legacy analog reads `0x57`/`0x5A`
([STEELSERIES_FAMILY_REVIEW_2026-09-25.md](STEELSERIES_FAMILY_REVIEW_2026-09-25.md), "Newer firmware
limits found"; SUPPORTED_HARDWARE: "Mini, TKL2023, TKL Gen3 and Wireless ... remain unadmitted").
Adding `1628` to the allowlist would send 4.16.8 commands to a firmware that does not implement them,
so it is not a fix.

The GG device descriptor for this model (`apex_pro_tkl_2022.edevice`) is encrypted; the 2026-09-25
review did not find a live per-key depth read in the 1.19.7 tables, which is not proof of absence.

## What would be needed

Reverse engineering of `firmware-apex-pro-tkl-1.19.7.bin` (SHA-256 6887795b..., local
`.local/research/steelseries-apex-20260925/`): dispatcher, a multi-key live ADC/depth read (if any),
reply format, sensor map; then a read-only diagnostic build for this user.

OWNER rule (2026-10-27): commands may be sent to a device only when the firmware/driver shows an
explicit, intended mechanism for them.

### Static review started (2026-10-27, offline, nothing sent to any device)

- Image base `0x08009000` (only base where code references the command table), size `0x17000`,
  initial SP `0x2000A000`, reset `0x0801265D`.
- USB command dispatcher `0x0801170C`: request byte 0 = command; bit 7 set selects the record's read
  handler (`+4`), clear selects the write handler (`+8`). Table `0x0801C158`, 36 records of 12 bytes
  (cmd, read, write); the 2026-09-25 list of commands at file offset `0x13158` is this table.
- Reply: byte 0 = request byte 0, byte 1 = handler status, payload from byte 2; payload limit 62
  bytes (64-byte endpoint `0x82`) or 642 bytes (`0x284` total) when the feature-report flag
  `0x2000528F` is set. Handler call: r0 = masked command, r1 = request + 1, r3 = reply + 2,
  [sp] = pointer to the remaining length.
### Full command-surface review (2026-10-27, offline; nothing sent to any device)

Method: static disassembly (Capstone) plus offline emulation of every read handler (Unicorn, as in
`tools/verify_apex_pro_firmware.py`) with synthetic RAM — reply size and read sources measured. No
USB, no device, no command issued. Scripts: `.local/research/steelseries-apex-tkl2023-20261027/`.

- HID descriptors (file `0x15DF0+`): vendor collection `FFC0` = 64-byte Input `0xF0`, 64-byte Output
  `0xF1`, 644-byte Feature `0xF2` (the command channel); plus boot keyboard and an NKRO bitmap. There
  is **no analog HID axis / gamepad / joystick collection** — the only analog path is the vendor
  command channel, exactly what HallJoy's `steelseries_apex` backend uses.
- The 36-record table is the top level. Four records are **nested sub-dispatchers** (handler
  `0x800F159`) for command groups `0x1F`, `0x24`, `0x38`, `0x73`, each with its own 12-byte-record
  table; group `0x73` also has a further runtime namespace (sub-command `0x11`, handler table in RAM
  `0x2000060C`, 2 entries, populated at boot — the SteelSeries engine bridge, not statically resolvable).
- Every read handler (top level and nested) was emulated. The host-readable data is **configuration
  and status only**:
  - per-key actuation / rapid-trigger **setpoints**, 4–5 bytes per key (e.g. group `0x73` id `0x51`
    `@0x8014EE1`: 5 bytes × N keys; config array `0x20001DA8`), written by matching write handlers —
    i.e. host-set static values, not live readings;
  - per-key calibration **range** (static), 640-byte profile blobs (group `0x38` id `0x83`
    `@0x8018375` and group `0x1F` id `0x81` `@0x80182B7`, both `memcpy(reply, cfg_base+0x7DA, 0x280)`,
    feature path only), small 32-byte / scalar state blocks, firmware version.
  - **No handler returns live, continuously-sampled per-key ADC depth.** The 4.16.8 live path
    (command `0x57`/`0xD7`, banks of raw+filtered ADC, and the per-key live read) has **no counterpart**
    anywhere in the 1.19.7 command surface.
- Autonomous IN reports: the send routine `0x800ECE2` has four callers — the dispatcher reply (ep
  `0x82`), a fixed status/heartbeat report (`@0x800FED6`, ep `0x85`), and an engine-message queue
  drain (`@0x80124FE`, ep `0x85`). None streams a per-key depth array; the device never pushes live
  depth on its own.

### Telling the supported TKL from the unsupported ones (PIDs from the firmware images)

Only the original Apex Pro TKL (`1038:1614`, firmware 4.16.8) is supported. Every other TKL is a
different firmware family and is not. What matters is the firmware family, not the "gen" label: the
full-size 2024/Gen 3 is still 4.16.8 (`1640`, supported), while the TKL Gen 3 is 1.19.7 (`1642`, not).

| Model | VID:PID | Firmware | HallJoy |
| --- | --- | --- | --- |
| Apex Pro TKL (original, 2020) | `1038:1614` | 4.16.8 | **Supported** |
| Apex Pro TKL 2023 | `1038:1628` | 1.19.7 | Not (no live depth) |
| Apex Pro TKL Gen 3 (2024) | `1038:1642` | 1.19.7 | Not |
| Apex Pro TKL Gen 3 Cyberpunk / Steel | `1038:1660` / `1662` | 1.20.0 | Not |
| Apex Pro TKL Wireless / Wireless 2024 | `1038:1632` / `1646` (dongles `1630`/`1644`) | 3.24.1 | Not |
| (ref) Apex Pro full-size | `1038:1610` | 4.16.8 | Supported |
| (ref) Apex Pro Gen 3 full-size | `1038:1640` | 4.16.8 | Supported |

How to check a given board: the HallJoy log (`Open log`) prints `hid.candidate vid=1038 pid=....`;
or Device Manager -> the keyboard -> Details -> Hardware Ids -> `VID_1038&PID_xxxx`; or the firmware
version shown in SteelSeries GG (4.16.x = the supported family, 1.19.x/1.20.x = not). The USB product
string also differs ("SteelSeries Apex Pro TKL" vs "... TKL 2023").

### Conclusion

On firmware **1.19.7 the Apex Pro TKL 2023 (`1038:1628`) exposes no host-readable live per-key depth
by any intended mechanism** (no command, no feature read, no autonomous report, no analog HID axis).
HallJoy drives a gamepad from live depth, so the board **cannot be supported on this firmware** —
adding `1628` to the allowlist would only send 4.16.8 commands the firmware does not implement. This
is the OmniPoint 2.0 generation where SteelSeries moved analog handling on-device; any depth the
firmware computes for its own actuation is not offered to the host.

Per `FIRMWARE_BEHAVIOR_REVIEW.md`: bounded emulation is not proof of exhaustive absence. Not resolved
statically: the 2-entry runtime engine namespace (sub-command `0x11`) and the exact 640-byte blob
layout — both are config/engine, with no sign of a live ADC export. A conclusive negative, or a hidden
live path, would need the GG client's USB capture against this exact model (owner decision; not a
tester workflow). Nothing was flashed, sent, or changed.
