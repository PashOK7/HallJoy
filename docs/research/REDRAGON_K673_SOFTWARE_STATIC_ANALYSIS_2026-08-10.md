# Redragon K673 software static analysis

Date: 2026-08-10

## Scope and safety boundary

This record covers a static, read-only analysis of the supplied Redragon
K673RGB-M software package. Neither the vendor installer nor any firmware
upgrade executable was run. No HID device was opened and no firmware or device
configuration was written.

Supplied archive:

- file: `K673RGB-M - Software.zip`;
- SHA-256: `662DF246CC09705537FD0A43CC991841A85A6D6296739EFB22E754B650F0CEEA`;
- payload: one Inno Setup installer, product version `V2.06.01`;
- installer SHA-256:
  `58FD83B3C65A81BA9A4E0F881339B7706C5F95EFC5316D061B2E2FE423F5A15B`;
- Authenticode status: valid, signed by Ruilian Micro Technology (Shenzhen)
  Co., Ltd.

## Extraction result

The Inno payload contains the Qt application, `witmodSdk.dll`,
`jl_firmware_upgrade_x86.dll`, and `UpgradeAppTool.exe`. Its installed
`configDir` and `UpdateFirmware` directories are empty.

The application's compiled Qt resource collection was recovered from its
`qRegisterResourceData` registration and extracted without executing the
application. It contains 569 files, including eight firmware images for other
models and two exact K673 keyboard-layout profiles:

- `KeyInfo_7272BRHEXYXK673JCARGB.config`, SHA-256
  `B44E9F06E49E41AFDCCF3545DDB6BBEC86D2F34095348DBCAE8C8AEBA88E1DF4`;
- `KeyInfo_7272USHEXYXK673JCARGB.config`, SHA-256
  `3E139E7D472E9BE2833002BDFF99B386B9FD045A959D1D6FDD8E45DC72BE0DE0`.

Both profiles describe an 82-key keyboard with `hidDeviceType` 4. The BR/PT
and US files differ in physical key shapes and labels, not in protocol
evidence.

No K673 firmware image is present. The embedded firmware files are named for
other products and must not be treated as compatible merely because some use
the same controller family.

## Update-channel cross-check

The application exposes a vendor update channel. The newest available signed
package was `V2.06.08` (SHA-256
`F410E0CB3EB66BB803EB092400B77C91B477B8B8463B308198A00602D4A97FCA`).
It was also unpacked statically. Its embedded firmware set is byte-identical to
the relevant set in `V2.06.01` and does not contain a K673 image.

The current firmware directory contains only an unrelated product image. The
vendor firmware-upload history lists several M484 products and one W669
product, but no K673 or `7272...K673...` firmware entry. Therefore a K673
firmware blob cannot be recovered from the supplied package or its currently
referenced update channel.

## Official iLLumiPC WebHID cross-check

The keyboard is also supported by the public iLLumiPC WebHID configurator. A
static analysis of its 2026-08-10 live device catalog and JavaScript corrects
the desktop SDK's misleading generic M484 association. The exact K673 entries
use:

- USB VID/PID `2E3C:C365`;
- vendor HID usage page/usage `FF1B:0091`;
- WebHID report ID 1 with a 63-byte payload;
- read-only opcode `0D` to obtain the firmware product string;
- opcode `18` for the 132-position key-map generation;
- opcode `21` subcommands `02`, `03`, and `04` for live subscription,
  unsubscribe, and travel metadata.

This is the same transport and command family as HallJoy's Standard/W669
backend, not the generic `0416:7372` M484 route suggested by the bundled
desktop SDK support table. The generic M484 entries remain useful family
hints, but they are not the identity of the exact K673RGB-M products.

The live catalog has three exact entries on `2E3C:C365`:

