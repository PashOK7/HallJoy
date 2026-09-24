# AJAZZ AK820 MAX firmware review — 2026-09-20

## Target and current boundary

Owner ultimately clarified: AJAZZ x NACODEX, white/lilac, RGB backlight,
no screen, no Bluetooth. The earlier no-backlight answer was corrected.
Do not apply SG8994HE firmware conclusions to this RGB keyboard.
No runtime support, build, flashing or device communication was performed.

The V2.06.01 wired desktop driver and illumipc device registry distinguish
SG8994HERGB (RGB) from SG8994HE (no light). Both appear under 0416:7372
in the illumipc registry; product identity is required as well as VID/PID.
The Driveall registry also contains AK820MAX USB revisions under 0C45 with
PIDs 8033,8044,8070,809B,80A0,80B0,80B1. The owner's exact identity remains
unverified. Branding/color alone does not establish the controller.

## Downloaded evidence

All artifacts are in `.local/research/ajazz-ak820max/`.

- Wired installer archive: https://cdn.shopify.com/s/files/1/0554/6678/6869/files/AJAZZ_AK820_MAX_RGB_Installer_V2.06.01.zip
  SHA256: 6a20f82e7b0c850a0eff9c5de78899a875d9466cbe9882d458a63c24803a6de6.
  Extracted with 7-Zip and innoextract, not executed. No matching RGB flash
  image established in its extracted resources.
- Registry/client: https://www.illumipc.com/app.js
- Firmware manifest: https://www.illumipc.com/config/firmware.json
  Contains SG8994HE V1.13.02, not SG8994HERGB at time of inspection.
- Downloaded firmware: https://www.illumipc.com/config/firmware/QZ034CKB_M484KBSG_SG8994HE_V1_13_02_CS_6DC5.rar
  SHA256: dfe75b0d08242d79fe983e63a770667a2c6ef954483ce1af78216713892d03ad.
- Extracted updater contains 512 KiB flash at file offset 0x2ce808.
  Saved as SG8994HE_V1_13_02_flash512k.bin.
  SHA256: fd42c6f691e353a4411c6c13023f7ee3eccd7726bb7b3ac28ee4fe62df24f680.
  Identity: M484,01,KB,SG,SG8994HE,V1.13.02.

Source navigation: https://ajazz.net/pages/ajazz-drivers and
https://www.ajazz.ru/programmnoe-obespechenie/ link relevant wired software.
The latter separately labels wired RGB magnetic and no-light magnetic versions.
Official catalog https://www.a-jazz.com/en/h-col-159.html requests product
barcode suffix for exact matching. Direct retrieval returned 403; searchable
page did not provide the required RGB firmware download.
Driveall firmware API requests returned code 0/data null for attempted inputs;
this does not prove firmware absence, since exact device metadata is unknown.

## No-light firmware findings only

The reviewed M484 image has real depth events, not merely digital key states.
Analog command handler 0x9fe4 uses wire opcode 0x21. Subcommand 2 sets 22
subscription masks (6 row bits); subcommand 3 clears them. Calibration commands
are separate. State base is 0x20003100; depth cache is base+0x125+row*22+column.

Accepted-change fragment 0x88d4..0x8912 writes the depth cache, marks pending
bit 22 at 0x20000400, and replaces the single pending row/column at base+0x10d.
Serializer 0xa428 reads only that last position. Complete scheduler 0xa5de
clears the serviced pending bit after preparing the report.

`tools/review_ajazz_ak820max_firmware.py` executes the real accepted-change
fragment and complete report scheduler in Unicorn with synthetic RAM. PASS:
changes (1,2)=17 then (3,4)=29 yield only the latter event; release (1,2)=0
then (3,4)=30 also yields only the latter. Both cache values remain in RAM.
Packets begin 01 21 00 00 00 03 01 03 04 1d / 1e.
This proves conditional overwrite if two accepted changes precede service;
it does not establish physical scheduling or a measured hardware failure rate.
It is not a full scanner emulation or an RGB firmware test.

Reviewed control replies at 0xa25c: subcommand 4 reports nominal 40;
subcommand 5 reads per-key settings, not live depth; subcommand 8 returns a
22-byte bitmask, not a depth array. No complete live-depth snapshot found in
this handler. Other routes are not exhaustively excluded. Similarity to
IROK M484/Witmod is a research lead, not support confirmation.

## Next useful evidence

Obtain the actual RGB firmware package or exact USB product identity and the
working vendor driver URL. Analyze SG8994HERGB independently. Do not flash the
no-light image, add an automatic profile, or declare analog impossible on the
basis of the present evidence. Application and public support table unchanged.

## Follow-up: command execution and matrix traversal

