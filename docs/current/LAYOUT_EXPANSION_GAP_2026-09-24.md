> Follow-up: owner narrowed scope to easy batches; five JingTai presets and automatic selection are now complete. See [implementation and remaining scope](LAYOUT_EASY_BATCH_2026-09-24.md). The startup defect found in the prior JingTai integration is corrected there. Historical audit below.

# Layout gap in recent protocol batches

Owner raised the omission on2026-09-24. Confirmed against the production-linked
compiled catalog at `.local/jingtai-profile-final-20260924.log` and the preceding
same-layout run's `layout-catalog.tsv` (144 catalog entries, not model count).

IROK entries present: MG75 Max ANSI, MG75 Pro ANSI, NA87 Mag ANSI.
The five newly enabled JingTai V1 models do not have exact named presets:
NA87 Pro, ND63, Mercury68, Mercury68 Pro, IYX MU68 Pro.
There are no Game Arena or EWEADN brand entries in that compiled catalog either.
Recent batch documents explicitly relied on manual visual layout selection;
that does not amount to shipping the correct per-model geometry/autoselection.

Analog sensor/HID factory maps and visual geometry are separate. Runtime analog
integration and its yellow status remain valid; missing geometry must not be
represented as complete model UX. The statement that all251 yellow models are
included in the EXE refers to enabled analog paths, not251 complete visual presets.

Next work: reconcile the whole support catalog with compiled presets and exact
automatic identities; extract manufacturer geometry in batches and deduplicate
only proven identical layouts within the existing catalog rules. Do not infer
geometry from key count or sensor matrix positions. No new model geometry or
autoselection was implemented in this audit; no support-status/Sheet change.
