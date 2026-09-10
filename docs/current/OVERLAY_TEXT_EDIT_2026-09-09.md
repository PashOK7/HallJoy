# Input Overlay: retained text editing

Owner scope: improve port and both HEX fields only. Gamepad power/removal and
layout-editor behavior remain unchanged. For future editor exit behavior, the
owner wants save-and-exit by default with no modal choice; a save/discard prompt
may be optional, not mandatory. See QOL_FINDINGS_2026-09-09.md.

Checkpoint: `.local/backups/overlay-edit-20260909/`.

## Behavior

Port / indicator HEX / label HEX use the same bounded ASCII editing model:
mouse caret placement and drag selection, double-click select-all, Shift selection,
Home/End/arrows/Delete/Backspace, local clipboard commands, 32 undo/redo states,
and a standard context menu. Tab/Shift+Tab navigate the enabled value fields and
bring the chosen field into view. Keyboard editing requires the page to actually
own focus and an active field; no global shortcuts are registered.

Whole paste replacements are validated before changing text or selection; invalid
characters and overlong values are rejected atomically, not silently filtered or
truncated. Leading/trailing clipboard whitespace is accepted. Clipboard reads are
bounded and clipboard lock/open failure leaves the source unchanged. Cut removes
selection only after successful copy. Control characters from TranslateMessage
are ignored after their commands were handled by WM_KEYDOWN (no double paste/delete).

Valid values continue to auto-apply/save, preserving existing live behavior. Invalid
or incomplete drafts never enter runtime settings: red field outline plus a small
anchored explanation in the page, no modal dialog. Enter leaves a valid edit;
Escape returns to the last valid runtime value. Hidden/reopened pages refresh from
runtime values. Starting the server with invalid port text focuses that field and
explains the range rather than silently starting on a different fallback port.

## Rendering

Keep the existing retained viewport and font. No visible child edit HWNDs, no edit
timers, and no polling. Selection/caret are painted into the retained page, clipped
to the field; narrow fields horizontally expose the caret. Error feedback is a
viewport overlay which does not change layout or scroll positions. I-beam cursor
applies only to enabled edit fields.

## Tests

Pure production model tests cover replacement, rejected-paste atomicity, selection,
caret boundaries, undo/redo, port and HEX validation, plus 20,000 deterministic
random operations. Static guards cover real-page focus/event/save wiring.
Production-linked simulator tests exercise actual page mouse/key/focus handlers on
a private desktop that is never shown. They do not use physical input or the user's
clipboard, start the server, or write user profiles. Owner evaluates appearance.

Validation PASS: compiled model tests (20,000 operations), complete static suite,
production-linked profile transaction suite including actual overlay edit events,
Release x64 build and main-window startup without error dialogs. No visual inspection.
Logs: `.local/overlay-edit-{static,profile-tests,build}.log`.
The first profile test attempt was blocked before producing test evidence by a
concurrently relaunched HallJoy; after its graceful close, both subsequent runs passed.
Deployed SHA256:
`92E1D9A7673EAF6B31C674941E7444A3EDB7C821EDCC8C3F011701574B47728C`.
