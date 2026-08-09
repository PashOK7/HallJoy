# MADLIONS MAD 68 Pro R: final Hall/analogue protocol audit

- **Image:** `35ed6b7a73cfe7e7fd222e6862032e49.bin`
- **SHA-256:** `2a7df4ffc491476b79da5333f51179f68fbbb5c5e146e987cfb5df764b79cead`
- **Size:** 124656 bytes (`0x1E6F0`)
- **Load base:** `0x5000`
- **Status:** supersedes Stage 1, Stage 2, and the earlier Deep Audit

## 1. Final conclusion

For this exact firmware image, live analogue values are obtained as follows:

1. open vendor HID interface 1 on VID `373B`, PID `1109`;
2. read 64-byte interrupt-IN reports from endpoint `0x82`;
3. accept asynchronous reports whose first byte is `0xA0`;
4. identify the key by the three-byte descriptor in bytes `1..3`;
5. decode and normalize the main analogue value:

```text
raw = (packet[4] << 8) | packet[5]       // big-endian u16
raw = clamp(raw, 0, 1600)
normalized = raw / 1600.0
```

Important correction: bytes `11..13` are not the main key travel. They encode
the adaptive per-key calibration scale used as a divisor by the scan code. The
live coordinate for HallJoy is bytes `4..5`.

No second host-visible live Hall path exists in this image: there is no opcode
for addressed Hall-channel or live Hall-RAM reads, vendor EP0 transfer, HID
Feature report, analogue axis in the standard HID interfaces, alternate command
table behind framing byte `0x5F`, or second USB-IN Hall producer. Direct USB
register access and every interrupt root were also classified. HallJoy must
therefore use the asynchronous A0 stream for this firmware SHA.

## 2. Corrections to earlier analysis

### 2.1 Correct analogue field

The earlier Deep Audit treated this expression as the live metric:

```text
packet[11] + packet[12]/10 + packet[13]/100
```

Deeper dataflow analysis disproved that interpretation. The structure at
`0x200040EC + 8*slot` contains a float loaded before `fdiv.s` and used to scale
Hall delta. The state machine later updates that same float from full range
divided by `1600.0`; its default is approximately `0.39`.

Consequently, bytes `11..13` are a decimal representation of calibration scale,
while bytes `4..5` contain current Hall delta after that scale is applied. The
firmware caps bytes `4..5` at `1600`, making them the appropriate live
coordinate.

### 2.2 Complete USB audit

The earlier document relied mainly on the seven direct callers of sender
`0x545A`. The final audit additionally classified the USB ISR at `0x558E`, EP0
dispatch, standard request table `0xF1D4`, descriptor dispatch `0xF204`, HID
class table `0xF290`, every load from `0x40023xxx`, endpoint/DMA setup,
watchdog/recovery, vector table, indirect transitions, literal function
pointers, and all EP81/EP82/EP83 producers. No unexplained USB-register root
remains.

### 2.3 Serialized activation

Firmware owns one receive buffer and one pending flag, not a command queue.
Sending A8 and A9 without awaiting each response could overwrite or lose a
command. Activation must therefore be serialized and verified.

## 3. Disassembly completeness

The image was disassembled at base `0x5000` with the WCH/QingKe `+xwchc`
extension, resolving the nonstandard compressed byte/halfword load/store
instructions. There are no unknown instructions in the critical A8/A9 state
logic, USB setup/sender/ISR/EP0, vendor parser and handlers, USB scheduler,
configuration initialization, A0 builder/service, or main Hall scan/state
machine. The bytes `4..5` conclusion follows a decoded instruction chain rather
than a packet-layout guess.

## 4. USB topology

```text
VID         0x373B
PID         0x1109
bcdDevice   0x0102
Product     MAD 68 Pro R
```

| Interface | Function | Endpoints |
|---:|---|---|
| 0 | Boot keyboard | `0x81 IN`, 8 bytes |
| 1 | Vendor HID | `0x82 IN`, `0x04 OUT`, 64 bytes |
| 2 | Composite keyboard/mouse/consumer/system | `0x83 IN` |

The vendor report descriptor declares 64-byte Input and Output reports and no
Feature report. EP0 implements standard USB plus limited HID class requests:
HID `GET_REPORT` stalls, `SET_REPORT` exposes no separate payload handler,
vendor request type `0x40` has no dispatcher, and control transfers cannot read
arbitrary memory or Hall state.

## 5. Vendor framing and command dispatch

Normal 64-byte OUT request:

| Byte | Meaning |
|---:|---|
| 0 | `0x55` request header |
| 1 | opcode |
| 2 | XOR key; zero disables XOR |
| 3 | checksum |
| 4 | payload length, at most `0x38` |
| 5..6 | little-endian offset/index |
| 7 | parameter/flags |
| 8.. | payload |

