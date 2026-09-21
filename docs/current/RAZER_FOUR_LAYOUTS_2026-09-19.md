# Four additional Razer ANSI layouts — 2026-09-19

> Regional deferral partially superseded by [Razer regional layouts](RAZER_REGIONAL_LAYOUTS_2026-09-19.md): three JIS variants and V3 Pro ISO added.

Owner requested the four remaining Razer layouts. This supersedes the old
pre-release layout-expansion pause in FINAL_LAYOUT_BATCH.md. No new analog
protocol, depth emulation or firmware behavior is added.

## Exact-model sources

Official Razer master guides, diagram on PDF page index 3:
- Huntsman V2 Analog, PID 0x0266 / decimal 614:
  https://dl.razerzone.com/master-guides/RazerSynapse3/HUNTSMANV2ANALOG-00000614-en.pdf
- Huntsman Mini Analog, PID 0x0282 / decimal 642:
  https://dl.razerzone.com/master-guides/RazerSynapse3/HUNTSMANMINIANALOG-00000642-en.pdf
- Huntsman V3 Pro, PID 0x02A6 / decimal 678:
  https://dl.razerzone.com/master-guides/RazerSynapse3/HUNTSMANV3PRO-00000678-en.pdf
- Huntsman V3 Pro Tenkeyless, PID 0x02A7 / decimal 679:
  https://dl.razerzone.com/master-guides/RazerSynapse3/HUNTSMANV3PROTENKEYLESS-00000679-en.pdf

Raw guides: docs/research/razer-layout-sources-20260919.
Reports: docs/research/razer-layout-reports-20260919.
Source hashes are pinned in tools/prepare_razer_legacy_layouts.py and reports;
the catalog independently locks the report bytes. Exact PIDs match the existing
bundled Soup Razer classifier. These are not the newer 8KHz products.

## Geometry and limitations

Reviewed each exact-model diagram and manually transcribed its ANSI key topology
to keycap units, normalized to HallJoy's 46px pitch / 4px gap. This is not a
pixel-perfect case drawing or automatic PDF contour extraction.

- V2 Analog: 104 keyboard keys, full-size navigation and numeric keypad.
- Mini Analog: 61 keys, no physical F-row/navigation/numpad.
- V3 Pro: 104 keyboard keys, full-size navigation and numeric keypad.
- V3 Pro Tenkeyless: 84 keyboard keys. Print Screen, Scroll Lock and Pause are
  Fn legends on F6/F7/F8, not three additional physical keys. No numeric keypad.
- All four use the documented right Alt / Fn / Menu / right Ctrl bottom row.
  Fn uses HallJoy's existing 0x409 identity; no ordinary HID is fabricated for it.
- Separate media/macro controls and dials are omitted because no analog depth
  identity is established for them. Their placement is not repurposed as keys.
- ISO/JIS remain pending exact regional source evidence; ANSI is not substituted.
- Presets are manual-selection only. Model PID alone does not establish region;
  no guessed automatic identity or firmware remap readback is added.
- Existing Razer analog decoder is unchanged. No new hardware validation.

## Reproduction and checks

python tools/prepare_razer_legacy_layouts.py --check
python tools/layout_pipeline.py check Razer
python -m unittest discover -s tools/tests -p test_layout_pipeline.py

Generator requires only the standard library and existing project modules.
19 layout pipeline tests pass, including all seven Razer variants, exact-model
counts, TKL's absent physical keys, tall/wide numpad keys and no guessed
automatic selection. All contours/usages are validated; no overlap or duplicate
HID is accepted. Runtime uses generated built-in geometry; PDFs are research
inputs and are not parsed or shipped inside HallJoy.exe.

Backup: .local/backups/before-razer-four-layouts.zip and the pipeline's
.local/backups/layout-integrate-e_0ng6s_.
Owner assesses HallJoy appearance. Agent inspected only manufacturer diagrams,
not the application UI. Final build evidence follows.


## Final delivery

MSVC Release PASS (existing ViGEm missing-PDB warning only), full native static
audit PASS, production-linked profile/layout-picker/persistence suite PASS,
16 startup recovery scenarios and repeated startup PASS. Exact delivery EXE
passes Shark, MINI60, NA87 and embedded installer self-tests without forced logs.
All four new preset names are present in the binary.

Delivery: build/bin/Release/x64/HallJoy.exe, version 1.5.4.0.
SHA256: 27b79ab0fb9eb7fa6ff770ab9cd662ec646d8e8b790e34c3d13c996ebcd8a308
Size: 9303040 bytes.
Evidence: .local/razer-four-build.txt, .local/razer-four-static.txt,
.local/razer-four-profile-tests.txt, .local/razer-four-verification.json.
No GitHub publication and no hardware verification claimed.
