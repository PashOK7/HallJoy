# MelGeek MADE68 Air: first analysis (2026-10-12)

> STATUS (2026-10-12, end of day): the analog path is IMPLEMENTED as a backend (see "Analog backend" at the end).
> The sections below are the research history in order; statements such as "not supported" or "probe only" in them are superseded.

Owner supplied a tester log `message (1).txt` (HallJoy 1.6.9.0, MelGeek MADE68 Air). Research only;
no code change, not supported (at that time). Local files (not published): `.local/research/melgeek-made68air-20261012/`
(HIVE web app chunks, WASM modules, WAT disassembly, device config JSON, API probes).

## Identification (from the log)

- `hid.candidate vid=0416 pid=0114 name="MADE68AIR ZG"`: interface `00` appears five times
  (several collections) and interface `01` once. No HallJoy backend recognized it
  (`present=0` for every native backend); the log has no other events about it.
- VID `0416` is shared (Nuvoton): HallJoy's Witmod/IROK ND75 `0416:7372` is a different device.
  PID `0114` is not used by any HallJoy backend for VID `0416`.
- The inventory rows carry only interface numbers, not usage pages or report sizes, so the vendor
  collection cannot be told from this log.

## Official software

- Configurator: MelGeek HIVE, `https://hive.melgeek.com/` (Svelte/Vite web app, WebHID). Device
  picker filters: vendor IDs `0x0416`, `0x3854`, `0x05AC`, `0x303A`. The device key is `made68air`
  (module `OneDevice`); the app has firmware-dependent UI branches for `made68air` at fw <= 1.3.6.
- The whole device protocol is implemented in a Rust WASM core (`mghive_core`, 2.8 MB) with a
  retained function-name section; the JS only passes high-level calls (`app_info`, `cfg_info`,
  `dev_info`). Device configuration JSON (`default/devices/kb_common/made68air.json`, layout of
  the standard keys) is public; it does not describe the protocol.
- Firmware: the app asks `POST /api/device/library/check_firmware_update` (JSON-RPC body) and
  downloads from `/official/Firmware/...`; direct paths return 403 and the check returns `error`
  without a real device identity. Not bypassed; no firmware image obtained.

## Protocol facts recovered from the WASM (static)

- Request frame: 8-byte header `[module, operation, data length, b3, b4, b5, crc_lo, crc_hi]` followed
  by the data. CRC16 is table-driven over the data bytes (`encode_crc16`, initial value 0xFFFF);
  incoming frames are verified the same way (`check_crc16`).
- Command headers (first two bytes) seen in the getters: `01 87` get version, `01 8B` hardware info,
  `01 8E` sleep time, `0A 81` get triggers, `0A 97` trigger accuracy, `0A 93`/`0A 94` set/get report
  (polling), `0A 99` dead zone and rapid-trigger data, `0A A3` switch info, `DE 86` ADC log lookup.
- Device-to-host data is parsed by per-`hid_ver` handlers (`handler_256`, `handler_257`). The event
  `up_trigger_adc` (an array of `{id, adc, trigger}` entries) is emitted only by the 257 handler;
  the UI uses it on the keyboard calibration screen (`key_adc_info` for up to 200 keys), and there is
  a separate `key_pressed` event.

## What is not established

- The `hid_ver` of the MADE68 Air (256 or 257), the HID collection and report ID used by HIVE, and
  the exact frame layout of the ADC notification.
- Whether ADC values are streamed outside calibration mode, for all keys at once, without disturbing
  normal typing. The only known consumer is the calibration UI, which suggests a calibration-time
  stream; the HallJoy game requirement (analog travel of all keys, no calibration mode, no
  single-key limit) is therefore not shown to be met.
- Firmware version of the tester's unit.

## Verdict and next steps

Not supported; Sheet `Main!C561` set from "Not investigated" to "Research incomplete" (value-only
write, validation and effective gray colour read back, neighbours unchanged). Needed to continue
(owner rules: one HallJoy.exe, results in HallJoy.log):
1. Inventory rows with usage page, usage and report lengths per collection (metadata only) to pick the
   vendor collection from the next tester log.
2. A read-only identity probe on the exact identity `0416:0114` (version `01 87`, hardware info `01 8B`)
   once the collection and report framing are confirmed, logging the replies and `hid_ver`.
3. A real firmware image or tester capture to see whether the ADC stream exists in normal mode.

## Official firmware source check (2026-10-12)

- Hive (`https://hive.melgeek.com/`) without a device shows only "Add Device", "Demo Mode" (the
  demo opened MADE68PRO, not MADE68 Air), "Tool" (Keyboard test) and "Settings". No firmware list.
- MADE68 Air product page (`melgeek.com/products/made68-air-low-profile-hall-effect-gaming-keyboard`):
  no firmware or driver file. Its "Software" link goes to `/pages/download`, which redirects to
  `/pages/hive` with no firmware files. The product is shown as sold out.
- No device with VID `0416`, `3854`, `05AC` or `303A` was present on the USB bus of the analysis PC.
- Result: no firmware image obtained, nothing downloaded. Hive's firmware check needs the connected
  device identity, so the image can only be reached with the physical keyboard attached.

## Firmware archive obtained (2026-10-12, owner-supplied official link)

The owner supplied a signed official download link, `made68air_v1.6.0.zip` from
`hive.melgeek.com/official/Firmware/MADE68AIR/` (98,557 bytes, SHA-256
`347d5eb9d703f6a6d56d75e6f89c5b742f34b2cd7c9eef3c899be9edcfd48307`). Local only:
`.local/research/melgeek-made68air-20261012/fw/` (archive, response headers, extracted copy; nothing executed).

- `DRIVER_MADE68AIR_FW_1.6.0_HW_1.0.1_20260128134637.mg`: 139,568 bytes, SHA-256
  `029ab79a4153b24a55c9f317e151bd5e20475950f7bb7c90866bdb73a5655458`. Firmware 1.6.0, hardware 1.0.1,
  build 2026-01-28. The container header is not plain (constant filler bytes), a few readable thread and
  profile names (`cali_thread`, `Default/Typing/Rapid Profile`) appear in the clear.
