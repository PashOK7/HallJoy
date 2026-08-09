# AULA WIN60 HE Standard / W669 native analogue protocol

Status: `firmware-proven / official-driver-cross-checked / physical live stream proven / host snapshot-contamination correction pending physical rerun`.

This is a separate protocol family from the existing WIN60 HE MAX backend. It
must not be routed through the MAX `5C/12/23/2B` transport.

## Proven identity

The supplied updater contains a raw 256 KiB Cortex-M image with this identity:

```text
Firmware family         W669 / SI2825KZHEARGB
Firmware version        V3.17.08, Aug 3 2026 17:17:37
USB VID/PID              2E3C:C365
Vendor HID               FF1B:0091
HID report               report ID 1 + 63-byte payload
Protocol IN endpoint     0x83, 64 bytes including report ID
Logical matrix           6 rows x 22 columns (132 positions)
Known physical keys      61
```

The firmware contains the exact vendor report descriptor for report ID 1. It
also contains a 39-entry opcode dispatcher covering `00..26`; the analogue
path is opcode `21` and the current base-layer key map is opcode `18`.

The official AULA driver currently lists several products on the same
`2E3C:C365` transport, including WIN60 HE, WIN68 HE and KP-TE153 variants. A
matching VID/PID is therefore only a candidate signal, not a model identity.
The complete HID shape plus read-only travel and map responses identify a
protocol session; PID alone never does.

The official driver does not identify those geometries from PID. It first sends
read-only opcode `0D`, parses the fifth CSV field as the firmware product, and
then loads `config/keys/<product>.json`. HallJoy implements the same offline,
bounded classification and embeds the audited official factory maps:

| Firmware products | Geometry | Official layout SHA-256 |
|---|---:|---|
| `SI2825HEARGB`, `SI2825KRT12HEARGB`, `SI2825KR-AHEARGB`, `SI2825KZHEARGB` | 61-key WIN60 | `04E2FDA00FDB1645C74D42121102C1CF233658DDF43FB2611ACE57460CFCB448` |
| `SI2828HEARGB`, `SI2828KZHEARGB` | 68-key WIN68 | `CC2CBBC9C051230279BA8CE3B52054739446DC19C78CF1A38ADFD2D7C6CDF9E1` |
| `SI2851UKKZHEARGB` | 69-key KP-TE153 UK | `FCB98F5DF82C2E00D94501389C452172C4FAB6F103DF3CBF489FFFE9B89945C3` |

WIN68 and KP-TE153 are implementation-tested from official protocol/layout
evidence but remain physically unvalidated until their own hardware is tested.

## Canonical frame indexing

Windows HID reads and writes 64 bytes including the report ID:

```text
wire[0]  report ID = 01
wire[1]  opcode
wire[2]  opcode-specific selector
wire[3]  fragment high byte / reserved
wire[4]  fragment low byte / reserved
wire[5]  payload length
wire[6]  analogue subcommand or response subtype
wire[7+] data
```

WebHID removes `wire[0]`, so every index in the official JavaScript is one
less than the Windows wire index. The firmware internally prefixes another
queue marker byte; that marker is not part of USB traffic.

Responses are asynchronous and have no transaction ID. A receiver must keep
reading, route frames by opcode and subtype, and tolerate unrelated valid
events while waiting for a requested response. It must never assume that the
first report after a write belongs to that write.

## Read-only admission proof

A safe HallJoy session can be proved without changing calibration, bindings or
persistent keyboard state:

1. require `FF1B:0091`, report ID 1 and the 63-byte input/output payload shape;
2. query opcode `0D` and select a factory map only for an exact known firmware
   product; if the command is unavailable, an exact HID marketing name is a
   compatibility fallback, never a substring match;
3. query opcode `21`, subcommand `04`, and validate the travel descriptor;
4. read opcode `18`, selector `80`, and validate one complete 132-record
   override-map generation;
5. only after the independent responses, claim the exact interface path;
6. install the RAM-only live mask with opcode `21`, subcommand `02`.

No interface is claimed by marketing name, VID/PID or geometry alone. The
known firmware products select the exact official SI2825, SI2828 or SI2851
factory layout; unknown products must supply enough explicit map entries to
prove their own layout. Analogue state is published only from valid
subtype-`01` live events. A shared PID or a reported key count never selects a
layout by itself.

## Factory layer plus dynamic overrides (`18/80`)

Request payload, excluding the WebHID report ID:

```text
18 80 00 ...
```

The firmware returns ten fragments. Fragments `0..8` contain 56 data bytes
(14 four-byte records); fragment `9` contains 24 data bytes (six records):

