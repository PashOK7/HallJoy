# Madlions Titan68 Turbo maximum reverse roadmap

Purpose: authoritative working state for investigating whether the wired
Madlions Titan68 Turbo can provide HallJoy with continuous per-key analogue
data without breaking ordinary keyboard operation.  This is an evidence log,
not a promise that a mode is safe or that a backend exists.

Status legend: `[x] done`, `[~] in progress`, `[ ] pending`, `[!] blocked by
hardware`.  A stage is complete only when its stated exit gate is met.

## Current state

- Date: 2026-09-05.
- Active stage: **T5 analogue transport confirmed; per-device key-index
  mapping is now acquired read-only before the diagnostic mode is entered**.
- Current candidate: the physically confirmed, RAM-only `0x36,01` simulation
  mode, followed unconditionally by `0x36,00`.  The earlier `0x37,01`
  calibration hypothesis is excluded from this visual diagnostic: it is not
  needed for the confirmed stream and it is not sent.
- Candidate output: asynchronous report-ID `07` records encode a key index and
  a 12-bit travel value.  The official client multiplies that value by the
  device's configured key-step to render live travel in millimetres.
- Safety decision: `0x36` is eligible only for the isolated diagnostic image,
  never gameplay ownership.  Firmware tracing proves that it changes volatile
  RAM state rather than entering the distinct calibration/flash path.  The
  actual retail keyboard must still confirm the stream and typing coexistence.
  Neither the old MAD UAP path nor MAD68 Pro R native commands may be sent to
  this device family.

## Evidence index

