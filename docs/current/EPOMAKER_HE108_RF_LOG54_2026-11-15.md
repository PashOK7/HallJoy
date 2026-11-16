# EPOMAKER HE108 over 2.4 GHz, log 54

Discord's 646 kB attachment matches `HallJoy (54).log` in the local downloads.
It contains 645,627 bytes. SHA256 is
`ebfc9f73a9277d649376506fdab0be7463e1e99ed51b5a72256db0c374f36749`.
HallJoy 1.6.11.0 exported it at 2026-11-15 16:46:22 UTC.

- Line 155 records `rongyuan.connected` for board 3365 in the wired
  `3151:5030` session. Line 174 records its end with Windows error 1167 when the
  device disappeared.
- Lines 699-705 list receiver `3151:5038`, product `2.4G Wireless Keyboard`,
  with MI_00, MI_01 and MI_02. Line 725 reports no connected RongYuan source.
  Its 28,046 updates and two failures are cumulative counters from the wired
  session. This HallJoy version did not query HE108 analog through the receiver.
- The preceding local RongYuan receiver code identified the remote board, but
  `ry_receiver::Model` admitted only Keydous NJ98-CP V4S board 3793 on that
  receiver PID. Sharing a receiver ID does not establish how it forwards the
  HE108 analog reports.

OEM GearHub v4 ties board 3365 to `ry5088_fq_x113c_8k` and ANSI geometry
`Common108_X113C`. Its shared receiver registry entry advertises
`3151:5038`, MI_02, usage page `FFFF`, usage `0002`. These entries establish
the wired model and the shared HID interface. The log still lacks a receiver
reply confirming the remote board and any evidence of fresh key travel over
that interface. The exact board lookup for firmware through the OEM
`get_fw_version` endpoint returned HTTP 500 on api2.rongyuan.tech and 404 on
api3.rongyuan.tech and api2.qmk.top. No exact firmware image was obtained.

Wired support stays documented in the [original HE108 note](EPOMAKER_HE108_2026-09-26.md).
Before publishing a 2.4 GHz build as working, identify the paired board and
validate live per-key depth, simultaneous presses and return to zero through
the receiver. Board 3365 has an existing ANSI preset for automatic selection
after positive identification.

## 2.4 GHz tester integration

The exact MI_02 vendor collection must match `3151:5038 FFFF:0002` with a
65-byte feature report. An online F7 receiver, exclusive open and a matching
remote 8F board reply precede any attempt to read or enable analog. Other
boards continue through their existing policies. An occupied interface or
routing collision keeps the command pipe from overlapping another client.
The firmware scale comes from E6 or the existing board-specific version
fallback. No calibrated depth is published if neither reply supplies a usable
scale. Wireless uses the existing physical ANSI map independently of live
bindings and assignment data.

Receiver pages use the F0 freshness proof already implemented for the shared
receiver family. A missing marker cannot authorize an anonymous table reply.
The exact 32-byte report-5 input collection is independently matched within
the receiver's USB container. If available, a failed marker or a malformed
table reply still allows a 1B stream attempt. Stream data is published only
after a valid sample for a mapped physical key. The normal teardown sends
1B off for a session that attempted 1B on. This path changes no firmware,
pairing, calibration, saved profile, or persisted keyboard configuration.
The test covers protocol structure using reconstructed replies. Radio
forwarding and behavior on the tester's firmware still need physical evidence.

Open log records receiver discovery, remote board/firmware, marker result,
pages and stream outcomes in the ordinary report. `evidence.he108.rf` adds
structural mismatch flags and event-driven depth, chord and release counts.
There are no new fixed wait periods, packet dumps or separate tester steps.
A receiver with no matching interrupt collection can still supply valid
synchronized pages. Wireless remains experimental.

