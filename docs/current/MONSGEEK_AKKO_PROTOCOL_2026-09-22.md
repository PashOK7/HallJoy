# MonsGeek / Akko protocol work — 2026-09-22

Status: active research; runtime integration and support-status synchronization pending.
Owner authorized native support for established protocols with an untested/yellow
notice. Emulation is a tool choice, not a required substitute for sound analysis.
No physical tester is required solely to award support. No publication requested.

## Sources and acquisition

- jackuson14/keypad-gamepad, commit 26becddfde2888bc74a22e4440667f2c9b3f5686:
  upstream M1 V5 HE event-stream evidence; see KEYPAD_GAMEPAD_REFERENCE_2026-09-22.md.
- echtzeit-solutions/monsgeek-akko-linux, commit
  79c45f11dcc0fdd24d6075a0f30f5dabbc748c9b, local research clone.
- Official POST https://api2.rongyuan.tech:3816/api/v2/get_fw_version with dev_id;
  download https://api2.rongyuan.tech:3816/download/ plus returned file_path.
  Raw responses and packages: .local/research/monsgeek-20260922/.
  Packages are raw DEFLATE or ZIP; ZIP main member firmwareFile.bin.
  No updater was executed, no keyboard settings or firmware were changed.
- Official current web app entry bundles downloaded on Sep 22:
  https://web.akkogear.com/js/index.4289e208.js
  SHA256 b51206558dbe3ba055ad9f01ab8045cb89bbaeb72b47e8c2bc9351d24e9afdc1;
  https://app.monsgeek.com/js/index.13c08899.js
  SHA256 e88543d0138780139c5c8dbadeac4f3f860d728a5b3d1c39dbb430d494ccb96a.

## Exact-image execution

Tool tools/review_monsgeek_firmware.py executes pinned ARM Thumb instruction
fragments using Unicorn with synthetic RAM. It does not emulate physical ADC,
complete USB scheduling or measure latency. Four GET_MULTI_MAGNETISM handlers
return all four pages of 32 u16 values correctly without writes outside response
storage (apart from the stack):

| Board ID | Official image | SHA256 |
|---|---|---|
|2704|2704_v509.bin|d0942ea4820b2900287338462e63cdc51f8a61f61a6484d803615c7179d7d1aa|
|2782|2782_v502.bin|2e188151b6025aac685855c6088959fa3d9c9058d9055bfb07a92c380b68f306|
|2949|2949_v410.bin|e48e9c44bbe3f9cdca83ac7bacc67f9dea1c6c7c8f5ff718a90029c7df876762|
|3113|3113_v510_oledv107_flashv102.bin|97934a52a825cb97c033c0eb664f28ba7f79501274d86ce92c9c1068444c9dfa|

2949 is M1 V5 TMR, not automatically the M1 V5 HE advertised by the first
reference. ID3300 M1 V5 HE metadata request returned Record not found. No
unsupported inference that these are interchangeable.

E5 / FE snapshot reads current depth table without starting calibration.
The v410 producer fragment also executed with monitor flag OFF, two simultaneous
keys, individual releases, and depth 9/10 boundary. Firmware itself gates values
below 10. Other producer modes and host response freshness still need review.

The 1B event path uses one staging buffer; scheduling must be checked before
claiming actual event loss. Snapshot payload lacks command/page echo. Do not
accept arbitrary previous raw responses as fresh; correlation needs an explicit
transport solution. Scaling and exact physical matrix/Fn mapping remain pending.
Upstream driverClass/fallback layout associations are not exact-model evidence.

## Additional lead supplied by owner: Slice75

https://github.com/panininis/slice75-gamepad-emu, pinned commit
 afc2b05b70004fde6fe7a889bc114feb1facb977 (local research clone).
Source app/slice_capture.py targets Chilkey Slice75 HE 1CA3:0701, 5C / 12 command
family, two-part matrix reports. It includes empirical bank classification,
raw ADC baseline/range learning and interactive WASD mapping. These are research
leads, not proof that existing HallJoy RM6x21 parsing fits this model. Do not run
its calibration/save commands or copy heuristics into a support declaration.
No support-status change made for Slice75.

## Next work

Verify USB response ownership/freshness, precision/range and exact physical maps;
implement a complete admitted native path with bounded shutdown and neutralization;
then synchronize runtime notices, README, SUPPORTED_HARDWARE and live Sheet with
readback per SUPPORT_STATUS_SYNC.md. No yellow status for research-only paths.

