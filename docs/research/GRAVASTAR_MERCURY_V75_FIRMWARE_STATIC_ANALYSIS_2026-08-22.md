# GravaStar Mercury V75 firmware static analysis

Date: 2026-08-22  
Status: firmware-proven and implementation-tested for V75, V75 Pro and V75
Lite; no physical-device transcript yet.

## Scope and safety

The received file was analysed statically and was not executed:

- source file: `GravaStar75.exe`;
- size: 16,060,416 bytes;
- SHA-256:
  `116735B7974AE8487DD424E5F7C1D4250A45495B06B3E1916BF0CAA53939DA01`;
- PE: unsigned x64 executable wrapped with Enigma Virtual Box/Protector.

The wrapper was unpacked without launching it. It contains a separate updater,
Qt/HID dependencies, `config.ini`, and three application firmware images. The
absence of a signature and an independently authenticated download source means
this report does not claim that the wrapper is safe or an official GravaStar
release. Product names, USB identities and protocol results below are claims
about the extracted bytes only.

## Extracted targets

| Model in package | USB VID:PID | Board ID | App | Firmware SHA-256 |
|---|---:|---:|---:|---|
| GravaStar Mercury V75 | `1CA5:2201` | `16052201` | `1.0.8` | `BDD05FF86B40448C0DFA3B4642B023AD59B59C94CD40F48C6EE8980C8B2938A3` |
| GravaStar Mercury V75 Pro | `1CA5:2202` | `16052202` | `1.0.8` | `06E640D02FFB22ACFFCD91AB6C0ACDBA4A6ABA5B981AF4735C50E8AA4CF3AD7F` |
| GravaStar Mercury V75 Lite | `1CA2:2201` | `2E022201` | `0.2.5` | `CA188314481B404C1D21B962557758262485F7C36C6B11A622D1524283F4B7C9` |

All three updater profiles select vendor HID usage page/usage `FFA0:0001`.
The unrelated `[General]` fallback in `config.ini` names an ET65 HE and a
firmware file that is not present; it must not be treated as a fourth extracted
target.

V75 and V75 Pro are Cortex-M/Thumb RT-Thread applications. V75 Lite is a
separate RV32/RISC-V implementation. Therefore the Lite result was checked
independently rather than inferred from the other two binaries.

## Protocol result

All three extracted applications implement the SparkPlayJoy framed 6x21
analogue protocol already used by HallJoy's `aula_win60he` family engine. This
is not the legacy SparkLink `01 02` row-read protocol in
`backend_sparklink.inc`.

The 64-byte logical HID report begins with:

```text
5C <payload-length> <command> <checksum> <payload...>
```

The response command is `request-command | 80`. The checksum is the low byte
of:

```text
35 + 5C + payload-length + command + last-payload-byte
```

Long responses continue over multiple 64-byte logical reports. On the proven
HallJoy implementation this corresponds to a 65-byte Win32 HID buffer whose
first byte is the zero report ID; the physical V75 HID capabilities must still
be recorded before claiming that Windows transport detail for these identities.

### Read-only capability and analogue commands

| Purpose | Request semantics | Proven response semantics |
|---|---|---|
| Device sync | command `01` | command `81`; board/device data plus serial, `App V...` and build descriptors |
| Travel scale | command `00`, order `25` | command `80`; echoed order, precision, minimum and maximum travel |
| Default physical map | command `2B`, operation `00`, two row indexes | command `AB`; two firmware-mapped rows of 21 factory key identifiers |
| Active base-layer functions | command `23`, operation `00`, fourteen four-byte queries | command `A3`; fourteen correlated key/layout/16-bit-function records |
| Live travel | command `12`, selector `02`, half `01` or `02` | command `92`; 63 little-endian `uint16` samples per half |

The two travel halves form a fixed 6x21 matrix. This is direct sensor travel;
neither firmware nor the HallJoy family transaction needs a Windows digital
keydown to identify an analogue position.

The exact travel requests are:

```text
5C 04 12 A6 02 01 FF FF
5C 04 12 A6 02 02 FF FF
```