Owner explicitly requested continuing the downloaded no-light firmware review
while the actual target remains RGB. Added
`tools/review_ajazz_ak820max_routes.py`; both offline scripts PASS.

Executed real command handler 0x9fe4 and reply code with synthetic RAM:

- Subscribe (2) and unsubscribe (3) change masks without clearing the 132-byte
  cached depth array. Subscribe sets reply bit 21, not event bit 22. Its ACK
  does not contain a state snapshot.
- Valid addressed request (5), row 1 / column 2, returns the same five settings
  bytes at live cached depths 0,17,40. Source is configuration 0x20002378,
  not live depth state. No out-of-range requests tested.
- Handler jump-table coverage: 0/1/12/13 write settings; 2/3 subscription;
  4 capability; 5 settings read; 6 reset-related path; 7/8 calibration;
  9/10 settings paths; 11 returns without a reply. This is the 0x21 handler,
  not an exhaustive review of every USB opcode.

Executed the matrix traversal from 0x83b4 through 0x8ad0 using real flash
pointer tables 0x1352c (physical scan), 0x1393c (logical coordinates), and
mask bytes 0x1fe60. Synthetic key-object coordinate fields and ADC/calibration
RAM are initialized by the harness; firmware startup is not emulated. Only
initial wait 0x5c56 is stubbed; execution stops before post-scan processing.
ADC source is 0x20002070 (values shifted right by 3 in the scanner), baseline
0x20001d48, filtered samples 0x20002170. Subscription covers all row bits.

Four selected keys with synthetic samples 360/350/340/330 against baseline400
produce depths 11/13/16/17 in the fourth traversal. The real report scheduler
then emits only (row3,col0)=17. Next, samples400/320/340/330 yield release
(row0,col0)=0 and (row1,col0)=19 in one traversal; only the latter is emitted.
Eight additional stationary traversals emit no correction. Unsubscribe then
resubscribe followed by eight more stationary traversals also emits no
correction. This demonstrates persistent loss under the tested service order.
The initial 385 sample did not pass its longer near-release filter within eight
passes; the final reproducible scenario uses the values above. This was a test
input adjustment, not a firmware modification.

The report service is called from the loop at 0x78e2. Physical interrupt/task
interleaving, USB backpressure timing and full startup remain unverified.
Do not describe this as a complete device emulation or measured hardware rate.

Conclusion: independent depths exist internally, but the tested ordinary event
route lacks a demonstrated reliable recovery mechanism. A production backend
based only on it is not justified yet. Other top-level read commands still
need investigation; no universal impossibility claim is made. No runtime,
firmware, public sheet or supported-model changes.

## HallJoy (19).log supplied by owner

Report version 1.5.3.0; UTC 2026-09-20T13:54:11Z. Session spans69.485s.
Contains HID candidate0416:7372 on interfaces00/01/02, consistent with the
Witmod registry but insufficient to distinguish SG8994HE from SG8994HERGB.
Product identity and device firmware version are absent. No depth reports.
All60 support snapshots show analogue_connected=0; no supported keyboard
was admitted. This is not evidence that firmware cannot provide analog.

4464 device.change events and67 child starts/67 exits are recorded. Exit
3762832470=0xE0484456 matches kHostExitDeviceRefresh in analog_host_client.cpp:
controlled replacement for device refresh, not a crash exception. Final
restart counter66. The event storm and repeated refreshes require separate
runtime investigation; source of Windows device notifications is not identified
by this log, and cannot be attributed to the AJAZZ keyboard alone. Current
source still forwards generic topology notifications into the refresh queue;
no claim that upgrading automatically fixes this issue is supported here.
No firmware commands were evidenced by this metadata-only report. No app change.

## Tester build — diagnostic revision 1

Owner requested a test HallJoy.exe after supplying log19. Delivered to existing
`build/bin/Release/x64/HallJoy.exe` (this delivered file is diagnostic, not the
ordinary release). Rebuild: `tools/build_ajazz_diagnostic.ps1`.

Compile flag HALLJOY_AJAZZ_DIAGNOSTIC is opt-in. Ordinary builds do not enable
these changes. It reuses the existing bounded overlapped HID transport and
native worker lifecycle. Exact USB shape0416:7372 / FF1B:0091 /64-byte reports
is logged and checked. An identity response must name M484 and SG8994HE or
SG8994HERGB before capability and subscription commands. Unknown identities
are not subscribed. Commands are identity0D, analog capability21/04,
subscribe21/02 and unsubscribe21/03 only. No calibration or settings writes.

