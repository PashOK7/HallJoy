# Apex Pro TKL Gen 3: renewed firmware analysis — 2026-11-06

Exact wired1038:1642. Owner requested renewed firmware reverse engineering.
A concrete read-only implementation now supersedes the previous status-only
review. Hardware testing remains pending. No device command or firmware flashing
was performed by the agent.

## Pinned inputs and corrected replay scope

Official GG123.0.0 (2026-11-03), acquired as a public installer without executing
it, still carries bundle272111170 / firmware1.19.7. The exact 94,208-byte image
has SHA256 e492fccadfdbc6bb6eb4872166a8a20c1abc68ca5cf8ff0235af0ff7f8b8018f,
identical to the older GG120 acquisition. Installer SHA256:
e8846a8793b707e2829702112fe47314ed3dd2433ab80dc42099e33b68288bcc.
Vendor bytes remain private under
`.local/research/steelseries-apex-tkl-gen3-20261106/`.

Reset replay executes 42,768 actual instructions from08012744 to0801276A,
verifying the flash-to-RAM data copy exactly and stopping before hardware init.
No MMIO is modeled. The previously unresolved RAM dispatch objects are initialized
by this copy, with pointers into ROM. Group73/sub11 routes two SPI objects with
RGB-sized buffers; it is not yet a selected depth path.

The previous neighboring-model harness passed the command-byte pointer as R0 to
nested leaf handlers and swallowed emulator errors. The actual nested dispatcher
0800F240 calls08014340 to select an object context first. Those old bounded
negative leaf results cannot establish absence of live depth for Gen3.

## Concrete read-only candidate under verification

ROM context0801C4A8 references70 per-key records at200006F4 (24 bytes each).
Group73/sub50, read leaf08014F34/helper08014ED0, maps HID codes with0800EF70
and reads each record's halfword+2. It has no write counterpart in the table.
Group73/sub51, read leaf08014F6C/helper08014E44 with layer0, reads the separate
word+4, containing the sensor extrema used by ordinary key processing.

The sensor completion path08013CD4 calls08014B00, which copies the70 halfwords
from context+2C (20006710) into record+2. Scan-DMA callbacks080145A0/08014B2C
fill this buffer in five28-byte blocks. The same record+2 feeds ordinary
actuation/rapid-trigger logic08014918/08014B8C and travel display08017324.
This establishes a concrete current-sensor candidate, not just actuation settings.
End-to-end dispatch replay, response framing, mapping, side effects and host
integration still require verification. No hardware rate/typing test is implied.

Private evidence: inputs.json, acquisition-gg123.json, replay_boot.py,
boot-copy-proof.json, exact extracted images and pinned community references.
Public source: https://techblog.steelseries.com/2026/11/03/GG-notes-123.0.0.html.
## Selected path and separate behavioral review

Reproduce with tools/verify_apex_tkl_gen3_firmware.py, the exact external firmware
file and a new private output directory. It pins firmware, producer and replay
core, saves104 raw scenarios and a separate assessment. Result under the task
directory: behavior-reviewed/assessment.json. Execution PASS; assessment
REVIEW_INCOMPLETE. Shallow physical input, upstream filtering/timing, packet loss,
crash recovery and configurator contention remain unknown, not accepted as success.

The full080117F4 dispatcher executes90/F2/F5/F3-50/F3-51/B6. Outside-image factory
flash for F2/F5, ADC inputs and active mappings are explicit modeled inputs.
USB RX/TX success and GPIO row selection are modeled boundaries. DMA-buffer
assembly, current-sensor copy, context routing and reply formation execute the
actual image. A strict write guard permits response/stack/RX bookkeeping only.

Sensor context0801C4A8+30/+32 gives an operational window124..3970. A fitted,
calibrated sensor outside it fails the host session rather than becoming a
clamped full-axis value; this is a fault check, not a shallow-input deadzone.
Update interval average/max use QueryPerformanceCounter microseconds and include
metadata time between publications. No polling-frequency claim is inferred.

Ordinary typing is established only in the scoped sense that these reads do not
enable a mode or alter sensor/configuration state; physical USB delivery remains
untested. Independent per-key input, holds, release and learned extrema have
branch evidence. Normalization maps sensor extrema to0..1000, not measured
linear millimeters. The host never synthesizes analog from digital input or time.

Alternatives: legacy D7/DA absent;38-group thresholds are settings;73/11 SPI
access is not a complete sensor source;73/80 changes calibration/state and is
withheld. FFC1 states remain incompletely explored. No exhaustive absence claim.

## Integrated behavior and tester procedure

steelseries_apex_gen3_protocol.h owns Gen3 framing. The shared backend retains
legacy4.16.8 paths. Only1038:1642 with reviewed1.19.7, FFC0:1 /65/65/645, factory
variant, calibrated letters and a complete sensor snapshot may show connected.
Opened identity/capabilities are revalidated before output; write variants and
foreign/legacy depth commands are withheld. Empty discovery retries every2seconds
so a transient metadata failure cannot wait forever for another USB arrival.