| ID | Evidence | State | Fingerprint / result |
|---|---|---|---|
| T01 | Official FGG V2 Star Driver client | confirmed | `v2-hub-index-C7RydE5E.js`, SHA-256 `80F90F6EBAA8F500B3CCD68485F0949EEA41C85957E08091C068B422A1771BDD` |
| T02 | Official Titan68 Turbo normal firmware updater | confirmed | `Titan68Turbo-V0121-standard-20260203.exe`, SHA-256 `DA9003A241A102B16541341533FB43248175A71D67A89861E64B0FF680F7EFE6` |
| T03 | Official Titan68 Turbo carbon firmware updater | confirmed | `Titan68Turbo-V0121-carbon-20260203.exe`, SHA-256 `FDB08E5B311171B4C8E24A0D9FE0F3516DC9C2A1C34846BE7F1055542CDE0CF3` |
| T04 | Updater provenance and target | confirmed | PE is a 32-bit `PAN108xDfu` package; its post-PE overlay contains the exact Titan68 Turbo target name.  It is a firmware delivery package, not the normal V2 web driver. |
| T05 | Embedded payload container boundary | confirmed | PE sections end at file offset `0x391E00`; an overlay of `0x1FEE9` bytes follows.  It begins with the target filename; data begins at `0x391E80` and differs between normal/carbon packages. |
| T06 | Controller architecture | confirmed | PAN1080 SDK documentation identifies an ARM Cortex-M0 architecture; recovered application code is expected to be Thumb.  Exact Titan PCB/chip variant remains unproven. |
| T07 | V2 ordinary framing | confirmed | `buildPacket`: command, 16-bit address, length, 16-bit additive sum, result byte, payload.  Firmware-upgrade commands (`B0`--`B7`) are a separate dangerous family and are excluded from all future diagnostics. |
| T08 | Separate `0x36` live-travel candidate | confirmed in official client | `0x36,01` enters simulation/travel-test; `0x36,00` exits.  Client consumes asynchronous ID-07 records as key-index plus two 6-bit halves, yielding a 12-bit travel value. |
| T09 | `0x37` calibration semantics | confirmed in official client | `0x37,01` enters calibration; it carries ADC/calibration-progress reports.  It is a different mode and never substitutes for `0x36`. |
| T10 | Exact USB admission identity | confirmed and corrected by physical trace | Official V2 configuration maps `28E9:31FD` to the Titan model family. Firmware produces `07` on a distinct short IN endpoint. The physical V0121 trace proves the paired HID topology: control `FF87:0020`, 64-byte input/output, report `06`; stream `FF88:0021`, 3-byte input, report `07`. The diagnostic pairs only sibling interfaces of one HID parent and never writes to the stream interface. |
| T11 | Recovered normal V1.21 application | confirmed | Reproducibly extracted from overlay offset `0x391E80`; length `0x1FE50`, SHA-256 `119984FBC70A971A912E5190193B2349210AE3F69C11655A7906F5772D26CE96`, initial SP `0x20017068`, reset vector `0x0800822D`. |
| T12 | Firmware `0x36` branch | confirmed | Dispatcher `FUN_08004DCC` reaches `0x08005A70`; non-zero data sets bit 1, zero clears bit 1, in RAM byte `0x20010D0A`.  Its common tail calls `FUN_08006954`, which zeroes RAM work/key-state regions, then clears temporary buffers; no flash, bootloader, setting, or reset call occurs in the reviewed branch. |
| T13 | Firmware ID-07 producer | confirmed in both V1.21 variants | Normal `FUN_08012508` and carbon `FUN_08012514` each construct 3-byte `07,keyIndex,valuePart` records and send them directly to USB IN endpoint `0x82`; paired bit-6-marked six-bit fragments reconstruct a 12-bit Hall/travel raw value. |
| T14 | Diagnostic image and static safety gate | confirmed | `HallJoy-Madlions-Titan68-Turbo-Diagnostic` has an exact-device allow-list, sends only `0x37,01`, `0x36,01`, then `0x36,00`, logs every frame and owns no game input. It has a static prohibition on `0x37,00`. Its physical V0121 topology is explicitly split: write only report-06/`FF87:0020`, read only report-07/`FF88:0021`. |
| T15 | Normal/carbon safety comparison | confirmed for the diagnostic transport | The payloads differ and must not be cross-flashed.  The dispatcher and `0x36` handler windows (`0x08004DCC`, `0x08005A70`) are byte-identical.  Carbon has a relocated scan function, but its `0x08012D28..0x08012DB6` sequences independently prove the same key/low-half/high-half report-07 transmission on endpoint `0x82`. |
| T16 | Simulation-bit coexistence trace | static boundary confirmed | Every recovered ID-07 queue operation is inside the scan branch guarded by mode mask `0x3C`; bit-1 from `0x36` is not in that mask.  The normal key path produces report IDs `01`/`02` and the analogue path produces `07`; both are valid queue entries for IN endpoint `0x82`. |
| T17 | Transport-only runtime isolation | confirmed | The diagnostic configuration bypasses UAP and ViGEm startup, ViGEm recovery publication, and normal realtime ticking. This removes the unrelated ViGEm generation storm that previously prevented the bounded exchange from reaching the keyboard. Static audit enforces the boundary; current EXE SHA-256 `8DC2269B540FCDD26A879B3241221272BA13FFAF03190863F10AEFD72C9602E4`. |
| T18 | Physical V0121 `0x36` analogue trace | confirmed | `HallJoyStabilityTrace (11).log`: enter and exit each received ACK `0x55`; the active ten-second interval yielded 3,686 report-07 records, 1,843 well-formed raw12 pairs, zero malformed packets and zero transport failures.  Motion was observed for key indices 16, 17, 18 and 61; each of 16--18 spans raw12 0--3500.  No packets were received before the command.  The target-scoped raw-key filter is an exact `VID_28E9&PID_31FD` match; while physical motion demonstrably fed report-07, ordinary raw-key events begin only immediately after exit.  This is strong runtime evidence that stock `0x36` suppresses ordinary keyboard HID during the stream. |
| T19 | `0x36` coexistence search | in progress, narrowed | The dispatcher normalises every non-zero `0x36` payload to the same bit-1 state, so there is no hidden `0x36=2` submode.  Every recovered ID-07 producer is reachable only while one of mode bits `0x04/0x08/0x10/0x20` is set; the successful trace therefore already had a second state in effect.  `0x37,01` is the only explicit host setter found for bit `0x04`; its entry reads prior calibration RAM/flash data but does not call the persistent write primitive.  Its normal exit reaches `FUN_080015E8 -> FUN_08007FA4`, a persistent-storage write, and remains forbidden.  The other explicit setters (`0x83`, `0x84`) follow the same calibration-state family. |
| T20 | Normal-HID coexistence of the combined mode | in progress | The normal scan loop `FUN_0800C32C` calls the raw scanner, then independently sends normal key deltas unless `0x200157BD`, `0x2001589C`, or `0x20016062` is set.  It contains no direct check of the `0x36` bit (`0x02`) or the `0x37` bit (`0x04`) in `0x20010D0A`; likewise the ordinary key-delta handler has no `0x20010D0A` read.  Static evidence therefore permits, but cannot yet prove, simultaneous normal HID plus ID-07 after `0x37,01` + `0x36,01`. |
| T21 | `0x36` visual diagnostic artifact | in progress | The active command set is `0x12`/`0x16` read-only mapping, then `0x36,01 -> 0x36,00`; no calibration command is emitted. `0x36,00` is always attempted after the observation interval and during ordinary shutdown. The dedicated diagnostic image normalizes report-07 travel for the HallJoy visual path only; its isolated configuration disables gamepad output. |
| T22 | Official V2 per-device key mapping | confirmed | For `28E9:31FD`, the client binds `keyboardLayout_12797_MK25066` and `GamingProduct2`, but does **not** assume a fixed index order. It reads `0x12` device info, takes byte 4 as `key_rect_size / 3`, then reads `0x16` default key-rect triplets in offset-addressed chunks. Triplet slot `n` is the physical key index; keyboard triplets are `[10,00,HID usage]`, modifiers are `[10,modifier-bit,00]`. The diagnostic performs the same read-only acquisition and feeds `report-07 key_index` through this map into HallJoy's visual analogue path. The V2 `keyMaxMarker` sets the normalized full-scale; an absent/invalid value falls back to raw12 full scale and is explicitly logged. Mapping-read failure is non-fatal: stream capture continues and the failure is explicit in the trace. |
| T23 | Full-pipeline experimental image | implemented, hardware validation pending | `HallJoyTitan68TurboExperimental` builds an isolated Titan-only catalog without `HALLJOY_DIAGNOSTIC`; therefore ordinary HallJoy realtime processing, curves, bindings, LKP/Snap Stick and ViGEm output remain enabled. It uses the same read-only `0x12`/`0x16` mapping and reversible `0x36,01 -> 0x36,00` source contract as T21. It is intentionally separate from the visual diagnostic until a keyboard verifies end-to-end input behaviour. |

