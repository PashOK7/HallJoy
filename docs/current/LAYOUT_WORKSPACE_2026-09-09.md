# Canvas-first layout editor

Owner request: replace the technical control panel with a usable visual editor.
Follow-up requirement: preserve exact layout-pixel coordinates and dimensions;
keep ordinary row-based editing, plus a precise vertical mode (e.g. arrow keys
offset by 23 pixels, half the old 46-pixel row pitch).

Implemented:
- Main toolbar: preset, Undo/Redo, Fit, Add key, Save; visible save state.
- Canvas on the left, selected-key properties on the right; no technical list.
- Persistent viewport transform, mouse-centred wheel zoom, middle-button pan,
  100% view. Moving/resizing keys does not recompute the fit during the gesture.
- Bounded 64-operation owning undo history; one drag is one action; consecutive
  edits to a text field are grouped. Selection is restored with undo/redo.
- Row mode preserved. Exact Y is optional on KeyDef (-1 retains old row-derived
  Y); optional Yn INI entries preserve existing escaped Kn label format. Existing
  files load without migration. Precise mode preserves row grouping and allows
  independent Y from 0 to 4000; numeric dimensions keep existing 18..600 limits.
- Main preview, editor and browser overlay use the same effective Y. Browser
  layout cache identity includes Y, not only the legacy row.
- Snap-to-8-pixel grid is optional and off by default; numeric values bypass
  snapping. Arrow movement is one pixel in precise mode, one row in row mode;
  Shift increases the step. Width adjustments via brackets now use one pixel.
- Manual X positioning materializes existing automatic spacing before disabling
  it, preserving other keys' displayed positions. Exact Y does not regroup rows.
- Save/Discard/Cancel and transactional shared-preset safeguards remain in force.
- Resizing uses a selected key's bottom-right corner; numeric width/height remain
  available for exact single-pixel sizing. Duplicate key creates an unassigned copy.
- Changing rows preserves an existing precise offset (23 pixels remains 23 pixels
  relative to the new row). Merely switching vertical modes changes no geometry.

Validation:
- Production Release x64 and simulator builds passed (existing optional ViGEm
  PDB warning only).
- Portable model tests passed: bounded/coalesced/branched history, row offsets,
  and 20,000 cursor-anchored zoom checks.
- Production-linked Windows tests passed: actual editor control events, exact-Y
  file roundtrip, legacy files, malformed coordinates, drag/resize undo, stable
  viewport, pan without mutation, overlay selection, profile persistence and
  all five injected atomic-save failure stages. Tests use a private desktop and
  isolated data root; no real keyboard protocols are exercised by these tests.
- All static audits and the source inventory check passed. Full native backend
  suite was not rerun for this UI change.

Scope: single-key editing; multi-selection and group alignment are not implemented.

Follow-up: pixel grid and themed confirmation
- Grid uses 1-layout-pixel minor lines (0.7 screen pixels, low opacity) and
  stronger 5-layout-pixel lines (1.1 screen pixels). Anchored to layout origin,
  unaffected by snapping settings. Minor lines fade in between 150% and 300%;
  below that density they are hidden to avoid solid shading/moire. Major spacing
  increases by factors of five only when projected separation falls below 4px.
  Visible-range integer indexing bounds drawing work and avoids accumulated drift.
- Save confirmation replaces the stock TaskDialog with a modal Win32 dialog
  using HallJoy palette, font, rounded buttons and dark title bar. Cancel remains
  the default; Escape/close cancel, creation failure cancels. Native modal loop
  owns keyboard navigation and owner disabling. No polling or animation timers.
- Simulator decisions now dispatch commands through the real modal dialog instead
  of bypassing it. Portable grid tests cover the supported 5%..800% zoom range.
- Checkpoint before these changes: `.local/backups/layout-pixel-grid-20260909/`.

Checkpoint: `.local/backups/layout-workspace-20260909/`.
Additional checkpoint: `.local/backups/layout-guides-20260909/`.
No visual inspection/screenshots; validation uses code and automated tests only.

Follow-up: edge resizing, rulers and guides
- Selected keys resize from side/bottom edges and corners; exact-Y mode also
  enables top edges/corners. Opposite edges remain anchored, including at size
  and coordinate limits. Side-only resizing never changes height, and vice versa.
- Maximum zoom is now 6400% (64 screen pixels per layout pixel); pointer anchoring
  and pixel grid use the same viewport transform.
- Top/left pixel rulers create horizontal/vertical guides by dragging onto canvas.
  Guides can be moved, dragged outside to delete, or deleted with right-click.
  Escape/capture loss cancels a guide gesture. Up to 64 guides per editor session;
  these are non-persistent editing aids, cleared on preset load/reload, and do not
  dirty a layout or enter geometry Undo history.
