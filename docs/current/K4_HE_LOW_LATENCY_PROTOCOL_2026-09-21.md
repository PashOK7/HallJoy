## r5 precision path installed and owner raw capture completed (2026-09-21)

LIVE: firmware r5 installed, full flash readback matched. Keyboard back in
ordinary mode phase0/native0 after probes; XInput/DirectInput empty. HallJoy
closed, no recorder running. Owner may type normally/open existing EXE. Host
EXE unchanged0671d3a6cd63c61364448b2cd3b6dfcd4c5858f33031a7d93699baee88fd4fa3;
r5 is compatible with that host. No publication or forced logging.

Implemented:
- analog_matrix.c stores fresh ADC/validity BEFORE the legacy5-count gate.
  halljoy_analog_precise uses current individual calibration and scale factor,
  same polynomial, factored float evaluation to avoid subtraction cancellation.
  No0..240 quantization or temporal filtering on the onboard gamepad path.
- hjo_curve_apply now consumes that float directly; only final XInput integer
  encoding rounds. Trigger format is still8bits. Digital/rapid-trigger logic
  and legacy scan schedule remain unchanged; no per-key hardcoded calibration.
- Existing keyboard UI telemetry is still legacy0..240 to retain r4 transport
  compatibility/performance; actual Windows gamepad tester sees precise output.
- Status bit3 precision/capture. Explicit7C RAW1 capture512 consecutive full-scan
  samples of one physical slot;7D retrieves raw ADC, depth16 diagnostic copy,
  scan duration, old travel, validity and per-sample zero/full calibration.
  Allowed only OFF. Bounded buffer shares profile staging, invalidated by OPEN.
  Not active in gameplay; no persistent logging/EEPROM writes by capture code.
- Diagnostic initially used separate6KiB buffer; changed to staging union before
  flashing. Final heap0x2000d660..0x20010000=10656bytes, stacks reserved separately.

Validation:
- Portable precision test54054 samples: monotonicity, every single ADC step
  retained as distinct stick code for modeled spans800..1200, endpoints and
  independent double polynomial reference PASS. Registered native test runner.
- All100 saved device calibrations:826..1174 distinct modeled stick codes,
  maximum normalized reference error4.47035e-6. This is mathematical coverage,
  NOT that many reliably distinguishable mechanical positions.
- ARM/QMK build PASS. r5 raw app142788bytes, QMK suffix removed; final full image
  SHA256 fac1a4322d0de1be684792f5797fce30ad97dd3c0ca5161757ca4a6d50b628ed.
  QMK BIN31909af4d4c59ee6d7d7b9ec8aa51998b9632294202ce87970f016e4a5acf749.
- Fresh double-read r4 backup onboard-r4-before-r5.bin SHA256
  702b80f74e95e9f31df1c2d8cebdeb3420f9f6fa0ec77482b9fae4c92e686c2b.
  Matched r4 outsideEEPROM; preserved fresh0x4000..0x7fff. Full256KiB readback
  verified before boot. Existing automatic calibration behavior stays enabled;
  no manual reset or forced calibration. Baseline saved endpoints need not equal
  current adaptive calibration. Complete old UAP backups remain untouched.
- Initial W/A/S/D capture each512samples: calibrated precise depth zero throughout,
  no invalid ADC, stable calibration within each recording. W raw3092..3099,
  std1.104counts. S/A/D larger ranges may include incidental touch/drift: no
  owner-confirmed stationary posture in those initial captures. One D scan outlier
 13734us exists; do not conceal it or claim a hard latency bound.
- Owner said ready; recorded five bursts during slow W press/release/holds:
 2560consecutive-within-burst samples, gaps while reading each burst. Raw930unique,
  precise16 depth842unique, old clamped travel237unique. Range0..65535 reached.
  Calibration stable across all bursts:zero3073/full1949. Scans2344..2507us.
  Unique counts include movement AND noise; not independent-position resolution.
- After explicit release instruction and3s pause, separate512W samples all0depth;
  raw3091..3098, std1.157counts, scan2344..2373us. Release-to-zero confirmed.
- Quietest interior32-sample window selected retrospectively: raw range4counts,
  std1.290, normalized depth51454..51586 (~0.20% peak-to-peak). Handholding can
  include motion; this is NOT an unbiased full-range sensor-noise specification.
- Existing production worker tested against r5:active1,neutral actual report1,
 219telemetry updates, Joined, reservation cleared. This is neutral profile;
  owner movement capture was OFF and does not itself record actual XInput codes.

Evidence under .local/backups/keychron-protocol-20260921/:
 before-precision-r5.zip, onboard-r5-source-overlay.zip, onboard-r5-owner-analysis.json,
 onboard-r5-w-owner-1..5.json, onboard-r5-w-owner-release.json, *initial-raw.json,
 onboard-precision-r5-image.json and full/readback/boot images. Logs .local/k4-r5-*
and k4-precision-r5-build.log. Overlay/base.patch/README refreshed.

User requirement to remove unnecessary analog discretization is implemented.
Do not call hardware noise/latency ceiling fully measured or claim ideal/noiseless
behavior. Full scanner redesign/fresh1000Hz remains separate. Next owner action
is ordinary use of current EXE to assess finer analog feel; no further mandatory
question or active physical recording. Future qualification can measure fixed
mechanical positions and real output codes across the range if warranted.

## Analog resolution audit after owner gameplay acceptance (2026-09-21)

Owner confirms game feels lower-latency than UAP and is satisfied; requests
actual analog step counts and achievable resolution ceiling. Acceptance does
not establish hardware-optimal latency or supersede unfinished scanner work.
No firmware/runtime change in this review.

Current chain: STM32F401 ADC configured12bits (ADC CR1=0),4096 code alphabet;
update_raw_value rejects abs(last_val-value)<5 before updating calibrated travel;
legacy polynomial rounds to uint8 travel with TRAVEL_SCALE6, FULL_TRAVEL_UNIT40.
Nominal useful range0..240 =>241 levels including rest. Above240 clamps to1
at onboard curve input; changing telemetry integer width adds no information.
Firmware applies float curves then rounds stick to32767*v and trigger to255*v.
Compiled enumeration using actual shared curve/mapper with one key, linear
identity curve:241 unique axis codes AND241 unique trigger codes. This is
software enumeration, not hardware observation of all levels in a slow press.
Curves/plateaus and sampling/gating can reduce levels. Multiple-key combinations
are a separate question, not resolution of one key. XInput format allows65536
signed axis codes (current symmetric mapper uses-32767..32767) and256 trigger
codes. Single positive half axis allows32768; source is bottleneck for sticks.