- A UTF-8 release-notes text file (MADE68AIR V1.6.0, 2026-01-28, with the earlier MADE68/MADE68PRO releases).
- Not done: decoding the container or analysing the image. An automated safety classifier interrupted
  the analysis step twice, so it was stopped; the analysis is pending an owner decision.

### Release notes in the archive (plain text, Chinese; MADE68AIR V1.1.7 .. V1.6.0, 2026-01-28)

- The Air firmware is code-synced with the MADE68 and MADE68 PRO ("sync made68pro" in 1.3.9 and 1.6.0,
  "sync latest made68 code, support adaptive RT" in 1.5.0): it belongs to the MADE68 family protocol, not a
  separate platform. The WASM alias table lists made68air with its own family key `made68air`.
- Calibration is a firmware feature with a live page in HIVE (MADE68 PRO 1.1.7: "fixed calibration screen
  refresh delay"; 1.1.3: "calibration value accuracy"; Air 1.3.2: recalibrate all keys for RT precision;
  1.3.9: automatic and manual calibration code, relaxed thresholds). This matches the `up_trigger_adc`
  calibration data seen in the web app.
- No release note mentions a continuous analog or travel report for use outside calibration.
- Other features: DKS, MT, TGL, macros, SOCD, single-key stop, FN layer, 8000 Hz class (not stated).
- Conclusion so far: analog values are visible to the host during calibration; availability in normal
  mode is still unestablished. Image analysis pending.

## Diagnostic probe build (2026-10-12, owner request)

Owner request: a HallJoy.exe whose log decides whether the MADE68 Air has an analog path. Implemented as the
backend `melgeek-made68air-probe` (`NativeAnalogProtocol::MelGeekMade68AirProbe = 32`,
`melgeek_made68air_probe_backend.cpp`, `melgeek_made68air_probe_protocol.h`, test
`melgeek_made68air_probe_test.cpp`, switch `kEnabled`). Exact identity `0416:0114` only. It publishes no
analog value, claims no key, shows no UI notice (`connected=0`; `present=1` only while the vendor collection is
open, so the Configuration page lists it as "detected"). Backups: `.local/backups/melgeek-probe-20261012/`.

What it does, in order, every time the keyboard appears:

1. **Inventory.** For every HID collection of `0416:0114`: `evidence.melgeek caps idx= mi= col= up= u= role= in= out= feat= ...`
   and one `cap` line per input/output/feature value or button capability (report id, usage page, usage range,
   bit size, report count, logical min/max). Device paths are never logged, only the interface and collection numbers.
2. **Passive readers.** Every collection except keyboard and mouse (owned by Windows, never opened) is opened
   for reading with overlapped I/O and kept open. Each received report is hashed; the first 300 distinct reports per
   collection and the first report of each new class (first two bytes of a vendor frame, report id otherwise; up to 64) are
   written as `rx col= ctx= n= rid= hdr= data=`. In addition, per byte position the minimum, maximum and change count are
   written as `stats` / `statbytes` lines (`index:min-max:changes`, index 0 is the report id) at a geometric schedule
   (every frame at first, then +25 %) and at session end, so a stream that follows the keys is visible even when
   individual frames are not listed. A gamepad-role collection with axes would show up here too.
3. **Read-only getters on the vendor collection** (usage page >= 0xFF00 with an output report): `get_ver (01 87)`,
   `get_hw_info (01 8B)`, `get_triggers (0A 81)`, `get_report (0A 94)`, `get_trigger_accuracy (0A 97)`, `switch_info (0A A3)`,
   each as an empty-payload frame `[module, op, 0, 0, 0, 0, FF, FF]`; with no reply within 600 ms the same getter is
   retried once with CRC bytes `00 00` (the HIVE receiver skips the check on zero CRC bytes). Logged as `tx` and every
   frame that follows as `rx ... ctx=reply|during|stream seq=` with the parsed header and CRC state. `probe_done` gives
   `replies=N of=6`.

Safety: `MakeGetterFrame` refuses every (module, op) pair that is not in `kGetters` and leaves the buffer untouched;
the unit test enumerates all 65,536 pairs and requires exactly those six. No setter, calibration, interaction,
gamepad-tester, factory, log or firmware command exists in the module. Three repeated failed sessions block the probe until
the device list changes. The set was chosen from names in the HIVE core (`get_ver`, `get_hw_info`, `get_triggers`,
`get_report`, `get_trigger_accuracy`, switch info); `0A 99` (dead zone / rapid trigger data) and `01 8E` (sleep time) were left
out because their read/write direction was not certain.

How to read the result (the tester only needs to run HallJoy with the keyboard connected, press some keys and open the log;
no script, no timed window):

| Log shows | Meaning |
| --- | --- |
| `caps ... role=gamepad` with value caps (axes) and `statbytes` that vary while keys move | The keyboard presents key depth as ordinary HID axes: analog path exists, decode from the descriptor. |
| Vendor `rx ctx=stream` frames or `statbytes col=<vendor>` bytes that change with key presses and no `tx` was needed | Unsolicited depth/ADC stream in normal mode: analog path exists, decode the frames (header `m.. op..`, CRC state). |
| `rx ctx=reply` for `get_ver` / `get_hw_info` | Transport and framing confirmed; the reply carries the `hid_ver` (256 or 257) and firmware version. |
| Replies only, nothing on stream, no gamepad axes | No analog in normal mode that does not need a command. The only known ADC consumer is the calibration screen (`up_trigger_adc`), which the probe does not enable. |
| `no_reply` for all getters | Framing assumption wrong (collection, report id, length); the `caps` lines give the correct framing for the next build. |

