# Window placement and multiple monitors

Checkpoint: `.local/backups/window-placement-20260909/` (previous source and EXE).
This supersedes the implementation described in WINDOW_PLACEMENT_REVIEW_2026-09-09.md.

## Contract

- Keep normal size, position and maximized state globally, independent of profile.
  A minimized window retains its normal/maximized restore state; next ordinary
  launch does not unexpectedly start minimized. Explicit minimized launch still wins.
- Version 2 stores normal SCREEN coordinates plus system DPI. Legacy Window
  Width/Height/PosX/PosY remain readable; legacy workspace coordinates are converted
  once using current monitor information. Historical monitor arrangement cannot
  be reconstructed if it has already changed; placement is safely fitted instead.
- Windows workspace/screen conversion is explicit at Get/SetWindowPlacement.
  Existing system-aware DPI policy is unchanged. Saved dimensions scale if system
  DPI differs across launches; unknown legacy DPI is not guessed. This is not
  per-monitor font/layout reinitialization.
- Fit to real monitor WORK AREAS, including negative origins and top/left taskbars,
  not the enclosing virtual rectangle. Pick greatest overlap, then nearest rectangle
  by Manhattan edge distance for disjoint rectangles. Fit the complete normal window
  to one available work area, shrinking only if needed. A window straddling screens
  is fitted to the best single screen on restore. Valid single-monitor placements
  are unchanged; absent saved location starts on the cursor's monitor.
- React to display changes and work-area changes, coalesced at 350ms, without moving
  the window while the user is dragging/resizing it. Never switch DPI awareness.

## Persistence and ownership

Capture after interactive move/resize, debounce other move/size events, and flush
on session-end query and normal close. No steady timer and no disk writes for every
drag step. Geometry-only changes atomically update ONLY Window in the existing base
settings.ini. Readback validates all seven placement values. Failure preserves the
previous destination. No extra log or migration files, and no geometry-only file is
created when the base settings file is missing.

An additional pre-existing cause was fixed: named-profile shutdown/autosave previously
saved profile settings, overlay and active marker but omitted window geometry. That
path now includes a window-only update without copying profile values into Default.

## Verification

`window_placement_test.cpp`: 20,000 deterministic topology/size cases, negative
origins, gaps, missing monitor, oversized window, top/left work-area offsets,
idempotence and DPI scaling. Win32 Get/SetWindowPlacement roundtrips and
normal/maximized/minimized restore states run on a private desktop which is never
displayed to the user (no screenshots or visual evaluation).

Production-linked profile transaction tests additionally cover all seven fields,
unchanged non-Window sections, profile-load isolation, failed atomic replacement
and missing-base rejection. Static guards cover the production event/save wiring.
Validation: full native/static/portable suite PASS; production-linked profile
transaction suite PASS, including window isolation and failed replacement; Release
x64 build PASS. On the actual deployed release, the end-of-move handler saved the
real placement without changing any non-Window sections. A graceful restart then
restored the saved geometry exactly (read-only comparison before any test save).
The user's window was not moved/resized for this check. No visual inspection.

Evidence: `.local/window-placement-{native-tests,profile-tests,build}.log`,
`.local/check-window-placement-runtime.ps1`. Startup after restart: PID 29980.
Deployed SHA256:
`4DA6B5D0DD22F18B2C7C1EB867FAE8A1577A9DAE93653392954CCAFACA701C95`.
