# Pause button and Configuration caption — 2026-09-08

Removed the redundant SDK / Gamepad Tester caption below Block Bound Keys.
Preset save/error feedback remains available in that row. Tester diagnostics
and settings persistence are unchanged.

The Global settings pause button is now 210 logical pixels wide, aligned with
the other compact buttons. Active: Pause HallJoy; paused: Resume HallJoy with
an amber fill, outline, static soft halo and adjacent Paused status. Transitions
are disabled and labelled Pausing / Resuming. A pause fault is not presented as
a successful pause: it shows Restart required / Needs restart in rose.

Root cause of delayed text: timer invalidation did not invalidate the retained
CustomPageSurface image. Mouse interaction marked that image dirty, revealing
the updated text. Replaced Global-page timer invalidation with owner state-change
notifications: publish atomic state, enqueue a root-window message, forward on
the UI thread and mark the retained surface dirty. The optional owner callback
must only enqueue work, never wait for UI or re-enter the owner. Button text,
colour and enablement render from one state snapshot. No animation, additional
timer or continuous idle repaint. The halo is six cached translucent rounded
rectangles; input/hit testing still uses the original button rectangle.

Validation so far: Release x64 build passed (existing ViGEm PDB warning); complete
static audit suite passed. Added a native owner/message-queue regression for
active, paused, resumed, failed pause and no-change idle notifications; direct
Clang build/run passed. Test is registered in the standard native check runner.
Full static and portable C++ suite passed, including the Windows notification
test: `.local/pause-ui-full-tests.log`. Source inventory check passed (465).

Deployed and launched the release through Explorer. Real-window verification
on PID 16700: sent Pause and Resume commands without moving the cursor over
the button; both completed and rendered the correct label/colour without hover.
Screenshots: `.local/pause-active.png`, `.local/pause-paused.png`, and
`.local/pause-visual.png` (resumed). Visual check confirms compact alignment,
unclipped labels and restrained static halo. Left HallJoy running and active.

Rollback: `.local/backups/pause-ui-20260908/` contains pre-change sources and
the previous release HallJoy.exe. New release SHA256:
`D8CDE514C5F481631B24BD4C9E2DF29EFB61A665C18AC06D774D67DF636AB7C2`.
