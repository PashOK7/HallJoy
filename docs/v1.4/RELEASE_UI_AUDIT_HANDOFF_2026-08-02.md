# HallJoy v1.4 pre-release UI audit handoff

Snapshot date: August 2, 2026

## Status at handoff

The build captured by this document was **not ready for release**. Automated
V14-12Q checks had passed, but the project owner's subsequent manual review
rejected the UI, so V14-12Q returned to active work. None of the findings below
could be closed by a static audit or successful compilation alone; acceptance
required the real executable and physical analogue input.

Audit baseline:

- branch: `v1.4-integration`;
- problematic commit: `3b3e9a3` (`fix(ui): align Global settings scrolling and reset styling`);
- backup branch: `backup/pre-release-ui-audit-20260802`;
- executable: `build/output/HallJoy.exe`, 2,228,224 bytes;
- SHA-256: `6DFC616422D89783A846F7EE8CAEA64AD7D951591092576DEFB717320543DF96`;
- owner's physical keyboard: Irok MG75 Max through SparkLink.

## Recorded findings

### UI-01 / P1 release blocker: analogue input flickers the interface

Reproduction: open HallJoy and press analogue keys on the Irok MG75 Max. Visible
flicker is especially apparent along the tab row.

Known code path: `KeyboardUI_OnTimerTick` consumes analogue dirty bits and
invalidates owner-drawn keyboard windows. Several independent `InvalidateRect`
and `RedrawWindow` paths mix cached and uncached surfaces with parent-owned
drawing. The exact path responsible for touching tabs or exposing background
gaps was not yet proven; no path should be blamed without paint/invalidation
trace evidence.

Required result: physical input updates only changed dynamic regions. The tab
row, page background, and static controls do not redraw or flicker. Validate all
six tabs, multiple UI refresh rates, multi-key holds, rapid press/release, and
input while scrolling.

### UI-02 / P2 visual blocker: Factory Reset button does not match the UI

Observed behavior: a large bright-red strip appears at the left edge, followed
by a coarse dotted focus rectangle after clicking.

Confirmed cause: `Global_DrawActionButton` draws a separate four-DPI-pixel
accent strip and handles `ODS_FOCUS` through the standard `DrawFocusRect`.

Required result: remove the separate strip. Preserve visible keyboard focus,
but render it through the same rounded themed system as the button, without the
standard dotted GDI rectangle. Validate idle, hover, pressed, focused, disabled,
and 100/125/150/200% DPI states.

### UI-03 / P1 correctness: stale Configuration telemetry

Reproduction: open Configuration and watch backend telemetry while providing
input. Text remains stale until the pointer moves over `HE poll mode` or `Rows`,
at which point it suddenly refreshes.

Confirmed cause: `Config_DrawCustomControls` reads `BackendAnalogTelemetry`
only while painting its cached surface. Telemetry changes do not call
`Config_MarkSurfaceDirty`; hover happens to do so and refreshes the entire
cache.

Required result: visible live data updates at a bounded rate and affects only
its dynamic region. Hidden pages neither query nor repaint it. Add a regression
test proving refresh without mouse movement and no repaint for an unchanged
snapshot.

### UI-04 / P1 UX: `HE poll mode` and `Rows` are not real combo boxes

Confirmed cause: both fields are hand-drawn by `Config_DrawCustomCombo`, and
`Config_HandleCustomControlsMouse` cycles values on every click. There is no
dropdown, normal keyboard navigation, focus behavior, or standard combo-box
semantics.

Required result: replace both with themed combo boxes from the shared HallJoy
control system (`PremiumCombo`, unless the audit justifies a new shared
control). Validate dropdown selection, mouse, keyboard, focus, DPI, page
scrolling, and persistence.

### UI-05 / P2 information architecture: duplicated diagnostics

Backend/debug telemetry appears in both Configuration and Gamepad Tester, with
different update mechanisms and settings mixed with diagnostics.

Required result: define one canonical live-diagnostics area. The preferred
direction was to consolidate analogue/backend telemetry into Gamepad Tester and
leave Configuration with settings, the two real combo boxes, and only a short
status when configuration genuinely needs it. Before moving anything, map every
field so no Aula, UAP/Soup, SparkLink, or other native-route evidence is lost.

## Required clean-room audit

1. Map every HWND, owner-drawn control, custom surface, and parent on all six
   tabs: background ownership, cache, invalidation, and double buffering.
2. Instrument `WM_PAINT`, `WM_ERASEBKGND`, `InvalidateRect`, and `RedrawWindow`
   during physical input, recording HWND, region, cause, and frequency. Remove
   the instrumentation from production.
3. Prove that dynamic-model changes invalidate only their region and hidden
   tabs perform no live repaint.
4. Inventory controls that merely resemble standard controls, then validate
   mouse, keyboard, focus, accessibility, DPI, and scrolling semantics.
5. Map every diagnostic/telemetry line in Configuration and Gamepad Tester to
   one canonical location and snapshot/update pipeline.
6. Run the visual matrix at 100/125/150/200% DPI, minimum and normal window
   sizes, every tab, all interaction states, scrolling, and physical Irok input.
7. Repeat the complete build/runtime gate, but do not mark the UI Verified until
   the owner manually accepts the produced `HallJoy.exe`.

## Acceptance criteria

- no visible flicker in tabs or static regions during analogue input;
- no arbitrary page/tab-wide invalidation from one key change;
- live telemetry refreshes without hover and remains untouched when unchanged;
- `HE poll mode` and `Rows` are genuine themed combo boxes;
- diagnostics appear once without losing route-specific information;
- Factory Reset matches the shared design in every state and retains a clear,
  tidy keyboard-focus indication;
- owner visual acceptance and automated regression gates both pass.

Physical MAD68 HE/UAP and Aula validation remained independent release blockers
and could not be closed by this UI audit.
