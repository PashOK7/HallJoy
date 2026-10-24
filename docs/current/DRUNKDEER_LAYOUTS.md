# DrunkDeer layout and model batch — 2026-09-09

Current implementation supersedes the investigation-only status in
`DRUNKDEER_LAYOUT_IDENTIFICATION.md`. Single-brand batch; no new vendor/model
families beyond the DrunkDeer identities already declared by embedded UAP.

## Catalog

Seven physical layouts: A75 ANSI, A75 Pro, A75 ISO, G60 ANSI, G65 ANSI,
G75 ANSI and G75 JIS. Shared by main preview, Layout editor and Input Overlay.
No UK/FR/DE duplicate categories; English labels. ISO/JIS Enter uses the existing
compound contour in all consumers. No image widget or custom Enter renderer.

`tools/build_drunkdeer_layouts.py` generates both the UI geometry header and
126-cell physical tracking maps. Inputs are the parsed Antler factory/preview
audit and the frozen CSS rectangle extraction in
`docs/research/drunkdeer-layout-sources/rectangles.json`. Source hashes are kept
in each review; reproducible INI exports are in `docs/exports/drunkdeer`.
Normalize ordinary key height to 42 px with ONE scale for both axes, rounding
absolute edges. Do not reflow rows. Validate actual contours for overlaps,
unique identities, dimensions and missing keys. Compound notch widths follow
the straight edges of vendor transparent assets (46/164 ISO, 52/220 JIS).

Tracking uses preview offsets, because Antler's B7 handler uses those offsets
directly. Explicit reviewed exceptions to stale factory defaults: End at 99
for A75/Pro/ISO; ISO # at 75 and non-US backslash at 85. End=99 also matches the
previously shipped A75 decoder. Factory keyIndex typo at unused slot 19 on
G75/JIS does not alter array positions. G65 matches the full official factory
array; owner approved navigation offsets 35/56/77/98. Preserve all 45 earlier
physically observed G65 cells, including arrows, modifiers and extended Fn/Fn2.

## Runtime identification and analogue maps

The existing UAP device owner sends the official read-identity report
`04 A0 02 00 ...` only after stable-ID deduplication and before starting its
worker. Existing stable-ID seeds are unchanged. Request is restricted to the
five known PIDs. It uses the same handle and named mutex as analogue reads,
bounded mutex acquisition (100 ms) and bounded write/read transactions
(100 ms each), never UI-thread HID or repeated per-frame identification.
No calibration, firmware write or change to letter output.

Accept only an exact 64-byte response, expected report/command/status, known
signature AND matching PID. Verified name and flag travel through the existing
telemetry structure; ABI structure size/version unchanged. First-run selection
requires that flag, exact model/PID and 6x21 transport matrix, plus existing
fresh/coherent one-device one-shot policy. Device names alone do not select a
layout. Saved/manual selections, independent overlay choice and later hotplug
remain authoritative.

Verified models select their own generated analogue map. Raw travel scaling
and B6/B7 framing remain unchanged; this batch does not claim new high-precision
firmware scaling. If optional identity is unavailable, preserve the previously
shipped decoder for compatibility, WITHOUT setting a verified model or choosing
an A75 layout. This is deliberately not a claim that the legacy generic map
becomes exact for unknown firmware.

Append three Soup identities (non-US hash, Ro, Yen), preserving every existing
enum value. Add their exact bidirectional HID mappings. Both legacy dense and
provider-v2 paths therefore retain distinct ISO/JIS keys without aliases or
OEM-code substitution. Seven Soup overlay files are hash-locked. The dependency
audit now verifies the actual complete file set and normalized contents instead
of merely asserting that there are five files.

## Existing files and tests

A75 Pro remains catalog index zero; G65 stays index one. On load, upgrade an old
default only when ALL effective geometry, key identities, labels and spacing
still match the original. Any user edit preserves that file. Use the existing
atomic layout writer, no migration sidecars or extra backup spam. New layouts
use normal existing preset creation. Backup is
`.local/backups/drunkdeer-integration-20260909/` (release, source, old runtime DLLs).

Checks include generated-output reproducibility, source parser rejection,
all 126 G65 cells versus official factory data, all seven model signatures and
PID/name rejection, full Soup HID roundtrip for every generated key,
production-linked first-run selection for seven models, no-name-only selection,
one-shot policy and preserving edited legacy layouts. Existing editor event
test was corrected to resolve sorted combo rows by catalog identity: adding
models exposed its old assumption that row 1 was always catalog entry 1.

