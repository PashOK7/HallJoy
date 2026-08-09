# HallJoy hardware compatibility

HallJoy supports several analogue-keyboard protocol families. A brand name,
USB VID/PID, or similar product title is never enough by itself: native routes
claim only the exact HID interface that completes their protocol proof.

## Status definitions

- **Physically tested:** HallJoy has direct hardware or tester evidence for the
  listed model and route.
- **Protocol-compatible:** the device can be admitted after a complete,
  read-only capability proof, but that exact model has not necessarily been
  tested by the HallJoy project.
- **Embedded Universal Analog Plugin:** support is declared by HallJoy's pinned
  private UAP/Soup runtime. This is not the same as HallJoy physical validation.
- **Unsupported:** the device or firmware was tested and does not provide a
  release-quality route.

## Physically tested devices

| Device | Identity | Route | Evidence |
|---|---|---|---|
| Aula WIN 60 HE MAX | `1CA2:1902`, `FFA0:0001`, `App V1.1.6 / Feb 4 2026` | Native Aula `5C/12/23/2B` | strict capability proof, real analogue matrices, 10+ rollover, up to 22 simultaneous keys, release-to-zero, about 344 Hz, and three disconnect/reconnect cycles |
| Aula WIN 60 HE (Standard/W669) | `2E3C:C365`, `FF1B:0091`, report ID 1 | Native W669 `0D/18/21` | physical live stream and user confirmation on the SI2825 family; exact firmware-product selection is still required |
| Irok MG75 Max | `1CA6:0529`, usage page `FFB0` | Native SparkLink/XD | real analogue input, sustained polling, held-key unplug/reconnect, repeated startup/shutdown, and no reconnect after stop |
| MADLIONS MAD 68 Pro R | `373B:1109`, `bcdDevice 0102` | Native asynchronous A0 | A9/A8/A9 capability proof, A0 stream, exact 68-position sequence, and physical W/A travel changes |
| SayoDevice O3C | `8089:0009` | Native depth `0x22` | physical O3C depth stream; current keyboard HID assignments are used instead of fixed letters |

Physical evidence for one model does not prove every keyboard sold under the
same brand.

## Native HallJoy protocol families

### Aula WIN 60 HE MAX and compatible 6x21 devices

The MAX-family backend is not tied to one PID or a fixed 61-key assumption. A
candidate must pass all of the following before HallJoy claims its exact path:

- Aula/SparkPlayJoy brand scope or the audited `VID 1CA2` scope;
- `FFA0:0001` and an exact 65-byte HID envelope;
- a structurally valid firmware descriptor;
- positive precision and travel limits;
- a unique dynamic map bounded to 126 positions;
- two identical active Fn0 generations;
- valid travel data for both matrix halves;
- a complete read-only proof of no more than 25 transactions.

The physically tested WIN 60 HE MAX completes the known 17-transaction proof.
A sibling with another PID, firmware build, or key count may be admitted only
after the complete structural proof and remains **protocol-compatible** until
physically tested.

### Aula WIN 60/68 HE Standard and KP-TE153 (W669)

The Standard/W669 family is a different protocol from the MAX family. HallJoy
reads the firmware product through opcode `0D`, selects the matching official
factory profile, applies the complete live `18/80` override generation, and then
uses the `21` event stream.

| Firmware product | Geometry | Validation status |
|---|---|---|
| `SI2825HEARGB`, `SI2825KRT12HEARGB`, `SI2825KR-AHEARGB`, `SI2825KZHEARGB` | WIN60, 61 keys | official profile; physical Standard/W669 stream evidence and separate WIN 60 HE user confirmation |
| `SI2828HEARGB`, `SI2828KZHEARGB` | WIN68, 68 keys | official-driver-derived profile and automated tests; not physically tested by HallJoy |
| `SI2851UKKZHEARGB` | KP-TE153 UK, 69 keys | official-driver-derived profile and automated tests; not physically tested by HallJoy |

An unknown W669 product never inherits a guessed WIN60/WIN68 layout from its
PID, product-name substring, or key count. It must return a sufficiently
explicit map when no exact factory profile is known.

### ATK Hex80-compatible `0x96`

Known Hex80 devices have used PIDs `1176`, `1177`, and `1250`, but acceptance is
not limited to that list. A candidate requires `VID 373B`, `FF60:0061`, matching
report lengths, and valid GET responses for `02 96 24` and `02 96 1C`.

