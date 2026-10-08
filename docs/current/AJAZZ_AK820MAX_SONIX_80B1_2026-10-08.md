# AJAZZ AK820 MAX HE, Sonix revision 0C45:80B1 — 2026-10-08

Owner request: add full support for the keyboard in the user log
"message (15).txt" (user says AK820 MAX HE). The log shows `0C45:80B1`
"AK820MAX" (interfaces 00..03). This is not the supported Witmod unit
(`0416:7372`, SG8994HERGB) and not the RongYuan tri-mode screen model
(`3151:4015`). Status: **Not supported (red)**, owner decision 2026-10-08
(see the final section "Decision: red").

## Sources

All local evidence is in `.local/research/ajazz-ak820max-80b1-20261008/` and
`.local/research/ajazz-ak820max/`.

- Official web configurator `https://ajazz.driveall.cn/` (linked from
  ajazz.net "AJAZZ AK680 Max Online Driver"). Bundle
  `assets/layout-classic-CcyMQaqB.js` is unchanged since the 2026-09-20 copy
  (same hashed file names today).
  - Device config `vendorId:3141, productId:32945, name:"AK820MAX"`, USB only,
    routes customKeys/lighting/macro/performance/advancedKeys/settings,
    report rates 1000/4000/8000, max trigger travel 3.3 mm. Extract:
    `config-32945-AK820MAX.js`.
  - A second 32945 entry, "AJAZZ KEYBOARD" (TFT), uses another product name;
    it is not this keyboard.
- Firmware: none published.
  - Driveall `cp.driveall.cn/api/device/firmware/index` with the bundle's own
    parameters (`supplier_name=ajazz`, `connect_type=USB`, vid/pid in decimal
    and hex, product names "AK820MAX" and "AJAZZ KEYBOARD") returns
    "数据为空" (no data). The configurator has no other firmware source
    (`it` is always true, so `update.json` is never used).
  - ajazz.net drivers page: "AK820 Max Driver" `AJAZZ_AK820_MAX_driver_V1.0.zip`
    (SHA256 a264ecb2...d0b6cf) is the 2024 BYCOMBO4 driver for the mechanical
    AK820 MAX (BY platform), not this keyboard. "AK820 Max HE" links point to
    the already reviewed Witmod installer and the RongYuan tri-mode driver.

## Protocol: identical to the AULA MINI60 stream backend

The Driveall code uses the same Sonix AA/55 command set that HallJoy already
uses for AULA MINI60 HE/Pro/MAX (`aula_mini60_diagnostic.cpp`):

- `GET_DEVICE_INFO` 0x10: VID at payload+4, PID +6, version +8/+9,
  manufacturer +12, product +14 — the same offsets HallJoy reads
  (`reply+12/14/16/20/22`). Driveall requests 48 bytes; the AULA SDK 56.
- `GET_KEY` 0x12: 512 bytes, 128 records of 4 bytes, read in
  `reportCount-8` chunks (56 for 64-byte reports). Page type 0 = factory,
  2 = keyboard (`param1` modifiers, `param2` HID usage). Fn is keyCode 175
  (0xAF), as in the AULA HFD driver.
- `SET_SIMULATION_TEST_ON/OFF` 0x66/0x67 and the `55 FB` stream. Driveall's
  decoder `Sd` reads key, calibration status, max, min & 0x7FFF, current ADC,
  keyStroke and maxStroke at the same offsets as `mini60diag::Decode`, and
  shows keyStroke/100 mm and maxStroke/10 mm (no rtPrecision dependency) —
  the same normalization as `mini60::Milli`.
- The newer addressed read `GET_MAGNETIC_AXIS_STATUS` 0x68 (frameVersion 1,
  used only inside calibration V2) is not used.

Key positions use `row*16+col`, like MINI60 (W 34, A 49, S 50, D 51).

## Implementation

`aula_mini60_native_model.h` / `aula_mini60_diagnostic.cpp`:

