# One paint standard and animated tab transitions (2026-10-03)

## Owner request

- Render every tab the same way, keeping the advantages of each.
- Animate tab switching in 0.3 s, like a camera moving sideways along the
  tabs. Remap to Configuration: Remap leaves to the left, Configuration
  enters from the right. A long jump flies past every tab in between.
- The active tab indicator moves with the camera.
- Rapid clicks and resizing during a transition must keep working.
- Optimised throughout. The owner allowed rewriting the architecture if the
  result is not worse.

## One paint standard (`custom_page_surface.h`)

`CustomPagePaintScope` serves WM_PAINT and WM_PRINTCLIENT with one drawing
code. It combines what the pages did separately:

- **Configuration:** a persistent top-down 32bpp DIB (no allocation per paint;
  GDI+ writes the pixels directly) and composition clipped to the invalidated
  region. The pool now serves every page; one buffer of the largest page size
  is normally enough, plus a small pool for nested paints.
- **Global:** only the invalidated region is copied to the window.
- **All retained pages:** `CustomPageSurface_Present` of the shared scroll
  cache. `CustomPageSurface_Paint` is the complete standard handler, with an
  optional overlay (Global's pulse, Configuration's live telemetry draws
  after Present inside the same scope).

What moved to it:

- Tester, Overlay content, Overlay container, Global, both hidden Mouse
  pages, Configuration (its private buffer was removed), Remap and Profiles
  (page and selector);
- the slider, chip and combo face controls answer WM_PRINTCLIENT through
  their paint functions.

The obsolete Begin/EndBufferedPaint helpers are gone.

`CustomPage_PrintTree` draws a page and every descendant whose WS_VISIBLE
style is set (even when the page itself is hidden), bottom-up in z-order,
with exact offsets and clipping.

`tests/custom_page_paint_static_audit.py` enforces the standard:

- Present only inside the scope;
- WM_PRINTCLIENT next to every page/control WM_PAINT, except listed
  top-level popups (toasts, open drop-down lists, the layout editor);
- no private double-buffer helpers.

The simulator test `CustomPage_TestPrintTree` checks offsets of nested
children, hidden-child skipping and printing of a hidden page.

## Tab transition (`tab_transition.h/.cpp`, `tab_transition_motion.h`)

- **Camera.** The position is in tab units. Motion is a cubic Hermite from
  (x0, v0) to (target, 0) over 0.3 s; from rest it is the smoothstep
  ease-in-out.
- **Click during a transition.** The new motion starts at the current
  position and velocity. Velocity toward the target is clamped to the
  monotone bound (no overshoot); velocity away from it is kept (smooth
  turnaround).
- **Fly-through.** Pages are laid out side by side with a 24 px gap (scaled).
  Remap to Profiles passes all tabs in between.
- **Layer.** A second `keyboard_canvas` (Direct2D, frames paced to the
  display refresh like the keyboard view) covers the page area.
  - Snapshots are kept as CPU pixels; GPU bitmaps are recreated after a
    device loss.
  - Fractional positions use linear filtering, so motion has no 1 px
    stepping.
- **Start sequence (no visible change).**
  1. Capture the shown page.
  2. Show the layer and paint its first frame synchronously (that same page).
  3. Capture the path behind the layer: hidden pages are shown for the
     capture, so their on-show refresh runs.
  4. Keep only the target shown under the layer.
  5. Restart the clock, so captures do not eat into the 0.3 s.
- **End.** The final frame is the target at rest. Then the target is
  committed and repainted, and the layer is hidden: identical pixels, no
  flash. Snapshots are freed.
- **Resize during a transition.** `OnLayout` (called after pages are resized)
  moves the layer and captures the path again at the new size.
- **Input.**
  - While moving, the layer takes page-area clicks, so blind clicks cannot hit
    the hidden target.
  - The tab strip stays live; a click on the current target is a no-op.
- **Instant switch** when Windows client-area animations are off, or the
  window is hidden or minimised. Programmatic switches stay instant
  (`JumpTo` ends a running transition).
- **Tab strip (`tab_dark.h`).** The highlight (fill and accent underline) is
  drawn at the interpolated tab rectangle. Text colour blends from muted to
  full by distance to the camera. At rest it sits on the page that is
  actually displayed (`g_shownSubTab`), not on the control's own selection.

