# Logitech G RAPID (PRO X TKL RAPID, PRO X2 RAPID)

**Current decision (2026-11-07).** PRO X TKL RAPID (`046D:C35B`) is unsupported
on the reviewed stock path. The owner requires independent analog for the full
keyboard and simultaneous presses/releases. The runtime excludes this PID from
both exact and generic Logitech admission. The archived diagnostic build and
layout remain available for research. PRO X2 RAPID (`046D:C364`) is Experimental.
The earlier implementation and test builds below are historical evidence.

The exact C35B USB identity is in the red runtime catalog. Its experimental
read-order and trace changes are archived, including the test EXE, under
`.local/backups/logitech-c35b-unsupported-20261107/`. The existing layout is
retained. The next admission requires evidence of independent simultaneous
depth and release across the keyboard from a verified stock or alternative
firmware path.

Live Sheet readback on 2026-11-07: Logitech G / PRO X TKL RAPID,
Main!C492, Implemented; awaiting hardware testing -> No usable analog found.
B492:C492 have red base and effective backgrounds, validation and neighbors
are preserved. Main!C491 (PRO X2 RAPID) retains its yellow status.
Live audit PASS: 283 yellow, 11 red (6 detected), 88 Supported layouts.
The standard release build and native backend checks passed. The installed
local EXE SHA-256 is
`4f73026a23ce0110f5ab6f518615a5024f9b5026fcfbfe3b1d99a0a4fe7fc4ab`.
No GitHub release was made for this change.

Owner request: support the Logitech PRO X2 RAPID while the Redragon build is in
testing. Tester log `HallJoy Logitech g pro x2.log` (1.6.7): USB product
"PRO X2 RAPID" `046D:C364` (interfaces 00/01/02) present, no analog source.

## Product

PRO X2 RAPID: wired keyboard, hot-swap magnetic (Hall) switches, 8 kHz, OLED
screen and roller, announced 2026-09-23 (Logitech G PLAY 2026). Some press calls
it 75%, but the official specification says "TKL design", 84 keys, 363 mm, and
the official top view shows an ANSI TKL whose third navigation column (Pause,
PgUp, PgDn) is replaced by the OLED and roller: 87 - 3 = 84 keys. The earlier PRO
X TKL RAPID is `046D:C35B`.

## Evidence

- Logitech's public HID++ 2.0 documentation (github.com/Logitech/cpg-docs) does
  not describe analog keys. Solaar lists `0x1B0C` as "analog button tuning"
  (mouse). No vendor firmware or configurator was downloaded.
- RigDeck (github.com/gabriellaines/rigdeck, GPL-3.0), protocol notes for the
  PRO X TKL RAPID (`docs/protocols/logitech-pro-x-tkl-rapid.md`, read
  2026-10-05; local copy `.local/research/logitech-20261005/rigdeck/`), based on
  G HUB USB captures and checks on the device by its author:
  - HID++ on interface 2: report 0x10 (7 bytes) / 0x11 (20 bytes), device 0xFF;
  - undocumented feature `0x1B08` (analog): fn 0 = info, byte 3 = total travel
    in 0.1 mm (`0x28` = 4.0 mm); fn 2 = Rapid Trigger master switch; fn 3 [1/0]
    = live key-depth stream on/off, events = fn-0 notifications
    `[key id][depth 0.1 mm]`, described as **lossy when typing fast**;
  - analog settings are flash files written through `0x8101`; HallJoy does not
    touch them;
  - key ids `00..56` mapped against HID key-downs (ANSI TKL); 0x47 = Fn.
  Only protocol facts are used; no RigDeck code was copied (license).

## Implementation (`logitech_rapid_backend.cpp`, protocol 30)

- Exact admission: `046D:C35B` or `046D:C364`, vendor-page HID++ long
  collection (20/20 bytes); the short collection (7 bytes) of the same
  interface is also read because events may arrive as short reports.
- Read-only discovery: root `getFeature(0x1B08)`, feature list via `0x0001`
  (logged once: `logitech.feature`), fn 0 info (`logitech.analog_info`), travel
  plausibility 1.0..8.0 mm. Without `0x1B08` the keyboard is present but not
  connected (`logitech.no_analog_feature`).
- Live: stream on (fn 3 [1]) after routing claim; re-armed every 2 s (G HUB may
  switch it off; our software id 0x0B separates replies); switched off (fn 3
  [0]) at pause/exit/disconnect. Shared handles (G HUB may hold the interface).
- Depth: `depth / travel` -> permille, 0.1 mm steps; key id -> HID usage table
  `logitech_rapid_protocol.h` (87 TKL keys; Fn = HallJoy Fn). Unknown key ids
  are logged once each (`logitech.unmapped_key`), extra payload bytes once
  (`logitech.extra_payload`).
- Notice: yellow group `LogitechRapid` (flag 134217728, protocol 30), title
  "Logitech G RAPID: hardware testing incomplete".
- Sheet: Logitech G / PRO X TKL RAPID Not investigated -> Implemented; inserted
  PRO X2 RAPID Implemented (rows 488/489, read back: only these rows changed;
  structure PASS 150 blocks, notices PASS 285, supported 72). Snapshots in
  `.local/research/logitech-sheet-20261005/`.
- Layouts (automatic selection by the verified session): "Logitech G PRO X TKL
  RAPID ANSI" (87) and "Logitech G PRO X2 RAPID ANSI" (84) from
  `tools/build_logitech_layouts.py` (standard ANSI TKL; the X2 without Pause/PgUp/
  PgDn per the official image, kept local, SHA-256 `85114262...`); every key is
  asserted to exist in the analog key-id table.

## Behaviour review (FIRMWARE_BEHAVIOR_REVIEW)