No visual test or physical DrunkDeer test is claimed. Owner evaluates appearance.
Connected-device private UAP ABI gate passed with the existing keyboard;
legacy/provider-v2 projections agreed. Release/simulator builds pass with the
existing optional ViGEm PDB warning. Final test/deployment evidence follows below.

## Delivered

Release SHA256: `F9FF13072B96C85FC0078EB5B0A45B98D9BAE6599FF14844E75914102213680A`.
Installed at `build/release/HallJoy.exe`, dependency lock and SHA256SUMS updated,
ordinary HallJoy restarted through Explorer. Embedded ABI1 resource SHA256 and
extracted runtime agree with the rebuilt plugin:
`196987606A83E7D4DD2D9F7FA253C209B70572331A65CC178C1DAA1E40A98FAE`.

All static and portable C++ checks PASS (source inventory 521); 11 DrunkDeer
Python tests, 3 Lemokey tests, 16 importer tests and both generated catalog
checks PASS. Lemokey geometry is unchanged; its source-review hashes were
refreshed because the shared Soup source changed. Soup key roundtrip PASS for
all seven maps. Private ABI live gate PASS: one device, equivalent legacy and
provider-v2 snapshots. Full Windows profile/editor/picker tests and rejected
startup file-preservation tests PASS:
`%TEMP%/HJProfileTest-9f03c11b2fcd4db0a6462eca3c9bbc62`.
The event suite exercises all seven DrunkDeer model selections and both compound
Enter variants; count assertions now follow the actual filtered catalog.

Owner check: select Brand **DrunkDeer** in Global settings or Input Overlay;
test the seven models, including ISO/JIS Enter. This completes this brand's
implementation batch. Wait for owner feedback before starting another brand.

## Tracking formats: A75 Ultra and high-precision A75 firmware (2026-10-20)

Trigger: YouTube comment (A75 Ultra, no log): full press at about a third of the travel,
constant instability warning, depth shown in only 2 of 4 onboard profiles. Owner: fix and
release.

Official Antler driver (`.local/research/layout-import-drunkdeer/index.CJWCGjvj.js`, static
read, nothing executed):
- Identity reply `A0 02` (payload offsets): signature bytes 4/5/6, firmware number =
  `Number(String(payload[8]) + String(payload[7]))`. A75 Ultra = 11/4/4 (type 756), A75 Master
  = 11/5/4 (757); both use PID 2383 like A75.
- A75 Ultra and Master run `precision 2` without `oldOpenHighPrecision`: tracking start
  `FD 03 01`, replies `FD 06 <chunk>` with 30/30/30/30/6 little-endian u16 values, displayed
  as `a / (3.3 * 200)` (values below 40 zeroed). One request per frame, like `B6`.
- A75 (11/1/1, 11/4/1) firmware >= 35, A75 Pro (11/4/3) >= 17, A75 ISO (11/4/2) >= 25 run
  `precision 2` WITH `oldOpenHighPrecision`: same `B6/B7` frames, u8 in 0.01 mm (action point
  written as mm * 100, sliders max 2 mm), displayed as `w / 200` (values below 2 zeroed).
- HallJoy/Soup sent only `B6 03 01` and divided every u8 by 40, so both cases were
  mis-scaled; the Ultra additionally got legacy requests.
- Ultra key positions (`getA75Ultra`) equal A75 ANSI plus cell 15 (F13); Master positions
  are shifted (not handled; it stays on the legacy path).

Change (plugin, version-gated by the verified identity; unverified boards unchanged):
- `halljoy_drunkdeer_identity.h`: `Model::A75Ultra` (appended), `Identity::firmware`,
  `Tracking {Legacy, HighPrecision, Wide}`, `TrackingMode` (thresholds above), `FullScale`
  40/200/660, `NoiseFloor` 1/2/40, `Travel`, `TrackingSource` (Ultra -> A75 ANSI map),
  `WideTrackingRequest`, `WideChunk`, `WideValue`.
- Soup `AnalogueKeyboard.cpp/.hpp`: `drunkdeer_tracking`; Wide frames read by
  `halljoy_drunkdeer_wide` (five chunks, duplicate = invalid, at most three unrelated
  reports skipped); verified B7 maps scale with `Travel(mode, raw)`. Legacy fallback decoder
  for unverified boards unchanged.
