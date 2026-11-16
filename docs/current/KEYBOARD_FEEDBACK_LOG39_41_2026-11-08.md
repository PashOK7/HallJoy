# Keyboard tester feedback: HZ-94, F68 and NJ98-CP V4S

2026-11-08. Downloaded logs and screenshots supplied by the owner. Private raw
reports and SHA-256 manifest: .local/research/logs39-41-20261108/.
This is log/source analysis, not an agent hardware session.

## E-YOOSO HZ-94: successful admission and gameplay

Log39, 77,024 bytes, SHA-256
`99344ecc54a27a4a735f33918f9ed21f73c9f76300a80a2e2c0eead24355bd19`.
Report UTC 2026-11-08T08:05:21Z, HallJoy 1.6.15.0; four complete snapshots.
0416:7372 interface2, FF1B:0091, 64-byte I/O. Exact M484 / 7187_AUSXHERGB /
V1_13_05 identity and scale40 pass. Admission revision2 continues with all94
physical channels, ignoring the ten nonphysical assignment entries. Subscription
reaches a first valid zero-depth report in187ms. By35.062s the stream summary has
340 samples, ten releases, peak raw40 (full reviewed range) and peak five active
channels. Final provider has453 updates, zero failures and no communication
warning. Bound axes reach full signed output and return to center. A small25/1000
residual in one traced input is below the configured deadzone and produces no
axis output; it is not a stuck gamepad axis in this trace.

The tester explicitly confirms working Cyberpunk gameplay and depth-responsive
Gamepad Tester bars. This supersedes the private trial awaiting hardware feedback.
Supported scope: exact HZ-94 USB ANSI / built-in94-key automatic preset. Firmware
V1_13_05 on0416:7372 is the observed hardware, not a new firmware-version pin or
proof of unrelated retail variants. Existing exact identity/transport/scale gates
and physical matrix admission remain unchanged. Only the testing notice is removed;
MIDI and the full retained catalog remain in the ordinary EXE.

## FREEWOLF F68: duplicate of the already reviewed result

Log40,130,643 bytes, SHA-256
`6e3e07d7f729f4c9d3895cc20984c25e591ed5b83a0ade5621ed40bb54776f80`,
is byte-for-byte identical to log37. Report UTC is2026-11-07T17:06:57Z.
It is not a new test or a regression after the Supported change. Existing result:
3151:5029 board2634, USB4.02,111,583 updates, zero provider failures. The original
report has empty bindings; it cannot add a new bound multikey gamepad claim.
F68 remains Supported; F68 PRO remains separate. See
[the accepted F68 decision](FREEWOLF_F68_2026-11-07.md).

## Keydous NJ98-CP V4S: live connection does not prove releases

Log41,569,680 bytes, SHA-256
`e967dc45c5407d2edc199733049a94b042cfb9ffd72bb99495a7e33c61524093`.
Report UTC2026-11-07T13:57:22Z, HallJoy1.6.14.0; eight complete snapshots,
no writer queue loss. Receiver3151:5038 reaches connected on the RongYuan stream;
97 mapped keys,16,254 combined updates at the final snapshot, zero provider
failures and communication warning0. At least5,680 direct stream packets are
reported. This agrees with the tester's improved startup/connection observation.

The tester reports sticking, especially with six simultaneous keys. The trace
confirms persistent nonzero NATIVE depth before the curve/gamepad conversion,
while other keys continue changing. One channel remains278/1000 across13.844s.
At the end two other channels remain108 and440/1000; their last digital-up events
are9.922s and7.453s before the final pad trace. Their curve outputs31 and396/1000
leave the right stick at1009/12976 while the left stick returns to center.
Digital up alone is not physical analog ground truth, but these concurrent native
plateaus, nonneutral output and the tester's explicit release complaint agree.
This is not merely a tester visualization problem or a curve creating a value
from native zero. The log cannot distinguish missing/misdecoded release packets,
stale table reconciliation or keyboard/receiver firmware behavior.

The last completed table summary has average1.598s and maximum12.016s per sweep,
92 pages and174 command-channel transients; at that point there are zero applied
table corrections and three superseded entries. Later stream counters continue,
without later completed table summaries. This does not establish a six-key limit,
the exact time of a lost zero, or that no additional table page ever completed.
The report lacks revision4/5 bootstrap/mode/table_order markers. Its shared
application version alone cannot identify the exact RF implementation. Do not
claim this is a confirmed revision5 retest or dismiss its sticking solely as an
old EXE without further binary/revision evidence.