- PID `0x80B1` added to the MINI60 backend (protocol 16) with its own
  82-key factory map (`Ak820Factory`, from the Driveall keyList; Fn -> 0x409)
  and per-product key count. AULA maps and behavior are unchanged.
- Admission: `0C45:80B1`, collection `FF68:0061`, 65-byte input/output, then
  a device-info reply whose VID/PID echo is `0C45:80B1`. Manufacturer/product
  values are unknown for this firmware, so they are logged, not required
  (AULA keeps its `0x0166` restriction). Device-info length 48, as Driveall.
- Same lifecycle as MINI60: key assignments read once (0x12), 0x66 on start,
  0x67 at pause/exit, per-key 50 ms freshness, live remaps applied.
- Layout: verified token = the existing "AJAZZ AK820 MAX HE ANSI" geometry
  token (`layout_identity::Token("ajazz-m484","SG8994HERGB")`). The preset's
  82 keys equal the Driveall key list exactly (set comparison, 82/82).
- Telemetry device name "AJAZZ AK820 MAX HE (0C45:80B1)".
- (Superseded by the red decision: the yellow notice group was removed.) Yellow notice group `AjazzAk820Sonix` (protocol 16, AK820
  token, shared flag `Mg75Pro` = "Keyboard: hardware testing incomplete").
  MINI60 tokens on protocol 16 get no notice.
- Self-test `--halljoy-mini60-self-test` now also runs the 80B1 model:
  identity rules, 48-byte info request, 82-key map, Fn/F-row/arrows/Delete,
  publication and expiry. `keyboard_support_status_test.cpp` checks the notice.

## Unknowns (the first tester log answers them)

- The HID collection shape: if the 80B1 vendor collection is not
  `FF68:0061` with 65-byte reports, the worker logs a
  `descriptor vid=0c45 pid=80b1 ...` line and does not connect.
- Device-info manufacturer/product and firmware version (logged).
- Whether the stream keeps sending held keys (needed for the 50 ms freshness)
  and whether typing continues while the stream is on. MINI60 logs showed
  both; Alumix104 (another 0C45 board) showed lingering releases.

## Sheet

Planned new row: AJAZZ "AK820 MAX HE (wired, RGB, 0C45:80B1)", status
"Not supported" (red), next to row 17
"AK820 MAX HE (wired, RGB)" (Supported, 0416:7372). **PENDING:** no Google
Sheets connector in this session; `support_notice_catalog.py --sheet` will
fail on this row until the Sheet is updated.

## Validation (agent, no hardware)

- `tools/build_release.ps1` EXIT=0 from PowerShell: identity audit, notice
  catalog (288 yellow models), support diagnostics gate, MSVC Release build,
  all candidate checks including `--halljoy-mini60-self-test` (now with the
  80B1 model) and `--halljoy-require-full-catalog`, embedded licenses,
  profile/layout tests (`SUPPORTED_LAYOUTS=PASS models=72`), installed.
  EXE SHA256 `500c9eea3e644792a249b71118acb7fd149d47315e88f34b9d9c0dc5758e794d`.
- `research_reference_checks.py --record` after the notice catalog change: all
  original private audits passed.
- `run_native_backend_checks.py --require-compiler` EXIT=0, including the
  extended `aula_mini60_native_model_test.cpp` (82 unique keys, identity rules,
  info length, layout token) and `keyboard_support_status_test.cpp` (notice).
- Note: MinGW tests started from Git Bash crash (known, FAST_CHECKS doc); run
  gates from PowerShell.
- No device test and no visual check by the agent.

Backups: `.local/backups/ak820-80b1-20261008/`.

## Tester log "message (16).txt" and screenshot, 2026-10-08

- Build of this document (report still says 1.6.7.0). `aula-mini60-he-pro`
  connected to `0C45:80B1`, `FF68:0061`, 65/65 bytes, `mapped_keys=82`,
  7061 updates in ~75 s, 0 failures. Identity, collection and stream work.
- Tester: "only one key at a time works". The report could not show why: no
  keys were bound (`unique_keys=0`), and the worker's per-key/chord lines went
  only to the stability trace, not to the report. **This build's log was
  insufficient — an agent error.**