```text
payload[0]     18
payload[1]     80
payload[2:4]   fragment index, big-endian
payload[4]     data length (38 hex or 18 hex)
payload[5...]  records
```

Record index is `fragment * 14 + item`, producing exactly 132 row-major
positions. Position index is `row * 22 + column`. Each record is:

```text
byte 0  function class
byte 1  candidate HID usage for keyboard bindings
byte 2  auxiliary/function byte
byte 3  auxiliary/function byte
```

An all-zero record means "inherit the factory assignment"; it does not mean
that the sensor position is absent. The first physical log proved this
directly: all ordinary factory keys were zero records, while position 122 held
`01 FA`, exactly matching the official Fn position. HallJoy therefore starts a
known WIN60 session from the official SI2825 61-key map and overlays only
explicit non-zero records. This preserves factory keys and remains remap-aware.

The generation is complete only when every fragment is present once and all
lengths match. Composite, macro, advanced and internal function classes must
not be truncated into keyboard HID usages. The physical WIN60 layout remains
the authoritative identity of a key; an unknown product does not receive that
layout merely because it shares the 132-position transport.

## Live subscription (`21/02` and `21/03`)

The official driver builds 22 column bytes. Bit `row` in byte `column` selects
one logical matrix position:

```text
payload[0]     21
payload[4]     18 hex (official declared request length)
payload[5]     02
payload[6:28]  22-byte column mask, six low bits used per byte
```

The firmware copies only those 22 bytes into RAM at `0x2000E878`. There is no
flash write. Subcommand `03` clears the same RAM mask and unsubscribes. Both
return a short `21` acknowledgement.

Subscribing all known physical positions at once is supported. Subscribing all
132 matrix bits is also representable, but a product-specific physical mask is
preferred so unused/noisy sensor positions cannot consume bandwidth.

The end-to-end firmware data flow was re-audited after the first physical log.
The `21/02` handler at `0x080118C2` copies the 22 request bytes into RAM
`0x2000E878` with no additional enable command or digital-actuation gate. The
normal scanner's mask literal at `0x08018558` and the alternate scanner's mask
literal at `0x08018E94` both resolve to that exact RAM address. Each producer
tests `mask[column] & (1 << row)` immediately before queuing a live report.
Thus `21/02` is not merely inferred from the web client: its complete command,
shared state and both consumers are linked in the supplied firmware image.

## Live travel events

Both scanner paths in this firmware emit subtype `01` and the same five primary
fields. The normal path declares length `03`; the alternate/batch path declares
length `05`:

```text
payload[0]   21
payload[4]   03 or 05 (producer-dependent declared length)
payload[5]   01
payload[6]   row (0..5)
payload[7]   column (0..21)
payload[8]   processed travel, little-endian low byte
payload[9]   processed travel, little-endian high byte
```

The normal packet constructor is at `0x080180E2`; the alternate constructor is
at `0x08018BB4`. Their field order matches HallJoy's Windows-wire parser
exactly. Emission is controlled by processed-travel change plus the subscription
mask and is not conditioned on the normal keyboard make/break actuation point.

The normal subtype `01` producer writes additional diagnostic/calibration
fields beyond its declared primary payload. They include baseline, raw sample,
direction/filter state and thresholds. Those trailing fields are not needed
for analogue publication and their presence or value must not be used as a hard
blocker. Subtype `05` is the requested per-key trigger-configuration response;
it is not a live travel event. Confusing the alternate producer's declared
length `05` with its subtype was the earlier reconstruction error.

Other opcode `21` subtypes (`07`, `08`, and calibration/status variants) can be
emitted asynchronously. A robust receiver recognizes and ignores them without
poisoning the session. Unknown and malformed reports remain visible in the raw
diagnostic trace but are ignored by the analogue publisher. A transport failure
ends the current session and causes the normal reconnect path to repeat the
complete read-only proof instead of permanently blocking the device.

## Units and normalization

Opcode `21`, subcommand `04`, returns the device travel descriptor. In the
supplied firmware its fixed data bytes are:

```text
54 01 01 01 08
```

The official decoder obtains maximum travel as `(data[3] << 8) | data[0]`,
which is 340, and obtains the unit numerator from `data[1]`. The divisor
(`100` or `1000`) comes from a separate capability flag, not from the final
`08` byte shown above. Live values are processed
travel counts in `0..maximum`; HallJoy should normalize with rounding:

```text
normalized = clamp(round(raw * 1000 / maximum), 0, 1000)
```

