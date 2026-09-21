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


## Tester log17: X65 Pro HE hardware evidence

Reviewed Downloads/HallJoy (17).log (310992 bytes), SHA256
74b42a4cba0233f612820b309987eadb5ea9dd54f5b61758ee542e388f08dfeb.
Session2026-09-19 15:05:23.475..15:06:49.579. IMPORTANT: the log explicitly
identifies build20260919-shark-2, not the subsequently reviewed shark-3.
Do not attribute shark-3 timing changes to this hardware run.

Identity2308, VID3151/PID502F, USB version788 (0x0314), RF0; vendor scale rule
reports100 units/mm. Eligible Feature collectionFFFF:0002, length65 opened.
No stream/calibration/settings-write commands. Tester reports normal typing.
Final capture85500ms: pages1361/454/454/453, malformed0, io_errors0,
mean Set/Get exchange15565us, maximum51041us. Approximate accepted page rates:
page0 (WASD)15.9Hz, other pages5.3Hz. These are diagnostic rates, not a claim
of game-ready latency; page scheduling and exchange delays need attention.

All four WASD slots14/9/15/21 show rise, fall, zero release and observed raw
maximum350 (3.5mm under the driver rule, not independent full-travel calibration).
Page0 peak simultaneous nonzero slots5; digital WASD chord frames5 and one
moving-plus-steady nonzero observation. Sufficient status reached and capture
continued. Digital totals343 presses/343 releases, final held0. This is hardware
evidence of changing multi-key depths, rather than only binary key detection;
aggregate metrics do not establish every pairwise key mapping or update timing.

53 populated vendor-map slots had positive depths and all finished at zero.
Slot127 also varied (max290, final30), but its entire vendor mapping record is
zero: exclude it from key publication, retain as an unexplained unused-slot
observation. Do not describe it as a stuck physical key. The full vendor table
includes extra/alternate assignments; unobserved entries are not automatically
missing physical keys. No all-key analog coverage claim is established.

Clean app exit0, worker cooperative cancellation exit4/forced0. Analog output
was intentionally disabled (analog_output=0), so the tester's lack of visible
travel and unsupported-device notice are expected for this diagnostic build.
An Xbox device in Steam only confirms the ordinary virtual gamepad path.
Next implementation can use this evidence for exact2308 gameplay integration;
X68/X82 and other X65 revisions still lack equivalent physical validation.
No additional log-only retest is required merely to establish that analog exists.


## Next iteration: shark-play-4 (implementation, validation pending)

Owner requested a playable EXE after log17. Native registry publication is gated
on exact dev2308/USB0x0314, the tested X65 Pro revision. Other known Pro devices
retain diagnostic reads without gameplay publication. No release publication
requested; ordinary1.5.3 is preserved in .local/backups/HallJoy-v1.5.3-tested.exe.
Source backup: .local/backups/attackshark-before-play4.zip (internal only).
Delivery remains build/bin/Release/x64/HallJoy.exe, one adjacent HallJoy.log.

Uses the existing isolated child, read-only E5/FE pages, no stream/calibration
or configuration writes. Interlocked page sequences/timestamps protect shared
memory publication; samples feed NativeAnalogBackends_ReadMilli and the normal
configured gamepad builder. Only mapped keyboard/Fn slots publish; consumer
records and unused slot127 do not. Fixed provisional full travel350 from log17;
no learned maximum. Uses factory assignments; device remaps and automatic
exact-layout selection are not implemented in this iteration. Manual layout
selection remains available.

High-resolution waitable timers preserve the vendor's1ms pre-write and pre-read
waits without normal system-tick rounding. Current-page bursts omit redundant
flush exchanges; changing page or returning from identity verification still
flushes once. Schedule prioritizes page0, covers1/2, checks unused page3 once
per128 accepted-loop attempts for diagnostics. No cross-page atomic snapshot
is claimed. Freshness150ms per page, connection age500ms; worker failure clears
publication. Runtime page rate and expired-page counters stay in the log.
Actual improved hardware cadence remains to be measured by the tester.


### shark-play-4 delivery verified

MSVC Release x64 build PASS (existing ViGEm missing-PDB warning only). Portable
native model test PASS; full static backend audit PASS; official wire comparison
PASS (six packets). Exact EXE self-tests PASS: actual shared-page child transport,
registry-to-configured-gamepad path, independent release, torn-page rejection,
stale release, revision gate, forced child timeout and cooperative shutdown.
Existing MINI60/NA87 self-tests and embedded installer verification also PASS.
No hardware or visual run was performed. Evidence: .local/shark-play4-verification.json.
Final SHA256: a2a12b5b8b98dc07a6fa8232df26fe70186adcbbe245b6da6c23aa2a3f4b92b3
Size: 9375232 bytes. Delivery: build/bin/Release/x64/HallJoy.exe.
Single adjacent HallJoy.log; no test deadline. User can exercise depth, chords,
release and actual game movement, close the app and return the log. No firmware
update or calibration procedure. Only diagnostic build enables this new backend;
published1.5.3 has not been replaced. Local native publication ignores page3,
which remains diagnostic-only and cannot sustain a connected analog source.