Existing connected-device saved calibration (before first flash, not a new
measurement):100keys, zero-full raw spans min825, median1109, max1173 counts.
W1101, A1123, S1060, D1097. Inclusive integer ranges add1 to these spans.
These are calibrated endpoints, not independently distinguishable positions.
The5-count gate suppresses small changes; it is not a fixed241-level quantizer
and must not be simply divided into a claim of hardware resolution.

Next engineering target: capture unfiltered raw ADC BEFORE that gate and before
travel rounding, retain exact per-key calibration/polynomial and compare noise
at rest/held positions with very slow press/release/repeatability. Record actual
scan timing while instrumented; do not improve numbers by smoothing away onset.
Then feed higher precision calibrated values directly to onboard curves/pad,
independent of legacy digital/rapid-trigger travel as necessary to preserve
behavior. Existing compact UI telemetry can remain lossy only if explicitly
separate from high precision gamepad output, or negotiate a new representation.
No promise of4096 stable key positions or1000+ usable levels before noise data.
No claim that all hardware steps reach a game during a fast key press.

Evidence: .local/k4-resolution-enumerate.cpp/.exe actual-header enumeration;
connected-k4-calibration-before.json under existing backup root. API references:
https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/xinputongameinput/structs/xinput_gamepad
https://www.st.com/resource/en/reference_manual/DM00096844.pdf

> 2026-09-21 Owner requires an ANALOG scale, not binary camera flashes. Replaced camera tester patch with buffered signed per-axis scale and0..100% triggers, raw value/percentage, no smoothing. Direct fast OS read preserved; build and linked checks passed, EXE delivered. White-stuck report is not proven firmware stuck input; previous any-nonzero/max-axis visualization was unsuitable. See GAMEPAD_LATENCY_TESTER_2026-09-21.md.

> 2026-09-21 Delivered Gamepad Tester > Camera latency test: independent direct Windows controller observation/render thread, high-contrast patch and explicit CSV export. Headless real-device poll gaps median0.9974ms / p95 1.0648ms; not physical latency or GUI validation. Firmware remains r4. Owner camera check next. See [tester checkpoint](GAMEPAD_LATENCY_TESTER_2026-09-21.md).

## Slow-motion target clarified (2026-09-21)

Owner filmed HallJoy Gamepad Tester and key highlighting, judged latency by
Gamepad Tester. Key highlighting visibly lagged more but is not his concern.
Therefore the film includes our independent OS monitor polling (minimum10ms)
plus window timer/render/display sampling; it does not isolate native gamepad
arrival. These asynchronous stages can produce variable visible delay even
with unchanged device latency. Faster release is an observation, not yet a
proven firmware asymmetry. A near-simultaneous filmed release does not establish
zero input latency. Need event/acquisition timing independent of HallJoy's UI
before scanner changes are evaluated against this observation. Prior question
is answered; do not ask again.

## Owner slow-motion observation and scanner source review (2026-09-21)

Owner reports lower visible latency than UAP, but variable delay and apparently
faster release than press; one release appeared nearly instantaneous. Asked what
was filmed (game/browser tester/HallJoy tester/key visualization) and what screen
change defined the event. Answer pending. Do not interpret this as measured
sensor-to-XInput latency, proven press/release asymmetry, or zero-latency output.
No runtime/firmware change or hardware recording in this review.

Confirmed current source, isolated r4 worktree:
- analog_matrix.c update_raw_value rejects abs(last_val-value)<5 before updating
  travel. This is a sensor-delta gate, not a fixed millisecond delay; applies in
  both directions. Slow movement may accumulate before publication.
- travel is quantized and uses per-key calibration/polynomial. DEAD_ZONE30 in
  convert_to_travel only feeds an unused local delta: do NOT claim that constant
  is an effective30-count deadzone. Zero calibration offset is separate.
- analog_matrix_scan.c reads columns sequentially with40us settling and ADC;
  transition rechecks can change scan duration. Native mapping/send is after
  all19columns. Existing measured scan duration~2.3-2.4ms is NOT end-to-end delay.
- gamepad submit retries when endpoint busy; no intentional press-only timer.
- HallJoy key visualization has~20ms snapshot cadence; real pad display has
  independent OS polling/window rendering. Filmed output must be identified
  before attributing timing variation to scanner versus display sampling.

Next: use owner's measurement target to choose the corresponding diagnostic
path; retain calibration and avoid arbitrary filter removal without raw-noise
qualification. Scanner redesign remains authorized and unfinished.

## r4: real Windows pad monitor and faster key telemetry (2026-09-21)

CURRENT STATE: r4 flashed, full 256 KiB readback matches. Keyboard is in ordinary
mode, phase0/native0; XInput and DirectInput have no attached controller after
headless tests. HallJoy is closed; no recorder/DFU session is active. New EXE
installed at build/bin/Release/x64/HallJoy.exe, SHA256 25b868e51e076443170780d66a1eb244c38705e06a1870e5af7e3a760beccc80.
No forced logging, GUI run by agent, public release or support-status change.

Owner confirmed r3 works with his profile and browser tester is smooth, but
HallJoy keyboard UI and internal tester look approximately10fps. This supersedes
the previous pending app-functionality question. Cause: full114-key telemetry
used12 separate HID exchanges and the pad display was published only alongside
that slow snapshot. Smooth external controller output did not depend on it.

Changes:
- keychron_onboard_gamepad.cpp reads actual Windows.Gaming.Input Gamepad state,
  matched through RawGameController VID3434/PID0E40; ambiguous devices are refused.
  This is OS controller state, not a reconstructed profile or vendor pad query.
- Dedicated monitor thread does not share HID, profile upload or depth generation.
  Backend_GetLastReportForPad reads its cached snapshot on each UI call. Polling
  follows UI refresh with Windows SetTimer minimum10ms, not1000Hz; no OS reads
  when main-window monitoring demand expires. Standard WGI GamepadButtons does
  not expose the reserved Guide button. Legacy ViGEm paths remain unchanged.
- A9/7B capability bit2 sends one coherent130-byte compact frame in6 response
  fragments from one request. Exact existing0..240 travel preserved, with CRC,
  session/order/length checks before publication. Old12-page fallback retained.
  Gamepad send stays before bounded telemetry send; no blocking response queue.
- Removed redundant post-telemetry worker sleep while visible; bounded HID reads
  already wait for fresh data. Hidden worker keeps its lease/settings waits.

