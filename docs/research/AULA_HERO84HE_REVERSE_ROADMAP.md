# AULA HERO84 HE maximum reverse roadmap

Purpose: working state machine for the HERO analogue investigation.  This file
is the authoritative progress record.  Update it before changing stage, after
new evidence, and before preparing any tester artefact.

Status legend: `[x] done`, `[~] in progress`, `[ ] pending`, `[!] blocked or
requires hardware`.  A stage may be marked done only when its exit gate is met.

## Current state

- Date: 2026-09-01.
- Active stage: **R8 - controlled physical validation** (hardware required).
- Current decision: do **not** build or send a tester yet.
- Safety policy: no firmware flash, bootloader, factory reset, profile/remap,
  lighting, `94 00`, `94 04`, `94 05`, or `98` command may appear in a future
  first diagnostic build.
- Existing HallJoy backends are not protocol-compatible with HERO and must not
  claim its interface.

## Evidence index

| ID | Evidence | Status | Location / fingerprint |
|---|---|---|---|
| E01 | Official Hero84 updater | confirmed | `HERO_84_HE_V2.22.exe`, SHA-256 `5720C2A2CD63189E480F62555476F08FE7A851C340AF853F709F71F139D7D2C2` |
| E02 | Extracted Hero84 firmware | confirmed | `firmware.bin`, SHA-256 `750F1A9D78F6155E894611488967CE9DE43992C34E0DDC131AFB9A975BC2D47F` |
| E02b | Extracted Hero99 firmware | confirmed | official V1.4 updater SHA-256 `4DC61E7B35458725FF3FA9F849FF004C12D2F38269BEF109EF92C00D1BD76EDC`; firmware SHA-256 `C5A9582A5B364C50BB6CBF0BDB6776C12BB39F182A3D263F9013638907C3E144` |
| E03 | Official legacy HERO WebHID client | confirmed | `https://heb.aulacn.com/app-B6-MiDbc.js` |
| E03a | Archived official WebHID client | confirmed | SHA-256 `16030C913FE6F6A4B0326A346EC9920A6A33BA16FE569BFCB4C5B6E9CE6BBAAB` |
| E04 | Hero84 transport identity | confirmed | `372E:103E`, report ID `09`, 63-byte payload, model UUID `18691697672197` |
| E05 | `94 00/04/05` calibration semantics | confirmed | official driver + Hero84 dispatcher |
| E06 | Direct `94 02` handler | confirmed | Hero84 `0x0801D512`; response records are position/current/minimum |
| E06a | Direct-path read/write set | confirmed | ID lookup `0x08012D5C` and record reader `0x080127D0` are read-only; the handler writes only stack/TX response state |
| E06b | Normal scan source for `current` | confirmed | `0x08018A00`/`0x08018B04` maintain a 96-slot moving accumulator; `94 02` reads its bits `7..22` |
| E06c | `current` high-bit status | confirmed | `0x08018B44` sets `+0xE0.bit0` only as a per-key normal-scan re-entrancy lock; `0x08018C24` clears it after that key's action handler |
| E06d | Direct-record `minimum` | confirmed | `+0x1C` is seeded from filtered `current` on a normal-scan processing episode and subsequently updated only when a lower filtered value appears; it is not the `94 05` calibration extrema store |
| E09 | Normal typing and vendor response separation | confirmed | scan action handlers enqueue 32-bit keyboard events at `0x20006660` through `0x08012F74`; `0x08010FC4` dequeues them for ordinary HID handling, while `94 02` builds its own report-ID-09 response at `0x0801DA4E..0x0801E288` |
| E10 | Safe identity and mapping reads | confirmed | `82 01 00 01 00 06` returns the six-byte UUID through pure-copy branch `0x0801C554`; `83 <layer> 00 01 00 <2*N>` returns 6-byte position/assignment records through the read-only `0x0801C5E6` path |
| E11 | Independent Thumb instruction pass | confirmed | `94/02` loop at `0x0801D518..0x0801D58A` loads request IDs, calls `0x08012D5C` and `0x080127D0`, serializes six-byte records; `0x0801DA4E..0x0801DA86` copies payload, checksums `0x3F` bytes at `0x0801F3B0`, then enters TX state `0x0801E288` |
| E12 | Complete `94 00..05` jump-table classification | confirmed | Thumb `tbb [pc,r3]` at `0x0801D468` maps `00..05` to `D472/D4E0/D512/D594/D5D4/D5EC`; `03` persists a per-key setting, `00`/`04` mutate calibration state, and `05` is the official extrema path |
| E13 | `94/03` non-volatile save chain | confirmed | `0x08016C3C` updates the live per-key setting then marks deferred save; `0x080192A8..F8` invokes `0x080164A4` for settings areas `0x7C000/0x7E000`; `0x080164A4` serializes/checksums 0x1EC0 bytes then hands it to transfer scheduler `0x080190BC`, whose sole static caller is `0x080164E4` |
| E14 | HERO USB descriptor load path | confirmed | initializer table at raw `0x21D98` LZ-decompresses `0x08030000` into `0x20000000` (0x450 bytes) before USB use; the archival official client independently requires HERO84's normal HID input/output report ID 09 with 63-byte payload on its `FF60:0061` vendor collection |
| E15 | Separate normal-event and vendor-reply paths | confirmed | ordinary USB task `0x08010FC4` consumes a 32-bit ring event through `0x08011BEC` (ring base `0x20005824`, producer/consumer bytes `+0x401/+0x400`); direct `94/02` instead returns through the checksum routine `0x0801F3B0` and vendor TX state machine entered at `0x0801E288` |
| E07 | Hero68 direct `94 02` analogue path | confirmed | Hero68 V3.23 equivalent sample-record call path |
| E08 | Hero99 direct `94 02` analogue path | confirmed | V1.4 dispatcher `0x0801F4CE`, subcommand 2 loop `0x0801F596`, ID lookup `0x0801357C`, direct reader `0x08012F54` |