Limit of the method (stated so the answer is not over-read): the probe is passive plus read-only. A "yes" is conclusive.
A "no stream" means no analog without an enabling command; the only candidate left is the calibration-time ADC stream, which
needs a state-changing command (calibration state / `use_adc_log`) that can change calibration data or block typing. That
step is not in this build and needs a separate owner decision.

Validation: `run_native_backend_checks.py --require-compiler` PASS (log `.local/melgeek-probe-checks-20261012.log`, includes
the new test and the audits); `build_release.ps1` PASS with every gate (full catalog image check with the new protocol in the
expected list, support diagnostics, profile transactions, layout catalog, supported layouts, startup recovery; log
`.local/melgeek-probe-build-20261012.log`). Local EXE `build/bin/Release/x64/HallJoy.exe`, SHA-256
`46204ebcbcb26bc2a7e215a1e302d46abb9556097efb67524e5e5c152683d3f1`, version string unchanged (1.6.9.1): the build is told apart
from the release by its `evidence.melgeek session rev=1` lines and by the hash. Not published, no version bump.
No device test: the probe has never run against a MADE68 Air; the CRC, frame builder, getter allow-list, role classification and
statistics are unit-tested, the Win32 HID side (enumeration, overlapped readers, getter exchange) is compile-checked only.

## First hardware log of the probe (2026-10-12, `message (2).txt`, HallJoy 1.6.9.1 probe build)

Result: the transport and framing are confirmed; the stream question is NOT answered by this log.

- Collections of `0416:0114` (6): interface 00 has keyboard `01:06` (in 9 / out 2, report id 1), mouse `01:02` (report id 2),
  system control `01:80`, consumer `0C:01` and a second keyboard `01:06` (in 32, report id 6, the NKRO collection); interface 01 has the
  vendor collection `FF60:61`, input and output 65 bytes (report id 0, one 64-byte value array each, usages `62`/`63`). So the other
  assistant's "FF60:61, 64-byte, no report id" note was right for the transport. There is no gamepad/axis collection.
- Probe: the vendor collection was opened, all six getters were answered on the first attempt with CRC bytes `FF FF`, every reply has a
  valid CRC, and the reply op is the request op with bit 7 cleared (`87->07`, `8B->0B`, `81->01`, `94->14`, `97->17`, `A3->23`). Reply data
  lengths: `get_ver` 28, `get_hw_info` 20, `get_triggers` 6, `get_report` 1 (`00`), `get_trigger_accuracy` 1 (`FF`), `switch_info` 1 (`09`).
  So the HIVE frame description (8-byte header, CRC16/CCITT over the data, empty frame `FF FF`) is correct for this keyboard. The `get_ver`
  data holds version fields and an 8-byte device identifier; the version layout and `hid_ver` are not decoded yet (a `00 01` pair at data
  offset 8 would read as 256 little-endian, a guess only).
- Why no stream conclusion: `uptime_ms` in the log is the Windows uptime (`GetTickCount64`), the PC had been up 79 s, and the report was
  written about 50 ms after the probe finished (last evidence line 79,671 ms; probe done 79,640 ms). No key was pressed in that window, so
  zero unsolicited reports (no `ctx=stream` line, `stats` stopped at the six replies). Absence of frames here is not evidence of a missing stream.
- The other assistant's `KNOWLEDGE.md` hypotheses (a `buf[0]` command byte 0..15, a 0x00..0xFF command sweep) do not match the real device
  (it answers the HIVE `module/op` frames). Do not use them; never sweep unknown command bytes on this keyboard.
- HIVE UI evidence (static, JS): the ADC grid of the Verify tab (`key_adc_info`, up to 200 x 200 `{adc, trigger}`) is filled only from
  `up_trigger_adc` events; the Calibration button sends `set_perf_info` with `calibration state = 1` and leaving the tab resets it to 0. So HIVE
  itself shows ADC values together with a calibration state. This is UI behaviour, not proof of what the firmware sends in normal mode.

Next: a second log from the same build after keys have been pressed while HallJoy runs (partial and full presses, several keys, a few
seconds, then Open log). Stream reports, if any, appear as `rx ... ctx=stream` and `statbytes col=5` lines. If it shows nothing, the
remaining question (calibration-state stream) needs an owner decision; calibration mode is outside the game requirement anyway.

## Second probe log and the report-mode finding (2026-10-12, `message (3).txt`)

Second log from the same probe build (system uptime 420 s, final snapshot at 449.7 s, so about 29 s of observation after the probe).

- Same inventory and the same six getter replies as the first log (byte for byte).
- Unsolicited reports in the whole window, on every collection the probe could read (vendor, system control, consumer): 7 in total
  (telemetry `updates=7`): the 6 getter replies plus ONE consumer-control report (`rid=04`, data `0000`, a release) at 428.1 s.
  The vendor collection `FF60:61` produced no unsolicited report at all. The log cannot prove that ordinary keys were pressed (the
  keyboard collections are system-owned and never read), so this is "no spontaneous stream while the keyboard is idle or typed on",
  not a verdict on a stream that must be enabled.
- `get_report` (`0A 94`) returned 0, consistent with "report mode off" (see below).

New static finding from the HIVE UI code (JS, `KBDevKeyPerf-*.js`, tab switch handler): when the performance page switches tab,
- to tab 3 (`perf.tabs.verify`, English "Calibrate") or tab 4 (`perf.tabs.test`, "Detect", mode `key_perf_tester`) it sends
  `set_perf_info --path <dev> --use_report 2`;
- to any other tab it sends `set_perf_info --key_ids [0] --calibration {state:0} --use_report 0`.
Manual calibration is a separate action on the Calibrate tab (`--calibration {state:1}`), so `use_report` is the switch that makes the
keyboard push live per-key data (`up_trigger_adc`: `{id, adc, trigger}`), independent of the calibration state. The UI tells the user
"Staying on this tab may affect performance", and states that the keyboard otherwise calibrates itself automatically in real time.

