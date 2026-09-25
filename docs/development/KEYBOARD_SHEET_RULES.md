# Keyboard Google Sheet rules

Owner instructions, updated 2026-09-21. Read before every keyboard catalog or support-status edit.

Target: https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit
Main: A brand, B model, C short support status. Verify live metadata and rows; positions move.

- Do not add cell comments or notes. Do not put research explanations, sources, firmware details or internal identifiers into cells. Store evidence and sources in project documentation.
- Keep the public table simple. Preserve existing styling, dropdown validation, conditional status colors, brand separators and alphabetical model ordering.
- Read current cell values and native metadata before every edit; preserve unrelated data. Verify inserted/changed rows, neighboring rows, validation and effective colors by readback.
- Catalog inclusion is separate from HallJoy compatibility. Manufacturer-announced magnetic models may be included. Sharing a configuration app does not establish analog compatibility.
- Reconcile product catalogs with official driver/HUB lists and announcements so distinct MAX/PRO/Ultra/Air/XS versions are not omitted. Deduplicate naming aliases, colorways and layouts appropriately.
- Supported requires an implemented and justified path. Physical testing of every model is not mandatory, but known unimplemented admission or mapping must not be labeled Supported. Yellow requires a COMPLETE enabled path: detection, independent analog input, bindings and gamepad output. It may indicate remaining range/firmware nuances, never unfinished integration. A tester is NOT an admission prerequisite.
- Keep every yellow row synchronized with `keyboard_support_notices.json` and the generated runtime notices. Compare a freshly read Sheet snapshot using `python tools/support_notice_catalog.py --sheet <snapshot.json>`; do not use stale snapshots to claim a live check.
- Follow [support-status synchronization](SUPPORT_STATUS_SYNC.md) for README, hardware documentation, runtime and Sheet consistency. Do not change compatibility merely because a row was added.
- Owner explicitly requested IO Type 68 Magnetic Pro Wireless catalog-only on 2026-09-21: do not investigate its firmware as part of this task.


## Mandatory structural check after every row insertion (2026-09-24)

Cell values/colors alone do NOT verify layout. Insertion can copy separator
height16 and obsolete top/bottom borders, even when copied cell styles match.
The sheet uses repeated brand values hidden by conditional formatting, NOT
merged brand cells. Do not merge or blank repeated brand values.

- Read native grid data AND rowMetadata/columnMetadata/merges, not just cells.
  Use metadata to bound Main!A1:C<rowCount>. When get_spreadsheet_metadata omits
  these fields, request updatedSpreadsheet with grid data; an idempotent title
  write using its freshly read existing title exposes them. Empty requests are
  rejected by this connector. Never use ungrounded coordinates.
- Back up the native snapshot before structural changes; re-read before applying.
- Insert model rows with explicit height32px minimum (retain taller rows).
  Separators remain16px; never inherit separator height for a model.
- Recompute the entire affected brand block after insertion: medium gray outline
  only, no interior horizontal/vertical borders. Move the former bottom/top edge
  when extending a block. Do not copy border positions from a sample row.
- Copy status validation only to model rows. Separators/unused rows have no
  dropdowns. Preserve model values, validation choices, conditional colors,
  column widths, fonts, wrapping and unrelated layout.
- Run `python tools/check_keyboard_sheet_structure.py <native-snapshot.json>`
  on a fresh full bounded native read after EVERY insertion, even for one model.
  The input is updatedSpreadsheet (or wrapper) with row metadata. Packed backups
  from the2026-09-24 repair are also supported. `--plan <new-file.json>` emits
  repair requests without sending anything. Inspect the plan, re-read live data,
  apply only to an unchanged snapshot, then re-read and require PASS.
- The audit checks all brand block borders, row minimum heights, blank dropdowns,
  status validation, complete model rows and split blocks. Compare before/after
  values, conditional rules/colors, widths and other formatting separately.
- Structure PASS supplements `support_notice_catalog.py --sheet`, never replaces
  support-status synchronization. Do not report successful sync before both pass.

For reviewed multi-model packets, `tools/plan_keyboard_sheet_batch.py` produces an offline insertion/status/explicit-rename plan from a fresh native snapshot. Follow the live comparison and readback procedure in `KEYBOARD_BATCH_PIPELINE.md`; the planner does not write to Google and does not replace support or structure audits.


### Mandatory base and effective color check (2026-09-24 follow-up)

An owner screenshot contradicted otherwise-correct API conditional results: repeated brand labels and missing yellow backgrounds. Exact client cause was not proven. Do not assume conditional rules or effectiveFormat alone establish client-visible formatting.

Keep repeated brand values for row identity, but explicitly materialize base font colors: first row of each block dark, subsequent brand cells white on white. Materialize B:C status background/text colors as well. Keep conditional rules for manual edits. Recompute these base colors after model insertion, rename, status changes or row reordering, including the former first row when a model is inserted above it. `plan_keyboard_sheet_batch.py` now does this automatically. This is an authorized formatting change; preserve all other fields.

`check_keyboard_sheet_structure.py` CLI now ALSO requires `check_keyboard_sheet_presentation.py` checks for both userEnteredFormat and effectiveFormat. Standalone presentation audit is available for diagnosis. An effective-only mismatch must fail even when no base repair is needed. API checks cannot prove browser rendering; never label them visual acceptance. Owner performs the visual check.
