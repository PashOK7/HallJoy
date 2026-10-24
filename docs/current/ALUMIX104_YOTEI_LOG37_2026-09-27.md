# Red Square Alumix 104 Yotei — tester log 37

2026-09-27. Owner supplied the model name and HallJoy (37).log.

- Log is HallJoy 1.6.4.0, schema1. Completed active search records analogue_connected=0, devices=0. UAP available/ready, error=0; no UAP restart or invalid snapshot reported.
- HID inventory includes 0C45:80AC interfaces00–03. This is the target candidate, not a proved retail identity: the legacy inventory has no product strings. Other inventory entries include Logitech/Razer devices and later045E:028E.
- Current src/tools search finds no exact80AC or Alumix/Yotei runtime implementation. Existing research only establishes the retail models; it does not establish their analog protocol.
- shark eligible=0/vendor_collections=0 describes that backend only. It does not prove a broken keyboard or absent firmware analog.
- Legacy analyzer explicitly reports incomplete provider coverage. Do not diagnose configurator contention or protocol impossibility from this log.

Next evidence needed: official exact-model configurator/firmware, mapping0C45:80AC to the model, then review live depth commands and normal typing/cleanup behavior. Initial web searches did not locate an authoritative exact-model driver. No commands sent to hardware, runtime edits, build, support promotion, Sheet edits or publication.

Log SHA256: 399ea7f4bd76bfff07d6a7e97b7558eb4e3b1158ad5edf84deb43ca4dbc55405

## Exact official configurator found — 2026-09-27

Owner requests implementation. Old conf.red-square.org redirects users through an app link; the newer https://app.red-square.org/ contains the exact target.
Private input bundle: `.local/alumix104-review/`, source-lock.json pins URLs/bytes/SHA256. Vendor sources remain local-only.

- New layout-classic-CQddK7Ye.js declares vendorId3141/productId32940, productName `Alumix 104 Yotei Magnetic`, supplier rsq. This establishes exact model correlation with log0C45:80AC.
- SAME PID also declares `Alumix TKL Horizon`. Runtime must match product string, not globally admit80AC or infer a104 layout from PID alone.
- Extracted104 unique physical indices0..120, HID mappings including modifiers and numpad. Fn is vendor175 and must become HallJoy logicalFn, not ordinary HID175. Extraction saved in keyboard-map.json. No guessed60% map.
- Official protocol is HFD: AA command header, reportID0, payload size from descriptor; USB payload may vary, do not assume65-byte reports without descriptor evidence. Header byte2 length, LE16 offset at3, final-chunk flag6, data offset8.
- 0x10 reads48 bytes device info; contains descriptor VID/PID, manufacturer/product, version, rtPrecision, frameVersion. 0x12 reads mappings. No full deviceInit replay: it reads unrelated macros/settings.
- `ec` startSimulationTest sends0x66, waits for ACK, attaches live listener. `tc` stop sends0x67. `ji` decodes55FB: key index2, status3, high LE16@4, low LE16@6 masked7fff, ADC@8, travel@10, maxStroke@12. Browser parser incorrectly permits13 bytes while reading14; native parser must require14 or exact report size.
- Official UI rt callback displays travel/100 mm and maxStroke/10 mm. Thus normalized travel/(maxStroke*10), with validated bounds, is indicated by producer code. Actual resolution/filtering/shallow cutoff and stream cadence on104 remain UNKNOWN.
- Alternative0x68 getMagneticAxisStatus queries8-byte records, but UI uses it inside calibration flow. Do not substitute it as a clean independent stream without firmware evidence. Calibration0x64/65/69/6a, flash/update/reset must not be sent for this investigation.
- Current app update.json contains ONLY32930/RSQ-20058 v1.30 (Alumix68). Downloaded its HEX for family comparison; it is NOT target104 firmware and proves no104 behavior. No updater executed or hardware commands sent.

### Pending evidence before native activation

Requested tester check using official Performance travel test (NOT calibration): multiple simultaneous depths plus ordinary Notepad typing, and new1.6.5 log/configurator detection. Exact104 firmware is not offered in the inspected update catalog. Unknown typing coexistence, streaming/release and clean stop must remain explicit; do not repeat MAD68 mistaken unrestricted claim.

No runtime support/status changes yet. No build/publication/Sheet mutation. Next: use tester product string/report descriptors and behavior evidence to select exact model, then integrate sharedHFD transport with104 map, per-key freshness, guaranteed0x67 cleanup onPause/exit, malformed-frame/identity/lifecycle tests and support-status synchronization. Existing MINI60 provider has fixed61-key map and manufacturer restriction; merely adding PID would be incorrect.

## Neighbor firmware execution authorized and completed — 2026-09-27

Owner explicitly requests using neighboring68 firmware; preceding suggestion to wait for tester is no longer the prerequisite for offline investigation. This does not authorize flashing68 into104 and no flashing occurred.

Tool: tools/review_redsquare_alumix68_firmware.py. Strict shared Thumb core; verified IntelHEX checksums and nonoverlapping records. Decoded image SHA256020778ba842603c49fe21365ad10380dd877f9be29ecaeefb9760eeab69461c1. Private results replay68.json and alternative-commands68.json under .local/alumix104-review/.

PASS is reproduction, NOT suitability.544 synthetic post-processing tail cases, full dispatcher66/67/66, disabled tail, ordinary-processing gate, actual USB scheduler with modeled busy/success.

Established for RSQ-20058 v1.30 ONLY:
- Dispatcher E40C sets RAM20000477 with66 and clears it with67; calibration20000476 remains zero. These paths only alter transient state/ACK buffer, no flash calls.
- Reporting tail4E1C..4F26 in simulation mode emits only selected index20000117. A different index produces no pending packet. Static4C80..4C98 selects a key whose processed depth is >= current selected depth. This is not an independent multi-key stream.
- Packet builder1284 builds64-byte55FB at200042E4, travelLE16@10, stroke34@12.544 selected-key cases cover68 synthetic indices and depths0/1/9/10/11/93/170/340. This establishes formatting, NOT sensor resolution or all normalization behavior.
- Broader nonselected reporting4ED8..4F10 requires calibration flag. It is not an acceptable workaround; do not turn calibration on.
- Normal-processing gate4F26..4F40 reaches ordinary continuation with simulation enabled/calibration off. Full ordinary HID transmission remains unexecuted; do not claim physical typing proved.
- USB scheduler133C0 uses one pending buffer. Modeled busy return preserves pending; success clears it and marks emitted key. Does not establish actual scan/USB frequency or fairness.
- Read candidates60/68 and newer calibration69/6A with zero payload produce header echoes only, no data in this firmware. Static dispatcher6x branch only implements64..67. Do not mistake echoed ACK for analog source.

Result: best identified noncalibration analog path on neighboring68 is LIMITED selected-key simulation66/67. Unsuitable for yellow/full multidirectional analog expectation without further evidence.104 still has exact official host protocol/map evidence, but its firmware may differ. No firmware equivalence claim, runtime activation, support promotion or Sheet change. Original implementation task remains pending; this finding must be disclosed before any limited tester build.

## Broader path search: independent RAM depth reads found — 2026-09-27

This supersedes the previous conclusion that selected-key66 is the best identified path on68. Tool tools/review_redsquare_alumix68_paths.py; private path-sweep68-complete.json PASS, pinned by path-search-sources.json. No device commands.

-1536 dispatcher scenarios:256 opcodes x3 length/offset shapes x2 simulation states.1398 returned; others stopped at explicit flash/zero-fill/calibration/assignment side-effect boundaries, NOT falsely modeled as successful. Zero-filled RAM and sampled payloads are NOT exhaustive all-state/all-payload coverage.
-0x16 Fn-map read adds unsigned16 offset to20002A9C without restricting it to Fn-map length. Offset0200 reads depth table20002C9C,128 LE16 scan slots.40 real-dispatcher transactions retrieved8 frames/1024 changing synthetic values; modes stayed off.
-Two distinct nonzero key depths and a zero release were transported in one response through actual133C0 USB scheduler with only low-level submission stubbed. No native HID timing/physical transmission claim.
-Full clean-read RAM diff found only request/reply scratch, stack and ACK-pending change. Dispatcher has a common live-lighting cleanup if200000CE is set; zero-state result must NOT be generalized to no effects in all configurator states. No flash writes on identified read route.
-Depth table is prior to temporal filter46EC, whose separate smoothed destination is20002F9C. Earlier nonlinear conversion and ADC quantization/filtering still apply; this does not create unfiltered sensor resolution.
-Scan slot is NOT physical key index. Immutable table159D2 maps128 slots to68 unique keys with125 sentinel, independently checked.0x12 base9600 plus offsetC3D2 reads exact table through firmware.0x10 is unsuitable for byte-perfect code reads because E534 zeroes reply byte1A; regression caught this and switched to0x12.
-0x12 large offsets also provide a possible read-only code-window acquisition route for104. Its flash/RAM bases and mapping cannot be assumed identical. Use verified code fingerprint/identity before accepting raw depth offsets; never adapt using arbitrary RAM values as analog proof.

Candidate inventory:
1.16 read-only RAM depth snapshots: BEST FOUND for exact68, independent multiple keys, no mode/calibration/persistent settings enable. Not full-scan atomic across five reports; real polling cadence unknown.
2.66/67 simulation: selected-key limitation confirmed, inferior to1.
3.64/65 calibration: deliberately rejected, changes calibration and suppresses ordinary processing.
4.60/68/69/6A: no useful handler in inspected68 dispatcher; echoed request is not data.
5.21..28 and reset aliases endingF: settings/flash/reset paths, rejected.
6.USB feature/bootloader or other firmware images: not exhaustively executed. Linear candidate scan found AA checkE424;55 comparison53AE is key processing, not another vendor dispatcher. This is not a proof all possible transports are exhausted.

No runtime/support/Sheet/publication changes. Original104 integration remains open. Important progress: neighboring firmware DOES expose a better path than the selected-key simulation;104 requires exact layout/address fingerprint, not physical tester permission as a prerequisite to further research.

##104 code-window acquisition build delivered locally — 2026-09-27

Owner authorized all necessary work. Added explicit CLI-only `--halljoy-redsquare-code-probe <new-output-file>` to ordinary HallJoy and decoder tools/read_redsquare_code_capture.py. Ordinary startup does not probe or activate104.

Probe admission:0C45:80AC AND exact official product string (Alumix104 Yotei Magnetic or Alumix TKL Horizon), uniqueFF68:61 collection, matching33/65-byte input/output. Only command12 reads offsets2A00..F9FF. Reference68 interpretation flashC000..18FFF;104 mapping remains unverified. No RAM sensor reads, mode/calibration/flash/settings commands.30s overall deadline plus bounded per-I/O waits; strict header/command/length/offset response matching; first failed transfer stops, partial report retained. CREATE_NEW avoids overwriting prior captures. Exact string/descriptor mismatches fail closed; need report if actual product differs. Export private/local-only, not for GitHub.

Files: src/HallJoyProject/HallJoy/redsquare_code_probe.h, main.cpp command dispatch; build_release.ps1 now includes probe self-test in subsequent standard builds. Existing active process lifecycle maintained by standard publisher. New decoder verifies schema/identity/contiguity/length/count/completion; does not award compatibility based on similarity.

Validation: standard Release diagnostic contract, six existing executable gates and embedded resources PASS (.local/alumix-probe-build.log); additional candidate probe self-test PASS. Decoder valid synthetic68 window and five malformed/incomplete cases PASS. ZIP readback/executable byte equality PASS. No physical104 run/analog-support claim.

Installed EXE SHA2569b1e69415e70d03044529288e84424d236a50d1451e5f77654bb91ede861950b.
Tester package `.local/alumix104-review/HallJoy-Alumix104-research.zip` includes same EXE, Start-Alumix-research.cmd, short instructions and licenses. This is packaging only, not a separate compilation directory. User must extract, close HallJoy/web driver, run CMD and return generated research TXT. Necessary external step: target104 is not connected here. Continue by validating export and locating actual firmware structures, not by blindly enabling68 offsets. No support-status/Sheet/publication changes.

## Owner correction — standard HallJoy workflow only