RF remains Experimental. Next investigation should replay received per-slot
stream/table order and examine release/partial-packet handling against the vendor
path. Record slot freshness/order/loss and reconciliation outcomes in bounded
ordinary diagnostics if existing evidence is insufficient. Do not replace analog
with digital-up clearing, a held-key timeout, rate reduction or guessed firmware
blame. No Keydous runtime change or speculative fix was made in this log review.

## Support synchronization and validation

HZ-94: README, hardware overview, Supported layout manifest and exact-model
runtime notice reconciled locally. The existing compiled catalog passes all90
Supported layout records against the intended Sheet state. The planned Sheet
insertion is E-YOOSO / HZ-94 / Supported at Main237 plus a separator; all existing
model values must be preserved. The plan also fixes the already-stale F68 B:C
base foreground colors to match its existing green Supported state. No comments
or notes are added. A failed initial connector write was followed by readback proving no insertion
had occurred; the unchanged grounded batch then succeeded once. Independent native
readback confirms Main!A237:C237 = E-YOOSO / HZ-94 / Supported, strict validation,
matching base/effective green B:C,32px model row and16px separator. Existing model
sequence, all original values and validations are unchanged. The two F68 base
foreground fields are corrected at their shifted B314:C314 addresses.

Full native structure/presentation audit PASS:152 blocks, zero issues. Live Sheet
matches all282 yellow runtime entries and all90 Supported layout records;670
model rows. Source and native snapshots, reviewed requests and actual after-model
rows are retained in the private task directory. Owner evaluates rendering; no
visual run is claimed.

Cached full native static/portable suite PASS. Ordinary release build PASS:
independent full catalog, exact-image tester admission gate, embedded resources,
profile transactions/layouts and18 recovery scenarios plus repeated startup.
Installed EXE equals the verified candidate; the previously running app was restored.
Version1.6.15.0, 11,558,912 bytes, SHA-256 `5c3cf550326bb5dfb89e1efc2a078fe872c7af139d5b4ac4c162298646717d9f`.
Delivery: build/bin/Release/x64/HallJoy.exe. This removes the HZ-94 testing notice;
it is not a new Keydous RF fix. Existing MIDI M1 and retained keyboard paths remain
in the complete build. No GitHub publication was performed.

Backup: .local/backups/logs39-41-supported-20261108/manifest.json.
No GitHub release or change to Keydous/F68 support scope is requested here.


## 2026-11-09 — NJ98-CP V4S 2.4 GHz retest, log 47

Downloads HallJoy (47).log, 377859 bytes, SHA-256
51ed5a5ebbb5ec66e2b3c58ff473771447fe60949b89fa50c80394db7b56303f.
The report header 2026-11-09T20:04:18Z matches the tester's attachment.
HallJoy 1.6.15.0, five complete snapshots, receiver 3151:5038, 97 mapped
keys, 17299 combined updates and zero provider failures at the last
snapshot. At least 5336 direct stream packets. After an initial 8F
read-ready timeout, all captured connected snapshots show no communication
warning.

Tester reports a residual after releasing three or more keys together.
At uptime 5952234..5952265 the trace records a cluster of digital releases.
At 5952281 the affected bound keys include native depths 135 and 373 out
of 1000, held at those exact values across 22656 and 22203 ms of observed
pad rows. The right stick simultaneously shows RX 1982 and RY -10561.
Both channels change when the respective keys are pressed again. Other
channels keep updating. The digital release timing corroborates the
tester report and does not define true physical depth.

E5 reconciliation does work in this run. At 5941796..5941828 and
5949796..5949828, table-zero corrections coincide with bound channels
returning to zero, and the correction count reaches 8. Last complete
summary: 456 pages, 139 transients, 298816 us average and 6180844 us
maximum sweep, 8 superseded slots. No later complete-sweep summary is
present despite stream events through uptime 6009046. The absence
does not establish that no later individual page completed. Output
curves are downstream of the recorded persistent native values.