Supporting narrative and acquisition details live in
`AULA_HERO84HE_FIRMWARE_2026-08-31.md`.

## Non-negotiable protocol facts

- HERO framing is **not** HallJoy Addressed Analog.  HERO begins the payload
  with `94`; Addressed uses `09 94 02` and map `09 83 00`.
- HERO checksum includes report ID `09`; the complete report sum is `FF` modulo
  256.
- The Hero84 official model uses `AULA_2829` layout.  Known positions:
  `W=30`, `A=43`, `S=44`, `D=45`, `Space=70`.
- The firmware dispatcher handles `94 00..05`.  `94 00` sets calibration state;
  direct `94 02` does not write that state in its own handler.
- A `94 02` batch is expected to contain at most nine two-byte position IDs,
  and the response contains six bytes per position.

## R0 - acquisition and reproducibility

- [x] Download exact Hero84 official updater without running it.
- [x] Extract embedded `firmware.bin` and record hashes/configuration.
- [x] Acquire Hero68 and Hero99 official comparison images in memory and record
  version/hash/vector observations.
- [~] Acquire Hero87, Hero84 JIS and any later Hero84 firmware variants.
  Public-archive audit: on 2026-09-01 the official
  `hubapi.aulacn.com/user/EXE/getFile/18691697672214` (Hero87) and
  `.../18691697672245` (Hero84 JIS) endpoints answered `code:0`, "file
  information unavailable".  Do not substitute a sibling image; retry only
  when AULA publishes an official asset or a user supplies its updater.
- [x] Archive exact driver asset hash and reproducible extraction notes.

Exit gate: all public Hero sibling variants discoverable from the official
catalog are represented or explicitly unavailable, with reproducible hashes.

## R1 - image architecture, memory and USB transport

- [x] Locate Hero84 application vector table at file offset `0x8000`; reset
  handler `0x080101A9`.
- [x] Identify USB command dispatcher and primary `0x94` branch: dispatcher
  `0x0801C460` validates the 63-byte checksum before dispatching command byte
  `+1`; `0x94` routes to `0x0801CCF0` and the direct `02` record loop is
  instruction-traced in E11.
- [~] Decode the actual HID report descriptor completely, including interface,
  endpoint and vendor collection relationship.  The flash blob is LZ-loaded
  into RAM, so static template bytes alone are not a valid descriptor; vendor
  collection/report-ID/length are independently corroborated by E14.  Exact
  keyboard/consumer collection and endpoint scheduling remain to be traced.
- [~] Recover USB ISR, TX/RX queue implementation, packet allocation and
  checksum validation paths.  E15 proves the separate application queues and
  `0x0801F3B0` proves the checksum routine; endpoint ISR/DMA arbitration and
  the final physical IN scheduler remain to be traced or measured.
