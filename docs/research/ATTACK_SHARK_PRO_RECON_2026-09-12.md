# Attack Shark X68 Pro HE / X82 Pro HE: feasibility investigation

> Superseded in part on 2026-09-19: [X82 dev2935 v503 firmware acquired and component-tested](ATTACK_SHARK_PRO_REVIEW_2026-09-19.md). Five other Pro revision images remain unavailable. All three requested families are Pro; external testers now exist.

## Result and limits

Whether X82 Pro HE can supply usable analog data while preserving normal keyboard
input is UNKNOWN. The earlier "strong candidate" wording overstated the evidence.
The exact official driver supplies model identities, matrix maps and a shared
E5/FE live-travel read implementation. This is NOT shipped support or a hardware
test. Neither exact keyboard firmware image was acquired: the official firmware
API returns `Record not found` for all four identified device IDs. Do not confuse
downloaded desktop installers with keyboard firmware, or the earlier X65 reference
firmware with either requested model. No device was opened or flashed, no vendor
executable was run, and no production source, release or public README was changed.

## Sources and downloads

- Repository: https://github.com/adapt-to-it/he-analog-gamepad
  commit `b462e2f45d03ed7065e308fa5453489efe608371`, MIT, copyright Nerjak.
  Local checkout `.local/research/he-analog-gamepad`.
- Official X68 Pro page:
  https://attackshark.com/products/attack-shark-x68pro-he-wireless-rapid-trigger-keyboard-magnetic-switch
- Official X82 Pro page:
  https://attackshark.com/products/attack-shark-x82pro-he-wireless-rapid-trigger-gaming-keyboard-magnetic-switch
- Both link qmk.top. Current web index remains `index.Bs-RnLKs.js`.
- Official downloads, saved in `.local/research/attackshark-pro/`:
  - https://download.attackshark.pro/ATTACKSHARK/X68PROHE/ATTACKSHARKX68PROHE.zip
    `X68PROHE-software.zip`, 101813510 bytes,
    SHA256 `e25635b05e6b2817461a5293eb6f2debdab88fce9e23321447967e3daf981b84`.
    Installer hash `3ee70880c8b5adcd469a0e3ebefa7e280a22fa23ff74e369955511a2c58e04a2`
    equals the already statically extracted X68 package in the September 9 research.
  - https://download.attackshark.pro/ATTACKSHARK/X82PROHE/ATTACKSHARKX82PROHE.zip
    `X82PROHE-software.zip`, 107412003 bytes,
    SHA256 `abe45ad1b24455aa327488b2ff4da98f5314f80de1ed898ac155268a05f0b6b2`.
    Contains `ATTACK_SHARK_V4_Setup_20260429.exe`; static 7-Zip extraction in `desktop/`.
    AppVersion 3.2.17, IOTVersion 222. Firmware images not found inside.

Firmware lookup exactly follows official `index.2e5bd916.js`:
`POST https://api2.rongyuan.tech:3816/api/v2/get_fw_version`, JSON `{"dev_id":N}`.
IDs 2356, 2370, 2901, 2935 return HTTP 500 / `Record not found`; responses saved.
Control ID 2268 succeeds with X65 v309, so this is not a blanket server outage.
Alternative qmk/api3 hosts returned no useful metadata. Bounded public filename
probes for these four IDs (v100–110, v200–210, v300–310, v500–505) found no image.
This does not prove no firmware exists anywhere; an official service image or an
owner's captured update URL could resolve the gap. Browser HID emulation cannot
by itself supply a missing server-side firmware record.

## Exact driver identities

From official X82 desktop `dist/js/index.2e5bd916.js`, loader `0e38223b.js`:

| Model | dev_id | VID:PID | Internal name | Model chunk |
| --- | --- | --- | --- | --- |
| X68PRO HE | 2370 | 3151:502F | ry5088_x68_8k_002 | c3329646.js |
| X68PRO HE | 2901 | 3151:502F | ry5088_x68v2_8k | c99d34c9.js |
| X82PRO HE | 2356 | 3151:502F | ry5088_sg_sg9015_3m_8k | 359ae4d7.js |
| X82PRO HE | 2935 | 3151:5030 | ry5088_sg_sg9015_8k_1k | fb3f6dd5.js |

All four inherit `C` from `f9b6af43.js`. Identity command 0x8F must distinguish
models sharing USB IDs. The linked community repo hardcodes 3151:5030 for its
X68PRO HE, conflicting with this catalog's X68 entries. Do NOT silently relabel
that owner's board as X82, nor assume catalog covers all firmware revisions.
Collect the actual 0x8F identity and product string before routing unknown boards.

All four matrices have 128 four-byte slots. W/A/S/D slots are 14/9/15/21.
The revisions differ: left Win / left Alt are 17/23 for 2356+2370 but 11/17 for
2935+2901. Copying a single full key map between models/revisions is incorrect.
Raw matrices and source hashes are preserved in `driver-evidence.json`.

## Analog path and repository assessment