2026-09-27: owner rejects archive/CMD/separate research TXT delivery. Supersedes
the preceding tester handoff instructions. Do NOT send the prepared research ZIP
as the next solution. One HallJoy.exe; bounded device investigation inside the
normal application; result in HallJoy.log accessible through Open log. Local
research tools/exports remain private agent artifacts only.

Implementation pending: replace explicit CLI/export route with normal background
lifecycle plus shared log evidence, preserving privacy, size limits, cancellation
and existing logging policy. The current9b1e6941 executable does NOT yet implement
this corrected workflow. No new build or support-status change in this update.


## Standard HallJoy executable and single-log implementation — 2026-09-27

Supersedes the rejected ZIP/CMD instructions above. The ordinary executable now
starts one bounded background attempt after engine admission (3s deferred start).
Exact PID/product/collection guards remain. No calibration/simulation/settings
write commands, no guessed analog RAM offsets, no standalone export route.
The neighboring68 read-only code window is a research hypothesis for104, not
proof of104 memory layout or absence of every firmware-specific side effect.

Only command12 is issued; offset2A00..F9FF,33/65-byte reports, strict reply fields,
30s acquisition deadline,400ms I/O waits and at most200 records/s. Error records
identify Windows I/O failures, short reports, rejected identity/collection and
incomplete captures. Pause/Exit cancels and joins the worker before native-provider
release; a join timeout is a lifecycle failure rather than a false successful pause.
Read metadata/attempt once per process; no continuing background polling on other
keyboards. A new run is required after an interrupted/failed attempt.

All evidence goes through the ordinary support writer and existing Open log.
Up to2300 reviewed code-window records survive512-line history eviction and
snapshot resets, within the existing4MiB bound. No separate TXT/BIN/ZIP, settings
changes or forced continuous logging. Instability/limited warning policies remain.
Code capture is private support evidence, not a distributable firmware artifact.

Validation: real Windows writer regression passed both ordinary/input-path variants,
including750 records, history eviction, snapshot reset, duplicate prevention and
invalid-control rejection. Normal-log decoder passed both report geometries,
10 malformed/incomplete cases and latest-capture selection. Self-test covers request
bounds/response mismatch and immediate background cancellation without hardware.
Final standard Release build result/hash recorded below after publication locally.
No physical104 test, no analog support/status promotion, no Sheet or GitHub change.

Final ordinary Release PASS: diagnostic contract, seven executable gates and
embedded license checks; standard publisher installed identical candidate bytes.
Build log: `.local/alumix-standard-log-final-build.log`.
EXE: `build/bin/Release/x64/HallJoy.exe`; SHA256 `8930d6bc9a07d1dc15727b3f81759c2792778959c37d14ff5fc93d4f0a8aecba`.
Tester: close configurator/older HallJoy, run this EXE normally, leave active about
35 seconds, send HallJoy.log using Open log. This is acquisition time, not a
normal keyboard-detection delay. No scripts/flags/additional files.


## Actual Alumix104 capture: HallJoy (38).log — 2026-09-28

This log is ALUMIX104, not the MAD68 tester. Exact allowlisted product is
Alumix 104 Yotei Magnetic,0C45:80AC,65-byte report collection; matched=1.
MAD68 USB28E9:3265 is absent; its not_present row is expected catalog telemetry.
Log SHA25606d3107a74990be24f213d54131f82fbbe6c9af5d84c9949ac2ec8f277804386.

Shared-log acquisition completed951 blocks in9531ms. Strict decoder verifies
contiguous command-relative2A00..F9FF,53248 bytes, no missing/duplicate blocks.
Private extracted artifact: .local/alumix104-review/log38-code-window.bin
SHA2560e3daac352b39edfd2def4dc54ed4eba74f016121df63ec854ae2e8e5e8d3c5e.
36987 bytes nonzero. Only2180 bytes equal the same-position68 reference slice;
NOT an identical firmware. No conclusion about104 flash base from this comparison.
This is a partial code/data window, not a full firmware download or analog proof.

Normal diagnostic workflow succeeded: one EXE, one ordinary log, no tester script.
Runtime source remains none_reported because no104 analog backend is implemented.
Next: locate104 read handlers, establish bases/depth/index mapping from exact
captured code and compare/emulate against68 evidence before runtime integration.
Keep vendor capture local-only; no support status or Sheet changes yet.
MAD68 no-analog report from the other tester remains unresolved independently.


## Exact104 partial-code analysis — 2026-09-28

New tool tools/review_redsquare_alumix104_capture.py pins capture0e3daac3 and
executes exact Thumb bytes C000..18FFF only. Entry E388, request200065B4,
reply20006574, pending20000300/200002FF. Results private exact104-replay.json.
27 address-copy cases plus951 captured code blocks verified;512 opcode/RGB-state
scenarios cataloged (232 normal returns,2 read postprocessing boundaries,
278 faults at unavailable code/data boundaries). These are classified outcomes,
NOT512 proven usable command paths. Missing lower flash code is not invented.
Synthetic config9000..BFFF and zero RAM are explicit; no USB/ADC/typing model.
Commands10/11 tests stop atE4F0 before postprocessing, not full command success.

Critical exact104 difference: command16 selects FLASH B000 (E3EC/E4CC), unlike
neighbor68 RAM mapping. Bases:10=9000,11=9200,12=9600,14=9A00,15=9C00,
16=B000,17=B600,18=B200,1C=BC00. Offset is unsigned16 addition, no signed wrap.
The capture is consistent with command12 base9600 and mapped flash C000..18FFF.
These established reads cannot reach0000..8FFF or sensor RAM; reading more
through the same mechanism would mostly add configuration/upper flash, not the
missing scanner/producer code. Do not send another pointless diagnostic build.

Additional restrictions discovered: command10 has special postprocessing that
can call flash writer3BF8 for9003 when stored value is12 (E4F4..E50A), and reads
unmapped chip register400180; it is NOT a generally side-effect-free dump path.
Any command with active live-RGB flag200000AC may clear it and call391C before
dispatch (E3A4..E3C0). Our real successful code capture is not proof of all-state
side-effect freedom. Command12 under zero-RGB state writes request/reply/stack
and pending only in these replays.

Simulation66/67 toggle200002FE; calibration64/65 are excluded. Producer and typing
coexistence remain unreviewed because lower code is missing. Official newer
GET_MAGNETIC_AXIS_STATUS=68 and69/6A merely echo the request in this captured
dispatcher; no live ADC/depth read performed. B0/B1 also do not implement readback
in this dispatcher. Another transport/bootloader is not exhaustively ruled out.

Acquisition checks: fresh https://app.red-square.org/update.json still contains
only RSQ-20058 v1.30 (68). Scanned433 local BIN files and34 decoded HEX files for
multiple exact64-byte capture anchors: no other matching image. Acquired official
sn_isp_lib_bg-DOswDDJh.wasm privately and recorded URL/hash in source-lock.json;
visible updater interface exposes programming/checksum/security-error paths,
not an established full readback method. No bootloader entry, erase/program,
calibration, unsafe-length experiment or hardware write performed.

Next external evidence needed: exact original104 firmware file from Red Square
(or a documented readback/live-depth interface). Current fragment is enough to
reject blind68 integration, not enough to establish simulation typing behavior
or a clean analog transport. No new EXE or status promotion; existing complete
ordinary EXE remains ec88ecd8 with MAD68 and bounded capture unchanged.


## Existing-capture experiments continued — 2026-09-28

Owner explicitly directs continued experimentation with available evidence; an
unavailable full firmware is NOT a reason to stop research. This supersedes the
previous "next external evidence needed" work-blocking conclusion, not its
technical unknowns. No invented lower-code execution or blind calibration.

Added tools/review_redsquare_alumix104_experiments.py, exact capture hash locked,
strict shared Thumb executor, synthetic RAM seeds disclosed. Twenty same-machine
stateful scenarios execute140 commands: OFF/OFF/ON/ON/OFF/ON/OFF, starting both
on/off, with calibration flag0/1, five RAM seeds. All commands return; simulation
flag changes as intended; calibration flag unchanged. Only reply/pending/flag/
stack writes admitted. These results cover dispatcher idempotence and RAM state,
NOT scanner behavior, USB delivery, typing coexistence or crash recovery.

280 legal command68 payload variants (1..7 eight-byte records, four offsets,
five RAM seeds, simulation both off/on) return EXACT request echo except55 header.
This closes the hypothesis that starting simulation makes this query return depth
within this dispatcher. ACK and payload length alone must never count as analog.
Private result: .local/alumix104-review/exact104-stateful-experiments.json.

Official layout-classic-CQddK7Ye.js SHA256
9c98d748e103aba32f808a7db1a748741194de4246c64018f78e9eb5e8f59cac:
ec/startSimulationTest uses66 and listener Ks -> ji; stop uses67. ji accepts55 FB
and decodes key index byte2, calibration status3, maximum LE16[4], minimum LE16[6]
masked7FFF, current LE16[8], stroke LE16[10], maximum stroke LE16[12]. Vendor
length check >=13 is incorrect for reading byte13: HallJoy must require >=14.
This is a configurator wire-format hypothesis for104, NOT a captured104 stream.
Packet carries one key; that does not establish whether all keys are multiplexed
or only one selected key is sent. Neighbor68 chooses greatest depth;104 unknown.

Next candidate experiment: bounded official66/67 stream lease, mandatory OFF on
normal exit/cancel/error, structured aggregate evidence in ordinary HallJoy.log.
Before hardware handoff disclose UNKNOWN typing, multikey, range, shallow/release
behavior. Do not ship guessed full analog mapping, force logs due to limited
status, or send calibration/settings/flash commands. Live-RGB cleanup391C remains
an unexecuted lower-code boundary. No claim of absolute side-effect freedom.
No new app EXE in this step; installed completeec88ecd8 remains unchanged.
No hardware commands, support promotion, Google Sheet edits or publication.


## Restrictions re-reviewed and official stream diagnostic — 2026-09-28

Owner challenged the earlier limits as potentially premature. This review separates
proved behavior of the captured dispatcher from properties of the missing scanner
and USB producer. No full firmware is required before testing the known official
stream. This is a diagnostic trial, not native analog admission or a Supported
conclusion.

- Exact104 command16 copies from flash B000 plus an unsigned16 offset
  (E3EC/E4CC/E426..E43E), so the neighboring68 RAM-depth read does not transfer.
  This excludes that particular read route; it says nothing about the official
  asynchronous sensor stream or other uninspected transports.
- Commands10/11 are not safe generic dump aliases. In the captured code, command10
  checks byte9003 for12 and can call flash writer3BF8 (E4F4..E50A). The side
  effect is conditional, not evidence that every read command is destructive.
  Command12 was observed to work for the exact code capture; RGB-active common
  cleanup branches to unavailable391C, so zero-state replay cannot establish
  all-state side-effect freedom.
- Command13 branches to unavailable7C30. Its exact runtime result is unknown;
  the official client labels13 GET_LED_EFFECT, not a live-depth API. Execution
  faults at missing code are not negative responses from real firmware.
- The expanded exact104 dispatcher replay covers20 same-RAM lease scenarios /
  140 mode transitions and280 payload/offset/state cases EACH for60,68,69,6A.
  All 1120 candidate cases return request echoes from the captured dispatcher.
  This rules out a synchronous depth reply in those tested paths, not an
  independent asynchronous producer.69/6A are calibration commands in the
  official client and are NOT candidates for hardware probing. Private output:
  `.local/alumix104-review/exact104-candidate-review-v2.json`, SHA256
  67ef534fad23603567181717ff68eab3266617ab1f864658f2db4f64489c78dc.
- Exact104 66/67 alter a transient simulation flag and ACK in captured upper
  code. The official client `ec` sends66, waits for ACK and listens for55FB;
  `tc` sends67. The vendor parser decodes one key index, calibration status,
  extrema, current ADC, travel and maximum stroke. The 13-byte guard is shorter
  than its final two-byte field; the HallJoy parser requires15 Windows bytes,
  including report ID. The selected-key-only result of exact68 is NOT a finding
  about104. Typing coexistence, multiple keys, release quality, low-depth
  response, stream cadence and recovery remain unknown until actual104 evidence.

