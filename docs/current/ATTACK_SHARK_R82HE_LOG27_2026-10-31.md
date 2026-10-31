# ATTACK SHARK R82 HE: log 27 review — 2026-10-31

Owner request: review HallJoy (27).log for R82 HE and explain "support is unverified".
Source: schema 2, HallJoy 1.6.13.0, report UTC 2026-10-31T11:03:36Z, 918 lines,
218757 bytes, SHA-256 `6b7261cda6f935c608421a6450734131d7c3eef271f9a7f7181a63f9d2021ee2`.
Log contents were treated as evidence, not instructions.

## Observed result

- Windows metadata names the keyboard R82HE, USB `3151:502D` (e.g. lines 334-344).
  The exact firmware board ID was never read successfully. The existing R82 HE
  profile expects board 2844 on 502D; that board is not established by this log.
- At uptime 23064640 ms, `shark.metadata_open_failed error=32`
  (sharing violation), `shark.eligible value=0`, inventory 7 collections with one
  metadata failure, followed by `shark.state value=1` (lines 113-125).
  The six described collections omit the required FFFF:0002 / Feature65 collection.
- All 17 retained snapshots have attackshark-pro not_present, connected=0,
  mapped_keys=0, updates=0. Source summary is none_reported / sources=0.
  No successful Shark identity or travel session is present.
- Bound digital A/D/S/W presses are recorded, but the only two trace.pad records
  are neutral (all axes/triggers/buttons zero). No analog/gamepad activity is
  established. This is a failed admission, not simply a working experimental model.
- `ui_notices=00000040` is FamilyCandidate. Current keyboard_page_main.cpp maps
  that flag to "Keyboard support is unverified": shared USB identity is visible,
  but the exact supported model has not been confirmed. It is distinct from the
  ordinary ATTACK SHARK hardware-testing notice after successful admission.
- analyze_support_log.py: 17 snapshots; latest structurally_complete,
  telemetry_complete and summary_consistent true, 26/26 backend rows,
  queue_loss_observed=false. Completeness is not connection success.

## Existing fix and limits

The signature matches the already fixed busy-inventory failure documented in
[X82 HE inventory retry](X82HE_BUSY_INVENTORY_2026-10-26.md): the older worker
waits after an empty inventory rather than retrying a temporarily busy collection.
Current source retries metadata every second when busy and logs inventory_busy /
inventory_busy_cleared. Neither marker appears in this report.

The exact holder of the conflicting interface is unknown in this R82 log.
HallJoy's RongYuan probe is one plausible cause (established in the earlier X82
case), but this report does not identify the handle owner. Do not blame the user,
a configurator or firmware without further evidence. If contention persists, the
existing retry fix cannot guarantee access until that handle is released.

Seven initial engine starts failed at step 3 with issue mask 8. Source review
identifies BackendInitIssue_VigemBusMissing (backend.h:19), passed through
Backend_GetLastInitIssues at app.cpp:1270. It is an issue mask mislabeled as a
Win32 detail, NOT evidence of insufficient memory. Backend_Init sets it when
the output start reports VIGEM_ERROR_BUS_NOT_FOUND. Dependency guidance then
times out (1460) once and returns busy (170) on later UI requests. These are
UI-bridge outcomes, not keyboard-interface errors. Engine startup later succeeds
at 23064328 ms. How the driver became available is not established by this log.

## Initial handoff recommendation — withdrawn after deeper review

Use the existing ordinary local build/bin/Release/x64/HallJoy.exe, SHA-256
`4e250e718b5a368511f451e441068441b632c5035fb212f2a1fd8d36ebd50114`.
Its version string is also 1.6.13.0; unlike the public 1.6.13 artifact it includes
the busy-inventory fix. The hash matches [the 29 October delivery](AULA_AG75_2026-10-29.md).
The owner's objection prompted a deeper review. Do not send this artifact as an
established R82 fix on the basis of the symptom match alone. The existing retry
addresses one temporary-blocking case; the additional findings below narrow that
claim and expose two remaining host-side gaps. No new tester handoff was performed.

R82 HE remains implemented/experimental; this unsuccessful run does not justify
Supported or a new unsupported classification. README/runtime notices and the
Sheet status are not changed by this analysis. No new restriction or support
conclusion was introduced. No app execution, physical test, build or publication.

## Deeper review — owner correction, 2026-10-31

The previous recommendation to hand off the existing EXE was premature. This
section supersedes that recommendation, without changing the keyboard's status.

### Timeline and alternative explanations