The ordinary portable build is `build/bin/Release/x64/HallJoy.exe`
(11,939,328 bytes, SHA256
`cb919ee314407328d4be1204108f757279c69d5f93865adbb2320d9dfaefec57`).
All 12 registered exact-image tester checks passed, including three positive
and three negative HE108 receiver fixtures. Native and compiled layout
checks passed. The compiled preset is EPOMAKER HE108 ANSI. The first full
build attempt stopped on the existing Akko 3131 all-aliases fixture before
any HID test commands. The repeat passed with no changes to the fixture.

## November16 screenshot feedback — no new log

The owner reports that wireless still produces no analog. Disappearance of the
old banner was initially mistaken for success; the tester explicitly corrects
that interpretation. A communication warning appears after switching keyboard
modes. No new HallJoy.log or tester-image hash is available. Do not identify an
actual protocol failure stage or promote RF from this screenshot. USB confirmation
is retained. The targeted wireless implementation is2.4GHz via the receiver;
Bluetooth is a different transport with no implemented HE108 analog path.

Read-only review found a concrete bootstrap defect that remains in ordinary
imagea3f93418: HE108 normally waits for four complete E5 pages before starting1B.
Its fallback predicate is snapshotProblem || syncUnconfirmed. A properly shaped
but never-ready remote command yields ReadReady, neither condition is true, and
the pending request blocks SendKeyboard too. Therefore replacing only the fallback
condition would not fix it: enabling the stream still tries to drain that pending
page first. Start ordering and pending ownership must be handled together.

Private `.local/research/he108-no-log-review-20261116/bootstrap.cpp` compiles the
current production Client, HE108 fixture, decoder and health observer. It proves
exact3365 identity and F0 synchronization, then models an absent E5 response using
the current10ms handoff configuration. PASS: ReadReady with pending page; fallback
false;1B enable cannot be sent; no invented depth. It separately proves all-zero
pages are accepted, and a wired-to-wireless connection loss can raise the generic
warning without any sharing-conflict anomaly. Source hashes and receipt.json pin
this bounded reconstruction. No physical device was opened and no claim is made
that this is the tester's actual interleaving.

Other compatible explanations remain: all-zero E5 pages with no useful1B input;
no matched32-byte report5 vendor input collection; an actual open/transport error;
or Bluetooth instead of the implemented2.4GHz route. Current enumeration requires
one31-byte8-bit value cap for report5; a benign descriptor split could prevent
stream pairing despite matching receiver identity. The screenshot supplies no HID
caps or packets to decide that branch. The warning text suggests another app but
is not proof of one: CommunicationHealth also warns after a previously connected
source stays absent while the receiver remains present. The layout and bound
icons are not evidence that live depth is being received.

The appropriate bootstrap correction is to attempt the independent1B reader and
enable after exact identity/scale/map proof, before an optional table request can
block it, retaining table-only fallback if interrupt input is unavailable. It must
preserve original ownership/order for later replies and cannot relabel a pending
page or use digital releases as depth. Add never-ready bootstrap and descriptor
variation cases through the production start path; the existing3/3 HE108 image
fixtures cover bad-marker/bad-depth fallback, not this never-ready case.

This task is the requested review without a new log: no production source change,
ordinary rebuild, firmware operation or further tester trial was performed.
The existing shared revision8 handoff/late-page fixes may help an older build but
do not remove this bootstrap dependency. Hardware cause remains unconfirmed.

Fresh native Sheet metadata/Main!A262:C266 locates EPOMAKER/HE108 at row264:
Supported, strict dropdown, effective green RGB(.65882355,.8666667,.70980394).
This is the retained USB classification; no RF promotion or Sheet edit is made.
README's HE108(USB) scope remains accurate; hardware and follow-up records now
include the negative wireless feedback. No change to yellow runtime notices.

## Revision2 stream-first tester build — 2026-11-16

Owner requested the actual bootstrap fix, regression coverage, a new ordinary EXE
and a persistent yellow wireless notice with Open log. The preceding review is
historical; its no-production-change statement does not describe this revision.