The review used the pinned official JS SHA256 9c98d748e103aba32f808a7db1a748741194de4246c64018f78e9eb5e8f59cac,
exact code capture0e3daac3 and shared strict replay. A fresh official
`https://app.red-square.org/update.json` request returned only product32930
RSQ-20058 v1.30; no104 image. The updater feed does not determine whether a
vendor-held104 image exists.

### One-EXE stream trial now prepared locally

The ordinary HallJoy build retains the full approved backend catalog and replaces
repeated code dumping with one bounded stream experiment for the exact
`Alumix 104 Yotei Magnetic` product,0C45:80AC,FF68:61,33/65-byte collection.
The shared PID's `Alumix TKL Horizon` cannot enter this trial. The worker starts
3 seconds after runtime admission; Pause/Exit cancels and joins it. It increases
HID input buffering to256 if the driver accepts that request, observes a500ms
passive baseline, sends official67 to clear a stale lease, then66 and waits for
ACK. After at most25 seconds it sends67, with a bounded retry, and logs whether
that OFF was acknowledged. An exception during the lease also attempts OFF.
Forced process termination and disconnected-device cleanup cannot be guaranteed;
the next run begins with67. No calibration, settings, flash, bootloader or
arbitrary RAM command is sent.

The existing HallJoy.log/Open log records one baseline and five-second aggregate
windows: valid55FB frames, distinct physical indices0..120, nonzero/zero and
changed/release counts, coarse shallow (<10% indicated travel) and near-max
(>90%) counts, malformed/out-of-catalog/range anomalies, idle/transport failures,
ACKs, cancellation and final cleanup. No key code, exact travel value, raw packet,
serial or device path is written. A window with two indices is a lead, not proof
of simultaneous depth; the tester's two-key observation and typing report are
still needed. No analog backend is admitted and no input is emitted to a gamepad.
The research record schedules the normal bounded log writer without enabling
continuous logging due to a limited-support banner.

Standard `tools/build_release.ps1` completed: diagnostics contract, full catalog
exact-image check, eight executable gates and embedded resources PASS. Installed
ordinary `build/bin/Release/x64/HallJoy.exe` SHA256
f339e04007791c2455622141f6f08619326d598e0df4ce41d1dbddc6a53d5b31.
Build log `.local/alumix104-stream-release-build.log`. No physical104 run yet.
No support-status conclusion changed, so no Sheet promotion or release publication.

Tester handoff: one HallJoy.exe, close the vendor configurator and any older
HallJoy, launch normally, leave Active for about35 seconds. During the capture,
press/release one key shallowly and fully, hold two keys together, then try
ordinary typing in a text editor. Return the ordinary HallJoy.log through Open
log and report whether typing or the two-key test behaved normally. No scripts,
flags, archives or separate export files.


## Readiness-driven stream trial and Russian window status — 2026-09-28

The owner rejected the arbitrary 25-second capture / 35-second tester wait.
This section supersedes those durations and the prior handoff instructions.
The exact-device official66/67->55FB trial now starts with the engine, with no
fixed experiment duration, ACK deadline, or silent-stream deadline. Individual
overlapped HID operations still use finite 250ms reads / 400ms writes for Pause
and Exit cancellation. Final OFF uses a bounded retry and eight short reads so
engine shutdown can complete. A silence streak is evidence, not a device-failure
conclusion; change-driven streams may legitimately be quiet.

The temporary window title gives Russian prompts: searching; waiting for a key;
slow press/release of one key; hold two keys; check ordinary typing; then press
Pause. The stages advance on parsed evidence, not elapsed time. The one-key gate
requires shallow (<10% reported maximum), near-full (>90%), nonzero depth change
and release for the same index. The two-key gate sees two distinct positive
indices after its prompt. That does not prove their strokes overlapped. Typing
cannot be inferred without collecting text, so the user must check it and report
the result. Pause is the explicit completion action even if a stage never
advances. The title then distinguishes complete evidence, insufficient evidence
and fault, and directs the tester to the ordinary log.

The log retains startup/admission, OFF/ON write outcomes, asynchronous ACKs,
stage transitions, power-of-two idle/nonstream/sample checkpoints, counts,
anomaly flags, aggregate calibration/ADC changes and bounds checks, cleanup
result and explicit missing-evidence fields. If there is
no device, no stream, no ACK, read failure, partial stage or user cancellation,
the available evidence remains in HallJoy.log and survives Open log snapshot
replacement. Report-ID-aware parsing requires at least15 Windows bytes for55FB.
No key text, serial/path, raw HID packet or exact travel value is logged. Neither
ACK nor two indices alone is called usable analog support. The internal EXE
self-test no longer starts a hardware worker; synthetic stream parser and normal
log retention tests cover the new cases. A real104 run is still required.

The normal full-catalog `tools/build_release.ps1` passed diagnostics, writer
regressions, all eight candidate gates including the exact-image catalog gate,
and embedded-resource verification. Candidate and installed EXE SHA256 both
3A9A89671D3FAC9711F76B1E51BE81DC7AA470E1ADFE45242F634C187536FF23.
Build record: `.local/alumix104-readiness-build.log`. No physical104 result,
support-status change, Sheet edit or public release in this task.

New tester handoff: deliver the one ordinary HallJoy.exe. Close the vendor
configurator and older HallJoy, launch normally and follow the Russian title.
Press/release one key shallowly and fully, hold two keys together and check
ordinary typing in a text editor. Press Pause when done, or when any stage seems
stuck; then use Open log and return HallJoy.log plus the typing/two-key
observation. No timed wait, flags, scripts or separate diagnostic files.


## HallJoy (39).log: physical stream found; readiness verdict corrected — 2026-09-28

Private tester log `C:\Users\PC\Downloads\HallJoy (39).log` SHA256
8A19340C5AC57B1D803DF66F3ED81A937B6A02D8FEF4AB6DCC5B2F2216566CAF
records the exact `Alumix 104 Yotei Magnetic` 0C45:80AC with one accepted
FF68:61 65-byte collection. The initial67 and66 writes succeeded, both ACKs
arrived, and 440648 55FB samples arrived during445.406 seconds (about989/s
overall). Seventy physical indices appeared,68 with nonzero travel; there were
34243 depth changes,258 reported releases and2111 other full-length reports.
No malformed or out-of-catalog frames, read errors, dropped research records
or support-log queue loss were observed. The final67 write/ACK succeeded on
manual Pause; log analyzer found16 structurally and telemetry-complete support
snapshots. The generic `source=none_reported` is expected: the stream probe is
research only and does not publish HallJoy/gamepad analog input.

The original title reached `ready_for_typing_check` after38.859 seconds, then
remained there until the tester ended the seven-minute run. The screenshot says
the tester pressed keys, but it does not establish that alphabetic digital
events or text delivery occurred. The 2111 `unrelated` frames include events
near those presses; their packet class was not recorded, so they cannot be
called typing events. Two positive physical indices after a prompt likewise
do not prove simultaneous depth. The former `evidence complete=1` meant only
that the OLD code's analog counters fired; it is not a support verdict.

Most importantly, re-reading the hash-pinned official
`layout-classic-CQddK7Ye.js` at its UI consumer shows `keyStroke/100` and
`maxStroke/10`. One raw maxStroke unit therefore equals ten raw keyStroke
units. The delivered trial had mistakenly compared the raw numbers directly.
Its `shallow=63`, `near_max=659` and `range_anomaly=84095` use wrong thresholds;
the high anomaly count is not device-failure evidence, and the recorded
near-full stage did not prove a >90% press. The corrected parser compares
`stroke` with `maximum*10`, shallow with `maximum`, and near-full above
`maximum*9`; its synthetic test uses maxStroke35 and stroke340. The log's ADC
changes are usable evidence of varying sensor values, but scale-dependent
coverage must be measured again.

The next one-EXE trial also counts only physical letter down/up transitions
from a Raw Input keyboard matching the admitted exact VID/PID; it logs no
letters, order or text. It distinguishes matched keyboard identity, misses,
repeats and transitions after the analog prompt. When the analog gates and a
post-prompt letter down/up are observed, the trial ends automatically, sends
OFF, logs cleanup and shows a Russian `всё готово` title. If data never become
ready, closing HallJoy still attempts OFF and preserves missing-evidence
fields. Raw Input arrival does not prove that another app displayed text; it
does directly answer whether physical letter signals reached HallJoy.
Other reports are now classified in aggregate and logged at sparse cumulative
milestones. The normal writer retains the first v3 marker and the newest
records including final cleanup even if a long run exceeds2300 research lines.

Support reconciliation: exact Alumix 104 (Magnetite Ice) changed from `Not
investigated` to `Research incomplete` in live Sheet Main!C644. A643:C645
readback confirmed the model, unchanged Alumix 68 neighbor, strict dropdown,
and preserved base/effective gray RGB 0.92156863/0.93333334/0.9490196.
`docs/SUPPORTED_HARDWARE.md` now records the same research scope. No native
analog support or publication claim; no TKL/Alumix68 status change.

Ordinary full-catalog `tools/build_release.ps1` PASS: diagnostics contract,
Windows writer long-run retention regression, source audit, eight executable
gates including the research self-test, embedded resources and verified install.
Candidate/installed SHA256 both
1FA537024E71489C473DBA4556266FAFC4C88D773073995912B6907BCB2C9CC4.
Build log `.local/alumix104-log39-build.log`. No second physical run yet.
Tester receives this single normal HallJoy.exe, presses/releases one key gently
and fully, holds two keys, then presses/releases any letter when the title asks.
HallJoy turns the trial off automatically and displays `всё готово`; the tester
returns the ordinary HallJoy.log through Open log. No timed wait, Notepad,
Pause, scripts, flags or extra diagnostic files are required.
Fresh live Main!A1:C796 comparison found252 yellow rows, exactly matching
the252 explicit runtime notice models; no missing/wrong/unexpected yellow
status. `python tools/support_notice_catalog.py` also passed. No row insertion,
layout/status notice or README model-list change was needed.

## Log39 follow-up: distinct v4 correlated-hold experiment — 2026-09-28

The prior corrected v3 EXE was mainly a repair of the original66/67 stream
trial. Its `two_indices_after_prompt` accepted two different indices at
different times and could not establish simultaneous keys. Its Raw Input
letter counters had not yet been exercised on the tester's device. The owner
explicitly asked that the next run investigate what log39 could not answer.

The v4 trial retains the exact official66/67->55FB transport and corrected
`keyStroke/(maxStroke*10)` scale. It adds a separate in-memory join between
the official exact-model physical letter HID-to-sensor-index map and Windows
Raw Input from the target VID/PID. The map was checked against the pinned
`.local/alumix104-review/keyboard-map.json`: all26 letters match, with26
distinct indices. No key identities, text, raw packets or individual travel
values are written to HallJoy.log.

The two-key stage now requires positive55FB samples for two corresponding
letter sensors while those two physical letters are simultaneously held in
one unchanged Raw Input state. Merely seeing two indices over a session no
longer completes it. The log records counts for paired-hold witnesses,
positive/zero sensor samples while their letter is held, distinct matched
letters and peak digital letters held. Raw Input and HID stream callbacks
are asynchronous, so this is corroboration of two-key coexistence, not a
synchronized hardware snapshot or proof for all104 keys. No witness is an
unknown, especially if digital HID is suppressed in simulation mode.

After the paired hold, the final stage requires a complete down/up of the
same physical letter after the prompt. On that evidence HallJoy attempts67
OFF, records the ACK/cleanup and shows `всё готово` in the Russian title.
If any stage remains incomplete, the tester closes HallJoy and returns its
ordinary HallJoy.log; cleanup and explicit missing-evidence fields still run.
No arbitrary experiment timeout, Notepad or Pause instruction. A successful
Raw Input pair establishes delivery to HallJoy, not rendered text in a
different app. Support remains Research incomplete; no native backend or
support promotion follows from this local build alone.

