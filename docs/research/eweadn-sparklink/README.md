# EWEADN E HUB / SparkLink source review — 2026-09-24

Official entry points: https://www.eweadn.com/pages/eweadn-drivers and
https://www.eweadn.cn/drive/index.html link https://new-hub.eweadn.cn/.
Reviewed HUB build: 3.3.2-20260924093658738. `catalog.json` pins exact official
asset URLs/SHA256 and preserves source catalog records, including held records.
The three JavaScript files are evidence, not code executed by HallJoy.

17 model/variant labels, 21 distinct wired VID/PID pairs (VID1CA6). Same PID is
not counted twice for DK63 Star / DK68 Star. ES68 EVO Full is one revision of
ES68 EVO. Gamma75 aliases EXXGamma75 / METAWAY Y75 are not separate models.

## Wire review

- SDK `$n.Keyboard=1`; `getDeviceInfoData` returns type at byte2 and subtype at
  byte3. HallJoy's `01 02` probe and type1 check are correct. Magnetic subtype16
  is not device type16 and does not require weakening the probe.
- Official xingshan adapter selects HID usage1/page65456 (FFB0).
- `Nr` constructs zero-padded64-byte packets. HID transport sends reportID0.
- `getKeyLayoutPack`: `03 01 layer row`; layer0 is current base assignments.
  `keyLayoutResult` decodes21 little-endian16-bit keycodes starting at offset4.
- `getRoute`: `04 03 01 row`; `getRouteData` returns little-endian16-bit travel
  values at offset4, with row at byte3. No stream-enable/calibration write is
  in this call path: public API -> performanceController -> sendProtocol.
- UI divides route values by1000 to display millimetres, matching micrometres.
  Switch `axisRangeMax` can vary. HallJoy keeps its existing provisional3.5mm
  initial ceiling, increasing with observed per-key travel up to5mm. Shorter
  travel switch normalization is a specific remaining experimental uncertainty.

Native backend already enumerates this HID interface without an IROK name filter,
proves device-info replies, discovers layout rows and polls analog rows. It
publishes through ordinary bindings/gamepad output, including row freshness
release. No new calibration, settings, firmware or SDK dependency is introduced.
This batch supplies the exact experimental identity tokens/notices and updates
the shared notice title so EWEADN is not presented as IROK.

This is source review, not firmware emulation or physical-device testing.
Manual visual layout selection can be needed; live key assignments are obtained
from the device. Shared-PID Star variants therefore do not use a guessed matrix.

## Deliberately held

Alpha87 has magnetic/mechanical SDK variants requiring clearer retail scope.
PID1C2D combines disabled X75 V3 and X75 Ultra / Gamma75 entries; it is not part
of this declaration. Shared VID1A86/PIDFE81 ES68/EVO is not admitted. Other HUB
SDK families, mechanical namesakes and wireless transport are not covered.
Gamma75 declaration covers PID1C37/1C45 only. No bootloader commands were sent.

Validation: `python tools/check_eweadn_sparklink_sources.py` checks source hashes,
reviewed identity-to-model coverage, wire anchors and runtime notice coverage.
Run the existing native backend regression suite and ordinary Release gates too.
