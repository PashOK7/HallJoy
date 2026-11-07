# CORSAIR VANGUARD PRO 96: native integration and tester build

2026-11-02. Ordinary test EXE built and installed. Experimental stock analog on
firmware 2.8.171, ANSI/ISO/JIS; no physical test or GitHub publication. Earlier
research checkpoints below are historical; final delivery evidence is at the end.

## Exact device and tester evidence

Owner requested support after supplying `message (21).txt` (a HallJoy log,
not executable instructions). The exact USB model is `1B1C:2B0E`, product string
`CORSAIR VANGUARD PRO 96 Hall Effect Gaming Keybo`. The log is from HallJoy
1.6.14.0; it contains no admitted analog source and no current Corsair backend.
UAP is ready with no reported UAP error. This does not establish a driver, cable,
binding or gamepad fault. It does not report the keyboard firmware version.

Log SHA-256: `ef617d167147f72d3cca5c618d97712e3b7d0af7780daff64943cafeb6e3b106`.
Private input/copy remains local. Do not extend this identity to the GUMA edition,
VANGUARD 96 Wireless, K70 MAX, or other Corsair products.

## Applicable integration rules, reviewed at owner's request

Both AGENTS.md files and the active owner context apply. In particular:

- Exact identity and protocol proof precede admission; connected requires live
  independent analog. Digital/time-based substitutes and guessed fallback curves
  are forbidden. A vendor name, ADC-looking bytes or protocol enum is insufficient.
- Follow [firmware behavior review](../development/FIRMWARE_BEHAVIOR_REVIEW.md):
  pin sources, execute relevant branches, compare alternatives, and document every
  behavior finding. Unknown is not PASS. Execution PASS is not suitability.
- A reviewed limited analog route may be retained with explicit restrictions;
  an unrestricted route and a separate physical test per model are not mandatory.
- Yellow requires the complete enabled detection/analog/binding/gamepad path.
  Research-only code is not Experimental support. Supported additionally requires
  a built-in layout, supported_layouts.json and compiled-catalog/Sheet gates.
- Follow [support synchronization](../development/SUPPORT_STATUS_SYNC.md) when
  a conclusion changes: runtime, README, hardware evidence, current record, and
  live Sheet including readback. Local runtime/docs now mark the implemented path
  experimental; live Sheet sync is explicitly pending (connector unavailable).
- Deliver one ordinary HallJoy.exe and collect the ordinary HallJoy.log via Open
  log. No tester scripts/flags/archives, hidden simulation or forced logging merely
  because support is limited. Use standard build/lifecycle checks once with cache.
- Preserve fresh file contents; back up broad changes. Owner assesses visuals.
  Local compilation is separate from GitHub publication authorization.

## Pinned official reference

Official Web Hub identifies `1B1C:2B0E` as VANGUARD PRO 96 with Bragi, wired USB,
multisession control and configuration reads without requiring host-control mode.
Sources: https://www.corsair.com/web-hub/index.html and its fetched
`assets/B1aWXCAu.js`; firmware storage public manifest
`manifests/vanguard-pro-96_1b1c_2b0e_manifest.json` and metadata
`fw/vanguard-pro-96_1b1c_2b0e.json` under the official firmware-storage public base.

Reference application: 2.8.171, package VANGUARDPRO96_2.8.171.zip, published by
vendor on 2026-09-03. Package and image SHA-512 matched vendor metadata.
`fw-VANGUARDPRO96_App_v2.8.171.bin`, 1,905,760 bytes, SHA-256
`72131be2bb9509a609067d5ceb76575fb023fdef0e9f968108064351b9ae87f7`.
Thumb image base `0x08020000`; reset vector `0x08020395`.
This is a reference image, not proof of the tester's installed version.

All acquired vendor assets remain local; no redistribution permission established.
Evidence directory: `.local/research/corsair-vanguard96-20261102/`.
`research-manifest.json` pins source/input/script/result hashes and dependency
versions. `replay_probe.py` is the initial narrow harness, not a production module
or a completed behavior assessment. Reuse it rather than repeating discovery.

