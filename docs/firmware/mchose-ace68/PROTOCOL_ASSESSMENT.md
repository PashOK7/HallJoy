# MCHOSE Ace 68 analogue-protocol assessment

Assessed: 2026-08-28. Scope is read-only static analysis of the verified
`Ace 68 I` firmware and the live official M HUB web-driver assets. No keyboard
was connected and no firmware was modified or flashed.

## Conclusion

Stock Ace 68 firmware contains a real asynchronous analogue candidate, not
just internal Hall processing: 64-byte `A0` reports are built from a live
per-slot Hall coordinate and scheduled to M HUB `EP82 IN`. The coordinate is
big-endian bytes `4..5` and statically ranges up to `1600`.

HallJoy does **not** support it as gameplay input yet. Hardware validation and
the report's three-byte key identity map are still missing. The tree now has a
separate opt-in diagnostic image that never owns keys or publishes controller
values, but records full vendor traffic and makes only the bounded `A9`/`A8`
state experiment documented in `DIAGNOSTIC_BUILD.md`.

## Evidence

- Static USB descriptors in the exact firmware show independent boot keyboard,
  NKRO keyboard and M HUB configuration interfaces. The configuration link is
  a 64-byte IN/OUT Generic-Desktop/undefined HID report, not a declared
  analogue HID report. Earlier `FF00:0001` wording was driver filter metadata,
  not a descriptor-level protocol fact.
- M HUB's public V3 implementation exposes version, UUID, keymap/remapping,
  lighting and performance/configuration operations, but it is not the whole
  firmware transport. The exact firmware accepts a separate `55`/`5F` framed
  command family with XOR and checksum. Its handler table is absent from the
  update image, so host command opcodes remain unresolved.
- Builder `0x080047F6` writes `A0`, a three-byte per-slot identifier, and a
  big-endian value from `0x20002038 + 4 * slot`. Scan code supplies that value
  and clamps it at `0x640` (1600).
- Service `0x08004A9A`, called from the ordinary periodic scan loop, emits
  changed values and can sweep all 72 slots. Scheduler `0x0800299A` sends the
  pending 64-byte buffer on endpoint index 2 (`EP82`).
- Boot code enables the service gate (`0x200044CF`, bit 3) and starts the
  initial sweep. No static connection to calibration or normal-keyboard
  suppression was found. This makes simultaneous normal typing plausible, but
  only a physical capture can prove runtime behaviour.
- HallJoy's native catalog has no MCHOSE backend. The bundled Universal Analog
  Plugin's supported-device list also does not include MCHOSE.
- HallJoy's SparkLink prefilter can look at a generic `FFxx:0001` vendor
  interface, but it sends and requires its own exact 64-byte `01 02`,
  `03 01`, and `04 03 01` request/response family. MCHOSE uses a different
  framed protocol; SparkLink must reject it when that proof fails and does not
  constitute support.

## Required next evidence

1. Passively capture `EP82` after normal boot; verify fresh `A0` reports
   without a host command.
2. Correlate `report[1..3]` with physical keys while retaining `BE16[4..5]`
   as the candidate 0..1600 travel value.
3. Test idle, press, release, multiple keys, reconnect and simultaneous normal
   typing on interfaces 0/2.
4. Gate any backend by `VID 41E4 / PID 2114` and a protocol fingerprint so
   incompatible MCHOSE revisions cannot be claimed.

Until then HallJoy must not send exploratory packets or infer analogue values
from ordinary digital reports.

See `PROTOCOL_COMPARISON.md` for the complete comparison against all current
HallJoy native and Universal Analog Plugin routes, and `DEEP_REVERSE.md` for
the descriptor-level and firmware-map evidence.