The ordinary report contains early evidence only. The continuous writer
appends ordinary events and traces after its initial reset; the separate
evidence store is serialized only when rewriting a snapshot. The file
ends with a clean session.end, so the missing RF markers cannot be
recovered by requesting a snapshot from this session. The version and
Discord attachment size are consistent with the newer EXE, but the log
does not give an exact SHA or retained mode revision.

Root cause remains open: distinguish a lost or delayed zero delta, a
zero table reply rejected as superseded, and a table that returns no
usable zero or is stalled in the feature exchange. A future build should
preserve bounded per-slot source/order and pending-page outcomes in the
ordinary log, including when the continuous file is copied directly.
Keep stream timing and quiet holds intact. No runtime change follows
from this log review alone. RF remains Experimental.

## 2026-11-09 — RF release trace revision 6

This build is a measurement for log47. Admission, commands, table ordering,
quiet holds and analog timing policy stay as in revision 5. No synthesized
release, extra E5 query, timeout change or polling-rate cap is introduced.
Trace activation requires the existing proven receiver 3151:5038, remote
board 3793 and routing ownership. The table and interrupt readers store
observations in memory. The existing support writer samples once per tick
and serializes at most 12 bound slots, four pages and one health line.
No text formatting or disk writes are added to the input receive loop.

Interpret the residual in trace.pad against trace.os_key and
trace.keydous_rf.slot for the matching session, HID and physical slot:

- stream_zero_ms and stream_zero_applied_ms separate a valid received zero
  from zero applied by the interrupt path. physical_milli is the selected
  publication's stored slot value; effective_milli includes freshness and
  the maximum of aliases for that HID.
- table_ms/table_raw/table_accepted describe the most recent decoded table
  result, with table_request matched to the page attempt count.
  table_zero_ms, table_zero_applied_ms and skipped_zero_ms preserve the
  last evidence for each outcome across repeated snapshots.
- drained_ms/drained_raw/drained_zero_ms describe a reply consumed by the
  existing late-reply discard path. This is the host's association with
  its outstanding E5 page, since that response has no page echo. An
  unsolicited reply is never attributed to a slot. A drained reply never
  changes input.
- Page attempts carry the pre-write send marker, end/last valid reply
  times, read-ready/parse outcome, pending flag, last failure and drain
  counters. F7 bits are online=1, send=2, read=4, boot=8. Stage numbering
  is ry_receiver::Stage; snapshot_reason is 1 report ID, 2 echo, 3 range,
  4 missing scale. An active attempt has end_ms=0. Times are uptime ms,
  zero means unseen, and applied_source is 1 stream or 2 table.
- The health row identifies revision, session, units, and structural
  rejects/other reports/I/O errors. A missing zero in accepted Windows
  HID input cannot by itself separate keyboard MCU, RF, receiver MCU,
  Windows HID queue, or a report rejected before a slot is recoverable.

Trace output uses the existing bound-key scope, ordinary bounded queue
and trace window. It includes no unbound key identities or HID payloads
and does not force file creation. Continuous append and Open log both
serialize the new records. Logs copied before opening a manual snapshot
remain useful when continuous logging was enabled.

Test in HallJoy over 2.4 GHz. Press and release keys as usual and observe
whether input lags or sticks. Click Open log and send HallJoy.log in either
case. A reproduced symptom is enough to request the report immediately.
The writer samples current receiver evidence before completing the report.
An unfinished E5 request is recorded as pending. Its eventual outcome is
unknown at the time of the snapshot.

Local tester build verified 2026-11-09. `build/bin/Release/x64/HallJoy.exe`
is version 1.6.15.0, 11,609,600 bytes, SHA-256
`e91f5077c3c5d911f8d47feedd2379bbb7b163e79b3e3fb2276790a073751777`.
The installed image matches the exact-image tester receipt. The Keydous
simulation passed four scenarios and three negative cases. The native suite,
Windows log append and snapshot regression, production profile/layout gates,
and startup recovery passed. The compiled Keydous ANSI preset has 97 keys,
and the identity test resolves the receiver to that preset. Hardware testing
of this build is pending.

## 2026-11-14 — revision6 tester result

[Log51 review](KEYDOUS_NJ98_V4S_LOG51_2026-11-14.md): observed zeros are applied,
while a retained residual is also returned by fresh accepted table replies.
Exact firmware/RF/cache root cause remains unproved; wireless stays Experimental.
