# Offline keyboard batch pipeline

Use `tools/rongyuan_batch.py` for future batches on the reviewed RongYuan stream family. It treats downloaded vendor files as data and never executes vendor JavaScript. It does not emulate firmware or prove hardware behavior. SparkLink and unrelated protocols still use their existing separate research workflows.

## Inventory

```powershell
python tools/rongyuan_batch.py inventory --catalog .local/research-expansion-20260924/vendor_profiles_womier.json --archive .local/research-expansion-20260924/WOMIER-3.2.15-technical-js.zip --out .local/inventory-new.json
```

The inventory checks exact board/VID/PID identities, existing stream profiles, conservative references in other backend headers, duplicate/placeholder identities, magnetic flags, hybrid-key hints, loader uniqueness, pinned parent bytes, literal 512-byte maps, and known class behavior. Unknown overrides are held. `held` includes already implemented devices, not just rejected candidates. A `review_retail_mapping` row is an OEM revision requiring review, not a supported retail model.

## Review and prepare one large packet

Collect as many independently established mappings as practical in a JSON list. Each entry requires:

- `id`, `vid`, `pid`: exact numeric catalog identity.
- `source_sha256`: hash from inventory, checked again during preparation.
- `brand`, `model`: reviewed retail names; resolve regional variants and mechanical namesakes.
- `range_um`: usable analog normalization in micrometres.
- `evidence`: concise evidence reference for model/transport compatibility.
- `range_basis`: source of normalization, explicitly recording provisional estimates where applicable.

Review these facts from sources; the tool checks field presence, not whether written evidence is true. Do not promote an entire brand or count OEM aliases as new models. No mandatory physical tester gate is introduced. Experimental support must remain fully usable.

```powershell
python tools/rongyuan_batch.py prepare --catalog .local/research-expansion-20260924/vendor_profiles_womier.json --archive .local/research-expansion-20260924/WOMIER-3.2.15-technical-js.zip --manifest .local/reviewed-batch.json --out .local/prepared-batch.json
```

Preparation does not edit application files. The package includes exact before/after hashes and proposed content for profiles, pinned sources/admissions, runtime profile rows, regression revision count, notice catalog/generated header when changed, experimental README table, hardware evidence, and next patch notes. It also lists intended Sheet statuses. Existing formatting anchors must match; future document format changes may require adapting the generator.

Review the package, including names, ranges, README ordering and source evidence. The agent performs this technical review without asking the owner to reconfirm already authorized routine integrations.

## Apply and finish

```powershell
python tools/rongyuan_batch.py apply --package .local/prepared-batch.json --backup .local/before-reviewed-batch.zip
python tools/check_rongyuan_stream_profiles.py
python tools/support_notice_catalog.py
```

Apply checks every target hash before any writes, creates a new backup ZIP, then checks each target again immediately before writing. Backup includes original files and the full manifest; entries with a null before hash were newly created. A concurrent change during writes stops with an explicit partial-write warning: this is not an atomic multi-file transaction or automatic rollback. Inspect changes and backup before recovery; do not overwrite concurrent edits. Output and backup paths must be new.

Then run the relevant native regressions and ordinary `tools/build_release.ps1` with its six gates. Follow [support synchronization](SUPPORT_STATUS_SYNC.md) and [Sheet rules](KEYBOARD_SHEET_RULES.md): fresh live read/backup, locate brand/model pairs, apply minimal changes, read back validation/colors and audit block structure plus all yellow statuses. `sheet_intent` is not a completed Sheet update and contains no stale row numbers. Do not add cell comments or notes. Record the batch in current documentation and OWNER_CONTEXT. Do not publish without authorization.

## Pipeline tests

```powershell
python -m unittest discover -s tools -p test_rongyuan_batch.py -v
```

Generic safety tests always run. Corpus tests additionally use the local vendor archive when available: all pinned source maps, stale evidence hashes, duplicate identities, and preparation/application inside a temporary directory. They never enable a synthetic model in the working application.

2026-09-24: inventory also holds explicit mechanical-switch options; README additions sort models within their brand. First real generated packet: [batch6](../current/RONGYUAN_BATCH_6_2026-09-24.md). Sheet structure audit accepts omitted empty CellData while still rejecting incomplete model rows.


## Expanded parent review and native Sheet planning (batch7)

The stream source gate now also recognizes three pinned updater-only subclasses of the same Common parent. Reviewed model overrides require an exact source SHA256 and parent in `reviewed-overrides.json`; a changed method invalidates admission. Do not add a parent or override merely because its name resembles an existing one.

After preparing an application package, create a Sheet plan against a fresh native snapshot:

```powershell
python tools/plan_keyboard_sheet_batch.py --snapshot .local/fresh-native.packed.json --package .local/prepared-batch.json --out .local/sheet-plan.json
```

An optional `--renames` file is a reviewed list of `{ "brand": "...", "from": "...", "to": "..." }` objects. Renames must not be inferred from fuzzy matches. The plan contains requests, old-to-new row mapping, expected models and a snapshot hash. It performs no network writes. Review it, re-read the live native resource and require an unchanged snapshot before applying. Do not treat a saved plan as permanently authorized against future Sheet changes.

The planner preserves existing cells, inserts natural model ordering within brand blocks, sets explicit heights/validation, and recomputes outlines. It rejects pre-existing structural problems; repair those separately. After application, independently read back and require both native structure and all-yellow checks, plus expected-model and unchanged-neighbor comparison. Tests: `python -m unittest discover -s tools -p test_keyboard_sheet_batch.py -v`.

Batch7 completed50 exact revisions in one application packet and37 inserted Sheet rows in one native batch. [Whole-catalog dispositions](../research/rongyuan-stream/catalog-review-20260924.json) distinguish records from retail models and preserve unresolved aliases/source holds. Refresh this ledger when those conclusions change.


### Mandatory base and effective color check (2026-09-24 follow-up)

An owner screenshot contradicted otherwise-correct API conditional results: repeated brand labels and missing yellow backgrounds. Exact client cause was not proven. Do not assume conditional rules or effectiveFormat alone establish client-visible formatting.

Keep repeated brand values for row identity, but explicitly materialize base font colors: first row of each block dark, subsequent brand cells white on white. Materialize B:C status background/text colors as well. Keep conditional rules for manual edits. Recompute these base colors after model insertion, rename, status changes or row reordering, including the former first row when a model is inserted above it. `plan_keyboard_sheet_batch.py` now does this automatically. This is an authorized formatting change; preserve all other fields.

`check_keyboard_sheet_structure.py` CLI now ALSO requires `check_keyboard_sheet_presentation.py` checks for both userEnteredFormat and effectiveFormat. Standalone presentation audit is available for diagnosis. An effective-only mismatch must fail even when no base repair is needed. API checks cannot prove browser rendering; never label them visual acceptance. Owner performs the visual check.