Depths are diagnostic only: no guessed factory map, digital fallback, or AJAZZ
analog publication to the gamepad. Normal HallJoy/gamepad functionality remains.
The title reports searching, collecting or basic data collected (four positions
with release evidence); this does not claim simultaneous-value correctness.
The test has no elapsed-time stop. First4096 events retain row/column/raw/order;
per-position counts, releases, maximum and last value continue indefinitely,
with progress every2 seconds and final summaries. Cached active count is
explicitly not proof of simultaneous presses because releases may be lost.
HID metadata, identity/capability failures and transport errors are logged.
Logging is automatic to HallJoy.log beside EXE, independent of optional logging.
Diagnostic matrix/depth data is recorded, not arbitrary HID text/payloads.

This build suppresses UAP restarts requested by generic Windows topology events,
while retaining initial UAP discovery and native discovery. It isolates the log19
refresh storm for this test; it is NOT a general production hotplug fix. UAP-only
hotplug therefore needs application restart in this focused build.

Validation: mock transport tests include actual diagnostic function and actual
protocol decoder/builders:8 scenarios PASS (stream, open failure, write failure,
identity timeout, unknown identity, subscription failure/cleanup, disconnect,
capability failure). Real firmware offline tests remain separate evidence.
Final EXE passes existing linked-image shark/mini60/NA87/resource checks.
Source encoding audit PASS. Hardware behavior and visual UI not tested locally.
Existing compiler size-conversion/PDB warnings were not introduced here.
Backup: .local/backups/ajazz-diagnostic-before/.

Tester: close vendor configurators, launch EXE, press/release several keys
individually, hold a few together, vary one while holding another, release all,
close HallJoy and send HallJoy.log. No settings change or fixed duration needed.

Delivered SHA256: 78fe9cc7a25a5d87c6ab3c7af41802b008cafbe9796d10348b61255b8f299b1e

## Log destination clarification

Owner explicitly requires the diagnostic log beside HallJoy.exe, not AppData.
The actual stability sink already used the executable directory. Corrected
AJAZZ-only SupportLog_Directory and DebugLog_Path metadata to the same location;
no silent AppData fallback. Ordinary release storage is unchanged. Rebuilt and
four linked-image checks PASS; self-test HallJoy.log observed beside candidate.
Latest delivered SHA256: 510446a8a0753d0b3f7d613010b7231f509d949f4b60e902d32f344c0d52ce3d.

## Log20: missing diagnostic coverage and log-filter repair

Owner supplied HallJoy (20).log,1850 bytes/11 structured records. It contains
startup, storage/layout initialization and window.close/shutdown.begin at40.750s.
No AJAZZ worker start, interface, identity, depths, support snapshots or session
end. Absence of UAP exits is NOT proof the restart issue is solved. Missing
session end does not by itself prove a crash; the supplied file can be incomplete.
The storage root localappdata record concerns settings, not this trace location.

Confirmed diagnostic defect: KeepSingleLogDiagnosticLine selected the global
NA87-native branch and discarded [support] records even though the AJAZZ build
uses that shared sink. Added an AJAZZ-specific branch retaining support and
NA87 diagnostic lines. This repairs missing observability, not an established
cause of the absent AJAZZ worker. Do not claim the hardware test succeeded.

Added linked-image logging sentinels to the isolated NA87 self-test and a build
gate that reads the actual HallJoy.log beside the candidate, requiring both
support and structured AJAZZ records. Gate and all four EXE checks PASS.
Latest delivered EXE SHA256:
b215446f7f83f56264f722e8f94107fa6a333a1787b73937edb8bb8303f4cb10.
Need a fresh complete run to determine engine state/device admission. Previous
mock transport tests did not cover this log filter; new gate closes that gap.

## Full local verification after insufficient log20 coverage

Added AJAZZ-only per-provider prepare entry/exit, engine-operation entry/exit,
and independent UI health (worker/status/stop flags every2s). Runtime ordering
unchanged. Diagnostic-only --halljoy-ajazz-startup-smoke requests normal WM_CLOSE
after5s of UI activity. Normal tester invocation has NO timeout.

Actual delivered application was tested locally with Keychron attached. Initial
hidden smoke could not be closed through external window enumeration; its
verified process was terminated and is NOT a successful shutdown test. The
subsequent automatic smoke used normal window close and exited0 after5.56s.
Log assertions PASS: AJAZZ worker start, provider probe stages, UI health,
support snapshots, successful engine resume, UAP ready/devices1, virtual pad
start, shutdown and session.end0. No 0xE0484456 refresh exit. One provider-plane
resize exit0xE0485632 is expected. No physical key input or gameplay was tested;
analogue_connected remains0 in captured support snapshots. Do not claim live
Keychron input validation from this startup check.
Evidence: .local/research/ajazz-ak820max/startup-smoke-final.log.
No visual evaluation and no AJAZZ hardware present locally.

