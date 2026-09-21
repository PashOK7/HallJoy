# Firmware research foundation

## Scope and evidence contract

Owner requests preparation before broad collection: reproducible acquisition,
verified extraction, precise identities, reusable analysis and explicit unknowns.
No runtime/EXE/Sheet changes. No firmware execution during extraction. No protocol
claims from strings, command bytes, USB libraries, names or structural candidates.

## Data layers

1. Acquisition: source URL/local origin, immutable metadata snapshot, observed
   package hash, product/revision claims as published by that source. A failed
   refresh must preserve the last successful bytes without reporting them as fresh.
2. Artifact: immutable SHA-256 blob. Package, resource and firmware-image roles
   are separate claims; a blob count is never a firmware count.
3. Extraction: versioned parser plus dependency identity and policy parameters;
   parent hash, exact member/boundary/address, child hash. Completion means this
   parser finished, never that all embedded firmware is known.
4. Image: validated encoding and address segments, or exact previously reviewed
   raw image. Sparse gaps are unknown, never silently filled. Address-aware image
   identity is separate from container/file identity and from device identity.
5. Device association: explicit brand/model/hardware revision/layout/transport
   relation, evidence reference and verification state. OEM labels stay source
   labels until a reviewed mapping establishes retail identity. Ambiguity persists.
6. Analysis evidence: exact input image, harness/code/dependency identities,
   configuration, outcome and evidence level. Static analysis, real executed MCU
   code with mocked peripherals, synthetic unit tests and hardware are distinct.
   Prior references are not new executions. No automatic public support changes.

## Execution and recovery contract

One writer per store, enforced for the CLI. Parser jobs share transaction handling,
retry states, cross-format depth and run budget. An attempt has an immutable result;
current state can advance without erasing history. Incomplete attempts resume;
resource-budget deferrals resume next run; invalid/unsupported inputs need explicit
retry or a new parser/dependency/policy identity. Successful jobs are reused.

Plan sizes before extracting; cap member count, individual output, total run bytes,
depth and subprocess time/output. CRCs/format checks are mandatory. Reserve budget
before work, including failed decompression. Native output must be bounded as it
is read, not after an unbounded file has been written. Never execute vendor tools.

Publish immutable bytes atomically; child index rows, extraction edges, image
records and successful job result commit together. Interrupted files are not input
roots. Reconciliation reports missing, corrupt, unindexed and staging objects;
it does not delete evidence or invent provenance. Back up before migrations.

## Acceptance matrix

- ZIP, native archive and PE nested in different orders share one queue and budget.
- Interrupted extraction, parser failure and corrupt later members publish no
  partial lineage; repeat/resume is idempotent and preserves earlier attempts.
- Sparse Intel HEX parsing checks record lengths, checksums, EOF, addresses,
  overlap and entry points; equivalent encodings have equal address-aware identity.
- Old databases migrate additively; original source/model/research rows survive.
- Changed parser/dependency/policy identities rerun only affected job classes.
- Separate acquisition, extraction, image and analysis summaries; no heuristic
  promotes an object into verified firmware or a source product into retail model.
- Existing corpus and a deterministic mixed-format fixture set pass targeted tests.

## Staging

Implement shared extraction lifecycle and bounded native execution first; then
strict image normalization/inventory, integrity audit and evidence/source contracts.
Validate each layer with failure injection and the real pilot corpus. Expand source
adapters only after these gates. Function matching and universal emulation dispatch
remain later analysis work; their absence must stay visible in reports.

## Accepted first foundation implementation — 2026-09-20

The acceptance matrix above passed on deterministic fixtures and the existing pilot.
73 tests pass in a pinned project environment. Shared extraction, real process-crash
recovery, bounded native I/O, normalized image inventory, immutable attempt/evidence
history, source snapshots, explicit retail associations and one exact-image MCU
runner are implemented. The actual existing AJAZZ suite executed successfully and
its next invocation reused verified evidence without running MCU code again.

See [the current runbook](FIRMWARE_CORPUS.md) for exact commands, receipt, corpus
counts and limits. The first foundation is suitable for controlled expansion;
whole-catalog coverage, new family research and universal matching are separate
future deliverables. No unknown model or protocol is silently treated as supported.

Storage detail: `image_segments_v2` uses ordinal keys. An early nullable-address
key admitted a duplicate unknown-address row; canonical layouts drive additive
repair. The old table remains history, not the authoritative image index.

Extractor code is captured in content-addressed snapshots. Shared implementation
changes invalidate the kinds sharing that code; dependency-only changes affect the
relevant adapter. Repeated versioned edges never count as new firmware discoveries.
CLI locking does not protect direct library callers; they acquire writer_lock.

Source-reference receipts, imported evidence and locally executed emulator receipts
remain distinguishable. Stored assertions are not automatically independent proof.
The existing emulator's expected known defects remain defects even when its suite
passes. No new hardware test, RGB inference, runtime build or support change occurred.
