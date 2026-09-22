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
