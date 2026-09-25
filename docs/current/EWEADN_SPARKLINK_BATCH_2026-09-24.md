# EWEADN SparkLink batch — local, 2026-09-24

Owner direction: continue large real protocol integrations, experimental notices
without a mandatory physical tester. No GitHub publication authorized here.

## Result

17 additional experimental model/variant labels, 21 exact USB identities:
DEEP68 Pro HE; DEEP80 Max HE (magnetic version); DK63 HE; DK63 Star HE;
DK68 HE; DK68 Pro HE; DK68 Star HE; DK68 V2 HE; DK75 HE; DK75 E HE;
DK75 Pro HE; DK80 HE; ES68; ES68 EVO (including Full revision); ES68 Lite;
X87HE; Gamma75 HE (EXX collaboration, two reviewed PID revisions).

All use the already-enabled native SparkLink V2 device/layout/travel path with
ordinary HallJoy bindings and virtual gamepad output. Exact identity tokens and
runtime notices added. Shared notice title no longer incorrectly says IROK for
other manufacturers. No new protocol toggle, calibration write, forced logging,
or keyboard-specific runtime test added. Manual visual layout selection can be
needed; current base-layer assignments are obtained from the keyboard.

Official E HUB3.3.2 source review establishes HID pageFFB0/usage1,64-byte packets,
reportID0, keyboard type1 at device-info byte2, dynamic layout03/01 and travel
04/03/01 row commands. Source files/URLs/hashes and all catalog records are pinned
in [research evidence](../research/eweadn-sparklink/README.md). Source review is
not hardware testing or firmware emulation. This resolves the DK68 lead retained
in RONGYUAN_BATCH_7_2026-09-24.md without extending support to RongYuan namesakes.

Travel units are micrometres. Existing3.5mm initial observed-upward normalization
remains provisional for switch variants; this is a specific experimental range
uncertainty. No claim of measured endpoint accuracy or latency.

Held: Alpha87 magnetic/mechanical name ambiguity; PID1C2D X75/Gamma75 entries;
shared ES68/EVO boot identity; non-xingshan HUB families; all wireless transports.
These holds are research scope decisions, not proof that integration is impossible.

## Validation and build

- Pinned-source/protocol/identity/notice check PASS:
  `python tools/check_eweadn_sparklink_sources.py`.
- Ordinary Release and all six linked-image gates PASS. Diagnostic switches off.
- EXE `build/bin/Release/x64/HallJoy.exe`, SHA256
  `3551d1242e2b30a67d4f577a106ef931afff328457caae666773a98c57b79b93`.
- Full native backend regression PASS: all static audits and portable C++ tests,
  exit0. Log `.local/eweadn-native-checks-20260924.log`.
- Before-change backup `.local/backups/before-eweadn-sparklink-20260924.zip`.

## Live Sheet

[HallJoy supported keyboards](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit), Main, sheetId0.
12 new rows; five existing gray rows become yellow: DEEP80 Max, DK63, DK68,
Gamma75 and X87HE. One reviewed123-request batch after fresh unchanged-snapshot
comparison. Status dropdown already contains `Implemented; awaiting hardware testing`.
No comments/notes added. Native planner materialized base brand/status colors.

Independent full readback:644 model rows,245 yellow,63 green,332 gray,4 red;
146 brand blocks, grid1271rows. All245 yellow model pairs match runtime catalog.
Structure and base/effective presentation audits PASS, zero issues. Expected
model/status list exactly matches; prior row heights, validation, non-color and
non-border formatting, notes, column widths and conditional-rule semantics
preserved. Borders/colors adjusted only as required by the insertion/status plan.
This is API verification, not a claim of owner visual acceptance.

Snapshots `.local/eweadn-native-before-20260924.packed.json`,
`.local/eweadn-native-after-20260924.packed.json`; plan
`.local/eweadn-sheet-plan.json`; all-yellow readback
`.local/eweadn-sheet-statuses-20260924.json`. Superseded
`eweadn-sheet-before-20260924.packed.json` has a different packing shape; use the
`eweadn-native-*` snapshots with the maintained tools.

README, hardware documentation and next-release patch notes synchronized locally.
No GitHub push, publication or physical testing performed.
