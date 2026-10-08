# Logitech G RAPID (PRO X TKL RAPID, PRO X2 RAPID): experimental support (2026-10-05)

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
