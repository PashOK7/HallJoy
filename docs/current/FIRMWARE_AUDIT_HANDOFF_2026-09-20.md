# Firmware audit handoff — 2026-09-20

## Latest continuation checkpoint — foundation accepted

Owner prioritized preparation before broad collection and authorized autonomous
work until a consequential question needs input. Read the current
[FIRMWARE_FOUNDATION.md](FIRMWARE_FOUNDATION.md) and
[FIRMWARE_CORPUS.md](FIRMWARE_CORPUS.md) before the historical pilot below.

Shared extraction jobs, bounded native output, process-crash recovery, image/source/
evidence layers, source snapshots and exact-image emulator dispatch are implemented.
73 tests PASS in `.local/firmware-corpus-venv/Scripts/python.exe`; wheel hashes are
pinned in `tools/firmware-requirements-win-py310.lock`. No global package changes.

Current corpus: 766 objects, 4 image identities, 90 historical platform records,
81 complete jobs under current extractor identities. All baseline data is preserved.
Clean integrity audit; repeating extraction and emulator dispatch adds no rows.
The no-light AJAZZ existing suite actually ran with real MCU code and mocked inputs;
its receipt is in the current runbook. No tester RGB/hardware conclusion follows.

The first foundation is ready for controlled extension; remaining work is explicit:
live registry adapters beyond Illumi, evidence-backed retail associations, additional
payload formats and verified family-specific analysis. Function matching/universal
emulation and OS isolation of Python parsers are not implemented. Do not claim
whole-catalog support or treat historical source errors as no-analog findings.

No runtime/EXE/Sheet changes. The sections below preserve the ORIGINAL pilot state
unless superseded above. Native snapshot/retry limitations below are now historical.

## Read first; do not restart the investigation

Workspace: `W:/github/HallJoy/HallJoy-main`.
Read `OWNER_CONTEXT.md`, `../README.md`, this handoff, then
`FIRMWARE_CORPUS.md`. Read individual keyboard reviews only when needed.
The owner requested a fresh chat to reduce irrelevant context, not to reset work.

## Owner requirements that must survive the handoff

- Build a systematic audit of the analog keyboards in the Google Sheet, organized
  by brand. Aim to acquire all publicly available relevant firmware, including
  revisions. The catalog includes Hall Effect, TMR and other genuine analog
  sensing technologies; do not narrow it to Hall Effect without authorization.
- Acquisition and extraction automation comes first. Do not spend model tokens
  manually downloading and unpacking every keyboard's firmware in turn.
- Build the system properly: solve recurring causes with reusable components.
  Do not accumulate one-off scripts, fragile guesses or patches that merely hide
  failures. The current pilot is a starting point to review and improve, not an
  architecture the owner has declared final or ideal.
- Do not manually reverse every image from scratch. Deduplicate identical payloads,
  identify related revisions, and reuse verified protocol knowledge and emulation
  harnesses. Human/agent investigation should address a genuinely new format,
  family, changed function or unresolved behavior, then encode the discovery in
  reusable tooling. Novel work is allowed; repeating identical work is not.
- No naive protocol searches. Finding a command byte, string, USB ID, filename,
  controller name or a familiar library is not evidence that the desired analog
  protocol exists or works. Text searches may locate documentation/configuration
  sources, but cannot substitute for executable-code and behavioral analysis.
- Optimize token use: scripts do bulk fetching, parsing, hashing, extraction,
  comparison, caching and report aggregation. Bring compact actionable differences
  and failures into context, not entire bundles, disassemblies or repetitive logs.
- Reuse our actual firmware emulation work. Distinguish real executed MCU code
  from mocked peripherals and distinguish both from hardware confirmation.
- Never silently convert uncertainty into support or lack of support. A failed
  download, unresolved extraction or unsuccessful search is not proof of no analog.
- Preserve exact model/revision identity and provenance. Shared OEM protocols do
  not justify merging different retail brands or treating all their devices alike.
