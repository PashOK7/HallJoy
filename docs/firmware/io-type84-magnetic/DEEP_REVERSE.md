# IO Type 84 Magnetic v1.17: reviewed analogue path

Review date: 2026-09-15. Supersedes the August 28 conclusions in the preserved
[historical report](DEEP_REVERSE_2026-08-28_PRE_REVIEW.md).

**Status: real analogue measurements confirmed; a complete independent multi-key
stream suitable for HallJoy has not been established.** No IO backend was found
in HallJoy source. No backend or delivery EXE was changed by this review.

## Evidence and reproducibility

Both locally preserved Black/White V1.17 HEX files match the SHA-256 values in
[SOURCE.md](SOURCE.md). Their mapped addresses match, with only five differing
bytes at `0x10BA8..0x10BAC`: the Black/White product label. Protocol code is identical.
Images map `0..0x16263` plus four bytes at `0x7DFFC`; reset vector `0x205`,
initial SP `0x20007E48`.

Run `python tools/review_io_type84_firmware.py` from the repository root.
Dependencies: IntelHex, Capstone and Unicorn (the script also searches the
existing `.local/ipi-reverse-deps`). The script pins both firmware hashes.
[Results](REVIEW_2026-09-15_RESULTS.json): 22 recorded cases PASS across both images.
[Disassembly excerpts](REVIEW_2026-09-15_DISASSEMBLY.txt) preserve the reviewed
serializer, scan selection, dispatcher and transmit scheduler.

Tests execute original Thumb components with synthetic RAM. The ADC filter
at `0x6C14` is an identity stub; USB send at `0x107DC` is a capturing success
stub; normal-key processing at `0x43FC` is an entry-recording stub. Firmware
travel calculation and its subsequent smoothing helper execute normally.
These are component tests, not a full MCU, scan-timing or physical USB test.

## Command handling: simulation and calibration differ

Dispatcher `0xD7C4` accepts `AA <command>` and builds a `55` reply.
The `0x6*` branch at `0xDAF2` implements:

| Command | Observed immediate behavior |
| --- | --- |
| `64` | Clear 16 bytes of working RAM, set calibration flag `0x2000036C` |
| `65` | Call helper `0x1930`, clear calibration flag |
| `66` | Set simulation flag `0x2000036D` |
| `67` | Clear simulation flag |

The previous description that all four handlers were just direct byte writes
was too broad. The `66/67` paths are direct volatile flag changes. Their tests
preserve the calibration flag in both initial states, zero and one.

`60/68` have no measurement implementation in this branch. However, they are
**not explicitly rejected**: fallthrough `0xDD36` schedules the copied request
with a `55` header. Tests receive an echo-like reply. Such an acknowledgement
must not be mistaken for supported axis-status data.

## Report format: 64-byte mailbox, 14-byte meaningful prefix

Serializer `0xBF0` clears 64 bytes at `0x20004482` and fills this prefix:

```text
55 FB key calibration-status
max-value-le16 min-value-le16 current-adc-le16
key-stroke-le16 max-stroke-le16
```

The minimum high bit supplies byte 3 and remains present in the encoded minimum;
a decoder must mask it when interpreting the numeric endpoint. Bytes 14..63
remain zero. Scheduler `0x12620` passes 64 bytes to USB send `0x107DC`, not 14.
Actual HID interface/report-ID framing is not established by this component test.

Callers set pending byte `0x20000101`. There is one mutable buffer, not a queue.
Writing a second report while pending replaces the first before a successful
send. The test writes keys 7 and 16, then runs the scheduler: only 16 is sent.
Actual loss frequency depends on device scheduling; this test proves the
buffer semantics, not a physical loss rate.

Do not normalize key-stroke by max-stroke without checking units. Actual scanner
execution produces travel 167 with a serialized maximum field of 34. Those
fields cannot be assumed to share a numeric scale. Millimetre scaling remains
a separate integration requirement.