- Screenshot: W, A, S, D show "Unassign", Caps shows "-". The live key records
  (0x12) of those keys are not page type 2. The Driveall configurator offers
  advanced keys on this model (RS, SOCD, DKS, MT, TGL = page types 12, 11, 8,
  9, 10); `Assigned()` turned every non-type-2 record into 0, so remapped
  W/A/S/D owned no analog.
- `analog_error=-2000` is `WootingAnalogResult_UnInitialized` from the UAP
  fallback read of keys the native backend does not own; not this keyboard.

### Fixes (next tester build)

- AK820 only: Driveall advanced pages 8..12 keep the physical key's factory
  HID (the key still reports its own depth). AULA behavior unchanged.
- The ordinary HallJoy.log capture store (first 8000 records) now carries:
  - `capture.ak820w ...`: worker descriptor lines, device info
    (manufacturer/product/version), every changed key record with raw bytes
    (`type p1 p2 p3` and resolved `hid`), mapping, start/stop, read errors,
    and stream checkpoints when `keys`/`recent50_max` change;
  - `capture.ak820 press|release|expire|sample`: every press and release,
    every release forced by the 50 ms freshness limit, and every new sample
    while 2+ keys are active, with travel/stroke, gap since that key's
    previous sample and the active positions;
  - `capture.key`: first Windows key events per key (typing during stream).

Validation: build_release.ps1 EXIT=0 (all candidate checks), native checks
EXIT=0 incl. the model test cases for pages 8..12 (AK820 keeps factory key,
AULA unchanged, macro/FUNC stay unassigned). Tester EXE SHA256 `7d0a35581181c7febef86599bcab4ace6549e48b5aef600c47742597741127ae`.
No device or visual test by the agent.

## Multi-key: what the vendor tool actually guarantees (2026-10-08)

- Driveall uses the 0x66 stream only for the single "test trip" readout:
  `startSimulationTest` -> callback throttled to 10 ms that shows the
  keyStroke of the LAST event, whatever key it came from. The vendor tool
  never consumes several held keys from this stream, so the firmware has no
  demonstrated multi-key contract on it.
- HallJoy's 50 ms per-key expiry (inherited from MINI60) and a
  "hold until 0" rule are both guesses about that stream. An unbuilt
  hold-until-0 change was reverted on owner direction (no workaround fixes).
- Full-state read: `GET_MAGNETIC_AXIS_STATUS` 0x68 returns, per requested key
  (up to 7 per report, 8-byte slots, address `index*(reportCount-8)`),
  calibration status, ADC and current stroke. Driveall calls it only on
  frameVersion 1 firmware and only between `SET_CALIBRATION_ON_V2` 0x69 and
  `OFF_V2` 0x6A. Whether 80B1 is frameVersion 1 and whether 0x68 answers
  outside calibration V2 are unknown. Calibration mode is not acceptable
  during play.
- Digital (Windows) key events are diagnostics only and never enter analog
  values (owner, 2026-10-08).
- No firmware is available to settle this offline (see Sources).

## Probe build (owner: "do everything we can, no time or log limits"), 2026-10-08

Only official configurator commands; no memory reads outside documented data.

- Device info: `probe device_info rom_size rt_precision frame_version
  lighting_version firmware_status` (Driveall offsets) and the raw 56-byte
  payload hex.
- `GET_MAGNETIC_AXIS_STATUS` 0x68 without calibration mode (read-only): once
  at rest before the stream (W A S D Space LShift LCtrl), once after the
  stream starts, and once for every new set of 1..7 keys that the stream
  shows as held. Each reply is logged raw (hex), marked echo or data, and data
  slots are decoded (status, ADC, stroke) next to the stream travel of the
  same keys at that moment. No reply is logged as `reply=none`.
- Stream 0x66 unchanged (same as the AULA/Alumix neighbours).
- New support-log evidence store (`SupportLog_Evidence`): probe results,
  identity, key records and chord summaries are kept whole; stream records
  keep the first 2000 and the newest 8000 with an explicit
  `evidence.omitted` count, so neither the session start nor what the tester
  did right before Open log can be pushed out. Written in every full report.
  `support_log_windows_test.cpp` covers it (`evidence_store`).