## Additional lead: EPOMAKER G84 HE

Owner supplied https://github.com/vozuri/XinputG84HE during ongoing research.
Pinned commit 9c1a7e25e5a44647cc3d772f1f73640f12d865ab; cloned read-only into
.local/research/XinputG84HE. Services/HidDeviceService.cs enables feature command
1B 01 with checksum E3, reads report05 depth u16 at offset2, key ID at offset4.
This is the same event family as the MonsGeek/Akko reference, not independent
proof of HallJoy compatibility. Candidate USB IDs3151:5030/5038 are shared.

Implementation caveats: accepts report05 without validating event byte1B;
merges all matching devices into one key state; optional AntiGhostEnabled
(default false) drops unchanged non-full-scale values after2.5seconds, which
can also suppress a genuine steady held key. Do not copy this timer or infer
firmware is defective from the workaround. Source labels wired/wireless PIDs
oppositely to the MonsGeek reference; actual identity/transport must be verified.
No application launch, hardware commands or support-status changes performed.

Current manufacturer web driver also exposes exact model classes and static
geometry. M1 V5 HE ID2819 is another official identity (besides earlier ID3300);
its firmware API currently returned HTTP500. M1 V5 TMR2949 class d5 in official
a0c3a7d8.js has Fn action10/1 at matrix index65; encoder entries use other action
types and must not become keyboard keys. Current precision feature encoding
is0=0.01mm,1=0.005mm,2=0.001mm (upstream Rust interpretation is inconsistent).
Official geometry2771d791.js SHA256
1bea282276d5bfe3f369054b639693bde8cdd32bc088c603d3b0928b2ff0a693.

### G84 HE acquisition follow-up

Official current MonsGeek web bundle identifies EPOMAKER G84 HE revisions2642
(ry5088_fq_x122_rt001_3m_8k_8k) and2959 (ry5088_fq_x122_3m_8k_8k), both3151:5030,
Common84_x110_Deep80. Their exact manufacturer classes R1/H2 place Fn at index71.
Firmware metadata requests for both IDs returned HTTP500 on2026-09-22; neither
is a firmware acquisition success. Manufacturer matrix data for these two,
M1 V5 HE2819, TMR2949 and Akko2704/2782/3113 are retained locally in
manufacturer-model-maps.json alongside the pinned source bundles.

G84 reference uses350raw units for Gateron Magnetic Jade Pro3.5mm and suppresses
raw<=3. Those constants are specific to its assumed switch configuration; they
are not a valid universal range/noise floor for the common backend.

### Host freshness investigation — not yet implemented

Static v410 disassembly: HID OUT callback08014988 copies64request bytes to
2000D11A and sets pending byte at2000D118; GET_REPORT handler08014A68 returns
64bytes from2000D11A. Request and response use the same storage. Therefore an
unprocessed E5/FE request can be observed by a rapid GET_REPORT. A parser must
reject it rather than treating header/parameters as depths. Exclusive vendor
interface ownership and complete-response validation are necessary. No host
transport or timing claim has been validated yet. No new runtime source was
added during this follow-up.


## 2026-09-22 implementation checkpoint (local only)

Latest owner scope explicitly includes all three references: MonsGeek M1 V5 HE,
Chilkey Slice75 HE and EPOMAKER G84 HE. No GitHub push or publication authorized.
Backup before integration: .local/backups/before-three-keyboard-integration-20260922-164443.zip.

Implemented native modules: slice75_backend / slice75_protocol, and
rongyuan_snapshot_backend / rongyuan_snapshot_protocol. Both are enabled in the
ordinary native catalog, use independent physical-key publication, live base
assignments, verified automatic-layout tokens, cancel/drain/join lifecycle,
per-key freshness and disconnect neutralization. Existing bindings/curves/ViGEm
consume their normal native input. No calibration, persistent settings writes,
anti-ghost hold timeout, learned baseline, digital-to-analog fallback or forced
logging. No GUI/hardware test was performed.