| Finding | State |
| --- | --- |
| Typing coexistence | Restricted-unknown: the stream is HID++ notifications; normal key reports continue (RigDeck measured key-downs while streaming). Not verified on C364. |
| Range / resolution | Restricted: 0.1 mm steps (40 levels on 4.0 mm). |
| Shallow input | First step 0.1 mm. |
| Release / packet loss | **Unknown, risk**: the source reports dropped events when typing fast (keys left half pressed). Its reader was a Python loop with 50 ms polling; HallJoy reads both collections continuously with 512 input buffers, which may avoid host-side loss, but device-side loss is not excluded. |
| Mapping | TKL ids established by the source; C364 is the same TKL minus 3 keys, so the same ids are expected (not verified): unknown ids are logged. |
| C364 feature | Unknown: `0x1B08` presence/version verified at run time and logged. |
| Persistent settings | No flash writes; the stream switch is assumed volatile (not established). |
| Normal exit / crash | Stream switched off on exit; after a crash it may stay on (only extra HID++ notifications). |
| Configurator contention | G HUB may switch the stream off; re-armed every 2 s. |

## Validation

- `logitech_rapid_protocol_test`: models, 87 unique ids, request bytes, long
  and short replies, HID++ errors, foreign replies, events, travel, scaling.
- `build_release.ps1` (full catalog includes protocol 30, layout catalog and
  yellow audit with the two presets), diagnostics gate and native suite PASS.
  EXE SHA-256 `17b3f4b5a6e6890aa49011feb22ec65de8c07a959f7d899c0c6ddb6e31e7c4bb`.
- No physical test. Next log tells: feature list, `0x1B08` info, unmapped key
  ids, event format and whether keys stick.

## Firmware search (2026-10-05)

- No firmware image was obtained. Logitech has no standalone updater for the
  PRO X2 RAPID; firmware is delivered only through G HUB. The locally installed
  G HUB manifest (2025.9, December 2025) predates the PRO X2 RAPID and lists no
  RAPID firmware component; the server manifest path was not found.
- Strings of the locally installed G HUB agent (2025.9) name the feature:
  `Feature1B08AnalogKeys` (`feature_1b08_analog_keys.cpp`) with key-travel event
  state (on/off), rapid-trigger and key-priority state, and a key-travel change
  event handler; also `Feature1B20SwitchSwapability` (hot-swap switch types) and
  `Feature1B30DeviceMode`. This confirms that 0x1B08 is Logitech's "Analog Keys"
  feature and that the stream used by HallJoy is its key-travel events. Exact
  function numbers and units still rest on the RigDeck notes and the tester log.

## Tester log "HallJoy v2" (2026-10-05 19:33 UTC, build 17b3f4b5)

- `046D:C364` found, both HID++ collections open (`logitech.model` detail 2).
- 32 features: 0x0001 v2, 0x0003 v7, 0x0005 v5, 0x00D0 v3, 0x0011, 0x8071 v4,
  0x8081, 0x8040, **0x8102 v2, 0x8103 v1** (instead of 0x8101), 0x1B10,
  0x1B05 v1, **0x1B08 v2** (index 13), 0x8051, 0x4540 v1, 0x4523 v1, 0x00C3 v1,
  0x8061, plus hidden/engineering features (0x1E00, 0x1E02, 0x1602, 0x1801,
  0x1807 v4, 0x1805, 0x1E81, 0x920C, 0x1EB0, 0x18A1, 0x1802, 0x18B0 v2, 0x92E1,
  0x180B).
- 0x1B08 v2 info starts `02 05 80 01` (v0 on the TKL: `01 05 80 28`): byte 3 is
  no longer the travel in 0.1 mm; the old build logged only 4 bytes, rejected the
  travel and did not connect (correct, nothing was published).
- Change: format by feature version. v2: travel as 16-bit big-endian in
  0.01 mm at bytes 3..4 if within 3.00..6.00 mm (the spec's 0.05 mm actuation
  steps), with 16-bit depths; every depth must stay within travel + 10%, else
  the session stops (`logitech.depth_format_mismatch`). Unknown format: nothing
  is published; once per process the stream is watched for 30 s and the event
  shape is logged. The full 16-byte info reply is logged
  (`logitech.analog_info` value = bytes 0..7, detail = bytes 8..15), and
  `logitech.event_shape` = events << 32 | nonzero payload byte mask << 16 |
  max byte 5 << 8 | max byte 6 (aggregate, no key ids).
- Validation: protocol test covers both formats, the real "02 05 80 01 00"
  reply staying unknown, implausible values and depth checks; build, native
  suite and diagnostics gate PASS. EXE SHA-256
  `422776fa0eed6406416a7aeec7dcb12570928fc459577f5aae5f69603d5e6ed5`.

## Tester log "HallJoy v3" (2026-10-05 19:48 UTC): old build, no new data

- Same machine session as "v2" (continuous uptime) and still the previous build:
  it emits `logitech.travel_implausible`, an event that build `422776fa` no
  longer contains (verified in the EXE). Info reply unchanged (`02 05 80 01`),
  146..152 failed attempts, nothing published.
- No code change. The tester must run EXE `422776fa...6ed5`; that log should
  show the full `logitech.analog_info` (detail = bytes 8..15) and either
  `logitech.stream_on` or `logitech.format_unknown` + `logitech.event_shape`.

## Tester log "HallJoy Logitech v3" (2026-10-06, build 422776fa)

- The new build ran: `logitech.analog_info` = `02 05 80 01 90 05 00 00`, bytes
  8..15 zero; `logitech.stream_on` value 400 (4.00 mm, 16-bit reading) - the
  v2 travel reading is confirmed by the specification (4.0 mm, 0.05 mm steps).
- 24 s after stream on, the first and only analog notification (function 0,
  long report 0x11) failed the depth check: `logitech.depth_format_mismatch`
  (version 2, travel 400), session stopped, nothing published (correct).
- `logitech.event_shape` = 1 event, nonzero payload bytes 1,3,4,7,10,13
  (report bytes 5,7,8,11,14,17), byte 5 = 0x2d, byte 6 = 0. A single
  `[key][depth16]` record cannot produce this (depth 0x2d00). The layout
  looks like several 3-byte records (first bytes 1,4,7,10,13 nonzero), but one
  aggregate cannot tell record boundaries, byte order or what payload byte 0
  means. The latest RigDeck (v0.8.0, 2026-10-06) still covers only the TKL
  RAPID; no other public description of 0x1B08 v2 was found.
- Change: a feature version whose event layout is not established
  (`AnalogFormat::eventsVerified`, only version 0 so far) publishes nothing
  and captures the raw stream for the log instead.
