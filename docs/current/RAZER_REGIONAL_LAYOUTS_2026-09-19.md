# Razer regional layouts — 2026-09-19

Owner requested ISO/JIS presets for already-supported Razer analog keyboards.
This change adds geometry only; no protocol or analog scaling change.

## Delivered variants

| Model | Region | Keys | Exact source |
|---|---|---|---|
| Huntsman V2 Analog | JIS | 108 | GRAPHT/MSY RZ03-03610900-R3J1 |
| Huntsman V3 Pro | JIS | 108 | GRAPHT/MSY RZ03-04971300-R3J1 |
| Huntsman V3 Pro Tenkeyless | JIS | 88 | GRAPHT/MSY RZ03-04981300-R3J1 |
| Huntsman V3 Pro | ISO | 105 | ComputerBase photographed German test unit, 2024-07-24 |

JIS V2 Analog and V3 Pro share one visible group, retaining both source names.
Original ANSI variants are unchanged. Runtime catalog audit: 101 source variants,
82 visible variants. Only same-brand identical geometry is merged.

All new variants are manually selected: a model USB PID does not identify the
regional physical arrangement. English physical HID labels are used; this is not
an OS locale/keycap translation table. No guessed regional automatic match.

## Sources and transcription

- https://store.grapht.tokyo/products/rz-n0-00082
- https://store.grapht.tokyo/products/rz-00400-n0
- https://store.grapht.tokyo/products/rz-00402-n0
- https://www.atpress.ne.jp/news/373907 identifies MSY as Razer's Japanese distributor.
- https://www.computerbase.de/artikel/tastaturen/razer-huntsman-v3-pro-test.88727/

Exact product JSON, photos and existing English master guides are pinned by SHA256
in tools/prepare_razer_regional_layouts.py and its generated reports. Raw inputs:
docs/research/razer-layout-sources-20260919. Reports:
docs/research/razer-layout-reports-20260919. Catalog locks report hashes separately.
Images/PDFs are research inputs, not executable resources or startup work.

JIS: 1u Backspace, Yen/Ro/Muhenkan/Henkan/Kana, 3.5u Space, no separate Menu;
compound Enter spans two rows. ISO: short left Shift, physical Non-US key,
compound Enter, standard bottom row. Geometry is normalized to 46px pitch / 4px
gap, not a pixel-exact photograph. TKL has no physical Print Screen, Scroll Lock,
Pause or numpad; media controls have no fabricated analog channels.

## Source discrepancies and remaining work

The localized Japanese/German master guides still show ANSI on diagram page 3.
Their language is not evidence of regional geometry. The LDLC French V3 Pro
listing's LD0006097031.jpg also depicts ANSI (local v3pro-iso.jpg is rejected,
despite its initial download filename). Neither source is used for regional rows.

ISO Mini Analog, V2 Analog and V3 Pro Tenkeyless remain pending verified regional
geometry. Regional products exist, but attempted exact review/product photographs
were unavailable (timeouts/403), or advertised region did not match the image.
No assertion of unsupported analog is made. Mini Analog JIS product existence
has not been established, so no JIS variant was invented for that model.

## Validation

- Regional generator --check and layout_pipeline.py check Razer: PASS.
- 20 layout pipeline tests: PASS; counts, international keys, compound contours,
  unique HID identities, no overlap, outer sections and manual-only identities.
- Production-linked profile/layout tests: PASS, including alias round trips,
  same-brand duplicate audit and hidden-control regression.
- Recovery startup/preservation checks: PASS.
- Evidence: .local/razer-regional-profile-tests.txt.
- Backup: .local/backups/layout-integrate-o2qsq3us plus content-hash backups of
  catalog/merge/test files in .local/backups/mg75-pro-native.

Agent inspected source photographs only. Owner evaluates the application UI;
no physical Razer keyboard test is claimed. Final EXE verification follows.

## Final delivery

Ordinary release build and all four candidate self-checks passed. Installed through
tools/build_release.ps1 after candidate validation; no GitHub publication.
Build evidence: .local/razer-regional-release-build.txt.

EXE: build/bin/Release/x64/HallJoy.exe
Size: 9371136 bytes
SHA256: 9cef70dab8254d9e1ce12193e83a907f4e13422fbcaf332ad9acc88219afc950
