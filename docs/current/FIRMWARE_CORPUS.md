# Firmware corpus: current runbook and evidence

## Current scope

The owner prioritized the research foundation before broad collection and asked
for autonomous work until a consequential question requires their input.
Architecture and acceptance contract: [FIRMWARE_FOUNDATION.md](FIRMWARE_FOUNDATION.md).
Original pilot/hardening record: [archived checkpoint](../archive/FIRMWARE_CORPUS_PILOT_2026-09-20.md).
The historical limits in that archived checkpoint are superseded here.

No HallJoy runtime/EXE, Google Sheet, public support status or release was changed.
No device access or flashing occurs in the corpus tools. Attack Shark tester work
remains paused. The one emulator integration below reruns existing no-light AJAZZ
research; it does not establish behavior of the tester's RGB firmware.

## Modules and state

- `tools/firmware_corpus.py`: acquisition, immutable objects, CLI, reports and audit.
- `tools/firmware_pipeline.py`: shared ZIP/RAR/7z/PE/HEX/pinned/known-image jobs.
- `tools/firmware_sources.py`: pure source snapshot adapters and explicit mappings.
- `tools/firmware_evidence.py`: validated evidence receipts, with separate levels.
- `tools/firmware_research_runner.py`: allowlisted exact-image emulator runner.
- `tools/firmware_store_lock.py`: OS-owned single-writer CLI lock.
- `tools/tests/test_firmware*.py`: 73 focused automated tests.

Store: `.local/firmware-corpus/`; immutable objects are SHA-256 addressed. SQLite
keeps original pilot tables and additive job/image/source/evidence tables. Old
engine results and edges remain historical. `image_segments_v2` is authoritative;
the earlier nullable-address key table is retained but no longer written.
`tooling/<hash>.json` preserves extractor source snapshots outside firmware objects.

CLI writers acquire an OS lock; process death releases it. Direct library callers
must use `writer_lock(root)` themselves. Do not run old pre-lock CLI copies against
the same store. Back up the SQLite database using its online backup API before
schema or broad policy changes; copy source/documents with freshness checks.

## Reproducible environment

Canonical tested runtime is `.local/firmware-corpus-venv/Scripts/python.exe`:
Windows x64 / CPython 3.10. The seven package versions and downloaded wheel hashes
are pinned in `tools/firmware-requirements-win-py310.lock`. Wheel cache:
`.local/firmware-wheelhouse/`. Global Python packages were not modified.

To recreate this environment in a new, unused local directory:

```powershell
python -m venv .local/firmware-corpus-venv
& .local/firmware-corpus-venv/Scripts/python.exe -m pip install --no-index --find-links .local/firmware-wheelhouse --require-hashes -r tools/firmware-requirements-win-py310.lock
& .local/firmware-corpus-venv/Scripts/python.exe -m pip check
```

The lock contains Windows/CPython-3.10 wheel hashes, not a cross-platform lock.
A missing wheel cache can be refilled from the package index with the same lock
and `--require-hashes`; do not silently upgrade versions. Installed 7-Zip remains
`C:/Program Files/7-Zip/7z.exe`; its executable hash is part of native job identity
and is rechecked before each helper call. No vendor updater is executed.

## Commands

Run from the project root:

```powershell
$fwPython = '.local/firmware-corpus-venv/Scripts/python.exe'
& $fwPython -m unittest discover -s tools/tests -p 'test_firmware*.py'
& $fwPython tools/firmware_corpus.py audit-store
& $fwPython tools/firmware_corpus.py extract
& $fwPython tools/firmware_corpus.py report
```

`extract` processes mixed nested formats through one queue and shared run budget.
`expand`, `extract-native` and `extract-pinned` remain scoped convenience commands.
Complete jobs are reused; interrupted/deferred jobs resume; blocked jobs require
`extract --retry-blocked` or a changed code/dependency/policy identity. Failed work
consumes the reserved budget. Attempts retain their own results across retries.

Default limits: 128 MiB per object/member, 512 MiB per extraction invocation,
4 extraction levels, 4096 archive members/resources, 4 MiB native listing output,
64 KiB native stderr, 30s native listing and 45s member extraction. Native stdout
is bounded during reading, including overflow and timeout termination. ZIP CRC
checks and native member size checks are required. Unsafe/duplicate member names,
archive links and encrypted archives are rejected. PE completion means selected
RCDATA/custom resources only; embedded/overlay payloads remain explicitly unresolved.

