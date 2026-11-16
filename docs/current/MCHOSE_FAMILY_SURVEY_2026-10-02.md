# MCHOSE: static survey of every M HUB firmware (2026-10-02)

Continuation of [Jet 75 II](MCHOSE_JET75_2026-10-02.md): the owner asked to
analyse all MCHOSE models. Input: the 33 official M HUB images and 70 web-driver
files downloaded under the owner's 2026-10-02 authorization
(`.local/research/mchose-mhub-20261002/`, nothing published). Static analysis
only: no device, no tester log, no runtime or Sheet change in this record.

Scripts (all in that folder): `survey.py` (architecture, RISC-V opcode table),
`family.py` / `family2.py` (RISC-V anchors, A0 descriptor and maximum tables),
`writer_facts.py` (`06` writer reload and reboot rule), `arm_family.py` /
`arm_cmp.py` / `arm_facts.py` (ARM images against Mix87 III 1.22),
`mkelf.py` (raw image → ELF for `llvm-objdump`). Outputs: `survey.json`,
`family.json`, `family2.json`, `family_keys.json`, `writer_facts.json`,
`arm_family.json`, `arm_facts.json`; listings in `dis/`.

## Result in one line

Every magnetic M HUB model with a published image uses the **same analog
design** as the two Supported boards: the `A0` key-travel report produced only
while **profile byte 7 bit 3** is set, behind the same `55` / `AA` command
transport. Three implementations of it exist, matching the two existing
backends plus one new chip.

## Group A: WCH RISC-V, base 0x5000 (Jet 75 II design)

All 11 images: opcode table `01..0E, A0..A9, (AA), B0, B1, DD, DE, EE, F1, F2`
(the survey's "base 0x0" for four of them was a detection miss; at 0x5000 the
anchors resolve), `04` reads 0x20200, `05` reads 0x20100, `06` writes ≤256
bytes at 0x20100 and reloads the active profile (stride 64) into config RAM,
the A0 service gate is exactly config RAM + 7 bit 3, and the A0 builder has the
same ≤9 → 0 / ≥max−5 → max clamp. Descriptor slots have no duplicate HID codes.

| Model (catalog) | VID:PID | FW | Keys (+Fn) | Max (0.01 mm) | Reload | Reboot after `06` if |
| --- | --- | --- | --- | --- | --- | --- |
| Jet 75 II (Supported) | 41E4:211A | 1.16 | 79+1 | 331/341/351 | 64 B | flash/reset flag set |
| Zero75X | 41E4:211C | 1.14 | 79+1, same set as Jet 75 II | 341 | 64 B | flash/reset flag set |
| Ace 68 Pro | 41E4:2116 | 1.21 | 67+1 | 341/351 | 64 B | not resolved by script |
| Ace 68 Air II | 41E4:2120 | 1.17 | 67+1 | 341/346 | 64 B | byte 4 low nibble changed |
| Ace 60 Pro | 41E4:2103 | 1.18 | 60+1 | 341/351 | 64 B | byte 4 low nibble changed |
| Ace 60 Pro Nordic | 3837:3002 | 1.07 | 61+1 (ISO key 0x64) | table not decoded | 64 B | byte 4 low nibble changed |
| Ace 60X II | 41E4:2112 | 1.04 | 60+1 | 330/341/351 | 64 B | byte 4 low nibble changed |
| Jet 75 I | 41E4:2118 | 1.09 | 79+1 | 341/351 | **32 B** | byte 4 low nibble changed |
| Ace 68 I | 41E4:2114 | 1.09 | 67+1 | 311/341/351 | **32 B** | byte 4 low nibble changed |
| Ace 60X I | 41E4:2126 | 1.05 | 60+1 | 311/341/351 | **32 B** | byte 4 low nibble changed |
| Mix 87 I | 41E4:2122 | 1.12 | 86+1 | 311/341/351 | **32 B** | byte 4 low nibble changed |

Notes:

- Older generation (32-byte reload): the reloaded part still contains byte 7,
  so the flag takes effect without a reboot. HallJoy writes only byte 7 bit 3,
  so the "byte 4 changed" reboot never triggers; the Jet 75 II "first write
  after flash/factory reset reboots" rule applies only to Jet 75 II and Zero75X.