- Do not substitute digital key presses for analog depth. The owner forbids
  simulated analog/digital fallback in the product. Emulator fixtures are research
  tools, not evidence from a physical keyboard.
- Do not require the owner to repeat settled decisions. Ask about genuinely missing
  consequential choices; make routine engineering decisions independently.

## Active task and boundaries

Attack Shark and AJAZZ testers were expected to test on 2026-09-21. Await their
results; current work is the firmware corpus, not another speculative tester EXE.
Do not close/rebuild HallJoy just to work on the corpus. Do not publish a release
or change Google Sheet support statuses based on acquisition/fingerprints.

Communicate with the owner in Russian. Code, comments, logs and project documents
are English. Maintain documentation. Before overwriting a file, read/check its
current state; take backups before broad changes. No visual UI test runs: owner
evaluates visuals, agent checks code and meaningful automated tests.

## Source of scope

Google Sheet:
https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit

Last native API snapshot: `Main!A1:C558`, 473 records / 85 brands. Columns are
Brand, Model, Support status. Brand appears only at the start of its block, with
blank separator rows. Local snapshot: `.local/firmware-corpus-sheet.json`.
Use native Google Drive/Sheets tools and their skills for future live reads/edits,
not browser automation. This turn did not modify the Sheet. Read it fresh before
any future mutation; do not overwrite newer owner changes from the snapshot.

## Existing implementation and measured result

- CLI: `tools/firmware_corpus.py`.
- Tests: `tools/tests/test_firmware_corpus.py`; last run: 10 passed.
- Store: `.local/firmware-corpus/objects/<prefix>/<sha256>`.
- Index: `.local/firmware-corpus/index.sqlite`.
- Generated report: `.local/firmware-corpus/report.json`.
- Detailed commands, limits and roadmap: `FIRMWARE_CORPUS.md`.

Implemented: SHA-256 deduplication, separate source/model/extraction records,
download caching/conditional refresh, failure cooldown, bounded archive expansion,
static PE resource extraction, exact-hash links to prior research, a pinned AJAZZ
payload extractor, and structural Cortex-M candidate triage.

Official Illumi registry had 40 product records: 39 package references, 38
successful package downloads (~88.9 MB). RK7013HEARGB V3.22.08 reference returned
404; RK858HERGB had no file. Completed extraction processed 31 ZIP containers
(including one local import), 8 RARs and 38 PEs. Static-v2 had no failures.
This means those extraction stages completed, not that every embedded firmware
was recovered. Repeated download invocation added no objects or edges.

The report currently contains 736 objects and 43,538 extraction edges, including
historical UI-resource extraction and repeat lineage under different engine
versions. These are NOT counts of firmware images! Static-v1 history is retained;
static-v2 fixed native listing parsing and excluded standard UI resource types.
Four imported images matched prior research references (AJAZZ, MG75, two IO HEXs).
Known-hash references do not mean their emulator tests ran again this turn.

Verified AJAZZ lineage:
- Updater SHA256: `9b4529df72210440abe8c9f7685e55129fbb7f5c6f6ff67fc70cdd0f76f2e073`.
- Payload offset `0x2ce808`, size 524288.
- Payload SHA256: `fd42c6f691e353a4411c6c13023f7ee3eccd7726bb7b3ac28ee4fe62df24f680`.
- No-light SG8994HE V1.13.02, NOT the tester's SG8994HERGB revision.
The adapter checks both hashes; do not reuse this offset on arbitrary updaters.

## What is not implemented yet

There is no universal firmware analyzer, automated function matching, reusable
cross-version emulator dispatcher, complete brand/source mapping, or verified
coverage of all firmware. Current Cortex-M vector heuristics are only triage.
ZIP/RAR/PE-resource extraction cannot recover every custom embedded/encoded image.
Intel HEX normalization, custom installers and additional vendor adapters remain.