- `main.cpp`: tracking mode set with the verified model; `nominalRawLevels` 41/201/661 shows
  the mode in HallJoy.log. Name "DrunkDeer A75 Ultra"; no automatic layout (no Ultra preset).
- Test `drunkdeer_identity_test.cpp`: Ultra signature, firmware digits, thresholds, scales,
  FD request and chunk parsing. Dependency lock (Soup overlay hashes), Lemokey review
  hashes and the research reference record updated.

Not verified on hardware. Risk: if a high-precision A75 / A75 Pro firmware reports B7 in
0.1 mm after all, those keys would now reach only 20 % depth; the log shows
`nominalRawLevels=201` for that mode. Profiles question (2 of 4) not explained without a log.
Backup: `.local/backups/drunkdeer-ultra-20261020/`.
Released in 1.6.11 (commit 28b573b, EXE 793e0949), https://github.com/PashOK7/HallJoy/releases/tag/v1.6.11.

## Correction: the tester's A75 Ultra is 19F5:FB3E, read as NuPhy (2026-10-20)

Tester log `HallJoy — копия.log` (HallJoy 1.6.11, firmware v121 = latest stable per the
tester's screenshot; the tester says it behaved the same before 1.6.11): the keyboard is
**19F5:FB3E "A75Ultra"** (NuPhy USB VID), not 352D:2383. The 1.6.11 change above never applies
to it. The log shows `uap.child_exit` / `uap.child_failure` with 0xE0484944 (host "HID"
controlled restart after 4 consecutive plugin transport errors), `restarts` 17..26,
`ui_communication_warning=1`.

Cause (all versions): Soup routes VID 19F5 to its NuPhy A0 reader, which takes travel from
bytes 4..5 with a 800 full scale and marks the whole report invalid (disconnect) when the
value exceeds 800. The official 19F5 DrunkDeer driver (`https://drunkdeer.keybord.net.cn/`,
local copy `.local/research/drunkdeer-keybord-20261020/`, the 352D Antler driver redirects
these PIDs there) reads A0 as: code `K(e[1],e[2],e[3])` (type 16 + modifier mask + HID,
240/255/1 = Fn), travel `F([e[7],e[6]])` = big-endian bytes 6..7, shown against
`switchMaxTravel` 400 with step 0.01 (switch travels 400/360/370/330 = 0.01 mm). Device table
`0x19F5_0xFB3E` = A75 Ultra type 101, firmware v121. So HallJoy read the wrong field (full press
early) and dropped the connection on large values (restarts, instability warning).

Fix: `IsNuphyPlatformBoard` (wired FB3E A75 Ultra, FB5E A75 Master, FB5C/FB63 A75 Pro, FB50
X60, FB42 X60 HE, FB7B A75 Air), `NuphyPlatformTravel` (bytes 6..7, clamped to 400); Soup NuPhy
reader uses it for those PIDs (clamp, never disconnect), 0xFF01 = Fn; telemetry
`nominalRawLevels=401`. NuPhy's own boards unchanged (audit strings intact). Test extended.
The 352D type-756 FD path stays (official Antler evidence) but is unproven on hardware.
Profiles "2 of 4": not explained; the A0 stream needs no enable command in the driver.

Tester EXE (local, not released, version string still 1.6.11): build.ps1 PASS (NUPHY_UAP_STATIC_AUDIT, DRUNKDEER_IDENTITY, 0 unexpected warnings), SHA-256 97839cb8e3dd9a53e37a74f1760fc99b7ca1a8b4b77b62121c1ad10ba93055f9. Expected in the next log: no uap.child_failure, nominalRawLevels=401 for 19F5:FB3E.

## Profiles 3/4: Fn held at full depth, no other depth (2026-10-20)

Tester log `HallJoy — копия (1).log` (19F5 fix EXE 97839cb8): no `uap.child_failure`, one start
restart only; bound WASD travel smooth 0..1000 in working profiles. Presses in blocks: whole
blocks with full analog, whole blocks with OS key-downs and zero analog (e.g. 40+ presses at
77..95 s). Tester: profiles 1/2 work, 3/4 not (earlier build: 2/3); in 3/4 HallJoy shows Fn
pressed to the bottom until profile 1/2 is selected again. Earlier build (bytes 4..5, >800 =
invalid) restarted the host in the same situation. Conclusion: in those profiles the keyboard
sends A0 records that HallJoy decodes wrongly (key ends up as Fn 0xFF01); the official driver
decodes the key with `K(type, b2, b3)`, Soup ignores the type byte. Not a keyboard fault.

Diagnostic (tester EXE, not released): `drunkdeer_nuphy_probe.cpp` = second shared read-only
reader of the 19F5 DrunkDeer collection (usage 1:0, path filter `vid_19f5` before any open),
records `evidence.dd19f5 a0 type= soup= driver_key= f45= f67= st= hex=` on change (new key,
travel step >= 40, to/from zero); ordinary unbound keys only counted; non-A0 report heads once
per first byte; open/closed/summary counters. Started in app.cpp, stopped in main.cpp.
build_release.ps1 PASS, EXE SHA-256 b0748ecc461ed934167370f4a3ea3c937e22f03d030a1a5b4317b9f232f7710b.

Probe rev 1 log (`HallJoy — копия (2).log`, 13:50 UTC): `open pid=fb3e input_bytes=65` but zero reports read
(analog worked meanwhile): the probe opened only the first usage 1:0 collection. Tester facts after it:
the set of working profiles changes between runs ({2,3}, {1,2}, {2,3}) and does not follow the profile
active at start; on a non-working profile HallJoy's view freezes (WASD partial, Fn lit, `[` partial =
last values of the switch combo); the web driver shows the Fn keys as FN9/FN10 in some profiles (Fn code
is per profile). Soup reads every matching collection (no dedupe), so either the A0 stream stops or moves
to a collection with another usage. Stale values are a HallJoy fault in any case (no reset on stream stop
or profile change).

