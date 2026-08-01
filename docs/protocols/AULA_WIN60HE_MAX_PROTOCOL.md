# Aula WIN 60 HE MAX native analogue protocol

Status: `firmware-proven / implementation-tested / hardware-unvalidated`.

HallJoy supports only the exact read-only protocol described here. No physical
Aula keyboard was available during integration, so this document does not claim
real-device timing, hotplug or firmware coexistence.

## Proven identity

- USB VID/PID: `1CA2:1902`.
- HID usage page/usage: `FFA0:0001`.
- protocol reports: 64 bytes; Win32 HID envelope: exactly 65 bytes with report
  ID byte zero.
- accepted firmware identity: `App V1.1.6`, build `Feb  4 2026`.
- accepted precision/minimum/maximum travel: `10 / 10 / 3400` micrometres.
- matrix: 6 rows by 21 columns; 61 physical non-zero positions and 60
  publishable default HID keyboard usages.

## Frame and response contract

The 64-byte protocol report begins with:

```text
5C <payload-length> <command> <checksum> <payload...>
```

The response command is `request-command | 80`. Checksum is the low byte of
`35 + 5C + payload-length + command + last-payload-byte`. Length, response
command, selector/row echoes, continuation count and checksum must all match.

The production capability proof is exactly 17 read-only transactions:

1. sync and exact 54-byte firmware identity;
2. precision/stroke parameters;
3. three default-map row-pair reads;
4. two complete active Fn0 generations, each five 14-record reads;
5. initial travel half 1 and half 2.

Every command starts with a successful input-queue flush on one exclusive
read/write handle. The protocol has no transaction ID and travel responses have
no half ID. Any flush, write, timeout, continuation, framing, correlation or
decode uncertainty permanently poisons that client/session; HallJoy closes it
before retrying.

## Active map

Command `23` reads fourteen four-byte key-function records. One complete Fn0
map requires five packets. The final short request repeats its final real key as
padding, and every returned key/layout field is correlated with the request.
Two complete generations must be identical before publication.

Function values remain 16-bit through decoding. Only keyboard functions
`0004..00E7` are published; values such as the physical Fn marker `F001`, macro
codes and vendor/internal functions are never truncated into HID usages. Only
Fn0/base-layer publication is implemented.

## Travel matrix

Requests are issued strictly in order:

```text
5C 04 12 A6 02 01 FF FF
5C 04 12 A6 02 02 FF FF
```

Each response contains `00 02` followed by 63 little-endian `uint16` values:
128 payload bytes, 132 frame bytes, carried by three HID reports. The two halves
form the 126-value 6x21 matrix. Values on physical positions are range-checked,
then normalized to `0..1000` with rounding against the proven 3400 micrometre
maximum.

## HallJoy ownership and lifecycle

Candidate selection requires the complete fingerprint above. A foreign exact
interface-path claim is rejected before any metadata open. HallJoy then opens
the command interface exclusively, re-correlates the SetupAPI instance/path,
re-reads capabilities through that handle, runs the full proof, and only then
claims that exact path before UAP starts. Coarse VID/PID reservation is forbidden.

Multiple matching candidates fail closed. The proven path, instance identity
and meaningful firmware serial evidence are retained for reconnect. Worker
shutdown cancels pending I/O, waits at most three seconds and retains resources
if completion cannot be confirmed. `TerminateThread` is not used.

## Evidence provenance

- received archive SHA-256:
  `E695AFDCF8C41982101B4C10BFA0C9C0DB52F9661AC2AEA723871554D0C632D4`;
- verified firmware SHA-256:
  `B6E87B28E9249EDA27FF251D3F793AC1D80F75180547E6C9166331F741F33381`;
- archived oracle output SHA-256:
  `85C70BFAABE599F65A7EECBB5E6566D1B95679DB41520AAAEA8AF0566E7EFDC4`;
- independently fetched exact oracle packages:
  `@sparklinkplayjoy/protocol-keyboard@1.0.6`,
  `@sparklinkplayjoy/sdk-keyboard@1.0.14`,
  `@sparklinkplayjoy/hid@1.0.9`;
- ten oracle source files matched the independently downloaded packages by
  SHA-256, the official oracle reproduced byte-identically, and the firmware
  verifier passed all 57 checks.

The archive itself explicitly reports `physicalHardwareValidation=false` and
`realWindowsMsvcValidation=false`. HallJoy adds native MSVC, GCC/portable,
ASan+UBSan, parser/oracle/end-to-end/session-policy and integration gates, but
keeps the physical Aula hardware gate open.
