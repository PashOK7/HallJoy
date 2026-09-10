# IO Type 84 Magnetic v1.17: static analogue analysis

Date: 2026-08-28  
Status: firmware-proven raw analogue stream behind a reversible test mode;
physical USB topology, live packet capture and coexistence with ordinary
keyboard reports remain untested.

## Scope and image identity

The two Intel HEX images in this directory decode to a Cortex-M Thumb image at
`0x00000000..0x00016263`, plus four bytes at `0x0007DFFC`. The reset vector is
`0x00000205`; initial SP is `0x20007E48`.

The executable protocol regions below are byte-for-byte identical between the
black and white files. The only mapped difference is the five-byte product
label at `0x00010BA8` (`Black` versus `White`). Thus the findings apply to both
colour variants.

## Host command dispatcher

The vendor-report handler begins at `0x0000D7C4`. It accepts a host frame whose
first two bytes are `AA <command>` and prepares device reports beginning with
`55`. The command `0x6*` branch at `0x0000DAF2` explicitly accepts exactly:

| Host command | Effect in the image | RAM state |
| --- | --- | --- |
| `0x64` | start calibration | set `0x2000036C` |
| `0x65` | stop calibration and reset calibration working data | clear `0x2000036C` |
| `0x66` | start simulation test | set `0x2000036D` |
| `0x67` | stop simulation test | clear `0x2000036D` |

These four paths perform direct byte writes to volatile RAM. Their immediate
handlers do not invoke the image's flash/update commands. This is evidence of
a reversible runtime transition, not evidence that changing the mode is free
of input side effects.

The generic web-driver command catalogue also names `0x60` (axis-key status)
and `0x68` (axis status). Neither command has a case in this Type 84 v1.17
dispatcher: the `0x6*` branch falls through after the four values above.
Therefore they must not be used as speculative read-only probes for this
firmware.

## Proven raw analogue report

Function `0x00000BF0` serializes a 14-byte asynchronous report:

```text
55 FB key calibration-status
   max-value-le16 min-value-le16 current-adc-le16
   key-stroke-le16 max-stroke-le16
```

The `min-value` high bit supplies `calibration-status`; the host-side decoder
masks that bit. This report includes two independently useful live values:
the raw ADC reading (`current-adc`) and the calculated key travel
(`key-stroke`).

All five static callers of this serializer are in the magnetic scan routine
at `0x00003AA8..0x00004063`. Each send path is gated by the simulation-test
byte `0x2000036D`:

- `0x00003C20` checks it before the first send path;
- `0x00003F4A` checks it before the other four send paths.

Consequently the firmware does **not** passively publish this `55 FB` analogue
stream. It does publish the stream after `AA 66 ...`, and `AA 67 ...` turns
that condition off. Calibration (`0x64/0x65`) is a separate state flag, not
the switch proved to gate the report serializer.

The official web-driver source independently parses `55 FB` as
`keyValue`, `calibrationStatus`, `maxValue`, `minValue`, `currentValue`,
`keyStroke`, and `maxStroke`, matching the serializer byte-for-byte. Its
generic `GET_MAGNETIC_AXIS_STATUS` (`0x68`) helper is not proof of support by
this particular firmware, because the image dispatcher rejects that command.

## HallJoy implication

There is a real, per-key analogue route in Type 84 Magnetic v1.17; it is not a
digital-key approximation and does not require reflashing. The only
firmware-proven way to expose it is the reversible simulation-test pair
`0x66`/`0x67`, with `55 FB` decoded as above.

Before production support, a diagnostic must record the actual HID
interfaces/reports, send `0x66`, capture and correlate changing `55 FB` values
with physical travel, send `0x67` on every exit path, and record whether normal
keyboard input remains available. Static analysis alone cannot prove the last
property, so it must not be assumed either way.