## Findings and important corrections

The older [K70 MAX report](IPI_FLASH75_AULA_AG75_CORSAIR_K70MAX_2026-10-12.md)
examines different firmware/identity. Its bounded negative result does not rule
out Vanguard analog. Web Hub's AnalogKeyPressChange=12 enum also does not by
itself prove any producer or usable event stream.

Vanguard dispatcher `0x08066FA0` uses Bragi 1.5's four-byte header. Commands 1/2
set/get properties, 8 reads an open file, 9 describes, 13 opens, 27 manages sessions.
The 1024-byte protocol payload requires accounting for Windows' report-ID byte.

- Property read `C7`, command 2, branch `0x080671AC..0x08067224`, serializes each
  current 16-bit value from `0x20051EA0 + 64*index + 0x0E` to reply offset 5.
  These are current sensor readings, not already normalized millimetres.
- Property read `C6` must be classified by subcommand. An early broad suspicion
  that every C6 branch starts calibration was incorrect. Table at `0x08067148`:
  1/2/3 read calibration flags/bottom/rest from EEPROM; 4/5/6 read the corresponding
  data from saved `ID00CA.hex`; 16 enters per-key calibration and is excluded.
  Read helper `0x080766E8..0x0807688C` reads and serializes, whereas calibration
  helper `0x08076890` changes scan/calibration state and waits for samples.
- Saved calibration reads open ID00CA.hex with flag 1, read 719 bytes and close.
  Loader `0x08071B84` also selects EEPROM versus the saved file. Its actual active
  selection, missing/invalid file behavior and switch overrides need review.
- C6 serialization starts at reply offset **3**, overwriting the usual command
  and status positions. A generic Bragi success/echo decoder would reject or
  misinterpret it. Do not accept arbitrary bad replies: resolve exact framing,
  matching/session validation and cross-command ambiguity before runtime use.
- C6/C7 set `0x200002F1=1`, call a bounded delay, then restore it to zero.
  Scan code at `0x080645F4` observes this flag. Thus a claim that reads cannot
  affect scanning/typing would be premature; timing remains unmeasured.
- Host open-file rejects ID00CA directly (branch `0x08072C28`); C6 is a separate
  legitimate read path. Some other open-file IDs create defaults when missing,
  so their names/read-file APIs do not establish absence of persistent writes.

## Initial branch execution

`replay-read-branches.json`: four cases (C7 and C6/4, /5, /6) execute the pinned
full dispatcher from `0x08066FA0` to return sentinel `0x08020000` using the shared
strict Thumb replay core. Synthetic RAM: 96 slots, session 1, rest 6000+i,
bottom 2000+i, current 5000+i and distinct flag bytes. Saved-file open/read/close
are explicit stubs; read returns a synthetic 719-byte record. No ADC/MMIO, real
USB, boot or scheduling is modeled. Instruction budget 150000 per case.

PASS: all expected output bytes match, C6's offset-3 behavior is reproduced,
C7 uses offset 5, and the scan flag returns to zero in all four completed cases.
C6 file calls are read-only open/read/close in these executed paths. Recorded
non-stack writes include the reply/output area, session timeout refresh, ready
flag and scan gate. These are scoped results, not proof of global side-effect
freedom or crash recovery. The initial 96-slot fixture is not a count/layout
validation; real layout variants require their own tests.

## Initial checklist (see expanded review below)

1. Resolve active calibration source, switch-specific transfer curves, scaling,
   raw fault sentinels and safe error/fallback behavior. Do not hardcode synthetic
   fixture endpoints or derive depth from OS key-downs.
2. Prove physical key mapping per hardware layout. Candidate forward table is
   used by `0x0805A5A4`; factory curve selector `0x08070474`; normalization code
   `0x08067A4E`/`0x08067AAA`. Table-like constants are not a reviewed map yet.