- [ ] Map RAM structures: incoming frame, outgoing frame, command state,
  keyboard matrix, ADC samples, calibration min/max and persistent settings.
- [ ] Identify flash-write primitives and enumerate every caller.

Exit gate: a named memory/transport map exists and every packet field used by a
candidate backend is traced from USB receive to response transmit.

## R2 - full command dispatcher and mutation classification

- [x] Identify Hero command family and the `0x94` dispatcher split.
- [x] Prove driver meanings of `94 00`, `94 04`, `94 05`, `98 00/01/02`, `93`
  and `99`.
- [x] Reconstruct every `0x94` subcommand (`00..05`) from firmware control flow;
  see E12 and the exact handler/safety table in the firmware report.
- [ ] Enumerate all dispatcher opcodes/subcommands, including undocumented ones.
- [ ] For every branch classify: read-only, RAM-only, persistent write,
  calibration, reset, bootloader/flash, or unknown.
- [ ] Trace error cases: bad checksum, invalid length, unknown position, queue
  exhaustion and malformed fragment.
- [ ] Produce an emitted-command allow-list for the future diagnostic.

Exit gate: no command eligible for diagnostic has an unresolved side effect;
all unclassified commands are absent from diagnostic code.

## R3 - direct `94 02` analogue path

- [x] Locate direct Hero84 `94 02` branch in dispatcher.
- [x] Prove its output record order: two-byte position, two-byte current,
  two-byte minimum.
- [x] Prove current's high-bit status handling from the official receive parser.
- [x] Prove branch-local absence of calibration-flag write; distinguish it from
  `94 00` initialization and `94 04` exit.
- [x] Prove equivalent Hero68 sample-record route.
- [x] Trace direct record reads: `0x08012D5C` maps an input ID through the
  108-entry position table; `0x080127D0` reads existing scan fields and writes
  only the caller's response record.
- [x] Determine direct-path mutation: `94 02` does not trigger ADC conversion,
  calibration initialization, persistent storage or a calibration flag.  It
  reads current scan-loop state.
- [x] Trace `current` producer: `0x08018A00` and `0x08018B04` maintain a
  96-slot rolling accumulator.  New sample minus prior ring sample updates
  field `+0x1F0`; `current` is its bits `7..22`, a 128-sample moving average.
  The same source feeds normal key processing.
- [x] Trace `minimum` and high-bit status: `+0x1C` is seeded from filtered
  `current` at normal-scan episode entry and monotonically tracks the lower
  filtered value until that episode resets.  It is a transient scan-state
  minimum, not the separately queried `94 05` calibration extrema.  The
  `current` high-bit instead mirrors `+0xE0.bit0`, a brief re-entrancy lock
  set before per-key normal processing and cleared immediately after it; it is
  not a stable physical pressed state and must be ignored by HallJoy.
- [x] Prove record batching: the response is six bytes per accepted position;
  diagnostic must cap requests at nine positions, the maximum fitting the
  63-byte payload.
- [!] Establish raw range, polarity, zero/rest value and physical full travel
  representation from a hardware trace; firmware alone cannot attach sensor
  units to physical millimetres.
- [x] Identify direct-handler calibration condition: none is read on the
  `94 02` path after dispatch; invalid position IDs are skipped rather than
  entering calibration.

Exit gate: direct polling's complete call graph, read/write set and exact wire
contract are documented; only physical properties remain unproven.

## R4 - ordinary typing coexistence and timing

- [x] Trace normal matrix scan -> keyboard HID report path: scan action
  handlers call `0x08012F74`, which appends a 32-bit key event to the separate
  `0x20006660` FIFO; the ordinary USB task at `0x08010FC4` dequeues it through
  `0x08011BEC` and dispatches standard keyboard/consumer HID report handling.
- [x] Trace raw scan -> `94 02` response path: the command branch populates a
  63-byte report-ID-09 vendor response, computes its checksum at
  `0x0801F3B0`, then enters the vendor transmit state machine at
  `0x0801E288`; it does not enqueue keyboard events or touch the scan action
  FIFO.
- [!] Identify shared USB locks, DMA descriptors, endpoint arbitration, timers
  and priority.  The two paths have distinct application queues/state
  machines, but both ultimately require the device's USB IN scheduling; a
  static non-interference proof is not available.  The bounded R8 matrix owns
  this remaining hardware-only question.