RongYuan enumeration reported 6 matching collections, 0 control collections,
1 input collection and 0 pairs at 22906109 ms, 158.531 seconds before the Shark
failure. No RongYuan identity query, rejection or session-open failure is logged.
Its session code requires a paired control/input candidate. This does not support
importing the earlier X82 explanation that HallJoy's own RongYuan identity probe
briefly held this R82 interface.

The visible-but-unpaired RongYuan scan repeats every 2 seconds and logs count
changes. No new count signature appears. This is consistent with continued
unavailability but is not continuous measurement and does not identify a handle
owner. An absent/different vendor collection with a sharing error on another
collection also cannot be excluded: the failed event lacks collection identity.

General device.change value 7 events are not proof of Shark rediscovery or R82
unplug/replug. app.cpp sends these through the native catalog; Shark's descriptor
has no catalog notify callback. Its topology is updated by WM_INPUT_DEVICE_CHANGE.
The old empty-inventory wait emits filtered heartbeats, so it can stay alive
without trying again despite the generic events visible in this log.

| Possible cause | Finding for this run |
| --- | --- |
| Curves, deadzones, inversion, bindings, Fn or normalization | These are downstream of discovery and cannot cause this early metadata failure. |
| Forgotten R82 PID/profile | CandidatePid accepts 502D; 2844/502D is implemented. The actual board query was never reached. |
| Exact firmware-version lock | No exact version lock exists here. Later admission needs a known board/PID and nonzero USB version. |
| Block Bound Keys or game controller handling | Cannot cause the pre-identity failure. |
| UAP crash | Initial child exits have code 0; later snapshots show ready=1, error=0, restarts=0. No crash proof. |
| ViGEmBus unavailable | Established for the initial starts, followed by engine recovery; separate from the later HID failure. |
| Persistent handle/HID-stack/filter contention | Plausible for error 32; owner and duration remain unknown. The vendor app is not proven responsible. |
| Different/missing vendor collection or hardware revision | Still possible; exact collection caps and board ID were not obtained. |
| Incompatible analog report after identification | Neither tested nor excluded by this run. |

Metadata queries with CreateFile desiredAccess=0 differ from ordinary read/write
opens. Do not infer a HID driver's behavior or handle owner solely from a
regular-file sharing test. See [Microsoft CreateFileW documentation](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew).

### Local replay of actual discovery code

Evidence directory: `.local/research/r82he-log27-review-20261031/`.
The harness extracts actual Find and Capture discovery prefixes from the
publication baseline and current source, pinning their hashes in source-pins.json.
SetupAPI, HID, time and cancellation are replaced by explicit mocks. Execution
ends when discovery produces an eligible collection, before session opening,
identity or depth reads. There is no real device access or gamepad output.
The parent's no-output watchdog is modeled from RunChild, not run against USB.

Eight scenarios passed their assertions:

- Old source plus one temporary busy pass: one enumeration, then no recovery
  before the explicit test stop at 12000 simulated ms.
- Current source plus one temporary busy pass: eligibility after 1000 simulated
  ms and three enumerations (busy, quiet success, extra logging rescan).
- Temporary access-denied: the current retry also recovers.
- Missing control collection, wrong usage, and receiver exclusion: none is
  repaired by this retry and no protocol commands are reached.
- **Remaining gap 1:** if busy returns during the logging rescan,
  `if(quiet)devices=Find()` discards a successful result without collecting busyOut.
  The busy branch has already run, so the worker enters indefinite empty-device
  waiting. Replayed: three enumerations, no recovery by the 12000 ms test stop,
  although the mocked interface is available after the third pass.
- **Remaining gap 2:** repeated busy passes are quiet and emit no heartbeat.
  RunChild cancels a play worker after 8000 ms with no pipe output. The modeled
  watchdog fired at 8100 ms / nine enumerations. Actual production then follows
  the normal restart/backoff path; it does not steadily wait for release.

The existing self-test tests shared session opening and parsing busy log strings;
it does not execute this discovery schedule. Its PASS did not rule out these gaps.

A local-only proposed variant retained the eligible result instead of rescanning
for logging and sent the already filtered waiting heartbeat on busy passes. The
same eight cases passed: temporary busy recovery uses two enumerations, the
clearing race disappears, and persistent busy reaches the explicit 12000 ms stop
without a false watchdog. This does not make a held interface available.
No production patch or application build was made from the proposed variant.

Actual profile-header checks also pass: 2844/502D is accepted; unknown board IDs,
2844/5030 and zero USB version are rejected; nonzero USB version 0505 is accepted.
These establish code policy, not the tester's exact hardware.

### Revised next step