Writer recognizes the v4 marker and replaces an earlier retained research
trial while preserving the marker and newest records when its2300-record
bound is reached. The writer's zero-stream and long-run regressions use v4.
Normal `tools/build_release.ps1` PASS, including support diagnostics and
verified ordinary install. Exact-image `--halljoy-require-full-catalog` and
v4 synthetic stream self-test exited0. Installed EXE SHA256
462853E30E8997C98A0262387901C928A454FA0EF9FEF92CBDFD9E1AB6B01AE8;
build log `.local/alumix104-v4-build.log`. No physical v4 test yet.
The last title refinement keeps the two-letter and final letter instructions
visible during stream silence; the initial no-stream hint applies only before
the active key stages. Final ordinary build and both EXE gates passed again.

## HallJoy (40).log: v4 physical result and corrected release step — 2026-09-28

Private tester log `C:\Users\PC\Downloads\HallJoy (40).log` SHA256
BF2FF3E79AF810F669399048287A5088870718E6EDDCEFBC5994DB7F1A216ABD
(28374 bytes) is the v4 run. Exact Alumix104 admission, initial67/66 writes
and ACKs,13530 valid55FB samples from two positive sensor indices in13.672s,
and final67 write/ACK all succeeded. There were4 analog positive-to-zero
transitions,618 depth changes, no malformed/out-of-catalog packets, read
errors, research-record drops or support-log queue loss. The51 unrelated
full-length reports had a different prefix; they are not classified as
letter events. The generic `source=none_reported` reflects the absent native
analog backend, not absent stream data.

Two mapped sensors gave2480 positive samples while the target Raw Input
keyboard reported letters held, with two paired-hold witnesses and a peak of
two digitally held letters. The target keyboard produced5 physical letter
downs and5 ups, no orphan ups, and57 repeats. This establishes delivery of
alphabetic Raw Input events to HallJoy during the simulation lease and
corroborates two-key analog observations for the tested pair. It does not
show rendered text in another app, all104 key positions, or sustained
independent updates for both sensors: v4 recorded only aggregate pair samples.

The tester reported that `всё готово` appeared only after pressing and
releasing the letters again. This follows directly from the v4 state machine:
after the paired-hold phase it required a *new* same-letter down/up pair.
Log40 has `after_ready_down=2`, `after_ready_up=4`, `after_ready_pairs=2`:
the first two ups were the original held pair, then two new downs/ups
completed the artificial extra step. The v4 title combined "release the two"
with "press a letter", obscuring which action unlocked completion. The
tester did the instructed physical release; the test criterion was wrong.

The corrected scale still reported2999 `range_anomaly` samples out of13530.
v4 combined `maxStroke==0` with `keyStroke>maxStroke*10`, so this count
cannot establish overtravel, a defective sensor or the correct normalization.
`max_stroke_changed=4`, `adc_outside_bounds=2`, `shallow=69` and `near_max=64`
are retained as scoped evidence. The official UI uses `keyStroke/100` and
`maxStroke/10`, but its rendering alone does not explain the anomalous
physical samples. Do not declare full-range accuracy yet.

Local v5 diagnostic retains one normal EXE and one HallJoy.log. After a
paired-hold witness it explicitly asks to release those same two letters;
completion requires both Raw Input ups and positive-to-zero analog transitions
for their mapped sensors, without any new press. A target Raw Input device
removal cannot masquerade as release. Pair sample/change balances and coarse
range categories now split zero maximum from above-maximum readings without
logging identities, key text, raw packets or precise travel values. The v5
marker resets retained v4 evidence. No experiment-wide timer was added.

Normal `tools/build_release.ps1` PASS, including writer long-run regression;
v5 synthetic parser/release self-test and exact-image full-catalog gate exited0.
Installed ordinary EXE SHA256
7B6DF7FEF432B08C15505CF324AC2EA8F782FA0C896B9C0C9C771F84095C518B;
build log `.local/alumix104-log40-v5-build.log`. No v5 physical result or
native analog backend yet. The tester's next run, if needed, should press one
letter gently/full and release, hold two letters together, then release that
same pair; HallJoy should finish by itself. An incomplete stage can be ended
by closing HallJoy and returning Open log. No Pause, editor, script or flag.

Support synchronization: exact Alumix104 remains `Research incomplete` in
`docs/SUPPORTED_HARDWARE.md` and live Main!C644; raw value, strict dropdown,
gray base/effective colors and A643:C645 neighbors were read back. Live
Main!A1:C796 snapshot `.local/alumix104-log40-sheet-snapshot.json` SHA256
628BDA09957B75FF211782153BBEBC886309B97D1DD109423E29E9589D21A81D
passed `support_notice_catalog.py --sheet`: all252 yellow rows match runtime
notices. No Sheet cells were changed; Alumix68 remains Not investigated.

Offline recheck before asking for more device data: the pinned official UI
parses `keyStroke` and `maxStroke` from the same55FB packet and displays them
as `/100` and `/10`. The exact104 dispatcher replay establishes the66/67
mode flag and bounded query echoes, but explicitly does not execute its
scanner or USB stream producer. Neither source can divide log40's2999
aggregate anomalies into zero maxima versus above-max travel. The v5 coarse
categories are therefore the narrow remaining measurement; this is not a
reason to use calibration/settings writes or to infer defective hardware.

## Decision after owner review of the repeated tester loop — 2026-09-28

Log40 was a substantive new experiment, not a failed rerun of log39. Log39
established the exact-device stream and OFF cleanup, but its scale gate was wrong
and it could not identify alphabetic keyboard input or simultaneous analog keys.
Log40 used the corrected official scale and observed two mapped positive sensors
during one overlapping two-letter Raw Input hold, plus target keyboard letter
downs/ups. The tester's extra press was required only by the v4 completion
state machine. It does not invalidate the preceding sensor or keyboard evidence.

The local v5 EXE fixes that completion condition and refines coarse range
counters. It has not produced a physical result and is not a new support
experiment. Do not ask the tester to repeat the same scenario merely to confirm
that the title says `всё готово`. The2999 v4 range anomalies remain an honest
unknown: v4 did not split zero maximum from above-maximum stroke. That unknown
limits a full-range accuracy claim, but it does not erase the observed varying
depth, shallow/full/release samples or paired-key evidence. A backend can
explicitly reject or clamp invalid denominators while recording aggregate
quality; its actual gamepad behavior must then be tested as a new integration.

Next substantive work is a native exact104 analog-to-gamepad path based on the
official transient66/67 lease, 55FB samples and the pinned104-key map. It must
own the collection exclusively (the research worker cannot run concurrently),
publish measured depth only, clear stale/released keys, send67 on Pause/exit,
and retain ordinary support-log diagnostics for transport/range failures. Do
not claim support from an EXE which merely prints research counters. Preserve
the other ordinary backends and full-catalog gate. The eventual tester run
should exercise actual HallJoy/gamepad analog behavior and return its normal
HallJoy.log; the v5 release-hint repair is not its purpose. Exact104 remains
Research incomplete until that path and support-status reconciliation exist.

## Native analog-to-gamepad trial implemented locally — 2026-09-28

This is the new integration test requested after log40, not another v5
completion-hint run. The ordinary HallJoy backend now admits exactly product
`Alumix 104 Yotei Magnetic` at 0C45:80AC with FF68:61 and 65-byte in/out
reports; the PID alone is insufficient because TKL Horizon shares it. The
backend owns the HID collection without the earlier research reader. It sends
67 OFF then 66 ON, waits for the ON ACK and a valid mapped 55FB sample, then
publishes measured travel into the normal native analog routing. The official
104 factory map has 103 bindable positions; calculator has no keyboard HID
usage and is not exposed. Fn uses HallJoy's dedicated logical key. The
producer applies `stroke/(maxStroke*10)`, clamps above-full samples and never
invents a positive value when the maximum is zero. Per-key values expire after
1000 ms of silence for runtime safety. There is no experiment-completion
timer: the analog session remains active until Pause, exit or a transport
failure. Pause/exit clears gamepad values, attempts 67 OFF and records the
final OFF ACK. The ordinary full catalog, including limited MAD68, remains
compiled into the same EXE.

The existing `HallJoy.log` records admission and mode/ACK evidence, bounded
stream/idle checkpoints, sample/range/integrity categories, target keyboard
letter events, source publications and consumer positive reads. The backend
also counts gamepad output publications and nonneutral reports while Alumix104
is connected, plus ViGEm state/error at session end. Those aggregate output
counts alone do not prove Alumix caused a gamepad report; a positive consumer
read together with a configured binding and tester observation is needed.
No key identity, text, path, serial, raw report or precise individual travel
value is logged. The dedicated trial marker and newest evidence survive the
normal Open log snapshot. Incomplete and failed sessions still record cleanup
and missing evidence. Ordinary support logging remains opt-in/incident-based;
the trial uses only the existing bounded research channel.

Synthetic exact-map/parser tests and a fake HID session cover connected
publication, malformed frames, disconnection, failed ON and missing ACK.
The writer's mirror timestamp is now shared across destinations so a second
boundary cannot make mirrored logs differ. The normal Release build and
exact-image catalog gate are required before handing the EXE to the tester.
No physical gamepad result exists yet; measured range accuracy, remapped keys
and rendered typing remain open. The exact104 status stays gray `Research
incomplete`, and no publication is authorized by this local build. Live Sheet
Main!C644 was read back gray with its strict dropdown/neighbors; a fresh
Main!A1:C796 export passed all 252 yellow notice rows, with no Sheet edit.

Final normal `tools/build_release.ps1` PASS: mandatory support writer tests
(ordinary and input-path), Alumix104 fake HID session, static diagnostics,
Release x64, exact-image full catalog and seven other executable gates,
embedded dependency/license verification. Installed tester artifact:
`build/bin/Release/x64/HallJoy.exe` SHA256
`C4960050A37E44131F4206E0434C72A8D979D3B902BD6905D225650C162AC9F0`.
Build record: `.local/alumix104-gamepad-release-final.log`.
Fresh live Sheet snapshot: `.local/alumix104-gamepad-sheet-snapshot.json`;
`support_notice_catalog.py --sheet` PASS with 252 yellow models.

## Physical gamepad log41 and release-latency review — 2026-09-28

Tester feedback: analog gamepad output works and presses are responsive, but a
released key can remain held for roughly 0.5–1.5 seconds. This is substantive
physical proof of the new integration and a real remaining quality defect, not
another completion-hint problem. Input `C:/Users/PC/Downloads/HallJoy (41).log`,
SHA256 `1308238500AD93610F62638D97E7BFCA29C93349C95830B2C42B2E7D041C104C`.

- Exact admission: 7 same-VID/PID collections, 6 rejected by exact descriptor,
  1 accepted; 65-byte reports, 256 input buffers, initial OFF/ON and ON ACK.
- 264392 input reports over about266 seconds, 262149 valid 55FB samples from
  12 indices, 11 positive; 21584 depth changes and 165 sampled releases.
  Max packet gap16 ms, idle reads0, malformed0, log drops0.
- HallJoy's native consumer made 16432710 reads, 103388 positive; 9992
  gamepad publications while connected, 9902 nonneutral, rejected0. ViGEm
  ready1 and error536870912 is `VIGEM_ERROR_NONE`. The tester independently
  confirms output responding to presses.
- Target Raw Input registered/matched1, letter down/up81/81, peak held4.
  Final OFF and ACK both succeeded. This does not measure each letter's
  digital-up-to-analog-zero or gamepad-neutral latency.
- Range quality: maxStroke zero104 times (19 positive-stroke cases), 47111
  samples above declared full but none above125%, ADC outside39. Clamping
  kept values bounded, but exact full-range calibration remains uncertain.

Source review found the native publication allowed a positive key depth to
remain fresh for1000 ms after its last sample. The stream may switch indices
without a zero sample for the prior key. That is a credible cause of much of
the reported release delay, but log41 cannot establish per-key causality;
firmware and output timing have not been ruled out.

The proposed Raw Input key-up mask was rejected by the owner and removed before
delivery. Rapid Trigger can change digital state while physical travel remains
positive; digital events must never determine analog/gamepad depth. They are
diagnostic reference only. No corrected physical run occurred.

Support sync: exact model remains live Main!C644 `Research incomplete`, strict
dropdown and gray B/C fill read back; Main!A643:C645 neighbors unchanged.
Fresh Main!A1:C796 export `.local/alumix104-log41-sheet-snapshot.json`
passed all252 yellow runtime notice rows. No Sheet cell was edited.

