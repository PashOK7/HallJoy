# ATTACK SHARK Pro integrated diagnostic — 2026-09-19

Owner requested one test EXE; the currently available tester has X65 Pro HE.
The target is an ordinary HallJoy executable with a diagnostic-only reader and
one HallJoy.log in the neutral build/bin/Release/x64 delivery folder. No ZIP or prescribed duration.
ATTACK SHARK measurements are not yet published as gameplay analog in this build.
Existing HallJoy bindings/gamepad and other supported keyboard backends remain.

## Scope and safety of the exchange

Build switch HallJoyAttackSharkProDiagnostic=true. Without it, all public hooks
are no-ops and this reader does not start in production builds. Target VID3151,
PID502F/5030, usageFFFF:2, feature size65/report ID0. Metadata enumeration
logs only USB IDs/report dimensions, not serials, names or HID paths. Product
strings identifying receiver/dongle are rejected before any feature command.

Repeated8F identity must agree and match a known revision/PID pair:
2308/502F or2938/5030 X65 Pro;2370/502F or2901/502F X68 Pro;
2356/502F or2935/5030 X82 Pro. Unknown identities get no depth commands.
Only8F identity/USB version,80 RF version andE5 FE page reads are allowlisted.
No1B stream mode,1C/1E calibration, setting writes, firmware updates or reboot.
An exclusive handle prevents concurrent vendor-client traffic on that collection.
Busy/inaccessible collections are logged and retried after the client closes.

Read sequence0,1,0,2,0,3 repeats while open; each page request is repeated once
and only the second response is accepted. This mitigates one-request-late replies;
raw responses still lack a page tag and this is not a guarantee against arbitrary
firmware staleness. Set/Get is serialized. Checks reject echoed E5 requests,
recognized stale identity replies, wrong report IDs and values above4096. The
plausibility ceiling is a diagnostic filter, not a calibrated full-scale value.
Failed reads are errors, not manufactured zeros. Timing starts1ms and can increase
to5/10ms on malformed replies or multiple ordinary presses without variation.
No speculative commands are tried. Scale logs the vendor USB/RF version rule;
no user-specific maximum is learned or assumed.

## Evidence and UI

English window title requests varied WASD, a chord with independently varied
depth and release of all keys. Sufficient status requires each WASD slot to show
positive depth, rise/fall and zero release, a digitally corroborated chord with
one changing/one steady nonzero slot, ordinary press/release counts and all four
pages read. Completion never stops capture. Early closing preserves available
summaries; no minimum or maximum typing time. No arbitrary8-key requirement.

Metrics include all128 physical slots, min/max/last raw value, sample/rise/fall/
zero/release counts, page coverage, observed chord evidence, call latency, timing
retries, identity and I/O failures. Active slot aggregates repeat every2 seconds;
full summaries every15 seconds and on normal close. These are periodic log flush
checkpoints, not timed test stages. No ordered keystroke events or typed text.
Raw Input correlation is VID/PID scoped, not proof of a particular physical unit
when multiple matching keyboards are attached. Use one target keyboard by USB.
Remaps may prevent the WASD-specific sufficient criterion even while raw data is
useful; do not call that a protocol failure or discard the log.

## Process containment

All synchronous Feature calls run in a hidden same-image child with an explicit
inherited-handle list and kill-on-close job assigned before resume. Parent drains
bounded line records and monitors liveness; blocked calls cannot freeze the UI.
Closing signals cancellation, then bounded forced shutdown if needed. Last command,
page and Set/Get phase remain in shared memory and are logged after child exit.
No mode restoration is necessary: commands do not change keyboard runtime modes.
Absent devices wait for topology notifications with quiet child liveness messages.
A denied/failed log prevents or ends diagnostic collection with an English error.
It does not disable ordinary HallJoy keyboard/gamepad handling.

## Validation and delivery

Pure request/identity/page/independent-release/sufficiency regression: PASS.
Final MSVC Release x64 build: PASS (existing ViGEm missing-PDB warning only).
Embedded SHARK self-test: PASS, including actual child pipe, forced timeout and
cooperative cancellation, without hardware access. Existing MINI60 and NA87
native self-tests: exit 0. Full native backend static audit: PASS.
The tests ran from a temporary copy of the exact delivery EXE, preserving the
adjacent delivery log. Verification: .local/attackshark-pro-diagnostic-verification.json.