After exact remote identity, usable scale, physical map and routing ownership,
HE108 calls the shared StartBeforeTable orchestration. The interrupt reader starts
before1B enable and before any E5 table request. A never-ready E5 therefore cannot
prevent the already-started stream from publishing real physical depths. Stream
confirmation still requires a valid mapped report; enable alone is not connected.
Missing interrupt input or a failed start leaves synchronized table fallback.
Other receiver models retain their original bootstrap order. No digital-derived
depth or timeout-generated release, calibration or persistent write is added.

Final cleanup is terminal: stop/join the interrupt reader, abandon the pending
untyped table without publishing it, wait for SEND readiness and send1B off. It
does not wait for the absent query's READ-ready. The client rejects any later
Page/Query/ordinary send, so an abandoned reply cannot be relabeled in that
session. Live page resume retains the revision8 ownership/ticket rules. Offline/
busy hardware may still prevent OFF; normal evidence records sent=0 explicitly.

Input collection matching now accepts split value/button capability records
declaring report5. Exact vendor collection FFFF:1,32-byte report, same-device
container pairing and first-frame1B/slot/scale validation remain mandatory.
Capability count, BitSize and ReportCount segmentation are metadata, not extra
analog admission requirements. Stream start failures preserve the original Win32
error before reader/enable cleanup. No matching input is explicitly logged while
the table path is still attempted.

The backend publishes the exact HE108 model name from its proved live receiver
session even before analog confirmation. ResearchNotice selects the new
He108Wireless bit only for protocol22,3151:5038 and that exact model. A shared
receiver PID alone cannot label a Keydous/unknown keyboard HE108. After matching,
the gold notice remains visible both before and after usable input. Its title is
EPOMAKER HE108: wireless analog test; the body explicitly asks for Open log even
if no analog appears. Open log is left-aligned; QR/join/copy controls are omitted.
The exact notice takes priority over the generic communication warning and
replaces the generic RongYuanStream notice for this session. It is advisory:
ShouldAutoSaveSupportLog does not force a report/continuous logging because of it.
Ordinary missing-source/open-conflict behavior remains for failures before model
proof. USB3151:5030 retains its existing green/Supported behavior.

Ordinary evidence.he108.rf revision2 records stream-first start, input availability,
enable/error and table fallback. Pending-page observations include page, transport
stage, original request age, stream request state, received count and I/O error,
emitted at stage changes/powers of two. Existing first-sample, changing depth,
simultaneous-key/release and cleanup records remain in normal Open log. No tester
script, special flag, minimum duration, forced logging or raw packet export.

Pre-analog refusals remain the exact control collection and remote board proof,
offline/boot state, failed/busy I/O, undecodable precision, ownership collision or
cancellation; invalid packets cannot be published. Assignment/profile metadata,
aliases/unbound keys, arbitrary counts, optional marker/initial table and layout
cache availability do not veto the reviewed physical stream. Existing wired
support and104-key EPOMAKER HE108 ANSI auto-layout are retained.

Production-client/start-helper/decoder/publication cases now cover7 positive and4
negative scenarios. New positives cover a never-ready table after early enable,
missing-input table fallback, absent optional metadata and split capabilities with
all104 independent sensor presses/releases and wireless notice checks. The added
negative covers cancellation before enable and cleanup. Terminal shutdown is
tested while E5 remains pending; later queries cannot publish a stale response.
The native receiver test executes these same cases, and tester_delivery_cases.json
requires their receipt from the ordinary EXE. Software tests do not prove RF
forwarding or the tester's firmware behavior. Exact firmware acquisition limits,
typing coexistence, real loss/range/latency and physical RF success remain as above.

Backup, check logs and final delivery receipt are under
`.local/research/he108-stream-first-20261116/`. The first native attempt caught a
source audit's old exact advisory-mask string. It was extended to preserve the
old exceptions and include the new no-forced-log HE108 notice; executable policy
tests cover both connected and waiting states. A compile-time array-size typo in
the terminal-state test was corrected before the final native run. Final native
and ordinary exact-image build gates passed. No publication.

