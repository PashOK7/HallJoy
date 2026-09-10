# AULA HERO84HE firmware acquisition - 2026-08-31

## Result

The official AULA HUB web driver identifies the wired AULA HERO 84 HE as
`VID 372E / PID 103E`, usage page `FF60`, usage `0061`.  Its device UUID is
`18691697672197`.

The public AULA HUB API endpoint for that UUID returned the exact current
official updater:

```text
https://app.aulacn.com/commonAssets/HERO_84_HE_V2.22.exe
```

API metadata names the release `HERO_84_HE_V2.22.exe`, version `V2.22`.

## Extracted artifacts

The updater embeds a ZIP archive at offset `1,218,153`.  Its `firmware.bin`
entry is the device image, rather than a generic downloader.

| Artifact | Size | SHA-256 |
|---|---:|---|
| `HERO_84_HE_V2.22.exe` | 7,142,578 bytes | `5720C2A2CD63189E480F62555476F08FE7A851C340AF853F709F71F139D7D2C2` |
| `firmware.bin` | 164,584 bytes | `750F1A9D78F6155E894611488967CE9DE43992C34E0DDC131AFB9A975BC2D47F` |

The archive configuration specifies `upgrade_method = GEEHY_USB_V2`,
`version = 0222`, `vid = 372e`, and `pid = 103e`.  It also embeds a dedicated
`BY_UpgradeTool.exe`; do not substitute a firmware image from another Hero
model.

## Protocol conclusion after firmware and driver analysis

The `FF60:0061` fingerprint is real, but it is **not** evidence that the
Hero84HE implements HallJoy's Addressed Analog `09/94/02` protocol.  The
official legacy HERO web driver (`https://heb.aulacn.com/`) contains the
complete WebHID client for the HERO68/84/87/99 HE family.  Its frames begin
with a one-byte command and use a final-byte checksum that makes the byte sum
equal to `FF` modulo 256.

The superficially similar `94` command has a different meaning and layout:

| Operation | HERO payload prefix | Meaning |
|---|---|---|
| start calibration | `94 00` | enter/refresh calibration flow |
| stop calibration | `94 04` | leave calibration flow |
| read calibration extrema | `94 05` | per-position saved ADC maximum/minimum |
| read stored trigger distance | `93 ...` | configured distance, not live travel |
| read stored rapid-trigger setting | `99 ...` | configuration, not live travel |
| calibration-distance flow | `98 00/01/02` | calibration setup/finish |

The published client does not issue a direct live-travel poll.  The firmware,
however, has an additional `94 02` dispatcher branch.  It accepts requested
two-byte position IDs and builds six-byte records:

```text
position (big-endian) | current raw sensor (big-endian) | per-key minimum (big-endian)
```

The official driver treats the high bit of `current` as a status bit and
exposes the remaining 15 bits as the sensor-domain value, labelling it
`pressed`.  Firmware proves that this interpretation is unsafe: the bit is
copied from `+0xE0.bit0`, a per-key normal-scan re-entrancy lock set at
`0x08018B44` before action processing and cleared at `0x08018C24` immediately
afterward.  It is neither durable nor a physical key-down indication, so a
HallJoy backend must ignore it.  The independent driver receive handler decodes
the layout as `pos`, `cur`, `min`, `pressed`, and `finished`.  Firmware further
proves that `min` (`+0x1C`) is reset from filtered current when normal
processing starts and only decreases during that processing episode; it is
distinct from the calibration extrema returned by `94 05` and is not needed as
the live analogue value.  In
Hero84 V2.22, the `94 02` handler resolves a requested ID through the 108-entry
position table, reads existing normal-scan fields, and only constructs a
response.  It does not start an ADC conversion or set the global calibration
flag.  By
contrast, the `94 00` branch initializes 108 calibration entries and sets the
calibration/runtime flag, while `94 04` clears it.  Thus direct `94 02` is a
firmware-proven read-only analogue-polling candidate, not a reason to enter
`94 00` calibration.

