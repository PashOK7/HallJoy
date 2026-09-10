# MCHOSE Ace 68: deep firmware reverse-engineering log

Assessed 2026-08-28 from the verified `Ace 68 I` image only. This log records
facts obtained statically. It does not treat an untested command as safe to
send, and it keeps calibration distinct from a usable runtime analogue mode.

## Confirmed USB topology

The image contains one USB configuration with three HID interfaces:

| Interface | Endpoint(s) | Max packet | HID report descriptor | Role established by descriptor |
|---|---|---:|---:|---|
| 0 | `81 IN` | 8 | 62 bytes | Boot-style keyboard plus LED output. |
| 1 | `82 IN`, `04 OUT` | 64 | 32 bytes | Bidirectional M HUB configuration transport. |
| 2 | `83 IN` | 64 | 163 bytes | NKRO keyboard, mouse and consumer reports. |

Evidence: the configuration descriptor is at image offset `0xE2AE`. The
corresponding report descriptors are at `0xB394` (62 bytes), `0xB370` (32
bytes), and `0xB2D0` (163 bytes), respectively.

The M HUB transport descriptor is **not** a generic analogue HID report. It is
`Generic Desktop / undefined` (`05 01 09 00`), with 64 one-byte input fields
and 64 one-byte output fields. It has no report ID and no axis, physical
maximum, unit, or per-key analogue field. The old `FF00:0001` label came from
web-driver filtering metadata and is not confirmed by this firmware's actual
HID descriptor; it must not be used as protocol evidence.

## Separation from normal typing

The ordinary keyboard paths are physically separate from the M HUB IN/OUT
endpoint: interface 0 exposes the 8-byte boot path and interface 2 exposes
the 64-byte NKRO path. The USB interrupt handler configures separate endpoint
buffers and accepts up to 64 bytes from endpoint `04 OUT` into a working
buffer. Therefore a normal M HUB configuration exchange does not, by itself,
disable letter input.

This does **not** prove that every command preserves typing. A calibration or
test handler may still alter scan state, thresholds, or report scheduling.

## Recovered MCHOSE command transport

Endpoint `04 OUT` is copied to `gp+0x178`; its receive flag is `gp-0x7C2`.
The dispatcher at `0x080012D8` accepts the following MCHOSE packet, not merely
the newer public M HUB V3 framing:

```text
0       55          framed request marker (or 5F for the raw path)
1       opcode      dispatch index, valid through F2
2       xor_key     zero means no obfuscation
3       checksum    sum of bytes 4..(7 + payload_length), modulo 256
4       length      payload length, at most 38
5..7    parameters / header fields
8..     payload
```

For a nonzero XOR key, bytes `3..(6 + length)` are transformed in place before
validation and again before the response. A valid exchange is marked `AA` in
byte 0; a failed checksum is marked `AB`. The response is scheduled to
endpoint index 2, which is interface 1's `EP82 IN`.

The dispatcher makes the missing mapping explicit: it accepts an opcode through
`F2`, loads `uint32_t[0x0000FDD0 + 4 * opcode]`, then jumps to that address.
Every byte of the supplied image's `0x0000FDD0..0x0001019F` range is zero
(976 bytes / 244 slots; the first 243 cover the accepted `00..F2` opcodes).
This is not an undisassembled code path: the required dispatch table is absent
from this update payload. The per-key descriptor table at `0x000123BC` and the
mode table at `0x0001042C` are likewise zeroed. They are therefore unresolved
runtime, preserved-memory, or omitted-image regions.

MCHOSE must not be sent MAD68 `A8`/`A9` commands in normal HallJoy just because
the designs are related. A separately compiled opt-in evidence build may test
the narrowly recovered MCHOSE state-transition hypothesis; its constraints are
recorded in `DIAGNOSTIC_BUILD.md`.

## Proven asynchronous `A0` path

The firmware contains a live vendor report builder at `0x080047F6`. It writes
64 bytes at `gp+0x378`, puts `A0` in byte 0, and sets pending flag
`gp-0x79C`.

| Bytes | Static source | Meaning established |
|---|---|---|
| `0` | literal | `A0` event marker. |
| `1..3` | `0x000123BC + 3 * slot` | Per-key three-byte descriptor; its table contents are absent from this image. |
| `4..5` | `0x20002038 + 4 * slot` | Big-endian live Hall coordinate. |
| `6..7` | `0x0001042C + 8 * mode` | Mode-dependent configuration value, not the primary coordinate. |
| `8..23` | per-slot scan/configuration state | Additional fields, still to be named. |
| `24..63` | not cleared by this builder | Treat as unspecified/stale until captured. |