3. Extend replay across real layout counts, all admitted keys, rest/shallow/full
   and release, malformed/missing calibration, session/reply rejection and failures.
4. Complete the behavior matrix: typing, multikey, range/resolution, shallow
   input, filtering, releases, mapping, packet loss, persistent settings, normal
   exit, crash recovery and configurator contention. Choose the best usable route
   with explicit limitations rather than discarding a useful limited route.
5. Implement and test the complete native lifecycle, telemetry and normalized
   input path, then layout/status synchronization and the ordinary release build.

Production code, README compatibility, hardware support statuses, Sheet and EXE
are unchanged by this research. No firmware flashing or real HID commands were
performed. The original support task remains in progress.

## Selected route after expanded review, 2026-11-02

A concrete stock-firmware analog route has now been established far enough to
select it for implementation. Formal assessment: **LIMITED_ONLY**, selected ID
`snapshot-with-read-calibration`. This is an offline protocol selection, not an
implemented keyboard, a Supported/Experimental promotion, or hardware feedback.
The initial checklist above is superseded by the following findings and gates.

### Reproducible evidence

- `tools/corsair_vanguard_firmware_audit.py`: exact-image C6/C7 execution, all four
  sensor-count variants, per-layout index/curve selectors, and normalization.
- `tools/corsair_vanguard_transport_audit.py`: actual host file dispatcher and
  directory builder, official layout literals, saved switch-file transport and
  a blocked persistent-write boundary. Neither tool accesses HID.
- Local `review-v3/`: 48 calibration command/error cases and 4134 boundary
  comparisons covering 18 transfer tables and two calibration spans, PASS.
  Normalization starts at 0x08067A4E and stops before later per-key filtering.
  Equivalent normalization classes are executed once rather than repeating the
  same fragment at every index. Full C7/index selection still covers each layout.
- Local `transport-review/`: seven transport stages PASS, including actual
  directory open/describe/read and switch-file open/describe/read. Missing
  switch-file open reaches the persistent-write API and is deliberately blocked;
  that is a reproduced hazard, not a successful write or safe branch.
- `calibration-loader-replay.json`: eight actual loader cases PASS, both sources
  on four layouts. `lifecycle-replay.json`: three cleanup scenarios PASS.
- `snapshot-sequence-replay.json`: actual version/property framing from explicitly
  initialized RAM, then seven snapshots with shallow input, two-key hold and
  independent releases. No real boot/ADC/USB or physical depth is claimed.
- `assessment-v1.json` and `BEHAVIOR_REVIEW.md`: complete behavior matrix and
  candidate comparison; firmware_behavior.py validates the hash-pinned evidence.
- `reviewed-protocol-parameters.json`: reusable reviewed maps, curves, framing,
  calibration selection and restrictions. Vendor inputs remain local.

The initial v2 all-index numeric sweep was stopped before completion because it
repeated identical transfer classes; its incomplete directory remains preserved
without a completion manifest. It is not counted as a PASS. The v3 completed
bundle pins its own exact source. A later small harness fix handles 16-bit
property IDs; it does not silently replace the source pinned by that bundle.

### Initialization and sampling contract

1. Admit only exact VID/PID 1B1C:2B0E. Discover the control interface, expected IF2
   with a 1024-byte Bragi payload. Account for Windows' separate report-ID byte;
   verify actual HID caps in the backend and log structural admission failures.
   Do not use an uncertain usage-page number as a substitute for identity.
2. Open this control interface exclusively; if another configurator owns it,
   report busy. Do not fall back to shared access. Prove VID/PID, firmware 2.8.171,
   Bragi 1.5 and hardware layout with ordinary property reads. Unreviewed firmware
   gets identification/diagnostics, not guessed C6/C7 admission. The tester's
   installed version remains unknown until runtime reads it.
