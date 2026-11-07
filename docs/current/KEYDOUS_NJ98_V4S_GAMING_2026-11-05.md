# Keydous NJ98-CP V4S: RF gaming-path correction — 2026-11-05

Latest: [direct RF stream confirmed](KEYDOUS_NJ98_V4S_STREAM_2026-11-06.md),4277 samples and virtual pad output; startup remains unstable. Historical test/build record follows.

Update: [November6 wireless-v2 log](KEYDOUS_NJ98_V4S_OPEN32_2026-11-06.md) found the receiver/input pair but all73 control opens failed with sharing error32, before identity or analog. The fast RF path remains unmeasured.

Supersedes the scheduling and untested-depth conclusions in the initial
[receiver report](KEYDOUS_NJ98_V4S_WIRELESS_2026-11-05.md).
Owner requires genuine continuous analog, no artificial polling cap or extra
waiting. This is a local test build, not a GitHub publication or hardware-rate claim.

## Tester evidence and established cause

Three ordinary logs, schema2, catalog27, complete telemetry and no queue loss:

| Transport / tester setting | SHA-256 | Findings |
|---|---|---|
| USB | 59c87794f96ef455a2c2321af2ff114d837b471e64a1f330f46f8ffcdecc243a | 9,635 native updates, zero backend failures; tester reports smooth gamepad output. |
| RF / 2k | c2c94ccefde1f486fb63be15f4c1564dd43f2fdc27242af65d64738a63403ef1 | 343–391 ms table sweeps, 229 complete sweeps in latest native totals, genuine varying depths/releases and gamepad output; choppy. |
| RF / 8k | c06c9d08a05a41e72dbcc82694f9241bf8f9baae0edf5e24d70e8d44aee462a1 | 343–406 ms sweeps, 293 complete sweeps in latest native totals; choppy. Initial 31 exclusive-open errors32, holder unknown. |

Names reflect tester settings; these logs do not independently measure USB/RF
polling configuration. All prove board3793, USB firmware0410, precision0,
100 units/mm, 4 mm scale, 97 mapped positions. Duplicate 2k (1).log is identical.
RF native updates previously counted 97 published slots per sweep; this is not
comparable to wired change-driven packet counts. Trace cadence is not polling rate.

The former client executed eight fixed10ms waits for EACH of four pages: 320ms
host delay per sweep before physical I/O. This accounts for most observed latency;
it is not an established radio limit. Eight readiness polls also abandoned live
sessions during short busy periods, causing new identity/map negotiation and
observed 6.8–8.4s output gaps. Retained evidence is bounded and later detailed
summaries may be absent after an earlier Open log capture; native totals differ
in scope. No claimed complete radio-session totals are inferred from that window.

## Revision2 implementation

- Remove every fixed receiver delay. F7 readiness controls progress; busy retries
  yield the CPU without Sleep or a polling cap. A 200ms busy deadline bounds a
  retry, not successful-exchange latency. F6 target is cached under exclusive ownership.
- Publish validated pages immediately, without a four-page publication barrier.
  Initial table-only connection still requires all four pages. Actual per-key
  timestamps and the existing1500ms stale guard remain; no invented depth/filter.
- Keep the live handle/maps on transient readiness timeouts. Drain any outstanding
  response before issuing a different page so late replies cannot shift key pages.
  Offline, bootloader, invalid depth and hard transport errors still stop the session.
- Attempt vendor1B delta streaming only for exact remote3793 AND a uniquely paired
  receiver HID collection: same Windows container, FFFF:1, input32, report5,
  one8-bit value cap/count31. No guessed endpoint or packet format. A dedicated
  pending-read thread publishes received analog immediately, independently of
  table I/O. Per-slot sequencing stops a table request overwriting a newer event.
- Missing/ambiguous input collections use uncapped per-page reads. Failed stream
  opening/enable is logged. I/O failure or malformed stream stops that reader and
  keeps table reads. Normal stop cancels/joins the reader, sends1B0, and clears
  publication. Ambiguous enables also receive cleanup; unsuccessful disable is
  reported and retried by the session destructor with bounded cleanup operations.

Same ordinary EXE retains USB, all other approved backends and the exact97-key
ANSI preset/autoselection, token6CB4C60ADBE352F9. No calibration/save/flash writes.
Receiver3151:5038 is shared: F7/F6/FC +8F must prove3793 before model-specific work.

## Behavioral review and remaining unknowns

The pinned official driver/installer and exact MCU acquisition limits in the
initial report remain applicable. No exact MCU firmware was acquired or emulated;
there is no firmware execution PASS. Official setKeyMagnetismReport implements
1B enable/disable without a keyboard reply; its presence does not prove RF
forwarding on this receiver. This explicit streaming attempt supersedes the
initial report's decision to leave1B disabled.