## T0 - acquisition and chain of custody

- [x] Acquire official V2 Star Driver bundle without creating browser profiles.
- [x] Find official legacy update packages exposed by that client.
- [x] Download normal and carbon V1.21 packages without executing either.
- [x] Hash every acquired artifact and identify PE/overlay boundaries.
- [~] Acquire every official Titan68 Turbo package listed for the same product,
  then classify each target name before comparing it.  The `v1.05` float-light
  item names a different underlying target and must not be assumed compatible.
- [ ] Acquire an independent Titan68 Turbo firmware version if officially
  published, and compare only after its physical revision is known.

Exit gate: all publicly listed Titan68 Turbo package variants are represented
or documented as unavailable, with URL, version, target name and hash.

## T1 - payload recovery and firmware architecture

- [x] Prove the firmware package has a post-PE overlay rather than a misleading
  `RCDATA` resource.
- [~] Trace the PAN108x DFU packager code which locates, validates and decodes
  the overlay.
- [x] Extract an immutable decoded application image and record its length,
  SHA-256, vector-table offset, flash mapping and any header/checksum.
- [x] Verify that decoded bytes form a valid Cortex-M0 vector table before any
  decompilation claim.
- [x] Compare normal and carbon decoded images at the dispatcher, `0x36`
  handler and report-07 producer level; do not cross-flash or infer
  interchangeable boards from similar package names.  Both variants prove the
  diagnostic transport, while the images as a whole remain distinct.
- [ ] Map bootloader/application split, flash-write primitives and all
  updater-only packet commands.

Exit gate: a reproducible decoder obtains a byte-identical application image
from each official package, and architecture/vector mapping is independently
validated.

## T2 - USB transport and complete command classifier

- [x] Recover V2 client-side frame builder and identify `0x36`/`0x37`.
- [x] Identify Titan VID/PID, HID report descriptor, report ID(s), endpoint
  direction and exact connected-product admission rule.
- [x] Trace firmware receive -> command dispatch -> response/transmit path for
  the sole diagnostic command (`0x36`).
- [ ] Enumerate every accepted command and subcommand from firmware, including
  undocumented branches.
- [ ] Classify every branch as read-only, transient mode, persistent write,
  calibration, reset, bootloader/flash, or unresolved.
- [ ] Trace checksum/length/error handling and packet queue arbitration.

Exit gate: every byte a diagnostic could emit is traced in firmware and no
unresolved side effect remains in the allowed set.

## T3 - `0x36` simulation/travel stream

- [x] Establish host-side semantic separation from calibration: `0x36` versus
  `0x37`.
- [x] Recover host-side tentative record decoding: records begin with a
  key-index; two repetitions encode low six bits then high six bits.  The
  value is accepted as travel only when it is no greater than 4 mm after the
  configured step conversion.
