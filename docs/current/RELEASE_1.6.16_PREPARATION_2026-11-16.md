# HallJoy1.6.16 preparation — 2026-11-16

Owner requests a release-ready build and explicitly prohibits uploading it yet.
No push, tag, GitHub release or external message is authorized in this task.
Use tools/release.py without --publish; the publication clone is a local review
of cleaned main, not a new feature branch. Ordinary portable EXE/MIDI-off scope
and all retained protocol/research implementations remain in force.

## Support scope

- EPOMAKER HE108 USB/2.4GHz: accepted by owner after tester confirmation. Exact
  board3365/receiver5038 proof and layout token; no Bluetooth claim. Remove both
  the dedicated wireless test banner and generic protocol22 testing notice.
- MonsGeek FUN60 Pro USB: owner accepts Shimanto's positive report. Tested USB
  revision is unspecified. Established2304/502D,2600/5029,2785/5030 paths stay;
  exact61-key ANSI layout is added before Supported. Exact2304 geometry from two
  pinned official clients equals the existing normalized Ultra TMR geometry.
  The picker merges both names using existing migration/custom-file semantics;
  automatic products2600/2785 are distinct from Ultra2352. Receiver5038 is not
  promoted for this model. Nominal4mm range remains the existing policy.
- MCHOSE Ace68 Air: owner accepts the positive model report. III41E4:2132 is
  already Supported; promote II41E4:2120 on protocol29 using existing shared ANSI
  geometry and physical67+Fn map. No new HID log is claimed. Air2/3837:300A remains
  separate/experimental. Existing firmware cutoff/snap/mode lifecycle retained.
- Everglide SU75 Pro V2 discussion is prospective, not a positive test result;
  no status change for it. V1's existing support stays.

## Implementation and verification plan

Exact notice exceptions and generated catalog are reconciled with runtime and
README/hardware/model/follow-up docs. HE10811 production-image cases now require
Supported/no testing notice; multikey/release/cleanup/stream-first checks stay.
Status tests retain unknown-PID and neighboring-model warnings. The existing
profile suite tests all geometry merges, legacy customization and automatic
product selection. No new protocol admission or digital-derived depth.

Fresh Main metadata: sheetId0,1306rows/26cols. Before state: HE108 C264 Supported,
Air II C541 and FUN60 Pro C579 Implemented; awaiting hardware testing, Air III
C542 Supported; Air2 C540 unchanged. Strict dropdowns allow Supported. Add layout
entries and check the exact compiled catalog with the intended Sheet state before
changing C541/C579. Preserve validation and materialize B:C green/base text only
for changed rows. Read back all statuses/base/effective colors and reconcile ALL
yellow rows. Final checks and exact-image/package hashes are appended below after
execution; no hardware or visual test is claimed by the agent.

Backup: private .local/research/release-1.6.16-prepare-20261116/before.zip plus
additional source backups and layout-integrate backup. Before manifests have
original hashes. Vendor downloads/firmware stay private; no screenshot archive.
Release script may normalize source line endings to remote main's existing style.


## CPU budget requested during preparation

Owner requests compilation use at most90% CPU. Added a shared named Windows job,
ENABLE|HARD_CAP9000/10000 and BelowNormal priority. Python wrappers join BEFORE
launching MSBuild/UAP; native checks join before static/compiler work. Ordinary
HallJoy publisher/restored application are outside this job. No affinity change
or90% per-process loophole; nested entries rejoin the same quota. Keep a job handle
in each Python launcher because closing the helper's last handle otherwise loses
the named job lifetime. Real Windows unit cases PASS for queried flags/rate,
priority, children/grandchildren, repeated join, quoting, exit17 and fail-before-run.
No full-load benchmark or subjective UI responsiveness test is claimed. All
future standard entry points use this budget; no optional tester switch.