## Physical multi-key feedback and log42 — 2026-09-28

The tester reports that with two held keys the second can activate late and a
released key can remain active for roughly one or two seconds. With six keys
pressed together, gamepad inputs turn on and off inconsistently. These are
physical observations, not conclusions derived from the aggregate log.

`C:/Users/PC/Downloads/HallJoy (42).log` SHA256
`B98DA28D2D0EF495F57F96A9CC4D26B2A5ACD694D93E8EEC0599E449843D2CD9`:

- Exact 0C45:80AC admission, 65-byte FF68:61 collection, ON and final OFF
  acknowledgments. A ~100.9-second session contained 100312 reports and 99441
  valid 55FB samples over 35 indices (34 positive), 14404 depth changes and
  151 observed zero releases. `paired_hold=24` counts repeated two-letter
  positive witnesses while their digital states overlap, not continuous
  independent sensor histories. Maximum inter-report gap was 16 ms; idle reads,
  malformed frames and log drops were zero. 868 other reports were classified
  as unrelated, including 863 with a different prefix.
- Source publication 99431, consumer reads 5349798 (57973 positive), failed
  updates 9. The nine failures match positive stroke with zero maximum and
  were deliberately not converted into fabricated depth.
- Gamepad publications while connected 114, 111 nonneutral, rejected0, ViGEm
  ready1/error536870912 (`VIGEM_ERROR_NONE`). Raw Input matched2 devices,
  letter downs/ups123/123, maximum five letters simultaneously held. The six
  keys in tester feedback are not contradicted by this letter-only counter:
  some tested keys can be nonletters and aggregation has no timing trace.
  Log41 had 9992 publications over ~266 seconds, while log42 had 114 over
  ~101 seconds. Different binding/activity windows or output coalescing may
  account for this; without a common per-binding timeline the difference
  must not be labeled an output-layer fault.
- The log does not record per-key report intervals, the order of simultaneous
  values, digital-to-analog latency or gamepad state for each binding. It
  cannot distinguish firmware selection/serialization from stale-value
  retention or binding conflicts. The neighboring Alumix68 firmware's
  selected-key-only simulation is a warning, not proof about exact104.

Decision: the present 0x66/0x55FB gamepad trial has not established ordinary
Alumix104 support. Owner requires true measured, independent key depths and
rejects a digital-event approximation. Retain the trial source and catalog
entry as research while investigating the cause; do not deliver it as finished
support or treat the interim safe local EXE as abandonment of the integration.
The candidate routes remain: (1) 0x66/55FB single-record stream, whose
multi-key suitability is unproven and current gamepad result is inadequate;
(2) exact104 read-only
per-key snapshot, not yet identified in the captured upper code (0x16 is flash,
unlike neighbor68 RAM); (3) official 0x68 query, which only echoed requests in
the bounded exact104 dispatcher replay. The capture does not include the
scanner/USB producer; this is not proof that an independent source is absent.
Do not ask the tester to rerun the same gamepad trial or change freshness
timeouts. A future investigation must test a distinct analog-only source or
measure per-sensor timing and gamepad routing precisely enough to locate the
fault, without text, key identities or raw reports in the shared log.

Support sync: exact104 remains gray `Research incomplete` in public docs and
live Main!C644; the verified Sheet snapshot
`.local/alumix104-log41-sheet-snapshot.json` retained strict validation, gray
fill, unchanged neighbors, and all252 yellow notices passed reconciliation.
No Sheet write or other model status change is required. An interim local EXE
was built without the trial while replacing a rejected digital-mask build;
the source/catalog exclusion was reverted after the owner clarified that work
on full support must continue. Neither EXE is a new tester handoff.
The interim installed EXE SHA256 is
`58943354B24FBBB1B081DA7C065A753D32D645DADD1A85EB24A39C1C551C4D48`;
its source admission differs from the now-restored research catalog. It passed
the ordinary exact-image gates before installation, but must not be sent as
an Alumix104 test build. Build record:
`.local/alumix104-log42-safe-build-retry.log`.

### Root-cause review and next discriminating experiment

The existing logs establish a responsive USB stream overall, not a responsive
stream for *each* held sensor. The 55FB packet and the official configurator's
`ji` parser contain one physical index and one stroke/maximum pair. HallJoy
publishes that one index and reads each published value until it is 1000 ms old.
If the firmware stops mentioning a previously positive sensor without sending
its zero, HallJoy really does keep that value up to 1000 ms. That behavior is
proven from source and is a credible contributor to the observed release lag;
it does not establish why the firmware did or did not send a zero. Shortening
the timeout merely trades lingering values for dropped held keys and cannot
make a one-record event stream into a reliable whole-keyboard snapshot.

The second-key delay and six-key oscillation could arise before publication
(firmware selects or serializes a subset of sensors), inside publication
(missing zero/long freshness), or after publication (binding conflicts or
gamepad aggregation). Current logs provide totals at all three stages, not
a common per-sensor timeline, so none of these causes can yet be singled out.
The exact104 scanner/USB producer is outside the captured firmware window.
Neighbor68's selected-key-only producer increases suspicion but cannot be
substituted as exact104 proof.

The official client has a distinct `GET_MAGNETIC_AXIS_STATUS` request (0x68):
its `Qi` function requests up to seven physical indices per transaction and
parses eight bytes per index, including `currentStroke`. This is a concrete
candidate for independent per-key reads. However the only observed UI caller
first invokes `startCalibrationV2` (0x69) and ends with `stopCalibrationV2`
(0x6A). The captured exact104 upper dispatcher echoed 0x68 payloads across
280 tested cases, and no actual device 0x68 result exists. Calibration commands
must not be enabled merely to obtain gamepad input: their effects on typing,
calibration state and persistence are unverified. The unused official 0x60
`GET_MAGNETIC_AXIS_KEY_STATUS` constant has no client call in the pinned
bundle; exact104 replay also echoed its candidate requests. The neighboring
68 command 0x16 RAM-depth path is unavailable at the same address on104:
exact104 0x16 reads flash B000+offset. These are scoped negative results,
not proof of protocol absence.

Before another tester EXE, inspect the pinned official assets and captured
code for a stock104 full-matrix route, obtain exact104 firmware or producer
source if available, and review the other HID collections. If that yields no
safe independent reader, the next one-EXE experiment should be *diagnostic*,
not a claimed support build: keep 0x66/67 cleanup, record bounded per-sensor
inter-report/release timing and concurrently positive sensor counts, classify
the 868 non-55FB frames by header without payloads, and correlate source,
consumer and gamepad transitions in the same monotonic time windows. A separate
read-only 0x68 request with calibration OFF can test whether physical firmware
provides usable multi-index values; never send 0x69/0x6A or persistent writes
without a behavioral review. Digital Raw Input may annotate timing only, and
must not influence gamepad depth. This would distinguish source limitations
from HallJoy retention/output faults instead of repeating the current test.

## Owner correction: prove the cause with a complete trace — 2026-09-28

The owner requires diagnosis of the missing per-key chronology before choosing
an analog algorithm or declaring a firmware limit. Do not infer release from
an absent per-key report: an event/selected-key producer may omit a still-held
sensor, and USB loss can also create silence. Digital Raw Input can annotate
the trace but never zero, mask or synthesize analog depth, including with Rapid
Trigger. No new support status follows from logs41/42.

Replayed the pinned neighboring RSQ-20058 v1.30 firmware SHA256
`020778ba842603c49fe21365ad10380dd877f9be29ecaeefb9760eeab69461c1`
with `tools/review_redsquare_alumix68_firmware.py`'s `Machine` and synthetic
RAM flags. At branch0x4F26, stop0x4F40 is reached with calibration=0 for
simulation=0 and1. With calibration=1, both simulation states branch to
0x4B30 instead. This scoped execution confirms that calibration diverts the
neighboring firmware's normal-processing branch; it does not execute the
entire keyboard HID producer or prove physical typing loss on exact104.
Therefore 0x64/65 and 0x69/6A calibration commands remain excluded from
HallJoy gameplay and the planned trace. The neighbor's selected-key simulation
can omit a nonselected key whose synthetic depth is positive (0x4E1C tail);
silence cannot safely mean release on exact104 without independent evidence.

Trace v2 is being implemented in the normal exact104 session, with no new
mode writes beyond67 OFF/66 ON/67 OFF. Its time windows are observation
cadence, not test-completion deadlines. The shared HallJoy.log records:

- One opaque session slot per observed sensor, its reports/positive/zero/exact
  repeats, maximum inter-report gap after a positive sample and explicit
  positive-to-zero transitions. A missing zero stays explicitly unknown.
- At 250 ms while recent positives exist (1 s while quiet), aggregate source
  activity and slot cadence/age, Raw Input letter counts as reference, consumer
  reads/latest depth bucket, bound-pad mask, computed gamepad candidate state,
  and actual publication counts. This separates source, HallJoy retention,
  binding/output calculation and publication without using digital in output.
- Classification of other protocol headers by family/opcode counts, not raw
  payloads; final per-slot summaries and ON/OFF cleanup even if the stage
  stalls or the owner closes HallJoy. No keyboard text, exact travel, device
  paths/serials or physical key codes in the shared log.

The trace does not itself establish exact104 firmware behavior until a new
physical run. In particular, a high global packet rate cannot prove that
each held sensor repeats, and a source-positive/gamepad-neutral window may
still reflect intentionally opposing bindings. Review the tested executable
and log contract before tester handoff; do not treat a source build as a run.

### Trace v2 local build and scope

The completed trace also records the calculated nonneutral pad mask in each
window and, at session end, each anonymous slot's action mask for all four
gamepads. This distinguishes opposing axis bindings from source or consumer
loss without logging a physical key index, key text or raw travel. The stream
still uses only 0x67 OFF, 0x66 ON and 0x67 OFF. No calibration, 0x68 polling or
digital analog fallback was added. The existing 1000 ms last-value retention
is unchanged deliberately: the trace must reveal whether it contributes to
the tester's lag before replacing it with another unsupported assumption.

`python tools/check_support_diagnostics.py` PASS after the changes, including
the fake exact104 HID session, source repeat/silence/explicit-zero assertions,
binding-map assertion and Win32 writer snapshot retention. The ordinary
`tools/build_release.ps1` PASS, including exact-image full catalog, other
self-tests and embedded-license verification. Local installed image:
`build/bin/Release/x64/HallJoy.exe`, 10,471,424 bytes, SHA256
`7F9EB3F55F5C94C3732ED724119F34956423AE953B7AA78DA92310CA46820E12`.
Build log: `.local/alumix104-trace-release-build.log`. This is a diagnostic
trial for one physical exact104 run, not supported status or public release.
Sheet Main!C644 remains gray Research incomplete from the prior verified
readback; no status write was warranted by a host-only build.

## Physical trace v2: HallJoy (43).log and tester feedback — 2026-09-28

Private log `C:/Users/PC/Downloads/HallJoy (43).log`, SHA256
`8319E1E571218228EA273CD405A9B6720902A3F1D2E9CC56BDD6989B020EB496`.
The tester reports a smaller but still visible 0.5–1 s release delay; when two
keys are released together one can remain active longer. With more keys the
behavior is less chaotic than the prior trial but still jumps after 1–2 s.
This report is physical observation, separate from our log interpretation.

The ~69.5 s session received 69,096 reports, 68,270 valid 55FB one-index
samples, zero malformed/out-of-catalog reports, no idle read and a maximum
global report gap of 16 ms. Raw Input observed 99 letter downs and 99 ups,
peak six simultaneously held; it is diagnostic context only. Output published
1,645 gamepad reports, 1,615 nonneutral and zero rejected; ON/OFF and final
OFF ACKs succeeded. `analyze_support_log.py` found complete schema2 telemetry
and no observed queue loss. Thus this is useful evidence, not an incomplete
logging run or a global USB starvation episode.

Across 259 source windows, the maximum number of sensors sampled within the
last 50 ms was two. During 101 windows with two digitally held letters, only
11 windows had two recently sampled sensors; 90 had one. The per-window maximum
with three to five held letters was still two. These asynchronous windows do
not assert exact simultaneity or a permanent firmware limit, but they do show
that the current 55FB mode does not keep every held sensor fresh. The single
record per 55FB packet is also confirmed by the official client decoder.