Owner report fixed: the clicked tab flashed active for one frame, because the
control selects and repaints before HallJoy handles the click. The
highlight's start is also set when a transition starts or retargets.

## Validation

- Portable: `TAB_TRANSITION_MOTION=PASS`:
  - endpoints and 0.3 s;
  - monotone motion with smooth steps;
  - retarget continuity, no overshoot, turnaround.
- Simulator: `CustomPage_TestPrintTree` inside
  `PROFILE_TRANSACTION_WINDOWS_TEST=PASS`.
- `build_release.ps1` and `run_native_backend_checks.py --require-compiler`:
  PASS. EXE SHA-256
  `edee4efc89e3a7b91946bfc5e7014ba7f1b26440e39d366efc9c09deaf236d49`.
- No visual run by the agent; the owner evaluates the animation.
- Capture cost of each page is traced as `ui.tab_transition.capture` (perf
  mode). Not yet measured on hardware.

## Motion blur and a stronger ease (owner request, same day)

- **Easing.** The cubic is replaced by a quintic Hermite with zero end
  accelerations. From rest it is smootherstep: a slower start and finish,
  peak speed 1.875x the mean instead of 1.5x. Retargets keep position and
  velocity.
- **No overshoot.** The bound, derived for the quintic, is
  |v0 * T| <= 2.5 * distance, because
  dx/ds = (1-s)^2 (30 D s^2 + T v0 (1 + 2s - 15 s^2)). The test covers the
  exact bound.
- **Motion blur.** The strip is drawn into a compatible off-screen GPU target
  and blurred with the Direct2D `DirectionalBlur` effect (angle 0, hard
  border, balanced optimisation).
  - Length L = |camera speed| * stride * measured frame time * 0.5
    (180-degree shutter); sigma = 0.35 L, capped at 48 px.
  - Zero speed at the ends makes the first and final frames sharp.
  - Systems without `ID2D1DeviceContext` keep sharp transitions.
  - The pipeline is created once per render target and survives
    transitions; it is released with the layer.
- **Validation.** `build_release.ps1` and the native suite: PASS
  (`TAB_TRANSITION_MOTION=PASS` with the quintic expectations). EXE SHA-256
  `82c1c2e03fa69100f07e7f9ef4b0a07953243e1c8b9d5afc9a255c495db0c92c`.
  The owner judges the look.

## Faster start for distant tabs (owner report, same day)

The owner found Remap to Profiles slow while neighbours were fine. The motion
is always 0.3 s. The delay came before it: the start captured every page on
the path and showed each hidden page for its capture. Configuration, Tester
and Global relayout and fully re-render on show.

Now:

- **Before the first frame**, only these are captured:
  - the shown page (Fresh);
  - the target (Fresh, shown behind the layer as before);
  - the pages at the current camera position;
  - the next page in the direction of travel.
- **Intermediate pages** are captured Cached:
  - a snapshot of the right size from an earlier transition is reused;
  - otherwise the hidden page is rendered as it is, without being shown
    (no on-show relayout).
- **Prefetch.** After each frame, at most one missing page ahead of the camera
  is rendered. The slow smootherstep start gives the frames time to fetch it
  before it enters the view; a missing visible page is still captured
  synchronously.
- **Cache between transitions.**
  - CPU pixels are kept; GPU copies are dropped.
  - Shown and target pages are always re-rendered (`fresh` flag).
  - `InvalidateCache()` drops the cache on a profile apply or a keyboard
    layout change, so hidden pages do not fly by stale.
  - A resize recaptures as before.
- **Accepted limit:** an intermediate page may show slightly older live
  values (the Tester) while it streaks by, blurred.
- Build and native suite: PASS. EXE SHA-256
  `654cd883d6d959e44e783612bb93b23b7fb45430641e513c3e8def9d95dae85b`.

## Duration 0.2 s (owner decision, same day)

- `kDuration` changed from 0.30 to 0.20 s (`tab_transition_motion.h`).
- The motion test now derives every time from `kDuration`, so a later change
  needs no test edits.
- Motion blur follows automatically: higher speed gives longer streaks, still
  capped at sigma 48 px.
- Build and native suite: PASS. EXE SHA-256
  `b15f3f42880da0fc9f038f0aabe95402958ad351c353c88352f5338e96b1af23`.