## Tester log18: playable iteration hardware result

Reviewed Downloads/HallJoy (18).log,443774 bytes, SHA256
8e6d7baca07e75722336ac624aba927d16e5d5dcd285ef3915545a50b4489dda.
Session2026-09-19 15:27:08.883..15:28:56.394, shark-play-4. Exact2308/USB788,
RF0, playback enabled1 and high-resolution timer1. Tester reports visible depth,
working gamepad binding and no unsupported-device warning; explicitly has NOT
tried a game yet. Do not claim gameplay validation from this log/report.

Final107063ms capture: page counts10773/5386/5218/169, malformed0, I/O errors0,
89 digitally corroborated chord frames and60 moving-plus-steady observations.
All WASD show independent variation and zero release, max350. All populated
key-map slots finish at zero; only unmapped slot127 remains30 and is excluded
from native publication. Digital281 presses/281 releases, held0. Sufficient1,
cooperative worker exit4/forced0, app exit0.

After startup, page0 runs approximately104Hz, page1 approximately52Hz and
page2 approximately50Hz (e.g. interval15094..90094ms). These are accepted page
rates, not measured end-to-end gamepad latency. Earlier log17 was15.9/5.3/5.3Hz.
Mean final exchange3287us; maximum275539us. Native expired-page counter reaches
21 by5.25s and22 by10.25s, then stays22 through105.25s. A page expiry is not a
count of lost keypresses: it also occurs while all depths are zero. Early host/
transport timing stalls remain a recorded issue; cause is not established.
No continuing expiry growth is observed after startup. Do not call the entire
run stall-free or hide the initial latency outlier.

Tester reports Fn+number produces a digital F-key while analog depth stays on
the number. This matches current factory-position publication without Fn-layer
translation/remap readback; not evidence of missing depth or a decoding failure.
It is a known mapping limitation, not complete Fn-layer support. Next useful
hardware step is a game test using the same EXE: gradual partial/full steering,
simultaneous keys and complete release, then return the log. No new build was
created for this log review.


## 1.5.4 prerelease: native family and analog Fn routing

Owner confirms X65 gameplay works, requests a prerelease EXE with Fn correction,
and explicitly adds X68 Pro and X82 Pro to the same iteration. No GitHub publish
requested. Current artifact replaces local delivery only, version1.5.4.0:
build/bin/Release/x64/HallJoy.exe, SHA256
842498cb5803f006e8a24f2db37eca371b16ca450e76c43312f2e533037beaa3
Size9280512 bytes. Previous playable build is preserved at
.local/backups/HallJoy-shark-play4.exe; source backup shark-before-rc-fn.zip.

HALLJOY_ATTACKSHARK_NATIVE is now a normal default backend. The explicit
HallJoyAttackSharkProDiagnostic switch remains separate and OFF in this EXE.
No tester title, forced trace or logging requirement. Normal SupportLog obeys
the existing Enable logging/automatic incident policy. Successful continuous
operation does not force an adjacent log. Readout starts through the native
registry AfterRawInput phase, not early UI startup. The idle parent polls at
20ms instead of1ms. The sample/connection timeouts remain150/500ms.

Exact VID3151 revision/PID pairs admitted:2308/502F and2938/5030 (X65 Pro),
2370/502F and2901/502F (X68 Pro),2356/502F and2935/5030 (X82 Pro).
Each profile has its own pinned vendor factory and F-key Fn map. Fn is slot59
for2370 and slot65 for the others. Actual descriptor identity selects the map;
PID alone is insufficient. Records are decoded as [type,subtype,usage,extra].
The prior code incorrectly matched Fn as [0,10,1,0]; corrected to [10,1,0,0].
Dev2901/2356/2935 vendor maps already have a physical F-row; the X65 number-to-F
mapping is not copied to them. This preserves the vendor catalog discrepancy
at the point of use rather than guessing geometry from marketing names.

Fn+1..0,-,= uses the exact factory F1..F12 assignments where present. Depth and
Fn detection use analog pages only; ordinary native builds no longer collect
Raw Input press counters. Layer selection is latched at the first positive
analog sample until zero/expiry: Fn released first retains the F-key until the
physical key releases; a number held before Fn retains the number until release.
Both base and Fn publications are cleared on failure. Fn is sampled before
number-key bursts; all pages are collected before applying a layer decision.

Limitations: this is factory Fn+F-key support, not arbitrary vendor remap/macros
or all Fn actions. Firmware actuation thresholds and active-layer state are not
read; positive physical Fn depth is the signal. Very shallow/near-simultaneous
Fn edges can differ from firmware digital actuation. Physical confirmation of
these new edge cases is still required; do not describe the fix as fully tested
on hardware. No automatic exact-model layout selection was added.