Delivered `build/bin/Release/x64/HallJoy.exe`, version1.6.15.0,11,901,952 bytes,
SHA256 `b8fc4c70c02eae343c7100277fff7fe069061e6c2d8c60482525f0b185f84024`.
The ordinary release wrapper ran once. Its13-family exact-image receipt includes
HE1087positive/4negative,104 physical channels, stream_before_table=1,
pending_cleanup=1 and wireless_notice=1. Compiled catalog contains104 entries
for EPOMAKER HE108 ANSI; the exact auto-selection token/preset is verified by
the same image fixtures. The actual image contains the dedicated wireless notice
title and Open log instructions. Full catalog, retained protocol families,
embedded notices/licenses, profile/layout transactions,92Supported-layout checks
and18startup-recovery scenarios PASS. No physical or visual test is claimed.

Receipts: delivery.json, native-verified.log and build.log in the private research
directory, plus `.local/tester-delivery/<full-image-sha256>.json`. Fresh live
Main!A262:C266 confirms EPOMAKER HE108 at row264, Supported with strict dropdown
and green RGB(.65882355,.8666667,.70980394). That is USB support, not an RF
promotion. No Sheet write was needed. Fresh full color/status readback reconciles
all286 yellow catalog models with runtime notices. The dedicated runtime RF
test notice is transport-specific; README remains HE108(USB), and RF hardware
verification is still pending. The tester uses the ordinary EXE and Open log
whether or not analog works, with no extra flags, exports or prescribed duration.

## Log60 — exclusive open blocked, 2026-11-16

The next screenshot reports no operation and an instability notice. Downloaded
`HallJoy (60).log` matches its147,775-byte attachment and header
2026-11-16T17:34:40Z. SHA256
`c1a0337f3819bb46bfed429014396facca46a603917a83da16045fed55a59a58`.
The screenshot's11.35MiB EXE size/version is consistent with the delivered image,
but the report does not carry an exact executable hash. No new per-device stream
revision marker is reached; do not use its absence alone to call the EXE old.

Seven complete, telemetry-complete, summary-consistent snapshots cover30 providers;
no queue-loss record. Receiver3151:5038 MI02 FFFF:0002 /Feature65 is admitted and
one exact report5 interrupt pair is found. Capability matching did not stop this run.
There are69 saved rongyuan.exclusive_open_failed records, all Win32 error32,
from8376906 through8435687 (58.781s between first and last saved refusal).
Every retained RF attempt is phase1/open, stage0, board0, units0, command00,
zero exchanges/polls. No remote8F identity, HE108 bootstrap,1B enable, E5 query or
analog connection is recorded. This is an access-blocked run, not a failed test
of the stream-first change or evidence that RF cannot supply analog.

Session::Open requests read/write with share0 for a receiver. Error32 is a sharing
conflict; another incompatible open handle prevents that open. The report cannot
name the owning process or prove it is external to HallJoy. Ordinary keyboard
typing and metadata enumeration do not establish access to this control interface.
The communication-warning flag is set by the explicit open conflict; it does not
measure analog stutter in this run. Generic analog_error=-1996 is less specific
than the recorded Win32 open error. Native telemetry currently keeps failedUpdates0
and access_busy0 here despite the separate open-failure records; treat those as
counter-scope/diagnostic limitations, not absence of the sharing failure.

Relevant internal source paths were checked before blaming another application:

- Soup's pre-open UAP gate calls MayUsePath;3151 is absent from SupportedVendor,
  so the reviewed policy excludes it before read/write enumeration opens.
- The separate RongYuan snapshot backend filters to3151:5030/5029, not5038.
- AttackShark candidates reject receiver/non-target PIDs before control sessions;
  generic SparkLink explicitly excludes3151.
- RongYuan Prepare skips receivers; the worker owns the radio session. Its Start
  is serialized and does not intentionally create a second radio worker.