Expanded real-diagnostic-function/mock-transport tests to11 PASS scenarios:
previous8 plus >60s simulated idle without cutoff, malformed-coordinate
rejection, and5000 events with4096 detailed events plus continuing aggregates.
Four linked-image checks and support/structured log-route sentinels PASS.
Log20's missing worker start remains unexplained; instrumentation makes a
recurrence actionable, not proof of a fixed hardware/runtime cause.
Final delivered SHA256:
20101bd49d5963a9b555b680ff3e9aeb87f289d508ed00201b118b5e6f9fccd7.


## Log21: RGB identity and real depth events confirmed

Tester HallJoy (21).log identifies M484 / SG8994HERGB / V1.13.17,
VID0416 PID7372, vendor usage FF1B:0091, 64-byte input/output.
Capability nominal=40. Received 2192 depth events across31 logical positions,
with intermediate values and releases. This establishes real RGB depth output,
not complete simultaneous-state reliability. Eleven positions retain nonzero
last values at shutdown; without known physical state this does not prove a
missed release or justify substituting digital input. Diagnostic code explicitly
logs depth only and does not publish it to preview/gamepad. Missing analog
preview is therefore expected in this build.

Important startup defect: enumerate starts125ms; mad68-a0 takes30094ms,
addressed-099402 takes30095ms, aula-w669-adaptive takes27484ms. AJAZZ worker
starts88032ms, stream88047ms; session closes114391ms. This provides a plausible
explanation for log20 ending40.75s before any worker, not proof of identical
behavior in that earlier run. Fix provider probing/admission before another
short tester run. Error995 appears during cancellation at shutdown; unsubscribe
succeeds and application/session exit0. No claim that the tester held/released
specific keys simultaneously is supported by this log.

## Further no-light firmware research: command0x23 raw matrix stream

New executable evidence: tools/review_ajazz_ak820max_raw_stream.py exercises
actual dispatcher0xa5c8, handler0xa538, scanner0x83b4 and scheduler0xa5de.
Enable report bytes[0]=1,[1]=0x23,[6]=1; disable byte6=0. Handler changes
USB state bit20; tested calibration flags remain unchanged. Scanner writes
unshifted raw ADC into logical array0x20002798. Serializer0xa54a sends six
rows: report[3:5] big-endian row1..6, byte5=44, bytes6..49 contain22 big-endian
uint16 values. Repeated stationary sweeps and four independent changed inputs
PASS; all132 slots match source RAM. Rows read live RAM, not an established
atomic full-matrix snapshot. Synthetic ADC/RAM and simulated USB completion;
no physical hardware, interrupt timing or latency claim.

Vendor illumipc.com/app.js openADC/closeADC independently uses opcode35,
collecting six rows of22 uint16 values. This is raw sensor output, not calibrated
travel. Firmware depth calculation depends on per-key baseline and bottom
calibration; ordinary0x21/sub5 excludes those calibration fields. Accurate
normalization remains unresolved. Current delivered EXE tests0x21 only,
NOT0x23. RGB implementation of0x23 remains unverified; downloaded image is
SG8994HE V1.13.02 no-light. Indexed official RGB V1.13.19 upgrade name was
found, but download was not recovered (official site403); no RGB binary claimed.


## Play candidate revision2 and enumeration cleanup

Owner requested startup fix and playable analog via hardware-confirmed0x21.
Candidate now reads0x10 device mapping before enabling publication; incomplete,
invalid or empty mapping rejects play rather than guessing NA87 positions.
AJAZZ allows duplicate HID assignments; existing NA87 strict default unchanged.
Actual per-position0..40 events drive physical publications/registry/gamepad;
no digital fallback, calibration commands, guessed endpoints, idle expiration
or0x23 use. AJAZZ never publishes the NA87 automatic-layout token. Layout
selection remains manual for this candidate. Read map applies current device
assignments; a separate AJAZZ factory layout/remapping profile is not provided.
Log stays automatic beside EXE, without observation timeout. Session end or
transport failure clears publication; error samples above40 do not alter output.

Enumeration review found synchronous string queries before vendor/fingerprint
filtering in MAD68 and addressed providers; W669 also queried three strings for
logging. MAD68 now rejects other vendors before string requests. Addressed no
longer requests unused strings. W669 no longer requests strings in enumeration;
its product-name fallback is preserved and queried lazily ONLY after firmware
identity fails. No known-device identity gate was removed. Log21 stage timing
is consistent with these blocking queries; exact individual USB request timing
was not captured on tester hardware, so final remote latency remains unverified.