HEX decoding checks record lengths/checksums, EOF, extensions, overlaps and entry
records. It preserves sparse segments and never fills unknown gaps. Overlapping
or wrapping records are rejected rather than guessed. The strict parser supports
record types 00-05; [format reference](https://www.keil.com/support/docs/1584/_hlp_hexfile.htm).
Valid encoding is not proof of hardware identity or analog support.

`audit-store` is read-only: missing/corrupt/unindexed/staging files, dangling edges,
image-layout/index inconsistencies, toolkit corruption, SQLite and foreign keys.
It never deletes unexplained bytes or invents origins. A killed transaction can
leave a valid unindexed blob; replay reuses it only after hash verification.

## Source records and identity

`illumi --download` retains the original official registry acquisition adapter;
`--refresh` requests conditional refresh. Cache hits avoid HTTP calls. Connection/
timeout errors and selected temporary HTTP statuses retry at most three times.
Failed refresh preserves the last good object and validators while recording an
error; it does not report the old bytes as a fresh success. Fetch observations are
append-only for new network results. Historical pre-ledger fetches are not forged
into new observations.

Offline platform metadata can be imported with:

```powershell
& $fwPython tools/firmware_corpus.py source-snapshot illumi-v1 .local/research/ajazz-ak820max/illumi-firmware.json
& $fwPython tools/firmware_corpus.py source-snapshot rongyuan-research-v1 .local/research/attackshark-pro/family-firmware-20260920.json
```

`keychron-product-v1` accepts the saved native product API responses. Its stock
firmware references do not change the owner's settled custom-UAP support decision.
The Rongyuan adapter reads the saved research wrapper and preserves failed API
results and prior hash claims as historical claims. It does not claim fresh downloads.
Source products are never automatically merged with retail brands/models.

`associate <claim.json>` stores an explicit local mapping with exact source record,
catalog brand/model, revision/layout/transport (null means unknown), indexed evidence
hash and candidate/reviewed/rejected state. Reviewed is a mapping-review state, not
hardware support. Different retail brands remain separate. No real mappings have
been populated during this foundation step; examples live in source unit tests.

Only ingest explicit research roots/files. Never recursively ingest `.local`, which
contains repository backups, environments, dependencies and unrelated artifacts.

## Images and research evidence

Image inventory is separate from packages/resources. It contains addressed HEX
maps and exact previously reviewed raw images. Structural Cortex-M candidates stay
triage only. Raw images with unknown load address are distinct from address-zero
images. Equal encodings with the same addressed bytes share an image identity;
equal bytes at different addresses do not. Image source relations retain provenance.

`evidence <receipt.json>` validates exact image inputs, producer identity, hashed
assets, limitations and evidence level. Import is marked not-rerun. It verifies
receipt structure and bytes, not the truth of the submitter's claim. Historical
references cannot be imported as a new PASS. MCU emulation must name mocked
peripherals; hardware receipts must specify the device identity.

The runner has one reviewed suite. It refuses any other image, snapshots the three
existing harness files into a temporary tree with the exact input image, enables
assertions, captures bounded output and records code/configuration/dependency hashes.
It never dispatches by filename, strings, controller, USB library or similarity.

```powershell
& $fwPython tools/firmware_corpus.py run-suite ajazz-sg8994he-raw-v1 0252284172984e1fd699540b13f7178d506d70909a0044805c3f9719abe9fb5f
```

The first run executes the suite; the same inputs/code/dependencies reuse the
verified receipt. `--rerun` explicitly creates another execution. A changed input
is rejected; changed code/dependencies creates a new run identity. PASS means suite
expectations were reproduced, including the known event-loss defect; it does not
mean the keyboard is supported. Timeouts are inconclusive, never no-analog results.

Latest actual local MCU run in the pinned environment: PASS; firmware SHA256
`fd42c6f691e353a4411c6c13023f7ee3eccd7726bb7b3ac28ee4fe62df24f680`,
no-light SG8994HE V1.13.02. Receipt
`9f8994746bc219471c46193c2d277aacb500336d7dbd78dc8a67d827c9ee8c33`.
Synthetic ADC/RAM and scheduler servicing; no RGB, physical calibration, real USB
timing or full-device lifecycle claim. Earlier development executions remain history.

## Measured foundation checkpoint

- 73 tests pass in the pinned environment; `pip check` passes without the global
  requests/chardet warning. Tests include actual child-process termination during
  a transaction, OS lock recovery, real generated PE->ZIP->HEX and ZIP->7z->HEX
  chains, shared budgets/depth, corrupted members and assets, cache/retry behavior,
  exact-image dispatch and preservation of brand/unknown identity dimensions.
- Current extractor: 81 complete jobs (31 ZIP, 8 native archives, 37 PE, one pinned
  updater, two HEX, two known raw images); zero current blocked/deferred jobs.
  The pinned updater takes its exact-image route rather than a generic PE route.
- 766 objects, 70 origins, 4 normalized/review-linked image identities, 6 image
  segments. These are not counts of supported devices. Object growth includes
  metadata and evidence/code assets, not only firmware.
- 90 imported historical platform records: Illumi 40, Keychron 13, Rongyuan 37.
  Rongyuan has 34 saved source errors and three download references; these are
  acquisition observations, not analog conclusions. No new firmware package was
  downloaded during preparation. Only pinned Python wheels were newly downloaded.
- 106,252 versioned extraction edges, but 26,712 distinct parent/child/member
  triples. Development engine revisions explain the repeated edges; these are
  not firmware-image counts or independent discoveries.
- All baseline rows preserved: 736 objects, 46 origins, 40 fetches, 40 inventory
  records, 473 models, 43,639 edges, 146 analysis rows and 31 ZIP jobs.
- Repeating extraction and the emulator dispatcher added no rows to the checked
  object/job/image/evidence tables; the emulator result was cached. Full store
  audit is clean and SQLite integrity is `ok`.

Machine-readable verification: `.local/firmware-corpus/foundation-verification.json`.
Current report: `.local/firmware-corpus/report.json` (schema 4). Historical job
states are separate from `current_job_states`; consumers must not merge them.

Backups: `.local/backups/firmware-foundation-20260920-215934/` (before work),
`.local/backups/firmware-foundation-validated-20260920-223053/` (validated code/store,
before final documentation reconciliation). Earlier backups remain intact.

## Remaining scope

This foundation supports controlled extension. It is not a universal firmware
analyzer, complete 85-brand coverage or a network-facing unattended service.
Only Illumi has an integrated live registry downloader; other adapters currently
consume saved snapshots. Retail links need source-backed population. Custom/encrypted
payloads, additional formats, function matching and transfer of emulator suites
require their own verified adapters. CPU/memory isolation of in-process Python
parsers and distributed workers are not implemented. Native output/time limits
are implemented; they are not a complete OS sandbox. New dependencies/platforms
require a new tested lock. No automatic public support-status update exists.