These source-policy checks found no expected internal owner, but are not a handle
trace on the tester's PC. A vendor driver/tray process, web configurator, another
HallJoy instance or other HID-enumerating application remain possible holders.
Do not identify a specific process from error32 alone. Do not bypass receiver
exclusivity: concurrent F6 target selection/FC consumption lacks an ownership
protocol and can corrupt response attribution.

Next action is to release the control interface, then use the SAME ordinary EXE:
fully exit keyboard configuration software, including its tray process and web
configurator tabs, and avoid another HallJoy instance. Keep2.4GHz selected. The
existing worker retries when the handle becomes available, so a new diagnostic
build is not justified by this log. Open log again after ordinary use whether or
not input works. If error32 persists, identify the actual holder rather than
repeating analog/firmware changes or assuming the known conflict disappeared.

Private `.local/research/he108-log60-20261116/` contains original log, own asserted
analysis, relevant source hashes, before-edit documentation backups and live Sheet
readback. No runtime code/build/firmware/device action or external message. USB
support remains confirmed; RF protocol behavior is still unmeasured on this run.
Fresh Main!A262:C266 readback confirms HE108 row264 remains Supported with strict
dropdown and green color for the retained USB scope. No Sheet edit/status change;
README's USB-only supported listing and runtime trial notice remain accurate.


## Log61 after reboot — open succeeds, no depth observed, 2026-11-16

`HallJoy (61).log`,93,015 bytes, matches the new screenshot header
2026-11-16T17:45:18Z. SHA256
`e9e57b91874d7de704bd81b83170c2a3002d77ffe7ad920aa9e26bc2cce9eecb`.
Five complete, telemetry-complete, summary-consistent30-provider snapshots;
no reported queue loss. This supersedes log60's access-blocked next action.

There are NO RongYuan exclusive-open failures. Two saved revision2 sessions prove
remote board3365 on3151:5038, keyboard version0504 (usb0504/rf0000 as reported),
receiver00000802 and confirmed F0 synchronization. Identity records show
200units/mm and3300um range. Exact report5 input pairing succeeds. Both bootstrap
records show input_present1, stream_requested1, error0;1B is sent before E5.
Sending1B successfully is not a remote enable/forwarding acknowledgement.

- No stream packets, malformed packets or stream I/O failures in saved travel
  records. Periodic rf_stream counters remain zero through uptime38281ms.
- Synchronized table reads complete: the second saved session reaches8 sweeps,
  32pages at28156ms, average301309us/max318997us per sweep. positive_slots,
  varying_slots, releases and peak_raw are all zero, including its first matrix.
- Latest native snapshot at36515ms reports connected1, mapped_keys104,
  updates4039, failures0, age47ms. Updates include publishing valid zeros;
  these are not4039 presses. Valid zero pages establish protocol connection,
  not hardware validation or usable gamepad movement.

The first saved session ends on global topology change at25500ms, stop0, after
18pages;1B-off cleanup succeeds and the next session starts. More device.change
records and page-counter resets occur later. Notifications do not identify which
device changed; do not call these confirmed radio disconnects. Detailed table
travel evidence covers the intervals above, not every later page. There is no
independent digital transition/held-key reference, so the log cannot establish
exactly which depths should have been positive. Tester feedback still says analog
does not work; this report supplies no positive result.

ATTACK SHARK metadata discovery sees Win32 error32 on this same5038 MI02 at23796ms,
after RongYuan opened it exclusively. That is consistent with HallJoy's ownership,
not evidence of an external app blocking the successful RongYuan session.
ReceiveLine forwards only exclusive session-open sharing failures to the anomaly
counter; metadata failures do not do that. The generic warning rises at35703ms,
but carries no causal source/observer state. Global connection-drop observation
can also raise it. Its exact cause remains unassigned; neither this warning nor
the metadata error justifies blaming an external process in this run.

