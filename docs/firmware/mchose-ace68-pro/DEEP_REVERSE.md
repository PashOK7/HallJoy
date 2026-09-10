# MCHOSE Ace 68 Pro (`41E4:2116`) firmware reverse log

Static assessment on 2026-08-29 of the verified application image
`update_ace68pro.4ed9aa6d_562fcc8c87ad.bin` (SHA-256
`105E3C69E2F0DF779EB66FB345949530829241F0F1F27125BD47C830FE2621E9`).
This is a separate target from Ace 68 I (`41E4:2114`); no Ace 68 I command is
classified as valid for this device merely because its product name is similar.

### Retail-name caveat

MCHOSE's current web catalogue has exactly one `41E4:2116` record and calls it
`Ace 68 Pro`; the corresponding official updater is also named `ace68pro`.
The device descriptor embedded in that updater, however, calls the USB product
`Ace68-II`. A USB PID proves the running firmware/USB personality, not the
retail label printed on a unit or its original sales listing. The owner reports
this unit as regular Ace 68, so technical conclusions below are deliberately
about `41E4:2116` / `Ace68-II`; they must not rely on resolving that marketing
name conflict.

A saved Ozon page for SKU `3555727101` adds direct retail evidence: its stated
part number is `MCHOSE-ACE68`, while the selected seller variant is
`WhiteLine-iceRhino`; neither says `Pro`, exposes a USB PID, board revision, or
firmware version. Its marketing description also mixes Ace60/Ace68 claims and
inconsistent travel figures, so it cannot identify a firmware target in
advance. It is consistent with an ordinary retail “Ace68” being the observed
`41E4:2116` device, and it is not evidence that the device is a different
physical “Pro” product.

### What the official data does and does not show

M HUB separates the two USB/firmware targets: Ace 68 I is `41E4:2114`, type
`111`, firmware baseline `109`; `41E4:2116` is type `112`, firmware baseline
`121`. Their boot PIDs also differ (`2115` versus `2117`). This establishes a
real firmware/USB-target split.

It does **not** establish two visibly different physical keyboards or two
documented PCB designs. The `2116` Pro layout module is a one-line alias of
the Ace 68 I layout module, and its default-key module is likewise a one-line
alias of the I default-key module. Both records reference the same PDF manual
and PSD template. The current official global store sells `Ace 68` as a family
with Esports and Ultra Esports variants rather than a separate `Ace 68 Pro`
page. Marketplace titles use `Pro`, `Ace68`, and `Ace68 V2` inconsistently.

The supportable conclusion is therefore: they are separate firmware targets
and may be separate PCB revisions, but the available official material does
not prove a board-level difference. Treat `2116` as an Ace68-II firmware
generation, not as proof that the owner's retail keyboard is a distinct Pro
product.

## USB topology

The image is RV32 RISC-V code with compressed instructions. Its embedded USB
device descriptor identifies `VID_41E4 / PID_2116`, BCD device version `1.21`,
and product string `Ace68-II`.

It exposes ordinary HID keyboard reports and a separate 64-byte bidirectional
HID report:

```text
05 01 09 00 A1 01
15 00 26 FF 00 19 00 29 08
95 40 75 08 81 02
19 01 29 08 95 40 75 08 91 02 C0
```

The latter is a generic-desktop/undefined input-output transport, not an HID
axis descriptor: it describes 64 opaque bytes in each direction, with no
logical axis, unit, or per-key analogue collection. It is nevertheless the
only plausible channel for a vendor analogue feature, because it is separate
from normal keyboard reports.

## Recovered configuration parser

The parser around file offset `0x0130A` reads a received 64-byte working
buffer and has two accepted entry markers:

- `55`: framed configuration path;
- `5F`: distinct raw path.

On the `55` path it validates a payload length no greater than `0x38` and an
8-bit sum over the variable payload before dispatching. The static image does
not yet recover the reply layout for parser errors, so `AA`/`AB` must not be
assumed to be Pro reply markers. Thus the Ace 68 Pro does have a concrete
MCHOSE command transport; it should not be probed with arbitrary 64-byte
reports.

The dispatch instruction loads an entry using `opcode * 4` from runtime
address `0x0000FCB0`. The supplied flat update package does *not* contain a
valid pointer table at its corresponding file offset: those bytes decode as
instruction stream / non-pointer values. This can result from an updater
layout, omitted preserved region, or runtime patching. In all cases it means
that the update image alone cannot safely associate a wire opcode with a
handler. In particular, the generic M HUB names `60`, `64`, `65`, `66`, `67`,
`68`, and `FB` are **not proven Pro commands**.

## `A0` is a keyboard-event code, not an analogue command

The large switch around `0x01DC8` is not reached from the recovered vendor
parser. Its values include ordinary HID keyboard usages (`04`, `05`, `14`,
`1D`, and many others), so it handles local key/matrix events. Its `A0` branch
at `0x01F02` first requires an internal mode flag and then sets an internal
byte at `gp-0x78F`; the only other direct access in the image clears that byte.
The branch neither builds a 64-byte response nor submits a USB IN transfer.
It is therefore not evidence for a wire opcode, calibration entry, or
analogue event.