After revalidation HallJoy sends the documented `03 96 19` command once to
leave calibration mode. It never sends `03 96 18`, which enters calibration.
ATK Hex80 is the named supported model; another proved PID is reported as
protocol-compatible rather than physically tested.

### Addressed Analog `09/94/02`

This route supports QBZ75-compatible and other dynamically mapped devices. It
requires `FF60:0061`, reports of at least 64 bytes, a valid checksum, an exact
response for the requested key IDs, no duplicate records, and plausible Hall
values. A complete `09/83/00` map is used directly. The audited QBZ family has a
canonical fallback; an unknown incomplete map is rejected.

### Irok/SparkLink

HallJoy checks appropriate vendor usage pages such as `FFB0` and `FFA0`, report
lengths, device information, and route responses before claiming an interface.
Irok MG75 Max is physically tested. Irok MG75 Pro and other models that complete
the same proof are protocol-compatible.

Not every Irok keyboard uses this protocol. **Irok MG75 v2 was physically tested
and is not supported** because it belongs to another MCU/protocol family.

### SayoDevice depth `0x22`

Discovery is restricted to `VID 8089`. PID `0009` is the audited O3C route. A
different PID from the same vendor is accepted only after a valid read-only
depth response. Other SayoDevice products may therefore work, but O3C is the
model physically tested by HallJoy.

### MADLIONS asynchronous A0

The exact MAD 68 Pro R route uses `373B:1109`, `bcdDevice 0102`, and the audited
vendor-HID envelope. Another PID must prove both compatible MAD68-family
identity and reversible A9 control framing; one plausible ACK alone is not
enough to prove the physical 68-position layout.

## Support through embedded Universal Analog Plugin

HallJoy's pinned private Universal Analog Plugin/Soup runtime declares support
for the following models or families. No system-wide Wooting Analog SDK or UAP
installation is required.

| Brand | Models/families declared by the embedded runtime |
|---|---|
| Razer | Huntsman V2 Analog, Huntsman Mini Analog, Huntsman V3 Pro, Huntsman V3 Pro Mini, Huntsman V3 Pro Tenkeyless |
| Keychron | Q1 HE, Q3 HE, Q5 HE, K2 HE |
| Lemokey | P1 HE ANSI and ISO |
| NuPhy | analogue NuPhy devices, including explicit Air60 HE and Air75 HE decoding |
| DrunkDeer | A75, G60, G65, G75, and compatible family devices |
| MADLIONS | MAD60HE, MAD68HE, MAD68R |
| Wooting | analogue Wooting keyboards through the embedded build with Wooting-device support |

Some models require their official software to expose the analogue interface;
for example, Razer devices may require Razer Synapse.

### Keychron K4 HE ANSI

Keychron K4 HE ANSI (`3434:0E40`, `FF60:0061`) is physically validated and used
daily by the HallJoy author with the custom read-only `A9 31` full-report
firmware. Testing proved immediate sub-actuation input, the full value range,
simultaneous keys, release-to-zero, sustained 181-191 Hz complete snapshots,
and stable long-term HallJoy use.

The **stock K4 firmware is not supported for gaming**. It exposes only the
per-key `A9 30` command, so a previously idle key can wait for a complete 6x19
background scan and appear roughly one second late. Correct values after that
delay are not an acceptable gaming result.

[AnalogSense's full-report firmware page](https://analogsense.org/firmware/)
explains the purpose of patched full-report firmware and provides general QMK
flashing guidance. As of this review, its pre-built image list does not include
K4 HE; do not flash an image intended for another model. Custom firmware always
carries a brick risk, so keep the exact stock rollback image and verify the
keyboard, layout, MCU, and image before flashing.

## What happens with an unlisted keyboard?

1. HallJoy checks native routes in a fixed safe order.
2. A route may claim only the exact HID path that proves its protocol.
3. If no native proof succeeds, the device remains available to the embedded
   Universal Analog Plugin or continues as a normal digital Windows keyboard.
4. HallJoy never claims a random interface merely because its brand, VID/PID,
   report length, or key count looks similar.

If the keyboard is not recognized, contact **`pash.ok`** on Discord. An
open-source reader, public SDK/specification, firmware image, or offline `.exe`
updater is usually enough to begin protocol research. Some firmware exposes no
external analogue data at all; in that case application-only support may be
impossible.
