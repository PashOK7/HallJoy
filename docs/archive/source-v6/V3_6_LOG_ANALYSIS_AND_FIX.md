# v3.6 hardware-log analysis and correction

## v3.5 test result

- two successful sessions with confirmed `A9 -> A8 -> A9` acknowledgements;
- `68/68` physical descriptors in each session;
- `2914` valid A0 packets in total;
- `unknown_A0=0`, `malformed=0`, `checksum_errors=0`, `read_errors=0`, and
  `write_errors=0`;
- real analogue transitions on at least 28 keys, reaching values of `1600`;
- full mode did not activate: `steady=0`, `mode=EMERGENCY WASD`;
- the log contained no digital Raw Input edge, so the old proof path could not
  succeed.

## Root cause

v3.5 allowed full mode only after correlating an A0 transition with a Raw Input
event. Windows exposed the device's keyboard collections on the tested system,
but the worker received no digital edge events. The A0 protocol itself worked
correctly.

## Correction

After three ordered startup cycles and the 4.5-second forced-sweep grace period,
the backend accepts an A0 transition as steady-state proof only when it:

1. belongs to a known descriptor;
2. occurs after a complete 68/68 snapshot exists;
3. occurs after the forced-sweep grace period and at least 150 ms after an
   ordered cycle;
4. crosses the key's individual actuation threshold;
5. changes by at least 64 units, filtering idle Hall-sensor noise.

In the supplied log, the first qualifying evidence is the W transition from
`285` to `474` across a threshold of `350`, at approximately `18:42:00.764`.
At that point v3.6 would switch from Emergency WASD to Full mode and publish all
67 HID keys.
