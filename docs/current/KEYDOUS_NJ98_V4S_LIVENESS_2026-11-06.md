# Keydous NJ98-CP V4S: RF connection/hold correction — 2026-11-06

Latest [revision3 tester logs and revision4 table-order correction](KEYDOUS_NJ98_V4S_RECONCILE_2026-11-06.md): quick startup and stable connection confirmed in one run; sticking still reported. Initial access contention remains.

Latest tester says initial No keyboard can last about a minute, then tracking
starts, while the notice alternates between hardware testing incomplete and
Keyboard communication is unstable. This follows the successful direct-stream
[log and clarification](KEYDOUS_NJ98_V4S_STREAM_2026-11-06.md).

## Root cause and evidence boundary

Code review establishes a host-state error in revision2. Connected() and Get()
applied1500ms depth age to every RF path, including the new delta stream. A valid
unchanged hold or idle period does not necessarily produce a new delta sample.
The successful log already shows background table waits lasting17.97s despite
4277 real stream samples and zero recorded stream I/O errors. Aging on depth
changes can therefore falsely disconnect a live source or neutralize a steady
held key while table traffic waits.

KeyboardUI ObserveCommunication uses connection transitions and explicit access/
protocol anomalies, NOT failedUpdates. The initial suspicion that the failure
counter itself toggled the banner was corrected after reading its producer.
Repeated false connection drops can latch the instability notice;15 healthy
seconds clear it. No global UI observer thresholds or warnings are suppressed.

The separate minute-long opening issue is NOT solved or attributed by this
finding: the failed log had73 error32 exclusive opens before any firmware command;
the successful log had none. No handle owner was identified. Reboot did not
immediately resolve either build, and the observed older-then-newer success is
not proof that the older build initialized firmware. Exclusive RF opening remains.

## Later tester observation: vendor application trigger

Tester subsequently reports that opening and closing the driver application
appears to block HallJoy until reboot or receiver replug. This is a useful reported
trigger, not a verified handle owner or firmware diagnosis. No new post-application
failure log accompanies this observation. Do not assume it has the same phase/error
as the earlier error32 report: it could instead be a different transport state.
Closing a window does not establish full process/helper termination. For error32,
a firmware-only reset explanation does not account for the observed Win32 sharing
conflict. Verify full vendor-app exit (tray/processes), then test HallJoy before
replugging and retain the ordinary failed log if it still fails. No vendor process
is automatically killed. Revision3 does not claim to resolve this access issue.

## Revision3 changes

- Keep depth identity/admission mandatory: exact receiver/interface, proved remote
  board3793, live mapping and an initial four-page genuine depth table. Read those
  SAME four initial pages before1B enable so stream traffic cannot occupy the
  command channel needed for initial table admission. No added reads, fixed sleep,
  simulated sample, polling cap or new firmware opcode.
- Once streaming is enabled and the input path has genuine depth, distinguish
  unchanged input from transport failure. Valid F7 remote-online replies and
  valid mapped1B packets refresh a separate liveness clock. An enabled delta
  path preserves the last real held depth until an explicit zero/correction,
  source stop or loss of live status; it does not invent timestamps or packets.
- Valid remote-offline/boot status immediately invalidates the source. No valid
  heartbeat/packet for1500ms also disconnects. Table-only mode and stream fallback
  retain their original per-key1500ms depth guard. A receiver that incorrectly
  reports stale online status is a firmware/hardware unknown, not tested here.
- Readiness waits stay in retained transient diagnostics, rather than incrementing
  failed analog updates. Malformed status and real I/O errors still count. A real
  reader fallback reports a communication anomaly; access-error32 warnings remain.
- Log first actual stream samples (1/8/64 and30s milestones) independently of table
  sweeps. revision3 bootstrap/mode fields identify initialization order and
  liveness policy. Table sweep timing is no longer advertised as stream Hz;
  actual stream polling frequency remains unknown, input report size32 is shown.

All genuine samples still go straight to publication on the owned reader thread.
Existing table sequence protection, explicit1B0 cleanup/cancellation and all other
backends remain. Pause/stop clears state. No keyboard text, new raw capture or
forced logging; ordinary Open log is the tester workflow.

## Behavioral review

Host change only; pinned official driver inputs and missing exact-MCU firmware
boundary remain as previously reviewed. No new firmware acquisition/emulation
or physical validation performed. RF forwarding/gamepad input were confirmed by
the preceding tester log, not by these host tests. Typing, real RF loss, rare
missed releases, initial handle contention and actual exit/crash recovery remain
unknown. An actual missed release still requires real table/stream evidence;
there is no digital-release synthesis. Long unchanged input now follows delta
semantics with an independent genuine remote-liveness guard.

## Validation and delivery

Cached native suite PASS including the exact production Liveness and Client:
30s quiet hold/idle, unchanged depth timestamps, no communication warning,
explicit zero release, actual offline, missing heartbeat, table-only/fallback
age guard, all29 I/O failures/cancellations, late replies and snapshot ordering.
Windows headers are included in the test to retain the Yield macro regression.
These are deterministic host tests, not measured RF performance.

Ordinary tools/build_release.ps1 EXIT0. Support diagnostics, production exact-image
full-catalog/onboard/support/backend checks and embedded resources PASS. Production
profile/autoselection/catalog audits,87 Supported layouts against the fresh Sheet,
97-key V4S ANSI preset, and startup recovery18 scenarios plus repeated startup PASS.
No successful gate required a rerun. Evidence: native-checks.log, build-release.log,
and the archived compiled layout-catalog.tsv in the private task directory.

Delivered EXE: build/bin/Release/x64/HallJoy.exe, version1.6.14.0,
11336704 bytes, SHA256 `92c2001b5752e91863240e91a090e19db87755301cbb3e7590d40eb091195d92`.
Installed image equals the gated candidate and supersedes the prior5f667be6 local
build. revision3 bootstrap/mode and stream milestones identify it in normal logs.
All approved functions retained; no GitHub upload. Initial error32 contention is
explicitly unresolved; this build must not be described as a confirmed fix for it.

Tester uses this one EXE in RF mode, tests30s idle plus5–10s steady partial holds,
simultaneous keys and complete releases, then returns normal HallJoy.log/Open log.
If startup fails, return that same ordinary log without switching versions first.
New behavior remains pending hardware confirmation; no polling-rate promise.

## Support synchronization

2026-11-06 fresh live metadata Main/id0/grid1301x26 and brand/model search locate
Keydous NJ98-CP V4S TMR at472. A471:C473 readback preserves C472 Implemented;
awaiting hardware testing, strict dropdown and yellow base/effective colors.
No Sheet mutation: overall support remains Experimental. All283 yellow notices
match the fresh Sheet; runtime/README scope remains accurate. Hardware evidence
and NEXT notes reflect this local correction, not a claim of fixed initial access.

Private tests/Sheet receipts: .local/research/keydous-nj98v4s-liveness-20261106/.
Verified rollback archive: .local/backups/keydous-nj98v4s-liveness-20261106.zip,
with source/docs/prior EXE and per-file hashes in backup-manifest.json.
No GitHub publication authorized.