3. Start an owned, nonzero temporary session. Use file 0x11 for the directory:
   opcode 13 opens, 9 describes, 8 reads. Its format is u16 0xAB5B, u16 count,
   then count u16 file IDs. Close the directory handle with opcode 5 immediately
   after reading it; this frees its 8004-byte snapshot allocation.
4. Choose saved calibration when ID00CA exists, otherwise EEPROM, matching normal
   firmware startup selection at 0x0805F4CA. Read rest/bottom with C6 subcommands
   6/5 (saved) or 3/2 (EEPROM). Validate complete per-key records, source stability,
   nonzero span/scale and raw range. Missing/error data from the chosen saved
   source is a failure, not permission to invent defaults or switch sources.
5. Mirror the actual loader: EEPROM mode subtracts 200 from sensor 6's bottom
   before calculating its scale; saved calibration does not. This was confirmed
   by executing 0x08071B84. Synthetic fixture endpoints must never become defaults.
6. If ID004B exists, read its switch settings through file ID0x4B. If absent,
   do not open it: the open handler would create a default persistent file.
   Use the actual firmware factory selector for the absent-file case.
   Existing switch settings start u16 0x0067, u8 count, then four-byte records
   [protocol key index, switch type, custom max distance, reserved].
7. Apply records in firmware order using the reviewed switch selector. Explicit
   switch type 5 is not always identical to the absent-file factory selector
   (e.g. numpad Add/Enter and ISO Right Shift). Include firmware alias effects
   when calculating per-sensor curves; bind only genuine physical keys.
8. **Do not individually close the switch-settings handle with opcode 5.**
   That path closes, reopens and rereads ID004B, applying a switch reload even
   after read-only access. Instead end the temporary session with opcode 27,
   payload [2, own four-byte token, own session ID]. Actual session cleanup
   closes the file and frees the filename without the settings reload.
9. Start the ordinary sampling session and poll only C7. Decode all current
   sensor words at offset 5, then apply the acquired per-key calibration and
   reviewed transfer table. A complete live valid snapshot precedes connected.
   Use synchronous paced requests, bounded/cancellable I/O and immediate neutral
   publication on connection failure; never replace data with digital key states.
10. Close the sampling session on Pause/Exit. Do not start hardware calibration
    or edit switch settings while the session is active; reacquire on resume or
    reconnect. Runtime lifecycle/error handling and tests still need implementation.

### Framing, mapping and depth details

C6 remains exceptional: its calibration bytes start at offset3 and overwrite the
normal command/status fields. Require the owned session prefix, exact firmware,
expected payload/tail and valid calibration; do not globally relax Bragi replies.
Allow one in-flight command. A timeout aborts initialization/session rather than
allowing a late C6 response to be consumed by the next request.

Official Web Hub view `assets/ClyRZVhU.js` (SHA256
`b1374adb77b66b61441c82158507e8d5e3dd5ee9524e731a34231f537346b20b`)
imports inert row definitions from the pinned main bundle. The local literal
parser follows only data literals/references/spreads; no vendor JS is executed.
After excluding Game Mode/G1..G5 (officially non-actuation), executed maps yield:

| Layout | Physical analog keys | Sensor slots | Result |
| --- | ---: | ---: | --- |
| ANSI / ID1 | 96 | 98 | one-to-one |
| ISO / ID2 | 97 | 99 | one-to-one |
| BR / ID3 | 97 literals | 98 | hold: N and IntlBackslash alias sensor0 |
| JIS / ID4 | 98 | 100 | one-to-one |

Two slots per admitted variant are not mapped to physical keys and must not be
bound. Sensor0 **is** valid N; other unused protocol IDs also map to0. Presence
comes from the exact physical-key set, not from a nonzero-index heuristic.
USB modifiers are vendor codes105..111 -> HID224..230. Vendor115..119 correspond
to international keys; Fn and StreamDeck require deliberate existing private
HallJoy identities, not invented standard USB usages. A visual built-in layout
and automatic-layout publisher are still pending.

