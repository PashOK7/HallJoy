# Hex80 Analog Reference

This folder is a clean reference for working with the ATK x QK Hex80 analog HID interface.

It focuses only on the keyboard protocol itself:

- how to identify the correct HID interface
- which packets to send
- how to decode the returned analog travel values
- how matrix indices map to physical keys

This reference intentionally leaves out project-specific notes, machine-specific notes, and app-specific integration details.

## Contents

- `PROTOCOL.md`
  Explains the device identity, command structure, startup sequence, response format, and normalization rules.

- `MATRIX.md`
  Lists the current matrix-index-to-key mapping used to turn raw slot numbers into actual keys.

- `hex80_reader_example.py`
  A small generic Python example that opens the Hex80 analog interface and prints live travel values.

## Quick Summary

The currently useful normal-mode read path is:

1. Open the Hex80 vendor HID interface.
2. Send `03 96 19` once as a recovery packet.
3. Read `02 96 24` once to get `travel_max`.
4. Poll `02 96 1C` in chunks across the matrix.
5. For each returned entry, read the 16-bit big-endian travel value and normalize it to `0.0 .. 1.0`.

Important detail:

- `03 96 18` enters calibration mode.
- `03 96 19` exits calibration mode.
- For normal analog + typing at the same time, use `03 96 19` as a safety exit and do not send `03 96 18`.

## Known Interface Identity

- Vendor ID: `0x373B`
- Product IDs seen: `0x1176`, `0x1177`, `0x1250`
- Usage page: `0xFF60`
- Usage: `0x61`

## Core Constants

- `GET keyboard value = 0x02`
- `SET keyboard value = 0x03`
- `custom ID = 0x96`
- `travel info = 0x24`
- `travel/status buffer = 0x1C`
- `calibration start = 0x18`
- `calibration finish = 0x19`
- `slot count = 104`
- `chunk size = 4`
- `default travel_max = 3300`
- `raw deadzone = 8`

## Recommended Depth Formula

```text
depth = clamp((travel - 8) / (travel_max - 8), 0.0, 1.0)
```

Where:

- `travel` is the 16-bit big-endian travel value from each slot entry
- `travel_max` is read from subcommand `0x24`

## Practical Notes

- A leading report ID byte of `0x00` is used before the payload when writing through hidapi-style interfaces.
- A 128-byte payload buffer is a safe choice for writes.
- A 128-byte read buffer is a safe choice for reads.
- The polling path works in normal mode without forcing the keyboard into calibration mode.
- The matrix reports 104 slots, but the visible keyboard grid is effectively `6 rows x 17 columns`, plus two trailing unused slots.