Scale follows vendor USB/RF units10/100/200 per mm. Full travel remains a fixed
provisional3.5mm endpoint (35/350/700 raw), no live maximum learning. Only tested
X65 dev2308 has observed350 evidence. X68/X82 and other X65 revision have source
and synthetic coverage, not gameplay/hardware validation. Early log18 stalls
are NOT proven fixed: removed premature startup and added separate wait/Set/Get
maxima to settings-aware structural support logs for the next measurement.

Validation: MSVC Release PASS (known ViGEm PDB warning only); full static gate
PASS; portable tests cover all six maps, Fn release orders and unit conversions.
Exact production EXE self-tests PASS: all six profiles through shared-memory
publication/native registry, configured virtual stick values, Fn release orders,
expiry, torn pages, unknown identity, isolated child, cancellation and timeout.
MINI60/NA87/embedded installer tests PASS. Tests confirm trace disabled while
analog publication works, no forced adjacent log, no forced trace markers.
Evidence: .local/shark-family-rc-verification.json. No visual/hardware run by agent.


## 2026-09-19 — ATTACK SHARK Pro visual layouts and automatic selection

Supersedes the preceding statement that exact-model layout selection was absent.
Added built-in ANSI presets for X65 Pro HE, X68 Pro HE and X82 Pro HE, under
ATTACK SHARK. Native telemetry supplies a layout token only while connected
with an exact verified revision. Both automatic-selection paths consume it.
Unknown identities/shared PID alone do not select a layout. Existing multiple-
device/manual fallback and disabled Automatic layout behavior remain in force.
These are factory layouts; no custom remap readback has been added.

Official desktop index.2e5bd916.js assigns:
- dev2308/2938 to Common67_beat65; geometry 20e7e191.js, 66 ordinary keys.
- dev2370/2901 to Common66_X68; geometry 6a8f9cc7.js, 66 ordinary keys.
- dev2356/2935 to Common83_SG9015; geometry b2a10b4e.js, 83 ordinary keys.

Geometry comes from the default SVG key rectangles, translated to origin;
widths, heights, gaps and row offsets are preserved. X65 encoder controls
are not fabricated as analog keys. Fn uses HallJoy's extended physical code.
Source hashes are pinned in tools/generate_attackshark_layouts.py and generated
attackshark_pro_layouts.h. The generator checks counts, unique usages and
non-overlap and supports --check. No driver parsing or SVG loading occurs at
runtime; these use the existing built-in preset cache.

Known vendor discrepancy remains: dev2901 is cataloged as X68 Pro with the
66-key geometry, but its default matrix resembles X82 including an F-row.
Automatic geometry follows the explicit vendor catalog; this does NOT establish
correct physical-slot routing for that revision. Do not describe dev2901 as
hardware validated. No matrix was silently changed to fit the drawing.

Validation: portable layout test PASS for geometry and all six identities,
unknown/PID-only identity rejection; generator --check PASS. Exact Release EXE
self-tests PASS for Shark, MINI60, NA87 and embedded installer. Shark test now
also verifies connected layout telemetry for all six profiles and token clearing
after disconnect. Test directory contains only HallJoy.exe (no forced logs).
MSVC Release PASS; existing ViGEm PDB warning only. No agent visual/hardware run.

Delivery: build/bin/Release/x64/HallJoy.exe, version 1.5.4.0.
SHA256: b19c9946b254c6c691110c9ea87f7ba4cb9d6d8a0e4dd4ca3b1708dd34f05c2f
Size: 9290240 bytes.
Evidence: .local/shark-layouts-verification.json; source backup:
.local/backups/shark-before-layouts.zip. GitHub publication unchanged.

Final static audit gate PASS (.local/shark-layouts-static-final.txt). The new
portable layout test is registered in tools/run_native_backend_checks.py;
the coverage gate initially identified its missing execution route, corrected
before delivery. No binary change was needed for this test-runner registration.


## 2026-09-19 — tester reports Forza input switching and blocking failure

Owner supplied a Discord screenshot: tester reports alternating keyboard/gamepad
prompts in Forza and claims Block Bound Keys stops both inputs and the green
pressed-key circles. Exact executable version and a log of this incident have
not been supplied. Earlier gameplay success does not resolve this new report.

Initial source review: Settings_SetBlockBoundKeys only stores the setting;
the low-level keyboard hook reports Backend_NotifyKeyboardEvent before deciding
to suppress Windows delivery. Current Shark native polling does not use this
setting or digital key state. ReadRaw01Cached consumes native depth independently
and, at the time of that initial review, permitted digital fallback when no
native backend owned the key. That historical fallback was subsequently removed;
see DIGITAL_DEPTH_EMULATION_REMOVAL_2026-09-19.md. It is absent from the current
build and must not be used as its explanation. A direct blocker-to-native-depth
dependency has not been established. The later full-path review and focused
build are recorded in INPUT_PATH_DIAGNOSTIC_2026-09-19.md. Prompt switching alone
does not establish that analog gamepad output stopped.
