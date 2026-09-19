# IROK NA87 magnetic: firmware acquisition and driver reconnaissance

> Исправление 2026-09-13: 0x29 — opcode входа Witmod SDK; фактический HID
> opcode — 0x21. Полный scanner и все serializer entry points теперь проверены
> offline. См. [IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md](IROK_WITMOD_OFFLINE_CLOSURE_2026-09-13.md).


Status: exact firmware NOT obtained; no production support added.
Scope is NA87 Mag, not NA87 mechanical and not NA87 Pro Mag. No installer,
vendor executable or firmware was executed. No physical HID device was opened.

## Official acquisition evidence

- https://driver.irok.cc/ assets/index-5bc197c9.js links NA87 Mag to
  https://s.joyway.net/packages/irok/software/install/IROK_Setup_v2.5.6.1_Amazon.exe.
  Downloaded 131118208 bytes, SHA256
  5F7E723718D1EBD3522AD476F2E00AD51A0E91AEBFB42DAEDF669B1A03D86120.
- Current https://www.irok.cn/software uses
  https://irok.ast.joyway.net/deviceDrivers. Entry 26 is magnetic NA87,
  is_support_web_driver=false. Entry 25 is mechanical NA87; entry 32 is NA87 Pro.
  The current magnetic entry links the 2.6.0.1 desktop package:
  https://s.joyway.net/packages/irok/software/install/IROK_Setup_v2.6.0.1_online.exe.
  Downloaded 128526256 bytes, SHA256
  C3845B004E2ACCB7A5768118380F52BA0A3173031B72434496B43E7317720829.
- Neither download is being represented as a keyboard firmware image. NSIS
  listings identify desktop executables/Electron resources, not an identified
  exact NA87 flash image. No exhaustive encrypted-payload claim is made.
- The older package's actual driver-script entry is index-02-PGB8U.js, selected
  by its published index.html under driver_script_3.6.0. Other stale JS bundles
  coexist in the installer; do not assume every bundle is its active entry.
- askFirmwareData calls https://irok.ast.joyway.net/firmwareVersion/latest.
  It returned {"code":0,"data":[],"message":"ok"} twice. No device impersonation
  was needed for this public request. SupportFirmwareUpgrade feature 11 in this
  script includes NA87 Pro/MU68 Pro, not ordinary NA87.
- https://api.mall.irok.cn/v3/news?pageNum=1&pageSize=100 returned all 35 entries.
  NA87-related firmware articles are 46 (Pro 1.0.3) and 52 (Pro/ND63 1.0.8).
  No ordinary NA87 firmware article is listed. This is current catalog evidence,
  not proof that an unpublished/service firmware does not exist.

## Static host-code findings (NOT firmware findings)

The desktop JS identifies NA87 with internal device index 12953482098
(0x304167372), separately from NA87 Pro. Its magnetic driver executable has SHA256
97D53F6447C6F5203436E2306B9C1321D1B05B473A1CD069B7104542754F2004.
Go function metadata was parsed without running it; llvm-objdump disassembles
the exact resulting PE virtual-address ranges:

- driver.helper/support/magnet.FindDevices: 0x7a97d0..0x7a9aa0.
  At 0x7a97ee it supplies packed USB identity 0x73720416; subsequent code checks
  VID 0x0416. This is the magnetic family discovery path, not complete NA87 admission.
- (*Magnet).handleRecive: 0x7aa1c0..0x7aa500; filtered event path calls
  sendDynamicKey with a 64-byte report.
- (*Magnet).sendDynamicKey: 0x7abca0..0x7abd30. At 0x7abcc5..0x7abcd0,
  combines report[7]<<8 with report[8]. At 0x7abce2 it reads report[9], then
  invokes a callback with the identifier and one-byte value.

This host parser resembles the earlier ND75 magnetic event format. It does NOT
prove the NA87 firmware emits every sensor, that an arbitrary key can be polled,
that all held keys update independently, that releases emit zero, or that normal
keyboard reports continue while monitoring. Scale and exact model identity still
need tracing. Do not enable the ND75 backend for NA87 merely by matching VID/PID.

## Reproducible local evidence and next prerequisite

Files are in .local/research/irok-na87/: two vendor installers, captured public
catalogs, extracted scripts, go_*.asm, extract_asar.py and go_functions.py.
fetch.py uses normal TLS verification; it worked where Windows clients intermittently
failed connecting/checking revocation. Vendor binaries are not redistributed.

Next prerequisite for the requested firmware-level analysis is an official
NA87 Mag update/recovery package or flash dump with exact hardware revision.
Ask manufacturer/support for ordinary NA87 magnetic (explicitly not Pro and not
mechanical). The confirmed driver parser is a starting point, not a compatibility
verdict. HallJoy runtime, README and release assets were not changed.

## Continued search: actual web-driver family evidence

The owner suggested checking whether NA87 and NA87 Pro are related in the web
driver. Downloaded the live https://hid.irok.cn/ entry and keyboard SDK:

- assets/index-BQvZQSA6.js, 1487357 bytes, SHA256
  2B80DD9398565E13EBF315351AF96C3BFCF059FA917A774638B0BE42C69C44DC.
- assets/sdk-keyboard-naj1DnVU.js, 196233 bytes, SHA256
  5489D89F6A4C6E8968C059E367D422392F1AFFFAFB056338AD448EB4C589B554.