| Marketing name | Firmware product | Official key profile SHA-256 | Published HID keys |
|---|---|---|---:|
| `K673RGB-M` | `7272BRHEXYXK673JCARGB` | `E2ED942977639D2C925C083F186F8285045C212238BD54CA3DFA462B388E85B5` | 81 |
| `K673RGB-M` | `7272UKHEXYXBJCARGB` | `95CAECC35B88F7D5BB489FF00B966D5A027E525450898FB7066B8DC4C72E12A7` | 81 |
| `K673WB-RGB-M` | `7272USHEXYXK673JCARGB` | `3793C15E57E7B2F70F158EEFB746D85DA4D9A3CA04D625100B6888A3B64E06CD` | 80 |

Each profile assigns exact USB HID usages to firmware matrix indices in the
same 6-by-22, row-major space used by W669 live events. The BR profile contains
83 visual records: 81 publishable keyboard usages plus two non-keyboard or
internal controls. The UK profile differs at the ISO-enter/backslash and right
shift positions. The US profile has the corresponding ANSI geometry. These
differences make exact firmware-product selection mandatory; PID or key count
alone is not safe.

Evidence hashes:

- live `device.json`: `C5446151F55EB593C26DD61FC888CF682A09251BE5953807D60704EE19C99008`;
- live `firmware.json`: `451E0FEAADF917D9438B8F8851303355952647F97922477ECD9D7945E3DB63E3`.

The official WebHID client independently confirms HallJoy's existing live
event decoder: an opcode-`21`, subtype-`01` report contains row, column, and a
little-endian travel value. It also builds the same 22-byte column mask for
the `21/02` live subscription. This is sufficient static evidence to implement
an exact K673 W669 profile without guessing the wire protocol.

The public `firmware.json` does not contain any of the three K673 product
strings. Therefore the WebHID site supplies authoritative protocol and layout
evidence, but still does not expose a downloadable K673 firmware image.

## Decision

Do not flash any of the recovered firmware files to a K673. Add K673 through
the existing Standard/W669 architecture, with a separate exact factory map for
each firmware product above. Admission must retain the complete read-only W669
proof: exact HID shape, opcode-`0D` product identity, travel descriptor, and a
complete 132-position override generation before claiming the interface.

The static evidence was sufficient for a bounded implementation and automated
protocol tests. A subsequent physical diagnostic run, recorded below, proved
the exact BR identity, live analogue travel, full scale, release-to-zero, and
multi-key operation without flashing or writing keyboard configuration.

## Physical validation

The returned `HallJoy (9).log`, SHA-256
`B2D76566F0BCD93B4997081400DE6539694A0814F32415C8BCD3C2E2F7A72A94`,
physically identifies `K673RGB-M` firmware
`W669,34,KB,FR,7272BRHEXYXK673JCARGB,V3.18.01`. HallJoy selected
`redragon_k673_br_81`, recovered all ten map fragments, proved 81 publishable
keys and a processed travel range of `0..340`, and subscribed through the
shared non-exclusive W669 session.

During the 12.234-second session HallJoy decoded and published 1,491 real
`21/01` live events. Seventeen different HID usages produced positive travel,
with 72 zero-to-positive edges matched by exactly 72 release-to-zero edges.
The smallest positive raw value was 7 (about 2.06% of full travel), the maximum
was 340, peak simultaneous activity was four keys, and every observed HID usage
ended at zero. The worker exited without a protocol fault and all application
workers joined cleanly.

The observed 27-231 live events per second are change-event traffic generated
by the tester's motion, not a scan-rate or USB-polling measurement. Read-only
poll query `21/0A` returned code zero, meaning firmware default with no stated
nominal rate. This run therefore proves prompt continuous analogue delivery but
does not establish a numeric 1,000 Hz claim. It also cannot prove a ten-key
hold because the tester pressed at most four keys simultaneously.

The original diagnostic counted the expected `CancelIoEx` completion during
shutdown as one failed read. Packet chronology and `fault_kind=0` prove that it
was not a transport failure. HallJoy now classifies `ERROR_OPERATION_ABORTED`
or `ERROR_INVALID_HANDLE` as informational only when stop is already requested;
the static audit prevents this shutdown-accounting regression.
