# Layout workbench: low-context workflow

2026-09-09. Start here for another keyboard brand. Do not read the large UI files,
re-investigate supported protocols, or dump vendor bundles to the conversation.
Runtime/editor changes are required only for a genuinely new capability.

## Final pre-release batch

Owner requested Razer/NuPhy/Wooting together, then a stop to further layout work.
See `FINAL_LAYOUT_BATCH.md`: 18 variants, manually selectable, explicit deferred
models. Reviewed-report adapter checks source and report hashes offline; normal
builds need no extraction dependencies. Do not start another brand this release.

## Previous batch: Redragon

See `REDRAGON_LAYOUTS.md`: K673 ANSI/ISO use the same verified W669 identity
publisher. BR layout is pending a concrete official-map discrepancy; analog
support is unchanged. Manifest adapters use an explicit trusted dispatch map.
Append new brands after existing runtime brands to preserve catalog indices.

## Runtime generation is now connected

Follow-up: Aula's three layouts are integrated, including independently extracted
MAX. For manifest-backed adapters, `runtime: true` includes reviewed reports in
the common aggregate catalog. Run `py tools/layout_pipeline.py integrate` after
review: it emits geometry, registration and exact-identity tables for ALL enabled
manifest-backed brands, preserving the old built-in order by appending one shared
include. New models no longer require hand-editing several runtime tables.

The command checks existing content before each replacement and backs up every
changed existing output first. It does not edit user settings or install/build
an EXE. A failed multi-file update is recoverable from backup, not an atomic
multi-file transaction; run check before building. No existing output is deleted.
Legacy adapters remain byte-compatible and are not rewritten by integrate.

Generated identities use deterministic checked nonzero 64-bit tokens; duplicate
tokens and protocol/product selectors are rejected. Native owners publish tokens
only after live-session proof. The common first-run consumer needs no model-specific
changes. A genuinely new protocol still needs a reviewed publisher, not a VID-only
shortcut. This supersedes preparation-only/`integrated=false` notes below for Aula.

## First commands

```powershell
py tools/layout_pipeline.py list
py tools/layout_pipeline.py summary Aula
py tools/layout_pipeline.py check Aula
```

`check` is offline/read-only and prints a compact result. `summary` adds model
names/counts and outstanding evidence. Known adapters: Keychron (34 generated
variants; original K4/Q1 imports remain their existing separate fixtures),
Lemokey (2), DrunkDeer (7), Aula Standard/W669 (2 prepared, not integrated).
Existing generated outputs must match byte-for-byte; no automatic regeneration
hides a changed source. All adapters also pass the shared contour/HID validator.

## Architecture

- `tools/layout_catalog.json`: adapter routing and documentation index; Aula
  demonstrates model/variant, source URL/hash, exact firmware products, supported
  profile, pending/excluded models in one description. No guessed PID fallback.
- Existing adapters retain their tested interpretation and output formats.
  Wrapping them avoids a risky rewrite merely to reduce agent context.
- `layout_aula_w669.py`: converts one driver's schema to normalized reports.
  It verifies the pinned manufacturer file against the actual shipped factory
  map, including nonstandard physical Fn identity. No downloaded code execution.
- `layout_pipeline.py`: shared validation, C++ arrays, preset registration rows,
  verified-product identity rows, INI export, compact checks and isolated stages.
  A new driver needs an adapter; a new model on an existing driver normally needs
  data and evidence, not changes to the renderer or handwritten key coordinates.

## Explicit source acquisition and staging

```powershell
py tools/layout_pipeline.py fetch Aula
py tools/layout_pipeline.py stage Aula --output-dir .local/layout-pipeline/new-aula-review
py tools/layout_pipeline.py check-stage --output-dir .local/layout-pipeline/new-aula-review
```

`fetch` is explicit network access, only for manifest-backed adapters. Existing
sources are hash-checked, never overwritten; a new download must match its reviewed
lock before being stored. Legacy acquisition commands are in each brand's guide.
The normal check never visits a website or reads HID. Sources are cached in repo
research folders so the next chat does not need to download/reverse them again.

Stages use exclusive creation and confined paths. Generated files go only into
the new stage, and `stage.json` is written last as its completion marker. Failed
partial stages cannot pass validation. A stage contains hashes for all files;
missing, unexpected or modified files fail `check-stage`. Checksums detect drift,
not authenticity against a maliciously edited manifest. Existing user files,
settings and the running EXE are never touched by this workbench.

New normalized stage outputs:

- `exports/*.ini`, `*-review.json`: geometry, evidence and provenance;
- `generated/layouts.h`: key arrays;
- `generated/presets.inc`: ready registration initializer rows;
- `generated/identities.inc`: protocol/product/name initializer rows.

This is **semi-automatic preparation**, not an automatic runtime installer. Before
shipping a new protocol family, integrate its verified-session identity into the
existing first-run policy and test the consumer. The generated identity rows must
not be matched against an arbitrary HID marketing name. For W669 the runtime
currently does not expose that proof to first-run selection; that integration
remains separate, explicitly reported as `integrated=false`. No automatic install
button is provided that could bypass this gate. MAX is a different adapter.

## Session discipline / token economy

1. Read this page, the selected brand's short guide, then run summary/check.
2. Work only on `PENDING`/failed evidence; do not rediscover settled facts.
3. Locate exact symbols before reading code; extract selected vendor structures
   with scripts and keep complete bundles on disk, not in conversation output.
4. Stage one brand, run its adapter tests and common checks. Inspect machine
   output/diffs, not keyboard-by-keyboard handwritten edits.
5. Before runtime integration, back up and run production-linked tests; the owner
   assesses visuals. Update the short guide with exact result and remaining gates.

No measured token-reduction percentage is claimed. The saving is the elimination
of repeated source discovery, manual registration emission and repeated validation
implementation; a genuinely new driver's research cannot be eliminated.

## Validation / backup

Pipeline tests are included in `run_native_backend_checks.py`, followed by offline
checks of all four adapters. They cover source lock mismatch, wrong factory,
geometry bounds/overlap, compound notches, duplicate product identities, path
confinement, deterministic generation and stage no-overwrite/integrity.
Existing output checks: Keychron 71 files, Lemokey 5, DrunkDeer 16 unchanged.
Backup: `.local/backups/layout-pipeline-20260909/`. Runtime source/EXE unchanged.