- Switch maximums are inside the 200..600 range the Jet 75 II backend accepts.
- The 68/60-key images use 0x49 (Insert) and 0xE6 (Right Alt) slots; the
  60-key ones 0x65 (Application). Exact per-model layouts must come from the
  descriptor tables (already extracted) checked against the official
  `layout-*.js` files (present for all 32 catalog models).
- Ace 60 Pro Nordic: the switch-maximum table uses a different stride; not
  decoded yet (needs a manual look before use).

## Group B: ARM Thumb, base 0x08008000 (Mix87 III design)

Mix 87 III (Supported), Ace 68 III (3837:3003), Ace 68 Air III (41E4:2132),
Ace 68 Air 2 (3837:300A), Ace 68 V2 III (3837:3024), Ace 68 Turbo 8K
(3837:3028), Ace 75 8K (3837:303C). All have the same settings flash pages
(0x0802A000 base, 0x0802C000 settings), the `ldrb [cfg,#7]` bit 3 service gate
and the A0 builder. Builder versus Mix87 III over 150 instructions: identical
for Ace 68 Air III; first difference at instruction 87..130 for the others
(not yet classified: constants or logic). Service slot bounds: 92 (Mix87 III,
Ace 75 8K), 84 (Ace 68 III / Air III / V2 III / Turbo 8K), 76 (Air 2).

Ace 60 (41E4:2101, FW 1.20) is ARM too but at base 0x08004000 and a different
build: A0 marker at 0x0800B096 and a byte-7 bit-3 gate on config 0x20002AA2
were found; not compared further.

## Group C: new WCH core (Zba/Zbb + XW), dual image

Ace 68 GT (3837:3007, Feb 7 2026) and Ace 68 Turbo 16K (3837:3026,
Dec 12 2025), 432 KiB each. Disassemble with
`--mattr=+m,+a,+c,+f,+xwchc,+zba,+zbb,+zbs`. Found: the same `55` parser
(length ≤ 0x38, sum checksum, `AA`/`AB`, "unkw"), an extra `5F` frame type,
the M HUB opcode table `01..0E, A0..A9, B0..B2, DD, DE, EE, F1, F2` twice
(file 0xF394 and 0x5250C in GT; two linked copies), the A0 builder and a gate
on RAM 0x2017A217 bit 3 (= config + 7), with the same 80-slot round robin and
≤9 cutoff. Handlers `03..06` are not reviewed yet (runtime base of each copy
not established).

## Not analysed / no image

- No published image: Ace 60 Pro ISO FR (3837:3040), Ace 75 16K (3837:301D),
  GOD 60 (3837:3020), Jet 75 III (3837:3008).
- K87 V3 and its receiver: empty files on the CDN.
- G75 V2, G87 V2, G98 V3, K87S, K99 V3 and the 2.4 GHz receivers: different
  packaging (`37 38 <pid> <ver> AA BB` header) or a different image; magnetic
  sensing for these lines is not established and they are not in the Sheet.
- 19F5:FC30 / FC31 catalog records have no model or image.

## Sheet mapping (live rows, unchanged)

| Sheet row | Status now | Catalog devices behind it |
| --- | --- | --- |
| Ace 60 | Research incomplete | Ace 60 (ARM, group B-like) |
| Ace 60 Pro | Not investigated | Ace 60 Pro, Nordic (A); ISO FR (no image) |
| Ace 68 | No usable analog found | Ace 68 I (A), Pro (A), III (B), V2 III (B) |
| Ace 68 Air | Not investigated | Air II (A), Air III (B), Air 2 (B) |
| Ace 68 GT | Not investigated | GT (C) |
| Ace 68 Turbo | Not investigated | Turbo 8K (B), Turbo 16K (C) |
| Ace 68 V2 | Not investigated | V2 III (B) |
| Ace 75 | Not investigated | Ace 75 8K (B), 16K (no image) |
| GOD 60 | Not investigated | no image |
| Jet 75 (II revision) | Supported | — |
| Mix 87 (III revision) | Supported | — |

Not in the Sheet: Zero75X, Ace 60X I/II, Jet 75 I, Jet 75 III, Mix 87 I.