Evidence:
- Final production-worker headless probe: active=1, actual_neutral_report=1,
  telemetry_updates=228, stop_status=Joined, reserved_after=0. Same8s scenario
  previously produced53updates; includes startup and active profile replacement,
  so it is NOT a steady-state rate measurement. Neutral bindings only.
- Direct production client:120 coherent snapshots,48.6232Hz, median19.9907ms,
  p95 25.0055ms, active profile replacement and confirmed Close PASS.
- Compact precision(all241levels), corruption, truncation, ordering tests PASS;
  client lifecycle/legacy fallback/heartbeat-priority/failure tests PASS.
- Full static native-backend audits PASS. Final Release x64 build and four
  linked-image checks PASS; installed by standard atomic publisher. Known
  missing ViGEmClient.pdb warning only. First standalone worker compile omitted
  the existing UAP include directory; corrected and final compile/test PASS.
- Fresh r3 flash backup two matching reads: onboard-r3-before-r4.bin,
  SHA256 7fec05f95111b7bbc09771f14c695bfdcf8c00f61c6d69f1bbdbd457fe7f9286.
  Outside EEPROM it matched installed r3; current sector0x4000..0x7fff preserved.
- r4 full image SHA256216e19eeeb70cdff4f0257126394cd47385c71c08ed3b146704739b6dbce822e;
  raw app141852bytes; QMK suffix stripped. Full readback matched before boot.
  base.patch refreshed and onboard-r4-source-overlay.zip saved. Prior host and
  firmware sources are in before-smooth-monitor.zip. All under existing backup root.
- Logs .local/k4-r4-*, .local/k4-channel-r4-result.log,
  .local/k4-worker-r4-final-result.log; final status record onboard-r4-final-status.json.

NEXT OWNER CHECK: open updated EXE and compare internal tester/key animation to
browser with real key movement. No agent visual test was performed. Do not claim
visual smoothness verified by the neutral headless test. Scanner is unchanged
(~2.3-2.4ms); this fixes monitoring, NOT1000freshHz or physical onset. Scanner/noise
redesign, hidden-window overlay demand and remaining integration items below
remain open. Per-key calibration and S O-rings need no hardcoded workaround.

## First integrated host candidate delivered; owner app test next (2026-09-21)

LIVE STATE: r3 firmware booted in ordinary keyboard mode. No DFU/build/recording
is active and no HallJoy GUI was launched by the agent. Delivered experimental
EXE: build/bin/Release/x64/HallJoy.exe, SHA256 8d3485914b037a0b53254fabd399853c8f89d79fc523dda5326972c11339ecc9.
Built/published with tools/build_keychron_onboard.ps1; property
HallJoyKeychronOnboardExperimental=true admits the custom provider. Ordinary
build_release.ps1 does not opt into the experimental route. No forced logging.
Previous EXE backed up by the standard atomic replacement workflow. No GitHub
publication or public support-status change.

Implemented host:
- keychron_onboard_client.h: one session owner, exact response/token validation,
  staged profile upload, active updates with prioritized heartbeats every80ms,
  no repeat upload for unchanged profile CRC, coherent depth CRC checking,
  read-only actual pad report and STOP/ordinary-descriptor confirmation.
- keychron_onboard_channel.h/.cpp: native Win32 overlapped HID, bounded waits,
  cancellation/drain, exclusive command ownership, exact VID3434/PID0E40,
  revisions1212/1213, FF60/61 and HJO1 serial suffix; reconnect same serial only.
- keychron_onboard_backend.h/.cpp: registry-owned BeforeUap worker, exact path
  claim before UAP, capability-bit proof, reserved native output throughout
  reconnect, profile capture, telemetry only when visible, worker exception
  barrier and confirmed join. Unknown key/mouse/multiple-pad profiles rejected.
- backend.cpp reserves output instead of starting/configuring/publishing ViGEm;
  native branch only updates key UI/capture and displays the firmware-submitted
  XInput report. It does not recompute the gamepad on PC. App admission controls
  activation; app visible timer renews telemetry demand. Native K4 layout token
  selects the existing ANSI imported preset. Other users retain legacy UAP.
- realtime loop idle heartbeat is50ms on native output, fresh telemetry still
  wakes immediately. No deliberate delay is imposed on firmware gamepad output.

Firmware r3 only adds actual-pad query A9/7A. Status byte17 is now flags:
bit0=profile ready, bit1=actual-pad query supported. Query7A returns usual HJO1
header0..11 plus20bytes of last submitted native XInput report at12..31; zero
when inactive. Host native admission requires bit1. No scanner/calibration
algorithm change. Firmware source overlay and base.patch refreshed.

Flash evidence under .local/backups/keychron-protocol-20260921/:
- onboard-r2-before-r3.bin: two matching reads, SHA256
  99b00276853d31ed3eeb98665026c643f5c108251215fb926dca916d3c7902e2.
  Executable outside EEPROM exactly matched r2 image. First backup attempt ran
  before DFU enumerated and safely refused empty device list; retry confirmed
  sole ROM serial3381347A3035. No erase/write occurred on that failed attempt.
- onboard-integration-r3-full.bin SHA256
  208bec816587b16746ec8fa7a8ab99e482da3400d8a98e0926b156f35b1689fd.
  App141428bytes, QMK BIN SHA256
  5f751bcddb00cdd3e6f6054794029c8b123c704acaf8cafc3b3e8d9788d43136.
  Stripped DFU suffix, preserved fresh EEPROM0x4000..0x7fff. Full256KiB readback
  exactly matched before boot via16byte upload/leave. Source overlay ZIP saved.

Validation:
- portable client test PASS: lifecycle, >1s active upload maintains500ms lease,
  unchanged-profile skip, corrupted telemetry leaves destination unchanged,
  wrong token, cancellation and failures at OPEN/BEGIN/CHUNK/COMMIT/START.
- actual C++ channel/client on device PASS: neutral OPEN, active profile update,
  depth, heartbeat and confirmed Close. Then production native worker headless
  probe PASS, repeated after final worker changes: active=1, actual neutral
  report=1,53 telemetry updates, Stop Joined, output reservation cleared.
  The probe uses actual native routing; only realtime notification is a no-op
  because no realtime consumer/UI exists in that probe.
- after stop: phase0/native0, DirectInput8 returns S_OK with zero controllers.
- all static native-backend audits PASS. Final layout coherence and realtime
  cooperative-shutdown audits PASS after their source updates.
