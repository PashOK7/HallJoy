# HallJoy v1.4 pre-release UI audit — August 2, 2026

## Audit basis

The audit restarted from the handoff in
`RELEASE_UI_AUDIT_HANDOFF_2026-08-02.md`. Earlier Verified labels were not
accepted as visual evidence. No screenshots were created: automated acceptance
used code inspection, static tests, builds, and runtime logs; the project owner
performed visual acceptance.

Initial audited artifact: `build/output/HallJoy.exe`, 2,228,224 bytes, SHA-256
`6BCBA47D86448E7D262250AEAF7EAFD89FD448CA8A917B1C514711F31FCB6CC3`.
It was not a release candidate until owner review.

## UI ownership map

| Area | Drawing model | Update model |
|---|---|---|
| Six-tab row | subclassed `WC_TABCONTROL`, buffered by `TabDark` | tab interaction/layout only |
| Remap and persistent keyboard preview | child page plus owner-drawn keys | dirty HID regions; page content only when needed |
| Configuration | `CustomPageSurface` plus `PremiumCombo` | static cache plus bounded live-status region |
| Gamepad Tester | buffered dynamic page | visible-tab report/telemetry hash |
| Global Settings | `CustomPageSurface` and themed controls | command, focus, state, and scroll changes |
| Input Overlay | retained custom surface | setting, runtime-state, and scroll changes |
| Mouse Settings | retained custom surface | setting, runtime-state, and scroll changes |

Custom-painted surfaces suppress `WM_ERASEBKGND`; cached presentation respects
`PAINTSTRUCT.rcPaint`; `TabDark` composes its complete row off-screen and presents
it with one `BitBlt`.

## Handoff findings and initial corrections

| ID | Correction | Automated evidence | Status at this stage |
|---|---|---|---|
| UI-01 | hidden Remap no longer invalidates key HWNDs; commits are clipped; tab row is double-buffered | stable Configuration: 10 paints/s, zero erase, exact bounded dirty area; no telemetry-driven tab paint | Implemented; visual pending |
| UI-02 | Factory Reset lost the red strip and GDI `DrawFocusRect`; all states use one themed renderer | rejected markers prohibited by static audit; build passed | Implemented; visual pending |
| UI-03 | visible-tab telemetry polling plus hash-based changes; Configuration invalidates only status | runtime status rectangle evidence and unchanged-hash guard | Implemented; visual pending |
| UI-04 | HE mode and Rows became genuine `PremiumCombo` child controls | dropdown and keyboard semantics present; fake mouse cycling absent | Implemented; interaction pending |
| UI-05 | one route-complete diagnostic builder consumed only by Gamepad Tester | every route field retained; one detailed consumer | Implemented; visual pending |

The UI-audit-only build flag `HallJoyUiAudit=true` enabled paint/erase/dirty-area
aggregation. Production did not contain the instrumentation. A safe Irok/
SparkLink run across Remap, Configuration, and Gamepad Tester showed ten bounded
Configuration paints per second, tester refresh only while visible, 198 precise
Remap-key invalidations from physical input, no input/telemetry-driven tab-row
paint, and clean exit with no crash artifact.

Production and UI-audit builds, the UI static audit, three release-qualification
cycles, and the 15-second analogue simulator passed. At this point owner review
still had to cover rapid physical input without flicker; every Factory Reset
state at 100/125/150/200% DPI; Configuration live refresh, combos, scroll, and
persistence; complete Gamepad Tester diagnostics; all six tabs at minimum and
normal window sizes. MAD68 HE/UAP and Aula remained separate hardware gates.

## V14-12S: refresh root corrections

Owner testing found that the persistent keyboard preview was being invalidated
only on Remap. Backend dirty bits were consumed on every tab, but the key HWND
was refreshed only when Remap was active, losing release transitions on
Configuration, Tester, Global Settings, and Input Overlay. Press and release now
invalidate the matching preview key regardless of the active tab.

Additional cadence/layout corrections:

- Gamepad Tester hashes its compact gamepad report every UI tick; only heavy
  route diagnostics retain a 100 ms cadence;
- Remap, Configuration, and Global Settings coalesce thumb movement toward a
  bounded frame target;
- child controls move in one `DeferWindowPos` batch with no redraw/copy bits,
  followed by one non-erasing invalidation;
- the tab row paints only for pointer feedback or actual selection changes;
- a missing `InputOverlay/StrengthSmoothing` value defaults to 15%, while an
  existing saved value remains authoritative.

Stress instrumentation activated all tabs and dragged scrollbars. After the
correction, Configuration produced 24 paints during burst drag and zero erase;
the tab row painted only once or twice for actual tab changes. Build and 3/3
qualification passed with unchanged user state. Artifact SHA-256:
`1B5671F36EDE9CD2CB1153A2D02729387D0974D25FFB031D457A4FDC7AB523D1`.

### V14-12S.1: low-FPS thumb drag

The first coalescer depended on `WM_TIMER`, which Windows delays while higher
priority mouse input is continuous. A rigid 16 ms deadline could also miss an
event arriving at 15-16 ms, producing roughly 29 FPS.

