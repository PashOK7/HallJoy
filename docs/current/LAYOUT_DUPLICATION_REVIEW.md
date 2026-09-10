# Layout duplication review — 2026-09-10

## Owner-approved implementation

Owner subsequently requested consolidation and release preparation. Runtime now
registers one combined preset per reviewed pair: 55 manufacturer presets plus two
technical ones on a clean data root. Names display both models separated by `/`;
filesystem identities use ` + ` (slashes are not safe filenames). Partial equality
is not extrapolated: e.g. a shared ANSI item does not absorb distinct ISO geometry.
Service HID 1027 uses Assistant in the combined presets.

Original source definitions remain as provenance and legacy-file comparators.
Unedited original files stay recoverable on disk but are not loaded as duplicates.
Edited old models (including labels, spacing, row/exact-Y metadata) remain separate
and take precedence over aliases. Original device identity tables and protocols are
unchanged; old saved names resolve to the combined preset unless customized.
Editing/saving/deleting a combined item acts on its one concrete variant/file, not
on a group of independent user files. Built-in deletion retains existing behavior
(default is available again next process); no broad deletion or migration cleanup.

Production-linked tests cover all 13 alias pairs, source geometry equivalence,
overlay alias resolution, real preset-file roundtrips, preserving a one-pixel edit,
label/spacing changes, and exact-name override precedence. Final release evidence
is recorded in RELEASE_PREPARATION_2026-09-10.md. The original review below is historical.

Read-only runtime review requested by owner; no merging approved or implemented.
All 68 manufacturer review JSONs under docs/exports were compared. All eight
brand generators reproduce their checked-in outputs with zero differences.
Technical presets and user-edited layouts are outside this comparison.

Equality ignores source matrix positions/order and compares sorted tuples of HID,
x/y/w/h, notchW/notchY (absent notch fields = zero). Labels compared separately.
This is equality of HallJoy layout data, not hardware/protocol equivalence.

| Models within brand | Equal variants | Labels |
|---|---|---|
| Keychron K2 HE / K3 HE | ANSI, ISO | identical |
| Keychron Q1 HE / Q1 HE 8K | ANSI, ISO | identical |
| Keychron Q3 HE / Q3 HE 8K | ANSI, ISO, JIS | HID 1027: Cortana / Assistant |
| Keychron Q5 HE / Q5 HE 8K | ANSI | identical |
| Keychron Q6 HE / Q6 HE 8K | ANSI | HID 1027: Cortana / Assistant |
| Wooting 60HE / 60HE+ | ANSI, ISO | identical |
| Wooting Two / Two HE | ANSI, ISO | identical |

Original Q1 ANSI report is named Q1 HE ANSI Knob; shipped display name is Q1 HE
ANSI (stored name retains - Imported). Included once, alongside original K4 ANSI
and the 34 generated Keychron variants. Total: 68 entries, 55 distinct within-brand
geometry/HID definitions; 59 if labels must also match exactly. Other six brands
have no within-brand exact duplicates. No same-shape/different-HID pairs found.
Do not extrapolate equality to unlisted ISO/JIS variants or merely similar models.

Recommended implementation, pending owner direction:

- Share immutable identical geometry; retain model identities, provenance and
  exact device selectors separately. Do not change working analog protocols.
- Optionally show combined model labels (e.g. 60HE / 60HE+) with existing variant
  picker, retaining all model names for discoverability. Partial variant equality
  requires explicit variant-to-model metadata, not automatic suffix stripping.
- Preserve saved preset-name aliases, independent overlay choice, and first-run
  matching. Current FindPresetByName resolves exact names and user files replace
  matching built-ins, so deleting duplicate registrations is not sufficient.
- Never collapse divergent user edits. Define editor save/delete semantics before
  exposing a combined model as one editable item. Different service-key labels
  need a deliberate display decision, not silent data loss.

Release EXE and user settings unchanged; no hardware or visual tests performed.
