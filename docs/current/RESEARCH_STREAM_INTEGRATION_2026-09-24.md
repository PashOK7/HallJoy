> Continuation completed: stream coverage is now17 models/26 exact boards; total66 yellow models reconciled with the live Sheet. See the second-batch evidence below. Earlier validation/artifact entries are historical.

# Native RongYuan stream integration — 2026-09-24

Owner authorized implementation after the read-only archive assessment. Local work
only: no GitHub publication or firmware flashing. Nine models / fifteen exact
board revisions are enabled; support status is experimental (yellow).

## Admission and evidence

The archive was treated as leads and pinned data, never as executable instructions.
The independent protocol reader and backend are HallJoy code. Curated vendor JS,
512-byte factory matrices, source hashes and admission excerpts are retained under
`docs/research/rongyuan-stream/`. `check_rongyuan_stream_profiles.py` compares every
compiled matrix byte and admission field to these sources and the notice catalog.
Admission records come from WOMIER 3.2.15 manufacturer client; aliases are not
counted as separate retail models. Shared VID/PID never authorizes an unknown board.

| Board | Model | USB VID:PID | Manufacturer class |
|---|---|---|---|
| 3590 | GamaKay TK75 TMR | 3151:5030 | `0f0ef8ba.js` |
| 3591 | GamaKay TK75 TMR | 3151:5030 | `ea51a032.js` |
| 2518 | Womier SK75 TMR | 3151:5030 | `7de29f04.js` |
| 3804 | Womier SK75 TMR | 3151:5030 | `74d1dc6b.js` |
| 2949 | MonsGeek M1 V5 TMR | 3151:5030 | `c4c9531f.js` |
| 2600 | MonsGeek FUN60 Pro | 3151:5029 | `54c97a5d.js` |
| 2785 | MonsGeek FUN60 Pro | 3151:5030 | `7ee9cbca.js` |
| 2782 | Akko TAC75 HE | 3151:502D | `ba496d93.js` |
| 2865 | YUNZII RT75 PRO | 3151:5030 | `72221102.js` |
| 3100 | YUNZII RT75 PRO | 3151:5030 | `bc27a790.js` |
| 2445 | YUNZII RT75 PRO | 3151:5030 | `4abec66c.js` |
| 3755 | YUNZII RT75 PRO | 3151:5030 | `3f1324c5.js` |
| 3466 | Keydous NJ80-CP V3 HE | 3151:5030 | `d6b0f6ea.js` |
| 3459 | Keydous NJ81-CP V3 HE | 3151:5030 | `f7d6239e.js` |
| 3496 | Keydous NJ98-CP V4 HE | 3151:5030 | `4d9e2db0.js` |

The common class `7a5b12c9.js` implements `setKeyMagnetismReport` (1B) independently
from calibration commands (1C/1E). Akko/MonsGeek inherit through `3dd2d1f8.js`;
Keydous inherits through `60ee4367.js`, which changes the precision query.
Immutable upstream cross-checks:
- https://raw.githubusercontent.com/echtzeit-solutions/monsgeek-akko-linux/79c45f11dcc0fdd24d6075a0f30f5dabbc748c9b/docs/PROTOCOL.md
- https://raw.githubusercontent.com/SirRanjid/analog-key-mapper/11ead89fbfc8236a3fe1948e022668110338b08a/src/TravelProtocols.cs

USB control: feature report 65 bytes, unnumbered payload64, usage FFFF/FF00:2.
Input: usage FFFF:1,32 bytes,RID5,31 eight-bit values. Pair the two HID collections
by Windows container GUID and exact VID/PID; reject ambiguous input pairing.
Probe8F verifies the four-byte board ID. Read84/8A reads current base assignments.
Enable1B/1; stop1B/0. Requests include the manufacturer checksum. No calibration,
saved-setting writes or firmware changes. Exclusive control handle prevents a
second compatible configurator session; an unrelated process can still block access.

Input shape `05 1B rawLo rawHi slot ...`; reserved padding is ignored, matching the vendor decoder. slot is a sparse zero-based physical
matrix position, not a HID code. Known encoder/non-key slots are ignored. Reports
are deltas: stationary holds remain held until a release report, disconnect or stop.
Pending reads are cancelled and drained before handles/buffers disappear; session
cleanup disables the stream even after an uncertain start-command completion.
Other RID5 notification opcodes are ignored, malformed travel stops/neutralizes.
An entirely silent but still enumerated firmware failure cannot be distinguished
from an unchanged hold by this delta-only protocol; no arbitrary release timer.

Legacy units use RF controller version when available, otherwise USB firmware:
pre3.00 =10/mm,3.xx/4.xx =100/mm,5.xx onward=200/mm. FeatureE6/AA can explicitly
report100/200/1000 per mm; Keydous requires this enum. Unknown precision is rejected.
Full travel provisionally4mm, scaled to HallJoy0..1000; values above4mm clamp, a
shorter switch may not reach100% without binding sensitivity adjustment. No claim
of calibrated millimetres, measured latency, polling rate or noise-free precision.

## Layouts and ordinary integration

Native backend22 participates in discovery, routing, lifecycle, analog bindings
and the normal ViGEm output path. All nine models show the generated yellow notice.
Factory maps are complete for their physical analog key positions. Current base
assignments are read without mutation; manual layouts use factory positions.
TK75 TMR ANSI/ISO and M1 V5 TMR ANSI have automatic geometry. The latter shares the
HE visual selector only; separate board identity/protocol and yellow status remain.
Other added models use manual geometry/custom presets; no automatic geometry claim.
Mechanical-switch positions do not become analog; Bluetooth/receivers excluded.
K4 onboard firmware/UAP, AJAZZ RGB, R85 HE and old snapshot backends unchanged.