Four up-to20-key reads form a full matrix without artificial delay or smoothing.
A calibration/mapping sweep every roughly2seconds is spread over14 input frames,
one extra read after each publication. Malformed or over-age snapshots clear the
session; stale values expire after100ms. Actual USB latency/rate is unmeasured.

Single-key remaps follow Automatic layout; chords/macros/non-keyboard assignments
retain physical analog channels with explicit labels. Manual keeps factory
assignments. Many-to-one aliases retain independent channels. Regional filtering
excludes unfitted positions even if their default extrema look valid.

ANSI85 / ISO86 / JIS89 presets use exact Gen3 SVG selection zones. Factory F2
plus identity/session proof selects distinct tokens; mechanical/media geometry
does not imply analog. Fn uses HallJoy extended409. Numeric extraction:
GG120 render/735.js, module60288, Gen3 device329/Gn and dt/Nt/wt. GG123 exact
descriptor272111170 verifies membership/regional replacements. Vendor source,
SVGs and firmware remain private. The local descriptor reader uses steelpatcher
commit34fc95ab746f555e575ee3d1e3f0398e7a71e373
(https://github.com/hurryman2212/steelpatcher), with its CFB import adapted to the
local cryptography version; this reader is not shipped.

HallJoy.log includes revision, stage1..5, firmware, layout/region, capabilities,
Win32 I/O errors, named parser refusals, calibrated-key/span bounds and aggregate
frames/sensor changes/returns-to-low/raw range/maximum update interval.
Milestones1/100/1000,30second and session-end summaries use the ordinary bounded
writer. No raw key-activity payload or forced logging is introduced.

Initial test: wired USB, GG/Engine/Prism fully closed, logging enabled. Check
slow/held/released WASD and Space, simultaneous keys, typing, gamepad output,
then Pause/Resume/reconnect. Return HallJoy.log through Open log. Only1.19.7 is
reviewed. Yellow hardware-testing notice remains. Native/release/layout and
live Sheet results follow when completed; no published release or hardware
success is claimed.


## Completed validation and synchronized status

Native backend suite PASS (cached, required compiler). A focused follow-up
recompiled only the affected Apex protocol test after adding the sensor-fault
boundary guard:104 firmware-derived vectors, rejection/atomicity checks,
regional fit, aliases, layout tokens, notice and124/3970 limits PASS.
The pinned private-source/layout reference record was refreshed once because
the catalog/notice/generated layout sources changed; all original audits PASS.

Ordinary tools/build_release.ps1 PASS: all27 retained backends, actual-image
checks, embedded notices/licenses, production-linked profile/layout suite and
startup recovery. The compiled ANSI85/ISO86/JIS89 key sets and geometry match
their reviewed sources; all three exact auto-selection tokens are present.
No visual application test or physical keyboard test was run.

Delivery: build/bin/Release/x64/HallJoy.exe, version1.6.14.0,11,365,376 bytes.
SHA256:8302980892e1124bd7d02762b5a0c66a4c90d7e5028e20ef7bb2872b8d566933.
This is a local tester build, not a GitHub release. Existing Keydous RF revision4,
signed stick-range fix and all retained ordinary functionality remain included.

2026-11-06 live Main!C723 changed Research incomplete ->
Implemented; awaiting hardware testing. Fresh A721:C725 pre-read unchanged;
readback verifies exact identity/status, preserved strict dropdown, yellow base
and effective B:C colors, and unchanged neighboring rows. No notes/comments or
structural edits. Complete fresh Main!A1:C1301 status/color read: all284 yellow
rows match runtime notices; all87 Supported models pass the compiled layout/
live Sheet check. Original TKL/full-size Gen3 and all wireless/2023 neighbors
retain their prior statuses.

Private evidence in the task directory: native.log, apex-final-check.log,
release-build.log, behavior-reviewed/, sheet-before.json, sheet-after.json,
sheet-all-statuses.json and final-validation.json. Source/docs backups are
hash-verified under .local/backups/2026-11-06_apex-tkl-gen3/; generated layout
replacement has its own layout-integrate backup.

## First hardware report, reviewed 2026-11-07

The tester reported using the graph. A later screenshot supplied the preceding
complaint about buggy behavior, documented below. The complete support log (schema2,
2026-11-06T20:41:13Z, 722559 bytes) was reviewed locally. SHA-256
`d0d3c08071151832e3d332b2884cd41f3ab947e9d59cc27c4c610fe939f61779`.
The file is retained privately in Downloads. The log reports Gen3 revision1.

- Exact wired 1038:1642 and firmware 1.19.7. All four starts reached stage5
  in 16–31 ms after the backend started. F2=0 selected the ANSI identity
  with region1 and 61 calibrated magnetic channels in each session. The log
  establishes the identity used for selection. The tester did not report
  inspecting the preset in the UI.
- 97,533 complete matrices by the last native snapshot. The first session
  ran about 6m37s and published 78,808 matrices with 5.043 ms mean spacing
  (about 198 matrices/s) and 10.024 ms maximum observed spacing. This is
  host matrix cadence. It does not measure the firmware scan rate or
  keyboard-to-game latency. No sensor fault, parser refusal or expired
  frame was recorded. The report indicates five dropped log records.
- All three Win32 995 entries occurred at StoppingProviders on the way to
  Paused, alongside `apex.gen3_session_end value=1`. The backend counts
  cancellation of an in-flight read on normal stop as a failed update.
  The counter stayed at three during the fourth session.
- The virtual Xbox 360 target appears in Windows device inventory. The
  bound-key trace shows intermediate analog values, both left-stick axes
  moving at once and reports returning to center. Recorded X spans
  -32309..32767 and Y spans -32768..32767. Every traced button mask was zero.
  The trace is taken before transport submission. It verifies calculated
  reports and does not record game-side input or a Guide press. A dedicated
  slow press, hold and release test in a game or external controller tester,
  plus ordinary typing, remain open. GG coexistence is still unverified.
- ViGEmBus backs the virtual Xbox 360 controller. HallJoy does not depend
  on Xbox Game Bar. The log does not identify what requested the Windows
  Game Bar popup. If it recurs, the useful evidence is the exact popup and
  action that triggered it.

Status remains Experimental pending the targeted functional check. On
2026-11-07, a live read of Main!A721:C725 verified the exact SteelSeries
model at row723, `Implemented; awaiting hardware testing`, the strict
dropdown, and matching yellow base/effective B:C fills. No Sheet write was
required. The current runtime notice and README already match that status.

## Reported gameplay issue, reviewed 2026-11-07

The preceding Discord exchange shows the tester mentioning Roblox as a
possible test, then reporting buggy behavior. The exchange does not confirm
which game was running or specify the symptom. The posted attachment matches
the existing report by its UTC header and beginning. Treat the tester report
as a failed functional check with an unspecified symptom.

The engine was Paused from uptime 898140 to 1563984 (about 11m06s). The only
explicit snapshot request in the report is at 1577218. If the file header UTC
is anchored to that request and Discord is displayed at UTC+3, the pause
roughly spans 23:30–23:41 and overlaps the complaint at 23:37. This comparison
cannot date the actual gameplay or explain why Pause was requested.

All retained binding traces have Snap Stick enabled. The configured builder
can prioritize the deeper of opposing keys and retain direction near a tie.
The retained trace has no adjacent pairs within 100 ms jumping between
opposite axis values beyond 10,000 units on both sides. That is a bounded
check on report generation. The ordinary report lacks a ViGEm apply
checkpoint and any observation from the game. No cause is established.

Use the same Apex test build for a short reproduction while Active. Record
which game and what the stick did, then compare with an external controller
tester and repeat the failing action with Snap Stick disabled. Save the
ordinary log immediately after the symptom. If the source and calculated
report look correct during the failure, the next diagnostic build should
expose ViGEm publication and apply progress in the ordinary report.

Experimental remains appropriate. A live read of Main!A721:C725 on
2026-11-07 reconfirmed row723 with the same status, strict dropdown, and
matching yellow base/effective fills. Runtime and README agree.

## Symptom attribution after the tester clarified the Roblox report

The tester reports faster movement on light presses, slower movement on
deeper presses, and stutter while also using the mouse. They had edited
the HallJoy graph. The retained log shows the descending stick response
with only one side of the axis active. At uptime 705531 and 705562, the
bound A channel rises from native 163 to 377 permille. HallJoy raw input
equals those values. The curve output falls from 759 to 420 permille and
LX goes from -24887 to -13762. D is zero after the curve in both rows.
At uptime 706609–706734, native depth falls from 237 to 79 while the
curve output rises from 612 to 959.

All 16,132 bound-key observations in 4,033 pad traces have matching
native and HallJoy raw values. ReadFiltered01Cached and
BackendCurve_ApplyByHid supply the middle value to the stick builder.
The graph supports descending segments. A descending section of the
active curve is the leading explanation. The log did not retain the
full controls at the time, so the exact settings and any editor defect
remain unconfirmed. Later configuration snapshots describe a different
time. The exact Roblox play interval is unknown.

The log lacks ViGEm apply timing, Roblox input selection, mouse movement
and proof that Roblox received digital WASD. trace.os_key is a Raw Input
reference inside HallJoy, independent of low-level hook suppression.
Roblox documents preferred-input changes based on controller and
keyboard/mouse activity. Mixed input is a separate hypothesis.

Use the same test EXE. Capture the original graph, then test with the
active curve straight and rising from 0% to 100%, Invert axis off and
movement keys using that curve. The straight-curve button preserves
endpoint values and inversion. Sweep and release one bound key in an
external controller tester. Then enable Block Bound Keys with the keys
released and repeat in Roblox with the mouse still and then moving.
Save the ordinary log immediately after a failure.

Status remains Experimental. Live Main!A721:C725 again verified the
exact row723, yellow base/effective fills and strict validation.