## Missing multi-key completeness in the simulation path

The scanner is `0x3AA8`; wrapper `0x3A90` processes 16 rows of one column.
The mapped report key comes from ROM `0x13A90`. Internal per-position travel
is at `0x2000313A`; smoothed travel is at `0x2000343A`.

At `0x3DCA..0x3DE0`, a candidate's smoothed travel is compared against the
selected travel at `0x2000010A`. A qualifying candidate becomes the selected
matrix index at `0x200000FF`. Simulation report paths check this selection.
This is not unconditional reporting of every changing position.

Reproducible component sequence with endpoints 2200/1900 and simulation enabled:

| Matrix index | ADC | Calculated travel | Selected index | Serialized report keys |
| --- | ---: | ---: | ---: | --- |
| 0 | 2100 | 93 | 0 | 0, 0 |
| 1 | 2000 | 167 | 1 | 16, 16 |
| 0 | 2150 | 43 | 1 | none |
| 1 | 2000 | 167 | 1 | 16, 16 |

The weaker key changes substantially, but does not serialize that change while
the other key remains selected. This is an executable counterexample to the old
claim of a generally usable per-key stream. It is not an exhaustive simulation
of all scan cycles, release transitions or USB timing. Independent simultaneous
key depths and reliable release reporting cannot be promised from this route.

## Correction: not all five serializer callers are simulation-gated

Four scanner call sites (`0x3C3C`, `0x3F74`, `0x3FA2`, `0x3FEE`) are simulation
paths. The fifth at `0x4028` is reached through the **calibration** flag check
at `0x3FF8`, followed by pending/per-key bookkeeping checks.

Consequently the old statement that every `55 FB` packet requires `66` is false.
Calibration can also produce this format. It must not be used as an alternative
normal runtime subscription merely to obtain more keys.

At `0x4048`, calibration suppresses continuation into normal-key processing.
With calibration off and the normal-processing state enabled, the synthetic
scanner reaches `0x43FC` with simulation both on and off. This is positive
control-flow evidence that simulation does not itself stop this path. The body
of that function is stubbed in the tests, so this does not establish end-to-end
ordinary typing, USB coexistence, latency or compatibility with another driver.

## Alternative routes checked and remaining scope

The full visible vendor dispatcher `0xD7C4..0xDD3E` was reviewed, including:

- `10/11/12/14/15/16/17/18/1C`: fixed flash/configuration region readers
  (bases `9000/9200/9600/9A00/9C00/B000/B600/B200/BC00`), not live matrix getters.
  Follow-up: `10` also has a conditional flash-write side effect at `0xD90E..0xD922`;
  do not classify it as unconditionally read-only.
- `13`: settings/status fields, not a complete live depth array.
- `21..28`: configuration write paths; not analog read candidates.
- `32`: lighting-related state/data, not a live depth snapshot.
- Low-nibble `F` paths: reset/configuration mutation; not analog read candidates.
- `60/68`: echo-like fallback as explained above.

No alternative complete live analogue getter or per-key subscription was found
in these handlers. The existence of ADC/depth arrays in SRAM does not itself
make them available to the host. This conclusion applies to the pinned V1.17
images and inspected paths, not all IO models, newer firmware or every possible
undocumented route.

## Support decision

The old finding of real analog values was correct. The implied readiness of
that route for independent multi-key gameplay was not justified. Implementing
it as normal HallJoy support now would conceal known completeness problems.
A complete snapshot or reliable per-key feed with normal typing is still needed.
Vendor protocol information or a newer firmware mechanism would directly address
that gap. Physical testing can characterize this existing test mode but cannot
be assumed to remove its observed selection and mailbox behavior.

## Follow-up: command parameters and application receive path

The next bounded pass used `tools/review_io_type84_routes.py`:
[20 grouped checks PASS](ROUTE_REVIEW_2026-09-15_RESULTS.json), including 62
payload-position variations per image. These complement, rather than replace,
the 22 initial component checks.