- Release x64 app build PASS plus four standard linked-image checks PASS via
  build_keychron_onboard.ps1. An intermediate build failed after an include was
  inserted ahead of the existing UTF-8 BOM in realtime_loop.cpp; moved BOM back
  to byte0 and final rebuild/checks passed. Known missing ViGEmClient.pdb linker
  warning only. No UI/visual run by agent.
- result/hashes: onboard-r3-host-candidate-result.json. Logs .local/k4-host-* and
  .local/k4-onboard-r3-*. Worker hardware probe sources in tools/.

NEXT OWNER ACTION: open delivered HallJoy, use one keyboard-only gamepad profile,
verify bound keys move the tester, one controller appears while active and is
absent after closing. This is the first actual app/profile gameplay test; neutral
hardware tests do not establish correct existing-user-profile/UI behavior.

REMAINING, do not call ideal or completed: old scanner still~2.3ms/sweep and
legacy raw/travel gating; diagnostic paged telemetry about16fps; improve telemetry
transport and scanner without hiding noise or discarding movement. Need physical
qualification of actual analog onset, calibrated range and noise after redesign.
Suppression hotkeys/Allow Alt-Tab behavior needs explicit preservation review;
overlay demand while main window hidden and reconnect/failed-session edges need
further integration verification. Output reservation is chosen on engine startup;
a newly attached K4 after another route starts needs the normal engine re-probe.
Mouse and additional pads intentionally outside first onboard scope. S O-rings
must never cause hardcoded calibration. User may have touched arrow keys in prior
recording; those observations are not noise proof.

## Owner clarified extra keys and S travel (2026-09-21)

Owner could have touched other keys during recording; Down/Right observations are not evidence of noise. S has silicone O-rings, so physical bottom can differ from full sensor travel. No special-case calibration for S or this keyboard: retain per-key calibration and recalibration. Recorded full normalized output proves only the existing calibrated range. Resume host integration; no owner question is pending.

## Owner-assisted analog recording completed (2026-09-21)

After owner said ready, recorded 20 seconds of read-only depth telemetry:
onboard-r2-depth-wasd-owner.json (321 CRC-valid calibrated frames).
W/A/S/D are slots 40/58/59/60. All reached 65535 (full travel), with respectively
35/59/34/45 distinct observed levels including zero. All have released samples.
W and A were still nonzero when the timed recording ended; this alone is NOT
stuck input. Agent then asked owner to release all keys and captured a separate
five seconds: onboard-r2-depth-release-owner.json, 81 frames, all 114 slots zero
throughout. No XInput device in either recording. Scan duration 2324..2503 us
while pressing, 2326..2366 us after release. Sampled intermediate travel/full
range/release verified; neither onset latency nor fresh 1000 Hz established.

Additional small depth observations: Down arrow (slot109, HID0x51), one frame
at1365/65535 (2.08%); Right arrow (slot110,HID0x4f), two nonzero frames,
maximum546/65535 (0.83%). Asked owner whether these keys were touched; answer
pending. Do not classify as noise or accidental presses before that answer.
Unfiltered ADC measurement will be needed to qualify scanner changes without
hiding noise using an arbitrary deadzone. Existing depth values already pass
legacy raw-delta gating and travel quantization. Telemetry remains ~16fps.

Analysis plus evidence hashes saved as onboard-r2-depth-owner-analysis.json
in .local/backups/keychron-protocol-20260921/. No firmware, calibration, profile,
controller mode or app executable was changed by these read-only recordings.
Recorders are finished; owner need not keep pressing keys.

## Host profile foundation and next physical check (2026-09-21)

Added keychron_onboard_host_profile.h/.cpp to the app project. CaptureProfile
holds the profile read lease, exports one pad, exact K4 6x19 physical positions,
100 keys including Fn 0x409 and RGB 0x404, axes/triggers/many-key buttons,
Snap/LKP/sensitivity and bound-key suppression. Unsupported keys/mouse/multiple
pads fail explicitly without changing the destination. No native worker or
output ownership is registered yet; this code does NOT enable onboard in the
HallJoy EXE. Ordinary UAP routes remain unchanged.

BackendCurve_ExportPrepared uses the same normalized cached curve definition as
production ApplyByHid, with prepared rational weights. Updated real-production
curve comparison test: 102800 samples + curve boundaries PASS. Added host profile
export test: all 100 physical identities, real curve export, extended keys,
wire encode/decode and nonmutating rejection PASS. Both Windows tests are in
run_native_backend_checks.py. Project/filter XML parses. No full app build or
new EXE delivery performed in this step. Backups: before-profile-export.zip and
before-host-profile-registration.zip under the existing backup root.

Read-only depth mode added to tools/keychron_onboard_probe.py. It never opens a
gamepad session, writes a profile or changes calibration. Validates 12-page
HJK4 assembly, header, CRC and calibration flag and saves timestamped full
matrix frames. Three-second r2 baseline recorded 49 valid frames, all 114 slots
zero, no XInput, scan duration 2323..2358 us. Evidence:
onboard-r2-depth-baseline.json in .local/backups/keychron-protocol-20260921/.
This is gated/quantized travel, NOT unfiltered sensor noise or physical onset
measurement. The current request/page telemetry is only ~16 frames/s here;
it is a diagnostic transport, not the final smooth UI architecture.

Next owner action: a short coordinated recording of slow W/A/S/D presses,
full hold and release, to verify real depth/matrix/release on this image.
Recorder is NOT currently running. Start a 20-second depth recording after
owner indicates readiness; no app/game needed. Do not require another flash.
This does not establish 1000 Hz or complete scanner/noise qualification.

WinMM probe policy now always records residual metadata separately, and fails
on it only with --strict-winmm. Default cycle PASS means the XInput neutral and
watchdog lifecycle only, not clearance of every legacy API. Original strict
r2 failure evidence is preserved, alongside owner joy.cpl-empty confirmation.

Outstanding: native host worker and exact route/output ownership, actual
firmware pad report for UI, heartbeat priority independent of telemetry,
profile updates/rollback, pause/close/hotplug, suppression shortcuts/Alt-Tab
semantics, faster telemetry, scanner redesign and physical qualification.
Do not present the current firmware/host foundation as a finished release.

## Owner confirmation after r2 (2026-09-21)

Owner reports: "печатает нормально, в joy.cpl ничего нет". Ordinary keyboard input and user-visible controller disappearance are confirmed. This resolves the owner check below. XInput/DirectInput evidence remains valid; the strict probe failure for residual WinMM metadata is retained, not reclassified as every API being cleared. No registry cleanup is needed for the confirmed scenario. Continue HallJoy profile/telemetry/output integration; full app operation and faster scanner remain unimplemented.