The WASM builder of the corresponding commands (`set_report_v1_0`, `get_report_v1_0`): `get_report` = header `0A 94 00 00 b4 00 crc`
(no data; sent by the probe with b4 = 0 and answered), `set_report` = header `0A 93 01 00 b4 00 crc` plus ONE data byte (the mode); b4 is a
field of the device object (value unknown; the getter accepted 0). The modes seen in the UI are 0 (off) and 2 (on).

Consequence: the earlier "no stream without a calibration command" reading was too pessimistic. The vendor's own live-data mode is
`report = 2`, a runtime toggle that the vendor UI switches back to 0 when the tab is left. Whether it gives a usable all-key depth stream
without blocking typing, and at what cost to the key scan ("may affect performance"), is only answerable on the keyboard.

## Probe rev 2: reversible live-report experiment (2026-10-12, owner approval)

Owner decision: "yes, do it; there are already protocols with an analog-reporting flag and enabling it is fine as long as it is not
calibration". Rev 2 of `melgeek-made68air-probe` (`kRevision = 2`, switch `kLiveReportExperiment`) does, after the six getters:

1. If `get_report` read 0 (off): write `set_report` mode 2 (`0A 93 01 00 00 00 crc | 02`, the vendor UI's own toggle for its
   Calibrate/Detect tabs), then read `get_report` back. If `get_report` was not 0 or could not be read, nothing is written
   (`experiment_skipped`).
2. Log every report the keyboard sends while the mode is on (`rx ... ctx=live`, the first 1000 distinct vendor reports, the first report of
   each new class, per-byte `stats`/`statbytes`).
3. Restore mode 0 and verify it with `get_report`: after 20,000 unsolicited vendor reports (bounded by content, not time), or when the session
   ends, HallJoy closes (Stop) or the device list changes; on a removed device the write fails and `restore_incomplete` is logged.
   While the mode is on the Configuration page shows "live report mode ON for diagnostics, restored on exit".
4. Count key-down transitions of the whole keyboard (`GetAsyncKeyState`, count only, no key identity): `keydowns total=N` at 1, 2, 4, 8, ...
   and in every `stats` line, so the log shows whether keys were pressed during the observation.

Only these frames can be built: the six getters and `set_report` with the value 0 or 2 (`MakeSetReportFrame` refuses every other mode;
the unit test enumerates all 256 modes and all 65,536 getter pairs). No calibration, interaction, factory, log or firmware command exists.
The write of mode 2 is recorded before it is sent, so a half-failed write is still followed by a restore attempt. Reply matching uses the
observed rule reply op = request op & 0x7F; with no reply in 600 ms the request is repeated once with the other CRC variant (CRC bytes
`00 00`); the set-report acknowledgement format is unknown and only logged.

Log vocabulary: `experiment_start`, `tx name=set_report_live`, `rx ctx=reply` (ack), `experiment_on mode=<read back>`, `rx ctx=live ...`,
`experiment_restore reason=frame_limit|stop|session_end`, `experiment_off mode=0`, `restore_unconfirmed`, `restore_incomplete`, `keydowns`.

How to read it: `experiment_on mode=2` and `ctx=live` reports that change with key presses (see `statbytes col=5`, `keydowns` > 0) mean an
analog (ADC/trigger) stream exists without calibration: decode `m.. op..` frames next (the HIVE event is `up_trigger_adc`, entries
`{id, adc, trigger}`). `experiment_on mode=2` with `keydowns` > 0 and no `live` reports means the mode alone does not stream. `mode=0` after the
write means the keyboard refused the value or needs a different b4/mode. A typing problem while the mode was on is itself a result: tell the
tester to close HallJoy (restores 0) or replug the keyboard.

Rev 2 build: `build_release.ps1` PASS with every gate (log `.local/melgeek-probe2-build-20261012.log`); unit test PASS (setter allow-list,
CRC of the observed `get_report` reply, reply-op rule). Local EXE `build/bin/Release/x64/HallJoy.exe`, SHA-256
`9baddcd757571282cc2e2cf7fc2256e0be24ce98a4360533588254078494ac76` (still version string 1.6.9.1; recognized by `evidence.melgeek session rev=2`).
The rev 1 EXE and sources are kept in `.local/backups/melgeek-probe-20261012/rev1/`. Not published. No device test of rev 2: the setter,
verify and restore sequence has never run against a MADE68 Air; the unit test covers frame building and parsing only.

## Third probe log (rev 2, `message (4).txt`): the analog stream exists (2026-10-12)

Verdict: **the MADE68 Air streams live per-key travel to the host, outside calibration, for all 68 keys, once `set_report` mode 2 is written.**
Sequence in the log (system uptime 1495 s): `get_report` read 0, `experiment_start`, `set_report_live` (mode 2) was acknowledged
(`rx ctx=reply m0a.op13.len1`, CRC ok), the read-back `get_report` returned 2, `experiment_on mode=2`, and the first `ctx=live` frame arrived about
60 ms later. The session was still running when the log was written (34 s of stream, 3,393 reports, 31 key-downs counted), so the restore
(`experiment_off`) is not in this log.

Stream frame (vendor collection `FF60:61`, report id 0, 64 bytes): header `08 09 <len> 00 00 00 <crc lo> <crc hi>` (module 08, op 09, CRC16 of the data
ok on every frame), `len` = 4 x entries (4..56, so at most 14 entries per frame), then the entries, each 4 bytes:
`[key id u8][position u8][ADC u16 little-endian]`. This matches the HIVE `up_trigger_adc` event (`id`, `trigger`/position, `adc`).

- Key ids 0..67 (68 distinct ids, exactly the 68 keys). The id-to-key mapping is not in the log; it must come from the HIVE device configuration
  (`kb_common_made68air.json`, `kb_keycode_made68air.json`, local copies under `.local/research/melgeek-made68air-20261012/hive/cfg/`).