The red "Ace 68" outcome predates the A0/flag method: the historical capture
had PID 2116, which the catalog maps to **Ace 68 Pro**, not Ace 68 I
(see KEYBOARD_SHEET_MAGNETIC_AUDIT_2026-09-20.md). The new evidence does not
change that row by itself; it is an owner decision.

Owner-supplied log `HallJoyStabilityTrace.previous (2).log` (2026-10-02,
recorded in docs/firmware/mchose-ace68/TESTER_LOG_2026-08-29_PID2116.md):
the earlier session of the same attempt; again only `matched=0`, no command
was ever sent to `41E4:2116`. The 2116 updater's USB product string is
`Ace68-II` and retail units are sold as plain "Ace 68", so the Sheet row
"Ace 68" in practice means this device, and its red status rests on attempts
that never reached it.

## Outcome (same day)

- Group A (all 11 RISC-V images): implemented on the Jet 75 II backend, see
  [MCHOSE_RISCV_FAMILY_2026-10-02.md](MCHOSE_RISCV_FAMILY_2026-10-02.md)
  (and [MCHOSE_ACE68II_2026-10-02.md](MCHOSE_ACE68II_2026-10-02.md)).
- Group B (6 ARM images besides Mix 87 III): implemented on the Mix 87 III
  backend after emulator replay, see
  [MCHOSE_ARM_FAMILY_2026-10-02.md](MCHOSE_ARM_FAMILY_2026-10-02.md).
- Not implemented: Ace 60 (ARM, different build), group C (Ace 68 GT, Turbo
  16K: handlers 03..06 not reviewed), models without an image.


## 2026-11-09 — Ace 68 GT 3837:3007

Tester log HallJoy (43).log at 02:00:36Z shows the wired GT on MI_00,
MI_01 and MI_02. All ten complete snapshots have zero sources. Current
Jet 75 and Mix 87 allowlists omit this identity. The current official
MCHOSE HUB catalog still points to the same standard 1.13 image as
October (SHA-256 fd5c2e0d66799ad43531817a122591b4d0977741dacdacd3d4235130bd55e175).
The tester firmware version is unknown. The catalog also links a separate
16K USB 3.0 updater, which was not executed or reverse-engineered.

The wired data endpoint uses MI_01, interrupt IN 82/out 04, 64-byte
payload, usage 0001:0000 and no report ID. Both firmware dispatchers
implement 03 version/build, 04 read at 0x6F000, 05 settings read at
0x65000 and 06 persistent write. The 05 response projects byte 0x20
from a scratch view. The 06 path reloads profile 64, clears a RAM
flag and can reset when the flash flag is set or the low nibble of
configuration byte 4 changes. Do not reuse the older Ace 68 write
transaction without proving its effects and restoration on GT.

The RISC-V A0 builder is at 0x8C18 and service at 0x8E80 tests RAM
configuration byte 7 bit 3. The physical table at 0xF774 matches the
vendor ANSI geometry for all 67 non-Fn HID keys and the Fn position.
A8 can set the stream bit in RAM but enters calibration and changes
sensor baseline state via 0x933E. The official UI uses A8/A9 for
calibration, and A9 clears the calibration flag, not the stream bit.
A safe activation/restore sequence and hardware multi-key trace are
still needed. No test EXE was issued. Artifacts are in
.local/research/hator80-mchosegt-20261109/.

## 2026-11-09 — Ace 68 GT tester integration

The 03 reply header in both v1.13 dispatchers contains version 0113,
four FF bytes and build fields starting with F. The response length
is generated by the firmware. Validate the checksum and bounds, then
permit the writer only for the reviewed header. Other firmware replies
and probe failures still permit passive A0 on the exact GT collection.
A8/A9 and firmware updater commands are not used.

The writer at 428C copies the 4 KiB flash sector at 4B36 and patches
the requested bytes before erase and 256-byte programs at 4B5E/4B76.
It reloads the active profile, clears the RAM byte-1 bit-0 and may
refresh hardware state or reset. The active profile selector uses
the saved 8-byte base (magic AA BB, current bounded to 0..3 and
out-of-range entry falls back to 0), as in 3FEA..40D2. The reader
at 493A overlays byte 0x20 from RAM on a saved settings slice. That
byte is excluded only from comparison, never written by HallJoy.