Validation:12 mocked real-session scenarios PASS, including mapping rejection,
publication callbacks, malformed positions, transport failures, cleanup, >60s
idle and5000 events. Linked-image NA87 self-test additionally exercises real
AJAZZ map binding/publication, duplicate assignment maximum/release isolation,
registry routing, invalid depth rejection, no NA87 layout token and disconnect
neutralization. Four linked-image checks and log-route sentinels PASS.
Local hidden runtime smoke: enumeration203..235ms, AJAZZ worker250ms, normal
exit0 at5438ms with local Keychron. Not physical AJAZZ validation. Evidence:
.local/research/ajazz-ak820max/startup-smoke-play.log.

Delivered build/bin/Release/x64/HallJoy.exe SHA256:
74e1673dc5f6c366345975778fd433b70618933ea92f6e09ae5dc896a192d185.
This is an instrumented playable candidate, NOT proof of lossless simultaneous
RGB events. Firmware-event missed releases cannot be repaired by silently
inventing digital values or timeout releases. Existing log21 nonzero final
positions remain unresolved; a physical play/release test is still required.


## Log22 and tester report: play candidate NOT ready

HallJoy (22).log:289665 bytes, session55.719s, normal exit0. MAD68 and
addressed enumeration delays removed (each approximately1ms). W669 still takes
15.515s; total analog admission19.766s, including an initial identity timeout.
Startup is improved, not fixed completely. Map accepted; play.begin and
1298 depth events/7 positions logged. Support snapshot confirms connected
analog device; virtual pad generation ready. Six final positions are zero;
row0,col10 remains2/40 (max3), no events after about50s. User explicitly reports
stuck analog after release and Fn not detected. This is a hardware-observed
failure of the candidate, not release readiness. Firmware event loss is a
credible cause based on no-light emulation, but exact RGB mechanism unproven.

All map.position hid values were redacted as hid=* by trace privacy filtering;
Fn assignment cannot be reconstructed from this log. Do not infer absence of
Fn or map it from NA87. Investigate map visibility/actual special-key handling.
Do not fix missed releases with digital substitution or an invented timeout.
Need an authoritative refresh path (candidate0x23 full raw rows, still untested
on RGB and requiring normalization), or prove another lossless depth route.


## Log23: stronger residual-depth evidence

Tester supplied log23 specifically for sticking and Fn:880685 bytes,5297 events,
37 positions, normal90.109s session. Four final cached depths remain nonzero:
row2/col3=17 (no releases recorded), row3/col3=16, row5/col6=1,
row5/col11=2 (no releases recorded). 17/40 and16/40 are42.5% and40%, so this
is not merely a near-zero deadzone issue. Detailed event records stop at4096;
aggregates continue to5297. Do not invent full final event sequences past cap.
Map HID fields remain redacted; Fn location/assignment still cannot be proved.

W669 enumeration6.063s, play starts6.453s in this run (variable remaining delay).
Spark protocol_probe warnings recur about every2s while AJAZZ is streaming;
these warnings alone do not prove same-interface contention or packet theft.
Review cross-provider admission/routing before attributing all loss to firmware.
Normal unsubscribe and shutdown; error995 belongs to shutdown cancellation.


## Revision3: dual-stream research candidate, not playable

Owner approved the replacement approach and requested a new EXE. Delivered:
build/bin/Release/x64/HallJoy.exe SHA256
f6dfd99bec481d5f4aa9b128d572854832f225d420caf607ae08ed7e1067ef34.

Routing defect corrected: diagnostic previously opened AJAZZ without owning
its interface in NativeAnalogRouting. The M484 backend now admits exact
SG8994HE/SG8994HERGB identities in this diagnostic build and claims the path
after capability40 and complete device map validation during Prepare, before
W669 and runtime Spark enumeration. Both providers already skip another
protocol's claimed paths. Worker also establishes/rechecks the claim before
diagnostics, covering hotplug. Ordinary release identity admission unchanged.
This prevents subsequent probes of this claimed interface; no claim that all
Spark warnings in earlier logs were proven to target this device.

Session reads/logs0x10 mapping as key_code (public numeric assignment, not
private HID path). Enables existing0x21 events and new0x23 raw rows together.
0x23 decoding requires ID1, opcode23, reserved byte2=0, row1..6 and44-byte
payload;22 big-endian uint16 samples per row. Records each row at up to10Hz
plus per-position min/max/last/count/sum and bins against cached0x21 depth.
Bins explicitly marked simultaneous=0: cached events may be stale and must
NOT be treated as calibrated simultaneous samples. Full rows can have differing
acquisition times. No calibration/flash/settings writes. Raw enable write is
NOT considered proof of support; received rows/counters provide that proof.
Raw disable is attempted even if enable send fails; depth unsubscribe retained.