- Position byte: 0 = released, up to 46 (0x2E) = fully pressed (45 or 46 at the bottom); a press was seen as 0, 5, 12, 25, 46 over about 140 ms.
- ADC: idle baseline per key 3069..3530 (spread over keys, noise +-10); it falls as the key goes down, about 20 counts per position unit at the
  top, 900 counts (809..955) at the bottom. The position byte is monotonic with the ADC drop (checked on ~300 pressed-key entries), so both are usable;
  the position byte alone gives 47 levels, the ADC about 900 usable levels after per-key baseline/end calibration.
- The report is change-driven and sent in bursts: a burst of 4..5 frames carries the keys whose value changed (noise included), a pressed key is
  present in every burst. Bursts come every 47..62 ms (177 gaps of 47 ms, 44 of 62 ms, tick resolution 15.6 ms), i.e. **about 20 updates per second per
  key**; the average stream is ~99 frames per second. Per-key sampling of ~50 ms is the weak point for gaming (a 100 ms tap gives 2..3 samples).
- Idle: with no key pressed the stream still carries entries for noisy keys (position 0, ADC +-10), so a consumer must use the position byte or a
  threshold above the noise.

Open points before this can become a backend (none needs calibration):
1. **Rate.** Is 20 Hz the keyboard's limit in mode 2 or can another report mode or setting raise it? Only modes 0 and 2 are known (the vendor UI uses
   only these). Candidate probe: read-only `get_report_info` (op unknown), or a bounded, restored trial of other mode values (owner decision).
2. **Typing while the mode is on.** Not visible in the log (the keyboard collections are not read). The tester must say whether normal typing and
   key release behaved normally during the 34 s; the vendor UI warns "Staying on this tab may affect performance".
3. **Restore.** Confirm in the next log that `get_report` reads 0 at start (a leftover 2 would give `experiment_skipped reason=report_not_off mode=2`)
   and that `experiment_off mode=0` appears after closing HallJoy.
4. **Key map** from the HIVE config, layout/ANSI-ISO identity, and the absence of a stock configurator conflict (HIVE writes mode 0 when its tab is left).
5. The report gives no explicit "released" frame; a key leaves the stream when its position returns to 0 (observed: 46 -> 25 -> 0 over ~100 ms).

## Analog backend (2026-10-12, owner: "enable the analog fully, speed it up afterwards")

After the third log (the stream exists) the owner asked to turn the analog on completely first. The probe became the backend
`melgeek-made68air` (`NativeAnalogProtocol::MelGeekMade68Air = 32`; files `melgeek_made68air_backend.cpp/.h`,
`melgeek_made68air_protocol.h`, test `melgeek_made68air_protocol_test.cpp`; probe revisions 1 and 2 are kept in
`.local/backups/melgeek-probe-20261012/`).

How it works:
- **Admission**: exact `0416:0114`, vendor collection `FF60:61` with 65/65-byte reports, claimed through the routing registry, and the
  `get_ver` reply must come back with a valid CRC (the keyboard names itself through the protocol). Yellow notice
  "MelGeek MADE68 Air: hardware testing incomplete" while connected (`NativeNotice` protocol 32, flag `MelGeekMade68Air = 1<<30`).
- **Mode**: reads `get_report`. 0: writes `set_report` 2 and verifies it; 2: adopts it (a leftover from a crashed run); anything else:
  untouched (`mode_skipped`). Connected = mode verified. The mode is restored to 0 when the session ends, on HallJoy exit (Stop waits up to
  8 s for it), and the write is retried with the other CRC variant if no reply comes. If the stream is silent for 3 s the backend asks
  `get_report` again and re-enables a mode that fell to 0 (configurator tab left, keyboard reset).
- **Stream**: frames `08 09` decoded into key id, position (0..46) and ADC; invalid frames (CRC, id >= 68, odd length) are dropped whole.
  Depth = position * 1000 / 46. The ADC is logged and counted but not used yet (47 position levels; the ADC would give about 900 levels
  after a per-key baseline, a later refinement).
- **Key map**: learned at run time. A Raw Input key-down of this keyboard (`vid_0416&pid_0114` in the Raw Input device name,
  `MelGeekMade68Air_ObserveRawKey`, called next to the Logitech hook in `app.cpp`) is paired with the single key id whose position left 0
  within [-600 ms, +160 ms]; chords are not guessed. A learned key is bound to its HID usage and published (`learned_key id= hid=` in the
  log). Learned keys survive a reconnect inside the process. A key works as analog from the moment it has been pressed once; the first press
  of a key per session loses at most its first samples. A fixed table can replace the learning once the log has collected the 68 pairs.
- **Held keys**: the stream is change-driven, so held values are re-published every 100 ms while connected (freshness 1 s).
- **Diagnostics kept in the log** (`evidence.melgeek`): collection inventory, getter replies, first 200 distinct stream frames, per-byte
  statistics and, for the speed work, `rate` lines: average inter-frame gap and histograms of the inter-frame gaps and of the gap between
  updates of one pressed key (`key_gap_lt20ms/40/80/ge80`). The rev 2 key-down counter (`GetAsyncKeyState`) was removed: a keyboard-wide
  poll does not belong in a product build.
- **Not done / limits**: no built-in layout (manual layout selection); about 20 updates per second per key in this mode (the next task is to
  find out whether the rate can be raised); typing behaviour while the mode is on is untested; the full 68-key map is not known yet.

Validation: unit test (real stream frame from the log decodes with a valid CRC; synthetic frames; position mapping; learner scenarios),
`keyboard_support_status_test` (notice for protocol 32), `run_native_backend_checks.py --require-compiler`, `build_release.ps1` PASS with every gate
(log `.local/melgeek-analog-build-20261012.log`), EXE `build/bin/Release/x64/HallJoy.exe` SHA-256
`7acef26acde7e65619594d7cf1ab2742f3b0725e76e8cf31237d6e1be1b12b67` (version string still 1.6.9.1, not published). Sheet `Main!C561`:
"Research incomplete" -> "Implemented; awaiting hardware testing" (value-only write; dropdown value valid, effective colour yellow, neighbours unchanged).
No device test of the product backend.