The reference firmware converts rest/raw difference through a key/switch-specific
41-point table. Scale is integer((rest-bottom)/100); depth is the greatest n in
40..1 for which difference > floor(table[n]*scale/10), otherwise0. Host 0..1000
output is n*25 on the agreed 4mm scale. Preserve shorter switch travel/end-deadzone
behavior rather than stretching it to full scale. Some tables are non-monotonic;
do not replace the executed descending lookup with an assumed monotonic search.
This establishes the firmware's nominal 0.1mm conversion, not sensor resolution.
C7 exposes finer raw values, but finer host conversion needs its own evidence.

### Demonstrated cleanup distinction and remaining limits

The ordinary handle-close scenario reloaded the switch file and changed100 bytes
of synthetic key-processing state. Session-close produced only close/free calls
and zero changes to that state. A modeled timer edge expiring the session also
produced close/free without reload or key-state changes. The actual tick period,
complete crash recovery and USB reset recovery remain unmeasured.

The read route briefly gates sensor scanning; no continuous diagnostic mode or
host-control switch is required. Real typing coexistence, latency, ADC filtering/
freshness, USB backpressure/loss and Windows/configurator contention remain
unknown or restricted in the assessment. Keep them explicit in the eventual
runtime notice and tester instructions. Do not present this route as unrestricted.
Exclusive Windows ownership and preflight are implementation requirements, not
completed host tests. No arbitrary device-memory reads, calibration-start commands,
firmware writes, mode writes or real HID transactions occurred during research.

### Historical pre-implementation checkpoint

- Native backend, exclusive overlapped transport, strict session/calibration/frame
  parsers, complete key ownership, normalized samples, cancellation/neutralization,
  meaningful telemetry and bounded structural diagnostics in ordinary HallJoy.log.
- Admission, malformed/stale replies, missing/invalid calibration, disconnect,
  Pause/Resume and busy-access tests against production host code.
- The normal notices/catalog currently use all32 bits of their model mask; review
  a compatible extension rather than silently repurpose another model's warning.
- Built-in layout work via the existing layout pipeline, then the appropriate
  status decision and mandatory runtime/README/hardware/Sheet synchronization.
- One cached native suite and one ordinary release build after runtime changes.
  Do not compile an unchanged EXE just to label a research result as a new build.

At that research checkpoint no native input code, compatibility claim, support
status, Sheet value or EXE had changed. The implementation and final delivery
below supersede that checkpoint; its selected limited route was retained.

## Native implementation — delivered ordinary tester build

Owner explicitly requested continuing through a ready ordinary EXE, without
another stop at research. Native protocol33 (`corsair_vanguard`) is now integrated
through the common manifest and native input pipeline. Production code uses the
reviewed temporary read-session/C7 sampling sequence; calibration is never guessed.
Starts AfterRealtime because the embedded UAP has no Corsair route. Every changed
snapshot and disconnect wakes the common realtime loop. No special ViGEm branch.

Admission is exact VID/PID/control interface, opened attributes and caps, nonzero
owned session/nonce, firmware2.8.171, Bragi1.5, supported layout, full valid per-key
calibration, stable file directory, safe session cleanup and a live whole snapshot.
Foreign exact-path claims are rejected before any device open. Busy opens retry;
rejected proofs are remembered until observed bus absence or a fresh Resume.

Added48-bit notice capacity inside the existing atomic64-bit status snapshot,
preserving all old notice bits and the separate8-bit unsupported catalog index.
The connected Corsair advisory is yellow and does not force logging; failed
admission has its own pending notice and ordinary missing-source/Open log reporting.
No old model warning bits were repurposed. High-bit/index regression is included.

The pure production client passes complete source/layout combinations, releases,
unsafe-command exclusions and malformed/admission cases. The actual Win32 transport
and production Run session pass with fake I/O, including first-live ownership,
busy/caps failures, wrong report IDs, partial reads, timeout cancel/drain and
neutralization. These are software fixtures, not new hardware feedback.

