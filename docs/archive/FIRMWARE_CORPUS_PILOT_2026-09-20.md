# Firmware corpus and batch research — 2026-09-20

## 2026-09-20 continuation: storage and ZIP recovery hardening

This section supersedes the pilot's ZIP recovery and object-publication limits.
The corpus remains a single-writer CLI; native extraction is not yet migrated to
persistent jobs. No new acquisition, firmware analysis, emulator run, runtime
change, tester EXE or public support change was performed in this continuation.

Architecture findings and implemented fixes:

- Direct writes at the final object path could leave truncated indexed-name files
  after interruption. Objects and reports now use same-directory staging, flush,
  fsync and atomic replacement, checking the target immediately before publication.
  Existing corrupt objects are refused, never silently overwritten. A hard-killed
  process may leave unindexed staging files; cleanup/reconciliation is still pending.
- ZIP retry previously skipped every stored result, including budget exhaustion.
  The new `jobs` table records kind, engine, state, attempts and structured result.
  `zip-v2` resumes `running` and `deferred` jobs on the next invocation. `complete`
  jobs are reused; `blocked` jobs require `expand --retry-blocked`. Unsupported or
  unsafe inputs remain blocked; retries do not bypass validation.
- Seeding every stored object at depth zero lost the nesting limit across runs.
  Depth now comes from the shortest recorded acquisition path, including legacy
  native edges. Origin records and standalone objects without incoming edges are
  roots; rootless cycles remain unresolved. Four extraction levels are allowed.
- ZIP and native extraction now roll back all child index/edge writes on member
  failure. Successful ZIP completion and its child records commit together. Valid
  immutable blobs written before rollback may remain unindexed and can be reused
  after hash validation; they are not reported as acquired firmware.
- ZIP reserves the declared expansion budget before decompression, so failed CRCs
  consume budget too. A container larger than the whole budget is blocked; a
  container that only exceeds the remaining run budget is deferred.
- Extraction and report reads validate object hashes. Report schema is now 2:
  `extraction_results` rows include `(hash, engine, result)` and `job_states`
  aggregates persistent jobs separately from historical analysis records.

Validation: `python -m unittest discover -s tools/tests -p test_firmware_corpus.py`
passes all **20 tests**. New cases cover failed atomic publication, concurrent target
change, CRC rollback, restart after KeyboardInterrupt, deferred budget recovery,
limits across repeated runs and legacy edges, corrupted report input, native
publication failure and failed-CRC budget accounting. Native failure tests mock
7-Zip output; they do not execute firmware or confirm hardware behavior.

Real corpus: `expand` completed 31 ZIP jobs; another invocation left all job attempt
counts at 1 and added no objects or edges. There are still 736 objects, 46 origins,
40 source records, 473 model rows and 85 brands. Edges increased from 43,538 to
43,639 solely because zip-v2 adds 101 versioned lineage records. All original rows
in objects/origins/fetches/inventory/models/edges/analysis are preserved, verified
against the pre-change SQLite backup. `PRAGMA integrity_check` returned `ok`.
No new firmware image was discovered in this hardening pass.

Backup (source, tests, current documents and SQLite online backup):
`.local/backups/firmware-corpus-hardening-20260920-214920/`.
The installed requests stack emits a dependency-version warning; this pass made
no network calls and did not change global Python packages.

Next engineering work: migrate RAR/7z/PE to the same persistent-job lifecycle and
unify cross-format recursion/budgets; bound native output while it is produced;
add reconciliation of unindexed/staging blobs; implement validated Intel HEX
normalization and a firmware-only inventory. Then expand platform source adapters
using the reuse map. Do not describe the ZIP scheduler as a universal extractor or
automatically transfer protocol/emulator conclusions to candidate images.

## Owner scope

Attack Shark and AJAZZ tester feedback is expected tomorrow. In the meantime,
build reusable acquisition/extraction first, then audit analog keyboard firmware
by brand from the public Google Sheet. Avoid repeating research for identical
images and avoid treating command-byte matches as protocol proof.

This is an acquisition pilot, not completed coverage of every brand. No HallJoy
runtime, EXE, support status or Google Sheet cells were changed.

## Implemented foundation

`tools/firmware_corpus.py` stores immutable SHA-256 objects and a SQLite index in
`.local/firmware-corpus`. Source URLs/local import paths, manifest product/version
records, retail models, extraction edges and research references are separate.
Never infer retail-model identity from an OEM product name alone.

The native Sheets snapshot contains 473 records across 85 brands, range
`Main!A1:C558`, spreadsheet `1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c`.
Snapshot: `.local/firmware-corpus-sheet.json`. Blank brand separators are checked;
malformed or duplicate model rows are rejected before replacing the inventory.

- Official Illumi adapter: `https://www.illumipc.com/config/firmware.json`.
- Successful URL downloads are cached; explicit refresh uses ETag/Last-Modified
  when supplied. Errors have a one-day cooldown unless refresh is requested.