- The immediate `66` handler does not consume a key-selection parameter.
  Varying each of its 62 payload bytes individually preserves an existing
  selected index of 7 and selected travel of 167. This supports the disassembly;
  it is not exhaustive arbitrary command fuzzing.
- The fixed configuration readers tested (`11/12/14/15/16/17/18/1C`) return flash
  data unchanged when both live travel arrays change. Their decoded source is a
  fixed flash base plus a 16-bit request offset, not a host-selectable RAM address.
  Command `10` was excluded from execution because of its conditional flash write.
- Application bridge `0x12FE8` polls helper `0x1062C` for logical channels 0 and 2.
  Channel 0 copies one received byte to `0x200062E4`; channel 2 fills the vendor
  request buffer `0x2000637C` and sets ready flag `0x2000036F`. The periodic caller
  runs the bridge at `0xF946`, then dispatcher `0xD7C4` at `0xF94A`. A stubbed RX
  test executes that handover successfully with `66`. These are internal channel
  indices, not independently verified physical interface/endpoint numbers.
- A linear Thumb scan found only those two direct calls to the receive helper.
  This is not proof against indirect callbacks or alternative USB control paths.
- The transmit scheduler also references buffers `0x200044C2`, `0x20004502`,
  `0x20004542` and pending bytes `0x2000022E..0x20000231`. No additional producer
  was found through direct PC-relative references in this pass. Their presence
  alone does not establish a usable snapshot. Indirect/computed writers remain
  a reachability question, not a proven absence.

The follow-up does not reveal a usable alternative. Remaining targeted research
is USB control/feature-report callbacks and indirect writers of those additional
buffers. Do not report these as fully ruled out. No physical device or EXE was
changed, and no firmware writes were performed.

## USB feature/control closure pass

The additional analysis executes the firmware scatter initializers, including
its compressed initialized-data block (`0x15C48 -> 0x20000000`, 0x1200 bytes)
and zero-initialized region (`0x20001200`, 0x6C48 bytes). USB registration
`0x1076C` copies the actual initialized configuration at `0x20001168`.
The registered callbacks at `0x200066A0 + {0x80,0x84,0x88}` are:

| Callback | Registered target | Meaning established in this pass |
| --- | --- | --- |
| GET input report | null | No application producer registered here |
| GET feature report | `0x12C95` | IDs 0/2 copy the fixed 64-byte buffer `0x200062F8` |
| SET feature report | `0x1376D` | Application feature handling; completion also copies the received payload to that buffer |

`tools/review_io_type84_usb.py` executes class dispatcher `0x10864`, GET callback,
and SET completion `0x1032C`. It runs 3,072 GET_REPORT requests per image: four
synthetic enumerated interfaces, three report types, all 256 report IDs.
Only feature IDs 0/2 on configured feature interfaces 0/3 succeed, returning the
same fixed buffer. No reads from either live travel array occur. A complete
SET/GET sequence echoes the supplied payload; putting `AA 66` there does not
start simulation. Changing both live depth arrays does not change the reply.
A vendor-type setup request is rejected by this class handler.

[Ten grouped results PASS](USB_REVIEW_2026-09-15_RESULTS.json), including 6,144
GET requests, SET/GET cases, initialized-data pointer checks and the original
selected-key counterexample repeated after firmware RAM initialization.
[Annotated disassembly](USB_REVIEW_2026-09-15_DISASSEMBLY.txt).
Control transport send/receive/stall helpers are stubbed; enumeration state is
synthetic, using feature capacities from the registered configuration. These
are code execution results, not physical USB captures.

The application USB registration at `0xDD9E..0xDDA6` passes `0x5E01,0,0` to
`0x10BB0`. The first callback sets a status bit; the other two optional callback
slots are null in this registration. No second application analog getter was
established through that registration.

