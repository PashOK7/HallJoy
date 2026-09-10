# Shared layout catalog, independent overlay selection, protected editor drafts

## 2026-09-10: independent input subscriptions

Fixed overlay-only keys (for example a keypad absent from the main preview)
remaining at zero. The backend previously sampled only the main preview's HID
list. Main and explicit overlay now publish independent subscriptions; the tick
reads their immutable, deduplicated union. Follow clears only the overlay's
subscription. Catalog selection/edit/deletion uses RefreshOverlaySnapshot, so
input tracking follows the same changes as geometry. Duplicate keys are sampled
once; no per-tick allocation or extra device transaction for shared keys.

The old mutable array/count publication also allowed a UI writer to overwrite
an in-flight reader's array. Writers now serialize subscription changes and
atomically publish an owning immutable snapshot retained for the whole tick.
Tests cover keypad-only overlay input subscriptions, duplicates, invalid codes,
main clearing/rebuilding, follow removal, old-reader lifetime, and actual K4
overlay catalog selection without changing the main layout. This tests sampling
eligibility, not a physical keyboard or browser visual run.
Backup: .local/backups/overlay-input-union-20260910/.

Validation: .local/overlay-input-union-static.log PASS; native Release x64 build
PASS (existing optional ViGEm PDB warning only). Final production-linked profile
suite PASS: .local/overlay-input-union-profile-final.log, isolated evidence
HJProfileTest-255f8d86853b40d39b0549e36c8bd72f. First attempt timed out with the
installed app running; retry exposed the test using a display caption instead of
the catalog's persisted K4 alias. Corrected the test to use the stable saved-name
alias, then reran successfully. No live user data was modified by these tests.

## Owner contract

The preset list is shared. Main preview and Input Overlay can select different
presets. Overlay's first/default combo item, `Same as HallJoy`, follows the main
preview. Editing a saved preset updates every consumer selecting it; no automatic
copies or duplicate layout files. This supersedes speculation about private copies.
Owner explicitly requested Save / Discard / Cancel with unsaved editor changes,
superseding the earlier default-save-without-prompt preference for this editor.

## Implementation

- Overlay selection persists by name in the existing settings.ini InputOverlay /
  LayoutPresetName key, not catalog index. Empty/missing/unavailable means follow.
  Global profile loads do not overwrite the global overlay choice. Deleting the
  chosen preset returns to follow; deleting another preset does not shift choice.
- Worker threads read an atomic immutable owning layout snapshot. Explicit
  selection snapshots include uniform-spacing geometry and owned label strings.
  UI-thread selection/save/deletion publishes updates; HTTP requests do not copy
  the catalog, mutate the main preset or read mutable name/index state.
- Input Overlay uses the existing retained combo/button styling and a compact
  Edit layout action opening the selected shared preset (effective main when
  following). Narrow widths stack the button. No extra UI polling/timer.
- One draft-resolution gate handles close, app close, preset switch, reload,
  opening a different preset from another page, and protects catalog deletion
  and factory reset. Cancel is the default; dialog failure cancels safely.
  Save failure keeps editor/draft open and reports the failure inline.
- Windows session-end attempts a synchronous atomic draft save without a modal
  prompt; failure vetoes ordinary session termination. Forced termination/power
  loss cannot be prevented by a close confirmation.
- Dirty state compares actual labels/geometry/spacing against the saved preset;
  returning values to their saved state removes the warning. Window title names
  the preset and marks unsaved changes. Capture loss retains dirty tracking.
- Label edits now update the draft immediately (not the saved preset), matching
  geometry editing. Removed redundant Apply Label button and widened its field.
  Editor controls use the standard HallJoy font. Reload button names its actual
  action (`Reload saved`); minimum window size protects the fixed editor sidebar.

Backup: `.local/backups/layout-editor-overlay-20260909/` (previous EXE and affected
production source files). No user presets are changed by automated tests.

## Validation

Production-linked simulator tests use an isolated data root and a private desktop
that is never displayed. They cover actual editor commands/close, Save/Discard/
Cancel, cancelled switch/reload, interrupted drag, live label editing and five
injected save transaction failures. Overlay tests cover follow, independent
selection, immutable snapshot lifetime, saved edits, persistence/profile isolation,
catalog-index shifts and deletion/missing-preset fallback. Existing overlay text
editing and profile-transaction regressions also run. Source guards supplement,
not replace, event tests. No screenshots or visual inspection.

Final validation PASS: Release x64 and simulator builds; complete static suite
(inventory: 488 records); production-linked profile/UI tests, including 2000
overlay selection changes concurrent with snapshot reads, shared uniform-spacing
equivalence, actual overlay combo commands, editor dirty-state reversal/reentrancy
and deletion of an unrelated preset while its neighbour has an open dirty draft.
Deleting an unrelated preset keeps that editor open and preserves draft identity.

Logs: `.local/layout-editor-build.log`, `.local/layout-editor-simulator-build.log`,
`.local/layout-editor-static.log`, `.local/layout-editor-profile-tests.log`.
Isolated result: `HJProfileTest-2d9c31a104ea4776bb1457181921a189` in Windows Temp.
Pre-existing ViGEmClient LNK4099 (missing optional debug PDB) remains unchanged.
No physical keyboard testing, screenshot checks or full native-backend suite was
performed for these UI/storage changes.

Delivered `build/release/HallJoy.exe` SHA256:
`8F69419E48674BD8C707D1F2AE8752F093BC49D358D06726DFA84FA485384676`.