## Latest r2 hardware result and next owner check (2026-09-21)

r2 finished successfully and is NOW INSTALLED, booted in ordinary keyboard mode.
No current build or flash sessions remain. r2 full flash SHA256:
2a7bf03070ea02ff64782c3710f9b0ba89a775ed1c3ebc70370dfd7f872c338b.
QMK BIN SHA256 474ab53d6a079520198da4dce2575ac84ab19ee3a9ef496a5df08397ff7bd0e4.
EEPROM sector copied from double-read onboard-r1-before-r2.bin; full readback
onboard-integration-r2-readback.bin exactly matched before boot. `:16:leave -U`
boots without writing again. Flash/readback/boot logs are .local/k4-onboard-r2-*.
Maintained base.patch refreshed to match current source overlay.

Actual ordinary HID serial is now
45004B00145135303534363300000000HJO1. PnP creates fresh child nodes; former
joystick Col03 now correctly named System Control. HID enumeration has no
Generic Desktop joystick/gamepad usage 4/5. No registry value was removed and
no driver was installed/removed. joyConfigChanged(0) merely refreshed WinMM.

r2 neutral profile cycle again successfully activates one XInput gamepad with
all controls neutral; watchdog removes it and restores bcdDevice 0x1212. The
strengthened probe deliberately reports FAILURE because WinMM still returns K4
at index2. Do not hide this failure or call every API cleared. Independent native
DirectInput8 EnumDevices(DI8DEVCLASS_GAMECTRL,DIEDFL_ATTACHEDONLY) returns S_OK
and zero callbacks after stop. WinMM lists nothing during DFU, but after normal
keyboard enumeration it exposes the old K4 entry even without HID gamepad usage.
Its exact cause remains unresolved; serial suffix fixed PnP naming but did NOT
by itself clear WinMM. Do not claim it did. No speculative registry cleanup.

Evidence: onboard-r2-cycle.json, onboard-r2-directinput.txt,
onboard-r2-hardware-result.json in the backup directory. Native diagnostic C++
source/exe .local/dinput_probe.cpp/.exe. Hardware probe now waits up to 15 s for
initial custom enumeration; activation still has the firmware 5 s arming lease.

Next required owner check before further sensor work: confirm ordinary typing
on the new image and open Windows joy.cpl with HallJoy closed, report whether K4
is actually listed there. Owner evaluates visual UI, agent does not launch it.
This discriminates user-visible controller persistence from legacy API metadata.
Do not request another flash authorization or Esc entry: maintenance command
works. This remains an experimental firmware; HallJoy GUI/new-provider ownership
is not integrated, old HallJoy does not yet operate this new protocol. Legacy
UAP support in app is unchanged for other users. ADC/scanner still ~2.3 ms, no
physical-onset or 1000-fresh-Hz claim.

## Live hardware checkpoint: r1 installed, r2 being prepared (2026-09-21)

Owner entered DFU with Esc. ROM serial 3381347A3035 matches the historical dump;
QMK serial differs by encoding, so earlier different-device suspicion based only
on those serial strings is withdrawn. Exact current full internal flash was read
twice and verified:
.local/backups/keychron-protocol-20260921/connected-k4-dfu-3381347A3035.bin
SHA256 85967addf0541028e114c007cef57bbb0c0d1e848ecc6cbf34906150da0ca3f7.
Outside emulated EEPROM 0x4000..0x7fff, it matches recovered legacy full-report
BIN executable bytes; final 16-byte DFU file suffix is correctly absent in flash.
External EEPROM is outside this dump. Calibration backup already recorded below.

Implemented experimental firmware overlay in firmware/keychron_k4_he/:
halljoy_onboard.c/.h, repeatable usb_descriptor_override.c, base.patch and README.
Shared explicit 5044-byte HJP1 profile wire core with CRC and atomic decode added
as src/HallJoyProject/HallJoy/keychron_onboard_profile.h. Corruption/truncation and
semantic invalid-data tests PASS. Portable lifecycle ARMING lease increased to
5 seconds for USB enumeration/profile upload; ACTIVE timeout remains 500 ms,
heartbeat proposal 100 ms. Updated session tests PASS.

r1 build PASS, log .local/k4-onboard-build-final.log; QMK output BIN (including
16-byte suffix) SHA256 cd3265db57ac6f1378c5ecebde16ec58bd1e8d2f61cfdf2fd48ba21b613882eb.
Executable image bytes excluding suffix: 141276. ELF heap starts 0x2000cfa8,
ends 0x20010000 (~12 KiB); process stack 8 KiB already reserved separately.
Full flash image overlays application on erased fill and preserves exact EEPROM
sector from connected backup. Flashed serial-selected Internal Flash alt0;
no option-byte/unprotect/mass-erase operation. Full 256 KiB readback matched:
onboard-integration-r1-full.bin SHA256
27932be089aaa9356b977c07f5c26fb41c795cc995434f58164a61e6c230c0e1.
Readback file onboard-integration-r1-readback.bin. Device successfully booted
using dfu-util -s 0x08000000:16:leave -U <new-file>; no second write needed.

Hardware probe tools/keychron_onboard_probe.py requires exact custom revisions
0x1212/0x1213 and HJO1 reply magic; does not activate legacy firmware. Cycle test
uploads an all-unbound neutral profile (no game actions), starts, sends 30 ordered
heartbeats, then withholds heartbeat to test crash behavior. Actual r1 result:
- Off: phase0, native0, XInput none.
- Native descriptor enumerated ~1.22 s after open; start ~2.49 s.
- One XInput controller index0, all controls neutral, at ~5.75 s.
- Watchdog removes XInput and returns ordinary revision at ~6.77 s.
Evidence onboard-r1-cycle.json. Initial status probe timed out after 5 s on first
Windows enumeration; next probe succeeds, recorded separately, not hidden.
Read-only A9/42 after flash: all 100 saved key calibrations match preflash values.
Evidence onboard-r1-calibration-after.json.

Important unresolved r1 finding: WinMM still reported VID3434/PID0E40 at index2
after native XInput removal, although HID enumeration has NO joystick/gamepad
usage. Former Col03 now has System Control usage 0x0080, but Windows retained
old generic-joystick associations. joyConfigChanged(0) did not remove it.
No admin rights available; no driver removal/install attempted. r2 descriptor
appends HJO1 to the complete original hardware serial to create a clean device
instance (without changing VID/PID), stable across onboard mode changes.
Need repeat native lifecycle AND WinMM disappearance before calling fixed.