Default-map command `2B` converts the keyboard's own stored matrix to factory
key identifiers. Command `23` then reads the current 16-bit function assigned
to each of those identifiers. Consequently V75 support must use this live,
correlation-checked map. A hard-coded physical layout, guessed HID table, or a
digital-event learner would be both unnecessary and incorrect.

### Scale

V75 and V75 Pro use the same order-`25` response path and expose the active
profile's maximum travel. Their static code reports a 5 micrometre precision
and 5 micrometre minimum; the maximum is read from profile state and must be
accepted from the live response rather than guessed.

V75 Lite independently builds the same response with:

- precision: 5 micrometres;
- minimum: 5 micrometres;
- maximum: 3500 micrometres.

HallJoy's compatible-family decoder already accepts and normalizes these live
values.

## Per-model confidence

### V75 and V75 Pro

The binaries differ in product/board data, but the relevant dispatcher,
checksum, sync, order-`25`, default-map, active-function and two-half travel
implementations are identical at the corresponding code locations. Their
protocol compatibility is firmware-proven.

The full factory map is not contained in the supplied application image: the
application reads it from device-resident storage beyond the image boundary.
This is not a missing runtime dependency because command `2B` returns that map
from the physical keyboard. It does mean that no exact V75 layout should be
invented from this file alone.

### V75 Lite

The independent RISC-V implementation proves the same response builders and
request handlers:

- sync response `81` with three 16-byte descriptors;
- order `25` scale response;
- response `92` for the 6x21 status/travel command, including 3x21 `uint16`
  samples for each travel half;
- response `A3` with fourteen active-function records;
- response `AB` with two 21-position default-map rows.

This is strong firmware proof of the same read-only capability chain despite
the different MCU. It is not yet a physical validation of endpoint selection,
HID report lengths, timing or reconnect behaviour.

## HallJoy architecture consequence

The correct implementation is to reuse and generalize the existing
SparkPlayJoy framed 6x21 engine. Creating a GravaStar copy would duplicate a
session/protocol implementation, while routing these devices to the legacy
SparkLink row reader would select the wrong wire protocol.

The implementation now uses one bounded identity table shared by 6x21
discovery and legacy SparkLink exclusion:

- Aula WIN 60 HE MAX `1CA2:1902`, board `0A021902`;
- GravaStar V75 `1CA5:2201`, board `16052201`;
- GravaStar V75 Pro `1CA5:2202`, board `16052202`;
- GravaStar V75 Lite `1CA2:2201`, board `2E022201`.

An exact VID/PID grants permission only to run the existing exclusive,
read-only proof. HallJoy still requires `FFA0:0001`, the exact Win32 report
shape, valid framing/checksums/correlation, plausible scale, a unique physical
map, two identical complete active-map generations and plausible travel. A
known USB identity is also correlated with its proven board ID after sync.
Only then is the exact interface path claimed and analogue data published.
Unknown brand-compatible siblings retain the previous complete structural
proof and are never upgraded to an exact physical claim.

The production engine should eventually be named for the SparkPlayJoy 6x21
protocol family rather than one Aula model. A broad symbol/file rename is not
required to add support correctly and should be separated from the functional
change to keep regression evidence understandable.

## Fn, Menu and other non-letter positions

The protocol transports all physical positions in the 6x21 matrix. Menu is an
ordinary keyboard usage when the active function returns `0065`. A proprietary
Fn function can be returned as a 16-bit vendor value such as `F001`; it is not
an 8-bit HID usage and must not be truncated or replaced with a fabricated key.

HallJoy already has a common extended semantic key code for analogue Fn:
`halljoy::keycode::kFn` (`0x409`). The 6x21 engine now preserves the complete
16-bit active function and maps the firmware-proven `F001` function directly to
that code. Menu remains its real USB keyboard usage `0065`. Unknown `Fxxx`,
macro and internal functions remain filtered rather than truncated or guessed.

The extended Fn value is stored and published in the same matrix snapshot as
ordinary keys, with arrays sized to the common extended key domain. It is not
learned from, triggered by or delayed until a Windows digital event.

## Implemented admission and validation