### Missing yellow banner and Open log button (found and fixed 2026-10-12, same day)

Symptom (owner): with the analog backend the yellow notice disappeared and a user could not find the log (the banner carries the
"Open log" button and is shown only when no analog source is connected or a model notice is active).
Cause: `SetSearchObservation` (`keyboard_support_status.cpp`) packed the model flags into 29 bits (`frozenModels & 0x1fffffff`). The flags
`GenericProtocol` (1<<29, added 2026-10-07) and `MelGeekMade68Air` (1<<30) were silently dropped, so with a connected analog source the banner
was hidden. Alumix 68 (1<<28) still fitted. The same defect hides the "Keyboard not in the list" notice (and its Open log button) of generic
BY/IPI/Logitech keyboards in the published builds since the generic support shipped; it is NOT specific to the MADE68 Air.
Fix: the snapshot is a 64-bit atomic (bit 0 search done, bit 1 connected, bit 2 communication warning, bits 3..34 all 32 model flags).
Regression test: every flag bit 0..30 round-trips through `SetSearchObservation`/`GetStatusSnapshot` (the new test fails on the old code,
checked). Backups: `.local/backups/melgeek-probe-20261012/banner-fix/`. Rebuilt EXE SHA-256
`b68f625649c79e0bc2e75d85a6e3beb28d39fed3ea816872a67c3ac8aa666473` (build and native checks PASS; logs `.local/melgeek-banner-build-20261012.log`,
`.local/melgeek-banner-checks-20261012.log`). This supersedes the hash above.

## First log of the analog backend (`message (5).txt`, backend rev 3) and the prior key map (2026-10-12)

The backend works on the keyboard: `connected=1`, mode 2 acknowledged and verified, the banner (after the snapshot fix) is shown, bound keys
reach the gamepad (`trace.src melgeek ... milli=978` for W bound to a trigger; the left stick from A/D, S and W as triggers in the test profile).
- **Typing during the mode**: the Raw Input stream of the keyboard kept delivering ordinary key-downs the whole time (18 keys were paired from
  them), so the mode does not block normal key reports. (Text typed into other applications was not reported by the tester.)
- **Rate**: 3,385 stream frames in 34 s. Gap histogram: 2,697 gaps under 2 ms, 687 gaps over 30 ms, none in between: the keyboard sends bursts of
  5 frames (4 within 2 ms) about every 48 ms. Per pressed key: 635 updates 40..80 ms apart, 8 over 80 ms, none under 40 ms. So the whole keyboard
  refreshes about 21 times per second. First sample after the OS key-down: 94 ms in the W trace (one burst period plus the actuation lead).
- **Learner mispairs**: 18 keys were learned, 14 consistent with a 68-key 65% layout of 15/15/14/14/10 keys in reading order, 4 not
  (id 38 = N, id 50 = M, id 51 = Space, id 61 = V). Cause: the old learner paired a key-down with the only key that had started moving, but the
  next key's own sample arrives up to 60 ms after its key-down, so it paired it with the previous key. Pure run-time learning is therefore unsafe
  for fast typing.
- **Prior key map (rev 4)**: ids in reading order, rows of 15, 15, 14, 14 and 10 keys. Established by the log: W R T Y U I (17, 19..23), A S D F G H J
  (31..37), B (49). The rest of the alphanumeric block, punctuation, modifiers (LCtrl/LWin/LAlt, LShift/RShift), Space and the arrows follow the layout.
  Left empty (learned at run time, only for a usage the table does not know and only for an unmapped id): 0 (Esc or grave), 14 (Del), 29 (PgUp),
  43 (PgDn), 57 (End), 62..64 (right Alt, Fn, right Ctrl/Menu). 60 of 68 ids are mapped from the start (bound at connect, no loss of the first press).
- **Checking instead of trusting**: every key-down of this keyboard is compared with the table: `key_confirmed id= hid=` the first time a key is
  confirmed, `key_conflict id= table= os=` (at most 64 lines) when a different key moved than the table names. A wrong table entry is reported and
  never overwritten. The log therefore tells which of the 60 entries are verified (target: all 60 confirmed) and which need a fix.
- Rev 4 EXE: SHA-256 `220e7a78a074ad9a2733e2aed8a3f9b3403efe075dacb2cecbb64eac92dd7e6f` (build PASS; unit test covers the table, confirmation,
  the N-after-K case from the log, learning, chords, conflicts, auto-repeat). Supersedes the hash `b68f6256...` above.

## Table verified, layout added, speed trial (rev 5, 2026-10-12)