The ordinary GT session opens MI_01 exclusively. It checks a stable
base, settings magic and the reset flag before changing only byte 7
bit 3 in the selected stored profile. It checks the rest of the saved
settings by readback. It never overwrites a pre-enabled flag; cleanup
attempts restoration on normal shutdown while the same profile is
still proven. USB removal, process crash and a concurrent onboard
profile change can leave the flag set. No firmware version gate on
receiving A0, and no automatic repeat flash attempts in one worker
generation. The ordinary log separates caps/open, identity envelope,
writer eligibility, activation/readback, first A0, independent
channels, simultaneous keys, release and cleanup.

A0 report production at 8C18 takes the immutable factory descriptor
and current per-key depth, and the service at 8E80 iterates changed
slots. The normal producer treats raw values below 10 as zero for
change detection and submits one changed slot per service interval.
Real cadence, shallow travel, all-key chord timing, normal typing,
USB packet loss and software contention remain to be verified on
hardware. The maximum is carried in each report. Host never invents
analog from digital events. Independent analog for the 67 non-Fn
keys and the GT ANSI layout were exercised in the simulated test.

The stock 1.13 writer was also executed from 0x428C in Unicorn. The
replay models WCH compressed operations and the flash peripheral calls,
checks erase plus sixteen 256-byte programs, and compares the entire
4 KiB sector and reloaded RAM. Enable profile 0, restore profile 2 and
reset-armed profile 3 passed. The report builder 8C18..8D80 packed two
different factory slots at depth 0, 133 and 341 with maximum 341. This
starts after ADC conversion and stops before USB submission. Neither
replay models physical input, USB, power loss or the second core.

The vendor archive named 16k-USB3.0 was unpacked as data without
running its executable. It contains EnhanceHidUSB/HidUSB3 Windows
driver files and setup programs, with no alternate keyboard firmware.
Local replay: .local/research/hator80-mchosegt-20261109/replay_gt_writer.py.

The ordinary 1.6.15.0 EXE (SHA-256
4ec25f22e53df6db65a755c48693feab58001e5cfe08cf06bd98cc491da88a96)
passed four GT positive and six negative cases in the exact-image gate.
The compiled GT ANSI catalog has 68 entries including display-only Fn.
Targeted Jet75 protocol and worker session tests, profile/layout,
diagnostics and startup recovery passed. The wide native suite stopped
at two Win32 assertions (window_placement and process_generation_supervisor).
Hardware testing remains pending.

Main!C541 changed from Not investigated to Implemented; awaiting
hardware testing. Readback confirms B541:C541 base/effective yellow
(1, 0.9019608, 0.6392157), unchanged strict validation and unchanged
neighbors. Main!C362 HATOR Ultima keeps its existing yellow status.
Fresh 670-model snapshot: 283 yellow runtime notices and 90 Supported
layouts reconcile. No sheet comments or notes were added.

## 2026-11-10 — Ace 68 GT firmware 1.16, log 48

`HallJoy (48).log` (SHA-256
`7297d21c9fe3959794c03eeb54d0194c017303b2723de67a96fe2ced3b0c8c9a`)
matches the tester's 01:36:11Z report and `doesnt work` feedback. The
revision-1 backend found the control collection, opened it and received
an accepted opcode-03 reply (length 27, version 0116). The session also
read a stable profile base and valid settings before reaching
`unreviewed_writer`. That result specifically means the saved stream bit
was clear and the write eligibility check failed. HallJoy sent no opcode
06. It did listen for A0 and logged zero A0 packets, parser rejections,
and read errors. The last of four summaries was about 93 seconds after
activation. This run establishes where activation stopped. It provides no
measurement of A0 behavior with the flag enabled on 1.16.

The live official M HUB shared bundle `main.5a0731e1f799e2622d1f.js`
(SHA-256 `e30bc44b7c6a21c756917aa57e430d9706e1e8a125c89ba2f466305f34f1486a`)
links the same reviewed 1.13 image for `0x3837_0x3007` in the public web
and desktop renderer bundles. The current `keyboardConfig.json` override
for Ace 68 GT only changes manual URLs, and the public firmware-library
JSON has no GT image. The history key in the boot loader did not resolve
to a published JSON at the checked public catalog paths. In the shared
bundle, `openDebug` sets `debugMode` (byte 7 bit 3) through `getFuncConfig`
and `setFuncConfig` (opcodes 05 and 06). `startCalibration` is a separate
operation (A8). The vendor's profile-level write does not establish the
side effects of HallJoy's one-byte transaction on the tester's firmware.
Before extending the write gate, review an exact 1.16 image or independent
model-specific evidence for the writer, reload, reboot and cleanup behavior.