- HTTP 429 and selected temporary server failures have bounded retries.
- Individual objects/downloads are limited to 128 MiB.
- ZIP expansion has member-count, size, ratio and nesting checks, with a 512 MiB
  per-invocation expansion budget. CRCs are checked by the ZIP reader.
- RAR/7z use installed `C:/Program Files/7-Zip/7z.exe`, with its hash in each
  extraction edge. Members are read through stdout; vendor paths are never
  written to the filesystem. Helper invocations have timeouts.
- PE parsing extracts RCDATA/custom resources without executing the updater.
  Standard icons/dialogs/fonts are excluded in static-v2. This does not establish
  that all firmware inside an EXE was found; overlays/custom encodings remain.
- A pinned AJAZZ updater adapter validates both the whole EXE hash and the
  extracted 512 KiB image hash. It does not reuse the offset on unknown versions.
- Exact known image hashes link back to existing AJAZZ, MG75, IO and AULA review
  harnesses. A reference is previous research, not a new emulator run.
- Cortex-M vector plausibility is triage only. No analog-support classification
  is produced by strings, command bytes, filenames or vector candidates.

The JSON report contains the brand queue, source errors, extraction results,
candidate vectors and exact-hash research matches. Counts of objects/resources
must never be presented as counts of firmware images or supported keyboards.

## Reproducible commands

Run from the project root with Python 3.10, requests and pefile available:

```powershell
python tools/firmware_corpus.py sheet .local/firmware-corpus-sheet.json
python tools/firmware_corpus.py illumi --download
python tools/firmware_corpus.py expand
python tools/firmware_corpus.py extract-native
python tools/firmware_corpus.py extract-native
python tools/firmware_corpus.py extract-pinned
python tools/firmware_corpus.py report
python -m unittest discover -s tools/tests -p test_firmware_corpus.py
```

Native extraction processes its initial object snapshot; a second pass handles
EXEs extracted from RARs. This is intentionally not an unlimited recursive run.
Only use one writer process. Import explicit research roots/files with `ingest`;
do not ingest all of `.local`, which contains full repository backups, tools and
unrelated dependency binaries. Existing reviewed binaries are local imports,
not newly verified manufacturer downloads.

## Pilot evidence

Illumi manifest: 40 product records; 39 nonempty package references. Downloaded
38 packages successfully; RK7013HEARGB V3.22.08 returns HTTP 404 for the URL in the
manifest. RK858HERGB has an empty file field. Neither result means analog is
absent. Public registry completeness and retail mappings are not established.

Completed pilot: 31 ZIP containers (including one local import) and 46 static-v2
containers (8 RAR + 38 PE), no static-v2 failures. Pinned AJAZZ extraction restored
the expected image and its parent edge. Repeating `illumi --download` added no
objects or extraction edges. Ten focused unit tests pass, including network cache
reuse/error cooldown. SQLite extraction writes are batched per container.

The first real extraction pass exposed an empty-field parsing bug in native
7-Zip listings. Fixed before static-v2 and covered by tests, including CRLF
multi-member listings. Old static-v1 records remain as history. The initial PE
pass also extracted unnecessary UI resources; static-v2 filters standard UI
resource types. Old objects remain content-addressed and must not be counted as
new firmware findings.

## Next source adapters and analysis gates

1. Reuse existing Keychron product API downloader and saved identity maps;
   preserve the distinction between stock images and owner-confirmed custom UAP.
2. Reuse Attack Shark/Sinowealth family registries, including negative API results;
   keep Pro/non-Pro and revision identities separate until proven equivalent.
3. Add SparkLink/IROK/AULA manufacturer manifests and official driver-package
   sources; one platform adapter may serve several brands without merging brands.
4. Audit the remaining 85-brand queue for official downloads/web-app registries.
   Record no-public-source, broken-link, encrypted-package, extractor-missing and
   unresolved-model-link separately. Do not guess download filenames exhaustively.

For the analysis stage, use exact hashes first; then compare executable regions
and normalized functions/call graphs against reviewed families. Similarity only
prioritizes work. Reuse a harness only after validating its dispatcher, address
map, packet serializer and scanner-to-buffer path on the new image. A bootloader
or common USB library must not determine the analog protocol family.

Behavioral suites must test independent simultaneous depths, stationary holds,
release of one key while others remain held, lost/reordered events, full-state
recovery, Fn/remaps, normal keyboard output, numeric scaling and command exit.
Record real MCU code vs mocked peripherals and hardware verification separately.
No universal cross-firmware emulator or automatic function matching is implemented
yet. Current known-image references must not be described as that system.

## Remaining engineering limits

Single-writer CLI, not an unattended service. Atomic publication now prevents
partial final-path writes; pre-existing corruption is detected but not repaired. Custom installers, Intel HEX
normalization and encrypted images still need adapters. Native extractor output
is size-checked after extraction and time-limited, not OS-quota sandboxed. Blocked
native extractions still persist under the engine version; their retry mechanism
remains pending. ZIP uses the persistent jobs described above. Historical manifest versions are kept, but retail
source links and firmware-to-model associations are still to be curated.

Do not publish new support claims from this acquisition pilot.