AJAZZ gamepad/preview publication intentionally disabled in revision3, including
old lossy depth publication. Other HallJoy functionality remains available.
No digital substitution, release timeout, guessed calibration or test time
limit. One automatic HallJoy.log beside EXE. UI basic-coverage status requires
four depth positions with releases plus all six raw rows and at least four
changed raw positions; this is not proof of complete protocol correctness.
Fn requires tester input; no guessed mapping from NA87.

Validation:15 actual diagnostic-function/mock-transport scenarios PASS, plus
full six-row parser checks, repeated values/changed samples, invalid row/length,
raw-enable failure cleanup, no analog publication, event-only unsupported-raw
case, long idle,5000 events and mapping failure. Existing no-light firmware
emulation confirms raw-row protocol again. Four linked EXE checks PASS; log
gate now requires exact key_code and raw-sample sentinels survive privacy filter.
Local hidden startup smoke: enumeration235ms, worker250ms, exit0 at5485ms.
No AJAZZ present; RGB raw support and remote routing latency still unverified.
Evidence: .local/research/ajazz-ak820max/startup-smoke-raw.log.

Tester action: close web configurator, run EXE, fully press/release several keys
individually; hold several together and vary/release them separately; include
Fn alone and Fn+number. Close and send log. No game test requested for revision3.


## Log25: RGB raw stream confirmed; stale depth has independent evidence

HallJoy (25).log802410 bytes,32.359s session, SG8994HERGB V1.13.17.
Revision3 claims interface at297ms; W669 probe returns immediately; research
and raw enable start406ms. Received189706 valid0x23 rows, mask63, invalid0,
about31618 observations per row.82 positions have nonzero ADC;132 logical
slots transmitted. This is not a claim of82 intentional physical key tests:
changed_positions also counts sensor noise. Raw stream is now confirmed on RGB,
not just no-light firmware emulation. Both unsubscribe and raw disable succeed;
normal shutdown0. Raw data remains live per-row, not atomic matrix snapshots.

Depth0x21:1297 events/12 positions. S (row3,col3,key_code22) finishes at30/40,
while raw final2422 lies in that key's observed depth0 ADC range2378..2471.
Cached-depth30 bin includes5986 raw samples, min1928/max2494/mean about2417.
This is strong evidence that the event cache can remain pressed while the raw
sensor returns to its release range, even after interface claiming. Exact
firmware/transport cause still not exhaustively isolated; do not label this a
working lossless event protocol or invent a time-based release.

Fn is present: row5,col11,key_code250 (0xFA),257 depth events,max40,last2;
raw min1289,max2410,last2345. Firmware does expose this position in both
streams; prior missing Fn display is not proof of missing sensor/protocol.
Investigate HallJoy layout/assignment routing. Numeric mapping is now visible.

Map has107 nonzero assignments but only82 nonzero-sensor slots.25 assigned
slots have zero ADC throughout this run (including extra numpad positions).
Do not turn the larger firmware matrix into107 physical keys or assume all
assigned positions exist. Need a verified82-key physical map/validity policy.
Spark failed-probe warnings persist for other/no eligible candidates; warnings
alone do not prove access to the now-claimed AJAZZ interface.

Next implementation: reliable repeated raw states are available; determine
per-key travel conversion from firmware calibration/readable parameters, or a
clearly characterized calibration procedure, before promising accurate depth.
Do not normalize every key using a shared guessed ADC interval.


## Revision4: automatic per-key empirical learning candidate

Owner requires no manual calibration. Raw-to-depth conversion was not proven
from factory parameters; log25's100ms sampled raw rows and coarse timestamps
are insufficient for reliable simultaneous calibration fits. Do not claim a
factory-equivalent conversion or perfect first-press support.

Implemented ajazz_raw_learning.h: per-position empirical monotone interpolation
from fresh0x21 event / next0x23 row pairs using QPC microseconds, at most2ms apart.
Each event consumed once; cached events never continuously train. Per-depth
seven-observation median buffers, at least two observations per used depth,
minimum six knots including0/40, spacing at most12 depth units, strictly
decreasing ADC and signal separation against measured zero scatter. Zero/full
anchors use measured endpoint-range boundaries. Once ready, curve freezes;
four consecutive fresh-pair deviations over15% invalidate it and restart
learning. These are engineering acceptance thresholds, not measured accuracy
bounds. New/unlearned keys remain neutral; no guessed fallback. Learning is
session-local in this candidate (not persisted). First-use completeness and
runtime pairing quality require a real test. Learning through normal usage is
not a guarantee every possible key becomes ready without sufficient movement.