Probe rev 2 (EXE SHA-256 df89e5047f55f2fd9db0dd0b5a1a26a5627d1a677d5441d30114d68f7a9b80e4, build_release.ps1
PASS): reads ALL collections of the board in parallel (`at=mi_xx&colyy usage=`), first report per
collection, `profile` records for A1 (index byte 2), non-standard A0 codes in full. Wake experiment
(owner idea): read-only getInfo `55 03 00 20 20` on the 1:0 collection with an output report, at start
and 1 s after each A1, logged as `wake n= reason= sent= error=`.

Probe rev 2 log (`HallJoy — копия (3).log`, 14:00 UTC): the board has one usage 1:0 collection (`mi_01`,
in/out 65), other collections are keyboard/mouse/consumer/system. A1 heads: index 1 `a10401ff…`, index 0
`a10000ff…` (byte 1 differs). After switching to index 0 no A0 at all (plugin saw none either: WASD OS
downs, zero travel) and no Fn release (stale Fn = the "held" Fn). getInfo after the switch did not bring
the stream back. A0 layout confirmed: `a0 type b2 b3 f45(BE, raw ~0..7900) f67(BE travel 0..400) …`.
Probe rev 2 itself stalled after its third wake (write ran in the reader thread); fixed in rev 3.

Tester (17:03-17:06): the same EXE gives different working sets on different runs; opening the official
web driver and closing it WITHOUT applying anything changed the set to 1,2,3 (4 not). The driver loads a
profile (e5: settings, binds x4 layers, key trigger, DKS/MT/TGL, macro reads) on connection and after
each profile switch. Working hypothesis: reading a profile enables its travel stream in firmware RAM.

Rev 3 (plugin + probe), build.ps1 PASS, EXE SHA-256 1a5157b1340839d59a0283fe63a4c6caa6b5e67a1d2f77abf5cbc9cd08237bf4:
- Plugin: on A1 (19F5 DrunkDeer) all key values reset to 0 (no stale Fn); A0 reports whose type byte is
  not 16/240/224 are ignored (replies to vendor cmd 0xA0 also start with A0). Test extended.
- Probe: separate loader thread with its own handle repeats e5 exactly (requests `00 55 cmd 00 sum len lo hi`,
  sum = lo+hi+len, reply data from byte 8): all four profiles at start, the new profile 1 s after each A1.
  Logs func (64 B), binds (4 x 384 B), trigger (1024 B), counts for DKS/MT/TGL/macro (macro contents not
  logged), requests/replies/missing per load. Readers only read.

Layout and remaps (2026-10-20): the 19F5 driver layout `K75.json` (A75 Ultra; chunk 198, local copy in
`.local/research/drunkdeer-keybord-20261020/`) has 82 keys with factory codes (FN1 = 255, FN2 = 254); it
is the same 82-key set and codes as HallJoy's built-in `DrunkDeer A75 ANSI` (Fn 0x409, Fn2 0x403). No
automatic selection for 19F5 yet (MatchDrunkDeer requires VID 352D). The driver lights keys by matching
`K(type,b2,b3)` against the layout's factory codes, so A0 identifies the physical key; onboard remaps do
not move analog to another key (to be confirmed with the rev 3 bind dump).