Community `virtual_gamepad.py` enables `1B 01 ... E3`, disables `1B 00 ... E4`,
and parses report 05 payload `1B depthLo depthHi slot`. It normalizes against
350 by default. `TECHNICAL.md` section 9 warns standard keyboard reports stop
in analog streaming mode. This is the author's report, not our hardware finding.
The code also enables streaming before creating the virtual gamepad; the gamepad
creation exception returns without sending disable. Avoid copying this lifecycle.
Enumeration hardcodes an interface-path fragment and one PID, and swallows open
errors. It is useful protocol evidence, not a production-ready backend to transplant.

Official shared class instead has `_getJiaoZhunXinXi` calling
`_getMulitMagnetismCMD(254,4,1,1)` and decoding little-endian 16-bit samples.
Requests are 64-byte Feature payloads (Windows adds report ID 0):
`E5 FE 01 page 00 00 00 checksum`, four pages of 32 samples.
Checksum = FF minus sum of first seven bytes, modulo 256.
The direct read path is separate from 1B streaming and 1C/1E calibration writes.
Never invoke `setJiaoZhunKaiGuan` merely to obtain travel: it changes calibration.
The vendor polling loop explicitly excludes wireless mode. Initially scope a
HallJoy implementation to wired USB; neither Bluetooth nor 2.4 GHz analog is proven.

Previous X65 v309 firmware emulation proved its E5/FE live table is populated
without streaming. That establishes a useful family reference, NOT proof that the
unavailable X68/X82 images have identical handlers, latency or side effects.
Full-scale and units must be verified per version/switch configuration, not fixed
at 350 because the community example uses it.

No existing HallJoy/UAP route implements this exact RongYuan E5/FE exchange (see
September 9 protocol comparison). A dedicated backend is needed, not just a VID
addition. It should serialize set/get, reject unknown identities, clear stale
values, recover after unplug, and avoid concurrent vendor-configurator access.

## Reproducible checks / next step

Owner clarification (2026-09-12): no physical X82 is available. Continue seeking
the exact firmware, not proposing an owner hardware test as the available next step.
The four-page driver decoder is not proof that firmware independently updates all
128 samples outside streaming/calibration mode; this requires firmware analysis.

`py -X utf8 .local/research/attackshark-pro/evidence.py` passed offline assertions:
four model registry entries, shared read path, 128-slot maps and WASD/modifier
positions. It writes a new manifest and intentionally refuses to overwrite one.
`inspect_driver.py` prints bounded static excerpts; `acquire.py` records metadata;
`probe.py` performs only the bounded filename requests described above.

Next decisive test: a model-scoped diagnostic on an owner's X82 Pro via USB,
read identity and firmware version, read E5/FE travel with 1B left disabled,
verify ordinary typing remains intact, zero/full travel, multiple keys and unplug.
This hardware check is a future option, not available to the owner now. Stock
behavior remains unverified. Do not advertise support before implementation/testing.

## Additional firmware acquisition routes (same day)

- Chinese/English firmware pages `https://cn.attackshark.cn/downs.aspx?ClassID=22`
  and `https://attackshark.cn/downs.aspx?ClassID=22` contain only `updating...` placeholders.
- Chinese driver page links X82 to an older package:
  `https://attackshark.cn/upload/202511031709021.zip`, Content-Length 72354744.
  Fully acquired; both sequential and four-range downloads independently match
  SHA256 `e3e42b0a9d7e24178c17f51aeb83d1f6b94537c09d9914721ca897a6b438e042`.
  It is RAR5 despite the .zip URL, containing a March 2025 NSIS installer.
  Static extraction with full 7-Zip (7za alone does not handle RAR5) and nested
  `app-32.7z` yields an Electron app, not a standalone firmware updater.
  `cn-app/resources/app/dist/static/js/main_c366b7f7.js` again uses the same
  `/get_fw_version` API and download host. No exact keyboard image found in the
  extracted package; the two .bin files are Electron/V8 snapshots, not firmware.
- Russian distributor `https://ggwp.com.ru/attackshark/draivera` links X82 software
  to Google Drive file `17iI5TKFN5XCi0AytzQaW0nKfwk44R1iY`. Download saved as
  `X82PROHE-ggwp.download`, SHA256
  `23c1df1b786cec6fdf83c61f40b2620628b9e194fb63d073aa1fdbc483e642b4`.
  Its `ATTACK SHARK X82PROHE SOFT.exe` is byte-identical to the previously
  inspected installer (SHA256 starts `3ee70880`), so not a new firmware source.
- Legacy `https://iotdriver.qmk.top/static/js/main_68eaf5ce.js` acquired. Its
  `JAn` / `getFwVersion` uses the same api2.rongyuan.tech:3816 endpoint, not an
  alternative archive. Catalog identifies X82 with dev_id 2356; the negative
  bootloader entry is not evidence for another downloadable firmware revision.
- GitHub code searches for X82PRO and SG9015 found no exact firmware image.
  `adapt-to-it/he-analog-gamepad/issues/4` asks about X82 support, with no comments
  at inspection time. No issue/comment/message was posted by this investigation.

Current acquisition blocker: the checked public channels do not supply a verified
X82 image. This is not a claim that no such image exists. Next external route is
requesting an official offline firmware/recovery package for X82 PRO HE (including
revision mapping for dev_id 2356 / 2935), not another configuration-app installer.
Obtaining it requires vendor cooperation or an existing owner/service dump;
neither can be fabricated by HID emulation. Owner has no keyboard to dump.