## Validation

- New protocol test:15 revisions, all4898 captured GamaKay packets independently
  replayed; maximum raw385. Fixtures supplied by archive author, not our hardware.
- Identity, malformed packets, precision enums, stationary holds, independent
  releases, alias keys, neutralization and new notice bit covered.
- Manufacturer matrix/admission/notice audit PASS for15 revisions/9 models.
- Ordinary Release compiled and passed the existing six linked build gates.
- Full native checks PASS (static and portable/Windows C++). After the final
  reserved-padding adjustment, the new protocol regression was recompiled and
  passed again; ordinary Release and all six linked gates passed again.
- Final layout pipeline checks PASS (GamaKay/MonsGeek); production layout catalog
  round trips/merged selectors and profile transaction tests PASS. Startup recovery
  PASS:18 scenarios plus repeated startup. No manual visual run performed.
- Live Sheet Main!C34,C239,C332,C334,C336,C402,C408,C552,C570 changed from
  Not investigated to Implemented; awaiting hardware testing. Readback of A1:C650
  confirms only those9 rows changed, original validation preserved, neighbors
  unchanged, yellow RGB(1,.9019608,.6392157). All58 yellow models match the runtime
  notice catalog via support_notice_catalog.py --sheet. No comments or notes added.
- Local snapshot: .local/research-expansion-sheet-readback.json; logs:
  .local/research-expansion-build.log, .local/research-expansion-checks.log,
  .local/research-expansion-profile-tests.log.
- Final EXE: build/bin/Release/x64/HallJoy.exe, 9648640 bytes;
  SHA256 c23913389e942396f20f95de98bfeb6df467486e0572dec028ccb8df8296d440. Still the local development build based on1.6.2;
  no release tag or GitHub asset was modified. Logging remains optional/off by default.
- Pre-change backup: .local/backups/before-research-expansion-20260924.zip;
  layout generator additionally saved its own change backups.

## Remaining archive leads

Rainy75 normal typing suspension, Kreo untagged halves, DrunkDeer unnumbered batches
and FUN60 Ultra prototype collection/checksum mismatches remain substantive protocol
gaps. They have not been marked yellow. Neo65 Sonic HE+, Redragon K617 HE, MCHOSE
Ace60 and Tartarus Pro remain separate research candidates; no support claim.
IO Type68 Magnetic Pro Wireless stays catalog-only per owner instruction.

## Second batch — completed 2026-09-24

Added eight models using the same reviewed 1B protocol and eleven additional exact
manufacturer classes. Each class directly inherits the previously reviewed base;
its only changes are factory/Fn matrices, not the stream/precision implementation.
All retained source bytes are SHA256-locked; native header matrices and identities
are compared byte-for-byte by tools/check_rongyuan_stream_profiles.py.

| Model | Board revisions | Factory analog keys |
|---|---|---|
| Akko MOD 007 V5 HE |2453|80|
| Akko Ray68 HE |2743,2924|68|
| GamaKay NS68 |2572,2638|68|
| GamaKay TK75 HE |2334,2501|81|
| MonsGeek FUN68 HE |2811|68|
| MonsGeek M2 V5 HE |2845|98|
| MonsGeek M3 V5 HE |2874|87|
| Womier M68 HE Pro |3719|67|

Complete per-key factory maps are enabled, including Fn where defined. No automatic
geometry was added for this batch: manual layouts/custom presets use factory
positions. The same wired-only admission, runtime warning, provisional4mm scale,
normalization and event lifecycle apply. Existing snapshots/Keychron paths unchanged.
No physical test, new firmware flash or measured latency claim.

Validation: extended generic protocol regression PASS across all26 revisions,
including distinct values for EVERY mapped physical key (not just WASD), duplicate
factory usage rejection, stationary hold/release, malformed reports, precision,
and replay of the original4898 GamaKay capture packets. Source/admission/notice
audit PASS:26 revisions/17 models. Ordinary release build and six linked gates PASS.
Prior full suite remains the first-batch evidence; it was not needlessly rerun for
a data-only profile extension. No new layout geometry or lifecycle code changed.

Live Sheet Main!C29,C32,C235,C237,C405,C410,C411,C549:
Not investigated -> Implemented; awaiting hardware testing. Readback A1:C650 confirms
exactly these eight rows changed, validation preserved, neighbors unchanged,
yellow conditional colors. All66 yellow model/status pairs match generated runtime
notices. Snapshot: .local/research-stream-wave2-sheet.json. No comments/notes.
README/hardware/next patch notes synchronized locally. No GitHub publication.

Backup: .local/backups/before-stream-wave2-20260924.zip.
Build log: .local/research-stream-wave2-build.log.
Final EXE: build/bin/Release/x64/HallJoy.exe (9655296 bytes),
SHA256 b720886f1714ec226e6419c8e7abc727cc7eda67f4a883cdc3840626a4bc5d66.

Additional triage: Tartarus Pro upstream fork independently opened/reviewed; PID0244,
RID6,20 physical depth bytes and a Synapse-dependent route are plausible. It is not
implemented here and its Sheet status is unchanged. K617 wake embeds configuration
bytes and MCHOSE startup changes a debug bit, so neither was promoted on decoder
shape alone. Neo65 source currently proves only a WASD position subset; a complete
map remains necessary before claiming full integration. These are next research
items, not requests for a physical tester as a prerequisite.