Pending for the next build (code done, not built): type-aware key decode `NuphyPlatformKey` (driver K):
Fn2 (240/255/2 and 224/0/x) -> HallJoy Fn2 0x403; before, a 224-type record was read as HID byte 3 (Fn2
travel on a random key) and Fn2 was otherwise ignored. Test extended; nuphy audit PASS.

Rev 3 log (`HallJoy — копия (4).log`, 14:27 UTC): all 120 load requests per profile answered at start
(p0..p3); travel worked, then after A1 -> index 0 no travel even after our e5 reload of p0 (116/120, func
chunks missing); opening the web driver brought travel back (plugin pads from ~27453.6 s). The probe's
readers recorded nothing after the reload (second time; cause open, rev 4 logs a reader liveness counter).
Remap check by the tester: E bound to N on the keyboard still shows as E in HallJoy (A0 = physical key).

Profile settings (func, 64 B from reply byte 8): byte 7 = `04` for p0 (silent) and `0c` for p1..p3
(working); the only difference in that byte is bit 3 (0x08), the bit the driver's unused `openDebug(e)`
sets (`t[7] |= 8`). Bits 0 (Berserk/Tachyon), 1 (bottom RT), 5..7 (debounce) are equal. Hypothesis: bit 3
enables the A0 stream per profile. Why the working set changes between runs is not explained yet.

Driver connection sequence (before its first e5): getDefaultKeyInfos(profile 0, layers 0..3) = cmd 7,
getBase = cmd 4 `[4,0,32,32]` (reply data[0] = active profile), then e5(active). On A1 the driver runs only e5.

Rev 4 (build.ps1 PASS, EXE SHA-256 bf4c554f1ada1f073d854f4b36c932da68bc351ca3db095b26ffdac903e7196f): the
probe runs that full connection sequence at start and 1 s after every A1 (owner idea: wake as on first
connection), logs `connect … active= reader_reports=` and the active profile's func each time. Plugin:
type-aware key decode (Fn2) included. No write to keyboard settings; setting bit 3 would need owner consent.

## Cause found: per-profile "Travel" debug bit (2026-10-20)

Driver code: the tab bar calls `setDebugModeKey(profileIndex, tab == "travel" ? 1 : 0)`, which writes
`func[7] = (func[7] & 0xF7) | on << 3` with `setFunc(profile)` (cmd 6, `sendData`: 56-byte chunks
`55 06 00 sum len lo hi last data`, sum = len+lo+hi+last+data). The A0 stream of a profile runs only while
that bit is set, so a profile works iff the driver was last left on the Travel tab while that profile was
active. Matches every tester observation (set changes between runs; "open driver, look at travel in
profile 1, close -> profile 1 works"; rev 3 dump: p0 byte7 04 silent, p1..p3 0c working).

Owner decision 2026-10-20: HallJoy writes the bit (no other way to enable the stream). Implementation
(`drunkdeer_nuphy_probe.cpp`, replaces the e5 replay): at start for p0..p3 and 1 s after each A1 for the
new profile: read func (cmd 5); skip unless 64 bytes complete and bytes 2..3 = `aa bb`; if bit 3 is clear,
write the same 64 bytes with bit 3 set (cmd 6), wait 50 ms, read back and compare; log
`stream p= reason= action=already_on|set|skip_unreadable byte7= written= verified=`. Never cleared (same
state as closing the driver on the Travel tab; at most one flash write per profile).
Layout: MatchDrunkDeer selects `DrunkDeer A75 ANSI` for 19F5:FB3E (K75 = same 82 keys/codes); test added.
build_release.ps1 PASS (UAP from rev 4 build.ps1: reset on A1, Fn2 decode). EXE SHA-256
231a2a57a3e9ad65b5302c02e36ca5565ea1e2881c51419a76260267c75e4ca9 (tester build, not released).

Tester result (2026-10-20, EXE 231a2a57): all four profiles work, owner: "works perfectly". Onboard remaps are not reflected in HallJoy (A0 = physical key), accepted by the owner. Pending: release 1.6.12 + support-status sync (A75 Ultra, FUN60 Ultra TMR 2307) + correct the 1.6.11 A75 Ultra note.