- [x] Prove the exact `0x36` firmware handler and its entry/exit state bits.
- [x] Trace every direct state mutation made by `0x36,01` and `0x36,00`:
  bit 1 of volatile byte `0x20010D0A`, followed by transient key-state cleanup.
- [~] Gate reassessment (2026-09-05): the static code does **not** show
  `0x37,01` writing any of the three normal-HID suppression gates
  (`0x200157BD`, `0x2001589C`, `0x20016062`).  `0x200157BD=8` is written on
  `0x37,00` / persistent calibration commit instead.  Thus “`0x37` always
  disables typing” is not a firmware-proven statement and must not be used as
  a conclusion.  The physical trace’s missing `raw_key` records remain an
  unresolved runtime observation, not proof of that claim.
- [x] Enumerate every USB IN submitter: ordinary NKRO keyboard data uses the
  16-byte report-ID `02` path; raw travel uses the separate 3-byte `07`
  stream.  No additional USB IN producer remains unclassified.
- [x] Trace the asynchronous producer from Hall scan/filter data to ID-07
  output for record framing and USB endpoint: `07,keyIndex,valuePart` on IN
  endpoint `0x82`; cadence, overflow and retail key mapping remain hardware
  questions.
- [ ] Prove whether output is raw ADC, calibrated travel, or a thresholded
  diagnostic value; establish polarity and range without guessing.
- [ ] Prove whether entering `0x36` suppresses ordinary key reports, changes
  scan action logic, pauses lighting, changes persistent data, or only adds a
  telemetry producer.
- [ ] Prove cleanup on explicit exit, USB disconnect, reset and malformed
  command.

Exit gate: the full call graph and wire contract of `0x36` is known, including
its coexistence/recovery behaviour.  If typing is suppressed, it cannot be a
HallJoy analogue source regardless of data quality.

## T4 - comparison with HallJoy and design decision

- [ ] Audit every registered HallJoy native backend and the Universal Analog
  Plugin against Titan transport, payload capacity and key identity.  Reuse
  host safety patterns only; never reuse another brand's command bytes.
- [ ] Determine whether the `0x36` asynchronous stream can cover enough keys
  at a measured/supported cadence for a 1 kHz game-input loop.
- [ ] Define a strict device fingerprint and non-overlapping backend ownership.
- [ ] Decide one of: safe native backend, diagnostics-only build, or no viable
  source.  The decision must cite T2/T3 evidence.

Exit gate: a concrete transport contract and no ambiguity with existing
Madlions/UAP backends.

## T5 - physical validation and any diagnostic executable

- [!] Required: a volunteer keyboard, because firmware cannot prove physical
  motion correlation, USB scheduling latency or keyboard coexistence.
- [x] Implement a TX allow-list containing only the statically proven,
  reversible `0x36,01/00` pair.  Identity is admitted by descriptor inspection,
  not by a control command.
- [x] Log all TX/RX in full, exact interface descriptors, product identity,
  stream record statistics, timestamps and target-scoped typing continuity.
  The normal completion path sends `0x36,00` unconditionally after every
  enter attempt; operating-system/power loss cannot be guaranteed in software.
- [~] Raw-input evidence qualification (2026-09-05): the `raw_key` marker is
  reached only for a Windows `RIM_TYPEKEYBOARD` packet whose device path
  matches `VID_28E9&PID_31FD`.  The old trace has no such marker, but its
  target-path filter itself has not yet been instrumented to log a non-match;
  therefore the trace narrows the issue to “no keyboard packet or a filter
  false negative”, rather than proving keyboard suppression.
- [x] Provide a read-only trace parser that verifies the canonical TX pair,
  ACK/rejection/error evidence, report-ID counts, raw12 ranges and raw-key
  presence without contacting the keyboard again.
- [ ] Keep run duration short (about ten seconds) and collect maximum evidence:
  rest/full-travel correlation, four-key chords, typing before/during/after,
  disconnect/reconnect and no persistent-setting diff.
- [ ] Only after passing the trace gate, implement a production backend and
  regression tests.

Exit gate: hardware trace matches the static contract and proves both usable
analogue motion and no unacceptable side effects.

## Explicit exclusions until proven otherwise

- Do not flash, enter bootloader, reset, change profiles, write travel/advanced
  settings, change lighting, or issue `B0`--`B7` firmware-upgrade commands.
- Do not send current Madlions UAP / MAD68 Pro R packets to Titan68 Turbo.
- Do not equate calibration (`0x37`) with the travel-test candidate (`0x36`).
- Do not claim a passive analogue report or 1 kHz effective host cadence from
  firmware/client code alone.