- Key edges and integer centres snap to guides within 6 screen pixels (capped at
  8 layout pixels). Resizing edges also snap. Alt temporarily bypasses guide/grid
  snapping; numeric edits remain exact. Row-mode Y candidates must respect the
  existing row offset and permitted rows, never silently switching to exact Y.
- Manual resize materializes automatic spacing once before free editing. If
  automatic spacing exceeds the existing 4000px coordinate domain, dragging is
  refused with an explanation instead of truncating neighbouring positions.
- No timers, global hotkeys or backend changes. Ruler work scales with visible
  ticks, not total document size; zoom/pan invalidates rulers with the canvas.

Delivery: `build/release/HallJoy.exe`, SHA256
`545F9918980B8B343C25DC5FA6F75C2F8A62D6535819F1A44FADF11B6989EA5A`.
Final Windows test evidence:
`C:\Users\PC\AppData\Local\Temp\HJProfileTest-95fc07edccc7443795847f20f10a56aa`.
Both builds, portable model/grid tests, static audits and production-linked
Windows tests passed again after the pixel-grid/themed-dialog follow-up.

Edge/ruler follow-up delivery status: DEPLOYED, hash verified against native build
and rollback copy verified before replacement. Final production-linked Windows
tests passed, including all eight resize handles, Undo, ruler-created guides,
guide cancellation/deletion and unchanged document state. Both builds, portable
model tests (all eight handles, 16,008 delta cases, guide snapping and 20,000 zoom
anchors) and static audits pass. An earlier Windows attempt did not enter tests:
the old HallJoy owned the singleton with a dirty Generic 100% ANSI draft and open
save confirmation. Historical empty test root:
`C:\Users\PC\AppData\Local\Temp\HJProfileTest-84cd923d3d2d4d959232b7880706f45b`.
Owner subsequently authorized force-closing HallJoy during the current testing
session: drafts are disposable tests, not work to preserve. This supersedes the
previous pause for approval; target only verified HallJoy processes. The user
closed the old instance; tests then passed without needing forced termination.

Follow-up: wheel zoom during gestures and owner-draw coverage
- Wheel is accepted during key move/resize and guide creation/movement. The
  camera remains cursor-anchored. Key gestures rebase their origin and fractional
  pointer offset against current geometry after zoom; no geometry/history write
  occurs on wheel. Resize keeps the opposite edge fixed and remains one Undo.
  Middle-button panning still owns its gesture independently.
- Owner-drawn editor buttons now clear the complete rectangle to the panel
  background before rounded/alpha drawing. Previously uncovered corners retained
  native background pixels; disabled blending could depend on a previous frame.
- Production-linked tests cover captured wheel input, stationary pointer after
  zoom, continued drag, Undo, guide cancellation and offscreen pixel assertions
  for normal/disabled/focused/pressed buttons rendered over different backgrounds.
  No screenshots or human visual inspection were performed.
- Checkpoint: `.local/backups/layout-drag-zoom-20260909/`.
- Windows tests passed; evidence:
  `C:\Users\PC\AppData\Local\Temp\HJProfileTest-a00afbce01d848e49ef06dcc61b06eef`.
- Both builds and static audits passed; delivered `build/release/HallJoy.exe`
  SHA256 `0D13B19027A969289959CBF8F8F0E0B15E73F6DE7D32F7C4AA3F4E71FF473B97`
  supersedes the earlier delivery above. Rollback hash verified before replacement.

Four-edge helper follow-up: added matching dashed lines at the right and bottom
edges during selected-key dragging/resizing. Same clipping, style and visibility
as the original left/top lines; no input, geometry or snapping changes.
Production build and focused static audit passed; runtime suite not rerun for
these two additional drawing calls. Rollback: `.local/backups/layout-four-guides-20260909/`.
Latest delivered SHA256:
`3EC2686B901C9D29FC827CE556880B1B34E511D940BA7C321D6950FD0E249515`.

Row conversion, release repaint and button feedback follow-up
- Owner supersedes the earlier "mode switch preserves offset" requirement:
  switching to keyboard rows now snaps the selected key to its nearest supported
  row (halfway rounds down the screen), clears explicit Y, and records one Undo.
  Other keys are not moved. Switching to exact pixels remains non-mutating.
- Mouse-up and capture loss explicitly invalidate the canvas after clearing drag
  state. Dashed helper lines no longer remain until a later pan/zoom. No timers.
- Editor buttons no longer use the white XOR focus rectangle. Native pressed
  state changes fill (now also for Save); keyboard focus uses a subtle accent
  along the button bottom, respecting hidden focus cues and disabled state.
- Production-linked Windows tests passed, including nearest-row conversion/Undo,
  update-region assertions on release/capture loss, gesture zoom and button
  paint coverage. Evidence:
  `C:\Users\PC\AppData\Local\Temp\HJProfileTest-281e0565a163413e85509f3b25155b6a`.
