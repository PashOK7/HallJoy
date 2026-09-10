# Concrete QoL findings (inspection only)

## New owner request: layout editor and independent overlay layout

The owner now explicitly requests Save / Discard / Cancel on editor exit with
unsaved changes. This supersedes the earlier no-prompt editor preference below.
Also requested: Input Overlay follows the main HallJoy layout by default, with
an option to select and configure a different layout. Owner clarification: the
preset catalog is shared, selections are independent; the overlay combo includes
"Same as HallJoy". Editing a shared preset updates its consumers, not a private
copy. Implementation and validation: [layout editor / overlay](LAYOUT_EDITOR_OVERLAY_2026-09-09.md).

The remaining text records the pre-implementation inspection, not current status.
That inspection found drafts lost on host/app close and preset
selection/reload; saved presets are shared objects, so editing a preset affects
every consumer of that preset. Overlay currently reads the main render snapshot.
No editor or overlay implementation changes yet in this inspection.

Owner decision: implement item 3 only. Keep the gamepad power action unchanged.
For a future layout-editor change, default must be save-and-exit with no choice
dialog; asking Save/Discard may only be an optional setting. No layout-editor
behavior change is authorized by the current text-edit task. The proposals below
are historical findings, not an approved implementation plan.

No production edits or visual inspections. These are code-backed findings, not
claims of manual UI reproduction. Owner approval is needed before implementation.

1. Additional Remap gamepad cards display a power glyph
   (remap_panel.cpp DrawDisableGamepadPowerGlyph / retained renderer), but the
   remove command immediately calls Bindings_RemovePadAndCompact and saves.
   bindings.cpp shifts later slots and clears the last one. There is no confirmation
   in this command path. Proposal: unambiguous removal affordance and confirmation
   for a slot with bindings; do not call deletion a temporary disable operation.
2. Layout editor owns an in-memory draft, but hasUnsaved is assigned, never queried.
   LayoutEditorHostProc WM_CLOSE unconditionally destroys the window; child
   WM_NCDESTROY deletes the draft. Preset selection/reload calls
   Layout_LoadDraftFromPreset(..., true) without a dirty guard. Proposal: shared
   Save / Discard / Cancel gate on close, preset change and draft reset, including
   main-app shutdown. Failed save must keep the editor/draft open.
3. Input Overlay custom port/hex edit controls are append-only string editors.
   WM_CHAR appends input/paste and Backspace pops the final character. Caret is
   painted at the string end; mouse hit only assigns focusId; WM_KEYDOWN only
   handles Escape. No selection/caret movement/delete-at-caret. A full five-digit
   port or seven-character #RRGGBB prevents pasting a replacement until cleared.
   Proposal: normal editable text behavior within the existing dark UI, with local
   selection, clipboard replacement and explicit invalid-value feedback. No global
   hotkeys. Preserve retained viewport behavior; do not reintroduce repaint races.

Suggested priority: prevent destructive misunderstandings (1), draft loss (2),
then make value editing predictable (3).
