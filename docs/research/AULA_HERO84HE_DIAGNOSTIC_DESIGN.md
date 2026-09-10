# AULA HERO84 HE diagnostic: executable safety contract

Status: R7 design baseline (2026-09-01).  This document is intentionally more
strict than the firmware evidence: it defines the only packets the diagnostic
is allowed to originate, the evidence required before each stage, and the
conditions that permanently stop a run.  The binary is a diagnostic recorder,
not a HallJoy input provider and not a keyboard configurator.

## Proven frame envelope

The official AULA WebHID client sends and receives report ID `09` with 63 data
bytes.  The kernel-facing HID report is therefore 64 bytes including the report
ID.  The final data byte is the complement checksum: the byte sum of report ID
and all 63 data bytes is `FF` modulo 256.  The firmware dispatcher validates
this envelope before command dispatch.

The diagnostic accepts an interface only when all of the following are true:

| Property | Required value |
| --- | --- |
| Vendor/product | `372E:103E` |
| Top-level HID usage | `FF60:0061` |
| Input/output report length | 64/64 bytes |
| Input and output report-ID capability | `09` |
| Identity reply | valid `82 01` reply with the expected six-byte device UUID |

VID/PID alone is explicitly insufficient.  A malformed reply, invalid
checksum, wrong report ID, unexpected command/subcommand, duplicate/missing
requested position, or timeout fails the current run.  It never makes the
program try another opcode, a different checksum, or a different transport.

## Immutable transmit allow-list

The source contains one request builder for each row.  The static audit rejects
all other command construction and rejects bootloader/flash/configuration APIs.

| Packet | Purpose | Evidence | Diagnostic use |
| --- | --- | --- | --- |
| `82 01 00 01 00 06` | Read identity/UUID | official client + dispatcher | admission only |
| `83 <layer> 00 01 00 2*N + BE positions` | Read current assignments | official client + dispatcher | factory-map correlation only |
| `94 02 00 01 00 2*N + BE positions` | Read current/min samples | firmware dispatcher + ordinary scanner dataflow | analog evidence only |

All remaining data bytes are zero except the listed position IDs and the final
checksum.  `N` is restricted to one for the identity command, then at most four
keys (`W`, `A`, `S`, `D`) for `83` and `94 02`; the implementation never
transmits an empty query or more than four positions.  It has exactly one
outstanding request.  A received report is fully logged before semantic
validation, but logging redacts neither timing nor payload bytes.

The following commands are compile-time forbidden: `94 00`, `94 04`, `94 05`,
`98`, bootloader/flash frames, reset/factory/profile/remapping frames, lighting,
and any HID feature report operation.  The diagnostic never sends a keyboard or
gamepad report, calls no configuration API, never persists a value, and reports
`Owns=false` for every HID key.

The dedicated build also registers exactly one native backend: the HERO84
diagnostic itself.  Normal HallJoy protocols are not merely ignored at runtime;
they are absent from this build’s backend catalog, so a connected unrelated HID
device cannot trigger their probe traffic.

## Fixed run sequence (ten seconds of active sampling)

1. Before the isolated UAP may enumerate HID paths, open one write handle and
   send the official `82 01` identity read.  Only a valid exact UUID claims the
   interface; failure sends no further packet and leaves it unclaimed.
2. Start the diagnostic worker, but park it until app.cpp confirms keyboard Raw
   Input registration.  A ten-second handshake timeout sends no active traffic.
3. Open a read-only handle and listen for 500 ms.  This preserves evidence of a
   genuinely unsolicited stream without treating its absence as a failure.
4. Revalidate `82 01`, then send one `83` read for the factory `W/A/S/D`
   positions.  Record only the
   current binding bytes; never interpret them as keyboard input or execute
   macros.
5. Send correlated `94 02` reads for the same four positions at 25 Hz for one
   second, 125 Hz for two seconds, 250 Hz for two seconds, 500 Hz for two
   seconds, then 1000 Hz for three seconds.  An advance occurs only if every
   completed reply is well-formed and exactly correlated to its request.
6. Stop on the first failed stage, release the handle, and log summaries:
   request/completion counts, no-response count, checksum/correlation failures,
   stage RTT min/mean/max, observed current/min ranges, and exact Raw Input
   press/release timestamps from `VID_372E&PID_103E`.

The device user need only press/release the four normal movement keys while the
short staged recording runs.  Normal typing is deliberately preserved: the
firmware’s normal action FIFO and USB keyboard report path are distinct from the
`94 02` vendor reply path.  The hardware test still measures that premise rather
than assuming it.

## Interpretation limits

The `94 02` "current" word is the normal scanner’s 128-sample filtered value.
Its high bit is a short-lived scanner reentrancy lock, not a pressed flag, and
is removed before analog analysis.  The returned "minimum" is an
episode-minimum maintained by the normal scan routine; it is logged for reverse
engineering but must not drive HallJoy.  A successful run proves a useful
read-only telemetry path for that exact interface/UUID.  It does **not** alone
prove 1 kHz sustained gamepad behaviour, native keyboard firmware integration,
or compatibility with another AULA revision.

## Required tests before distribution

- Pure protocol tests: checksum, builders, bounded position count, parsing,
  malformed/truncated reports, mismatched/missing/duplicate positions, and the
  high-bit masking rule.
- Static source audit: exact admission tuple, allow-list, forbidden opcode/API
  absence, `Owns=false`, dedicated MSBuild target, and target-scoped Raw Input.
- Clean release build and generated map inspection.
- One real HERO84 HE trace demonstrating identity, mapping correlation, range,
  monotonic direction, typing before/during/after, and stage reliability.
- Independent source review before the diagnostic is offered outside the test
  participant.  A production backend is a separate R9 decision and requires
  these artefacts plus a second device/revision where feasible.