- Checkpoint: `.local/backups/layout-row-release-20260909/`.
- Both builds and static audits passed. Delivered release SHA256:
  `7F97B74F8ACA7E249BED95A6ABD140DC92C04B1A2E07173DE380300E9EC78349`.

Repeated-click follow-up: owner-drawn native BUTTON sends BN_DOUBLECLICKED on
the second press, while editor actions accept BN_CLICKED. A scoped button
subclass now routes WM_LBUTTONDBLCLK through native WM_LBUTTONDOWN handling;
actions still occur only on release, with capture cancellation preserved. No
cooldown, debounce or system double-click setting changes. Covers all 13 editor
buttons. Windows event tests passed with eight alternating normal/double presses
and a cancelled press, alongside existing persistence/editor checks. Evidence:
`C:\Users\PC\AppData\Local\Temp\HJProfileTest-df4f49bdab1f48a0b74a53dc80ee0f60`.
Rollback: `.local/backups/layout-repeat-click-20260909/`.
Both builds and focused static audit passed. Delivered SHA256:
`7896CC0658E61159C836B0AF94CDB43F70718DD18D29876513087F62ADB5DE19`.

Catalog management moved to the layout editor
- Editor preset combo owns Create New Layout (inline name) and per-item delete.
  Delete retains two-click confirmation / Shift bypass and the dirty-draft gate.
  Deleting the edited preset keeps the editor open on a surviving preset;
  deleting another preset preserves the current draft and corrects its index.
- New presets clone the editor's saved source after resolving pending changes,
  not the unrelated main-preview selection. Creation is atomic and does not
  change the main or overlay selection. Existing two-argument CreatePreset callers
  retain their original active-clone/select semantics through defaults.
- Global settings combo is selection-only, including its last real item. Its
  management handlers are removed; catalog notifications refresh its retained
  face/list, even when the editor remains open. No polling was added.
- Editor activation no longer blindly clears the combo (which removed management
  rows and could interrupt inline editing). Refresh only when needed and safe.
- Windows tests passed through actual combo selection, inline edit commit and
  delete events, including Cancel, Discard, source identity, independent main
  selection and keeping the editor open. Evidence:
  `C:\Users\PC\AppData\Local\Temp\HJProfileTest-cb06a909e670419bafaa542168129894`.
- Checkpoint: `.local/backups/layout-catalog-editor-20260909/`.
- Both builds and static audits passed. Delivered release SHA256:
  `C598B1D77BE60FF6EB72B4C54B18593986E0677945064801284F2664BA2FA34F`.

K4 HE built-in geometry correction from owner's saved layout (2026-09-09):
compared all 100 keys; only Enter width 97 -> 96 and right Shift width 74 -> 75
differed. Promoted those values into g_keychronK4HeKeys. Existing user presets,
selected layout and geometry revision are unchanged; no migration overwrites
custom layouts. Source INI and prior binary checkpoint:
`.local/backups/k4he-default-20260909/`.
Production build and focused geometry assertions passed. Delivered SHA256:
`E0D006D570D02DAB5DFBA0EE4857DB54EF8FF0365F019642B00C1FD03A67BCFF`.

Inline delete confirmation (supersedes editor two-click/Shift confirmation):
- Clicking a layout's trash icon keeps its name in the same tinted popup row,
  replacing icons with red Delete and neutral Cancel buttons. Confirmation is
  explicit, not a timed repeat click; no status/footer instruction or timer.
- Cancel occupies the rightmost area where the trash icon was, so a rapid second
  click at the old location cannot confirm destruction. Actual deletion accepts
  only ConfirmDelete for the currently armed row. Closing/rebuilding the popup
  clears confirmation; stale queued confirmations are rejected. Dirty-draft
  Save/Discard/Cancel still applies after confirming deletion.
- PremiumCombo support is opt-in; other profile combos retain their existing
  behavior. Popup drawing and hit-testing share action rectangles. Header enum
  additions preserve old numeric notification values.
- Windows tests passed, including both action hit targets, close cancellation,
  stale confirmation rejection, unsaved-draft cancellation and successful delete.
  Evidence: `C:\Users\PC\AppData\Local\Temp\HJProfileTest-1d3f0ac8c7ef4a9e8a56db0a4b10988b`.
- Checkpoint: `.local/backups/layout-inline-delete-20260909/`.
- Both builds and static audits passed. Delivered release SHA256:
  `481A893D023ED8006AB5120DB7E2C4EE95CE564C3D166E291E527B8254975D01`.

Label-only follow-up: Global settings launcher is now "Layout editor" in both
the retained drawing and underlying button text. Input Overlay uses the same
"Layout editor" label. No behavior changes.
Production build passed. Delivered SHA256:
`FB8E5EBDDFFB36DBB1C71A76283AE324AB488F6FB77349BE1B86FD6DD38B2F72`.
Rollback: `.local/backups/layout-editor-label-20260909/`.
