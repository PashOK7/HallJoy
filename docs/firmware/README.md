# Firmware knowledge base

The internal manifest verifies the
`MAD68_Pro_R_Firmware_Knowledge_Base_2026-07-29.zip` archive: all 69 tracked
files match their recorded SHA-256 hashes and sizes.

Principal confirmed findings:

- vendor HID topology: IF1, EP04 OUT, EP82 IN, 64-byte payload;
- activation sequence: serialized `A9 -> A8 -> A9`, with acknowledgements;
- asynchronous packet type: `A0`;
- main analogue value: big-endian `A0[4..5]`, range `0..1600`;
- 68 physical descriptors across 72 scanner slots;
- runtime command allowlist: volatile `A8` and `A9` only;
- the stock scheduler limits the aggregate stream to approximately one packet
  per 15 ticks.

Start with `00_README_MASTER.md` inside the archive.