Software DFU via guarded A9/79 + HallJoyDFU-K4-v1 succeeded on r1. Current hardware
is back in DFU while r2 builds. Fresh r1 image before r2 was read twice:
onboard-r1-before-r2.bin SHA256
2ef2655cb3f748bac26013991f35f9ebcb9f453331152c079abde96a2513db02.
Use its EEPROM sector for r2, not the old preboot sector. Build currently session
92057, log .local/k4-onboard-r2-build.log. Must check exit before flash; refresh
base.patch after final source edits. Original/legacy backups remain untouched.

Current limits: firmware uses the old synchronous ADC/travel algorithm; measured
scan section ~2321-2438 us, not 1000 fresh Hz. No new HallJoy app runtime/UI route
or app binary yet. Only hardware integration probe drives new protocol. Curve
and mapping software parity tests do not establish physical movement response.
Next: finish r2/check stale controller removal; then physical basic-key check,
scanner redesign/measurement and HallJoy host integration. No support/release
promotion or public documentation claims.

## DFU entry correction after owner reminder (2026-09-21)

Reset under Space is NOT the only method. K4 info.json and actual baseline
cflags enable BOOTMAGIC. Default position row0/col0 is Esc. Recovered source has
no Keychron override of bootmagic_scan/reset_eeprom in the searched model/common
paths: holding Esc while connecting enters DFU via generic bootmagic. That path
also calls eeconfig_disable before bootloader_jump, so it can invalidate saved
configuration. Installed binary equivalence is still unproven. Physical reset
remains preferable for an exact pre-reset backup, but do not claim a removed
keycap is mandatory. Owner recalls using the key-based path on earlier UAP flash.

## Implementation checkpoint and hardware action needed (2026-09-21)

Owner answers supersede open questions below:
- First version is ONE onboard gamepad, without mouse. Additional gamepads will
  later use ViGEm; mouse processing is future work, not required now.
- Gamepad MUST disappear outside HallJoy. Brief full-keyboard USB reconnect is
  explicitly accepted on entry/exit. Neutral but permanently visible is rejected.
- Flashing is explicitly authorized. No new permission gate needed.

New portable C11 modules in src/HallJoyProject/HallJoy:
- keychron_onboard_session.h: firmware-issued generations, OFF/ARMED/ACTIVE,
  expiring handshake and active lease, ordered heartbeats, STOP, timer wrap,
  no expired-session resurrection, neutral-delivery gate before reactivation.
  Initial lease 500 ms / heartbeat 100 ms is a tuning baseline, not hardware
  qualification. Adapter must clear pending active reports and implement actual
  neutral delivery/disconnect. Pure module does not itself perform USB I/O.
- keychron_onboard_mapper.h: compact 114-slot mapping, 4 axes/2 triggers/15
  buttons, production Snap/LKP transitions, validated binding slots and flags,
  bound-key lookup and XInput button translation. 40000 deterministic temporal
  comparisons against actual configured_xusb_builder.cpp PASS, including states.
- keychron_onboard_curve.h: prepared rational weights, current 18-step curve
  solve, segmented curves, invert, endpoints/anti-deadzone/output cap. No powf
  per sample. 102800 sample comparisons plus curve boundaries against linked
  production BackendCurve_ApplyByHid PASS (absolute tolerance 1e-6).
- Session edge-case suite PASS; all three modules compile together as C11 for
  Cortex-M4 hard float with warnings-as-errors. Probe text 2156 bytes, not a
  full firmware size or timing benchmark. Tests registered in native runner;
  Windows-only production curve test uses real settings/curve sources.

These modules are NOT yet wired into firmware or HallJoy UI/runtime. No new EXE
or integrated onboard firmware built. Existing baseline QMK build remains valid.

USB review: usb_endpoint_in_send has while(true) retry/reset even with
TIME_IMMEDIATE. Must implement a bounded send/owned latest-state path, not just
change timeout. Existing descriptor override mutates configuration and depends
on persisted game_controller_mode; new runtime mode needs repeatable descriptor
selection, no ordinary HID joystick outside HallJoy, and driver enumeration
validation. USB reconnect is now authorized, but not implemented here.

Recovery preparation:
- Read-only A9/42 saved calibration for all 100 physical keys of the connected
  K4; matching coordinates/status verified. File:
  .local/backups/keychron-protocol-20260921/connected-k4-calibration-before.json
  SHA256 048f7d18e4c1ad5fd8596bcdfa59fecf3eda0c6479e96f41cc4a07e7114753f7.
  QMK serial 45004B00145135303534363300000000. Not a full EEPROM backup.
- tools/keychron_dfu_backup.py is a read-only double-upload helper. Requires
  explicit confirmed ROM DFU serial and one STM32 device, validates 256 KiB,
  identical reads, Cortex-M vectors and SHA256, refuses existing output files.
  It has no flash-write, erase or unprotect command. Syntax/help checked; actual
  hardware upload still pending. ROM DFU serial differs from QMK serial.
- dfu-util 0.11 available at C:/msys64/ucrt64/bin/dfu-util.exe; --list finds no
  DFU device yet. Recovered source lacks VIA id_bootloader_jump handler and
  factory JumpToBootloader implementation is commented out. No confirmed remote
  transition for the installed old custom firmware; do not guess vendor writes.
- Need owner to physically enter DFU to acquire the exact installed flash image
  and verify recovery before first flash. Official HE-family instructions:
  https://www.keychron.com/pages/firmware-and-json-files-of-the-keychron-he-series-keyboards
  Cable mode, unplug, hold PCB reset under Space while reconnecting cable.
  Next: inspect DFU serial/driver, read image twice with helper; do not flash the
  unintegrated baseline as the promised onboard implementation. Continue USB,
  host integration and scanner work after exact recovery backup is available.

No support status, release artifact, driver installation or keyboard firmware
has changed. Legacy source/image archive is preserved. Before this step's edits,
three tracked working files were backed up to
.local/backups/keychron-protocol-20260921/session-core-before.zip.

> 2026-09-21 Owner now explicitly authorizes autonomous implementation, hardware diagnostics and flashing the connected K4 HE. Earlier no-flashing-authorization notes are superseded. Preserve backups/recovery and validate before flashing; stop only for necessary owner questions/actions. No flash has yet been performed.

## Confirmed lifecycle and telemetry requirements (2026-09-21)