The 64-byte output buffer is at `gp+0x3F8`. A scheduler at `0x02A4E` submits
that buffer on endpoint/report path `2` when `gp-0x747` is pending. The three
self-contained constructors at `0x050CE`, `0x05112`, and `0x0514A` initialise
its first byte respectively to `A1`, `A2`, and `A3` and set that pending flag.
The nearby `A0` literal at `0x05002` is used as a mask/class comparison on
internal records, not written as a USB packet header. A fourth path can set
the pending flag after helper calls, so this alone is not an exhaustive list
of all possible output frames; however it rules out treating the visible `A0`
literal as a Pro analogue report builder.

No immediate `FB`, `66`, or `67` command discriminator was recovered in the
Pro configuration switch, and no `55 FB` packet builder has been established.
This distinguishes the Pro image from the separately analysed IO Type 84
simulation-test protocol and from Ace 68 I's proven `A0` event builder.

## Official M HUB calibration proof

The current official M HUB configuration explicitly binds `41E4:2116` to
`Ace 68 Pro` (firmware `121`). Its model record does not override the device
manufacturer, and M HUB's `getDeviceManufacturer` defaults such records to
the GLW implementation. The factory consequently creates the GLW SDK for this
PID; that SDK declares `calibration: true` and its channel methods are:

```text
startCalibration   -> _simpleSendCommand(0xA8, 0, [0])
endCalibration     -> _simpleSendCommand(0xA9, 0, [0])
reStartCalibration -> _simpleSendCommand(0xB1, 0, [0])
```

M HUB's generic command builder uses `55` as request flag, command byte 1,
byte 2 equal to zero, byte 3 as the 8-bit sum of
`[length, offset-lo, offset-hi, 0, data...]`, then the same header payload.
The non-padded start-calibration prefix is therefore:

```text
55 A8 00 01 01 00 00 00 00
```

with the final zero being its one-byte data value; WebHID/report transport may
pad it to the descriptor's 64-byte output size. The corresponding end and
restart prefixes replace `A8` with `A9` and `B1`; their checksum remains `01`.
Normal M HUB replies are expected to begin `AA` and repeat the command byte.

Most importantly, the same official GLW channel routes an input report whose
first byte is `A0` to an event literally named `calibration`. It does not route
`A0` to an analogue/gameplay event. This proves that `A8/A9/B1` and this `A0`
event belong to the calibration control path for the Pro's official driver.
It does not prove whether normal key reports are suppressed during that mode,
nor does it provide any separate command for a normal-mode Hall stream.

The complete `A0` through `B1` range exposed by that channel has now been
classified from method call sites:

| Command | Official method / meaning |
| --- | --- |
| `A0` | Read key-trigger configuration (up to 1024 bytes, or an 8-byte key record). |
| `A1` | Write travel/trigger configuration. |
| `A2` / `A3` | Read / write DKS key configuration. |
| `A4` / `A5` | Read / write MT key configuration. |
| `A6` / `A7` | Read / write toggle-key configuration. |
| `A8` / `A9` / `B1` | Start / end / restart calibration. |

Thus neither form of `A0` is a normal analogue stream: `55 A0` reads stored
trigger settings, while an input report beginning `A0` is a calibration event.
There is no live/raw-analogue operation in this official command range.

### Per-key polling audit

There is a genuine per-key **configuration** read, which must not be confused
with sensor telemetry. `getKeyTrigger()` calls `55 A0` and its public wrapper
returns the result as `triggerTravel.travelKeys`; it requests either one
8-byte record or the complete 1024-byte profile block. The paired `A1` writes
that same trigger/travel configuration. `A2/A3`, `A4/A5`, and `A6/A7` likewise
read/write fixed DKS, MT, and toggle configuration records (24, 6, and 3 bytes
per key respectively).

The audited public GLW read API consists of device/base/function/key-matrix
configuration, these key-setting blocks, macros/custom data, custom lighting,
and `getRealtimeLightColor` (`DE`, RGB lighting). No method reads current Hall
coordinates, a per-key depth value, or an analogue state. This is a complete
statement about the public M HUB API for the Pro's GLW channel, not a claim
that an unexposed firmware command cannot exist.

## Consequence for HallJoy testing

The current Ace 68 I diagnostic intentionally rejects `41E4:2116`; the
returned tester trace therefore sent no reports to the Pro. It must remain so.
Any Pro diagnostic needs its own explicit allow-list and a command matrix
derived from Pro evidence rather than copied Ace I opcodes.

The highest-value next evidence is an authorised USB capture while the
official M HUB Web Driver is open on this exact `41E4:2116` device. Capturing
host-to-device report 64-byte frames and the matching device replies will
resolve the remaining opcode-to-handler mapping and the `A0` payload layout.
A full flash dump that includes the runtime table would provide the same
resolution without exercising device commands. The current official M HUB
source confirms that this PID is opened through its `usagePage 1 / usage 0`
collection, which also shows why a generic vendor-interface assumption is
unsafe.

## Current conclusion

The Pro firmware and official driver now prove a calibration command suite
(`A8/A9/B1`) and a calibration-labelled `A0` input event, but do **not** prove
a usable normal-operation analogue stream or a command that enables one. The
most promising route is protocol capture from the official driver, not sending
guessed `60`/`64`/`66`/`68` commands to the user.