An independent Thumb disassembly pass on 2026-09-01 confirms the exact direct
record loop rather than relying only on decompiler labels.  At `0x0801D518`,
the handler reads each requested big-endian ID from request offsets `+7/+8`,
calls the position lookup at `0x08012D5C`, skips `FF`, then calls the current
reader at `0x080127D0`.  At `0x0801D552..0x0801D574` it serializes exactly six
bytes per accepted record: original ID, reader return as big-endian current,
and the pointed `+0x1C` value as big-endian minimum.  It loops against half of
the request byte-length at `0x0801D57E..0x0801D58A`.  The response assembly at
`0x0801DA4E..0x0801DA86` writes byte length, copies the record block to vendor
payload offset `+7`, calls the checksum at `0x0801F3B0` with length `0x3F`, and
jumps to vendor TX state `0x0801E288`.  No calibration branch is reachable in
this direct-record path.

The `current` field is not a one-shot calibration sample: normal scan routines
at `0x08018A00` and `0x08018B04` maintain a 96-slot moving accumulator and
publish its bits `7..22`; the direct `94 02` reader returns that same filtered
value.  This establishes that the read path observes the normal scanner, while
not yet proving the physical range, direction, or exact meaning of `minimum`.

## Complete `94` subcommand table (Hero84 V2.22)

This table is from the firmware's own Thumb table branch at `0x0801D468`, not
from command names inferred from a UI.  The dispatcher rejects subcommands
outside `00..05`, then uses six byte offsets from `0x0801D46C`; the resolved
targets are listed below.  It is the safety basis for the diagnostic's strict
three-command allow-list.

| Subcommand | Handler | Static result | Diagnostic classification |
|---|---:|---|---|
| `94 00` | `0x0801D472` | Initializes all 108 calibration entries, calls calibration setup helpers, and sets runtime/calibration flags. | Forbidden: calibration mutation. |
| `94 01` | `0x0801D4E0` | Resolves one requested position and returns a two-byte table value.  Its observed handler body only reads tables and constructs a reply, but the semantic name is still unresolved. | Forbidden: undocumented/unneeded read. |
| `94 02` | `0x0801D512` | Iterates requested positions, reads filtered scan state with `0x080127D0`, and emits position/current/episode-minimum records. | The sole firmware-derived polling candidate; admitted only after exact identity and raw-input readiness. |
| `94 03` | `0x0801D594` | Reads a host-supplied 16-bit value and invokes `0x08016C3C`, which updates per-key configuration and schedules the deferred-save state at `0x08019288`.  The deferred worker serializes settings, including a checksum, and hands the `0x7C000/0x7E000` settings areas to the non-volatile transfer scheduler at `0x080190BC`. | Forbidden: persistent configuration write. |
| `94 04` | `0x0801D5D4` | Clears/changes calibration runtime flags and calls mode-transition helpers `0x08012034` and `0x08014A1C`. | Forbidden: calibration/mode mutation. |
| `94 05` | `0x0801D5EC` | Iterates positions and returns two stored 16-bit values using `0x08012538` and `0x08012554`.  The official client uses this as calibration-extrema data. | Forbidden: calibration data flow, unnecessary for live polling. |

The official archived driver contains literal builders for `94 00` and
`94 05`, but none for `94 01`, `94 02`, `94 03`, or `94 04`.  Its `94 05`
request begins `94 05 00 01 00 01 00 00`; this independently separates the
documented extrema query from the direct `94 02` record format.  The absence
of a published `94 02` builder is not proof of danger: the firmware read/write
set above is the controlling evidence.  It does mean production support must
remain gated on a real-device trace.

Normal typing has a separate application path.  The scan action handlers queue
32-bit key events through `0x08012F74` into a FIFO rooted at `0x20006660`; the
ordinary USB task at `0x08010FC4` dequeues and formats those keyboard/consumer
reports.  Direct `94 02`, by contrast, constructs a report-ID-09 vendor reply,
checksums it at `0x0801F3B0`, and enters a distinct vendor TX state machine.
It neither modifies the typing FIFO nor changes scan/calibration state.  The
two streams still share the physical USB device, so only a hardware continuity
run can rule out congestion at an aggressive polling rate.  HallJoy must use
small selected-key batches (one to nine; never a realtime all-key sweep).