Owner explicitly approved onboard architecture with two constraints: no deliberate
low-FPS analog UI, and gamepad mode active only while HallJoy runs. Saved profiles
must not imply automatic activation on boot or when HallJoy is absent.

Implementation contract (pending implementation):
- Boot, USB reset/disconnect and host-session loss default to inactive gamepad
  processing and ordinary keyboard behavior. Persist profile data only, not the
  active lease. A new host session explicitly negotiates and enables its profile.
- HallJoy sends STOP on normal shutdown and disabling output. Firmware owns an
  independent expiring lease for crashes/forced exit/hangs; refreshes require the
  matching current session and advancing control sequence. Late heartbeat cannot
  revive an expired session. Activation requires a new handshake. Heartbeat is
  independent of UI rendering but tied to a functioning owner control worker.
- Lease interval/timeout will be selected and tested for scheduling tolerance;
  loss detection cannot be literally instantaneous. The lease must not be tied
  to key changes, telemetry requests or foreground window visibility.
- Disable clears pending non-neutral reports and transformation state, restores
  normal bound-key handling, and schedules neutral output without blocking sensor
  work. USB failure cannot hold the scanner waiting to deliver a neutral packet.
  Re-enable must not resurrect queued reports from the previous session.
- One authoritative output route: no duplicate K4 ViGEm gamepad during native
  mode. Legacy devices keep their current route.

Telemetry architecture:
- Sensor acquisition and gamepad transformation/output never await the UI, host
  telemetry consumption or profile-upload completion.
- Publish coherent latest snapshots through bounded buffers. A slow reader skips
  old snapshots; no growing FIFO and no scan-path blocking telemetry send.
- Host keeps a latest snapshot and redraws at normal UI cadence when visible.
  Hidden/minimized analog views stop unnecessary telemetry/rendering work while
  gamepad output and the session lease continue. No artificial FPS degradation.
- Bound telemetry copying/USB work and give gamepad output scheduling priority.
  Measure that priority under active plotting and profile uploads; separate USB
  endpoints alone do not prove absence of shared CPU/bus contention.

Open technical distinction: existing XInput USB descriptors are static when
XINPUT_ENABLE is compiled. Disabling output does not itself remove the controller
from Windows/game device lists. Verify a genuine inactive/disconnected controller
state against the driver before claiming the gamepad disappears with HallJoy.
Do not introduce whole-keyboard USB re-enumeration (which can interrupt typing)
as an assumed solution without first establishing necessity and behavior.

No runtime change or flash in this requirements update; hardware behavior remains
unverified. Existing full source/firmware backups remain intact.

## Latest direction: onboard processing and native XInput (2026-09-21)

Owner proposes keeping HallJoy as editor/analog monitor while K4 applies bindings
and curves and emits its own gamepad reports. Adopt this as the architecture to
prepare; it supersedes the earlier pending native-XInput/bulk-endpoint question.
The keyboard keeps native XInput. Full-matrix telemetry is outside the game-input
critical path, so a new 1000-Hz full-matrix USB transport is no longer required.
The experimental HJK4 frame can serve telemetry; it is not yet an active route.

Source inspection confirms native host_xinput_send exists. The recovered dirty
onboard prototype is incomplete: engine omits button mask output, uses approximate
axis policies, evaluates powf repeatedly, and does not establish complete bound-key
suppression. Reuse only after comparison with current HallJoy semantics. Existing
USB send_report can wait 100 ms; new output and telemetry must not stall scanning.

Required implementation: exact current curve/binding/Snap/LKP behavior with
comparison tests; atomic validated profile updates; safe neutral transitions;
firmware bound-key suppression; nonblocking latest-state gamepad delivery;
lower-priority telemetry; explicit host ownership avoiding duplicate ViGEm output.
Preserve legacy UAP/ViGEm for old firmware and other keyboards. Cross-device input
cannot become self-contained K4 processing without a host path.

Current Windows MI02 has problem code 28, so native gamepad driver readiness is
unproven and must be resolved before claiming working native XInput. Moving the
transform onboard removes host analog polling and transformation from gameplay;
it does not eliminate sensor acquisition, noise constraints or USB scheduling.
The 4.999 ms legacy measurement is a full-matrix transaction, not a measured total
latency reduction. No zero-latency or 1000 fresh scans/s promise. No flash performed.

# K4 HE low-latency protocol work - 2026-09-21

## Owner request and current state

Owner has a connected K4 HE, previously adapted to UAP by this agent. Requests
coordinated firmware/HallJoy protocol, earliest movement response, real fresh
1000 Hz if possible without degrading other behavior; wants actual limits.
Explicitly preserve old UAP firmware backups and legacy Keychron compatibility.
No flashing authorized/performed in this preparation. Do not promise perfection.

Present device: Keychron K4 HE ANSI, 3434:0E40, vendor HID FF60/61, bcdDevice
0x0111. Read-only A9/01 returned protocol version 4 and FAR marker 0x45.
A separate MI_02 interface reports Windows problem code 28 (missing driver);
no driver change performed; vendor HID works. This does not explain the measured
FAR timing or establish failure of the existing HallJoy route.

Read-only bounded test: 160 A9/31 requests, 4x32-byte responses each, 640 valid
reports, zero malformed/timeouts. No key identities/depths retained. First reply:
min 1.756, median 1.991, p95 3.014, max 4.328 ms. Complete matrix: min 4.772,
median 4.999, p95 6.039, max 7.291 ms. Python/HID request-to-response timings,
not sensor latency, scan frequency or guaranteed bound. Device settings unchanged.

## Recovered history and backups

Older design record: docs/research/KEYCHRON_K4_HE_ONBOARD_HALLJOY_DESIGN_2026-08-30.md.
Source repo W:/github/keychron-qmk-k4he-onboard, HEAD ee7390c3bbdc1f71a1cc8d54323f3f1d97868593,
branch halljoy-onboard-k4he-a931. Contains dirty/staged work and inactive onboard
prototype. Do not overwrite it or activate the onboard engine for this task.
Its reconstructed A9/31 sends travel/TRAVEL_SCALE while current upstream FAR
sends travel directly; do NOT call this source byte-identical to flashed image.
Matching version/marker alone is insufficient to prove its normalization.

Backup: .local/backups/keychron-protocol-20260921/legacy-keychron-sources-and-firmware.zip
Manifest: same directory/manifest.json. 118 files, 11,541,423-byte ZIP; every
archived entry verified against SHA256, zip integrity PASS. Includes current
HallJoy UAP source, recovered K4 common/model source and working diff, old K4
images, and eight locally available AnalogSense Keychron binaries. Not a claim
that prebuilt firmware for every supported Keychron model exists in this backup.