The scanner rooted at `0x08005004` calculates the value at
`0x20002038 + 4 * slot` and clamps it to `0x640` (1600). Thus `A0[4..5]` is a
0..1600 Hall-derived coordinate, not a digital key state or a host calibration
result. The service at `0x08004A9A`, called just after scanning from
`0x080039EA`, emits changed slots and can sweep slots `0..0x47` (72 slots).
The USB scheduler at `0x0800299A` submits exactly 64 bytes from `gp+0x378` on
endpoint index 2 and clears the pending flag only after submission. `A0` is
therefore an asynchronous `EP82` event stream, not a reply buffer.

The same scheduler checks normal keyboard-report flags for other endpoint
indices before it checks the configuration reply and then the `A0` flag. The
event is therefore queued as background vendor traffic, rather than replacing
the ordinary keyboard endpoint path.

## Calibration versus normal operation

At ordinary boot (`0x08000312`) the firmware sets bit 3 of `0x200044CF`, the
`A0` service gate, and starts its initial sweep. This happens before the
normal scan/report loop. No static edge from this boot enable to a calibration
handler or to suppression of interfaces 0/2 was found. Together with separate
keyboard and vendor endpoints, it is strong static evidence that `A0` was
intended to coexist with normal typing rather than only a letter-blocking
calibration session.

An attached keyboard still has to verify that fresh reports actually appear
after boot and that a simultaneous key press reaches the normal interface.
But the old reason to reject this route as calibration-only is no longer
valid.

## Active fallback if passive reports are absent

The image also contains a matched state-reset/re-arm path reachable from
command-handler code, although its opcodes cannot be recovered from the
zeroed jump table:

- routine `0x08001146` clears `gp-0x753` and `gp-0x754`, runs the same
  scanner-state initializer, and clears a 72-byte per-slot state area;
- handler body `0x08001466` calls boot initializer `0x0800030E` only when
  `gp-0x753` is clear. That initializer sets the telemetry latch bit, restores
  `gp-0x753 = 1`, sets a forced-sweep count, rebuilds Hall state and fills the
  same 72-byte area with `FF`.

The official M HUB Web Driver now resolves the opcode question for this exact
identity (`VID 41E4`, `PID 2114`, `Ace 68 I`): its Ace 68 channel calls
`startCalibration(A8, offset 0, [00])`, `endCalibration(A9, offset 0, [00])`,
and `reStartCalibration(B1, offset 0, [00])`. Its packet builder emits the
canonical un-obfuscated frames `55 A8/A9 00 01 01 00 00 00 00` (then zero-pad
to the 64-byte HID report). This establishes that `A8`/`A9` are the official
calibration controls, rather than an inferred MAD68 opcode similarity.

This does not make calibration a usable analogue runtime mode. It may suppress
ordinary typing or alter volatile calibration state, so the diagnostic build
does not enter it automatically. Its active evidence stage is limited instead
to the official non-mutating getters `03`, `04`, `05`, `08`, and `A0`; any
separate `A8`/`A9` experiment requires explicit tester consent and must record
normal keyboard input before, during, and after the calibration interval.

## Firmware map established so far

- reset jumps to `0x08003B98`; the image is flat RISC-V code with compressed
  instructions, not an ELF with symbols;
- USB endpoint setup begins near `0x0800037E`;
- the USB interrupt service routine begins near `0x08000596` and contains the
  endpoint-04 receive path near `0x0800077E`;
- the HID and USB descriptors are static data at the offsets listed above;
- the main scan/report loop and nonvolatile settings code are separate from
  the USB interrupt routine.

## Static boundary and remaining high-value work

Further static work remains useful, but this particular update file cannot
recover the byte-to-handler mapping: it contains neither the opcode jump table
nor the descriptor/mode tables used by the live `A0` builder. Code near
`0x0800101A..0x08001214` also appears able to copy bounded data from fixed flash
addresses into responses, but without the omitted dispatch entries there is no
safe way to associate an opcode with those handlers.

The highest-value evidence is therefore either a full flash dump that includes
the `0x0000FDD0` table, or an authorised capture of M HUB host-to-device traffic.
The first would turn every opcode into an exact address; the second can identify
the specific command that enables or configures the already-proven `A0` path.
The diagnostic build is deliberately collecting the complementary dynamic
evidence: all vendor reports, exact replies/errors, and same-device raw-keyboard
events, without publishing input or issuing arbitrary opcodes.

## Required next evidence

The concrete HallJoy candidate is now a diagnostic `EP82` reader: accept a
64-byte report beginning `A0`, take `BE16(report[4:6])` as the provisional
0..1600 coordinate, and map `report[1:4]` only after physical correlation or
recovery of the missing table. The dedicated build captures all reports and
runs a bounded `A9`/`A8` state experiment when passive output is absent; it
never publishes values to gameplay.

Hardware validation must record idle, press, release, multiple simultaneous
keys, reconnect and normal typing. It must also establish the three-byte
slot-to-key map and reject incompatible PID/firmware revisions. Separately,
the unresolved opcode table should still be recovered before any optional
subscription, calibration, or configuration command is classified as safe.