The candidate wire contract is report ID `09` plus a 63-byte payload.  A
request starts `94 02 00 01 00 <2*N>`, contains up to nine two-byte position
IDs, and ends with the official checksum: the sum of report ID and all 63
payload bytes is `FF` modulo 256.  The response has the same command/subcommand
and six bytes per requested position.  This contract still requires physical
validation of typing continuity, value direction/range, rate, and the exact
HID interface; it must not be called production support yet.

The legacy driver also provides the required safe identity/layout material for
an eventual dedicated backend.  `82 01` reads the 48-bit family UUID; the
Hero84 UUID is `18691697672197`.  Read-only `83` returns the current assignment
for requested position IDs.  Its bundled `AULA_2829` factory layout identifies,
for example, `W/A/S/D = 30/43/44/45` and `Space = 70`.  This is a distinct
`83` operation from Addressed Analog's `09 83 00` map and must never be
confused with it.  The complete 83-key factory mapping, with the official
driver asset fingerprint, is recorded in `AULA_HERO84HE_FACTORY_MAP.md`; it is
only a default-layout reference until the live device's read-only `83` result
is checked.

The precise safe read contracts are now static evidence, not guesses.  The
official client sends UUID request `82 01 00 01 00 06`; Hero84 branch
`0x0801C554` only copies the six-byte identity value into the reply.  It sends
assignment request `83 <layer> 00 01 00 <2*N>` followed by N big-endian
position IDs; firmware branch `0x0801C5E6` reads the assignment store and
returns one six-byte record per ID: position (big-endian) plus four-byte current
assignment.  The client recognises leading assignment byte `03` as a macro;
the diagnostic should log but never execute or emulate such assignments.

In particular, the Hero frame starts with `94`; HallJoy Addressed polling
requires an entirely different frame beginning `09 94 02` and first proves a
map with `09 83 00`.  Neither Addressed byte prefix occurs in this exact
firmware image.  The Addressed backend must therefore not claim this interface
or send its packets to a HERO keyboard.

The official HERO68 V3.23, HERO84HE V2.22 and HERO99HE V1.4 images are a
related GEEHY family: each has the application vector at file offset `8000`
with reset handler `080101A9`, and the same HID-descriptor marker around
`280D0` containing `FF60:0061`.  Their code is not byte-identical, so layouts
and settings must remain model-specific.  The official client assigns the same
`VID 372E / PID 103E`, report ID `09`, 63-byte payload and `kb_by_v3_wired`
transport to Hero68, Hero84 and Hero99.  Hero68 V3.23 has the same direct
`94 02` sample-record call path.  The separately acquired official Hero99 V1.4
firmware (updater SHA-256 `4DC61E7B35458725FF3FA9F849FF004C12D2F38269BEF109EF92C00D1BD76EDC`,
firmware SHA-256 `C5A9582A5B364C50BB6CBF0BDB6776C12BB39F182A3D263F9013638907C3E144`)
confirms it statically: dispatcher `0x0801F4CE`, `94 02` loop
`0x0801F596`, ID lookup `0x0801357C`, and record reader `0x08012F54` have the
same read-only/current/lock-bit/episode-minimum roles.  This is strong family
evidence, not permission to admit a Hero99 without its own UUID/layout proof
and hardware coexistence check.

Consequently, no existing HallJoy AULA backend applies:

- AULA WIN60HE MAX uses `FFA0:0001` and `5C/12/23/2B` frames.
- AULA Standard/W669 uses `FF1B:0091`, report ID `01`, and `0D/18/21` frames.
- Addressed Analog uses `09/83/94/98` frames despite sharing `FF60:0061`.

The next HallJoy step is a dedicated HERO read-only diagnostic, not reuse of
any existing AULA backend.  It should poll only direct `94 02`, prove exact
request/response correlation and checksum, record raw ranges and keyboard
typing before/during/after the short run, then close without ever sending
`94 00`, `94 04`, `94 05`, `98`, or a configuration command.

## Acquisition method

No browser profile or physical device was used.  The current AULA HUB bundle
contains the HERO 84 HE device UUID and calls
`GET https://hubapi.aulacn.com/user/EXE/getFile/<uuid>` to obtain the official
asset filename.