Initial native run caught a missing direct notice-catalog include in the changed
HE108 fixture, corrected before product compilation. The next run stopped in the
existing MIDI observation fixture. Isolated reconstruction identifies Windows
STATUS_STACK_OVERFLOW0xC00000FD: each observation engine occupies409848 bytes,
with several engine instances/nested calls on a default1MB stack. Move only the
three large test-local engines to owned heap storage, preserving all assertions;
MIDI production code/scope is unchanged. Native failures now name the command
and hexadecimal exit code. Failed logs and original fixture are retained privately.


## Final result — prepared, NOT published

Ordinary1.6.16.0 EXE:11,904,000 bytes, SHA256
`a81447d7f5774775d536740f3982636b2716215fb86f7efe28f0063c26933368`.
The normal publisher installed it and restored the previously running HallJoy.
No firmware/hardware or visual operation was performed by the agent.

- Native static/portable suite PASS;150 binaries reused from exact-input cache,
  two rebuilt in the successful pass. CPU budget Windows cases PASS, with actual
  named-job readback during simulator compilation showing ENABLE|HARD_CAP9000.
- Ordinary release build PASS;13 exact-image keyboard families PASS. HE1087/4
  cases retain stream-first/multikey/release/cleanup104-channel proof and now
  require supported_no_notice1. Full retained catalog/gamepad-only image,
  embedded ViGEm/legal resources and profile/legacy geometry merges PASS.
- Actual compiled layout catalog: FUN60 Pro61, Ultra TMR61, Ace68 shared68 and
  HE108104 entries. Pro/Ultra share their reviewed display group; saved/custom
  layout migration and automatic identities pass the production-linked suite.
- All94 Supported layout entries PASS against the compiled catalog and intended
  Sheet BEFORE writes; live post-write check also PASS. All284 remaining yellow
  models match the runtime notice catalog. Profile/startup recovery18 scenarios
  plus repeated startup PASS. No status gate was weakened.
- Live Sheet Main!C541 (Air II) andC579 (FUN60 Pro) changed from Implemented to
  Supported. Strict ONE_OF_LIST is preserved; B:C base/effective green and text
  colors verified. Full fresh readback changes exactly those two rows. HE108
  C264 and Air III C542 already Supported; Air2 C540 stays yellow. No insertion,
  rename, row/column/style restructuring, note or comment. No Sheet sync pending.

Packaging first failed because a Python-launched PS5 inherited incompatible
module paths and could not autoload Get-FileHash. package_release.ps1 now uses
shared environment/Python discovery and explicitly imports this host's Archive
module. Packaging-only rerun PASS; no second product compilation. The existing
source/compiler warnings recorded in the prior HE108 build are not new to this
change. This is the incremental ordinary release path, not a clean dependency
rebuild or a claim of a GitHub CI run.

Portable package: build/packages/HallJoy-1.6.16-Windows-x64/
HallJoy-1.6.16-Windows-x64.zip, SHA256
`5db1a3e08ac202bb54891a8356ea95497685c3e5511700d5abbfe5b427ced016`.
Its five entries and both executable/archive checksum files are verified. The
ordinary EXE remains available separately; no installer was requested/generated.
Native/build/package logs, exact-image receipt, compiled catalog, CPU readback,
Sheet before/after snapshots and prepared.json are private local evidence.

Source preparation uses the cleaned-main local clone, staged on main. Release
notes1.6.16 are ready. Publication/private-data checks are repeated after this
final documentation update. No commit, new tag, push, GitHub asset upload or
release publication is performed; last-publication state stays1.6.15.


## Publication authorized — 2026-11-16

After owner acceptance, publish this exact approved EXE/package using the
publication phase of tools/release.py; do not rebuild unchanged application code.
Fresh full Sheet readback again matches all94 Supported layouts and284 yellow
notices. README Supported/Experimental lists are rechecked, including HE108 RF,
FUN60 Pro, Air II/III, FREEWOLF F68 and LANGTU BOOM68-USB.
Release tooling now replaces the prepared owner-context route rather than
prepending an entry beyond the20480-byte startup budget; the actual context
transformation passes its bounded preflight. Each push rechecks publication
inputs. The final publication record supersedes this note's earlier unpublished
state. No vendor firmware or correspondence is added to publication.
