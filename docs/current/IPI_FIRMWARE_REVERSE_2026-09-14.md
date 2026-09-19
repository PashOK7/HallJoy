# IPI firmware compatibility reverse — 2026-09-14

> 2026-09-14 implementation update: [IPI native support](IPI_NATIVE_SUPPORT_2026-09-14.md).
> Exact UUID profiles, complete live maps, device calibration, Fn and alias
> publication now replace the historical IPI fallback. Eight models/four layouts.
> Earlier layout-only/no-EXE statements below describe the preceding step.
> Physical USB tests and AURORA65W receiver forwarding remain unverified.

Owner requested firmware downloads and reverse engineering after questioning
whether the eight added IPI layouts correspond to supported analog devices.
This authorizes offline protocol investigation; the earlier layout-only scope
is superseded for this task. No device was connected, flashed or configured.
Updater EXEs were parsed as archives, never launched. HallJoy.exe is unchanged.

## Result

All eight exact models have a firmware implementation of addressed live-value
reading 09/94/02, and their firmware physical-ID tables exactly match the added
layout ID sets. This is now firmware evidence, not inference from a common web
configurator or common USB ID. Eighteen package images across eight UUIDs were
examined (17 distinct firmware SHA256 values). Their same command family does
not establish identical hardware, switch calibration, or wireless forwarding.

| Model | UUID suffix | Examined versions | Physical IDs |
|---|---|---|---:|
| flash68 | 0006 | 1.07, 1.08, 1.14, 1.16, 1.18 | 68 |
| AURORA65 | 0010 | 1.13, two packages | 67 |
| Aurora75 | 0013 | 1.08, 1.11, 1.22, 1.24 | 82 |
| RAIN65 | 001f | 1.05, 1.06 | 67 |
| QBZ65 | 0023 | 1.08, 1.20 | 67 |
| QBZ75 | 002c | 1.13 | 82 |
| Aurora75 PRO | 0040 | 1.10 | 82 |
| AURORA65W | 0003, prefix 1200 | 1.19 | 67 |

AURORA65W's image contains the keyboard's addressed handler. A separate receiver
image/USB-radio forwarding implementation was not supplied in its package;
wireless end-to-end compatibility is not established.

## Acquisition and provenance

Official app sources are cached in remaining-layout-sources-20260914. The
keyboard-session module calls https://hubx.pro/api/v1/firmware/fetch with
`device_uuid=0x` plus 12 hex digits, `type=EXE`, `current_version=0x0000`.
The unit helper explicitly adds the 0x prefix. Initial no-prefix exploratory
requests returned empty lists; those are not evidence that files are absent.
The exact official request succeeds for seven UUIDs; QBZ75's list is empty.
Raw official-request.json files preserve all responses.

Four packages came from https://keebforce.com/software-tools/ and fourteen
unique package objects from the manufacturer's returned file URLs. All fourteen
vendor MD5 object checks match. SHA256 is recorded independently. No account or
private API was used. Downloads are research inputs, not tester deliverables.

Source discrepancies preserved here:

- Keebforce's Aurora65 link points to G75 V1.22. Both config.pass and KB header
  identify UUID 110000000013 (Aurora75), not 110000000010. It is not Aurora65
  evidence. A genuine Aurora65 image was separately obtained from the vendor.
- The QBZ75 ZIP filename ends V1.132; internal updater/config version is 0113.
- RAIN65 API V1.06 has a filename mentioning TK52Y/V1.24, but both internal
  config and firmware KB header correctly identify RAIN65 and version 0106.
- Two Aurora65 V1.13 package hashes differ but their firmware image hashes match.
- The manufacturer's QBZ65/Aurora75 lists are older than the downloaded reseller
  packages; no assumption of global latest version is made.

Every extracted image's KB header UUID agrees with its internal config.pass;
manufacturer API UUIDs also agree. See image-identities.json, downloads.json,
vendor-downloads.json and extracted-images.json in the research directory.

## Code evidence and execution checks

Images have a KB container header, ARM Thumb application vector at file offset
0x8000, and application reset addresses at 0x080101a9 (0x080101ad for 65W).
File-to-address base is 0x08008000. The offline audit follows either the older
comparison/branch dispatcher or newer TBH dispatcher to command94, then TBB
subcommand2. It follows actual key-ID lookup tables and the sample helper.

For every image:

- The 108-slot firmware matrix has exactly the nonzero physical-ID set of the
  corresponding official layout. All compared missing/extra sets are empty.
- Subcommand2 takes the requested 16-bit IDs, looks them up, translates to matrix
  locations, and emits six-byte records: ID big-endian, raw/status big-endian,
  secondary sample big-endian. Raw uses bit15 as a status flag.
- Sample readers read per-key live RAM; they do not merely return the most recent
  key. The raw accumulator is divided by 128 in older branches or 192 in others.
  Different averaging divisors/structure offsets alone do not prove different
  physical raw calibration ranges. They do prove these are not identical images.

108 execution cases run the extracted ID lookup and response serializer with
synthetic samples, for 1/4/9 keys and official/HallJoy request byte layouts.
Another108 cases execute the actual sample/raw helpers with synthetic RAM,
covering raw1800/8400/10112, status0/1 and a distinct secondary value.
These are bounded offline emulator tests, not hardware, scanner or USB tests.

The initial suspected endian incompatibility was rejected by these tests:
firmware payload starts at byte7 with a big-endian ID; HallJoy treats byte7 as
zero high length and starts a little-endian low ID at byte8. With IDs<256, the
resulting consumed byte sequences agree. Response low ID/raw positions also
agree. Do not change byte order based on field naming alone.

## Confirmed remaining HallJoy gaps

1. ProbeCandidate sends empty 09/83/00 requests. In the four recent reseller
   images (QBZ65, QBZ75 and Aurora75 versions1.22/1.24), this read handler copies
   a zero-length ID list, skips its map loop and reaches response preparation
   with zero payload length. It is not a request for a full map. Static traces
   and empty-map-emulation.json record the exact addresses. The normal map read
   needs an explicit physical-ID list; the complete current helper must be
   reviewed before implementing a corrected map acquisition.
2. Canonical fallback is an82-entry QBZ75-oriented table with missing assignments
   and incorrect factory Left Alt identity. It cannot represent67/68-key layouts
   fully, even though their addressed analog handlers exist.
3. BuildProfile enables captured W/A/S/D raw endpoints solely by shared372E:105C.
   These captures cannot certify calibration for all eight models/versions. A
   model-aware policy or per-key measurement is still needed before claiming
   correct normalized depth everywhere.
4. Fn is a physical ID in firmware but HallJoy's existing published HID mapping
   and value storage do not automatically provide a usable analog Fn channel.

These findings are also referenced next to the relevant source sites. No
runtime patch or replacement executable was made during this reverse task.

## Reproduce and continue

python tools/audit_ipi_firmware.py
python tools/test_ipi_firmware_records.py

Dependencies: existing capstone; Unicorn2.1.4 is local to .local/ipi-reverse-deps.
All image hashes are pinned by audit_ipi_firmware.py. Reports include addresses,
handler disassembly, key sets and helper code for each image. Emulator source
states which inputs are synthetic. Artifacts are under
`docs/research/ipi-firmware-20260914/`.

Next implementation step: exact UUID-aware physical ID catalog, explicit-list
map acquisition and calibration policy, preserving the existing proved route
and testing the byte-compatible frame interpretation. These are implementation
work after this investigation, not findings of hardware success.
