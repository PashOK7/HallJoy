# "HallJoy is paused" right after start: fix (2026-10-05)

Report: a tester (MCHOSE Ace 68 Air III, HallJoy 1.6.7) saw "HallJoy is paused /
Input processing is stopped" right after start; Resume and other buttons did
nothing; restarting HallJoy fixed it. Not reproducible here, and the tester's
log came from the working run. Owner: fix the potential causes anyway.

## Mechanism found in the code

1. Startup runs the engine owner's Resume transaction in parallel with building
   the main page (1.6.7 faster start). Its RestoreUiInput step asks the UI
   thread through `engine_runtime_ui_bridge` and waited at most 5 s. A UI thread
   still busy building the window (slow PC, first start, antivirus scan) made
   the step time out; the transaction rolled back to Paused (generation > 0, so
   the pause card shows "HallJoy is paused").
2. After that timeout the bridge stayed `Cancelled` (or `Completed` when the
   handler finished late). `Execute` accepted only `Idle`, so every later
   request failed with ERROR_BUSY: each Resume failed again at the same step,
   and only a restart cleared it. This matches the report exactly.
3. If the handler was already running at the timeout, the late RestoreInput
   re-enabled the input hooks while the engine was paused, and the failed-Resume
   cleanup did not undo it (possible stuck/blocked keys).
4. The pause card's Resume set `pending` and posted a request whose result was
   ignored. A refused request (owner busy: `Submit` uses try_lock) left the
   button disabled for good.
5. A failed start was not an incident: no automatic log, no banner (the device
   search never ran), so there was no evidence.

`engine_runtime_ui_bridge_test` reproduces 2 against the previous bridge (it
fails at "next request after a timeout") and passes with the fix.

## Fix

- Bridge: `Cancelled`, `Abandoned` (waiter gave up while the handler ran) and a
  late `Completed` never block the next request; a result completed just after
  the timeout is used; an abandoned RestoreInput is undone with ReleaseInput on
  the UI thread before any newer request runs; timeout 5 s -> 15 s.
- Automatic recovery: Paused directly after a Resume phase marks the Resume as
  failed; HallJoy retries after 1, 2, 4, 8, 15 and 30 s (cleared when Active).
- Resume requests (card, toggle, hotkey, retry) go through one helper: a refused
  request is retried after 250 ms and the card is refreshed.
- Card text: "Input could not start" / "Retrying automatically", then "Click
  Resume to try again"; a user pause still reads "HallJoy is paused".
- Diagnostics: `engine.resume_step_failed` (step 1 reset providers, 2 catalog,
  3 backend init, 4 dependency guidance, 5 start generation, 6 restore UI input;
  Win32 detail or init-issue mask) and `engine.resume_failed` through
  `SupportLog_ReportFailure` (automatic log save), `engine.resume_retry`.

## Validation

- `engine_runtime_ui_bridge_test` (new, real message-only window): stale request,
  late restore undone, busy while running, shutdown cancel. PASS; previous bridge
  FAIL.
- `build_release.ps1` PASS (including the production pause-card test and 18
  recovery scenarios); native suite PASS. EXE SHA-256
  `2140a60a502750fca8c1f808afe4d66998b22a36d56089b83a2ad53a4d0fd50a`.
- The tester's exact trigger is not proven (no failing-run log); if it happens
  again, the automatic log will name the failing step.