EXE SHA-256: ed8136276e97c214a75aa38e027d905601b8fa78a60646c3e8c9ecf96972d409
EXE size: 9368064 bytes.
No physical ATTACK SHARK or visual run is available locally.

Delivery: build/bin/Release/x64/HallJoy.exe. The old delivery directory was moved
with its existing log and artifacts, not duplicated.
Backup: .local/backups/attackshark-test-before-20260919-124253.zip.


## Owner preflight log review and revised delivery

Reviewed the owner run at 2026-09-19 13:00:20 through 13:00:26, original build
20260919-shark-1: eligible=0, no Feature commands (last_command=0), no digital
samples, worker cancellation exit4/forced0, application exit0. Connected UAP
keyboard is VID3434/PID0E40 (Keychron), not ATTACK SHARK. This is a valid absent-
device/startup/shutdown check, not evidence of successful X65 communication.

Build 20260919-shark-2 adds metadata-only enumeration of other VID3151 PIDs,
explicit collection rejection reasons, metadata error counters, reply-command
fields for failed identity reads and a final no-eligible-device outcome. Unknown
PIDs still receive no Feature commands. Summaries now include capture elapsed_ms
for calculating effective page cadence. Identity keepalive repeats the request
before validating the untagged response, as depth reads already do.

The owner objected to the old IROK directory name. The diagnostic build property
now selects build/bin/Release/x64 by default; future deliveries use this neutral
path. Existing HallJoy.log was preserved during the move. Exact rebuilt EXE
SHARK/MINI60/NA87 self-tests passed; portable model regression and preserved
firmware component checks passed. Verification record:
.local/attackshark-pro-diagnostic-review-verification.json.
No UI or physical ATTACK SHARK run was performed. No claim that all possible
hardware faults have been excluded; a tester log remains necessary.


## Protocol review: build 20260919-shark-3

Reviewed actual official desktop source rather than relying on earlier notes:
index.2e5bd916.js (Tb transport, checksum encoder, identity and Windows report-ID
wrapper), f9b6af43.js (E5/FE page request, LE16 decoder, USB/RF versions and
version-dependent units), exact X65 chunks6589a4f6.js/aaf1260b.js and catalog.

Findings corrected before delivery:
- The vendor waits before SetFeature AND before GetFeature. Our implementation
  previously omitted the first wait. Both waits now use the selected depth
  timing (1/5/10 ms); identity/version requests use10 ms on each side. Identity
  keepalive temporarily uses10 ms and restores the depth timing afterwards.
- The two catalog transport entries use FFFF:0002, interface2. Removed the
  unsupported FFFF:0001 fallback. Only metadata is inspected on rejected usages.
- RF version reads repeat to mitigate a pending reply from the identity request.

New offline check tools/review_attackshark_wire_protocol.py executes only three
reviewed vendor methods in a mock transport, then compares their padded Windows
reports against compiled HallJoy Request(). All six 65-byte packets match exactly:
E5/FE pages0..3,8F and80. Checksum bytes are1B/1A/19/18,70,7F respectively.
LE16 boundary examples, LE32 identity offset and catalog usage checks pass.
The exact firmware component checks also pass again (12 page copies,16 scanner
transitions, six driver models). These are not full USB/ADC/keyboard emulation.

No packet layout, checksum, sample endian or version-offset mismatch was found.
X65 Pro still lacks its exact firmware image: the official shared driver path
is verified, but continuous table updates outside calibration are established
in firmware only for X82 dev2935 v503. Four pages are not atomic, response page
identity is absent, physical timing and typing coexistence still need the tester.
The 4096 plausibility filter is host policy, not vendor full travel. No gamepad
analog is published by this diagnostic. No calibration or1B stream command added.

Rebuilt exact EXE self-tests SHARK/MINI60/NA87 passed. Artifact/check results:
.local/attackshark-protocol-review-verification.json. Send this build rather than
shark-1/shark-2. Existing adjacent owner log was not overwritten by self-tests.