**Key table verified.** Tester log `message (8)` (slow sequential key presses, row by row; the build was still the pure run-time learner, rev 3):
66 key ids were paired with Windows key-downs, 65 equal to the photo-based table. The single difference: id 14 sends the grave usage 0x35
(its keycap shows a house symbol), not Home. Not paired: id 59 (Win, the Start menu opened) and id 63 (Fn, no Windows key event). The earlier fast
log `message (7)` mispaired many keys for the reason described above; the slow one shows the table is right. The table now has all 68 ids
(`kPriorKeyMap`; id 63 = HallJoy's Fn code 0x409); run-time pairing only checks it (`key_confirmed`, `key_conflict`).

**Layout.** `tools/build_melgeek_layouts.py` builds `melgeek_made68air_ansi` (68 keys, rows of 15/15/14/14/10, 16 units wide, from the owner's
top-view photo; the build asserts that the layout order equals the key-id table). Registered in `tools/layout_catalog.json` (brand MelGeek),
integrated by `layout_pipeline.py integrate` (identity `melgeek-made68air` / `MADE68AIR-0416-0114`), recorded in the research reference checks.
The backend publishes the matching layout token while connected, so the layout is selected automatically. The status in the Sheet stays
"Implemented; awaiting hardware testing".

**Speed trial (owner: "try to raise the analog performance in the next build").** The 48 ms burst period is 2 x 24 ms, so report mode 1
may stream twice as fast. The only setter values the code can build are now 0, 1 and 2. Each run:
1. mode 2 is enabled and verified; its stream is measured first (up to 60 frames, safety stop 3 s) - the keyboard is not published as connected yet;
2. mode 1 is written, verified by `get_report` and measured the same way (150 frames; safety stop 2 s without any frame, 6 s in total);
3. mode 1 is KEPT for the whole run when it is clearly faster (frame rate at least 1.3x and burst period at most 0.75x, `RateMeter`/`CompareRate`);
   it goes back to mode 2 only when something is not right: no frames, the read-back is not 1, not clearly faster. A mode that works is never rolled
   back because of elapsed time; the time limits only end the measurement. The choice is remembered for the rest of the run (reconnects use it);
4. if mode 1 is silent at idle (event driven), it is looked at once more after at least 5 key-downs and 5 lag samples: kept when the key-down to
   stream lag is below 0.6 x the mode-2 lag (and 20 frames), otherwise back to mode 2.
A leftover mode 1 from a crash is recovered to the chosen mode at the next start; mode 0 is written on exit as before.
Log lines: `trial_baseline_start`, `trial_start kind=idle|event`, `trial_result ... fps= period_us= lag_avg_ms= verdict=`, `trial_base`, `trial_keep`,
`trial_reverted`, `trial_failed`, `trial_revert_failed`, `trial_skipped`; `key_confirmed ... lag_ms=` gives the lag of the stream behind every key
(positive = stream late) so the latency can be read from any ordinary log. Connection is delayed by the baseline and the trial (typically 2-3 s,
at most about 10 s) once per run.

Validation: unit test (table, mode allow-list, rate meter and verdicts, learner scenarios incl. lag), `run_native_backend_checks.py --require-compiler` PASS,
`build_release.ps1` PASS with all gates (logs `.local/melgeek-speed-build-20261012.log`, `.local/melgeek-speed-checks-20261012.log`); layout audits PASS (yellow
layouts 80 existing). EXE SHA-256 `b85f2e7372c69dc4e7b9c8bb6a4f981276c22c548e4a8102537c9679c7c549c6` (supersedes `220e7a78...`). Not run on the keyboard.

## First rev 5 log (`message (9).txt`, 2026-10-15 review)

Rev 5 EXE (`b85f2e73...`), report written 2026-10-12T20:36Z, about 65 s of keyboard activity. The keyboard had just been plugged in
(`device.change` lines right before the backend started). The profile had no key bindings (`unique_keys=0`), so this log does not exercise the gamepad.

- **Start state**: the first `get_report` returned 0. A fresh plug-in also gives 0, so this does not prove that the restore on exit worked.
- **Connection**: all 6 getters answered, mode 2 was set and read back as 2, baseline measured, trial ran, then `connected=1`. The backend snapshot
  shows `connected=1 mapped_keys=68 failures=0`, with 5,767 updates by the end of the report.
- **Mode-2 rate, corrected**: baseline 60 frames, `period_us=49999`. The burst period is a steady 50.0 ms (20 Hz per key), not the "~48 ms"
  estimated earlier from the 15.6 ms log timestamps. At idle every burst lists all keys (5 frames of 14 entries: 56 bytes, the last frame shorter),
  so the stream is a continuous full scan, about 100 frames/s, not change-driven.
- **Mode 1 trial**: `set_report` 1 was acknowledged and read back as 1. The stream kept its mode-2 pace for about 0.5 s (55 frames), then stopped:
  no frame for the remaining ~5.5 s, until the 6 s cap ended the trial. `trial_result ... fps=107.9 period_us=49999 verdict=same`, then
  `trial_reverted mode=2` with read-back 2, and the full-scan stream resumed at once. So mode 1 is not a faster full scan. At idle it is (at least
  after a short tail) silent, which suggests event-driven reporting.
- **Trial accounting defect**: `RateMeter::Fps()` divides by the span from first to last frame, so the 5.5 s of silence is not counted. Mode 1 really
  delivered 55 frames in 6 s (about 9 fps), not 107.9. The outcome was right (back to mode 2), but for the wrong reason. Also,
  `g_idleSilent` requires exactly 0 trial frames, so a stream that stops after a short tail does not trigger the planned
  event trial (mode 1 while keys are pressed). The question "is mode 1 faster for a pressed key" is therefore still open.
- **Key table**: 52 `key_confirmed`, 0 `key_conflict`. Confirmed in this log: row 1 (ids 0..14), Caps..Enter (30..42), the shift row (44..55),
  the bottom row except Win and Fn (58, 60..62, 64..67), and Del 29 / PgUp 43 / PgDn 57 / Up 56. Not pressed: Tab row (15..28), Win (59), Fn (63);
  those were verified by `message (8)`. Together the two logs confirm the whole table.
- **Latency**: lag from the OS key-down to the first moving stream sample (`lag_ms`, 52 keys): median 16 ms, mean 17 ms, range -188..94 ms.
  The values come in steps of about 15.6 ms (GetTickCount resolution), so they are approximate. Negative values mean the stream showed travel
  before Windows reported the key-down (slow pre-travel, e.g. Caps -188 ms). In ordinary presses the analog value trails the key-down by 0..47 ms,
  which is consistent with one 50 ms scan period.

Next (needs a code change, not built): measure the trial over its whole window (frames per elapsed time, plus "stopped streaming" detection), and treat
"silent for the last 2 s" as idle-silent, so the event trial runs once keys are pressed. Optional: QPC timestamps for `lag_ms`.

## Rev 6: report mode 1 from the start, evidence from every key press (2026-10-15)

Owner (2026-10-15): test the real mode 1, possibly mode 1 only; a log must never come back useless because HallJoy was
waiting or measuring when the tester opened it.

**Behaviour.**
- The backend writes `set_report` 1 when `get_report` reads 0, verifies it by read-back and reports connected at once.
  No baseline, no trial, no timed phase; the rev 5 trial code (`RateMeter`, `CompareRate`, trial phases) is removed.
- Every key press is written as one line, about 0.2 s after its release (the time a late Windows key-down may still need):
  `press id= hid= m=<mode> os=<matched a Windows key-down> lag_ms=<stream start minus key-down> samples= rise=<samples up to
  the peak> rise_ms= dur_ms= peak= gap_min_ms= gap_avg_ms= gap_max_ms=`. The first 160 presses get lines; `press_summary` lines
  (per mode, at 1, 2, 4, 8 ... presses, on fallback and at session end) add up all of them. So the first key press already
  answers the question, and a log opened at any moment holds every press made so far.
- Timing uses QueryPerformanceCounter for the stream samples and the Windows key events (`lag_ms` is no longer in 15.6 ms steps).
  `NowUs` no longer overflows after about 10 days of system uptime (the old `t * 1000000 / freq` did with a 10 MHz counter).
- Mode 1 is kept unless key presses show it does not serve them (`mg::ModeHealth`, judged per press, never by time):
  - `key_miss`: Windows reported a key-down of a mapped key and no unexplained key moved in the stream around it
    (160 ms after the key-down; a Fn combination such as Fn+Up -> PgUp moves another key and is not a miss);
  - `release_stuck`: Windows reported the key-up, the key sent no later stream sample within 300 ms and its last position is
    above 0 (the gamepad would keep the key pressed).
  Fallback after 3 misses in a row, 2 stuck releases, or more than 20 % misses once 10 key-downs were judged; also when the
  mode-1 read-back fails. `mode1_fallback reason=` is logged with the mode-1 counts and summary, then mode 2 is enabled and
  verified; later press lines carry `m=2`, so the same log compares both modes. The next HallJoy start tries mode 1 again.
  Key-downs Windows never delivers (an elevated window in front) count neither way.
- Key matching (KeyLearner): a press stays known after its release and stays claimed until the next press, so a quick tap
  whose release sample arrives before the Windows key-down is still matched, and a second key-down never reuses an old press.
  Key-downs are matched against the positions before each frame as well as after it.
- Restore on exit is unchanged (`set_report` 0, verified; `mode_off`). Status text: "live key travel active (report mode 1,
  under test)" or "(low update rate, 20 per second)" after a fallback.