0x23 drives all actual output, with independent per-position refresh and100ms
transport freshness (continuous raw rows, not event idle timeout).0x21 trains
only. Raw zero is rejected, never full press. No digital substitution, firmware
calibration mode, settings writes, or shared guessed endpoints. Verified RGB
V1.13.17 physical sensor mask82 positions excludes25 phantom mapped slots.
Fn firmware250 translates to HallJoy1033; both read aliases supported. No
automatic AJAZZ geometric layout added in this iteration. Exact firmware gate
avoids extending this observed mask to other revisions/no-light devices.

Validation: portable learner tests PASS for independent scales, lost-release
recovery from raw, held-key preservation, Fn, absent positions, zero ADC,
stale/consumed pairs, reversed ranges and confidence invalidation.15 diagnostic
mock scenarios and four linked EXE/log-route gates PASS. Linked self-test checks
actual Fn read aliases. No real AJAZZ verification of learning yet. Existing
local runtime was left running; no new startup smoke claimed for revision4.

EXE build/bin/Release/x64/HallJoy.exe SHA256:
7fb3d4ae53b9f96ba64f4afe8437de88bf8b0dc635794d842d690a3ecdd7d2dc.
Automatic log remains beside EXE with raw rows, depth summaries and learning.key
statistics (accepted/expired pairs, populated levels, knots, readiness/endpoints).
This is an experimental learning/play candidate, not a completed perfect system.


## Revision5: owner-approved provisional dynamic limits

Owner explicitly selected immediate per-key dynamic bounds instead of waiting
for empirical curve learning. User-facing title and backend status mark
"dynamic key limits (preliminary)". Each physical key starts with its first ADC
sample as released zero. Full-press seed comes from log25 minima for the12 keys
that reached depth40; untested keys use median1353 from those12 observations.
These are observed seeds from one keyboard, not factory calibration constants.
ADC decreases on press: higher median samples extend the release bound, lower
samples extend the pressed bound. Linear sensor-range normalization is not
physical millimetres. Per-key limits are session-local and independent.
Three-sample median rejects isolated spikes (about one extra sample of latency
at observed1kHz). Zero ADC is invalid; a span<=32 is kept neutral to prevent
noise amplification when starting with a key held. Release then restores range.
No extra unannounced output deadzone; existing user deadzones apply. Noise and
switch tolerances can still affect the provisional scale near endpoints.

Replaced ajazz_raw_learning.h/test with ajazz_raw_limits.h and
 tools/test_ajazz_raw_limits.cpp.0x21 retained for diagnosis only; output uses
0x23 and starts without waiting for training data. Fn aliases and82-slot physical
mask retained. Title and log clearly identify preliminary dynamic bounds.

Tests PASS: immediate input, independent expansion, raw release recovery,
held-key preservation, Fn, phantom positions, isolated high/low spikes, zero
ADC, held-start recovery;15 session/mock scenarios and4 linked EXE/log gates.
No physical AJAZZ verification of revision5 claimed. Automatic log beside EXE.
Delivered SHA256: 07d71c70828a78aa2a116b931ac52db0c87500c313c17da4b79e72fb2b98e478.

### Deferred Configuration UI (owner direction)

Implement explicit per-key release/full limits, calibration actions, clear
units and validation, reset-to-auto, and persistence scoped to exact device and
firmware. Explain linear sensor percentage versus calibrated physical travel.
Expose/edit learned or measured bounds and make recalibration deliberate.
This is a future feature, not implemented in revision5; current dynamic limits
are explicitly not the final calibration design. Do not silently invoke the
keyboard's firmware calibration or write its settings.


## Log27 / owner-relayed tester confirmation, 2026-09-22

Owner reports that the tester says everything works perfectly. This supersedes
the absence of hardware feedback for revision5, not the provisional calibration
design. Exact scope: wired AJAZZ x NACODEX AK820 MAX RGB, SG8994HERGB
V1.13.17. Log compiled Sep20 17:02:26 matches revision5 build timing; no
executable hash was supplied by this log, so timing is not binary verification.

HallJoy (27).log: 6313955 bytes, SHA256
c5436a386f71548faa9ace5629ab6ce0701de97f53080d697719b8b5e78eff27.
Session89.282s; raw start250ms;532083 packets, invalid0, row_mask63,
82 changed positions and82 ready keys. Coarse diagnostic depth events8840
over46 positions are NOT the output source or82 intentional key tests.
Shutdown unsubscribe/raw-disable succeed; read.failed995 occurs during shutdown
and app exits0. No level=ERROR records; this alone does not prove fault-free operation.

