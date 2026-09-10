# Pwnage Zenblade 65 V2: protocol reconnaissance

Date: 2026-08-20

Status: the official host protocol is statically mapped; no live-travel command
was found in the host software; no production support has been added.

## Executive conclusion

The retail Zenblade 65 V2 must not be enabled by adding its VID/PID to an
existing Madlions or Keychron family. The new keyboard uses a direct 64-byte
VIA/QMK-like request/response protocol with Pwnage Hall-effect extensions.

The official host software contains setters and getters for actuation
configuration, key mapping, profiles, SOCD and lighting. It contains no command
that reads the current physical depth or raw Hall value. It also contains no
passive live-depth parser. Listening for unsolicited reports would therefore be
an uninformative diagnostic and is explicitly not the next experiment.

The next useful hardware experiment is active and read-only: use the proven
Hall getter command `20` to discover whether an undocumented parameter selector
returns live sensor values. Responses must be compared while a known key is
unpressed, moved slowly, held and released. Binary keyboard events must never be
used to infer which analog position changed.

An official-representative-linked SOCD updater and firmware V0022 was later
recovered for the **older** Zenblade 65. Its manifest proves that the old
keyboard runs as `3662:1001` and re-enumerates its bootloader as `3662:1002`.
That image is not V2 firmware and must never be offered to or run against a V2,
whose normal runtime identity is also `3662:1002`. Static disassembly of V0022
found no command that returns current Hall travel. No V2 firmware image or V2
updater has been found, so active read-only discovery on real V2 hardware is
still required.

## Sources pinned for reproducibility

- Official driver page: <https://pwnage.com/pages/drivers>
- Official Web Hub: <https://pwnage.com/pages/drivers-hub>
- Official product page: <https://pwnage.com/products/zenblade-65>
- Web Hub JavaScript selected on 2026-08-20: `hub-3.4.4.min.js`,
  2,466,514 UTF-8 bytes, SHA-256
  `94695F76147F2390DFCEE11F1D5F27CD02D016C9D17845B86B5E9B0A1B088E8B`.
- WASM used by the older Zenblade generation: 90,940 bytes, SHA-256
  `66D3E468D724E1A7BBE1BAB2C84786C5AFBF07757703B59F7D6244A9C100D913`.
- Desktop Hub 0.0.17 `app.asar`: 85,799,825 bytes, SHA-256
  `896613E145EF77353B6B01AA88263CB5207AA07792F68588CA359CB8BC359AD4`.
- Pwnage subreddit SOCD firmware post and representative-provided download:
  <https://www.reddit.com/r/pwnage/comments/1keqvzo/zenblade_65_socd_firmware/>.
- Downloaded V0022 archive: 3,324,158 bytes, SHA-256
  `66A4133BCFF5C5CC9993CB4E203E97E5D24FB91C1FED17108F4A55A4C1DCE177`.
- `ZENBLADE65_V0022.hex`: 129,793 bytes, SHA-256
  `80B482CFCBF085507B1183304FA469AA9B11EB6838FF86EBE50F19180E16F743`.
- Independent, hardware-tested macOS implementation used only as corroboration:
  <https://github.com/edgetr/Zenblade-65-V2-MacOS-App>.
- QMK VIA command definitions:
  <https://github.com/qmk/qmk_firmware/blob/master/quantum/via.h>.
- QMK Raw HID default usage documentation:
  <https://github.com/qmk/qmk_firmware/blob/master/docs/features/rawhid.md>.

The hashes identify the exact implementations inspected. Future work must
compare them before assuming that the live Pwnage assets are unchanged.

## Official WebHID identities

| Hub generation | VID:PID | Usage page:usage | Report ID |
|---|---|---|---|
| older Zenblade | `3662:1001` | `FF01:0001` | `0` |
| new Zenblade | `3662:1002` | `FF60:0061` | `0` |

For PID `1001`, the Hub queries a version and selects an older protocol/UI.
For PID `1002`, it hard-codes device version `35` and screen generation `3`.
The internal string `Zenblade65 v3` refers to this protocol/UI generation, not
the retail product name.

The exact descriptors on the owner's keyboard still need to confirm PID
`1002`. It is the strong official-code expectation, not yet physical HallJoy
evidence.

## Critical protocol-generation correction

The older Zenblade code routes commands through the pinned WASM and includes
packets such as `00 71 ...` with a generated check byte. Those packets do **not**
describe the new PID `1002` screen-3 implementation.

The PID `1002` implementation is directly constructed in JavaScript:

