# MAD68 Pro R Hall protocol — implementation specification

## Device path

```text
VID/PID       373B:1109
bcdDevice     0102
HID interface 1
OUT endpoint  04
IN endpoint   82
Report size   64 bytes
```

## Control framing

Zero-payload requests:

```text
A8 arm:       55 A8 00 00 00 00 00 00 + zero padding
A9 recovery:  55 A9 00 00 00 00 00 00 + zero padding
```

Wait for a valid `AA` response carrying the same opcode after each OUT report.
Never burst commands: firmware has one RX buffer and no queue.

Recommended state machine:

```text
passive listen
wait until standard keyboard reports all keys released
A9 -> wait AA/A9
A8 -> wait AA/A8
A9 -> wait AA/A9
validate A0 snapshot and enter steady state
```

`A8` reinitializes baseline/filter state from the current Hall samples and temporarily suppresses normal keyboard handling. `A9` clears suppression but leaves the telemetry latch and forced sweeps active.

## Async report

```text
byte 0       A0
bytes 1..3   key descriptor
bytes 4..5   primary live analog, big-endian u16, 0..1600
bytes 14..15 selected threshold, big-endian u16
bytes 18..19 baseline/reference, big-endian u16
bytes 20..63 undefined/stale
```

Decoder:

```text
raw = (b4 << 8) | b5
axis = clamp(raw, 0, 1600) / 1600.0
```

Do not use bytes `11..13` as the axis. They are a decimal serialization of the adaptive per-key scale coefficient used by the scan routine as a divisor.

## Stream behavior

After `A8`, firmware schedules three full 72-slot sweeps. Four descriptor slots are empty (`8,16,35,59`), producing 68 physical-key reports per sweep. Steady state is change-driven and sends one key per eligible service event. Maintain the last value per full three-byte descriptor.