- [x] Prove direct `94 02` does not set a keyboard-suppression or calibration
  condition through its branch-local call graph.  A hardware typing-continuity
  test remains mandatory because USB queue pressure is a separate question.
- [x] Calculate response capacity: one request/response carries 1..9 keys;
  payload cost is `6*N` response bytes (6..54), leaving nine bytes at `N=9`.
  An 83-key sweep needs ten transactions and is categorically unsuitable for
  a 1 kHz game-input loop.
- [x] Derive the only defensible static rate model: issue exactly one
  outstanding request, so completed sample rate is bounded by
  `min(host cadence, 1/round-trip time, available USB IN scheduling)`.  The
  descriptor/firmware does not prove the host-visible interval, therefore
  `1 kHz` is an attempted ceiling, not a supported claim before measurement.
  Diagnostic stages are 25 Hz for 1 s, 125 Hz for 2 s, 250 Hz for 2 s,
  500 Hz for 2 s, then 1 kHz for 3 s, advancing only after complete correlated
  responses; each uses the four-key WASD batch.
- [x] Define stale/release policy for the future backend: direct `current` is
  a continuous scanner value rather than a key event; publish zero on a
  bounded no-response timeout or disconnect, never from its high-bit lock.

Exit gate: a static upper-bound model and a hardware test matrix exist for input
continuity, latency, rollover and recovery.

### R4 hardware matrix prepared for R7/R8

1. Identity: exact `FF60:0061`, report ID/length, checksum and UUID.
2. Safety: no forbidden TX bytes; log all TX/RX; no calibration/profile/light
   changes before/after exit.
3. Analogue: one-key movement/rest/full-travel correlation, polarity, range,
   noise, monotonicity and stable rest.
4. Coexistence: normal typing before/during/after each stage, held WASD, and a
   four-key chord; record Raw Input timestamps from the exact keyboard path.
5. Transport: per-stage sent/received/correlated/duplicate/foreign/timeout
   counts, RTT median/p95/p99, disconnect/reconnect and final neutral state.

The normal staged run is ten seconds.  Any failed stage stops escalation and
records its evidence; it never retries by sending another protocol command.

## R5 - key identity, layout and remapping

- [x] Extract official `AULA_2829` factory layout and initial important IDs.
- [x] Identify and reverse official read-only `83` assignment query: request
  `83 <layer> 00 01 00 <2*N>` plus N big-endian physical IDs; response records
  are big-endian position plus four-byte current assignment.  Firmware path
  `0x0801C5E6` reads assignment storage and builds only a reply.
- [x] Extract complete 83-key Hero84 position -> factory HID mapping; see
  `AULA_HERO84HE_FACTORY_MAP.md`.  It is a default-layout reference only.
- [x] Reverse `83` in firmware: layer is the second byte, request IDs are
  big-endian, and each response record is position + four-byte assignment;
  a macro assignment is marked by leading type byte `03` in the official
  client.  Diagnostic records it but never interprets it as an analogue key.
- [ ] Determine exact handling of Fn, media, macros, advanced keys and layers.
- [x] Recover UUID read (`82 01 00 01 00 06`) in firmware and prove it is
  read-only: branch `0x0801C554` copies the six-byte identity value to TX only.
- [x] Define strict admission: `372E:103E`, vendor usage `FF60:0061`, exact
  report-ID-09/63-byte shape, UUID `18691697672197`, valid checksum, and
  correlated `94 02` records.  VID/PID alone never admits an interface.

Exit gate: backend can identify a Hero84 layout exactly and either follow a
safe remap or reject it without guessing.

## R6 - sibling comparison and HallJoy architecture

- [x] Establish Hero68/84/99 common GEEHY image family, reset vector and
  official driver transport.
- [x] Compare `94 02` implementation across Hero68/84/99 at function/control
  flow level.  Hero99 V1.4 has the same read-only record loop, normal-scan
  current source, transient lock-bit and episode-minimum pattern as Hero84;
  its offsets differ and do not authorize a shared unfingerprinted backend.
- [ ] Compare Hero87, JIS and later revisions; record layout-only versus
  transport changes.
- [x] Rule out reuse of Addressed, AULA MAX and AULA W669 backends.
- [x] Compare batching, stale-data, normalization and reconnect designs across
  every HallJoy native protocol and UAP; only the generic host-side safety
  patterns are reusable, never an on-wire builder (R6 comparison below).