The functional change reuses the existing engine rather than copying a
GravaStar backend. It adds exact USB/board profiles, prevents the legacy
SparkLink reader from opening those exact interfaces, preserves the selected
VID/PID in native ownership, expands 6x21 publication/diagnostics to the common
extended key domain, and raises diagnostic active-position capacity from 60 to
the full 126-position matrix.

Portable protocol, oracle, end-to-end, diagnostic-metrics and routing tests
pass, as does the complete native suite including 250,000 parser-fuzz
iterations. The official MSVC Release x64 production build and isolated
single-file diagnostic build both complete with zero errors. This is
implementation evidence, not a substitute for the physical run below.

The extracted firmware hashes, updater identities and `FFA0:0001` profile were
re-read before the final tester build and still match the table above. The
diagnostic now also has a conclusive-outcome contract: every handled run emits
one `diagnostic.verdict` containing its deepest proof stage, first material
failure, observed identity, last open/protocol error and matrix count. A matrix
stream reports `analog_stream`; every other result explicitly requests the log.
Diagnostic-only enumeration can report a changed family VID/PID or a
GravaStar-branded SetupAPI identity without opening that unknown interface.

The original physical diagnostic passed local startup/privacy/shutdown but was
superseded after its returned trace exposed an overly strict firmware gate. Its
SHA-256 `3F987F6CFCBD38396D8E3DE30237A75183CC7D5D7884DFB36C05ABB399548AD0`
must not be distributed. The corrected exact diagnostic passes an isolated
startup/privacy/shutdown and
verdict smoke. It accepts `WM_CLOSE`, exits with code zero, writes the complete
lifecycle, leaves no process, keeps its SHA-256 unchanged and exposes neither
private filesystem/raw HID paths nor unrelated hardware inventories. The
earlier `29E123...A19` artifact failed privacy acceptance; the later
`8FEB8E...BD50` artifact was privacy-safe but lacked the mandatory final
verdict. Neither may be distributed. The current diagnostic is 8,680,960 bytes
with SHA-256
`E71EDB6BA9734F6019F5C22EAF48ED6DD299EA91D1F00E4D564705D66312C3D5`.

## First physical V75 trace and correction

The returned Windows trace directly confirms `1CA5:2201`, `FFA0:0001`, 65-byte
input/output reports, board `16052201`, App `V1.0.8`, 5 um minimum/precision,
3500 um maximum, 79 physical positions, 78 default mappings and 79 active
mappings. It retains `F001` and contains valid non-zero travel values. The file
was copied at 4.969 seconds while HallJoy was still running, so it has no final
verdict and cannot prove shutdown, sustained rate or reconnect.

It nevertheless made the old failure conclusive: each proof had valid later
stages but mismatch mask `00000001`. HallJoy incorrectly required the V75 sync
to contain Aula model bytes `C0/01/00`; the real V75 reports `00/04/00` there.
The exact registered V75 now proves its expected board plus structured sync and
the unchanged full live protocol. Unknown family candidates still require the
strict Aula signature. Deterministic semantic failure also waits for an actual
device change rather than repeating the proof on a one-second timer.

## Required physical validation

The first physical run has already completed items 1-5 and established direct
travel for item 6. One bounded run of the corrected diagnostic should finish
the runtime portions, without any configuration writes:

1. actual analogue matrix publication after the corrected exact-board gate;
2. smooth travel and release-to-zero for ordinary keys, arrows, navigation,
   Menu and Fn, including representative simultaneous input;
3. sustained polling plus disconnect and reconnect results;
4. handled window close so the mandatory final verdict is flushed before the
   log is copied.

This single run is validation, not matrix learning. The implementation already
obtains the map directly from the keyboard before polling and must fail closed
on any contradictory response.

## Conclusion

The received package does contain firmware matching the reported GravaStar V75
family. For all three included variants, analogue travel comes from a direct
vendor-HID 6x21 matrix protocol. HallJoy now admits the three exact identities
through the existing live map/scale/correlation proof and preserves Fn/Menu
without digital correlation. No UAP dependency, digital-key fallback, guessed
layout or guessed travel scale is used. Physical V75 transport, board, map,
scale and direct travel are proven; corrected analogue publication, sustained
runtime and reconnect remain pending one final bounded diagnostic run.
