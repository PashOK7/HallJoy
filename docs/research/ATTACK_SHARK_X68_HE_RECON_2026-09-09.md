# Attack Shark X68 HE: acquisition and analog protocol reconnaissance

Status: **X68HE vendor-client route established; reference X65HE firmware
reverse-engineered and handler emulated. X68HE firmware NOT acquired. No
production or diagnostic keyboard support added.**

Do not describe the reference image, the installer or an emulated handler as
the requested X68HE firmware or as physical validation of simultaneous typing.

## Acquisition

The [official product page](https://attackshark.com/products/attack-shark-x68-he-rapid-trigger-keyboard-magnetic-switch)
links [qmk.top](https://qmk.top/) and
`https://support.attackshark.com/ATTACKSHARK/X68HE/ATTACKSHARKX68HE.zip`.
The official ZIP advertises 101,813,504 bytes; download/range attempts stalled
after approximately 15 KB. Those partial files are not usable archives.

A desktop driver was obtained from the
[cykorr mirror](https://github.com/cykorr/AttackSharkX68HE/releases/tag/main),
`ATTACK.SHARK.X68HE.SOFT.exe`, 101,868,672 bytes, SHA256
`3EE70880C8B5ADCD469A0E3EBEFA7E280A22FA23FF74E369955511A2C58E04A2`.
This is a mirrored vendor installer, **not a directly downloaded official ZIP**.
It was unpacked with 7-Zip; neither it nor its driver executables was launched.
It contains model-specific JavaScript and no acquired X68HE firmware image.

Artifacts live in `.local/research/attackshark-x68he/`.

| Artifact | SHA256 |
|---|---|
| Current web `index.Bs-RnLKs.js` (saved as `index-alt.js`) | `E9F67D61CABF56D9A13086E7F9CE0244C8828AF2DB65B553AB23E7FB843E0F58` |
| Desktop `index.b078bf5f.js` | `5A0A28C49E629E8DC6E517C7C99008C88120F5CBBE9676E54074B4EB1CE3C476` |
| Desktop shared RY5088 class `5e635fe2.js` | `99DB04B99796DD3D604B9D207F13A122BAA878F30546B586CBF000A92CF74512` |

The current web bundle lacks the target model entries. The desktop registry,
loader `4a39166d.js` and model chunks establish these identities:

| dev_id | Internal model | Chunk | USB VID:PID |
|---|---|---|---|
| 2270 | ry5088_x68rt004_8k_dm | 51bbd794.js | 3151:502D |
| 2472 | ry5088_x682_8k_dm | bd86972c.js | 3151:502D |
| 2902 | ry5088_x68v2_8k_dm | 69a807f0.js | 3151:502D |

All three inherit the same shared command class. Their factory matrices are
512-byte arrays of 128 four-byte records. 2270/2472 match byte-for-byte; 2902
differs. All three put A/D/S/W at slots 9/21/15/14. Do not confuse slots with HID
usages or assume that every nonzero factory entry is a physically present key.
Do not route every VID 3151 device here: this platform also hosts mice and many
unrelated keyboard models. Application identity uses command 8F and a little-
endian 32-bit dev_id at response bytes 1..4 (vendor payload coordinates).

Both official clients implement firmware lookup as
`POST https://api2.rongyuan.tech:3816/api/v2/get_fw_version`, JSON
`{"dev_id":N}`, then GET `/download/<file_path>`. A single-component version is
raw DEFLATE; multi-component packages are ZIPs with `firmwareFile.bin`, etc.
No connected HID device, user login or flashing is needed for these downloads.
Replaying this exact lookup for **2270, 2472 and 2902 returned Record not found**.
Alternate client hosts did not provide those records either. A bounded check of
the same IDs and filename versions v100..110/v200..210/v300..310 found no image;
this is not proof that every historical filename is absent. Browser device
emulation cannot make the server return a missing record for the same dev_id.

The nearby X65HE ID **2268** does return official metadata for **v309** and
`fw_upgrade_file/2268_v309`. That reference was downloaded and raw-inflated:
117,852 bytes, SHA256
`503940D85D865339BF6250A3C1AD464303A760F3B6DA6A6F43DD02FAB833A2FA`.
It identifies `AT32F405 8KMKB` and loads at 08000000. This is proof of the
reference MCU, not identification of the owner's X68HE chip.

## Candidate normal-mode analog read

Transport from vendor client: usage page FFFF, usage 2 (generic driver also
accepts usage 1), 64-byte Feature payload, report ID 0. Windows HID API buffers
add the report-ID byte. Bit7 checksum: byte 7 = FF - (sum(bytes 0..6) & FF).

The shared X68HE client exposes `_getMulitMagnetismCMD(254,4,1,1)`:

```
E5 FE 01 00 00 00 00 1B ... zero padding to 64 bytes
E5 FE 01 01 00 00 00 1A ...
E5 FE 01 02 00 00 00 19 ...
E5 FE 01 03 00 00 00 18 ...
```

E5 = read magnetic column, FE = live travel, 01 = page read, byte 3 = page.
Each response is **64 raw bytes, no echoed header**, 32 little-endian u16
values. Four pages cover 128 positions; WASD fits in page zero. The vendor UI
uses this helper in its calibration display, but the read operation itself
does not establish that calibration must be enabled. That distinction was
checked against the actual reference firmware below.

Do not hardcode 350 as full-scale based on a different model. The client uses
10 units/mm before firmware 0300, 100 from 0300 through 04FF, and 200 from 0500;
actual firmware/version, switch travel and physical range still need validation.

## Reference firmware proof (X65HE v309 ONLY)

- Dispatcher 08010AB2 calls handler **08006414** for E5.
- Handler's FE branch at **08006472** selects **08006754**.
- That branch copies 64 bytes from **2000222C + page*64** into reply
  **2000725E**, via real memcpy **08005578**. It does not set stream/calibration
  flags, write flash, or gate the read on a special mode.
- Scanner stores live depth at **0800CFCA/0800CFEC** before inspecting the 1B
  stream flag at **0800CFCE/0800CFF0**. The table is therefore maintained with
  streaming off. This image applies a shallow-travel threshold of 15 units in
  this publication path; do not promise full sensor precision near rest.
- 1B writes flag **20002A69** at **080109EC**. 1C/1E are distinct calibration
  controls, handled at **0801100C/08011034**. They must not be sent merely to
  obtain live travel; calibration can change retained sensor parameters.

`py tools/attackshark_x68he_recon.py` executes the **real Thumb E5 handler and
memcpy from this hash-pinned image** in Unicorn 2.1.4 (local `pydeps`, not a
replacement parser). Eight cases: four pages with stream disabled/enabled.
All return exact seeded 16-bit samples; whole-RAM comparison permits mutations
only in the reply and saved-register stack. All PASS. The script also checks
X68HE class inheritance, identities, matrices and request checksums.
Evidence: `recon-result.json`, `x65-disasm.txt`, `acquire.log` under artifacts.
This does not emulate a USB controller, scanner timing, or a physical X68HE.

## Stream versus calibration: caution about outside claims

The [X68 Pro third-party implementation](https://github.com/adapt-to-it/he-analog-gamepad)
uses 1B 01 / 1B 00 and report 05 carrying `1B depthLo depthHi slot`. Its
TECHNICAL.md explicitly reports loss of ordinary keyboard reports while active.
It also calls its PID 5030 device X68PRO and its controller Sonix; these do not
establish the identity or behavior of the owner's 502D keyboard. Do not promote
those claims to universal X68-family facts. In particular, the reference's 1B
flag accesses examined here control extra reporting; this is not a completed
proof of keyboard suppression across firmware revisions. Prefer the no-mode-
change E5/FE candidate until exact firmware/hardware evidence resolves it.

## Comparison with HallJoy/UAP

No existing route implements the Rongyuan E5/FE Feature-page protocol.
The pinned UAP/Soup recognizes Wooting, Razer, DrunkDeer, Keychron/Lemokey,
NuPhy and Madlions; its protocol families are not wire-compatible with this
one. HallJoy native addressed analog uses FF60:0061 addressed requests; AULA
MAX uses 5C/12/23/2B; W669/M484 uses 0D/18/21; MAD68 Pro R uses A8/A9/A0;
SparkLink/XD, Sayo, HEX80, frozen HERO84 and ROG have other framing/identities.
An incidental byte E5 in a keymap is Right Shift's HID usage, not this opcode.
Scheduling/ownership architecture can be reused, but changing a VID/PID list
is not support. No existing working route has been modified.

Performance candidate: poll only required 32-slot pages, one serialized Feature
request/read at a time. Do not inherit the desktop UI's 10 ms sleeps as a
hardware requirement, or promise 8 kHz telemetry from the keyboard's advertised
USB rate. Measure USB round trips, page skew, stale data and disconnects on the
actual revision. Raw responses lack page identity: concurrent native driver
access and delayed replies need explicit ownership/failure handling.

## Remaining gate / next input

The requested **exact X68HE firmware download and reverse are not complete**.
Safe available acquisition paths above are exhausted for the known IDs, but
the owner's actual revision is not yet known. Asked owner via async question
for HallJoy.log and the native driver's model/firmware-version screenshot.
These can reveal another revision or an available download path. If the vendor
has never published its image, obtain it from the vendor/owner; do not substitute
X65HE, Pro or Max firmware. There is no test EXE to send yet. Production HallJoy,
its settings, the connected keyboards and frozen-support gates are untouched.