Separate unresolved finding: ViGEm output reaches generation2670 in89s;
preceding generation.end records have outcome1 (IncompletePlannedStop),
neutral0/removed0, final outcome0 neutral1/removed1. Investigate this old
diagnostic build's output lifecycle before treating the whole log as clean.
Do not dismiss the tester's successful experience or blame AJAZZ protocol
without tracing the restart cause.

Production status: AJAZZ still gated by HALLJOY_AJAZZ_DIAGNOSTIC in
irok_na87_backend.cpp; ordinary Release leaves it disabled. No code/build
change in this feedback task. README supported tables remain unchanged;
hardware inventory records verified diagnostic support separately. Remaining
work is ordinary integration without forced logging, exact identity/layout
and support notice policy for provisional limits, plus output-lifecycle review.
No new tester request is needed merely to establish that revision5 worked.

Sheet sync: live Main!C13 AK820 MAX HE changed Not investigated ->
Research incomplete (allowed dropdown value; ordinary integration incomplete).
Readback A12:C14 confirms gray RGB .92156863/.93333334/.9490196,
preserved validation and unchanged neighbors. RGB tester result does not
cover Ultra or no-light variants in the broader catalog. All50 yellow rows
read live and reconciled with runtime catalog/header; no yellow changes.
No notes/comments, no GitHub publication.


## Ordinary support enabled locally, 2026-09-22

Owner explicitly authorized full support, README and Sheet updates; NO GitHub
publication. This supersedes the diagnostic-only status recorded above.
Backup: .local/backups/before-ajazz-production-20260922-205406.zip.

Normal NA87/M484 transport now proves exact SG8994HERGB V1.13.17 identity,
accepting trailing space padding from log27 but rejecting other suffixes and
no-light products. AJAZZ uses a separate raw-only session: fresh map/capability
proof, reversible0x23 enable/disable, no calibration/settings writes and no
forced logs, depth subscription, raw dumps or diagnostic UI. Preserves the
owner-approved/tester-confirmed dynamic limits and three-sample median.
All six rows must arrive before connected is announced; a row silent100ms
ends/neutralizes the session and retries. Initial rows have1200ms allowance.
Only82 physical sensor positions publish, with live assignments, duplicates
merged by maximum, and Fn250/1033 alias. Disabled assignments are allowed.
This is normalized sensor percentage, not factory-calibrated millimetres.
There is no new AJAZZ geometry preset; existing manual layouts remain usable.

Native telemetry ABI4 adds an optional model name so shared M484 transport
shows AJAZZ rather than NA87. No NA87 automatic-layout token is published for
AJAZZ. Keyboard-support flags/catalog remain unchanged: supported RGB has no
yellow notice; other AJAZZ variants are not admitted by analogy.

Output lifecycle correction: Backend_NotifyDeviceChange formerly requested
ViGEm restart whenever g_vigemOk was false, including startup. Creating or
removing the virtual pad emits Windows topology notifications; those could
cancel startup and repeat. Removed this external restart trigger. The existing
output owner still monitors failure/timeout and retries with250ms backoff;
explicit configuration changes/restarts retain their existing behavior. This
code path explains log27's rapid planned-stop generations; no new physical
AJAZZ/gameplay run was performed to claim a hardware reproduction or retest.

Validation: ordinary Release plus five linked gates pass. The existing NA87
linked gate now also exercises actual AJAZZ raw publication, independent
press/release, Fn, malformed rows, neutralization, common-registry routing,
model name and padded/rejected firmware identities. Existing dynamic-limit
test passes; topology regression audit forbids restart from generic Windows
notifications. Full native-check runner result/artifact recorded below.

Support sync: README main table (alphabetical AJAZZ before ATK), hardware
inventory and unreleased patch notes updated locally. Live Sheet metadata
verified; inserted exact AJAZZ AK820 MAX RGB row at Main!A14:C14 with Supported
instead of marking the broader AK820 MAX HE row supported. That existing row
stays Research incomplete; Ultra and other variants unchanged. Readback verifies
green RGB .65882355/.8666667/.70980394 and preserved strict dropdown/neighbor
values. No cell notes/comments. All50 yellow rows freshly reconciled using
.local/ajazz-production-yellow-readback.json; catalog/header check PASS.

Final validation: full native/static/portable/Windows runner PASS (108
executable invocations); log .local/ajazz-production-checks.log. Final normal
Release and five linked gates PASS; log .local/ajazz-production-build-verified.log.
Only known pre-existing ViGEmClient missing-PDB linker warning. Dynamic limits
and topology regression checks PASS. Artifact build/bin/Release/x64/HallJoy.exe
SHA256 89aa858c6fcbb520828800fadc24e1721463175b96ab66ba36d4bb1494636ea0.
No GUI run, keyboard flash or GitHub publication.