Checksum is `sum(packet[4 .. 7+length]) & 0xFF`. Response header `0xAA` means
normal response and `0xAB` means checksum error. Unknown commands reach the
default `unknw` handler. Header `0x5F` enters the same jump table at `0xF2C0`.

The complete `00..F2` table contains 243 entries: 30 active handlers and 213
default entries. A8 maps to `0x63F8`; A9 maps to `0x63E0`. Other read commands
access fixed configuration/NVM windows or unrelated runtime buffers, never the
live Hall arrays at `0x20003CFC..0x200047F4`.

Do not confuse host opcode A0, which reads configuration/NVM at `0x20400`, with
the `0xA0` header of an asynchronous IN report. DD/DE use unrelated RAM windows,
0B is a data-flash writer, and EE belongs to factory/maintenance behavior and
must not be probed. The complete table is in `command_map_final.csv`.

## 6. Proven Hall dataflow

```text
physical Hall scan / filtered sample
        -> raw/current samples       0x20003CFC[72]
        -> baseline/reference        0x20003EAC[72]
        -> filter accumulator        0x20003F3C[72]
        -> per-key scale record      0x200040EC + 8*slot
        -> abs(filtered - baseline) / per_key_scale
        -> cap and rounding
        -> live coordinate           0x2000444C[72], u16, 0..1600
        -> A0 builder                0x9F30
        -> asynchronous buffer       gp+0x358
        -> USB scheduler             0x7AA4/0x7AB0
        -> interface 1 endpoint      0x82
```

Both polarity branches calculate absolute filtered deviation from baseline and
divide it by the per-key scale. The result is converted to an unsigned integer,
may round upward at the internal fractional threshold `0.6`, is stored at
`0x2000444C[slot]`, and saturates at `0x0640` (`1600`). This proves the internal
domain, but not any physical unit such as hundredths of a millimeter. HallJoy's
`raw/1600` normalization is valid relative to the firmware's own range.

Both polarities become a positive deviation: rest is normally near zero and
maximum deviation approaches 1600. The exact full-stroke point depends on
per-key calibration.

## 7. Asynchronous A0 report

Builder `0x9F30` initializes bytes `0..19` only; ignore stale bytes `20..63`.

| Bytes | Format | Meaning |
|---:|---|---|
| 0 | `u8` | `0xA0` header |
| 1..3 | 3 bytes | fixed descriptor from table `0xFB74` |
| 4..5 | `u16 BE` | primary live analogue value, `0..1600` |
| 6 | `u8` | high byte of secondary quantized/gate value |
| 7 | `u8` | conditional threshold/helper |
| 8 | `u8` | auxiliary low byte |
| 9 | `u8` | auxiliary state |
| 10 | `u8` | per-key state field |
| 11..13 | decimal bytes | calibration scale, not primary analogue value |
| 14..15 | `u16 BE` | selected actuation threshold |
| 16..17 | `u16 BE` | auxiliary live word |
| 18..19 | `u16 BE` | baseline/reference Hall sample |
| 20..63 | undefined | stale tail; ignore |

```cpp
if (report.size() == 64 && report[0] == 0xA0) {
    const uint16_t raw =
        (static_cast<uint16_t>(report[4]) << 8) |
         static_cast<uint16_t>(report[5]);
    const float analog =
        std::min<uint16_t>(raw, 1600u) / 1600.0f;
}
```

Bytes `11..13` may be logged diagnostically but must never drive an axis.

## 8. Key addressing and stream behavior

The descriptor table at VA `0xFB74` contains 72 three-byte entries. Scanner
slots 8, 16, 35, and 59 are empty, leaving 68 unique descriptors. Identify a
key by all three bytes, not HID usage alone, because modifiers and Fn use
separate encodings.

Initial forced sweeps visit all 72 slots; the builder skips descriptors whose
first byte is zero, yielding 68 reports per complete sweep. Afterward the stream
is change-driven. Service `0xA0A8` compares current trigger state at
`0x20004644[slot]` with its mirror at `0x2000456C[slot]`, normalizes values at or
below 4 to zero, updates the mirror on change, and builds one report using the
current high-resolution value at `0x2000444C`.

The protocol is therefore an event stream, not a host-polled full matrix. After
the initial snapshot, HallJoy must retain the latest value per key. Firmware
quantization/gating controls event cadence; a single USB report never contains
all 68 values.

## 9. A8/A9 activation and recovery

When suppression is not already active, A8 sets telemetry latch bit 3 at
`0x2000580B`, sets `gp-0x760 = 1`, sets the forced-sweep count at `gp-0x761` to
3, fills `0x20004BE4[72]` with `0xFF`, and invokes baseline/filter initializer
`0xA15A`. A9 clears suppression and `0x20004BE4[72]`, but does not clear the
telemetry latch or directly clear the forced-sweep counter.

