# MonsGeek M1 V5 TMR: wired lag report (2026-10-16)

## Report

Discord tester (iddqd), MonsGeek M1 V5 TMR, board 2949, wired 3151:5030, 2.4 GHz
receiver 3151:5038 also plugged in. Over 2.4 GHz the keyboard sends no analog
values (power saving, expected). The tester says that even wired it is "buggy
with ~200 ms latency sometimes". The number is the tester's impression, not a
measurement: the problem is an occasional noticeable lag, of any size.

## Log `HallJoy (4).log` (1.6.9.1)

- HallJoy ran about 4 minutes (first trace at uptime 138257 s, report at
  138497 s); the whole bound-key trace is in the log (W = HID 0x1A on LY+).
- About 55 W presses in two sessions (Raw Input keyboard handle changed once,
  device change at 138369.9 s, reconnect `rongyuan.connected 2949` 0.4 s later).
- Every press: the analog value starts rising 0-16 ms before Windows' key-down
  and returns to 0 together with Windows' key-up; the gamepad output follows the
  same tick. No stall, no missing release, no invalid packet in the second
  session (`failures=1` is the first session's end at the replug).
- Conclusion: the lag did not happen in this run, or it is outside HallJoy's
  input path. The log cannot say which, and it lost the first session's
  `rongyuan.session_end` and `keyboard.communication_warning=1`: the shared
  512-line history was filled by HID inventory and snapshot lines (about 70 per
  device change).

## Changes (local build, not released)

1. Reader scheduling. The RongYuan stream worker ran at normal priority while
   the realtime loop that consumes its values runs MMCSS "Games" + highest. A
   foreground game that keeps every core busy gets longer quanta than the
   background HallJoy thread, so reports could wait in the HID queue for several
   quanta: an intermittent lag that appears only in game. The worker now uses
   MMCSS "Games" + `THREAD_PRIORITY_HIGHEST` (reverted when the thread ends). It
   sleeps in the HID read almost all the time. Logged once per connection:
   `evidence.rongyuan reader board= pid= priority= mmcss=`.
2. Press timing (`rongyuan_press_timing.h`, unit-tested). Windows key events of
   the same keyboard (Raw Input device name matches the connected VID:PID) are
   compared with the stream, QueryPerformanceCounter clock. One line per
   finished press of a key bound to the gamepad:
   `evidence.rongyuan press hid os stream lead_ms down_wait_ms up_wait_ms samples
   max_gap_ms peak dur_ms backlog ui_ms slow [after_release_ms]`.
   - `down_wait_ms`: Windows had the key while the stream still read 0 (stream late).
   - `up_wait_ms`: stream reached 0 after Windows' key-up (release late).
   - `os=1 stream=0`: the stream never reported the press.
   - `backlog`: reports already queued when the reader got to them (reader late).
   - `ui_ms`: how long WM_INPUT waited in HallJoy's UI queue (Windows side late).
   A press is `slow=1` when either wait is at least 30 ms, the stream missed it,
   or the backlog reached 16; slow presses are kept whole (first 256), all
   presses follow the evidence head/tail rule. `press_summary` lines at 1, 2,
   4, 8 ... presses. Content-bounded, no time window; only bound keys.
3. Support log: periodic state (`hid.candidate`, `inventory.complete`,
   `snapshot.*`, `diagnostics.coverage`, `uap seq`, `input.config seq`,
   `native seq`) has its own 512-line window; events keep theirs. Both are written
   merged in arrival order; the header adds `bounded_state=512`. Test
   `state_window` in support_log_windows_test.

Checks: `run_native_backend_checks.py --require-compiler` PASS
(`RONGYUAN_STREAM=PASS ... press_timing`, `SUPPORT_LOG_WINDOWS=PASS ... state_window`),
`build_release.ps1` PASS. EXE SHA-256
55a977034b1368a26d2533d0f8d1611c1fda3e0d0d01e1cdd3407f8025483908 (local).

## Next

The tester plays as usual with this EXE and sends the log after a lag. The press
lines name the late side: keyboard stream (`down_wait_ms`/`os=1 stream=0`),
HallJoy reader (`backlog`), Windows/UI (`ui_ms`), or none of them (then the
lag is after HallJoy's gamepad output). No support status change.

## Log `HallJoy (13).log` (evidence build, wired 3151:5030 board 2949), 2026-10-22

Tester: "even in wired mode it's buggy with 200ms latency sometimes".

- 30 presses: `backlog=0`, `ui_ms=0` (one 15), `down_wait_ms=0`; the gamepad follows the stream in the same tick.
  `up_wait_ms` 30-121 ms on the early presses is the keyboard's Rapid Trigger (Windows key-up on the first lift
  while the key is still deep); this is normal RT behaviour, not a delay. The press lines cannot show a 200 ms event.
- Found: the Attack Shark backend (protocol 17, also VID 3151) re-probed this keyboard every ~11 s. Its child found
  the board owned by the RongYuan stream (`identity_stream_owned`) and waited for a device change, but it printed
  nothing while waiting, so the parent's 8 s no-output watchdog cancelled it (`shark.worker_exit value=4`) and the
  supervisor restarted it 3 s later: 9 probes in 90 s, each an exclusive open (sharing violation 32, held by HallJoy's
  own RongYuan reader), a shared open and 0x8F identity feature exchanges sent to the keyboard while it streamed.
  The sharing violation counted as a communication anomaly, so "Keyboard communication is unstable" toggled 6 times.
  The RongYuan backend itself had `failures=0` and no session end.
- Effect on the stream: only one probe (1036.906 s) overlapped a press; stream samples continued at the normal
  15-30 ms spacing through it. One overlap cannot rule out a stall on other probes; the probe is a plausible cause
  of an occasional lag (feature commands interleaved with the stream) and is removed regardless.

Fix (local): the waiting child sends a 1 s heartbeat (`waiting_for_device`, ignored by the parent log), so one probe
per device change; an exclusive-open sharing/lock violation is reported as an anomaly only if the pass does not end
with `identity_stream_owned`. Shark self-test covers both cases. Native checks EXIT=0, build_release EXIT=0,
EXE SHA256 `958fad4dff4bee203d07dc7048882170c8bfcb498f9b96b2ffa53ef5db1e988a` (local, not released; also contains
the red unsupported-keyboard banner). Backup `.local/backups/shark-stream-owned-wait-20261022/`.

## Supported (owner, 2026-10-22)

Owner: remove the yellow notice. Board 2949 (3151:5030) moved to `confirmed_devices` of the RongYuanStream
notice group (token 0xb51adb7f896f227f = layout "MonsGeek M1 V5 TMR ANSI", pid 20528); `supported_layouts.json`
entry with `shownAs` "M1 V5 HE / TMR" (the picker name of the shared preset); README Supported list and
SUPPORTED_HARDWARE updated. research_reference_checks --record PASS, native checks EXIT=0, build_release EXIT=0
(SUPPORT_NOTICE_CATALOG=PASS 286, SUPPORTED_LAYOUTS=PASS 75). Intended Sheet state checked with
check_supported_layouts / support_notice_catalog / unsupported_keyboard_catalog --sheet: PASS.
Sheet: Main!C579 MonsGeek / M1 V5 TMR "Implemented; awaiting hardware testing" -> "Supported" (value write), B579:C579
green pair as Womier SK75 TMR (format write), one revision-guarded batch. Readback A578:C580: value, ONE_OF_LIST and
colors as intended, neighbours unchanged. EXE SHA256 `f4ff31fc41e8964e939d1c8587130e628c93bf9f59387bcca48646da84a0a5e6`
(local, not released). Backup `.local/backups/m1v5tmr-supported-20261022/`.
