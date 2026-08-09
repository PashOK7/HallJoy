# HallJoy Custom UI Architecture

## Release rule

All scrollable keyboard pages use one viewport architecture. A page-specific
scroll implementation, a timer-led frame patch, or moving visible child HWNDs
during scroll is not an acceptable release solution.

The reference is no longer a single page. The contract is shared by:

- Remap;
- Configuration;
- Gamepad Tester;
- Global settings;
- Input Overlay;
- Mouse settings.

## Shared viewport contract

`HallJoy/custom_page_surface.h` owns the common mechanics:

- `CustomPageSurface` stores content height, viewport offset, retained bitmap,
  cache invalidation and scroll performance counters;
- `CustomPageScrollController` owns wheel remainder, scrollbar thumb capture,
  track paging and cancellation;
- `CustomPageSurface_HandleScrollMessage` is the only active wheel/thumb/track
  state machine for the six pages;
- layout and hit-test rectangles are content coordinates;
- `CustomPageSurface_ClientToContent` and
  `CustomPageSurface_ContentToClient` are the coordinate boundary;
- `CustomPageSurface_Present` rebuilds a dirty retained cache when necessary,
  copies the viewport slice and draws the common scrollbar;
- a scroll-only update changes `scrollY`; it does not rebuild unchanged content
  and does not reposition a forest of child windows.

Page renderers use `HallJoy/custom_page_controls.h` for shared dark-theme
primitives. Page-specific code describes items, layout, values and actions; it
must not implement another scrollbar lifecycle.

## Rendering ownership

Each scrollable page is one visible HWND and one composited visual result.
Labels, buttons, chips, sliders, closed combo faces, Remap cards and icons are
records rendered into the page surface. This prevents independent native
erase/paint lifecycles from exposing white frames or temporarily disappearing
elements while the viewport moves.

Dynamic regions may render live when their values genuinely change every UI
tick. Gamepad Tester bars are the current example. They still use the same
scroll controller, coordinate model and scrollbar; “dynamic” does not permit a
second scrolling architecture.

Configuration's live graph follows the same rule with an explicit two-phase
composition. `KeySettingsPanel_DrawGraphRetainedContent` renders only the
cacheable plot and curve into the page surface. After
`CustomPageSurface_Present`, `KeySettingsPanel_DrawGraphViewportOverlay`
renders the current analog marker and then the editable handles in viewport
coordinates. A telemetry tick invalidates only the graph rectangle; it must
never mark the full page cache dirty. This separation is required both for
continuous input updates and for retained-scroll performance.

Native controls may remain only as nonvisual compatibility/action controllers
or as transient popup/keyboard owners when replacing them would remove required
semantics. In particular:

- closed Configuration and Global combo faces are rendered in the retained
  page;
- their `PremiumCombo` HWND is shown only to own an explicitly opened popup and
  is closed before viewport movement;
- hidden Remap icon HWNDs preserve existing drag/action metadata, but never
  paint or move as scroll content; the selected controller is positioned once
  when a drag starts.

These compatibility HWNDs are transitional internal objects, not members of
the visual layout. New visible scroll content must not be added as child HWNDs.

## State and invalidation

The page owns hover, pressed, focus, drag and edit state. A semantic value or
layout change marks the surface dirty. A viewport-only change invalidates the
window without invalidating the retained content cache.

The following operations must mark content dirty:

- settings/profile/layout changes;
- graph morphs and status text changes;
- hotplug-dependent visibility or labels;
- Remap pack/icon/action changes;
- animation state that changes rendered content.

Scrolling, scrollbar hover and thumb capture must not mark stable content dirty.

## Popup and input rules

- Hit testing converts the client point to content coordinates exactly once.
- Scrollbar capture belongs to `CustomPageScrollController` and is cancelled on
  `WM_CAPTURECHANGED` or `WM_CANCELMODE`.
- An open popup is closed before scroll changes its anchor.
- Wheel and thumb paths must remain responsive without `RDW_UPDATENOW`, native
  background erase, or a low-priority `WM_TIMER` scheduler.
- The shared keyboard preview is outside the subpage viewport. Its input dirty
  bits must invalidate it on every active subpage and must never be gated by the
  Remap tab.

## Enforced invariants

`tests/pre_release_ui_static_audit.py` and
`tests/factory_reset_static_audit.py` guard the active architecture:

- all six pages call the shared scroll controller;
- retained pages present through `CustomPageSurface_Present`;
- Configuration's retained callback cannot call the complete graph renderer;
  the live graph overlay must be composed after retained presentation;
- Remap active layout does not call `SetWindowPos` or `DeferWindowPos`;
- Global active layout does not move child HWNDs;
- Configuration active custom layout and Spark rows do not move child HWNDs.

`tools/run_ui_scroll_stress.ps1` launches the production executable, activates
every real page surface, warms retained caches, sends bounded wheel/update
bursts, checks responsiveness and steady-state GDI/USER handle growth, then
requires a graceful zero-exit shutdown. It does not take screenshots and cannot
replace owner visual acceptance.

## Release validation

Automated gates:

- static architecture audits;
- full production build and native test suite;
- six-page scroll stress with stable GUI resources;
- production lifecycle qualification with unchanged user state.

Owner visual gates on the exact final executable:

- fast wheel scroll and scrollbar drag in every page;
- no missing elements, white flashes, stale pixels or ghost trails;
- resize while scrolled and tab switch away/back;
- sliders, combo popups, text fields and Remap drag while scrolled;
- keyboard preview and Gamepad Tester releases never stick;
- motion is acceptable on the owner's high-refresh display.

No previous `Verified` label substitutes for this final visual gate.

## Retained control visual identity

A retained page must not approximate an existing custom control with a generic
button or a second renderer. `PremiumCombo::PaintRetainedFace` is the canonical
closed-face renderer for Configuration and Global settings. It uses the same
font, text alignment, arrow separator, border metrics, placeholder rules and
extra save icon as the popup-owning `PremiumCombo` HWND.

The retained face is deliberately stable: opening a popup must not substitute a
different font or add a second focus outline. The controller may be visible only
while its popup is logically open. `PremiumCombo::MsgDropStateChanged` requires
the owning retained page to hide that HWND immediately after close, including
Escape, outside click and selection paths.

Encoding-sensitive glyph strings are forbidden for small UI icons. Remap's
gamepad-disable action reuses the vector power renderer; dirty profile state uses
the existing vector save icon rather than an appended Unicode status marker.

## Retained interaction semantics

Retained rendering does not weaken control behavior. Hit IDs are classified by
bounded domains rather than threshold comparisons. User binding mutations call
`KeyboardUI_SaveBindingsAfterUserChange`, which owns persistence plus the
global-profile dirty notification; direct active-binding saves in Remap paths
are forbidden by the static audit.

Input Overlay direction, depth source and label font follow the same hybrid
PremiumCombo contract: canonical cached face, content-coordinate hit testing,
and a hidden controller shown only while its popup is open. A choice control
must not emulate a combo by cycling text on button activation.

Because `PremiumCombo_Popup` is top-level, pointer wheel input targets the popup
rather than the hidden/anchored controller. `PopupProc` routes the unchanged
message synchronously to `ComboProc`. Only the controller may mutate
`hotIndex`/`scrollTop`; duplicating list-scroll behavior in the popup is
forbidden.

Wheel routing and option navigation are separate contracts. An open popup calls
`ScrollPopupWheel`, which may mutate only `scrollTop` and only if
`GetMaxScrollTop() > 0`. `curSel` and `hotIndex` are invariant under wheel
input. Fitting lists ignore the wheel; keyboard arrows retain option-navigation
semantics.