Detailed native contract: [CORSAIR_VANGUARD_PROTOCOL](../development/CORSAIR_VANGUARD_PROTOCOL.md).
Exact visual presets/automatic layout are explicitly deferred: compound ISO/JIS
contours and device-specific Web Hub sizing still need a faithful import. For this
test use manual layout selection; the native keys use factory positions. This
prevents a Supported promotion and does not disable the analog/bindings/gamepad path.

README, hardware evidence, next notes and runtime notices reflect local untested
implementation. Live Sheet sync remains pending: no native Sheets connector is
available in the current tool set. Exact model CORSAIR/VANGUARD PRO96 should become
Implemented; awaiting hardware testing when access is restored (find live row by
brand/model, preserve validation/base/effective colors and read back; no guessed row).

Backup before runtime/notice edits: .local/backups/corsair-vanguard-native-20261102/
(31 verified originals plus additional filter and pinned-reference backups).

## First delivery and validation (revision 1, superseded below) — 2026-11-02

- Installed ordinary EXE: `build/bin/Release/x64/HallJoy.exe`.
- Version resource: 1.6.14.0 (local unreleased test build); 11,304,448 bytes.
- SHA-256: `b7ead6f0775bc09be6ed37fc191645dd2f2dacc386044b26d90d7478cad6c954`.
  Installed bytes match the linked and tested candidate; differ from the prior
  published 1.6.14 EXE (`762d1a7b...`). Preserve the hash to distinguish them.
- `tools/run_native_backend_checks.py --require-compiler`: PASS with exact-input
  cache. Includes the production Corsair client (12 complete source/layout/switch
  sessions plus large directory and rejection cases), actual Win32 transport/Run
  lifecycle with fake I/O, and old/high-bit notice coherence.
- Changed pinned notice catalog initially failed the reference-hash gate; updated
  it using `research_reference_checks.py --record` after backing up the manifest.
  All 19 private reference audits passed; the subsequent full suite passed.
- `tools/build_release.ps1`: exit 0, 157.9 seconds. Independent full catalog and
  retained-feature/image self-tests, diagnostics and embedded licenses passed.
  Production-linked profile/layout transactions passed; 86 Supported models have
  built-in layouts. All 282 experimental models were audited: 78 existing, 41
  complete, 4 partial, 159 missing visual presets. Corsair is explicitly missing.
  Startup recovery passed 18 scenarios plus repeated startup. The standard
  publisher replaced the exact target and restored the previously running window.
- Evidence: `.local/research/corsair-vanguard96-20261102/native-suite-2.log`,
  `release-build-1.log` and `tester-build.json`. The build log identifies the
  corresponding production layout evidence directory.
  All checks are software checks; no hardware throughput/typing/USB claim.
- Runtime, README, hardware record and next release notes synchronized locally.
  Live Sheet remains pending: CORSAIR / VANGUARD PRO 96 -> Implemented; awaiting
  hardware testing. No native Sheets connector or connected document session was
  available. No Sheet write/readback, Supported promotion or GitHub publication.

Tester receives only the EXE. Close WEB HUB/other keyboard configurators, connect
by USB, and use manual layout selection. Firmware must be 2.8.171; do not assume
it from the old log. Enable logging to retain session diagnostics, test slow
and simultaneous presses in the existing gamepad view, and return HallJoy.log via
Open log if admission or motion fails. Pause before changing switches/calibration
or settings and resume afterwards. BR remains excluded; analog uses the reviewed
0.1 mm steps on the 4 mm scale. Physical coexistence and timing remain unverified.



## Admission and diagnostic review - revision 2

Owner challenged whether the first build could silently refuse its own target
or return an uninformative log. The first build had concrete gaps: parser false
returns could carry only numeric stage and zero transport status; metadata query
failures were remembered like incompatible devices; generic history could lose
initial diagnostics. This review does not establish that hardware admission will
always succeed or that the emulated firmware covers every actual USB behavior.

