# Window placement: read-only implementation review

Implementation follow-up: see [window placement update](WINDOW_PLACEMENT_2026-09-09.md).
The findings below describe the pre-fix implementation.

Existing feature, not a missing QoL addition. app.cpp WM_DESTROY reads normal
placement and writes width/height/x/y through Settings_SetMainWindow*; settings_ini
persists Window Width/Height/PosX/PosY. Startup restores them through CreateWindowExW.
No production changes or user-window manipulations during this review.

Findings from code (not physical multi-monitor reproductions):

- rcNormalPosition is workspace-relative for this top-level window, but startup
  treats saved coordinates as screen-relative. Top/left taskbar offsets can cause
  position drift. Microsoft documents this coordinate-system mismatch:
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-windowplacement
- IsWindowRectVisibleOnAnyScreen intersects only the enclosing virtual-screen
  rectangle, not actual monitor work areas. Empty gaps in monitor arrangements
  count as visible; a tiny edge intersection also passes with the titlebar hidden.
- Startup enforces minimum dimensions but does not fit oversized saved dimensions
  to the destination work area after a monitor/resolution change.
- Maximized state is not stored/restored; only the normal rectangle is retained.
- New geometry is captured only at WM_DESTROY, not at end of move/resize. A crash
  before normal close loses that session's geometry (not proof of lost profile data).
- main.cpp deliberately selects SYSTEM_AWARE (comment explicitly avoids per-monitor
  relayout), so absent WM_DPICHANGED handling is not independently a bug. Do not
  switch to per-monitor awareness as a placement fix. Monitor/DPI-change behavior
  needs targeted validation before changing that deliberate policy.

Focused placement/DPI/monitor topology regression tests were not found in the
test-source search. Recommend a narrow placement-policy fix with synthetic monitor
geometry tests and Win32 save/restore integration, not a rewrite of settings storage.
