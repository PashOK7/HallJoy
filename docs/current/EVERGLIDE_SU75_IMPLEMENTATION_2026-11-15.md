# SU75 revision layouts and physical input (2026-11-15)

Follow-up to [the exact firmware review](EVERGLIDE_SU75_REVISIONS_2026-11-14.md).
The previous report describes the pre-change executable. This continuation adds
revision-specific automatic layouts and cached diagnostic evidence to the ordinary
SparkLink backend; it does not change USB admission or promote V2 to Supported.

## Identity and layout

- V1:1CA6:3002 + device-info board00300002 -> existing SU75 Pro ANSI preset.
- Seller's pictured V2 / SU75 Ultra:1CA6:3007 + board00300007 -> dedicated
  `Everglide SU75 Pro V2 (SU75 Ultra) ANSI`,81 keys. Both source presets share
  one visible menu entry via the existing same-brand alias mechanism. Old names
  remain valid; customized legacy layouts remain separate.
- The token is exposed only after a successful live travel read. Unexpected board
  metadata removes autoselection only; existing safe protocol admission continues.
- Factory126-slot matrices were located independently in all nine acquired images
  (six XS117, three XS125); every matrix matches byte-for-byte. Public derived
  mapping/provenance: `docs/research/everglide-layout-sources/su75-revision-identities.json`.
  Vendor code/images stay private. No guessed regional variant is introduced.

## Physical channels, assignments and release

For these two proved board identities, retain the81 physical readings separately
from live key assignments. Manual layout uses factory positions; Automatic uses
current assignments and the shared native-layout remap snapshot. Disabled,
unknown, duplicated and extra nonphysical assignment entries cannot veto a sensor
read. A failed assignment row is recorded and falls back to established factory
positions for that row, rather than refusing the independent travel getter.

The six physical rows remain eligible even with all assignments disabled. Each
successful row copies all21 slots, including zero releases. No digital-key release
inference, calibration write, new delay, rate cap or guessed expiration was added.
Existing row freshness/transport handling remains unchanged. Other SparkLink
models retain their existing paths.

## Evidence in ordinary Open log

A dedicated support-log provider reads cached identity, board, app/PCB versions,
row count, missing-assignment mask, successful/failed query counters, physical
sample count and release-event count. It opens no device and requests no new
measurement. Evidence uses the bounded stream store; identity also enters the
structural event history. No serials, raw keyboard packets or new forced logging.

## Tester contract and validation boundaries

Registered family `spark-su75`:20 positive cases (two revisions x factory,
all-unbound, aliases, unknown assignments, remap, missing assignment row,
nonphysical extra entries, sparse assignments, changed firmware metadata,
and unknown board metadata preserving analog). Six negatives: foreign VID,
wrong page/usage, non-keyboard type, wrong-row travel echo and failed transport.

The isolated exact-image test hooks Win32 read/write only and executes the
production packet framing, device info, layout discovery, zero-depth proof,
row decode, multikey commit, ownership/value getters, V2 snapshot publication,
telemetry token gate and common release cleanup. Fixtures are reconstructed
from the reviewed factory map and protocol, not captured hardware packets.
No HID is enumerated/opened by this test. OS enumeration, real USB ownership,
thread scheduling and sensor-to-ADC behavior are outside this isolated case;
existing native routing/lifecycle gates cover their software contracts.

Pre-analog conditions retained: exact official identity and FFB0:1 collection
before commands; usable writable handle; keyboard-type info response; valid
reply framing/row bounds; live travel reply for generic admission; uncancelled
service and successfully allocated event/worker. Wrong identity/collection,
failed transport, malformed framing, cancellation and resource/ownership errors
remain hard refusals, with existing lifecycle/stability reasons and added
structural protocol/travel failures. Firmware/PCB version, assignments, aliases,
counts and layout display never become additional analog gates.

V1 remains Supported. V2 retains existing generic protocol-match yellow behavior;
no named Sheet model/status change is part of this layout/input refinement.
Hardware range, typing coexistence and USB contention retain the bounds of the
firmware review; software PASS is not a physical test.

## Completed ordinary image

Installed `build/bin/Release/x64/HallJoy.exe`, version1.6.15.0,11879936 bytes.
SHA256 `6a12acddb9c57adbfc4c996c44be3e9390923294d771b5f8f1c4e1a22adfdb17`.
No GitHub release or firmware update was performed.

- Exact-image tester gate:10 registered families PASS; SU75:20 positive /6 negative
  cases,81 physical channels, no hardware opened.
- All eight normal candidate checks PASS on this final image, including Shark.
- Support-log Windows gate PASS, including the independent cached Spark provider
  in an ordinary requested snapshot. The finite test provider uses the kept store;
  the production Spark provider uses the bounded stream store.
- Full production profile/layout suite PASS; both SU75 selectors exercise the
  common automatic consumer, reconnect and Manual restore. Same-brand alias audit
  PASS;285 yellow-layout rows checked;92 Supported layouts present.
- Startup recovery PASS,18 scenarios plus repeated startup; embedded resources
  and license provenance PASS. Runtime factory literals independently match all126
  reviewed slots /81 physical keys across the nine acquired images.
- Full private research references re-recorded after the generator/catalog changes;
  final input-hash validation PASS. Native static/portable suite completed with
  one assertion in the unchanged process-supervision timing test; isolated recheck
  of its identical cached binary PASS. All other native checks passed. No assertion
  or timing threshold was weakened. Early compile/test-fixture failures and the
  duplicate-geometry audit were corrected before delivery; the final image's gates
  all passed. An earlier candidate's Shark20 also passed an isolated same-hash
  recheck; the final image passed it on its first attempt.

Logs, source hashes and machine-readable receipt:
`.local/research/everglide-su75-revisions-20261114/implementation-receipt.json`,
`finalize-merged-layout.log`, `native-checks-20261115-final.log`,
`process-supervisor-direct.log`, and `.local/tester-delivery/<EXE-SHA256>.json`.
The normal release wrapper was used for admission/build checks; after the catalog
alias failure, the unchanged remaining gates were continued with the same compiler
flags and exact-image/source hash checks before the ordinary atomic installation.
Backup: `.local/backups/everglide-su75-integration-20261115/`; generated layout
outputs additionally backed up by `layout_pipeline.py integrate`.