Revision 2 adds named local refusal reasons, observed/reviewed version and layout,
request/transport details, exact-product/IF2 discovery counts and retained evidence
inside the ordinary Open log flow. Transient HID metadata/flush failures retry;
actual incompatible descriptors/protocol proof still reject deliberately. Failed
partial enumeration cannot falsely establish device absence. Periodic/end travel
summaries separate changed raw sensor slots, normalized positives, releases and
simultaneous motion without logging keys or raw depth values. Stage telemetry now
tracks each client transition rather than remaining Open through initialization.

The actual writer accepts only 384 bytes per evidence line, so failure, transport
and admission context are separate records below 330 bytes with a shared attempt
ID. Tests enforce that production bound. The writer regression intentionally
rolls ordinary history in an isolated session, proves no forced file when logging
is off, requests Open log and verifies the Corsair reasons survived in the file.

Fault injection covers production request/parser failures and recorded reasons,
wrong report ID/length/timeouts, metadata retries without commands, metadata-only
discovery and raw motion below the first normalized step. The initial expanded
writer test evicted a legacy fixture's expected history; it was moved to its own
session, preserving the original retention test rather than weakening its checks.

`native-diagnostics-v2-2.log`: full cached native suite PASS, 110.6 seconds;
production client/Win32 session and actual normal/diagnostic writer both passed.
Final ordinary build PASS (exit 0, 127.4 seconds), including exact-image full
catalog, diagnostic/legal, production profile/layout and 18 startup-recovery
scenarios plus repeated startup. Installed and restored by the standard publisher.

Revision 2 tester EXE: `build/bin/Release/x64/HallJoy.exe`, 1.6.14.0,
11,311,104 bytes; SHA-256 `85f159f0c58f2efb595e359ad4be386875b77aa74523e2fbb207376777ff0e7e`.
Installed bytes match the tested candidate and supersede the revision 1 hash above.
Evidence: `release-diagnostics-v2-final.log`, `native-diagnostics-v2-2.log`,
`corsair-final-targeted.log` and `tester-build-v2.json` in the local research folder.
Production profile evidence: HJProfileTest-8d46adfe33d949f9a483ba5a68f6bcb4.

A final sparse-milestone change makes short tests useful immediately, rather than
waiting 30 seconds: first valid frame/raw motion/positive depth/release/multikey
summary, each at most once. The production Win32 session check passed again after
that narrow change; the final ordinary build compiled those exact final sources.
The preceding intermediate build passed but predated that last logging change;
its binary is not the final tester artifact. No further input code changes after
the final build. No physical test, Supported promotion, Sheet sync or publication;
previous pending Sheet item and firmware/layout restrictions remain explicit.


## 2026-11-03: Google Sheet pending item resolved

Google Drive/Sheets read and write access verified after the owner's connection.
Existing Corsair / VANGUARD PRO 96 row Main!C183 changed Not investigated ->
Implemented; awaiting hardware testing. Base/effective yellow and strict dropdown
preserved/read back. Full structure and 86 Supported / 283 yellow reconciliations
PASS. No native C++ header or EXE change. See
[the Sheet record](SHEETS_RECONNECT_2026-11-03.md). Earlier pending notes describe
the 2026-11-02 build checkpoints; they are superseded by this verified readback.


## 2026-11-04: first tester log stops at exclusive open

Tester input message (22).txt is a schema-2 HallJoy 1.6.14.0 report with
`evidence.corsair.start revision=2`. The retained discovery found six collections
for the exact USB product and one control IF2 collection, with zero metadata
errors. The first open returned Win32 32 (ERROR_SHARING_VIOLATION). Both live
provider snapshots have access_busy=1, connected=0 and no samples. Latest
coverage is 27/27 and the source summary is consistent.