Remap, Configuration, and Global Settings began committing a due frame directly
from the mouse handler; the timer handles only the last pending frame after input
stops. Cadence follows `UIRefreshMs` in the 8-16 ms range with 1 ms sampling
tolerance. Alternating stress reached about 70 commits/s with zero erase and a
clean shutdown. Build and qualification passed; artifact SHA-256:
`F9A32FEB956E7ED38CE7CB75BBE8254D64B296259821B9922ECA8EFF9D5B040C`.

The owner still rejected S.1 because elements disappeared while scrolling.
Higher commit frequency had exposed, not solved, the split composition model:
some content was painted by the parent while independent child HWNDs had their
own erase/move/paint cycles.

## V14-12T: one retained scrolling architecture

A single contract replaced per-tab scrolling for all six pages:

- `CustomPageScrollController` owns wheel, thumb drag, track paging, capture,
  cancellation, layout coordinates, and hit testing;
- `CustomPageSurface_Present` caches complete retained content, copies the
  viewport, and draws one shared scrollbar;
- scrolling changes only `scrollY`; it never moves visual child HWNDs;
- Remap cards, labels, controls, and icons share one retained layer;
- closed Configuration/Global combo faces are retained; the real
  `PremiumCombo` appears temporarily only as popup/keyboard controller;
- Gamepad Tester keeps its live bars but uses the same controller, coordinates,
  and scrollbar;
- hidden Remap HWNDs are input/drag controllers only and do not participate in
  page layout.

Backup branch: `backup/pre-unified-scroll-architecture-20260802`; backup commit
`63024a9907dc946f4533c94a459fd65acec03df5`; local backup
`build/backups/unified-scroll-architecture-20260802-150715`.

Both static audits, the production build, 6/6 page stress with 240 cycles, and
3/3 release qualification passed. GDI and USER counts remained bounded and no
process survived. Artifact SHA-256:
`851C84A63AB6A1532C21E4FE477A16F9D9248BF4AB76090E3DD9AEAA969C2509`.
Automated status was PASS; visual status remained pending.

### V14-12T.1: retained-control visual regressions

Owner review accepted scrolling but found regressions: the gamepad power icon
looked like text, closed combos looked like wide centered buttons, the unsaved
label contained damaged characters, opening substituted a differently styled
face, and a bright focus border appeared.

Root causes were duplicated simplified renderers, a Unicode multiplication
character instead of the vector power icon, text-based unsaved decoration, an
extra focus frame, and incomplete popup-controller hide paths.

`PremiumCombo::PaintRetainedFace` now renders retained faces through the same
font, alignment, arrow, border, and save-icon rules as the live control.
`PremiumCombo::MsgDropStateChanged` hides the controller after Escape, outside
click, selection, or other close paths. Remap again uses the vector power
renderer; unsaved state uses the normal save icon.

Static audit, full rebuild, six-page scroll stress, combo lifecycle, and 3/3
qualification passed with bounded GDI/USER state. Artifact SHA-256:
`76CB02D76131E72B78652969C3673F3A2E586BCB843FD4ED57B78F51F6FD4677`.

### V14-12T.2: interactive-control semantics

Three root causes were corrected without changing the accepted scroll system.
The gamepad power-button ID range (`3000+`) was incorrectly swallowed by a broad
drag-icon test (`>=2100`); the range is now limited to actual drag icons. Every
remap mutation passes through one transaction that records bindings, marks the
global profile dirty, updates Global Settings, and requests persistence.

Input Overlay Fill direction, Depth display, and Label font cycling buttons were
replaced by real `PremiumCombo` controls. The dirty profile displays
`Global profile — unsaved` and its normal save icon. Static audit, production
build, six-page scroll and popup lifecycle stress passed. Artifact SHA-256:
`8BE26D58294AD7E02D38C0970E4A8F6BD321A976638DC0E809A0425020D714CA`.

### V14-12T.3 and T.4: combo popup wheel behavior

The font popup is a separate top-level `PremiumCombo_Popup`, so it received
`WM_MOUSEWHEEL` without forwarding it to the controller. T.3 routed the original
message to the shared controller, and build plus six-popup runtime stress passed
(artifact SHA-256
`45AEDB2FA1952843B004FED3F80EAD7F18DC8357FAD5EF2D64BBE237A6AC221B`).

Owner review correctly rejected its semantics: the controller called `MoveHot`,
changing selection rather than scrolling the viewport. T.4 added
`ScrollPopupWheel`, which changes only `scrollTop`, only when item count exceeds
visible rows, honors the system wheel-line setting, and accumulates partial
deltas. Neither `curSel` nor `hotIndex` changes.

Runtime state inspection proved that five-item fitting lists kept `maxTop=0` and
selection unchanged, while the 13-font list with `maxTop=3` scrolled its viewport
from 1 to 3 without selection change. Full build and 6/6 stress passed with a
clean exit. Final artifact in this record: 2,233,856 bytes, SHA-256
`ED0082DFDC24F8A4137B1559D1B43058186C19ED4B9142E7F9BEE107F65EB00D`.

No screenshot was created at any stage; final visual acceptance belonged to the
project owner.
