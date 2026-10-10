# K4 onboard: "No supported analogue keyboard" while a game holds the K4 (2026-10-10)

Status: host fixed and built (release EXE SHA256 a678c859989ed2b4...). Firmware unchanged.
Not yet run against the keyboard by the owner.

## Owner report
HallJoy 1.6.8 showed "No supported analogue keyboard detected" with the owner's
Keychron K4 HE (HJO1 firmware, 3434:0E40 REV_1212) connected, while Valheim was running.

## Evidence (owner's HallJoy.log, session 2026-10-10T16:59:44Z)
- `k4.onboard_prepare_failed value=3 detail=29`: stage 3 = Open. The HJO1 vendor
  interface (FF60:0061) enumerated (serial ...HJO1), but `CreateFileW` failed on all
  29 attempts in the 3 s window.
- `k4.onboard_late_takeover 1` paused/resumed the engine; the second prepare failed the
  same way. No further takeover: K4 row `observation=not_present` for the rest of the
  ~44 min session, banner on.

Live check with HallJoy closed (agent, read-only):
- K4 vendor interface, PDO `\Device\00000a56`: metadata open OK; exclusive R/W open
  (share 0) fails with Win32 error 32 (sharing violation); shared R/W open succeeds.
- System handle table: `valheim.exe` holds a handle to `\Device\00000a56` with access
  0x12019F (read/write). Unity's input system opens every HID interface.

## Root causes
1. `WindowsChannel::Connect` opened the vendor interface exclusively (share mode 0). Any
   program holding a shared handle (Unity games, RGB/launcher tools, WebHID pages) made
   the K4 unusable for the whole HallJoy session. Exclusivity was not needed: ownership is
   already enforced by the firmware's HJO1 session token (a foreign session is never
   taken over), and replies are matched by tag 0xA9, command and token.
2. Late takeover was lost: the timer handler dropped a retry that came within 5 s of the
   previous attempt, and nothing re-armed the timer afterwards.
3. The log did not say why the open failed.

## Fix (host)
- `keychron_onboard_channel.cpp`: shared R/W open (`FILE_SHARE_READ|FILE_SHARE_WRITE`);
  `LastOpenError()` keeps the Win32 error of a failed open.
- `keychron_onboard_client.h`: multi-page depth reads (sparse 0x7E, compact burst 0x7B)
  skip foreign reports between pages (`SkipForeign`, bound `kMaxStrayReports`=8 per
  read, counted in `StrayReports()`). Single-reply queries already skipped them.
  With a shared endpoint another program's QMK/VIA replies can arrive in our queue.
- `app.cpp`: a takeover retry inside the 5 s spacing re-arms the timer for the rest of
  the spacing instead of being dropped (attempt limit unchanged: 2 per K4 presence).
- New log event `k4.onboard_open_failed value=<attempts> detail=<Win32 error>`.

## Tests
- `tests/keychron_onboard_client_test.cpp`: 8 foreign reports between pages are skipped
  (sparse and burst), 9 fail the read without changing published depth.
- `tools/run_native_backend_checks.py --require-compiler`: all PASS.
- `tools/build_release.ps1`: PASS (full catalog, K4 onboard, profile/layout tests).

Agent environment note: running the portable tests from Git Bash picks up Git's
`libstdc++-6.dll` first in PATH and `support_log_windows` crashes at -O2 (ABI mismatch,
not a code defect). Put the WinLibs `mingw64\bin` first in PATH.

## Owner check
Start HallJoy while Valheim (or another Unity game) is running: the K4 should be found
and the banner should not appear.