- Chord summary per period with 2+ active keys:
  `evidence.ak820 chord ms max_active expired keys=pos:samples:max_gap,...`.
- Digital key events stay diagnostics only.

Validation: build_release.ps1 EXIT=0 (all candidate checks), native checks
EXIT=0, support diagnostics with HALLJOY_NO_TEST_CACHE=1 PASS (a cached
support_log_windows binary was stale; separate task). Tester EXE SHA256
`20b60e5ed10207bd31a13f33182b753e7bef944a02f1c54d6687027997d9529d`. Not tested on hardware by the agent.

## Tester log "W:\Downloads\message.txt" (probe build), 2026-10-08

- Collections: FF68:0061 65/65 (used), FF67:0061 feature 65 / out 4097, plus
  keyboard/consumer/system/mouse collections.
- Device info: manufacturer 0x0166, product 0x110C (same values as AULA
  MINI60 Pro), version bytes 11:01, work_mode 10. Driveall fields: rom_size 0,
  rt_precision 0, **frame_version 0** (older protocol generation).
- **0x68 is not implemented**: every query (rest, stream on, 1..4 held keys)
  returns an exact echo of the request. Same as Alumix104.
- Key records: W/S and A/D are SOCD pairs (type 11, mode 3, partner keys);
  Caps is remapped to keypad minus (type 2, usage 86, shown "-"). With the
  advanced-key fix all 82 positions map (W/A/S/D keep 26/4/22/7).
- Stream: up to 3 keys within 50 ms (`recent50_max=3`), so several keys are
  reported. But it is **change-only**: in every chord the still key got no
  samples (e.g. `keys=49:19:953,51:0:0` — D held at 217/340 without a sample
  for the whole chord) and HallJoy's MINI60 50 ms expiry released it
  (`expire p=51 t=217 v=638`). This is the "only one key works" cause.
- No reported travel between 1 and 15 (minimum non-zero 16). Releases end
  either with t=0 or with a last value of 16..28 and silence (D last 16,
  Ctrl last 21 per the worker's own last stream value).
- `malformed` grew to 441 of 21469 packets: the MINI60 decoder rejects
  status > 1 / index >= 126, which the Driveall decoder accepts.

### Conclusion and change

The firmware provides per-key depth for several keys; the defect was the
MINI60 timing rule. For 0C45:80B1 only:

- Driveall packet rule (any status, index < 128); every packet the old rule
  rejected is logged raw (`stream_vendor_only` / `stream_unparsed`).
- `GET_GAME_MODE` 0x11 read at connect (official configurator read): top and
  bottom dead zone (0.01 mm), logged raw.
- No time expiry: a key keeps its last reported depth and is released by a
  reported 0 or a value inside the keyboard's top dead zone. Worker loss
  clears all keys as before. Digital key events are not used.

Validation: build_release.ps1 EXIT=0 and native checks EXIT=0 with HALLJOY_NO_TEST_CACHE=1 (self-test covers hold, top-dead-zone release and MINI60 expiry unchanged; model test covers Held). Tester EXE SHA256 `3a4e06e21f969e030dc905bdd2eea354b14f20a457fc03f5b1a159bd08cd83f7`. Not tested on hardware by the agent.

## Decision: red, not supported (owner, 2026-10-08)

Owner decision: switch support off for `0C45:80B1` and set red. The
implementation stays in the tree behind `Ak820Admitted=false`
(`aula_mini60_native_model.h`); the device is not detected by any build. The
yellow notice group was removed from `keyboard_support_notices.json`.

Reasons on the evidence collected so far: `0x68` is not implemented (echo only,
frame_version 0), the stream is change-only, and the release-by-timeout
behaviour could only be replaced by a hold rule that is not verified on
hardware. Re-enabling needs a new owner decision with evidence.

Sheet: the red status and the new row are a pending sync item (no Google
Sheets connector in this session).