- OWNER (same day): no time limits and no instructions for the user; it must
  be enough to open HallJoy, press any keys for 5-10 s and send the log.
  Capture design (bounded by content only):
  - The stream stays on for the whole session (re-armed every 2 s as a
    protocol keep-alive against G HUB), on every reconnect, until the capture
    store is full.
  - Every report of the HID++ collections except our own replies (the first 4
    are kept) is stored as `capture.logitech n=<bytes> <hex>`; an exact repeat
    of the previous report only counts (`capture.logitech repeat=N`).
  - Physical reference: while capturing, every key's first two presses (down
    and up, any keyboard, keyboard id hashed) are stored as `capture.key
    hid=.. down=.. keyboard=..` (`InputTrace_SetKeyCapture`), so the raw ids
    can be matched to keys without asking the user what was pressed.
  - Records go to a new store in the support log (`SupportLog_Capture`): stored
    at once with their own `uptime_ms`, no timed queue, no per-second budget,
    never evicted; the first 8000 records are kept and every written report
    (Open log, banner, continuous logging) contains the whole store. When the
    store is full the stream is switched off (`logitech.raw_capture` = records,
    detail 1 = full) and the status asks to Open log.
  - The status text while capturing: "recording analog data for the log: press
    any keys, then Open log".
  - Privacy: during the capture the log contains Logitech key ids of pressed
    keys and the first presses of each key (owner rule 2026-10-06).
- Validation: protocol test asserts v0 verified / v2 unverified and that the
  observed v2 event is no single record; `support_log_windows` test covers the
  capture store (invalid prefix rejected, exactly 8000 records kept, overflow
  refused, first and last record in the written report);
  `research_reference_checks.py --record` re-run (the notice catalog had
  changed earlier on 2026-10-06; all original private audits passed);
  `run_native_backend_checks.py --require-compiler` PASS; `build_release.ps1`
  EXIT=0 (all gates, diagnostics release gate, layout catalog, yellow audit
  286, supported layouts 72). No physical test.
  EXE SHA-256 `132d74c255ed614a916432580f43169003dd9e003ebdeffa50f21c0064271b3e`.

## Tester logs "Logitech v4" and "v4 + GHUB" (2026-10-06, build 132d74c2): v2 decoded

The content-bounded capture worked: 4318 and 1735 raw reports plus 167/108
`capture.key` records (the second log with G HUB running; no difference in
the stream). Offline analysis (scripts not kept in the repository):

- Layout: function-0 long reports `11 ff 0d 00 [more] 5 x [key id][depth hi]
  [depth lo]`, key id `ff` = empty slot (empties trail, zero bytes), depth in
  0.01 mm, 0..400 (= info travel 4.00 mm; never 0 inside a slot). A frame lists
  every key with a non-zero depth; more than 5 keys use reports with `more` = 1
  then a last one with 0 (seen up to 10 keys); an empty last report = no key.
  A released key is absent, so release to zero is guaranteed by the format.
- Loss: a pressed key vanished for exactly one frame 145 times (132 next to
  two-report frames: a lost report). Decoder: a key absent from one frame keeps
  its depth, absent from two it is released (about 1 ms release delay); a key id
  repeated inside one frame drops the partial frame (lost last report).
- Tester settings visible in the data: Rapid Trigger on, actuation as low as
  0.25..0.5 mm. Windows key events therefore line up with direction changes of
  the depth, which was used to match ids to keys.
- Key ids: blocks of 8 per keyboard row; positions 0..3 of each block follow
  physical columns (left 2-W-S-X, 1-Q-A-Z, grave-Tab-Caps-LShift, 3-E-D-C;
  right mirrored 0-P-;-/, 9-O-L-., 8-I-K-comma). 52 ids mapped
  (`kKeyMapV2`): 49 seen pressed (every Windows down/up event of a mapped key
  is consistent with the decoded state: 139 downs, 136 ups, 0 contradictions),
  3 inferred from the column rule (Tab 0x12, ; 0x38, / 0x30). Not yet mapped:
  F2..F12, 5, 6, [ ] \ ' Enter, RShift, right Alt/Fn/Menu/Ctrl, PrtSc, ScrLk,
  Insert, Home, End, arrows. Unmapped ids are logged once
  (`logitech.unmapped_key`).
- Change: version 2 is published (`FormatFromInfo` verified for version 2
  only; other versions still capture). `V2FrameAssembler` + `V2KeyState` in
  `logitech_rapid_protocol.h`; a report outside the layout is skipped (first one
  logged as `logitech.v2_invalid_report`, payload bytes 4..19) and the session
  stops only when such reports outnumber good frames (more than 16). While the
  capture store is not full, the raw capture and `capture.key` keep running
  during normal use, so later logs complete the key table without user steps;
  `logitech.v2_frames` = frames, detail = skipped reports.
- Validation: protocol test with real reports from the capture (single key,
  7-key two-report frame, lost last report, foreign/reply/short reports,
  invalid layouts, one-frame hold, unique v2 map); offline replay of both logs
  through the same rules: 0 invalid reports, 0 contradictions with Windows
  key events. `research_reference_checks.py --record` (the layout check reads
  this header; all original audits passed), `run_native_backend_checks.py
  --require-compiler` PASS, `build_release.ps1` EXIT=0 (all gates). No physical
  test. EXE SHA-256 `7b863a5f88e14d247ec58054ac58d2581957012051e9a65b27bfc108a3bce12b`.

## Tester report and log "v5" (2026-10-07): analog works; run-time key learning

- Tester (Discord, 2026-10-07): analog reaches the gamepad in two games; keys
  not in the table (F2..F12, arrows, some system keys) are not analog. (The
  report that unbound keyboard/mouse input stops when the game takes the
  gamepad is game behaviour, as the owner answered.)
- Log v5 (short session): key 5 (HID 0x22) = id 0x0f, a clean single press.
  6 = 0x0e by elimination (the last number-row id of the left block). Table: 54.
  The log also shows one `logitech.send_failed` (Win32 31) during the first
  feature listing; the worker retried 5 s later and the stream started.