There IS UI model inheritance: ordinary NA87 maps to q5t, and NA87 Pro maps to
X7t, declared `class X7t extends q5t`. q5t stores geometry, lighting controls,
key identifiers and feature defaults; X7t replaces the key layout/map, lighting,
Fn controls and limits. This is shared UI code, not proof of shared MCU firmware.

The same script explicitly separates vendor families:

- Witmod: IYXMu68, IROKNd75, IROKNA87.
- JingTaiV1Base: IROKNa87Pro, IYXMu68Pro, IYXMu68Pro_cyan, IYXMu68_ULTRA,
  IROKNd63, IROKNd63_cyan, IYXPolar75.

DevicesOfWitmod (feature 17) is populated from the first group. Calibration and
firmware-version handling branch on this feature. NA87 Pro WebHID filters specify
VID/PID 1C4F:EE88, secondaryProductId=32, usage FFA0:0001, and replacement
VID/PID 1CA2:0401. This differs from the ordinary magnetic-family 0416:7372
discovery proven in the desktop service. Presence of ordinary NA87 UI definitions
in the shared web bundle does not establish native WebHID support for that device.

The live firmware-version UI compares exact DeviceIndex entries; no ordinary
NA87-to-Pro firmware alias was found. The update URL remains the same public
firmwareVersion/latest channel already checked. Chinese-only visibility of the
upgrade UI is a presentation rule, not evidence that the returned list is complete.

Conclusion: shared UI ancestry is confirmed, shared wire protocol/flash image is
not. For ordinary NA87, ND75/MU68 are the better evidenced family leads, without
assuming binary compatibility. A Wayback CDX query for s.joyway.net/*NA87* with
successful responses returned an empty list; exact NA87 firmware is still absent.

## Sibling firmware acquisition (2026-09-13)

All files are retained only under `.local/research/irok-na87/`. No vendor
executable was run, no keyboard was flashed, and HallJoy runtime was not changed.
Authenticode verification reports Valid for all five downloaded updaters below.

### Ordinary Witmod family

- ND75: downloaded `https://s.joyway.net/packages/irok/software/install/ND75_Firmware_Upgrade_V12.exe`
  (5,344,536 bytes, SHA256
  `5DB98375D084A1F6EC4FA22CFA02123DBB81B701673521AA0772FBE981A4F16A`).
  Extracted 524,288 bytes at file offset `0x2ce208` using `extract_nd75.py`:
  `ND75_V12_flash_512K.bin`, SHA256
  `A5168399CACA4BBD2C04A1E988F478265364577A46994199A73848ABF0AB735F`.
  This exactly matches the previously documented ND75 image; it is a recovered
  copy, not a newly discovered firmware revision. Embedded identity:
  `M484,01,KB,ABT,X86HERGB,V1.00.09`. See
  `IROK_ND75_M484_STATIC_ANALYSIS_2026-08-17.md` for static protocol findings.
- NA87 Mag and ordinary IYX MU68: exact firmware still not obtained.
  Desktop installers are not counted as firmware.

### Separate JingTai Pro family

Official article APIs `https://api.mall.irok.cn/v3/news/<id>` and
`https://api.mall.iyx.fun/v3/news/<id>` expose download links in `data.content`.

| Local updater | Official article provenance | SHA256 |
| --- | --- | --- |
| pro-family-1.0.3-updater-0.0.2.exe | IROK 46 / IYX 45; firmware 1.0.3, updater v0.0.2 | F6B17484B9A7D17B1C623BBAA140A0B756608C5866A942B53DB1C7943AEE12B1 |
| pro-family-1.0.5a.exe | IYX 48, MU68 PRO 1.0.5 | B4EFAB94974BD2AC601EFA135A4606B8701787E4F3C3FA365509DCE88985E441 |
| pro-family-1.0.8.exe | IROK 52: NA87 PRO + ND63; IYX 61: MU68 PRO; identical download URL | 7B39BA7795B19EEE6A9083AE1B9B1FF8D66B0D64E1A914EE4E13E150EA62A958 |
| fw_cyan_1.1.4.exe | IYX 59, MU68 cyan; executable contains Pro-family model markers | 21DF0E79074F5CA95B432B7AD076BF4F13ADF23B90A6F9EBCEA0804DBFA78AEB |

The four packages are signed native updater executables, not yet separately
extracted and model-validated flash images. In particular the cyan title alone
does not establish applicability to ordinary MU68. A shared updater URL proves
shared distribution, not interchangeable firmware or one shared payload.
No separate firmware for MU68 Ultra or IYX Polar75 was obtained in this pass.
IROK/IYX public news catalogs and both firmwareVersion/latest APIs were checked;
the latter returned empty lists. This is not proof that no firmware exists.

Result: a genuine ND75 flash image is available for ordinary-family comparison;
NA87 analog support is still unproven. Do not transfer ND75 commands or flash
images to NA87 as an established compatibility claim.

### Later firmware analysis

The Pro payloads have now been extracted and hash-verified: ten unique RISC-V
images, with the same analog-array serializer in every image. This supersedes
the earlier "not yet extracted" acquisition status, not the ordinary-NA87
compatibility boundary. Commands, exact offsets, tests and remaining limits:
`IROK_PRO_FIRMWARE_PROTOCOL_2026-09-13.md`.

Further ND75 tracing found a single pending row/column event slot. Actual-code
component emulation reproduced overwriting one change (including a release)
with another before service. See the 2026-09-13 continuation in
`IROK_ND75_M484_STATIC_ANALYSIS_2026-08-17.md`; the older experimental-readiness
assessment must not be used as evidence of reliable multi-key delivery.
