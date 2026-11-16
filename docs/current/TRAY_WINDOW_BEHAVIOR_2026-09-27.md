# Tray window behavior — 2026-09-27

Owner requested independent minimize/close button behavior in Global Settings.
Two checkboxes select hide-to-tray. Both default off, preserving Windows taskbar
minimization and application exit until explicitly enabled. Close-to-tray also
applies to Alt+F4. Settings are application-wide Window preferences, atomically
saved to the base settings.ini; input profile loads do not change them. A failed
save restores the prior setting and reports the failure.

The icon exists while the main window is hidden. Click to restore, or right-click
for Open HallJoy / Exit. A second executable invocation restores the existing
window through a bounded acknowledged message instead of starting another engine.
Maximized state is preserved. No timers or polling are added for tray operation.
The existing hidden-window visibility path suspends preview/UI work; input and
gamepad processing remain active.

Hiding requires successful Shell_NotifyIcon registration. If unavailable, minimize
falls back to the taskbar and close keeps the window accessible with an error.
TaskbarCreated recreates a missing icon after Explorer restart; if that fails,
the application window is restored. Explicit tray Exit, OS session shutdown and
factory-reset restart bypass close-to-tray. Unsaved layout editor handling remains
in place before hiding/exiting.

Build tooling uses a capability window property and registered explicit-exit
message, with WM_CLOSE retained for older builds. This avoids treating an enabled
close-to-tray preference as failed shutdown. Automatic restoration remains enabled.

## Validation and delivery

- Production-linked profile tests PASS: preference roundtrip, profile isolation,
  atomic save failure, existing profile/layout tests and 18 startup recovery cases.
- Private-desktop Win32 test PASS: hide/restore, idempotence, maximized restoration,
  second-launch restore handshake and icon cleanup. Shell calls are injected to
  exercise Explorer restart and registration failure without changing the user's
  desktop. This is not a manual/visual Shell UI test.
- All static audits PASS. Ordinary Release build and six executable gates PASS.
- Installed build/bin/Release/x64/HallJoy.exe SHA256
  af3f6e5d62d4d7721879bb85a46848f560226f00ef1703bd647701ee267f2645.
  The build helper restored the previously running app; read-only inspection
  confirmed WasRunning=true and Normal window state.

Evidence: .local/tray-profile-20260927.log, .local/tray-static-20260927.log,
.local/tray-release-20260927.log. Backup: .local/backups/tray-20260927-*.zip.
No GitHub publication or keyboard support changes. Game profiles remain future work.

## Follow-up: tray actions and less explanatory text

Owner requested removal of the tray hint. Removed its text and reserved height.
The tray menu now adds Pause/Resume HallJoy and Start/Stop Input Overlay.
Engine actions use the existing owner transition path, are disabled during
transitions/fault, and recheck state after menu dismissal to avoid reversing a
newer state. Overlay actions use the configured port and existing server lifetime
API, persist autostart intent, and explicitly override command-line startup intent
so Stop is not undone by the watchdog. Failures restore the window and report the
server error. The retained overlay page receives state invalidation so its cached
controls match tray actions on reopening.

Extended private-desktop menu tests check action IDs, labels and enabled/disabled
states. Profile/lifecycle tests and 18 recovery scenarios PASS; engine owner,
overlay shutdown and build lifecycle static audits PASS. Ordinary Release rebuilt
with six gates and automatic app restoration. Evidence:
.local/tray-menu-profile-20260927.log and .local/tray-menu-release-20260927.log.
No publication.

## Pause icon

The confirmed Paused state adds an opaque amber badge with two dark pause bars
to the tray icon and changes the tooltip to HallJoy - Paused. Confirmed Active
restores the original icon. The previous stable indication stays during a pending
transition; clicking Pause does not prematurely claim completion. All pause/resume
entry points use the existing engine-state notification. No polling timer added.
The badge icon is generated once on demand at the system small-icon size, reused
across changes, and released with its owner. Explorer restart uses the current
status. Failed Shell icon modification attempts re-registration, or restores the
window if that also fails.

Private-desktop regression checks distinct pause icon and tooltip, duplicate-state
coalescing, Explorer restart retaining pause, and restoring the original icon on
resume. Evidence: .local/tray-pause-profile-20260927.log and
.local/tray-pause-release-20260927.log. Visual appearance remains owner-evaluated.


## 2026-11-09 — Alt+Tab window availability report

A tester reports that after switching to a game, the HallJoy window could
not be brought back while controller input still worked. The new downloads
checked for this report are copies of the earlier keyboard test logs.
No trace of the affected HallJoy session is available locally.

Focus loss cancels shortcut capture. Hide-to-tray requires an explicit
minimize/close action with the corresponding setting enabled. The normal
second launch sends a bounded request for the running main window to show.
This is a useful comparison with the taskbar and Alt+Tab. Successful
restoration demonstrates that the UI handled that request. Failure alone
can mean an unresponsive UI, a missing window or a failed activation.
Windows "Not responding" provides separate evidence about the message pump.

Triage on the affected build: record version, game and fullscreen mode,
try taskbar or the same EXE while the original process is still running,
and record whether Windows labels HallJoy "Not responding". With Enable
logging selected before reproduction, preserve the ordinary HallJoy.log
from its data folder before restarting. The ordinary log can correlate
engine and support events but does not identify a blocked UI call on its
own. No runtime diagnosis is established by the screenshot.