The installed EXE remains unchanged at hash 4e250e71. It solves a temporary busy
case but should not be presented as an established fix for this R82 report.
A meaningful next implementation would close both discovery gaps and preserve
bounded evidence of the failed collection, busy duration/recovery and correctly
typed startup issue mask, followed by relevant regression and ordinary build gates.
It must distinguish persistent busy, absent interface, rejected identity and live
analog in the normal HallJoy.log. No unsafe protocol fallback is justified.

R82 remains implemented/experimental. No runtime code, EXE, keyboard status,
Sheet data or hardware conclusion changed during this investigation. The original
review was backed up before this revision; both replay sources/binaries are local.

## Implementation authorized by owner — 2026-10-31

The owner requested one new ordinary EXE with a potential fix and useful normal
HallJoy.log evidence if it fails. This supersedes the source-unchanged statements
above for the implementation phase; their hardware conclusions remain unchanged.

The worker now retains the exact successful inventory, avoiding the second scan
race, and emits the existing filtered waiting heartbeat during metadata retries.
Metadata failures beyond sharing/access denied also retry without requiring a
new arrival. Exact PID, usage, feature size, board identity and live-travel gates
are preserved; no analog simulation or new protocol command was introduced.
New bounded numeric evidence identifies collection/stage, wait/recovery duration,
session access results, identity and first live travel/aggregate sample results.
Step-3 backend issue flags are logged as data, avoiding the false Win32 error 8.
See [diagnostics format](../development/SUPPORT_DIAGNOSTICS_CONTRACT.md).

Pre-change backup: `.local/backups/r82he-discovery-fix-20261031-190048/manifest.json`
contains hashes and copies of affected existing files and the previous EXE/PDB/MAP.
Restore only affected files after checking for later edits; ordinary EXE replacement
also retains its own rollback copy. New regression files can be removed separately.

### Validation and delivery

- Native checks: PASS with the normal cache. Initial static execution-coverage
  audit caught the new generated test's missing explicit filename in the runner;
  the runner now passes the source path explicitly. The failed audit and remaining
  suite were resumed; unchanged successful audits were retained, not repeated.
  Logs: `.local/research/r82he-log27-review-20261031/native-checks-fix.log` and
  `native-checks-resume.log`.
- The new regression executes actual production Find/Capture excerpts, ending at
  the session boundary, with mocked Windows/HID/time/cancellation. All 16 scenarios
  pass: immediate eligibility; transient errors 32/5/2; the rescan race; persistent
  busy heartbeat; 12-second busy then recovery; class-list failure; absent control;
  wrong usage/feature/PID/receiver; additional rejected collection; topology wake;
  prompt cancellation. No device access or protocol commands are used by this test.
- `tools/build_release.ps1`: PASS. All linked candidate self-tests passed,
  including the actual ordinary-log writer/privacy assertions for the new records.
  Full catalog, retained native paths, embedded legal/runtime inputs, production
  profile/layout transactions, all 84 Supported layouts and startup recovery
  (18 scenarios plus repeated startup) passed. Sheet=False in the layout gate is
  the existing local catalog check, not a live Sheet reconciliation claim.
- The only linker warning is the pre-existing missing ViGEmClient debug PDB
  (LNK4099); release linking and mandatory checks succeeded.
- Build log: `.local/research/r82he-log27-review-20261031/release-build-fix.log`.
  The standard publisher installed the verified candidate and restored the
  previously running HallJoy window. Candidate and installed bytes match.

Deliver **`build/bin/Release/x64/HallJoy.exe`**, version **1.6.13.0**, **11278848 bytes**,
SHA-256 **`054f8f1fd8b08f7d77c39a0da495782fb55c2fd976226450441f3d6205dcb8e4`**.
This is a new local ordinary build; identify it by hash and
`evidence.shark.discovery_revision revision=2`, not the unchanged version alone.
The previous installed EXE is also retained at
`build/obj/replacement-backups/HallJoy-ee0fc8d0300449609c23ad97c203d7aa.exe`.
Local source/artifact receipt: `.local/research/r82he-log27-review-20261031/fix-build-receipt.json`.

Tester workflow: launch this single EXE, try normal bound keys, then return the
ordinary HallJoy.log using Open log, whether it works or fails. No script, flags,
extra export or required continuous logging. This fixes demonstrated host-side
retry bugs, but cannot release a persistent foreign handle or establish the
tester's unread board identity. No hardware test or publication was performed.
R82 remains experimental; support conclusion, notice policy, README and live
Sheet are unchanged. Await the new tester log before a hardware success claim.