| Finding | Current disposition |
|---|---|
| Typing coexistence | Initial snapshots did not establish typing impact; RF1B typing behavior unknown. |
| Simultaneous keys | Host mapping/publication supports independent slots; physical RF multikey test pending. |
| Range / shallow input | Genuine varying RF depth established; full/shallow range and noise not measured. Host retains advertised precision and4mm scale. |
| Temporal filtering | Artificial host waits removed; firmware filtering and actual new rate unknown. No host interpolation or smoothing. |
| Release | Genuine RF zero/release transitions established in old logs; new stream release delivery pending. Table reads provide independent reconciliation. |
| Physical mapping | Exact factory/live maps and97-key ANSI preset retained; other remote boards excluded. |
| Loss / stale values | Transient late replies recovered in host tests; per-key age guard neutralizes stale values. Real RF loss and firmware stale replies remain unknown. |
| Persistent settings | No save/calibration/flash command;1B is a session-mode control. Its persistence across keyboard power loss is unknown. |
| Normal exit | Owned read cancellation/join plus explicit1B0, including ambiguous enables; actual receiver cleanup remains untested. |
| Crash recovery | Process handles close;1B0 cannot be guaranteed after crash. Actual firmware recovery unknown. |
| Configurator conflict | Exclusive RF command ownership retained; error32 observed but holder unidentified. Close vendor configurator for testing. |

Selected: exact-descriptor1B attempt plus immediate page reads, the best reviewed
available path. Table-only remains available when no matching input collection
exists. Calibration/raw-sensor/settings/flashing paths remain rejected; wired
stream is the established smooth alternative. RF gaming suitability is pending
new hardware evidence; no 2k/8k analog delivery is promised.

## Useful ordinary-log diagnostics

revision2 discovery/input/mode reports exact input pairs, eligibility, stream
start success/error, fixed_delay_us=0 and per-page table mode. Travel summaries
report sweep/page counts, measured mean/max sweep time, actual stream packets,
malformed/I/O counts, transient retries and releases. max_stream_gap_us includes
idle delta silence and is NOT latency. Continuous logging receives rf_sweep_us,
rf_stream and rf_pages events after Open log; retained evidence remains available
through normal Open log. No forced logging, raw HID or key identities added.

## Support synchronization

Fresh live metadata and Main!A471:C473 readback: exact Keydous / NJ98-CP V4S TMR,
C472 remains Implemented; awaiting hardware testing, strict dropdown and yellow
base/effective colors preserved. No Sheet mutation needed: wired result is accepted,
RF now demonstrates real analog but gaming suitability of revision2 is untested.
All283 yellow notices reconcile with the fresh snapshot. README, hardware evidence
and NEXT notes record the improved evidence and experimental RF scope.

Private logs, fresh Sheet receipt and validation logs:
.local/research/keydous-nj98v4s-gaming-20261105/.
Verified pre-change backup: .local/backups/keydous-nj98v4s-gaming-20261105.zip.
## Completed validation and delivery

Cached full native suite PASS. Receiver test covers all29 I/O failure/cancellation
positions, independent holds/releases, no added waits, cached target, late-reply
recovery, no-reply1B controls and stream/table ordering. MSVC compilation caught
Windows Yield macro collision; renamed helper YieldBusy and added Windows headers
to the receiver regression. That affected test was recompiled and passed.

Ordinary release workflow completed: support-diagnostics gate, exact-image full
catalog/onboard/support/backend/resource checks, embedded licenses, production
profile/autoselection/layout catalog, all87 Supported layouts against the fresh
Sheet, and recovery18 scenarios plus repeated startup PASS. The built catalog
contains97 V4S ANSI keys. No successful tests were rerun merely to restart a
failed build. After the compile fix, remaining steps resumed from the standard
build_release.ps1 body; a local resume-script path mistake interrupted the profile
invocation, then only unexecuted profile/recovery/publish steps resumed. Receipts:
native-checks.log, build-release.log, build-resume.log and build-finish.log.

Delivered EXE: build/bin/Release/x64/HallJoy.exe, version1.6.14.0,
11335680 bytes, SHA256 `5f667be6c02d8389e518fd7adb63f2d421ba5ba23192d55d4f46162533582040`.
Installed bytes equal the gated Release candidate. This supersedes the previous
142fe707 local EXE. No GitHub upload. All approved functions retained.

Tester workflow: close vendor configurator, unplug USB for RF testing, exercise
fast partial presses, holds, simultaneous keys and full releases, compare wired
using the SAME EXE, then return normal HallJoy.log via Open log. New log must show
revision2/fixed_delay_us=0 and will distinguish stream delivery from table reads.
No scripts, archive, firmware flashing or separate diagnostic export required.