### Additional transmit buffers

The three extra buffers and four pending flags remain without an established
producer. Their literal addresses occur only in the scheduler's pool. Searching
the decompressed initialized data also finds no pointers into the extra buffers
or their flags. A local constant-propagation pass over decoded instructions
finds serializer stores only into the known `55 FB` buffer and stores clearing
the extra flags in the scheduler. It finds no new measurement producer.

This supports the interpretation of retained unused scheduler branches. It is
not a whole-program proof: local constant propagation and a linear Thumb decode
cannot exclude every computed pointer, indirect write or unusual runtime state.
No host command to populate or select those extra buffers has been established.
Do not present their allocation as evidence of another usable stream.

### Available firmware and practical conclusion

The official `https://web.io.vision/update.json` was fetched again during this
pass; [saved manifest](UPDATE_MANIFEST_2026-09-15.json). Both Type 84 Magnetic
entries still name V1.17 and the same filenames. No newer Type 84 Magnetic
version is listed there. Other models in that manifest are not evidence about
Type 84 compatibility and were not enabled or flashed.

**No suitable independent multi-key analog mechanism was found.** The positive
finding remains a selected-key simulation stream; the extra HID feature route
is a fixed-buffer exchange, not a live matrix snapshot. All concrete candidates
from the preceding pass have now been investigated without a usable result.
Remaining uncertainty is general whole-program reachability, not a discovered
command waiting to be implemented.

The present support blocker is the lack of an established complete host-readable
analog feed. A vendor description of an existing getter, a firmware implementing
one, or new concrete evidence is needed before production integration is
justified. This does not prove that no undocumented route could possibly exist.
No HallJoy source/backend or delivery EXE was changed by this research.

## IO Type 68 Magnetic comparison (2026-09-15)

The official manifest also lists Type 68 Magnetic BL/WH V1.37, product ID 598.
These are separate images from Type 84 Magnetic V1.17 (product ID 32982).
Downloaded for preliminary comparison into the existing `.local` directory:

| File | SHA-256 |
| --- | --- |
| `IO_Type68_Magnetic_BL_V1.37.hex` | `3d1f4e3ceec2761d7856f729db20a18aa021a7ae066203f55cd9fa25790d6f19` |
| `IO_Type68_Magnetic_WH_V1.37.hex` | `002bb30f40da2cd39b77294bc0c15f26b73d24015e3758e73ca0f96fec6c6655` |

Sources: `https://web.io.vision/IO_Type68_Magnetic_BL_V1.37.hex` and
`https://web.io.vision/IO_Type68_Magnetic_WH_V1.37.hex`.
Each HEX is 255277 bytes and maps the same ranges as Type 84. Initial SP is
`0x20007DD8`, reset vector `0x205`. The two Type 68 color images differ in two
mapped label bytes at `0x10B88..0x10B89` (BL/WH).

Preliminary code evidence establishes shared implementation, not identical
protocol support across every path:

- Serializer bytes `0xBF0..0xC37` match Type 84 exactly.
- The `0x6*` branch at `0xDAC2` again handles `64/65/66/67`; simulation flag
  changes are at `0xDAF2/0xDAF8`. There are no `60/68` cases in that branch.
- Selection code `0x3E0A..0x3E20` again compares a candidate travel to selected
  travel and updates the selected index on a qualifying comparison. Its state
  offset is +6, whereas Type 84 uses +5: do not reuse RAM addresses blindly.

Thus Type 68 has a distinct but closely related firmware. It is a separate
research target, not evidence that Type 84's limitation applies to every IO
keyboard. Conversely, the common serializer and selection logic do not justify
assuming Type 68 solves the multi-key problem. No full Type 68 emulation,
complete alternative-route review or HallJoy backend implementation was done in
this preliminary comparison. Other manifest names without Magnetic have not
been classified as Hall-effect models by this evidence.
