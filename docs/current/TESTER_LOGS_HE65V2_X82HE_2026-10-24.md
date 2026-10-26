# Tester logs: EPOMAKER HE65 V2 TMR and ATTACK SHARK X82HE — 2026-10-24

Two user logs, both HallJoy 1.6.11.0 (before 1.6.12). Read as data. Both fixes implemented the same day (owner); see the last section.

## "HallJoy (16).log" — EPOMAKER HE65 V2 TMR (user's model)

- USB `3151:5030`, product string "HE65 V2" (wired); its 2.4G receiver `3151:5038` is also present.
- The keyboard answers the 0x8F board query with board **3417** on both backends:
  `shark.identity_pair_rejected value=3417 detail=20528` and `rongyuan.identity_pair_rejected value=3417
  detail=0x31515030`. Neither admits it: no analog source (`native_sources=0`, rongyuan-stream
  `present_not_connected`), no bindings in use.
- Official catalog (gearhub `index.ad43be22.js`, pinned in `docs/research/epomaker-layouts/sources/`):
  `{id:3417,vid:12625,pid:20528,keyLayout:Common67_beat65,name:"ry5088_MLCY_EPOMAKERHE65Pro",displayName:"HE65 V2",
  magnetism:!0}`, switch options include magnetic types and "机械轴" (mechanical). So the device tuple equals the
  catalog tuple exactly; no alias is needed. keyLayout is the same as HE65 Mag (2376, already in the stream table).
- Why it was never admitted: `catalog-review-20260924.json` holds 3417 as `held_technical`
  ("mechanical_switch_option_review"), and RONGYUAN_BATCH_5 notes it must not become HE65 V2 TMR by a name match.
  This log supplies the device evidence: the user's HE65 V2 TMR answers board 3417 on 3151:5030.
- To admit: add 3417 with `tools/rongyuan_batch.py` (matrix chunk `ca88c7a8.js`, SHA256 9699d1ba..., parent
  7a5b12c9.js; present in the pinned `.local/research-expansion-20260924/WOMIER-3.2.15-technical-js.zip`, matrix
  SHA256 21708b46... per `.local/inventory-gamakay-20261023.json`, so no download is needed), layout from gearhub
  Common67_beat65, then the usual gates.

## "HallJoy (15).log" — ATTACK SHARK X82HE (user's model)

- USB `3151:5029`, product string "Gaming Keyboard". The ATTACK SHARK backend gets two equal 0x8F replies with
  board **3086** and rejects the pair: `shark.identity_pair_rejected value=3086 detail=20521` (38 times),
  `shark.identity_rejected`, then `identity_waiting`. The RongYuan backend also rejects 3086 (not its board).
- Cause: `sharkplay::Profiles` has X82HE 3086 only on PID 0x5030 (official catalog tuple, also sharkfin BOARDS.md
  `3086 | 3151:5030`); `sharkdiag::Known()` requires the exact PID. Same case as R68 HE (board 3650 on 5029,
  ATTACK_SHARK_R68_PRELOG_2026-09-24.md), fixed by admitting that observed pair only.
- Fix: admit `3086` on `0x5029` in `Known()` next to 3650, keep 0x5030, extend the
  component test. X82HE already has a layout (`ATTACK SHARK X82 HE ANSI`) and a yellow listing.
- The user bound W (HID 0x1A) to RT: only OS key events (`trace.os_key`), no analog, as expected while rejected.

## Communication warning in both logs

Both logs toggle `keyboard.communication_warning` (HE65: 3 events, X82HE: 5). Source: every ATTACK SHARK pass
logs `shark.exclusive_open_failed error=32` (another handle without sharing holds the vendor collection; the
RongYuan backend holds none while it waits), followed by an identity rejection, which counts as an access anomaly
(source 17). The shared open still worked (identity replies arrived). Likely the vendor configurator was running;
not provable from the log. After HE65 V2 is admitted on the stream, 1.6.12's stream-owned rule clears it; for
X82HE (owned by the ATTACK SHARK backend) a sharing violation would still raise the warning by design.

## Implemented (owner, 2026-10-24)

**X82HE.** `sharkdiag::Known()` (attackshark_pro_diagnostic_model.h) admits board 3086 also on PID 0x5029, next to
R68 HE 3650; the vendor-catalog PID 0x5030 is kept. Component test: `Known(3086,0x5030)`, `Known(3086,0x5029)`, not
on 0x502D/0x502F. Layout "ATTACK SHARK X82 HE ANSI" and yellow notice already existed. Stays yellow until a log
shows analog.