- [x] Write the dedicated `AulaHero` diagnostic transport design and parser
  tests before implementation.  Contract: `AULA_HERO84HE_DIAGNOSTIC_DESIGN.md`.

Exit gate: the proposed backend has a bounded family contract and no ambiguous
ownership overlap with another HallJoy reader.

### R6 complete implementation comparison (2026-09-01)

This is a source audit of every descriptor registered by
`native_analog_backends.def`, plus the isolated Universal Analog Plugin (UAP)
host.  “Reusable” means a host-side safety mechanism only; it never means that
HERO may emit that protocol's probe or poll bytes.

| HallJoy source | Acquisition model | Probe / ownership consequence | What HERO may reuse | Why the wire protocol is excluded |
|---|---|---|---|---|
| MAD68 A0 | unsolicited per-key stream after a reversible activation sequence | `A8/A9` control sequence; needs Raw Input correlation | per-key freshness and neutral-on-stale | HERO has no A0 descriptor or activation flow. |
| ATK x QK Hex80 | dedicated polled HID channel | read-only semantic proof on a different usage/report contract | one-outstanding-request and correlated reply | no HERO `0x96` grammar or Hex80 interface fingerprint. |
| Addressed Analog | selected-key polling, up to nine keys | only claims after exact map and checksummed `09 94 02` reply | selected-key batch scheduler, RTT accounting, stale neutralisation | HERO payload begins `94`, has a different `83`, checksum and record format; it lacks the Addressed map precondition. |
| AULA WIN60HE MAX / SparkPlayJoy | family-specific polling | its own semantic proof and profile identity | strict path claim/reconnect discipline | `FFA0:0001` and 6x21 frames, unrelated to `FF60:0061` HERO. |
| AULA W669 | event stream | reversible control probe, distinct `FF1B:0091` interface | stream health/recovery accounting only | different report ID and `0D/18/21` wire family. |
| SparkLink / XD row | polling with permissive historical transport fallbacks | control probe can try output/feature transport forms | do not copy its multi-transport fallbacks; copy only bounded stop/neutral lifecycle | fallback behaviour is too broad for a first HERO diagnostic and packet grammar is unrelated. |
| Sayo depth | polling depth reports | reversible probe and runtime HID-to-index remap | runtime key identity validation | no HERO position-ID frame or sensor encoding. |
| IROK ND75 experimental | live event stream | identity/capability request then stream | identity-first and reconnect fail-closed behaviour | no common report syntax or Hall scanner layout. |
| DrunkDeer diagnostic | polled complete tracking matrix | one tracking request, multi-report frame decoding | malformed/foreign frame accounting and trace discipline | its 59+59+8 matrix is not a selected-key HERO response. |
| MCHOSE diagnostic | separately compiled no-input diagnostic | model-specific narrow allow-list | isolation of a diagnostic target from normal backends | its calibration/vendor family has no evidential connection to HERO. |
| Universal Analog Plugin host | third-party plugin publishes HID usages through an isolated child process | UAP must not open a path already proven/claimed by a native backend | HID-usage projection and neutral snapshot semantics | UAP has no generic way to discover or generate HERO's private direct poll; treating it as one would be a fabricated plugin. |

Conclusion: the production HERO backend, if hardware validation passes, will
use a dedicated `aula-hero` parser and builders.  It may retain the Addressed
style of selected-key scheduling and stale-value neutralisation, but it must
retain the diagnostic's exact `372E:103E` + `FF60:0061` + UUID + report-ID-09
admission.  It must never inherit SparkLink/Sayo transport fallback or any
activation/calibration command.

## R7 - diagnostic EXE design (do not implement before R3/R4 gates)

- [x] Specify only read-only command builders: identity, `83`, direct `94 02`.
- [x] Add compile-time/static audit that rejects all forbidden command builders
  and configuration-capable HID APIs.
- [x] Implement strict checksum, report-ID, exact-ID correlation and bounded
  parser, with portable malformed/checksum/correlation tests.
- [x] Implement staged rate test, beginning at 25 Hz and requiring one complete
  response before the next request.
- [x] Log identity, UUID, requested/returned IDs, raw range, scanner-lock bit,
  RTT, stage request/completion counts and target-scoped Raw Input edges.
- [x] Never create gamepad output in diagnostic mode (`Owns=false` for every
  key; no normalized value publication).