### Separate 117A updater found outside M HUB

A user reports receiving 117A from MCHOSE support on 2026-08-19 and
posted the [updater](https://drive.google.com/file/d/1Towayy-G2a87WwYMSM_tRO4FVyencS_K/view).
[Original report](https://www.reddit.com/r/keyboards/comments/1u9zcpe/comment/p4jlmsz/).
The downloadable `ACE 68 GT_V117A_20260729.exe` is an unsigned PE with
SHA-256 `60f81fa3110011e8599104cae7a744958abb2fd80377d9cd936991540b4bbfe6`.
The `.rsrc` `BIN/138/2052` resource at file offset `0x234B70` contains
442368 bytes, SHA-256
`41922c8fb88ef3217010f28bb4a74b027ff5fda7e7fd2395cc05e04c6c7dec24`.
Both USB cores identify the device as Ace 68 GT, `3837:3007`. Firmware
metadata at `0x5E000` and both opcode-03 dispatchers (`0x4934`, `0x47FAC`)
identify `0x117A`, with a July 29, 2026 build stamp. The tester reports
`0x0116`, whose distribution source and binary are still unverified.
This acquisition establishes that separate update packages exist even
while the public M HUB bundle points to 1.13.

117A keeps the opcode-06 profile writer (`0x4222`, `0x478A2`), saved
configuration base `0x65000`, and stream gate `0x2017A217` bit 3 in the
first core (`0x8E38`). The first-core replay passed enable and restore
with an exact whole-sector diff (one saved bit, 16 flash page programs),
plus the armed-reset branch. The first-core A0 producer (`0x8BD4` to
`0x8E18`) passed both distinct factory keys at 0, 133 and 341 with max
341. The live USB route, second-core runtime, physical behavior and
1.16 equivalence remain unverified. The updater was parsed as data.
Private inputs and replay are under `.local/research/mchose-ace68gt-20261110/`.
No runtime eligibility or support status was changed.

### Activation trial for 1.16

Owner authorized a hardware trial on the existing 1.16 keyboard. In
revision 2 the exact `3837:3007` MI_01 control collection and a valid
opcode-03 envelope reporting `0x0116` select `writer_policy=trial_0116`.
This is an evidence-based compatibility inference from the valid 1.16
03/04/05 exchange in log48, official M HUB debugMode path on the same
USB identity, and matching 06/A0 semantics in the pinned 1.13 and 117A
images. No exact 1.16 firmware or hardware write outcome is available.
117A (`0x117A`, build marker J) joins 1.13 as a reviewed writer. Arbitrary
firmware versions retain the passive A0 path.

Before a single automatic write per worker generation, HallJoy requires
the exact HID collection, exclusive access, routing ownership, the
version reply, a stable active profile with its expected signature, and
an unset reset flag. It writes only bit 3 of byte 7, verifies readback
and attempts to restore its own change on normal cleanup. A pre-enabled
flag is preserved. The opcode writes stored configuration. USB removal,
process crash or a changed active profile can prevent cleanup. Behavior
of the write, normal typing, USB timing and depth on live 1.16 and 117A
remain hardware questions. The host does not flash firmware.

`evidence.ace68gt probe revision=2` records writer policy, and subscribe
records write attempt, ACK and verification outcome. First valid sample,
first simultaneous input, first release, first rejected A0 and first other
report are retained as they occur. Open log captures available evidence
without a minimum test duration. For the tester, use the keyboard normally
in HallJoy, check movement and release, and send HallJoy.log via Open log
regardless of the outcome. Keep the current firmware.

Live Sheet readback 2026-11-10, `Main!A541:C541`, matches MCHOSE / Ace 68
GT / Implemented; awaiting hardware testing. B541 and C541 have matching
base/effective yellow backgrounds and the C541 dropdown remains strict.
The existing Experimental classification stays applicable during the trial.

The live 670-model snapshot reconciles all 283 yellow runtime notices.
The installed 1.6.15.0 `build/bin/Release/x64/HallJoy.exe` is 11,610,624
bytes, SHA-256 `1fb19571e11cb350302fb503bc22a0a21ae7d8c8d5c2eb6a95105d5df3e7cced`. The exact image passed
six GT positive and nine negative cases, the full native test suite,
production profile/layout and startup recovery. The generated identity
selects the 68-entry GT ANSI preset. Hardware results remain pending.

### Recovery image and hardware confirmation

The tester reports obtaining 1.16 by reconnecting with Esc held and
accepting M HUB's recovery prompt, from version `120` in the tester's
notation. The official Cizhou web and desktop 2.0.0 renderers currently
map bootloader `3837:3009` to the model's ordinary update URL. The
public production URL still downloads 1.13 (`0x0113` in its binary).
The published Windows installer contains no separate GT image. Which
M HUB revision or recovery catalog supplied the tester's download is
unknown. The report of `120` does not identify a verified 1.20 image.

The [vendor test firmware library](https://cdn.mchose.com.cn/configCenter-test/custom/%E5%9B%BA%E4%BB%B6%E5%BA%93.json)
separately lists `ACE 68 GT_CS45E7C6DD_V116_20260323.bin`. The downloaded
[1.16 image](https://cdn.mchose.com.cn/configCenter-test/static/binaries/20260323q3J4HoHv.bin)
is 442368 bytes, SHA-256
`992c017fabc1a5cd9b83c51392150886d74240ec1d3fa983881d8e61cf3573af`.
It identifies `3837:3007` and `0x0116` at `0x5E000`, March 23, 2026.
Both opcode-03 handlers report `0x0116` (`0x499E`, `0x47FF2`). 04/05
read the same profile settings layout, and 06 handlers at `0x428C` and
`0x478E8` use the same one-bit activation. First-core replay verified
that enable and restore only change the intended saved bit, and checked
one erase, sixteen page programs, RAM reload and the armed-reset branch.
The A0 builder `0x8C4C..0x8E90` passed the same two physical channels
at depth 0, 133 and 341, and the factory descriptor table matches 117A.
The service at `0x8EB0` checks the same bit. Scope is bounded emulation,
with first-core flash I/O modeled and no hardware driven by the replay.

The vendor test library also supplied 1.17 (`0x0117`, April 20) and its
[test keyboard configuration](https://cdn.mchose.com.cn/configCenter-test/custom/keyboardConfig.json)
listed 1.18 (`0x0118`, September 9). Both images were pinned locally and
matched the GT identity and embedded version. Their activation behavior
is outside this change. No downloaded firmware was run or flashed.

`HallJoy (49).log` (SHA-256
`ecd1c85ef3dc157ef9c74e5ebb68c3200cf87786ebdc40d6443fb82b0a9d27d0`,
2026-11-10 14:57:35 UTC) matches the tester's confirmation. Revision 2
on `0x0116` reports `subscribe result=verified write_attempted=1
write_ack=1 read_a0=1`. Real input followed, with up to 4132 updates,
0 provider failures, 67 mapped keys, 31 distinct observed channels,
a peak of five simultaneous channels and at least 219 observed releases.
The virtual left stick reached -32768 and +32767 and returned to center.
The log was captured while active, so it does not cover exit cleanup.

The exact wired GT is promoted to Supported. Revision 3 labels the
same guarded 1.16 path reviewed and drops the model's testing notice.
The hardware confirmation is specifically 1.16. 1.13 and 117A retain
their firmware-reviewed paths, while 1.17, 1.18 and reported 1.20 do not
inherit automatic activation. No distribution of vendor files is made.

The live compatibility sheet was updated at `Main!C541` to Supported.
Readback matched the green model/status format in `B541:C541` and
preserved the status dropdown and neighboring cells. The full sheet
readback found one changed model among 670. The compiled layout check
passed for 91 Supported entries, including the GT ANSI preset with
68 entries and device autoselection. The live warning audit passed for
282 models.

The full native suite passed, including six accepted and nine rejected GT
session fixtures. The normal x64 release build passed profile transactions,
layout checks and startup recovery. The installed HallJoy 1.6.15.0 EXE is
11610624 bytes, SHA-256
`982bc92f88bae2a9716681e4482e8cf608ffde0a1efab0a08da36c74037269c8`.