- Owner goal: the next test should not depend on which keys the tester
  happened to press. New `V2KeyLearner` (protocol header): a Windows key-down
  of the Logitech keyboard itself (Raw Input device name with its VID/PID) whose
  HID usage is not in the table is paired with the one unknown id that started
  moving from rest within 400 ms before it (or 60 ms after, for late frames);
  no pairing when two unknown keys qualify or the usage/id is known. Pairs live
  for the session (`Owns` includes them), are logged as `logitech.learned_key`
  (id << 16 | usage) for the static table, and the key is analog from its
  second press. Fn has no HID usage and cannot be learned.
- Offline check: the learner run over logs v4, v4 + GHUB and v5 with every
  key treated as unknown learned 42 pairs, all equal to the confirmed table,
  0 wrong.
- Validation: protocol test `v2_learning` (unique pairing, known usage/id,
  two unknown keys, stale press, Windows-before-frame, expiry, usage 0);
  `build_release.ps1` EXIT=0 (all gates; includes the named-profile layout
  save fix), `research_reference_checks.py --record` (all original audits
  passed), `run_native_backend_checks.py --require-compiler` PASS. No physical
  test. EXE SHA-256 `28e97567aef38142276f81fb35213522418150878b9b0757bcd800df8e09b892`.

## Log "V6" (2026-10-07, build 28e97567): all 84 keys mapped

The tester started the build and pressed every key once. Result:

- 28 `logitech.learned_key` pairs, each a single unknown id at the moment of a
  Windows key-down: F2 0x00, F3 0x03, F7 0x04, F4 0x05, F6 0x06, F5 0x07,
  RCtrl 0x2b, Menu 0x2e, RAlt 0x2f, RShift 0x33, Up 0x35, ' 0x3b, Enter 0x3d,
  [ 0x43, ] 0x44, End 0x46, Home 0x4c, Insert 0x4e, F10 0x50, F9 0x51, F8 0x52,
  F11 0x53, F12 0x54, ScrLk 0x57, Down 0x60, Left 0x61, Right 0x64,
  backslash 0x71.
- From the raw capture: PrtSc 0x55 (Windows reports only the PrtSc key-up,
  which lines up with 0x55 leaving; the learner uses key-downs) and Fn 0x2c
  (no Windows event; a notification of another HID++ feature, `11 ff 0e 00 01`,
  fires with it). Tab 0x12, ; 0x38, / 0x30 (column rule) and 6 0x0e
  (elimination) are now confirmed by single presses.
- 54 + 30 = 84 ids = the 84 keys of the PRO X2 RAPID (the TKL usages minus
  Pause/PgUp/PgDn); Fn maps to HallJoy's Fn code as on the TKL. The ids keep
  the row structure: the F-row left block repeats the number-row order
  (F2 F1 Esc F3 F7 F4 F6 F5 / 2 1 grave 3 7 4 6 5); 0x5_ is the F-row right
  block, 0x6_ holds Left/Down/Right, Up is 0x35 and backslash 0x71.
- `kKeyMapV2` is now static and complete; the learner stays for ids outside
  the table (other layouts). Test: 84 unique ids whose usages equal the TKL
  usages minus Pause/PgUp/PgDn; the learner test now uses ids/usages outside
  the table and checks that a table key is never learned.
- Validation (84-key table): protocol test (84 unique ids, learner outside the
  table), `research_reference_checks.py --record`, `build_release.ps1` EXIT=0
  (all gates), `run_native_backend_checks.py --require-compiler` PASS. Same
  build carries the FUN60 Pro 2304 alias and the layout save fix. No physical
  test. EXE SHA-256 `a47370a0c3f76c6b681ca37578436f254b415cf11826ce5e6615617838a62aa7`.

## PRO X TKL RAPID two-key release report (2026-11-07)

- Discord report from giaun: hold A+W, release W while A stays down; W keeps
  its value until pressed again. Also reported with A+S. At intake only the
  screenshot was available; the HallJoy and firmware versions are unknown.
- C35B v0 updates one key id per report. A/W/S map to separate ids
  (0x23/0x1d/0x22). An accepted W report with zero immediately clears the W
  provider value without touching A. Losing the final W update can preserve
  its last depth indefinitely while A continues reporting. Static holds
  do not require repeated samples. The checked implementation has no
  confirmed readback of a complete keyboard state.