A direct two-key release episode identifies the host delay. At uptime
26,183,609 ms Raw Input recorded two ups; at 26,183,859 ms the stream emitted
247 zero reports for one anonymous slot but no report for the other. The
omitted slot's last sample remained positive and 391 ms old, so the gamepad
candidate stayed nonneutral. At 26,184,359 ms that sample was 891 ms old and
the output was still nonneutral. At 26,184,609 ms it was 1,141 ms old and the
candidate had become neutral. This matches the implemented 1000 ms retention
in `Get()`. Other final slot summaries show long positive-to-next-report gaps
(up to 3,297 ms for one bound slot) and positive slots with no final zero.
The opaque trace cannot identify which physical key was pressed first in all
episodes, so do not assign the tester's first/second-key label to a slot.

Root cause in the tested path: the firmware/reporting mode omits individual
sensor updates for long periods while the global stream continues. HallJoy's
1000 ms last-value rule then converts an omitted zero/unknown state into a
visible stale held value and eventual timeout. Reducing that timeout would
prematurely drop genuinely held but omitted keys; extending it worsens release.
No algorithm over this stream alone can distinguish an omitted still-held key
from an omitted released key. Digital Raw Input cannot resolve it without
violating the owner's Rapid Trigger requirement. The exact firmware selection
rule and whether another independent analog interface exists remain unknown.

Next investigation must find an independent per-key source. The captured
exact104 upper dispatcher routes 0x16 to flash, while 0x60/0x68 candidates
echoed in the bounded replay; these results do not prove all transports absent.
The official 0x68 query is read-only but its observed UI caller enters
calibration first. Neighboring firmware replay shows calibration diverts
ordinary processing, so do not enable it for gameplay. Prioritize exact104
firmware/producer acquisition and a review of other HID collections or a
strictly read-only, calibration-off query before any new tester EXE. Do not
send another build that merely changes the 1000 ms timeout or repeats 55FB.
The official `https://app.red-square.org/update.json` was fetched again on
2026-09-28: its 128-byte response still lists only neighboring RSQ-20058
v1.30. This does not exclude an exact104 image at another vendor location.

Support synchronization: exact `Red Square` / `Alumix 104 Yotei (Magnetite Ice)`
at live Main!A644:C644 remains `Research incomplete`, with strict dropdown and
gray base/effective colors; adjacent Alumix 68 remains `Not investigated`.
Fresh Main value read found all 252 yellow rows match the local notice catalog
by brand/model/status; a second bounded native read found their base and
effective yellow backgrounds consistent. No Sheet write or support promotion.

## Seven-address matrix probe built locally — 2026-09-28

The owner's next experiment is the official read-only `0x68` query over seven
physical indices per packet, prioritizing bound keys. Review found an existing
independent addressed polling implementation: IPI requests nine IDs per packet,
validates every returned ID and uses `addressed::PollScheduler` for bound,
moving, active and background slots. Its scheduling policy now accepts a
transport capacity of seven; the initial sweep visits bound positions first.
IPI's six-byte wire records cannot parse Red Square's eight-byte records.

The exact104 probe uses the vendor client's AA:68 request layout, 56-byte
payload, reply offset and one-packet final flag. The vendor decoder treats the
response's third header byte as length *or* type, so HallJoy records variants
without rejecting a matching-address response for this byte alone. It requests all 121 known
physical positions (103 factory-mapped, remaining positions included for
coverage) in an initial 18-packet sweep while `0x66` is OFF. It then switches
`0x66` ON and continues priority scheduling in a separately logged phase.
An echoed payload is an ACK only and advances sweep fairness without replacing
last measured depth. Non-echo payloads are only *candidates*: per-anonymous-slot
positive, zero, change, release, last age, bound-action mask and digital
reference counters, plus same-reply/fresh-positive maxima, are logged. The
old selected-key `55FB` stream stays visible as reference; its stale values no
longer drive the gamepad in this probe. Raw Input is reference only and never
calculates analog. No calibration/startCalibration, flash or persistent-setting
commands are issued. Short write/read deadlines and 5 ms query pacing protect
the transport; there is no test-completion timer. Pause/exit still sends `0x67`
and records cleanup in the ordinary HallJoy.log. The Russian title describes
scanning, echo or a candidate response.

The captured upper exact104 dispatcher previously echoed `0x68` in 280 bounded
offline payload cases, including seven-key requests, with simulation on/off.
This probe tests the *physical* device with calibration off; it does not assume
that a firmware reply which differs from an echo is live independent travel.
The missing producer/firmware code and typing coexistence remain unknown.
The physical acceptance evidence would be repeatable positive and explicit zero
readings for several simultaneously held keys with acceptable per-key freshness,
and no typing loss. A non-echo packet or synthetic test alone does not promote
support or authorize analog-to-gamepad conversion without a validated range.

Fake HID test passed full 121-position/18-packet echo sweep in the OFF phase,
the subsequent ON query with bound-first ordering, candidate positive-to-zero
transition, malformed reply, disconnect,
failed ON, absent ACK and OFF cleanup. Shared scheduler tests passed seven-slot
capacity and preservation of active measurements across echoed queries.
Ordinary `tools/build_release.ps1` passed the diagnostics gate, exact-image
full-catalog and seven other image gates, embedded notices/license check and
atomic installation. Local EXE: `build/bin/Release/x64/HallJoy.exe`, 10,490,880
bytes, SHA256 `8448BD534B64697072B803941B791BF7F9372545F394B756E410DBDEA6544F4A`.
Build log: `.local/alumix104-batch68-final-release-build.log`. No physical
`0x68` run, release publication or support promotion yet. Exact-model live
Sheet Main!C644 stays `Research incomplete` (gray; prior readback of value,
dropdown and colors); no status write is warranted. This is a diagnostic
one-EXE/one-log tester trial, not a supported analog gamepad implementation.


## Physical seven-address result — log 44, 2026-09-28

The tester's ordinary HallJoy.log (83,813 bytes, SHA256
A6D773A025805A1AD38BECE243610D1FF79B510E66F1219B8423E546A84A875E)
came from the local batch68 EXE. Exact 0C45:80AC / product /
FF68:61 65-byte admission matched one collection; six other matching-VID/PID
collections were rejected by descriptor. The OFF phase queried all 121
positions in 18 replies: 18 echoes, zero non-echo candidates. After the
successful 0x66 ON acknowledgement, the ON phase again swept all 121 and
continued polling. Its final count was 1,981 requests and 1,981 matching
payload echoes; zero candidates, timeouts, malformed replies, header variants
or failed writes. The equality check covers every requested eight-byte record
and checks the response address. The exact upper-dispatcher replay's echo
behavior is thus reproduced on the physical device with calibration off.
This identifies no per-key depth in this 0x68 mode; it does not rule out
other commands, interfaces or firmware states.

This was an active keyboard trial: Raw Input recorded 30 letter downs and
30 ups with peak five held, and the independent 0x55FB reference stream
recorded 28,656 valid samples, 11 positive physical indices and 10 explicit
positive-to-zero transitions. Its global packet gap stayed at most 16 ms.
Thus the absence of 0x68 data is not explained by no key presses or a dead
HID reader. Raw Input only corroborates activity; it never calculates analog.
Actual text reaching an application was not verified. The 0x55FB trace still
has omitted/stale per-key readings established by log43. The trial deliberately
published no gamepad depths. Its two generic failure counts match two
zero-maximum/positive-stroke quality anomalies, not 0x68 transport failure.
OFF cleanup and acknowledgement both succeeded; log drops were zero.
The log also counts 124 frames whose byte 1 was not 0x55 in this same HID
collection. Current logging records only that prefix category, without a
bounded opcode/payload classification. Their content and role remain unknown;
they cannot be called analog data or noise from this log. Classify them locally
against the captured code before designing a distinct hardware probe.

Decision: do not repeat this 0x68 packet scan or promote support from its
echoes. Do not enable 0x69/0x6A calibration to force a candidate response;
neighboring firmware review shows calibration diverts normal processing and
typing coexistence for exact104 is unproven. The next useful work is local
analysis/acquisition of the exact104 scanner/USB producer or an independently
documented read-only interface across the other HID collections. Any later
physical experiment needs a distinct evidence-backed analog source, with
per-key freshness, explicit release and ordinary typing checked. The current
gamepad remains disabled for this exact model; no digital-event analog
fallback, retention-time adjustment, release or support promotion follows.

Status synchronization: exact Red Square / Alumix 104 Yotei (Magnetite Ice)
remains Research incomplete. Live Main!A643:C644 read on 2026-09-28
confirmed exact model at row644, strict status dropdown and gray base/effective
colors; adjacent Alumix68 row643 remains Not investigated. No Sheet write.
Public hardware documentation and OWNER_CONTEXT updated in this task.

## Alternative-source review and next diagnostic — 2026-09-28

Log44 closes the clean 0x68 matrix hypothesis for the tested exact104 mode,
not all possible firmware interfaces. Of its 2,123 unrelated reports, 1,981
were matching 0x68 echoes and 124 had a prefix different from 0x55. Log43
already counted 819 such non-0x55 frames without any 0x68 queries. They are
therefore recurrent independently of the matrix probe. Both logs record only
their count, so neither identifies their contents or proves analog value,
keyboard data, heartbeat or noise. The ordinary 0x55FB report is still a
single selected sensor with omitted/stale per-key states.

Rechecked pinned official layout-classic-CQddK7Ye.js: the client's active
simulation-test callback decodes 0x55FB; its 0x68 reader is called from the
calibration flow. Command 0x60 appears in the opcode enum but has no known
caller in this bundle. The exact104 captured upper dispatcher echoed 0x60
and 0x68 across the previously pinned 280 legal payload/state cases each.
The physical 0x68 echoes in log44 agree with that narrow replay. The code
capture omits the scanner and USB producer; neither the official bundle nor
the captured window establishes a different independent full-matrix reader.
The public updater feed inspected in this research contains only neighboring
RSQ-20058; no exact104 firmware was located in the prior local corpus search.
This bounded negative search does not imply that the vendor has no firmware.

A new local ordinary HallJoy.exe targets two remaining blind spots in ONE
normal HallJoy.log. It logs descriptor metadata (usage page/usage and report
lengths, no paths/serials) for all readable exact-product 0C45:80AC HID collections; descriptor failures are recorded explicitly.
Within the selected FF68:61 collection it groups non-0x55/non-0xAA reports by
anonymous prefix class and records counts, changed payloads, changes while the
Raw Input letter state is unchanged, two-letter overlap, zero-body count,
maximum nonzero-byte count and changed byte *positions*. It never writes
prefix values, payload bytes, keys, text or exact travel into the shared log.
The 250 ms/1 s windows place packet variation beside digital activity; they
do not determine analog from digital. The log records explicit collection
admission, no-data/error paths, OFF cleanup and final counters. It does not
query 0x68 again, enable calibration or publish these uncertain values to
gamepads. The exact-product 0x66 ON / 0x67 OFF selected stream remains a
reference; no test-duration timer or Pause instruction was introduced. Window
title asks for two to six letters together, releases one by one, then closing
HallJoy to send the log. Opening the log retains the new trial marker and tail.

Production fake-HID session validates anonymous frame changes, no physical
0x68 writes, failed ON, missing ACK, disconnect and final OFF. Writer
regression validates the new research marker replaces older retained trial
records. Ordinary tools/build_release.ps1 passed support diagnostics,
independent exact-image full-catalog and other executable checks, and embedded
notices/license verification; installed EXE is
build/bin/Release/x64/HallJoy.exe, 10,520,064 bytes, SHA256
EFE0E9EF2A839C9BA043777050379D487DEA1FC80D3026B794F84F9945716686.
Build receipt: .local/alumix104-unknown-trace-verified-release-build.log.
No physical run of this new frame classifier, release publication or support
promotion yet. Fresh live Sheet Main!A643:C644 read on 2026-09-28 confirms exact104 gray
Research incomplete, strict dropdown and gray base/effective colors; adjacent Alumix68 stays Not investigated;
no Sheet write is warranted.

