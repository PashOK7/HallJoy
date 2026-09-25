# Keyboard batch automation — 2026-09-24

Owner requested larger batches and maximum practical automation. Added an offline RongYuan inventory / reviewed-manifest / guarded-apply tool. Workflow: [KEYBOARD_BATCH_PIPELINE](../development/KEYBOARD_BATCH_PIPELINE.md).

Current vendor corpus: 479 records; 278 held (including already implemented records), 201 revisions eligible for retail-mapping review. These are NOT 201 new keyboard models or guaranteed integrations. Multiple revisions, aliases and unresolved retail identities remain. Inventory: `.local/rongyuan-inventory-20260924-verified.json`.

Preparation coordinates runtime rows, source/admission locks, profile regression count, yellow notices, README, hardware evidence, next notes and row-independent Sheet intent. Apply backs up and rejects stale targets. Unreviewed parent/class behavior remains blocked. Real source evidence, model mapping and normalization still require technical review; SparkLink discovery is not covered by this RongYuan-specific tool.

Validation: 8 regression tests PASS, including all 183 current pinned source maps and a complete synthetic package applied only in a temporary directory. Existing source audit PASS (117 models / 183 revisions); support-notice audit PASS (177 models; no live Sheet check in this task). No application/runtime changes or support-status changes were applied. Existing last synchronized totals remain 177 yellow / 63 green. No Release rebuild, Sheet mutation, hardware test or GitHub publication in this task.