- [RigDeck hardware notes](https://github.com/gabriellaines/rigdeck/blob/main/docs/protocols/logitech-pro-x-tkl-rapid.md)
  report similar stale values under fast typing on U1 70.04 build 0020.
  [Sainan's 2024 capture](https://gist.github.com/Sainan/0f21e6937ee72b7979ec44ee9fde6016)
  shows long packets for feature index 0x0f, function 0, with single-key
  release packets and additional bytes on press. It does not include a
  raw two-key trace or establish the semantics of the extra bytes.
  [Logitech's G HUB notes](https://support.logi.com/hc/en-150/articles/360048967733-G-HUB-Update-Release-Notes)
  say firmware 170.3.18 addressed certain improper key releases, and
  170.4.20 changed travel processing. Neither identifies this tester's failure.
- Test build adds bounded, bound-key-only C35B source traces beside Windows
  key events and native/filtered/output stick values. It also reads main
  firmware metadata via 0x0003 and reports actual HID input queue capacities
  and setup errors. Report and gamepad processing are otherwise unchanged.
  Digital key-up is not used as a sensor value because Rapid Trigger can
  report it while the switch is still physically down.
- Repro: with default WASD bindings, hold A+W, release W fully while A
  remains held for a moment, then release A. Click Open log immediately
  after the failure, before pressing W again. A traced W zero alongside a
  stuck native/filtered/output value localizes the problem downstream of
  receipt. Windows key-up without a traced W zero points upstream of the
  accepted-depth trace (transport, parser, or device); it does not prove
  firmware fault. Check that surrounding source and output lines are
  present before interpreting a missing event.
- Validation: native backend suite and standard release build PASS. Offline
  replay confirmed a valid W zero clears W while keeping A, and withholding
  that update preserves W. The replay used the production decoder with a
  synthetic two-key sequence. The compiled catalog contains 87 keys for the
  ANSI preset; the C35B model identity resolves to that preset. No hardware
  repro yet. The local diagnostic EXE (1.6.15.0) SHA-256 is
  `c370d535c5a35cbe02664bd550177b032c6a139e504f8ef7a01d002459453d0e`.

### Firmware and receive-path investigation (2026-11-07)

- The installed G HUB 2025.9.814156 build 634218 device database, decoded
  and verified against its embedded SHA-256, identifies model
  `pro_x_tkl_rapid`, runtime `046D:C35B`, update mode `046D:ABDB`, and DFU
  depot `0467c4b2-2e4e-4854-b4b2-c8473bf15297`. The official CDN depot
  SHA-256 matches the local vendor manifest
  (`2d7dd0574ae4f676ad1520dab977409b1a833e375d404a7656aa9485a434699c`).
  Its decoded and hash-verified DFU manifest independently names both
  identities and version 170.4.20. The 307,808-byte
  `pro_x_tkl_rapid_v170_4_20.dfu` hash matches both vendor manifests
  (`62dc1fbeb92c7be015e3cb63efe4e145ca2024f51a1589ecf3dc63f1c2126565`).
  Vendor artifacts are local under `.local/research/logitech-20261005/`.
- The DFU wrapper exposes a `U170_D0` header, followed by a body at offset
  0x20 with entropy near 8 bits/byte and no repeated aligned 16-byte
  blocks. Probes of common gzip/DEFLATE/LZMA entry points found no decoded
  image. G HUB agent's DFU packet builder copies ranges from the supplied
  binary to the outgoing buffer (RVA 0x10DA177–0x10DA253). The DFU transfer
  payload has not yielded executable MCU code; the update package by
  itself does not establish which scanner/report branch can drop a release.
- Static comparison against G HUB agent 2025.9.814156, SHA-256
  `4dd78336d58683f64632e34662bf123f95f879766b405b4d8b641c9b2be61cfe`:
  the function-0 report callback (RVA 0x1A969B0) calls the versioned decoder
  (0x1A9DFC0). Version 0 reads exactly payload offsets 0 and 1 as key ID
  and unsigned depth (0x1A9E004–0x1A9E0AD). G HUB batches those events by
  key for UI delivery and clears the batch after publication (0x1A9E2E0).
  The callback's later-version branch (0x1A9E103 onward) reads triples and
  accumulates a frame. No v0 snapshot readback appears in this reviewed
  report path. HallJoy v0 agrees on offsets and applies updates per key.
  G HUB does not interpret the extra v0 bytes as another key or a release map.
- Sainan's pinned capture revision
  [8d7939c](https://gist.github.com/Sainan/0f21e6937ee72b7979ec44ee9fde6016/8d7939cff101aea9f0b665491ea5ed3c8ef0f06e):
  all 78 A packets are long 0x11 with ID 0x23, one explicit final zero,
  and 77 with extra nonzero bytes. The additional single release sample
  is also 0x11. The capture provides no evidence of one key switching
  between short and long reports. Offline replay through the production
  decoder and stick builder, with synthetic W/S press/release events,
  passed both A+W and A+S with all four combinations of Snap Stick and
  Last Key Priority. Delivering zero clears only its axis; withholding
  it leaves the held value. The replay bypasses real USB, bindings/curve
  acquisition, SDK arbitration and the virtual-controller driver.
- A conditional HallJoy receive issue is reproducible with synthetic
  cross-collection events. `Session::Pump` puts the long completion event
  first. Win32 wait-any returns the lowest index if both are signaled.
  A newer long zero can be served before an older short positive for the
  same key; an offline test with real Win32 events and the HallJoy decoder
  ended at 500 permille in that sequence. On the observed long-only stream
  this ordering case does not arise. The worker also waits for its re-arm
  write every 2 s, during which driver queues and already-armed reads
  receive reports but the worker does not drain them. Actual overruns,
  mixed short/long traffic, and any firmware effects of re-arming are
  unverified on the tester's unit.
- The missing-final-update mechanism remains the best match. Available
  evidence separates it from a wrong byte offset or a demonstrated stick
  builder issue, but cannot assign the missing update to firmware or USB/
  host delivery on the reported unit. No runtime or support-status change
  was made in this investigation.

### C35B ordered reads test build (2026-11-07)

- For C35B sessions with both HID++ collections, both overlapped reads use
  one Windows I/O completion queue and one consumer. This replaces the
  lowest-handle-index preference with the order in which Windows queues
  read completions. The just-consumed collection is re-armed immediately
  after the callback. Writes still use their own event, excluded from the
  read queue. HID buffer setup happens before associating either handle.
- On setup failure the original event reader remains available, including
  if one handle was already associated. The support log records
  `logitech.read_mode` and, on fallback, `logitech.iocp_fallback`.
  Session teardown cancels and drains pending reads before freeing storage.
- Windows regression uses two named pipes and the production Session.
  An older short press followed by a newer long release now ends at zero;
  the reverse collection assignment also ends at zero. Tested command
  writes on the long handle, fallback after partial association, single
  collection, disconnect, and cleanup with an outstanding read.
- This corrects the handle-index ordering case. I/O completion order
  does not guarantee ordering of reports that were already independently
  buffered on the two collections before reads were posted. A device that
  never emits the final depth cannot be recovered by reordering, and the
  observed C35B failure is still awaiting an on-device confirmation.
  The change adds no polling delay or fabricated release value.
- Final validation: Win32 reader regression, native suite, standard release
  build, profile/recovery and compiled catalog checks passed. The 87-key
  ANSI preset is present and resolves from the C35B session identity.
  Local test EXE SHA-256 `12b85743665870328de413e7e6879fe9d89e97cfbd41259fe0fd25f3f5b3ede9`.

### Log 35: release missing from the analog stream with ordered reads (2026-11-07)

- Report from the C35B tester, `HallJoy (35).log`, SHA-256
  `61c599f0ba162177d2dd3d1c749d24eaed67b9b656ed13698a3192f29f0dcefd`.
  The report timestamp is 14:48:39 UTC. `logitech.read_mode=2` confirms
  the ordered-read build is in use and both Windows HID queues report
  512 slots. Firmware metadata `70 04 00 20` matches the 170.4.20 release.
  Feature 0x1B08 is v0 at index 0x0f, with total travel 4.0 mm.
- Separate W/A/S/D presses all produced explicit depth zero and neutral
  gamepad output. In the combined test A reached 4.0 mm first. W then had
  just one depth update at 95188546 ms (4.0 mm). Windows delivered both
  key-ups at 95189656 ms, on the same keyboard handle hash. A reached
  zero 15 ms later. No W depth update followed until the session stopped.
  The final affected pad trace shows A `0/0/n0`, W `1000/929/n1000`,
  X=0, Y=30441, so the output reflects the stale native W measurement.
- All 340 bound depth changes were on long report 0x11. Session totals
  show 341 accepted depth events, with only the key ID and depth bytes
  ever nonzero. The source plus Windows key-event trace peaks at 78
  lines/s overall and 51 lines/s during release, below the 300 lines/s
  budget. No re-arm or stream failure occurred. The sole failure was an
  optional feature-list timeout before analog startup. This reproduction
  does not fit a short/long ordering tie or a dropped release trace.
- The pattern is consistent with [Sainan's independent observation](https://github.com/AnalogSense/universal-analog-plugin/issues/1)
  of a single selected key in this model's stream. The log establishes
  absent independent W depth through this release, not the precise
  selection rule or whether every firmware/profile behaves the same.
  The reviewed v0 source has no confirmed per-key depth readback.
  A Windows key-up cannot substitute for physical depth, as even the
  standalone W press had a 125 ms interval from key-up to its real
  analog zero. The owner requires simultaneous depth and release for
  the full keyboard. See the current disposition at the top.

### Alternative sources for full-key analog (2026-11-07)

- **G HUB and HID++ 0x1B08.** The installed G HUB agent's feature vtable
  at RVA 0x327AD98 and the corresponding request builders expose function
  0 for capability info (0x1A95B90), 1/2 for Rapid Trigger read/write
  (0x1A95DC0/0x1A95FF0), 3 for the key-travel event switch (0x1A96740),
  and 4/5 for Key Priority read/write (0x1A96260/0x1A964B0). Both Key
  Priority operations reject feature versions below 1 in this agent.
  Function 3 constructs a one-byte payload with only bit 0 set or
  cleared. The G HUB UI subscription receives the same function-0
  events, via the versioned decoder above. There is no separate
  live-depth poll in the reviewed implementation of this feature.
- **Other advertised HID++ features.** The [31-entry enumeration on
  stock 170.4.20](https://raw.githubusercontent.com/gabriellaines/rigdeck/main/docs/protocols/logitech-pro-x-tkl-rapid.md)
  lacks 0x1F20 and 0x1B0C. The documented 0x8101 reads return stored
  profiles and thresholds. 0x1B05 is key customization and 0x1B10
  is the control list. The G HUB agent exposes 0x0011 Property Access
  as get info for a specific property ID, select property and read
  chunks (RVAs 0x19AA9C0, 0x19AAB90, 0x19AAD00). No C35B depth
  property ID or live usage was established. The hidden 0x1801 is
  Manufacturing Mode. Its generic wrapper forwards a caller-supplied
  operation and payload (0x19CBDB0) and handles test responses. It does
  not identify which tests C35B implements. No matrix or depth opcode
  is known for this board, and no manufacturing operations were sent.
- **USB and firmware.** The tester's report identifies USB interfaces
  00, 01 and 02; the independent hardware investigation identifies 02
  as the HID++ transport and also describes loss of depth updates.
  A full bus trace is still needed to separate keyboard transmit
  behavior from the host path and check for a second live report on
  any other interface. No C35B is connected locally. The official
  170.4.20 inner update image remains opaque after extraction. For a
  format control, the verified PRO X 60 v22.1.15 depot is plain but
  its inner DFU also has a short clear product header followed by a
  7.999-bit/byte body. C35B measures 7.9994 bits/byte, with no repeated
  aligned blocks in the analyzed body. This supports treating depot
  decryption and DFU decoding as separate steps. No executable C35B
  payload or confirmed read-back method was obtained. C364's v2
  frames belong to a different PID and provide no basis for flashing
  its firmware to C35B.

The remaining evidence to pursue is a passive capture of all C35B USB
interfaces during the G HUB depth display, an identified read-only sensor
command, or the executable C35B firmware. Any candidate requires independent
simultaneous depth and release, including sustained overlapping key presses,
before it can qualify for support.

### C35B DFU structure and silicon identification (2026-11-07)

The public Windows [GHUB10](https://updates.ghub.logitechg.com/pipeline/v2/update/ghub10/win/public/details.json),
[GHUB12](https://updates.ghub.logitechg.com/pipeline/v2/update/ghub12/win/public/details.json),
[GHUB13](https://updates.ghub.logitechg.com/pipeline/v2/update/ghub13/win/public/details.json)
and [GHUB14](https://updates.ghub.logitechg.com/pipeline/v2/update/ghub14/win/public/details.json)
manifests returned different CDN paths for the same C35B depot, size 314418
and SHA-256 `2d7dd0574ae4f676ad1520dab977409b1a833e375d404a7656aa9485a434699c`.
GHUB13 reported 2026.7.1011216 and GHUB14 reported 2026.8.1008724.
The local online installers 2025.4.719084 and 2025.5.730277 did not provide a
second C35B image. The archive lookup returned no pre-170.4.20 snapshot of
the public GHUB10 details endpoint. Earlier release notes name 170.3.18
and 170.2.16, but no corresponding executable image was recovered.

| DFU file offset | Observed field |
| --- | --- |
| 0x00–0x08 | `01 03 U170_D0` record and model tag |
| 0x11–0x14 | Big-endian load-address field `0x00010000` |
| 0x15–0x18 | Big-endian image size `0x0004B108` (307464 bytes) |
| 0x20–0x4B13F | 307488 bytes. Body length equals the declared size rounded up to 16 bytes, plus one 16-byte block. |
| 0x4B140–0x4B24F | 16-byte record starting with `02` followed by a 256-byte opaque field, consistent with a signature record |
| 0x4B250–0x4B25F | Terminal record `03` and fifteen zero bytes |

- The structure is shared by the [official MPK01/03/04 samples](https://github.com/Logitech/fw_updates/tree/1ddde61310573ecea54c4204b401393a90f4bbae)
  and multiple hash-verified G HUB depots. Control payloads with second
  byte `01` (MPK01/03/04, G915, G915 TKL, G502 HERO and PRO X Wireless)
  contain ARM vectors at file offset 0x20 and only alignment padding
  after the declared data. The `03` samples (C35B, G515 TKL, G515
  LIGHTSPEED TKL and PRO X 60) have the extra 16-byte block and opaque
  bodies. The C35B body has entropy 7.999361 bits/byte, 19218 distinct
  aligned 16-byte blocks and zero aligned initial SP/Thumb reset-PC
  pairs for nRF52833 SRAM and the declared application range. The
  evidence is consistent with a separately encrypted device image.
  The cipher, extra block position/role and signature coverage remain
  unverified. A bounded test of CBC/CTR/CFB/OFB with known local depot
  keys and five candidate IVs yielded zero plausible vector pairs in
  2960 probes. Those keys belong to the outer package and the negative
  test does not establish the inner cipher or exhaust possible keys.
- Ran the installed G HUB agent x64 packet-copy branch, RVAs
  0x10DA177–0x10DA25B, in Unicorn with the exact DFU. The local script
  `.local/research/logitech-20261005/check_c35b_dfu.py` pins both file
  hashes, models the binary map, packet offset and allocator, and
  stops before the caller and transport. All 19238 modeled 16-byte
  packet payloads reconstruct the input byte-for-byte. No loader, HID
  transport, target CPU or sensor behavior is modeled in this check.
- The [published teardown](https://www.weistang.com/article-25219-4.html)
  and its [republished close-up](https://q9.itc.cn/images01/20241206/9e3cec328f8741dfa3aabad4b97d6dbe.jpeg)
  identify an nRF52833. The pictured chip reads `N52833 QIAAB0`, a
  Rev. 2 aQFN73 Cortex-M4F with 512 KiB flash and 128 KiB SRAM.
  The board also has a GTRY52 at U5 and GTRY12 under each switch. The
  close-ups show pads labeled GND, SWDIO and CLK near U5 and signal
  pads labeled SCLK, MOSI and NCS nearby. These markings provide
  hardware leads, but the SWD pad target, inter-chip link topology,
  voltage and packet semantics need continuity/scope verification on
  the physical PCB. The photographed board revision is separate
  evidence from the tester's USB PID.
- On an available C35B board, verify SWD connectivity to the nRF52833
  (aQFN73 SWDCLK AA24 and SWDIO AC24) and check CTRL-AP.APPROTECTSTATUS.
  If AHB-AP is open, read and hash the 0x80000-byte flash twice without
  writes. Verify the Thumb vectors at the declared application
  address 0x10000 and identify the active firmware version before
  tracing the bridge receive and HID++ 0x1B08 report-selection paths.
  [Nordic Rev. 2 documentation](https://docs.nordicsemi.com/r/bundle/errata_nrf52833_rev2/page/err/nrf52833/rev2/latest/config_833_249.html)
  says APPROTECT defaults to enabled. A locked target stops the
  standard read-only SWD route. ERASEALL/recover destroys the stock
  image. A passive bridge-bus capture on a lab unit remains a
  separate way to determine what per-key measurements the MCU gets.
  No C35B or debug adapter was present locally. No plaintext
  C35B image or scanner/report handler was recovered in this pass.

### Offline DFU follow-up and newer G HUB agent (2026-11-07)

- Enumerated public G HUB Windows channels 7–10 and 12–14 and the macOS
  channel 10. The older Windows builds exposed by channels 7–9 date
  to 2021–2023. The Windows and macOS manifests inspected for channels
  10 and 12–14 identify the same exact C35B depot. Scoped the content
  sweep to named DFU depots and UUID-named depots between 2000 and
  1500000 bytes, deduplicated by published SHA-256. The sweep verified
  all 424 downloaded hashes, decoded 308 packages and identified
  firmware records in 153. Of those readable packages, only the known
  170.4.20 depot named C35B/ABDB or contained its image. 116 packages
  could not be decoded with the local public-channel key set. The
  historical G HUB 10 manifest and exact depot archive queries did
  not yield a dated old copy. The private inventory is
  `.local/research/logitech-20261005/dfu-cdn-inventory-20261107.json`,
  SHA-256 `4bed2b7d8f809b0c31d2122c7bbb00758f485b4959ef368a73f5fcb0f8b020b9`.
- The [public G305 SWD dump](https://github.com/you-wouldnt-reverse-engineer-a-rat/g305-re/tree/dac3bba486f7b113343641bf4524c949ee96fd7c/flash_dump)
  is a genuine plaintext control with boot vectors at 0x00000 and
  separate application vectors at 0x11000. The verified B0011 dump
  SHA-256 is `223df03547ca37ecac50bc8478c23f6833ae17d57ca9903a6655aa52098d05ac`.
  Its bootloader header handler at 0x30FC tests the byte at offset 1
  before checking the ten-byte model tag. Executed the handler with
  a mocked FICR page-size register and RAM. Mode 0x01 with its own tag
  returns 0x03. Mode 0x03 with either tag returns 0x93. Mode 0x01 with
  the C35B tag returns 0x94. The [fwupd HID++ DFU implementation](https://github.com/fwupd/fwupd/blob/main/plugins/logitech-hidpp/fu-logitech-hidpp-device.c)
  masks bit 7 and names 0x13 unsupported encryption mode. This fixes
  the role of the header field. The comparison bootloader has no
  implementation of mode 0x03. Its RSA-2048 public key at 0x63A0 was
  confirmed by the exponent 65537 and modular-exponentiation call at
  0x304. It did not validate the expected PKCS#1 padding for the
  C35B, MPK01 or G502 X Plus trailer in either byte order.
- The [public G502 X Plus support rollback mirror](https://github.com/BrightNDark/G502-x-Plus-Firmware-Revert/tree/dbc757bb98de14570c32999b929a94847902cb40)
  embeds an opaque mode-0x03 `MPM27_D1` record, SHA-256
  `8971137a73de96e018c3463541e917385567745605c29b63eb08f2af31762f29`.
  This is an application update for another PID. In the decoded
  CDN corpus, no tag appeared in both mode 0x01 and mode 0x03. A
  bounded comparison of 19 distinct mode-0x03 bodies found zero
  shared aligned 16-byte blocks between C35B and the other bodies
  or between different versions with the same tag. This does not
  identify the cipher, mode, IV or per-product key policy.
- Extracted the exact G HUB 2026.8.1008724 agent from the official
  GHUB14 core depot without installing or running it. Windows
  verified its Logitech Authenticode signature. Agent SHA-256
  `ea6007a69b6f22b2b7d790f34ced39152c3624b7ca9a340deb3f55dbbf2f7d27`.
  Its DFU copy branch (RVAs 0x22A1DC0–0x22A1E9B) also reproduces
  the entire C35B image across 19238 modeled payloads. The updated
  `check_c35b_dfu.py` replays both agents and the four G305 header
  cases. Script SHA-256
  `d744ef0a26bdc0b310bf11822097340826b4a497f811f9c02687cabd00528eb1`.
- The 2026.8 agent still exposes 0x1B08 outgoing functions 0–5 in its
  interface vtable at 0x4199878. Function 3 at RVA 0x20202F0
  enables/disables depth events with one bit. Incoming event 0 uses
  the depth decoder at 0x2028390, while events 1 and 2 are Rapid
  Trigger and Key Priority state changes. The v0 decoder now clears
  the depth map (call at 0x20283E8, clear at 0x178E880) before
  inserting the one key and depth at 0x20283F1/0x20283F4. The
  v0 callback therefore cannot supply a live map of simultaneous
  depths. This is host-side evidence and does not prove how the
  keyboard chooses the key reported in a packet.
- Verified the GHUB14 core device registry record from the official
  depot at offset 0x14DAC676. The decoded record is byte-identical to
  the installed 2025.9 registry record (SHA-256
  `9c3a91c4bcbb42771ab8abdfd9aa732963b875d8492d79bca45934c569803065`).
  It points to the single known C35B DFU depot and contains no
  separate recovery/engineering firmware reference. The
  analog onboarding reference is in `assetsPackages`.

No public C35B flash dump, bootloader image, old exact DFU or mode-0x03
decryption routine was recovered in this offline pass. A pre-170.4.20
G HUB cache or exact Logitech recovery/depot archive remains a useful
lead. If it contains another mode-0x03 image, comparison can narrow the
format but still needs the matching loader or a plaintext image to
expose the scanner and its HID++ dispatcher. The existing 0x8101
readback is evidenced for profile storage. No verified read-only
command returning C35B application flash or live full-key depth was
found in the reviewed interfaces.

### Historical G HUB manifests and alternate host implementations (2026-11-07)

- Logitech lists the PRO X TKL RAPID updates `170.2.16` with G HUB
  2025.2 in March and `170.3.18` with 2025.5 in July. The latter
  describes a switch-processing release fix, with no claim about
  changes to HID++ depth event coverage.
  [Logitech release notes](https://marketplace.logi.com/releasenotes/ghub/en)
- The [April 6 Triage report](https://tria.ge/250406-s9qenavmy2/behavioral1)
  lists installed depot build `489832`, four snapshots of
  `C:\ProgramData\LGHUB\current.json` and two of `next.json`.
  The first current snapshot has SHA-256
  `3525f2398c5c7acb37f022617c1779dd1ceb7dfb8d702dbc830d1956cc6c7245`.
  The public report and hashes are visible in the web index. Direct
  downloads of the first and last current snapshots and both next
  snapshots returned HTTP 403. The documented API download returned
  HTTP 403 as well. This establishes that an April catalog was
  captured. Its contents and C35B depot URL are still unverified.
  The public [June 19 installer report](https://any.run/report/832a29c9444dd0407f5cb311fa889e77b553e214489ab4b961c662baabc04658/a62b4a38-8b04-4380-8a7c-7a2656cf17d8)
  did not expose an old catalog or a C35B depot URL in the visible
  report.
- The local `C:\Program Files\LGHUB.old` updater reports
  `2025.8.789376` (October 2025). That backup has no matching DFU
  filename or exact image marker in the scanned binaries and
  resources. The available local depot manifest is the later
  `2025.9.814156` build `634218`. Of 34 local cache objects, 33
  match entries in that manifest. The remaining December object
  `a4d5d573916247c116d2ce7f9bc467582103682927c2a2dfc86cd7c1403e049d`
  matches its filename hash. Its decrypted, SHA-256-verified file
  table lists PRO X2 wireless headset assets, with no DFU.
- The official [versioned macOS installer](https://download01.logi.com/web/ftp/pub/techsupport/gaming/lghub_installer_2025.8.789376.zip)
  was examined in memory. Its archive SHA-256 is
  `371d935c3af6d8a91554e57494d23cb7fd330aa15cd9f8c597d9855b175014ec`.
  The Info.plist confirms `2025.8.789376`. Its 67 regular application
  members contain no depot, DFU or dated update catalog. The native
  installer names `ghub10`, the official update host and
  `details.json`. The live macOS public `ghub10` endpoint returned
  build `625362` (2025.9.807501) with a byte-identical copy of the
  known 170.4.20 depot under a different CDN path.
- [OpenMouse keyboard-protocol](https://github.com/OpenMouse-Project/keyboard-protocol/blob/3f9158fe83d7d04ae0b86e4ff26442359ec177c7/docs/logitech-support.md)
  also inspected the G HUB 2026.10 agent and lists the same six
  `0x1B08` functions. Its C35B catalog entry is unverified. Its
  [driver](https://github.com/OpenMouse-Project/keyboard-protocol/blob/3f9158fe83d7d04ae0b86e4ff26442359ec177c7/src/drivers/logitech/hid.ts#L642-L685)
  uses function 3 to toggle the existing key-travel stream. The
  `0x8102` rows documented there are profile tags with stored
  actuation and Rapid Trigger settings. Neither path supplies a
  second live depth source for C35B in that implementation.
  [Logitech's web beta](https://www.logitechg.com/zh-cn/software/ghubweb)
  links to an application that returned HTTP 403 from this
  environment. Its device behavior was not inspected.

The archive search has two concrete targets. An April `489832`
catalog can identify an earlier depot if it contains the C35B
record. A G HUB catalog from July 11 through December 2, 2025 is
the target for `170.3.18`. An exact C35B plaintext flash dump
or mode-0x03 bootloader would expose a different part of the
problem. No earlier C35B DFU, plaintext dump or alternative
full-key depth transport was recovered in this pass.