The pilot is single-writer. Native extraction processes a snapshot, so nested
containers may need another pass. Review retry/versioning, atomic writes and
interrupted recovery, recursion/budgets across stages, redundant resource work,
and extraction completeness before treating it as an unattended bulk service.
Do not simply raise limits or report every extracted resource as firmware.

## Recommended continuation

1. Read the existing code and compact report; audit the collection architecture
   against the requirements above. Preserve the corpus and provenance. Fix shared
   mechanisms instead of adding manual per-file workarounds.
2. Build source adapters around platform registries rather than one downloader per
   retail model. Reuse existing Keychron, Attack Shark and SparkLink/IROK/AULA
   metadata. Keep official source evidence and retail identity mappings explicit.
3. Turn extraction into resumable typed jobs with verified payload boundaries and
   clear unresolved states. Use package structure, MCU layout and validated format
   parsers; exact-hash adapters are useful seeds, not the general solution.
4. Build a firmware-only inventory distinct from packages/resources. Group exact
   duplicates, then related executable regions/functions. Exclude shared bootloaders
   and USB libraries as the basis for declaring a shared analog protocol.
5. Adapt existing emulator suites to verified families. Validate dispatchers,
   address maps, scanner-to-buffer paths and serializers before transferring tests.
   Test independent simultaneous keys, stationary holds, individual releases,
   dropped events/state recovery, Fn, ordinary keyboard input, scaling and exit.
6. Summarize by brand/model/revision with separate acquisition, analysis and
   hardware-evidence states. Only then consider changes to public support status.

A suggested analysis stage is not an accomplished feature. Report what actually
ran and what remains unknown. Do not claim perfect behavior without evidence.

## Reuse map; inspect selectively

- `tools/keychron_layout_firmware_fetch.py`, `tools/keychron_catalog_fetch.py`;
  `.local/research/keychron-layout-firmware` and `keychron-catalog`.
- `.local/research/attackshark-pro/family-registry-20260920.json` and
  `family-firmware-20260920.json`; `.local/research/attackshark-x68he`.
- `.local/research/ajazz-ak820max/illumi-firmware.json`, local driver/web sources;
  `tools/review_ajazz_ak820max_firmware.py`, `review_ajazz_ak820max_routes.py`,
  `review_ajazz_ak820max_raw_stream.py`.
- `tools/review_mg75_firmware.py`, `.local/research/irok-mg75-fn`.
- `tools/review_io_type84_firmware.py`, `docs/firmware/io-type84-magnetic`.
- `tools/review_aula_mini60pro_firmware.py`, `.local/aula-mini60-firmware-api.json`.
- `tools/audit_ipi_firmware.py`, `tools/test_ipi_firmware_records.py`,
  `docs/research/ipi-firmware-20260914`.
- `tools/review_sayo_o3c_firmware.py`, `tools/extract_panchip_dfu_payload.ps1`.

Do not recursively ingest all of `.local`: it contains full checkout backups and
unrelated dependencies. Keep imports scoped and distinguish existing local
artifacts from newly downloaded official packages.

Runtime available at last check: `C:/Program Files/Python310/python.exe`, requests,
pefile, capstone, unicorn; installed 7-Zip at `C:/Program Files/7-Zip/7z.exe`.
Commands and source files are local; no running corpus job remains from this turn.

## Separate tester state, only if feedback arrives

AJAZZ rev5 EXE: `build/bin/Release/x64/HallJoy.exe`, SHA256
`07d71c70828a78aa2a116b931ac52db0c87500c313c17da4b79e72fb2b98e478`.
Uses confirmed raw stream and owner-approved preliminary dynamic per-key limits,
not factory-calibrated millimeters. Full context is in
`AJAZZ_AK820MAX_REVIEW_2026-09-20.md`. Do not reopen settled decisions merely
because the new chat lacks the old conversation.