Recovered known UAP candidate:
W:/github/HallJoy_v1.4/build/firmware/keychron-k4he-ansi-full-report-20260807/keychron_k4_he_ansi_halljoy_full_report.bin
SHA256 fcebebd31e5d54a72d3c7c23619878f983606fe390584f2d9317693f40e31aa4.
Stock v1.1.1 SHA256 4e877497a0edc1a0d97cd52f5ff9ba86ef7dc84d56969e81db7de19eb6151e5f.
Historical full dump SHA256 bf53c24706d761eddf5af447549627020f1410b5daf431438fbcab1ff63ab0a0.
The dump filename identifies an earlier device; do not restore device-specific
calibration/identity blindly onto the currently connected unit.
An obsolete image is archived for evidence only and retains DO_NOT_FLASH name.

Clean isolated base worktree:
.local/research/k4-lowlatency at ee7390c3bb, not the dirty onboard tree.
Submodules initialized at pinned commits: ChibiOS 41e112ce, contrib 58047eb6,
printf c2e3b4e1. Existing MSYS2 and ARM GCC 13.4.0 / QMK CLI 1.2.0 found.
Baseline build log .local/k4-baseline-build.log. Check completion before any claim.

## Concrete source-derived limit (not a measurement of flashed firmware)

K4 has 6 rows x 19 columns, 100 physical keys. Final GCC preprocessor check
corrected the initial estimate: k4_he/halconf.h overrides the generic ADC /4
prescaler AFTER including mcuconf.h, selecting /8. Actual compiled source:
- HCLK and APB2 72 MHz, ADC prescaler /8 -> ADC clock 9 MHz.
- 56 sampling cycles plus 12 conversion cycles at 12-bit resolution.
- Six serial conversions per column; wait_us(40) before each column.
Thus unchanged sequential scan requires at least
19*40 us + 114*(56+12)/9 MHz = 1621.33 us, before shifter/CPU/RTOS work.
Ceiling from these two terms alone: about 616.78 scans/s. Initial 1190.67 us /
839.87 scans/s used the generic /4 setting and is superseded. Final preprocessing
of analog_matrix_scan.c confirms STM32_ADCCLK=(STM32_PCLK2/8).
Extra conversion attempts on state changes and other work lower it further.
ST primary explanation: https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Inc/stm32f4xx_hal_adc.h
This is NOT a fundamental sensor/MCU hardware maximum. ADC timing, settling,
parallel scheduling and actual installed binary must be validated separately.
1000 host reports/s with repeated old samples are NOT 1000 fresh scans/s.

## New protocol requirements (design, not implemented)

- Preserve A9/01 legacy marker and exact A9/31 old ABI; no global replacement
  of old Keychron paths or threshold changes for existing devices.
- Negotiate explicit magic/version, model, matrix, units, transport limits and
  capabilities. Stock echo or unknown versions never activate new mode.
- Avoid collisions with inert onboard A9/60..65 prototype; allocate versioned
  new commands only after checking the exact current firmware command space.
- Fresh scan sequence and acquisition timestamp are distinct from host packet
  arrival time. Never invent fresh measurements to fill a 1000 Hz schedule.
- Snapshot/fragment indices, session token, bounded lengths, explicit units and
  rejection of incomplete, duplicate, old-session or mixed-generation frames.
- No scan-path blocking send, unbounded FIFO, EEPROM writes per sample or
  calibration changes. Keep latest complete snapshot and report overruns.
- Bounded host waits/cancellation and stream lease/stop behavior; old firmware
  stays on old FAR automatically. Invalid new packets must not be reinterpreted
  as valid legacy input.
- Preserve typing, rapid trigger, wireless, RGB, profiles and recovery path;
  qualify changes before claiming no regression. No auto-flash.

Owner clarified: replace the old scanner and analog transport in HIS K4 with
a new implementation; no FAR compatibility requirement inside the new keyboard
firmware. HallJoy MUST retain old UAP for everyone else and select the new path
only for the new firmware. Backups provide rollback. Scanner redesign is now
explicitly in scope; do not ask that question again. Accuracy/noise and other
features still require validation, not an ideal-performance promise.

Next real tradeoff under review: STM32F401 exposes three non-control USB endpoint
numbers; current firmware uses XInput, vendor RAW HID and shared keyboard HID.
A full-rate independent bulk transport may need to reuse native-XInput
endpoints while retaining vendor config and normal keyboard. Asked owner whether
native standalone keyboard gamepad is required, or only HallJoy virtual output.
Awaiting answer; do not silently sacrifice native XInput.

Baseline Windows QMK build needs a host-only keyboard-list path separator fix
in lib/python/qmk/keyboard.py (_find_name emits POSIX slash for make). Applied
only in isolated source tree. Current build session/log must be checked; no
firmware compilation success claimed yet.

Public support remains unchanged; no Sheet/README support promotion is warranted.
No new firmware or new HallJoy protocol has been delivered at this checkpoint.

## New protocol core prepared (not an active device route)

src/HallJoyProject/HallJoy/keychron_hj_protocol.h is a transport-independent C11
wire core shared with host C++: 256-byte frame, HJK4 magic, version/type/length,
negotiated session, acquisition sequence/time/duration, 114 x 16-bit calibrated
values, explicit calibration/overrun flags, CRC32. Optional raw type is never
accepted as controller depth. No old-UAP or runtime code changed.

Independent tests: standard CRC32 vector, all 2048 single-bit corruptions,
256 truncated lengths, wrong session, duplicate/out-of-order/wrap sequences,
skipped-scan accounting, uncalibrated/raw rejection and full endpoint values.
C++ execution PASS, Cortex-M4 C11 compile with warnings-as-errors PASS.
The 16-bit field is transport precision, NOT proof of 16-bit sensor accuracy.
Firmware encoder must later prove calibrated mapping and scan origin.

## Baseline build completion

ARM GCC 13.4.0 / QMK CLI 1.2.0 build PASS for keychron/k4_he/ansi:keychron.
BIN size 104752 bytes; ELF/BIN/HEX hashes recorded in
.local/backups/keychron-protocol-20260921/baseline-build.json.
LUFA submodule 549b9732 was also required and initialized. Device source is the
clean pinned baseline; only Windows keyboard-list slash handling was adjusted.
This build is NOT byte-identical to the recovered UAP image and is NOT a new
protocol delivery. No device flash and no user-setting changes performed.
Pending native-XInput requirement still determines the new USB transport.