The maximum is a session capability and must be queried, not hard-coded.
Normalizing counts does not require guessing the millimetre divisor. Zero must
be published on release, disconnect, parser restart and session teardown.

Subcommand `0A` is a read-only query for the configured polling-rate code. The
response uses subtype `09`; official controls label codes `1`, `2`, `4`, `8`
as 1, 2, 4 and 8 kHz respectively. This is the configured device rate, not a
substitute for measuring delivered live-event timing, so diagnostics record
both values separately. The physical stock keyboard returned code `0`, which
is a valid untouched firmware-default value but does not encode a documented
nominal rate; HallJoy reports it as `unspecified` rather than inventing 1 kHz.

## Snapshot and mutation hazards

Subcommand `0E` exposes a cumulative 6x22 sensor-domain diagnostic matrix. This
firmware enqueues a response inside the inner column loop: 132 cumulative
48-byte responses with no explicit row/column tag. The physical trace returned
idle values around `0x0Axx`, outside the proved processed-travel range
`0..340`, so these values must never be normalized or published as key travel.

The first corrected physical build demonstrated a second, independent hazard:
the device emitted the 132-packet burst faster than the Windows queue retained
it, yielding exactly 64 reports, and a synchronous collector intercepted 532
valid live events while waiting for snapshots. That collector created 14 false
active keys and caused real release reports to be discarded. Consequently
`21/0E` is not part of admission, streaming, recovery or state resynchronization.
The single live receive dispatcher is the only owner of reports after
subscription; an idle event-driven stream is normal and triggers no command.

Subcommands `08`, `0F` and `10` alter calibration/runtime state and can reach
persistent configuration helpers. Subcommands `00`, `01`, `0C` and `0D` write
per-key advanced behavior. Subcommand `06` resets runtime state. HallJoy must
not issue any of them during discovery, proof or analogue streaming.

## Required implementation behavior

- Keep Standard and MAX transports as separate protocol strategies behind the
  same higher-level analogue backend contract.
- Route by the complete interface fingerprint and live proof, never PID alone.
- Use one long-lived read/write session and one receive dispatcher.
- Never perform a synchronous request/response probe while the live
  subscription is active; it would compete with the sole receive dispatcher.
- Collect fragmented responses by generation; allow live/status interleaving.
- Parse subtype `01` with either proved declared length and treat subtype `05`
  only as per-key configuration.
- Do not fail the entire device on one unknown, late or malformed report.
- Do not publish until the HID shape, range and complete map are proven.
- Never send mutating calibration/configuration subcommands.
- Never publish `21/0E` sensor-domain values as processed travel.
- On any reconnect, clear published state and repeat the complete proof.

## Evidence provenance

- supplied updater archive SHA-256:
  `B3A8A1CE180884491A2EC473A2BDFA79FC3B8F083066DF0B0DD1AB9F22605309`;
- signed updater EXE SHA-256:
  `10A597513911D614E95208F98BACDECBD34E08E028713A923A2C00DC4B02D715`;
- extracted firmware SHA-256:
  `430AB999079D861DE8076DB097484FFEFB45A4F970D2EDE2757782BCB698C33C`;
- official `wmIndex.min.js` fetched 2026-08-08 SHA-256:
  `4CF7EA0B82BDD76B906D2E8BCE93C0E3196F50E6D3E647E4D8E9F3DF7649986A`;
- official `agreement.min.js` fetched 2026-08-08 SHA-256:
  `B3CF089010778B4963782217D8974FD04174333B201ABDF17DFEF9C457B06334`;
- official device catalog fetched 2026-08-08 SHA-256:
  `37964C34D60DACA226594941802420C0B797728947D70068CC7D145C22470D76`;
- official `SI2825KZHEARGB` 61-key layout fetched 2026-08-08 SHA-256:
  `04E2FDA00FDB1645C74D42121102C1CF233658DDF43FB2611ACE57460CFCB448`.
- official `SI2828HEARGB` / `SI2828KZHEARGB` 68-key layout fetched
  2026-08-09 SHA-256:
  `CC2CBBC9C051230279BA8CE3B52054739446DC19C78CF1A38ADFD2D7C6CDF9E1`;
- official `SI2851UKKZHEARGB` 69-key layout fetched 2026-08-09 SHA-256:
  `FCB98F5DF82C2E00D94501389C452172C4FAB6F103DF3CBF489FFFE9B89945C3`.

The firmware is the authority for dispatcher behavior and safety properties.
The official driver is used as an independent cross-check for frame indexing,
the 22-byte subscription mask, the ten-fragment base map, travel decoding and
the product-family catalog.