The handle failed before capability checks, Bragi commands, firmware identity,
calibration or depth reads. Windows reported an incompatible open handle. The
log cannot identify its owner, and Corsair virtual input entries in inventory
are not ownership evidence. The first open failure is logged per error episode
while the worker retries roughly once per second.

Ask the tester to close the CORSAIR WEB HUB tab and exit iCUE from the tray if
running, then leave the same build active and let it retry. If the busy state
persists, disconnect and reconnect USB with the configurators closed. Capture
a fresh HallJoy.log through Open log after the next attempt. Look for caps,
identity and travel evidence or the next precise failure stage. Preserve the
reviewed exclusive-access policy during the preflight and session. The attempted
test did not reach analog validation, so the existing experimental status applies.

Private evidence: .local/research/corsair-vanguard96-20261104-log22/.
Input SHA-256: `566bb015f9da5a344d99303191c5127150c182776efdf8d2e7e4f899b0b4d01b` (46,159 bytes).


## 2026-11-04: second tester log confirms live ISO snapshots

Message (23).txt reaches the full Bragi admission on diagnostic revision 2.
Windows reports FF42:0001 and 1025 bytes in both directions. The device reports
stock 2.8.171 and layout ID 2 (ISO). Both sessions accept saved calibration for
97 keys and read the existing switch settings. The final complete snapshot
records 5,026 valid updates and zero backend failures.

The first session ends during a device-change notification burst. The cleanup
reply succeeds and the second session starts sampling 31 ms after the cleanup
record. The notification does not identify the affected device. The second
session's 2,995-frame summary shows nine positive key samples and nine releases,
with a maximum of one positive key per frame. This confirms live depth decoding
and return to zero. Sustained holds, usable range and simultaneous input remain
to be tested. The pad trace shows neutral output and the second session's
configuration snapshots have no bound axes, triggers or buttons.

For a functional follow-up, bind two known keys to separate axes on one pad.
Hold each key for two seconds, move slowly through its travel, release, then
hold both together. Check the virtual pad and ordinary typing while HallJoy is
active. Capture Open log and include what the tester observed.

Keep the experimental classification pending the functional test and exact
visual preset. Live Main!C183 readback on 2026-11-04 verified Implemented,
awaiting hardware testing, strict validation and yellow base/effective colors.
The compiled reader and its admission limits continue to apply.

Private evidence: .local/research/corsair-vanguard96-20261104-log23/.
Input SHA-256: `15aea03f0326601fa4e950ad66902f5cec2ae6e4a3665e78f94b1dab8a52f9b3` (90,331 bytes).

## 2026-11-07: built-in layouts for v1.6.15

The exact CORSAIR WEB HUB 2B0E view provides row anchors, ordered keycap
sizes, arrow and numpad groups and the ISO/JIS Enter shape. The reviewed
reports contain normalized key rectangles and the Enter notch, checked
against every nonzero identity in the physical sensor map. The 96/97/98
ANSI/ISO/JIS positions include Fn and the applicable Stream Deck key.
Separate software controls have no analog mapping. Vendor bundles and
artwork stay in local research.

Web Hub input SHA-256 values are
`3ac5e45206ec55cd677c21f938ea5524446a1e4554f91bdabe8b94dae6999b27`
for the main bundle, `b1374adb77b66b61441c82158507e8d5e3dd5ee9524e731a34231f537346b20b`
for the keyboard view and `bedf65b29677a33a10fd9cc2680435e3dc9faae50e9766f4f3e504e6c2f9cb8d`
for the keycap view.

The Bragi region selects one of three exact product tokens. Telemetry
publishes it only during a connected session after the first valid depth
snapshot. The production Win32 regression covers each region, no token before
proof, and cleanup after disconnection. Brazilian region 3 retains its
existing exclusion. The ISO hardware log validates depth and release. The
complete controller exercise is pending, so VANGUARD remains Experimental.
