# Brand protocol review — 2026-09-21

## Support criterion and outcome

Owner clarified that physical testing of every model is not required for Supported
when the known protocol and implemented route are established. This supersedes
previous yellow wording for the four locally implemented additions. It does not
assert that every device by a brand must work, or that new hardware was tested.

Sheet Main C294/C300/C304: Supported with custom firmware (K6/Q2/Q4 HE ANSI).
C542: Supported (Wooting 80HE+, including ordinary/split ANSI/ISO).
The earlier implementation, checks and local candidate remain unchanged.
These code additions are local; GitHub and the installed v1.6.0 EXE are unchanged.
README and SUPPORTED_HARDWARE now describe the local supported set.

## NuPhy: all remaining five catalog models inspected

Primary manufacturer bundle: https://drive.nuphy.io/static/js/main.23dc78ef.js
Snapshot: docs/research/final-layout-sources/nuphy.js
SHA256: 8b8fe5fdb1c13092bd3f38476688e4435b2ffdfcca552ba4b36d5fe149f27fba

| Sheet row | Model | Manufacturer USB PID | Outcome |
|---|---|---|---|
| 409 | BH65 / BH65 HE in Drive | 6130 | Yellow: known protocol, depth scale unverified |
| 410 | Field75 HE | FE70 | Supported |
| 411 | Field75 HE V2 | 6132 | Yellow: known protocol, depth scale unverified |
| 412 | Halo65 HE | 6112 | Yellow: known protocol, depth scale unverified |
| 413 | WH80 wired | A011 | Yellow: known protocol, depth scale unverified |

All five have VID 19F5, usage page 1 / usage 0 in the manufacturer's device
catalog, matching HallJoy's generic admission. No PID allowlist change is needed.
Modules 69731, 70396, 26068 and 96470 identify the exact models. Module 86736's
factory registers non-mechanical devices on the shared Pt protocol class;
module 97822 export Rx=X delegates to isMechanical, not to model generation.

The shared A0 calibration parser (Mt string index 491, parseCalibrationStatusBuffer)
reads the key identity at bytes 1..3 and travel at bytes 6..7, BE16. HallJoy's
existing UAP path reads key code at 2..3 and value at 4..5, BE16, normalized by
800 (1600 for Air60/Air75). The current manufacturer parser does not establish
the meaning/range of bytes 4..5 on the four additional models. This is a specific
missing compatibility fact, not proof of an incompatible protocol or a broken
keyboard. Do not substitute bytes 6..7 or assume a new scale without evidence.
No calibration, firmware, lighting or device command was sent.

Field75 HE has independent firsthand upstream support evidence: the UAP author
explicitly identifies it as personally tested at
https://github.com/AnalogSense/universal-analog-plugin/issues/1 .
The shipped generic decoder already covers it. Existing Air60/Air75 stay green.
Wireless/dongle WH80 is not established by this wired-device review.
The bundle also mentions Gem80 HE / Halo65 HE Pro outside the present Sheet's
NuPhy rows; no new catalog rows or support claims were created for those names.

Static inspection only: string pools were decoded from literals and checksum
rotation using restricted arithmetic, without executing vendor JavaScript.
Scratch decoded maps: .local/nuphy-reviewed-pools-20260921.json (54 pools).

## Razer: actual boundary, not a failed download

The preceding investigation did not attempt to download Razer firmware.
OpenRazer identity support alone does not prove HallJoy analog report compatibility.
Huntsman V3 Pro 8KHz PID 02CF has identification evidence, but no verified payload
matching the shipped report-7/report-11 decoders was established here.
Other new Razer revisions remain unpromoted; no claim of impossibility.

Tartarus Pro has a concrete existing fork implementation:
https://github.com/DenkiSuki/Soup/commit/2ef6356
It adds PID 0244, report ID 6, a dense fixed array of twenty 8-bit key depths
normalized by 255. HallJoy's admitted Huntsman models use report IDs 7 or 11.
This is a separate decoder/integration, outside the owner's current instruction
to avoid different-protocol work. The fork does not establish Tartarus V2 Pro.
Related primary identity evidence:
https://github.com/openrazer/openrazer/issues/2633
https://github.com/openrazer/openrazer/issues/2793

## Sheet verification and scope

Workbook: https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit
Nine status cells changed. Four NuPhy dropdowns gained the exact status
Known protocol; depth scale unverified, with one narrow conditional rule.
All existing dropdown choices and other rules were retained. Readback confirmed
five green B:C pairs (RGB .65882355/.8666667/.70980394) and four yellow B:C pairs
(RGB 1/.9019608/.6392157), with the intended model names and values.
No runtime edits, hardware tests, visual runs, build or publication this follow-up.
Backups: .local/backups/brand-protocol-review-20260921.