- `HIDDevice.sendReport(0, payload)` sends a zero-padded 64-byte payload;
- replies arrive through `inputreport`;
- the response queue correlates replies by an expected byte prefix;
- no checksum byte is generated or validated;
- the command vocabulary reuses standard VIA commands and adds Pwnage-specific
  Hall commands.

Consequently, the previous idea of implementing PID `1002` from the WASM
`00 71` format was wrong and has been removed from the project handoff.

## Proven screen-3 command map

### Standard VIA-compatible commands

| Command | Meaning in the Hub |
|---|---|
| `01` | protocol/version query |
| `02` | get keyboard value |
| `05` | set one dynamic keymap keycode |
| `0E` | get macro buffer |
| `0F` | set macro buffer |
| `12` | get dynamic keymap buffer |
| `07` / `08` / `09` | custom lighting set/get/save paths |

The VIA keyboard-value selector `03` is the digital switch matrix. It is not a
Hall-depth matrix and must not be presented as analog support.

### Pwnage Hall configuration

The generic getter is:

```text
20 <parameter-id> <profile> <offset> <count>
```

The reply begins with the same five bytes, followed by `count` little-endian
16-bit values. The Hub reads the 72 physical matrix positions in chunks of at
most 25 values.

The generic writer is:

```text
21 <parameter-id> <profile> <offset> <count> <little-endian values...>
```

Save is `21 F3`. Known parameter selectors are:

| Selector | Configuration value |
|---|---|
| `05` | traditional actuation distance |
| `07` | rapid-trigger press threshold |
| `08` | rapid-trigger release threshold |
| `09` | traditional/magnetic mode |
| `0F` | bottom zone |
| `10` | properties, including continuous RT |
| `11` | switch type |
| `12` | rapid-trigger start distance |

These are persistent configuration matrices. None is current key travel.

### Other direct commands

| Commands | Feature |
|---|---|
| `22` / `23` | get/set profile |
| `30` / `31` | get/set SOCD |
| `32` / `33` | get/set Mod-Tap |
| `35` / `36` | get/set Dynamic Keystroke |
| `37` / `38` | get/set Rappy Snappy |
| `39` / `3A` | get/set Toggle |

The screen-3 code contains no other static command that reads a live sensor
matrix. The independent macOS implementation reproduces the same direct
protocol and also contains no live-travel getter.

## Physical matrix map

The official Hub exposes 68 logical keys backed by an 8x9, 72-position matrix.
Logical order maps to matrix coordinates as follows:

```text
0,0 1,0 0,1 2,1 0,2 2,2 0,3 2,3 0,4 1,4 0,5 1,5 1,6 1,7 0,8
3,0 2,0 1,1 3,2 1,2 3,3 1,3 3,4 2,4 3,5 2,5 0,6 0,7 3,8 1,8
5,0 5,1 4,1 5,2 4,2 5,3 5,4 4,4 5,5 4,5 3,6 2,6 2,7 2,8
4,0 6,1 7,2 7,3 6,3 4,3 6,4 7,5 6,5 5,6 4,6 5,7 5,8 4,8
7,0 7,1 6,2 7,4 7,6 6,6 7,7 6,7 7,8 6,8
```

The four unused matrix cells must be identified from real responses rather than
guessed from UI geometry.

## Firmware and updater audit

The current official desktop installer (0.0.17) and the earliest obtainable
installer (0.0.1) were unpacked and inspected.

- Neither package contains a Zenblade firmware image.
- The firmware directories contain StormBreaker/Trinity mouse and receiver
  assets; later packages add related mouse assets.
- The desktop updater's selectable product list excludes Zenblade.
- The Hub only identifies PID `1002` as fixed version `35`; it has no Zenblade
  update workflow.
- The bundled `spi-tool-1.0.0.exe` exposes mouse sensor speed/acceleration
  commands and is unrelated to a keyboard Hall matrix.
- A source/repository search found no public Pwnage Zenblade firmware tree.

Those package findings remain correct, but a separate Pwnage subreddit post
contains a Google Drive link posted by `Pwnage_Lemonade` for an older keyboard's
SOCD update. The archive contains `ZENBLADE65_V0022.hex` and a standalone
updater. It was unpacked and statically inspected; the updater was not run.

Its `configs.xml` establishes this exact transition:

```text
runtime device:  vid_3662&pid_1001
command channel: vid_3662&pid_1001&mi_02
bootloader:      vid_3662&pid_1002
image:           ZENBLADE65_V0022.hex
```

