> Before any Sheet edit, read [keyboard Sheet rules](KEYBOARD_SHEET_RULES.md). Owner prohibits cell comments and notes; evidence belongs in project docs.

# Required support-status synchronization

This is an internal completion requirement, not a user support-report guide.
Owner instruction, 2026-09-21: do not forget to update the keyboard spreadsheet
when a support decision changes. Authorization covers corresponding status edits.

## Destinations

- Runtime admission, relevant feature flags and testing notices.
- README.md and SUPPORTED_HARDWARE.md.
- Current implementation/research document and OWNER_CONTEXT.md where decisions change.
- Google Sheet: https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit
  Spreadsheet ID: 1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c.
  Tab Main, sheetId 0; A brand, B model, C support status. Verify live metadata.

## Completion procedure

1. List exact affected models and the latest decision/evidence, including model
   aliases, transport, custom firmware and regional scope. Older audits do not
   override later owner decisions. Do not extend one model's result to a brand.
2. Reconcile runtime behavior and maintained public documentation. Distinguish
   locally implemented support from published builds and actual physical tests.
3. Read live Sheet metadata, locate rows by brand AND model (row numbers move),
   inspect current values and validation. Compare against the latest decision.
4. Apply only necessary status changes with a narrow field mask, preserving
   dropdowns, formatting and neighboring data. Remove obsolete pending-retest
   qualifiers when the corresponding decision has already been superseded.
5. Read back model/status pairs, validation and effective conditional colors.
   Record date, exact models/ranges, old/new status and readback result in the
   current task document. A successful write response alone is insufficient.
6. Reconcile ALL yellow rows with `keyboard_support_notices.json` and runtime notices, not only changed models. Generate with `python tools/support_notice_catalog.py --write`; compare a freshly read Sheet export with `--sheet <snapshot.json>`. Release builds reject a stale generated header. Yellow means a complete enabled input path with specific remaining uncertainties; neither a disabled backend nor unfinished integration qualifies. A physical tester is NOT a prerequisite.
7. Before release publication, repeat this reconciliation for ALL support
   decisions since the previous release, not just the last edited model.

A support task is not fully synchronized until these checks are done. If the
connector is unavailable, retain a specific pending item in OWNER_CONTEXT.md
with model, desired status and reason, and disclose it in the final response.
Do not silently skip the Sheet or claim synchronization succeeded. Resolve any
pending items at the next support/release task with restored access.

Physical-device testing is not an automatic prerequisite for Supported when the
known protocol and implementation are established. Keep technical uncertainty
specific; do not claim that source review was a hardware test.

## Correction verified on 2026-09-21

Missed release decision: X65 Pro ordinary support was already approved and
published in v1.6.0; only the Sheet retained an obsolete Block Bound Keys retest.
Source: docs/current/PRERELEASE_2026-09-21.md and
RELEASE_1.6.0_PUBLICATION_2026-09-21.md.

Inspected ATTACK SHARK rows 74..104. Changed only Main!C93, model X65 Pro HE:
Supported; Block Bound Keys retest pending -> Supported.
Readback Main!A92:C94 confirms exact model, new status, preserved dropdown and
green B93:C93 (RGB .65882355/.8666667/.70980394); neighbors unchanged.
Other family statuses remain consistent with the retained per-model notices.
No runtime change or build was necessary. No automatic background monitoring
was installed; this is a mandatory agent workflow loaded through AGENTS.md.