**HE65 V2 (board 3417).** `tools/rongyuan_batch.py` gained a reviewed hold clearance: a manifest entry may clear
only `mechanical_switch_option_review` (REVIEWABLE_HOLDS), only when the hold is present, and only with a written
`hold_review`; every other hold stays absolute. New test `test_mechanical_hold_cleared_only_by_written_review`
(12 tests OK). Manifest `.local/reviewed-epomaker-he65v2-20261024.json` (EPOMAKER / HE65 V2, 4000 um provisional
like HE65 Mag, same keyLayout; hold review: the user's unit is the magnetic TMR edition), prepare + guarded apply
(backup `.local/backups/he65v2-x82he-20261024/before-apply.zip`): profiles (259 revisions), pinned `ca88c7a8.js`,
admission record, source lock, `rongyuan_stream_protocol.h` row, regression count, notice catalog, README
experimental list, SUPPORTED_HARDWARE, RELEASE_NOTES_NEXT. Exact tuple 3151:5030, no alias.

Layout: `build_rongyuan_layouts.py` groups 3417 with HE65 Mag (identical SVG geometry and factory HID set):
`ry_layout_2376` is now "EPOMAKER HE65 Mag + HE65 V2 ANSI" (66 keys, RY1B-2376/3417). 40 RongYuan/EPOMAKER layout
reports re-pinned to the new profiles/admission hashes (field replace only), `ry_layout_2376.json` and
`inventory.json` regenerated with `--acquire` (inventory also corrects the stale 3613 hold reason to "existing
exact automatic preset"), layout catalog hashes + HE65 entry updated, `layout_pipeline.py integrate` (44 files),
`research_reference_checks.py --record` PASS. Backups in `.local/backups/he65v2-x82he-20261024/`.

Sheet: fresh native snapshot (`.local/sheet-native-before-he65v2-20261024.json`, structure PASS), offline plan
`plan_keyboard_sheet_batch.py` (one insert), applied revision-guarded: new row Main!A255:C255 EPOMAKER / HE65 V2 /
"Implemented; awaiting hardware testing", yellow, 32 px, dropdown. Readback (`...-after-...json`): structure PASS
(150 blocks), every other row and its C colour identical to the before snapshot, `support_notice_catalog.py --sheet`
PASS (286 models, live). X82 HE row unchanged (already yellow).

Checks: `run_native_backend_checks.py --require-compiler` actually FAILED (EXIT=127, misread as 0 at the time):
`attackshark_pro_diagnostic_model_test.cpp` still asserted the old `Known()` rule; fixed later the same day (see
TESTER_LOGS_ACE60PRO_TS61_2026-10-24.md). Then `build_release.ps1` EXIT=0 (LAYOUT_CATALOG PASS,
YELLOW_LAYOUT_AUDIT PASS 286, SUPPORTED_LAYOUTS PASS 78, also with --sheet on the live readback). EXE SHA256
`22ba254566904cca4942d19651fc7a165b9eacd51f67de3d525a21526675e77a` (local, not released).

## HE65 V2 TMR: logs from 1.6.12 — Supported (2026-10-24)

Same user, "HallJoy (19).log" (wireless) and "HallJoy1.log" (same HallJoy session continued, cable plugged in).

- Wireless: only the 2.4G receiver `3151:5038` ("2.4G Wireless Keyboard") is present. The RongYuan stream stays
  `not_present`; no analog over the receiver, the same as every other RongYuan keyboard (M1 V5 TMR, HE75 V2 TMR).
  Not a HallJoy fault; receiver admission is not planned.
- Wired: after the cable is plugged in, `rongyuan.enumerate`, then `rongyuan.connected value=3417 detail=100` and
  `evidence.rongyuan reader board=3417 pid=5030 priority=2 mmcss=1`. rongyuan-stream `connected`, 66 mapped keys,
  updates 30 -> 6547 over about 6 minutes, `failures=0`, `ui_communication_warning=0`. The stream is change-driven,
  so the growing update count is the user's key movement. No keys were bound (`unique_keys=0`), so the log has no
  gamepad trace; the analog source itself works.
- Result: criteria met (working analog path over USB, built-in layout "EPOMAKER HE65 Mag + HE65 V2 ANSI").
  HE65 V2 is Supported (USB): confirmed_devices token 0x0c659d0f66768267 for pid 20528 board 3417 (yellow notice
  removed; HE65 Mag on 502F keeps its own notice), supported_layouts, README, SUPPORTED_HARDWARE, release notes.
  Backups in `.local/backups/he65v2-supported-20261024/`.
- Checks: `support_notice_catalog.py --write` PASS (285), `check_rongyuan_stream_profiles.py` PASS, native checks
  EXIT=0, `build_release.ps1` EXIT=0 (LAYOUT_CATALOG PASS, SUPPORTED_LAYOUTS PASS 79). EXE SHA256 `53dcabd864e5ec7421cb15cb636de225442353097a947b14fac7c96115184945`
  (local, not released; also carries the Ace 60 Pro fixes). Sheet Main!C255 "Implemented; awaiting hardware
  testing" -> "Supported", B255:C255 green (value and format as separate requests, revision-guarded). Readback
  A254:C256: row 255 changed, rows 254/256 unchanged, ONE_OF_LIST kept.

## X82 HE: log from 1.6.12 — Supported (2026-10-24)

User log "HallJoy (20).log" (HallJoy 1.6.12.0), the user's ATTACK SHARK X82HE on 3151:5029 "Gaming Keyboard"
(also a 2.4G receiver 1D57:FA60 of another device).

- ATTACK SHARK backend: `shark.identity value=3086`, `shark.state value=2`; native row `attackshark-pro`
  `connected`, pid 5029, 83 mapped keys, updates 120 -> 27171 over ~2.5 min, `failures=0`, `io_errors=0`,
  page samples on pages 0/1/2, `expired_pages` ~1.5/s. The 1.6.12 `Known(3086,0x5029)` fix works.
- Gamepad: W (HID 0x1A) bound to RT. `trace.pad` shows the key depth 0 -> 1000 -> 0 with RT 0 -> 255 -> 0 through
  intermediate values (68, 97, 125, 301, 388, 448, ...), following the key, several presses. Analog reaches the gamepad.
- Communication warning on for ~10 s (266734 -> 276687 ms). Cause found in the log: the RongYuan stream backend
  answered board 3086 twice (`rongyuan.identity_pair_rejected`, not its board), then the ATTACK SHARK backend
  took the keyboard; every later RongYuan pass got no valid 0x8F reply (`rongyuan.admission_failed value=2
  error=13`, ERROR_INVALID_DATA) because the replies were lost in the other backend's traffic. Only answered
  rejections counted toward the 3-attempt limit, so the RongYuan backend probed the keyboard about once a second
  for the whole session (107 failed admissions, each a shared open plus 0x8F exchanges) and only reached
  `identity_waiting` by chance after ~2 min. Its handle made the ATTACK SHARK exclusive open fail
  (`shark.exclusive_open_failed error=32` at 261453), which HallJoy reported as a communication anomaly
  (source 17). The extra traffic is also the likely cause of the shark exchange maximum of 22 ms and the expired
  pages (not provable from the log).
- Fix (local): `rongyuan_identity_budget.h` (`IdentityBudget`): once a path answered a rejected board, every later
  failed pass on it also counts, so probing stops after 3 passes until a device change. Unit test in
  `rongyuan_stream_protocol_test.cpp` (`identity_budget`). Backup `.local/backups/x82he-ace60pro-supported-20261024/`.
- Result: criteria met (working analog path over USB, built-in layout "ATTACK SHARK X82 HE ANSI"). X82 HE ->
  Supported (USB): `keyboard_support_notices.json` AttackShark confirmed_devices token 0x007116390F63150E for
  pid 20521 (log) and 20528 (vendor catalog PID, same board and layout), model removed from the yellow list;
  `supported_layouts.json`; README; SUPPORTED_HARDWARE; RELEASE_NOTES_NEXT.
- Sheet: Main!C120 ATTACK SHARK / X82 HE "Implemented; awaiting hardware testing" -> "Supported", B120:C120 green
  (value and format as separate requests, revision-guarded, together with C532 Ace 60 Pro). Readback A119:C121:
  row 120 changed, rows 119/121 unchanged, ONE_OF_LIST kept. Intended-state snapshot
  `.local/sheet-rows-20261024-after-x82he-ace60pro.json` (the after-HE65 V2 snapshot with HE65 V2, X82 HE and
  Ace 60 Pro set to their live status; its Supported set was compared with a live read of Main!A1:C900):
  `check_supported_layouts.py` PASS (81), `support_notice_catalog.py --sheet` PASS (283).
Checks: `run_native_backend_checks.py --require-compiler` after `research_reference_checks.py --record` (the
notices JSON is a referenced source): every test PASS, including `RONGYUAN_STREAM=PASS ... identity_budget`, except the
parallel `process_generation_supervisor` (EXIT=1, line 412 timing assertion). Its cached binary is unchanged since
2026-10-12 and run alone it passed 2 of 3 times, failing on a different timing assertion (line 365): a pre-existing
flaky test, not this change. HallJoy (running elevated) was closed via UAC taskkill without /F. First `build_release.ps1`
run: the isolated profile test hit its 30 s limit after all stages up to overlay_edit had passed; the same simulator alone
passed (31 s for the whole script, the limit is tight). Second run EXIT=0: PROFILE_TRANSACTION PASS, LAYOUT_CATALOG PASS,
YELLOW_LAYOUT_AUDIT PASS 283, SUPPORTED_LAYOUTS PASS 81 (also with --sheet on the readback snapshot). EXE SHA256
`4cad3e1ab06f5604696e3b3b39dcb470e667e0460d3c2d94d786e7cd0d9c5e74` (local, not released; also carries the Ace 60 Pro Supported change).