Next investigation: actual depth generation/forwarding for exact HE1080504 plus
receiver0802. Compare official configurator initialization and ordinary wireless
depth reads with USB; trace1B report forwarding and E5 table population in acquired
exact binaries. Check whether another documented volatile enable is needed.
Do not replace sensor depth with digital keys, infer releases from silence, enter
calibration, flash another model or relax command ownership. Missing exact
binaries are missing evidence, not proof RF is impossible. Log60's interface-release
retest instruction is obsolete; another unchanged tester build adds no experiment.

No production change/build, firmware operation or external message in this review.
USB remains confirmed;2.4GHz is unsuccessful so far, neither promoted nor declared
impossible. Fresh live Main!A262:C266 confirms EPOMAKER HE108 row264 Supported,
strict dropdown, green RGB(.65882355,.8666667,.70980394), retaining USB scope.
No Sheet/status/catalog change; prior all-yellow reconciliation still applies.
Private `.local/research/he108-log61-20261116/` retains original log, asserted
analysis, source hashes, documentation backups and live Sheet readback.


## Tester confirms wireless analog after reboot — 2026-11-16

Later owner-supplied screenshot shows POLYMORPHIC's message below the same94kB
17:45:18Z attachment: "заработал аналог!". Asked whether it followed a particular
action, the tester replies at20:50 "после перезагрузки пк". This is positive
physical-user feedback for HE1082.4GHz on the current trial, superseding the earlier
RF-not-working verdict. The agent did not operate the hardware. No new log or
firmware/configurator change accompanies this message; exact EXE hash is still
unreported. The previously delivered revision2 image remains unchanged.

Log61's saved zero-valued intervals remain valid observations. They do not rebut
the later positive user report and do not establish no depth throughout actual
use. Exact onset relative to export, positive sensor samples, table versus stream
source at success, gamepad output details and the cause of reboot recovery are
not captured. Do not invent a later nonzero packet or claim reboot proved which
process had the earlier conflicting handle. The previous investigation plan that
presupposed continued RF failure is superseded; firmware is not a prerequisite to
recognizing this reported success, and another identical trial is not required.

Record the result as initial tester-confirmed2.4GHz analog after reboot. Retain
the existing yellow wireless trial notice for now: repeated clean startup,
simultaneous-key independence, releases/latency, calibrated range and ordinary
typing coexistence were not separately confirmed in this message. No unsupported
workaround, digital-derived depth, new admission, code change or rebuild is needed
for this evidence correction. USB support remains established. Public README,
hardware notes, release draft, tester follow-up and owner context are reconciled.

Fresh live metadata and Main!A262:C266 locate EPOMAKER/HE108 at264: Supported,
strict ONE_OF_LIST, green RGB(.65882355,.8666667,.70980394). No Sheet write or
status/color/catalog change is needed. The green row already covered USB;
RF initial user confirmation now has explicit scope in docs while the specific
runtime wireless notice remains. Previous all-yellow reconciliation remains
applicable because notices/catalog are unchanged. The screenshot is not copied
into research archives. Private confirmation/before holds exact doc backups;
confirmation/sheet-readback.json holds this live readback.


## Owner accepts Supported — 2026-11-16

The owner now explicitly confirms everything works and requests Supported.
The new screenshot shows proportional gamepad output and both wired5030 and
wireless5038 HE108 sources; ordinary factory layout is selected. The tester's
own actions/PC state caused the earlier unsuccessful run according to the owner;
the agent has no trace proving a particular external holder or recovery mechanism.
USB and2.4GHz are accepted, Bluetooth is outside this scope. Remove the dedicated
wireless testing banner and the generic protocol22 testing notice only for the
proved3365 layout token on5038. Preserve actual busy/communication failure notices,
stream-first handling, cleanup and ordinary bounded evidence. This supersedes
all earlier pending multikey/release/repeat-start gates in this note. The agent
claims source/tests/build checks, not independent hardware testing. See
[1.6.16 preparation](RELEASE_1.6.16_PREPARATION_2026-11-16.md).