Slice75: official console https://slice75he.chilkey.com/; manifest
/update/update.json; XS117_75HE_App_v1.1.7.3_optimized firmware acquired (original
Chinese filename retained in manifest). Image SHA256
8a18248235d86be6be6418f6cdd443eb3e2616e0f5ac61f3a034a6c173d96e85, 135048 bytes.
RISC-V image, not ARM; no full MCU emulation claim. Windows factory matrix at
file0x4B8 contains80keys, Fn slot116. Official RM6x21 handler reads63u16 values
per half, 132-byte frame (three64-byte HID chunks), unlike the reference's
128-byte/61-value guesses. Native proof requires exact1CA3:0701, FFA0:1,
Slice75 HE product, factory rows2B and valid12/type2 responses; assignments23.
Default3.3mm is based on the official switch table; alternate switches may need
range adjustment. Receivers, other IDs and Slice68 are not included.

MonsGeek M1 V5 HE2819 and G84 HE2642/2959: exact manufacturer factory actions,
82/84keys including Fn, feature collection3151:5030 FFFF/FF00 usage2.
No admission by sharedVID/PID alone. Four E5/FE pages independently refresh128
slots; no1B monitor is enabled. E6 precision follows manufacturer enum0=100,
1=200,2=1000units/mm, with documented legacy firmware-version scale. Baseline
normalization3.6mm M1 /3.5mm G84 remains provisional pending hardware/switch
feedback. Other Akko/MonsGeek models and wireless receivers are not admitted.
Exact HE firmware downloads remain unavailable; sibling-image fragment tests
are evidence for the shared getter, not tests of the exact HE firmware.

Unused E5/8A request tail is initialized invalid to prevent accepting partially
replaced raw replies. Four pinned E5 MCU fragments also PASS with this tail;
all64 response bytes replace it. One command at a time on an exclusive feature
handle; deadline failure ends the session. Feature IOCTLs retain buffers until
completion/cancel drain. Windows unnumbered GET_FEATURE reports64 transferred
payload bytes plus the zero ID byte in the65-byte buffer, as documented by
HIDAPI's Windows implementation. New raw parsers do not assume65 transferred.

Official layout sources are pinned under docs/research/three-keyboard-sources;
reviewed reports and tools/build_three_keyboard_layouts.py reproduce all three.
New runtime notice flags required removing the old14-bit status-storage mask.
Portable three_keyboard_protocol test PASS: partial pages, precision, exact
model maps, Fn, continuous10-second synthetic hold, aliases, releases, stale
neutralization, three-report continuation and new banner bits. Full release
build, broader audits and live Sheet readback are still in progress.


### Live Sheet synchronization

2026-09-22: Main!C142 Chilkey Slice75 HE, C209 EPOMAKER G84 HE and C406
MonsGeek M1 V5 HE changed from Not investigated to Implemented; awaiting hardware
testing. Readback A1:C650 confirms these are the only changed values, preserved
validation, and yellow RGB1/.9019608/.6392157. Catalog comparison of all50 yellow
models PASS; snapshot .local/three-keyboard-sheet-yellow-readback.json.
No comments/notes, row insertions, formatting changes or other model promotions.
TMR, other MonsGeek/Akko boards and wireless transports remain outside admission.
README/hardware inventory and local unreleased patch notes now list all three.

New Windows feature-I/O test PASS covers completion, pending request ownership,
second-start rejection, incomplete reap, explicit cancellation/drain and destructor
cleanup. Existing portable lifecycle fake updated for the new DeviceIoControl
entry point. Version audit updated to accept the owner's already-approved
version-independent README heading; README wording was not reverted.


## Final local delivery — 2026-09-22

All three requested models are integrated, with complete enabled native USB
paths and yellow notices. No GitHub push, commit, release or upload performed.

- tools/run_native_backend_checks.py --require-compiler: all static audits and
  portable/Windows C++ checks PASS (108 executable invocations). Final log:
  .local/three-keyboard-full-checks-final.log.
- Three-keyboard regression additionally covers incomplete8A assignment pages;
  both travel and assignment packets must be complete before publication.
- tools/build_release.ps1: ordinary Release x64 build and all five linked-image
  gates PASS; delivered build/bin/Release/x64/HallJoy.exe.
- EXE SHA256: 23a50bbe22164c6da74377b11bd1836f1a7341acaa13aaf53d97ca95fedce840.
- Layout extraction/source locks and all50 yellow Sheet/catalog rows PASS.
- Existing K4 onboard/UAP paths retained; no keyboard was flashed or calibrated.
  No forced logging or camera latency UI was enabled.

Remaining uncertainty is explicitly experimental: physical hardware behavior,
exact HE firmware revisions unavailable for download, alternate switches/ranges.
No claim of physical tests,1000freshHz or ideal/zero latency. Wireless receivers
and unlisted board revisions are not admitted by this implementation.