Because A8 initializes baseline from current Hall samples, activating while a
key is held can make that position the new baseline. The backend must therefore
wait automatically for an all-keys-up standard HID state.

Recommended state machine:

```text
1. Listen passively for valid A0 reports.
2. Confirm that all keys are released.
3. Send A9 and await the matching valid AA/A9 response.
4. Send A8 and await the matching valid AA/A8 response.
5. Send A9 and await the matching valid AA/A9 response.
6. Validate A0 header, descriptor, raw range, and convergence toward 68 keys.
```

Zero-payload requests begin with:

```text
A8: 55 A8 00 00 00 00 00 00
A9: 55 A9 00 00 00 00 00 00
```

All remaining bytes must be zero; the length-zero checksum is zero. Control
responses and asynchronous reports use separate firmware buffers, with control
responses prioritized by the USB scheduler. The host must still demultiplex
`AA/AB` command traffic from `A0` Hall events.

Static analysis cannot promise that no single ordinary keyboard event is lost
between A8 execution and confirmed A9, because A8 explicitly enables
suppression. Keep this interval minimal, record it diagnostically, and always
retain best-effort A9 recovery.

## 10. Why no alternative live path remains

- all 243 command-table entries were classified; no read handler addresses live
  Hall arrays;
- EP0 exposes no vendor Hall API, Feature report, or useful GET_REPORT path;
- sender `0x545A` has exactly seven classified callers: boot keyboard, mouse,
  NKRO, consumer, EP82 asynchronous, system, and EP82 control;
- EP82 has only the control `AA/AB` path and asynchronous A0 path built by
  `0x9F30`;
- all 24 instructions loading `0x40023xxx` belong to classified endpoint/DMA
  setup, attach/init, sender, ISR/EP0/EP4 OUT, reset/pull-up, or watchdog logic;
- real vector roots at `0x558E`, `0x7AF4`, `0xB01A`, `0xDEF8`, and `0x66EA` are
  classified; non-USB handlers do not access USB peripheral state;
- no literal pointer or unexplained indirect dispatch reaches another USB
  sender, scheduler, or Hall builder.

For this exact image, a hidden USB Hall transport would contradict the complete
USB register-access map.

## 11. Confidence boundaries

Proven for the stated SHA: load base, USB topology, absence of Feature reports,
complete dispatcher, absence of addressed live-Hall reads, A8/A9 state split,
A0 builder/service and EP82 route, 72/68 descriptor map, primary value in bytes
`4..5`, range `0..1600`, calibration scale in bytes `11..13`, three initial
sweeps followed by change-driven reporting, undefined tail bytes, and uniqueness
of the host-visible Hall path.

High-confidence items still requiring on-device integration were USB cadence,
the number of ordinary keyboard events lost in the suppression window,
sleep/resume and unplug/replug behavior, competition with the official driver,
and compatibility with other firmware/PID/bcdDevice revisions.

Do not claim that `raw/1600` represents physical millimeters, activation is
perfectly seamless, every MAD 68 Pro R revision is identical, or bytes `20..63`
contain stable data.

## 12. Backend requirements

A production backend must fingerprint device/interface topology, open vendor
IF1 independently of the keyboard collection, listen passively first, wait for
all keys released, execute correlated `A9 -> A8 -> A9`, demultiplex command and
A0 traffic, normalize `BE16(packet[4..5]) / 1600`, store state by the full
descriptor, validate initial 68-key coverage, ignore the stale tail, recover
with A9 on timeout/error, and never probe unknown opcodes.

This is a defined protocol implementation, not an exploratory probe. Hardware
integration validates the implementation and device revision; it no longer has
to guess which field contains analogue travel.

## 13. Compact specification

```text
Device:          VID 373B, PID 1109, bcdDevice 0102
Interface:       1
OUT endpoint:    04, 64 bytes
IN endpoint:     82, 64 bytes
Arm opcode:      A8
Recovery opcode: A9
Async header:    A0
Key ID:          packet[1..3]
Analogue raw:    BE16 packet[4..5]
Internal range:  0..1600
Normalized:      clamp(raw,0,1600)/1600.0
Initial state:   3 sweeps x 72 slots, 68 reports per sweep
Steady state:    one changed key per eligible service event
Ignore:          packet[20..63]
Do not use:      packet[11..13] as analogue travel
```

## 14. Reproducibility

`mad68pr_final_static_audit.py` operates on the firmware image without a
keyboard. It verifies the SHA, jump and EP0 tables, interrupt vectors, sender
callers, USB peripheral roots, critical disassembly completeness, A8/A9 state
operations, builder mapping, scan division/cap/store, calibration updates,
scheduler priority, descriptor map, and thresholds. A successful run ends with
`status: ok`; exact listings and tables are included in the evidence package.