Decision paths after this bounded local review:
1. If another collection or anonymous report class contains changing
   independent measured depths, decode it and require repeatable simultaneous
   hold/release, range and ordinary typing evidence before gamepad publication.
   Mere payload variation is insufficient.
2. If these reports provide no suitable data, obtain exact104 firmware/scanner
   source or a documented vendor read-only telemetry interface and review its
   normal-typing behavior. Existing upper code and neighboring firmware cannot
   substitute for the missing producer.
3. Calibration-gated 0x68 is unsuitable under the owner's typing requirement
   until exact104 evidence proves normal typing and nonpersistent safe behavior.
   Digital Raw Input and any retention timeout remain diagnostic context only;
   neither may synthesize analog depth. The stale selected-key 0x55FB path
   cannot be declared full support from current evidence.

## Earlier batch68 build with keyboard auto-calibration/stabilization disabled — log 45, 2026-09-28

The tester reports disabling the keyboard's auto-calibration and stabilization
before this run. The log itself does not read or confirm those vendor settings;
its `calibration=off` field describes HallJoy's own command choice. This is the
**older `HallJoy RedSquare batch68 probe v1`**, not the newer local
unknown-frame/collection diagnostic. Ordinary log45 is 72,333 bytes, SHA256
499F215FCC196072B23C6156ED88D4700061445DBDED358D22E8ED26C23CB70F.

The exact 0C45:80AC FF68:61 collection opened. In 0x66 OFF, all 121 positions
were queried in 18 replies, all 18 exact request echoes. In 0x66 ON, the
keyboard was active (Raw Input 51 letter downs and 51 ups, peak five held;
16,543 valid 0x55FB samples and 30 explicit positive-to-zero transitions).
All 1,152 0x68 replies in this phase were again exact echoes; zero independent
depth candidates, timeouts, malformed replies, header variants or failed writes.
The global report gap was at most 16 ms. The final 0x67 OFF was acknowledged.
No gamepad depth was published in this diagnostic.

The selected 0x55FB stream still lacked enough fresh simultaneous sensor values:
in 13 of 16 trace windows ending with at least two Raw Input letters held, the
number of positive sensors updated within 50 ms was lower than the held-letter
count. This is a lower-bound comparison, not key identity matching; even equal
counts do not prove that the same keys were sampled. Several windows had one
sensor receiving roughly 220–234 reports per 250 ms while two letters stayed
held, with other positive sensors stale for more than 250 ms. The old diagnostic
counted 196 non-0x55-prefix frames but did not classify their payloads. They
remain an open lead for the newer local diagnostic, whose physical run is still
pending.

The earlier log44 had 18 of 19 analogous multi-letter windows
with fewer fresh sensors, 1,981/1,981 0x68 echoes and 124 unclassified-prefix
frames. The trials differed in duration and key actions and the setting state
was not read by HallJoy, so these ratios do not establish a causal setting
effect. They do show that disabling the reported settings did not make the
calibration-off 0x68 route produce depths or make the old 0x55FB path reliable
for the observed simultaneous holds. The newer diagnostic remains the next
useful physical experiment; it does not re-run the echo-only query.

Support conclusion unchanged: exact Alumix 104 Yotei stays Research incomplete;
no digital-key analog fallback, support promotion or release. The previously
read live Sheet Main!C644 was gray Research incomplete; this log requires no
status edit.

## Can the exact firmware be read from the keyboard? — 2026-09-28

Partly, yes: physical log38 used the normal vendor HID command 0x12 to read
53,248 contiguous bytes mapped as C000..18FFF. This is a real exact104 device
capture, not a full image. The established command bases are at least 9000 and
use unsigned 16-bit offsets; they do not reach 0000..8FFF where the missing
scanner/USB producer may reside. Another repetition or extension of the same
0x12 scan cannot fill that gap. The capture may still guide code signatures
and interfaces, but cannot prove full firmware behavior.

Rechecked the pinned official web updater's `sn_isp_lib_bg-DOswDDJh.wasm`
(SHA256 8816f7c9...). Its 24 WASM exports include image loaders,
`set_code_security`, `start_isp`, process/message callbacks and completion
checking; none exposes an explicit flash/firmware readback operation. The
visible updater flow is programming oriented. This is a negative finding for
this particular updater API, not proof that the MCU or a different bootloader
has no read command. The updater lists series 248B/2480/290, but the exact MCU
inside the tested Alumix104 has not been physically identified; do not apply
one series' debug/security rules to the board without identifying it.

Candidate acquisition order: (1) exact official firmware/update file from
vendor or local configurator cache; (2) analyze the other exact-device HID
collections and the unclassified reports with the already built read-only
HallJoy diagnostic; (3) if the owner has a spare/disassemblable unit, identify
the chip and debug pads, then check the *specific* MCU's read-protection state
and documented non-destructive SWD/ISP readback procedure. Do not attempt to
clear protection merely to read: on some SONiX series the official datasheet
states that protected-to-unprotected transition mass-erases User ROM. No
software-only full-image readback, chip identity or safe hardware access has
been established for this exact keyboard. Nothing was flashed, erased or
changed on the tester's keyboard in this review.

## Physical unknown-frame/collection trace — log 46, 2026-09-28

The tester's normal HallJoy.log is 76,242 bytes, SHA256
32F9DDB2A30C42110E06B9CE6ADB74110AAB48C05C451540FA29A297781CED23.
Its start marker confirms the NEW `unknown frame trace v1` EXE, not another 0x68
scan. The exact 0C45:80AC product has seven HID collections. The only
65-byte interrupt-input vendor collection is FF68:61 (also 65-byte output),
which this diagnostic opened. FF67:61 has no input report in its Windows HID
caps, but exposes a 65-byte feature report and 4097-byte output report; it
cannot be dismissed as incapable of feature transactions. The remaining
collections expose standard keyboard, consumer, system and mouse input shapes.
No other passive vendor input stream was established by this inventory.

In roughly 21.9 seconds after ON ACK, FF68:61 supplied 21,379 valid 0x55FB
samples, 25 positive-to-zero transitions and one anonymous class of 359
non-0x55/non-0xAA reports. There were also three other 0x55 opcode-zero reports,
zero AA frames and zero short/report-ID anomalies. In the unknown class, 305
reports had an all-zero body; 101 reports changed body compared with the prior
report of that class, 85 of those changes occurred without a Raw Input held-state
revision, 128 reports overlapped at least two held letters and 38 of these
changed body. Across the run, exactly the first 12 body-byte positions changed
(`changed_offsets=4095`); at most 11 body bytes were nonzero in one report.
The class was present and changed in the initial idle windows too. Thus body
variation alone is not evidence of measured travel; the shared privacy-safe
log does not expose byte values, per-key identity or analog field semantics.

Raw Input recorded 58 letter downs and 58 ups, peak five simultaneously held.
In 17/24 windows ending with two or more held letters, the selected 0x55FB
stream still had fewer positive sensors refreshed within 50 ms than held
letters. Four of these 24 windows had no unknown-class report, and 11 had no
unknown-class body change. This does not prove the unknown class lacks an
on-change event protocol, but it prevents treating the aggregate as a
continuous independent per-key snapshot. No digital state was used to calculate
analog. The global report gap was at most 16 ms; malformed and out-of-catalog
counts were zero, log drops zero. Three generic failures match three
zero-maximum positive samples, not a transport failure. 0x66 ON and final 0x67
OFF were acknowledged. Gamepad publication stayed disabled.

Conclusion: the new report class is a concrete investigation lead, not a
decoded analog source. Next local work is to identify its format/producer from
available vendor code or an exact firmware image, and design a field-level,
privacy-preserving validation only if a measurable independent per-key
hypothesis emerges. Do not repeat 0x68, promote support, publish analog from
Raw Input, or label FF67 feature traffic an input stream. Exact Alumix 104
Yotei remains gray Research incomplete. Previously checked live Main!C644
already has that status; log46 changes no support decision and needs no Sheet
status write.

## Corrected exact104 unknown-packet capture after owner review — 2026-09-28

Owner challenged the decision to suppress the actual bytes in log46. That
suppression left one variable 12-byte region undecodable and made a repeated
physical test necessary. It was the wrong trade-off for this narrow support
investigation. The owner-authorized v2 diagnostic now records the COMPLETE
65-byte report as uppercase HEX for the first unknown vendor frame in each
class and every subsequently changed report or size transition, up to 512
successfully paired records. Exact repeats still contribute to timing/window
counters and `report_n`, without duplicating identical payload lines. The
`unknown_packet` line carries a stable event ID/class slot, class report count,
full byte length, Raw Input held-letter count and revision, and the last 55FB
sensor index/stroke/maximum with its age where available. The paired
`unknown_bytes` line carries the unmodified bytes including report ID/prefix.
This reference is for reverse engineering only; digital state does not set or
clear analog depth. The final `unknown_packet_summary` reports eligible,
captured, cap-skipped, emit-failed and limit counts even on incomplete runs.
A full capture cap changes the Russian title to request the normal log. No
fixed test duration, repeated 0x68 scan, calibration or gamepad output was
introduced.

Scope is limited to the exact 0C45:80AC product's accepted FF68:61 input
collection and its non-0x55/non-0xAA frames. Standard keyboard HID reports,
serials and paths are not read into this raw trace. Vendor payloads MAY encode
key state, so the window title asks for test letters only. The report header
marks `raw_hid_payload=1`; `no_keyboard_text=1` means no text transcription,
not that the raw packet is free of key information. This is an explicit,
model-specific diagnostic exception to the earlier no-raw-packet rule, not a
normal HallJoy logging policy. The tester still receives one ordinary EXE and
returns only HallJoy.log through Open log. The existing 2300-line research
retention, 4 MiB log bound, queue-loss reporting, OFF cleanup and full-catalog
ordinary build remain. A raw-data cap or failed writer is visible in the final
summary; a partial packet pair is not silently counted as captured.

Fake-HID session validates full payload plus selected reference, initial/changed
versus repeated report selection, no 0x68, failed ON/missing ACK/disconnect and
OFF cleanup. Windows writer regression verifies a 65-byte HEX line survives
exactly, `raw_hid_payload=1` is marked and the v2 marker resets retained v1
records. `tools/check_support_diagnostics.py` and ordinary
`tools/build_release.ps1` passed; independent exact-image full-catalog and
other executable gates passed. Installed local EXE:
`build/bin/Release/x64/HallJoy.exe`, 10,525,184 bytes, SHA256
90EA5CEFF1649E43E327B42DB2D126B20DBC0EDA7AE4CD31B6C090BC8C394803.
Build receipt: `.local/alumix104-raw-packets-v2-final-release-build.log`.
This version has not run on physical Alumix104 yet. No support promotion,
Sheet status change or publication follows from building a better diagnostic;
exact Alumix104 remains gray Research incomplete.

## Log46 UI-banner correction — 2026-09-28

The tester's screenshot exposed an issue missed in the first log46 review.
At uptime 37179093, `support.banner_shown incident_latched=1` and
`support.banner value=1` appeared while the same snapshot showed the exact
`alumix104-yotei` backend `present=1 connected=0`. The research worker was
active, but `connected=0` is intentional: no independent reliable per-key
analog source has been proved, so it must not drive the gamepad. The generic
UI only distinguished a supported analog source from none, then displayed
"No supported analogue keyboard detected". That wording hid the fact that
the exact keyboard had been detected. The raw-packet v2 EXE (SHA256 90EA5CEF)
still had this UI defect; raw logging alone did not fix it.

The UI now classifies exact protocol 28 / 0C45:80AC when `present=1` and
`connected=0` as a research notice. Its gray banner states in Russian that
Alumix 104 Yotei was detected, analog input is not confirmed, and the tester
should use test letters and send HallJoy.log when the window title requests it.
The separate `support.alumix104_research` transition is written to the normal
log. This notice does not set `connected`, map analog keys, enable gamepad
output, or promote support. Research records continue their bounded ordinary
log flush. The research notice alone no longer triggers the generic missing-
source incident and its automatic full support snapshot; Open log can still
request a complete snapshot. Other genuinely missing sources retain that
incident behavior. Exact104 remains gray Research incomplete; no Sheet status
change or publication follows from the UI correction.