The image starts its application at flash address `0x4000`, consistent with a
separate bootloader below it. Its command dispatcher masks the command byte to
six bits and implements the following low command IDs:

```text
02 04 05 09 0C 0D 18 19 1A 1B 1C 1D
20 21 25 26 30 31 34 35 3E 3F
```

This resolves an earlier ambiguity in the old WASM protocol: wire byte `71`
dispatches as low command `31` because `71 & 3F = 31`. Its handler reads two
68-entry persistent trigger/configuration arrays at SRAM `0x10000BCC` and
`0x10000C10`; it does not read the dynamic Hall-processing buffers. A complete
cross-reference of every host command handler likewise found no handler that
exports the current sensor/travel arrays. Therefore V0022 provides useful OEM
lineage and protocol evidence, but not a live-depth request that HallJoy can
reuse.

There is still no known V2 firmware image to reverse or flash. More importantly,
the old bootloader PID collides with the V2 runtime PID. Do not launch this old
updater while a V2 is connected, do not admit a device by PID alone, and do not
substitute V0022 for V2 firmware.

## Comparison with existing HallJoy/UAP families

The shared usage `FF60:0061` is QMK Raw HID's default vendor collection and not
a protocol fingerprint.

| Family | Request model | Why it cannot be grafted |
|---|---|---|
| Madlions | `02 96 1C ...`, 33-byte construction | wrong commands, lengths and response model |
| Keychron/Lemokey HE | custom `A9` protocol | Pwnage emits no matching command in its host code |
| DrunkDeer | proprietary matrix request | different identity, framing and map |
| Wooting/Razer/NuPhy | streaming family parsers | different identities and report layouts |

Keychron's open Hall-effect branch is useful as architectural precedent: it
defines an explicit active real-time-travel getter. It is not evidence that the
same selector exists on Pwnage.

## Correct next experiment: active read-only discovery

The first owner build must actively communicate with the exact Pwnage Raw HID
interface. It must not merely listen for passive reports.

### Phase A: identity and proven read-only baseline

1. Record every HID interface for VID `3662`, including PID, usage, report
   lengths, release, path and strings.
2. Admit protocol probing only on exact `3662:1002`, `FF60:0061`, report ID 0,
   with compatible 64-byte I/O capabilities.
3. Issue standard VIA protocol/version and keyboard-version getters.
4. Read current profile and all known Hall configuration selectors through
   command `20` to prove framing, correlation, matrix length and stability.

The VIA digital-matrix getter may be logged as a timing reference only. It must
not be used to assign analog records to keys.

### Phase B: hidden Hall selector sweep

Use only the already-proven getter command `20`; do not guess writer or
bootloader commands.

1. Sweep initially unused parameter IDs in the small neighborhood of the known
   selectors, then expand to `00..FF` only if responses remain safe and stable.
2. Request one matrix position first; expand a responsive selector to all 72
   positions using the official chunk size.
3. Capture baselines while untouched, then compare slow press, partial hold,
   bottom-out, release and simultaneous multi-key movement.
4. Log only semantic changes: new selector response, changed byte ranges,
   changed decoded values, timeout/error transition and periodic compact
   heartbeat. Do not impose a total file-size limit or stop logging after a
   duration; suppress unchanged high-rate duplicates instead.
5. Keep exact raw request/response bytes beside decoded deltas so an apparent
   depth scale can be independently verified.

A candidate is valid only if one or more stable matrix positions change
monotonically with physical travel, return to baseline on release, preserve
identity during multi-key motion, and repeat after reconnect.

### Phase C if command `20` has no live selector

Do not fall back to passive listening or binary-event correlation. The next
evidence source must be one of:

- an OEM/manufacturing configurator with a live sensor/calibration page and a
  USB capture of its active requests;
- a firmware image or updater that contains the command dispatcher;
- a safe read-only command-ID discovery plan based on recovered firmware or a
  confirmed protocol sibling.

Blindly sweeping arbitrary top-level commands is not authorized because the
unknown command space may include writes, reset, bootloader or calibration.

## Production acceptance gates

If an active live-depth getter is proved, implement Pwnage as a native HallJoy
family with:

- exact descriptor/capability admission;
- one owner for the 64-byte request/response queue;
- strict response-prefix correlation and timeouts;
- a proved 72-position physical matrix and 68-key export map;
- measured raw-to-normalized scaling;
- simultaneous-key, reconnect, stale-input and cancellation tests;
- smart change-based diagnostics with no arbitrary log-size cutoff.

No production compatibility claim is allowed until those gates pass on the
owner's physical keyboard.
