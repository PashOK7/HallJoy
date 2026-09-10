# MAD 68 Pro R hardware-log summary

Source: `HallJoyMAD68ProR.log`, captured on July 29, 2026.

## Identification

- VID/PID: `373B:1109`.
- bcdDevice: `0x0102`.
- Vendor HID path: interface `MI_01`.
- Windows HID report lengths: 65 bytes for both input and output, comprising a
  64-byte payload plus the report-ID byte.
- Firmware endpoint topology: IF1, EP04 OUT, EP82 IN.

## Confirmed activation

The log confirms the strict sequence:

- `A9` TX at `06:22:26.528`, valid `AA/A9` ACK at `06:22:26.540`;
- `A8` TX at `06:22:26.744`, valid `AA/A8` ACK at `06:22:26.751`;
- final `A9` TX at `06:22:26.751`, valid `AA/A9` ACK at `06:22:26.761`.

Before sending `A8`, HallJoy waited for all keys to be released and only then
allowed baseline initialisation.

## Asynchronous A0 traffic after the final A9

- Total A0 packets in the log: `272`.
- A0 packets after the final `A9` TX: `272`.
- A0 packets after the final `A9` ACK: `271`.
- Four consecutive cycles of 68 unique physical descriptors were captured.
- All four cycles exactly match the scanner order in
  `key_descriptor_map_final.csv`.
- Median interval between adjacent packets: `15 ms`.
- Observed interval range: `13..31 ms`.
- The fourth cycle ended `4.236 s` after the final `A9` ACK.
- Checksum errors: `0`.
- Malformed reports: `0`.

This rules out an explanation based solely on a queued USB/Windows backlog:
only about 7-10 ms elapsed between the `A8` ACK and the final `A9`, while the
firmware limits generation to approximately one packet per 15 ticks.

## Confirmed WASD values

- W: `0 -> 1600 -> 0 -> 2`.
- A: `5 -> 1565 -> 0 -> 0`.
- S: `0 -> 0 -> 0 -> 0`.
- D: `0 -> 2 -> 2 -> 2`.

Baselines reported in the log:

- W: `2394`.
- A: `2419`.
- S: `2444`.
- D: `2405`.

The W and A observations confirm that `A0[4..5]` follows physical key travel
and reaches the top of the internal range.

## What this log does not prove

The W and A presses occurred during the initial forced/catch-up passes. The log
does not contain a distinct press beginning at least five seconds after those
cycles ended. A separate hardware test must therefore still measure:

- the post-sweep steady-state path;
- W/A/S/D latency;
- fairness while several keys move simultaneously;
- practical smoothness of the blue UI bar and ViGEm stick/trigger output.