The final ordinary `tools/build_release.ps1` completed with the support
diagnostics gate, exact-image full-catalog and other executable checks, and
verified installed/candidate image equality. The dedicated support-state
regression passed detection, no false analog connection, missing-device
fallback and logging-policy cases. Installed local EXE:
`build/bin/Release/x64/HallJoy.exe`, 10,525,184 bytes, SHA256
`A919241322876138B23D233250147E3F983E6F8C6D1F1C5C0E901547B7F0C494`.
Build receipt: `.local/alumix104-research-banner-final-build.log`. The UI
correction has code/test verification but no physical Alumix104 run yet.

## Physical log47: full packet comparison — 2026-09-28

The tester ran the exact104 raw-packet v2 path for about 60 seconds. The log
contains 301/301 complete context+65-byte HEX pairs, with zero cap skips,
emit failures, queue drops, malformed samples or out-of-catalog samples.
The diagnostic observed 58,871 valid 55FB samples, 320 letter downs and
320 ups, peak five held letters, and a clean ON/OFF ACK sequence. Gamepad
publication remained disabled by design; this was not an analog support trial.
The last snapshot still identifies 0C45:80AC as `present=1 connected=0`.

The 861 formerly anonymous reports include 687 all-zero bodies. Of the 301
captured first/changed packets, 130 are all zero and 171 contain nonzero data.
All 171 nonzero captures have byte 13 equal to 0x22 (the usual max-stroke
field, decimal 34). Every one of the 36 with a nonzero index at byte 3
matches the last selected 55FB index; all 21 with byte 2 = 0xFB do too.
Among 145 captures with nonzero stroke bytes 11–12, 102 match the last
selected stroke exactly and 135 are within ten raw units. Their shared
65-byte structure, field offsets and selected-index correlation strongly
indicate blank/partial/stale variants of the selected-key report, not a
separate per-key matrix. The source of the missing 0x55 prefix and partial
zeroing is not established by this capture, so do not ascribe it to keyboard
firmware or Windows HID specifically. No captured packet proves an
independent release signal for another held sensor.

Across 231 trace windows, 117 ended with at least two digitally held test
letters; 108/117 had fewer 55FB sensors fresh within 50 ms than held
letters. Even in all 28 windows with two or more held letters and no Raw
Input letter transition, freshness remained insufficient. This reinforces
log43's release-lag finding; digital key state stays reference only. Also,
27,989 samples exceeded the current `maximum*10` range, while five positive
samples had zero maximum. Even if a suitable independent source appears,
range interpretation must be resolved before claiming accurate gamepad depth.

The single `support.banner_shown` line occurred at uptime 40911390 after
`session.end`, exactly when this backend calls `SupportLog_RequestSnapshot()`
after cleanup. The writer incorrectly labeled any explicit snapshot request
as a shown banner. Thus that line does not establish that the new Russian UI
banner failed or that the old generic banner appeared; the raw log does not
retain a direct screenshot/UI-title observation. The writer has been changed
to emit `support.snapshot_requested source=api` for an explicit request and
`support.banner_shown` only for `SupportLog_ReportMissingSource()`. A writer
regression covers the distinction. No repeat of the same raw-packet test is
justified by this result. Exact104 stays gray Research incomplete; no Sheet
status change or publication.

Live support-status readback after log47: spreadsheet `HallJoy supported
keyboards`, tab `Main` (sheetId 0), row 644 is brand `Red Square`, model
`Alumix 104 Yotei (Magnetite Ice)`, status `Research incomplete`. C644 retains
the strict status dropdown; B644:C644 have gray base and effective colors.
Adjacent Alumix 68 row 643 remains `Not investigated`. A fresh bounded
Main!A1:C1277 read found all 252 yellow status rows identical to the local
runtime notice catalog, with no missing or unexpected models. No Sheet cell
was changed because log47 does not establish an independent analog path.

The corrected writer passed its Windows regression: an explicit snapshot
request no longer creates a false banner event, and the actual missing-source
event still does. The complete ordinary Release and exact-image full-catalog
checks passed. Installed local EXE `build/bin/Release/x64/HallJoy.exe` is
10,525,696 bytes, SHA256
`96BA5A1BFD9DBD08D3DCCA3B148F3C1521B8F6524BDD3EFBCFC3C33619F56BF5`;
candidate hash matches. Receipt:
`.local/alumix104-log47-writer-final-build.log`. This is a logging correction,
not a new analog method or a reason for another identical hardware test.

## Firmware comparison with AULA MINI60 HE Pro — 2026-09-28

Compared the exact Alumix104 0C45:80AC **partial** physical capture
`.local/alumix104-review/log38-code-window.bin` (53,248 bytes, flash
`C000..18FFF`, SHA256 `0e3daac352b39edfd2def4dc54ed4eba74f016121df63ec854ae2e8e5e8d3c5e`)
with the complete AULA MINI60 HE Pro **V1.55** updater resource 4000
`.local/aula-mini60-pro-155-resource-4000.bin` (516,096 bytes, SHA256
`c070e514ff1bef20a71abff12a6c30b04f152892b0fa22e1b5e0709be63eb7cd`).
The AULA updater also embeds a different HEX image; this comparison uses resource
4000 only. The installed revision of the physical Alumix104 was not read, and
the Alumix capture is not a complete firmware image. Read-only local byte/code
analysis only: no flashing, device commands, HallJoy build or support change.

An exact 16-byte seed with at least eight distinct byte values was searched
through the AULA image; matching runs were extended and overlapping Alumix
source ranges counted once. Runs of at least 32 bytes cover **17,602 / 53,248**
captured Alumix bytes, including 15,447 bytes in `C000..13FFF` and 2,155 in
`14000..18FFF`. At 64-byte minimum the coverage is 13,448 bytes. The same
method against the neighboring RSQ-20058/Alumix68 v1.30 image yields 16,820
and 12,776 bytes respectively. Examples of Alumix104 -> AULA identical runs:
`115EA -> 1216A` (716 bytes, common Thumb code), `121CA -> 12D4A`
(542 bytes, code), `154DA -> 166CE` (1,162 bytes, mostly table/data),
`15AB2 -> 16CCD` (1,198 bytes, table/data). Thus these images share substantial
compiled code/data, but several longest runs are not the analog producer.
Same-address byte totals are not used as a similarity percentage because zero
fill and shifted functions would distort it.

The vendor command dispatchers are structurally related but not interchangeable.
Both have `AA` request / `55` reply handling and `64..67` branches; `66/67`
toggle a transient simulation flag. AULA's wired dispatcher starts at `E344`,
its mode branch is `E6C0..E6F8` and simulation flag is `20000475`;
Alumix104 starts at `E388`, its mode branch is `E6D8..E710` and simulation
flag is `200002FE`. Crucially, AULA's `16` branch at `E46E` selects RAM base
`200025B2`, while exact Alumix104's `16` branch at `E4CC` selects flash
`B000`. An AULA/neighbor RAM depth-read offset cannot be copied into the
Alumix104 command. This confirms a functional difference in the route that
would otherwise be promising for independent per-key snapshots.

AULA V1.55's normal scan/report tail at `2D82..2DB2` serializes the current
position and calls USB send for it when its ADC crosses the near-rest threshold;
the report builder is `BF8`. The already reviewed Alumix68 v1.30 instead
selects one key in simulation (`4C80..4C98`, tail `4E1C..4F26`). These two
complete, related images demonstrate that shared SDK, packet layout and even
substantial matching code do **not** imply identical multikey analog output.
The Alumix104 window begins at `C000`, so it omits both lower report/scan
regions. Its physical log43/log47 selected-key freshness failures remain the
direct evidence about the tested path; AULA's per-position producer is not
proof that exact104 contains the same behavior or that its missing reports can
be reconstructed. The downloaded RSQ-20058_V1.30.hex is an Alumix68 image,
**not** a flashable or complete firmware file for Alumix104.

This comparison provides a useful template for locating the scanner, report
producer, USB scheduler and RAM depth table if exact104's lower firmware bytes
become available. Until then, retain Research incomplete and do not route
Alumix104 through AULA's analog algorithm or treat absent per-key packets as
release. The stock 55FB path's multikey/release failure is not solved by the
shared code. No Sheet synchronization is triggered because the support
conclusion is unchanged.

## Standalone local research archive — 2026-09-28

Owner requested a separate, self-describing Alumix104 Yotei research ZIP.
The bundle at `C:/Users/PC/Downloads/Alumix104-Yotei-Research-2026-09-28.zip`
contains physical logs 37–47, this evolving report, pinned local firmware
fragments and comparison images, vendor-client evidence, replay results and
scripts, current research-specific source/tests, historical research snapshots,
verification receipts, a Russian index and per-file SHA256 manifest. It
contains no HallJoy executable, installer, command script, screenshot or chat
image, per the owner's clarification. Vendor artifacts and raw HID logs remain
private local research material; the archive is not a release or authorization
to redistribute firmware. The adjacent RSQ-20058 HEX remains Alumix68 only,
not an Alumix104 firmware image. Support stays Research incomplete.

## Official firmware V1.11 (user report "updated to 1.11"), 2026-10-23

Source: the Red Square web configurator (`app.red-square.org`, bundle `layout-classic-nWRG903k.js`) no longer uses
`update.json` for firmware; it queries `https://cp.driveall.cn/api/device/firmware/index` with
`supplier_name=rsq, connect_type=USB, vid=3141, pid=<pid>, product_name=<name>`. For PID 32940 ("Alumix 104 Yotei
Magnetic"; the same PID is also used by Alumix TKL Horizon / TKL SE configs) it returns V1.11, uploaded 2026-09-24,
file `CZ26032_黑岩HK1081G_单模ARGB磁轴_上灯位不开槽_HFD8KCZ700_20260907_1.11_0x3F01(1)(1).hex`. The same API also lists
RSQ-20058 (Alumix 68) V1.37 / V1.31 / V1.07 (HallJoy's Alumix 68 path was built on V1.30). `update.json` itself now
lists only RSQ-20058 V1.30 and "Alumix 104 Arita WL" (PID 32972) V1.24, a different model.

First exact Alumix 104 image ever obtained. Local only (vendor firmware, no redistribution):
`.local/research/alumix104-fw111-20261022/Alumix104_Yotei_V1.11.hex` SHA256
`0affd04829f752f14d560ff062148c0ce432e7efcb17f55e49034ab35ab4ae9e` (0x7E000 bytes from 0), and
`RSQ-20058_V1.37.hex` SHA256 `87da92aea8e7dd90cb7b0ae4282fd13a43b2d2e16768164cdb1887fd4075bf58`.

Static review (Capstone, Thumb), compared with RSQ-20058 V1.30 and V1.37:
- AA command dispatcher at 0xE2DC (V1.30: 0xE40C, same structure). Groups: 0x10-0x18 and new 0x1C, 0x21-0x28,
  0x32/0x33, 0x40, 0x64-0x67. **No 0x60, 0x68, 0x69, 0x6A**: `GET_MAGNETIC_AXIS_STATUS` (0x68) is still not
  implemented, which matches the echo-only replies in logs 44-47.
- Read commands copy `len` bytes from `base + (b4<<8 | b3)`: 0x10 0x9000, 0x11 0x9200, 0x12 0x9600, 0x14 0x9A00,
  0x15 0x9C00, 0x16 0xB000, 0x17 0xB600, 0x18 0xB200, new 0x1C 0xBC00. All bases are flash and a 16-bit offset
  cannot reach SRAM (0x2000xxxx), so there is still no RAM depth-table read like the one found on Alumix 68 V1.30.
  0x1C is one more flash configuration page.
- 55FB report builder at 0x08E8 is byte-for-byte the V1.30 builder (one key per packet: index, calibration status,
  max, min, ADC, travel, max travel). Its four callers (0x4594/0x45C2/0x460E/0x4648) keep the V1.30 rule: a packet
  is produced only when the scanned index equals the selected index and simulation (0x66) is on. The stream is still
  selected-key only; held keys that are not the selected key are not reported.

Conclusion: V1.11 adds no usable multi-key analog source. The blockers documented above (selected-key 55FB stream,
0x68 not implemented, no RAM read) are unchanged; status stays gray "Research incomplete", no runtime change.
