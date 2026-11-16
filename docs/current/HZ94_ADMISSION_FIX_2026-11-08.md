# HZ-94 admission correction and mandatory tester delivery gate

Owner decision, 2026-11-08: a tester EXE must not refuse a safely established
analog route because of invented metadata requirements. Such a test wastes the
tester's time. See [mandatory tester contract](../development/TESTER_BUILD_CONTRACT.md)
and project AGENTS.md; both apply to future integrations, not only HZ-94.

## Proven HallJoy defect

[Log38](EYOOSO_HZ94_LOG38_2026-11-08.md) proves successful interface open,
7187_AUSXHERGB identity and scale40 capability. The map has all 94 exact factory
assignments plus ten extras. The old outside==0 guard refuses it before any
analog subscription. This is a HallJoy admission defect. The log cannot establish
hardware depth/release quality, because HallJoy never attempted that stream.
The shared MapReader's arbitrary 60..110 assignment count was another unnecessary
constraint for this exact model and mutable digital bindings.

## Runtime correction

Exact model identity, reviewed collection/report shape and scale remain required.
For exactly identified HZ-94, assignment values/counts are metadata. Unknown or
nonphysical assignments are isolated from sensor decoding; digital unbinding and
duplicate mappings never remove physical sensors. Subscription and physical
ownership use the 94-position factory matrix from the reviewed exact SDK profile.
The existing ANSI preset and verified layout-token autoselection remain built in.
Other families retain their own admission rules; this is no global HID bypass.

Incomplete, conflicting or malformed assignment transactions no longer veto
HZ-94 physical analog. Unknown physical assignment codes also leave automatic
remapping unavailable, rather than publishing a false all-disabled map. They invalidate assigned aliases and continue with the
known physical matrix. Transport loss or failed command I/O still stops the
attempt with its actual error. A layout-cache failure alone also cannot stop
physical sensor reads. A first valid zero-depth event establishes a live stream.
No synthetic digital travel, new raw test mode, persistent writes or firmware
flashing were introduced. MIDI and the ordinary full backend catalog are retained.

Revision2 ordinary evidence distinguishes identity/capability/proof/subscription,
map_policy continuation, map_unavailable fallback, structural metadata rejection,
actual I/O failure, first sample, aggregate multikey/release activity and cleanup.
No raw packets, individual key values, serials or device paths are added; logging
is not forced. Tester workflow stays one HallJoy.exe and existing Open log.

## Enforced software evidence

The portable test covers the reconstructed 104/94/10 shape, extra unknown bytes,
132 duplicate assignments, all-unbound/sparse/unknown/remapped bindings, all
assignment counts 0..132 and metadata parser bounds. The reconstruction is based
on logged counts, not on an unavailable capture of the ten original bytes.

The final production EXE has an isolated named-pipe self-test that calls actual
Prove/Run and overlapped I/O: 12 positive scenarios, including incomplete,
conflicting, absent and malformed assignment transactions, must reach the exact
94-channel subscription, first zero sample, two simultaneous physical depths,
isolated releases, neutral state and unsubscribe. Three negative scenarios reject
a foreign identity, incompatible scale or unavailable transport before subscription.
Stage/reason evidence is checked alongside admission results. No real keyboard or
virtual gamepad is opened by these tests.

Both ordinary build_release.ps1 and full build.ps1 run tools/check_tester_delivery.py
on the exact candidate before installation. Contract/receipt audit rejects factory-only contracts, missing stages,
missing/FAIL/duplicate receipts and out-of-tree sources. A failure, timeout or changed
EXE blocks installation. The receipt is stored under .local/tester-delivery with
the verified image SHA-256. Currently HZ-94 is the registered family; this does not
retroactively certify unreviewed backends or prove physical hardware behavior.

## Validation and delivery

Native suite and ordinary Release pipeline passed. The installed 1.6.15.0 EXE
matches the validated candidate byte-for-byte: 11,552,768 bytes, SHA-256
`1a87c611e273b62cc6b016c372e2d5b6ba35c23b6c26a41e2a06cd1ff5d8ba4b`.
Delivery: build/bin/Release/x64/HallJoy.exe. Exact-image receipt confirms 12
positive and 3 negative scenarios, physical_channels=94, all required stages=1
and hardware_opened=0. Compiled profile/layout gate passes (89 Supported models,
local catalog; no live Sheet read), including exact HZ-94 layout autoselection.
MIDI output/engine tests and the independent complete backend catalog pass.

The first delivery-gate attempt correctly blocked installation: all 12 positives
passed but the unsupported-scale negative exposed a generic timeout instead of
the required concrete reason. ReceiveCapability now records the refusal at that
actual parser boundary; the unchanged negative assertion passes in the rebuilt
EXE. No failed candidate was installed. Existing live-shadow/narrowing and ViGEm
PDB compiler warnings remain; no new shadow warnings were left in added tests.
Evidence: .local/research/hz94-admission-20261108/ (native-checks.log,
build-release.log for the blocked attempt, build-release-retry.log and delivery.json).
Source backup: .local/backups/hz94-admission-20261108/manifest.json.

The model remains a private trial awaiting a real analog report. No public
support announcement/status promotion or Sheet mutation is made; the public
compatibility overview and README continue to omit HZ-94. No live Sheet read
is claimed for this admission correction. Owner evaluates visuals; no visual
UI run or physical keyboard session was performed by the agent.