- [~] Build simulator/fuzz coverage for malformed/stale/foreign reports.  Pure
  parser negative cases are present; transport-level replay/fuzz remains R8/R9
  hardening work.
- [x] Build after source-level allow-list review and independent static audit.

Build evidence (local, 2026-09-01):

- `HallJoy-AULA-HERO84HE-Diagnostic.exe`, MSVC Release/x64 SHA-256
  `403026295C2B9A866D61B2306F2D2EAD2DB52724663F339AD0364BAF547696A4`.
- Matching map SHA-256
  `0444726788D1C942EC509BAADCBDC47759EA19C42E65C472FFD3C90A34EDB343`.
- The active poll worker waits for app-confirmed keyboard Raw Input before it
  can send `83` or `94/02`; the pre-UAP admission stage can send only `82/01`.
  The diagnostic catalog contains only the HERO84 descriptor, excluding all
  foreign native backend probes from this image.
- `aula_hero84he_diagnostic_static_audit.py` passed, and the pure protocol test
  passed using the portable C++ compiler.

Exit gate: **met for the source and built image’s controlled transmit surface**:
the static audit permits only `82/01`, `83`, and `94/02`, and rejects feature,
output-report configuration and all excluded builders.  A real device is still
required to prove the assumed response framing and device behaviour.

## R8 - controlled physical validation

Prerequisite: R7 complete.  User run must be short and structured, not an open
ended experiment.

- [!] Confirm device identity and UUID.
- [!] Confirm one-key `94 02` response and exact checksum.
- [!] Confirm movement correlation and raw polarity/range.
- [!] Confirm typing before/during/after direct polling.
- [!] Measure low-rate, target-rate and multi-key throughput.
- [!] Check held key, release-to-zero, disconnect/reconnect and application exit.
- [!] Confirm no changed profile, calibration prompt, lighting, mapping or
  persistent state.

Exit gate: all tests pass on an exact production-candidate binary and the log
contains enough evidence to reproduce the parser assumptions.

## R9 - production support

- [x] Implement isolated `AulaHero84He` experimental backend behind exact
  `372E:103E` + `FF60:0061` + report-ID-09 + UUID capability proof.
- [x] Read the live layer-0 mapping through official read-only `83`; only
  plain, unique HID assignments are admitted. Macro and internal Fn entries
  are skipped rather than reinterpreted.
- [x] Add selected-key polling (maximum nine positions, one outstanding
  request), freshness neutralisation and lifecycle stop/cancel handling.
- [~] Adaptive per-key normalisation uses only observed release maximum and
  press minimum. It is deliberately marked experimental: no hardware trace
  yet proves range, full-travel calibration, typing continuity or 1 kHz rate.
- [ ] Add protocol/unit/fuzz/reconnect tests and output ownership checks.
- [ ] Require separate validation for each non-identical Hero model/revision.
- [ ] Promote the experimental entry to public supported status only after the
  exact device passes R8.

### R9 experimental build evidence (local, 2026-09-04)

- Standard MSVC Release/x64 build completed with the production descriptor
  registered before UAP startup.  Output:
  `src/HallJoyProject/x64/MAD68ProRNative/HallJoy.exe`, SHA-256
  `B0E59ED554CCA5D5938AB4A0D94A96A61BEBEAD8742D1BE46140655B65B6B3C4`.
- The portable packet test proves four-key and maximum nine-key requests,
  checksum rejection and record correlation.
- `aula_hero84he_backend_static_audit.py` is part of
  `tools/run_native_backend_checks.py`; it passed together with the existing
  static suite.  It pins the exact identity, live `83` map admission,
  allow-list, stale-neutral behaviour, bounded cancellation and the absence
  of calibration/configuration builders.
- This is an opt-in-by-hardware experimental route only.  No claim is made
  that the observed adaptive range is calibrated, that ordinary typing is
  unaffected, or that actual 1 kHz transport works until R8 evidence exists.

## Immediate next actions

1. Run R8 only on a physical exact HERO84 HE and archive its log.  The binary
   deliberately stops on the first unexpected frame and never escalates to
   another command.
2. Add a replay corpus from that first trace, then transport-level malformed,
   stale and disconnect fuzzing before any R9 production route.
3. Recover USB endpoint/queue ownership and scheduling to bound interleaving
   of ordinary keyboard reports with vendor `94 02` responses.
4. Continue Hero68/99 semantic comparison; do not infer Hero99 support from
   VID/PID or product name.
