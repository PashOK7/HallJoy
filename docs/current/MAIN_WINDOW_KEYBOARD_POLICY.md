# Main-window keyboard policy / compound rendering — 2026-09-09

Owner report: Enter after selecting a layout reopened its combo. Keyboard testing
must not operate the main UI. Owner explicitly preserved text/number input in
fields opened with the mouse. Backup: `.local/backups/main-keyboard-input-20260909-193036/`.

## Audit and policy

Found PremiumCombo Return/Space/arrows/F4/search keys, native button/check/tab/
trackbar defaults, custom slider arrows, Remap/page Escape handling and
Configuration Ctrl+S, Ctrl+Z/Y, F2 and Shift+Delete commands. No registered
accelerator table or IsDialogMessage loop is used in the main pump.

`main_keyboard_input.h` rejects keyboard/character/system-key messages before
TranslateMessage and DispatchMessage for the main root, descendants and owned
popups. It covers existing and future controls, not just VK_RETURN handlers.
Focused native EDITs are permitted; custom overlay text fields and explicit
Block Bound Keys capture opt in only while their edit/capture state is active
through a registered query message. Configuration's implicit profile shortcut
handler was removed. Mouse operations and mouse modifier gestures stay.

Low-level hooks, digital indicators, analogue readers, controller output,
configured Block Bound Keys shortcut and its private capture messages are not
filtered. Independent Layout Editor and modal dialog roots retain their own
input scope. No timers, polling loops or global keyboard suppression were added.

## Compound Enter

Native window region previously clipped the outer contour, but the interior was
an inset rectangle. Blue fill therefore reached the cutout edge without the
normal margin. The interior now follows the inset six-sided contour, and both
background and analogue/flash fill are clipped to it. Insets shrink safely for
tiny scaled keys; normal rectangles retain the original path. DC state is restored
before labels and markers. Outer HWND region creation remains layout-only.

Overlay's compound path previously had six sharp corners despite rounded ordinary
keys. It now rounds every convex/concave vertex, bounding each radius by adjacent
edge lengths. Inset outlines clamp very thin legs; no NaN or out-of-bounds points.
The existing sprite cache retains the result, so this adds no idle redraw loop.

## Code-only checks

Production Windows event suite: main-policy rejection on real combos/buttons,
owned dropdown, text edit acceptance and separate-editor scope; explicit overlay
edit admission/end-of-edit rejection. Existing configuration persistence and
private-desktop editor tests remain in the suite. Native contour region test
checks the 3 px inner elbow margin over 81 fill depths. Node executes the production
overlay path, checks six rounded corners including the concave vertex and finite
bounded output for narrow/extreme notch sizes. No screenshots or visual inspection.

PASS: native/simulator Release builds (existing ViGEm PDB warning only), all
static audits, Node outline test, Windows profile/event suite
`HJProfileTest-bf6e38a1da7947ee9611d4d11a2cf913` in the temporary directory.
Released SHA256: `4BA397DBB5BDCA52AD00047560BB1E3AAF9CF1EC465151CC7A5E03B494B9F4BE`.

## Follow-up: final compositor must preserve the notch

Owner reported a grey patch covering the adjacent # key. The previous tests
validated the HWND region and analogue interior, but not the final transfer to
an owner-draw DC. The backing bitmap still held a rectangular control background;
the final full-rectangle BitBlt depended on clipping supplied by the destination.
`KeyboardRender_DrawKey` now explicitly excludes the absent corner on the output
DC before either the buffered or fallback path. RAII restores the caller's clip.
Rectangular keys are unchanged; no timer or extra bitmap allocation was added.

A production-renderer regression test fills an unclipped offscreen destination
with sentinel neighbour pixels, renders Enter at nonzero coordinates at four
depths and two selection states, and checks every pixel of the notch. It failed
before the fix (`HJProfileTest-d340c53a1ad043e6a73eacbd500b2dc3`) and passed after
it (`HJProfileTest-324841fdb8764707bb5ae287ebcd21f3`). No manual visual inspection.
Native/simulator builds and static audits PASS. Backup:
`.local/backups/compound-composite-20260909-194217/`.
New release SHA256: `37B378C4901940B31905F3B0229C4DDA820701274986FDE60EE01E2125C9A4D9`.

## Follow-up: native BUTTON paint ownership (2026-09-09)

White notch after a layout switch exposed a missing test boundary: calling only
KeyboardRender_DrawKey omits native BUTTON background painting. A new test uses
the real KeyBtnSubclassProc on a BS_OWNERDRAW BUTTON and sends WM_PRINTCLIENT.
It reproduced notch corruption before the fix (private desktop evidence
`HJProfileTest-af3f18a719ea4e3eb1d36fdf58fdf955`).

The key subclass now owns WM_PAINT, WM_PRINTCLIENT and WM_ERASEBKGND completely,
dispatching one owner-draw request without the native rectangular clear. Native
mouse/capture behavior remains. Existing buffering and the renderer's final
shape clip remain authoritative; no redraw timer or background patch was added.
Selection glow and drag-target outlines follow the six-sided contour. GDI+
impact flash explicitly receives the inner contour rather than relying on GDI
clip inheritance. The isolated renderer alone did not reproduce the user's
white strip; the native-control test did. Do not conflate those findings.

Coverage now includes native erase/print, shape-before-resize order, repeated
rectangular/compound switches at 1x/2x/4x, all missing-corner pixels, analogue
depths, real-HID impact animation, selection and inner elbow margins. The first
post-fix full suite passed at `HJProfileTest-0293093cc4c649f79016add934ccb581`.
Backup: `.local/backups/compound-effects-20260909/`.

Final native/simulator builds, static audits and full Windows suite PASS:
`HJProfileTest-1c5b9c33221e4d059d3cb8f3e7cc6ce9`.
Release SHA256: `EEF65473C8A24590C33715243B3535090506FCF78B5423B05488C1D6AA1E3562`.