**Reading the next log.**
| Log shows | Meaning | Next |
| --- | --- | --- |
| `press ... m=1` with `rise` above 1-2 and `gap_min_ms` well under 50, `os=1`, small `lag_ms` | mode 1 reports motion faster than the 50 ms scan | keep mode 1 as the normal mode; drop "under test" |
| `press ... m=1` with gaps around 50 ms | mode 1 is not faster | go back to mode 2 as the default |
| `mode1_fallback reason=misses_in_row` / `miss_ratio`, `key_miss` lines | mode 1 does not report presses (or not all) | mode 2 default; the `m=2` lines are the baseline |
| `mode1_fallback reason=stuck_release`, `release_stuck` lines | mode 1 loses releases | mode 2 default |
| `mode1_fallback reason=readback` | the keyboard did not keep mode 1 | mode 2 default |
| new `rx` classes other than `08 09` after `mode_on mode=1` | mode 1 uses another frame format | decode it from the logged frames |

Tester steps: start HallJoy, wait for the keyboard to show as connected, press keys (taps, a few slow presses, a held key),
then Open log. No waiting time and no particular duration are needed.

Validation: unit test extended (quick tap, no reuse of a claimed press, miss only when nothing moved, Fn+Up, press records
for scan and event streams, the three fallback reasons); `run_native_backend_checks.py --require-compiler` PASS
(`.local/melgeek-rev6-checks-20261015.log`); `research_reference_checks.py --record` re-run because the protocol header is a
layout reference (key table unchanged, `MELGEEK_LAYOUTS=PASS` same layout); `build_release.ps1` PASS
(`.local/melgeek-rev6-build-20261015.log`). EXE SHA-256 `c86588b86bf312df6a94acb9c37066d662388bdf4987d1a5ef5f73a2f72fc791`,
version 1.6.9.1, not released. Not run on the keyboard. Backups: `.local/backups/melgeek-rev6-20261015/`.


## First rev 6 log (`message (10).txt`, 2026-10-15)

Rev 6 EXE, report 2026-10-15T12:51Z, about 2.5 minutes of use. `get_report` read 0, `set_report_mode1` acknowledged and read back
as 1, `connected=1` at once. No `key_miss`, no `release_stuck`, no `mode1_fallback`: mode 1 served every key press.
- 160 `press` lines (the cap), `press_summary` up to 128 presses: os-matched 119/128, misses 0, stuck 0. The 9 presses without a
  Windows key-down are all `peak=1` touches of A/W (a resting finger, below actuation), not lost key-downs.
- Mode 1 is change-driven: 891 frames in the whole session (mode 2 sends about 100 frames/s at idle). Only `08 09` stream frames,
  no new frame class.
- **Rate: unchanged.** `gap_min_ms` is exactly 50.0 ms in 152 of 160 presses (the rest 100/150 ms); a key moving down gets one
  sample per 50 ms (rise 1-3 samples, median 50 ms). Mode 1 sends only the keys that changed, on the same 50 ms (20 Hz) tick as
  mode 2. The firmware's report period is the limit in both modes.
- Lag from the Windows key-down to the first stream sample (QPC): median 13 ms, 10-90 % range -28..37 ms (negative = travel seen
  before the key-down).
Conclusion: mode 1 works and is safe in this log, but it is not faster. Modes 0/1/2 cannot raise the 20 Hz rate. Remaining
options for speed would need other commands (for example the scan-speed/report-info getters named in the HIVE core), which are
not built and need an owner decision.
