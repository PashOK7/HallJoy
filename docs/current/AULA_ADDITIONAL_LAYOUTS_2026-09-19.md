> Superseded integration limits: [HERO84 automatic layout, modifiers and Fn](HERO84_INTEGRATION_2026-09-19.md). Historical validation below describes the earlier build.

# Additional AULA layouts, 2026-09-19

Owner requested the remaining HERO84 HE and KP-TE153 presets. No additional protocol was enabled and no digital depth emulation was added.

- HERO84 HE ANSI: 84 physical keys from the pinned official AULA_2829 default WIN layer. Manual selection; experimental backend warning remains. Fn is displayed but its internal assignment is not published by the retained backend.
- KP-TE153 ISO: 69 keys from SI2851UKKZHEARGB. The vendor calls this UK but includes IntlRo. Preserve that assignment rather than substituting a conventional UK map. Exact firmware identity enables the existing W669 automatic selection route; no HID-caption/VID-only inference. This is an AULA-hosted/shared-driver profile, not independent proof of retail branding.

## Source evidence

`tools/prepare_aula_additional_layouts.py --check` reproduces the source-locked reviewed reports without executing downloaded code. Sources and SHA-256 values are included in each report under `docs/research/aula-additional-layout-reports/`.

HERO84: https://heb.aulacn.com/app-B6-MiDbc.js, first default_key_layer WIN keyboard array under AULA_2829. Static parsing explicitly handles JavaScript backtick strings. Position 53 uses such a label and was missing from the old 83-position extraction. The source actually has positions 1..77 plus 95 and 98..103. Corrected the original factory-map document and native polling list. The production publication regression checks position 53 press and release.

KP-TE153: https://hed.aulacn.com/config/keys/SI2851UKKZHEARGB.json and https://www.illumipc.com/config/keys/SI2851UKKZHEARGB.json are byte-identical. All 69 assignments match KpTe153UkFactoryMap. The shared driver's pinned mainIndex8.min.css supplies the UK Enter contour (top 48%, lower right 80% width). Geometry uses absolute-edge rounding and passes compound overlap validation. No generic rectangular Enter approximation.

## Runtime boundaries

A layout is geometry, not a hardware support claim. HERO84 keeps its current live-map policy and observed-range normalization; automatic selection and automatic/manual map-policy parity remain separate work. The source ID list correction is necessary for the apostrophe to be queried. No physical hardware or visual UI test was performed.

Generated layout files were integrated through the existing pipeline with backups. User profiles and old aliases remain intact. Ordinary delivery uses tools/build_release.ps1: compile and validate the staged candidate while HallJoy stays open, then replace only the exact delivery target and reopen it if it was running.

## Validation

PASS: source extraction reproducibility; 2 additional-layout source tests; 19 pipeline tests; 4 catalog-audit tests; production-linked profile/alias round trips, hidden Configuration controls, HERO84 publication (including position 53), and 16 startup recovery scenarios plus repeated startup. Actual compiled catalog: 97 source variants / 79 visible variants.

Ordinary staged Release build and four linked-image self-tests passed. The verified EXE was installed at build/bin/Release/x64/HallJoy.exe and the previously running window restored. Existing compiler conversion/PDB warnings remain; no build errors.

Evidence: .local/aula-additional-profiles.txt; .local/aula-additional-release.txt; C:/Users/PC/AppData/Local/Temp/HJProfileTest-06ccbfec0b3145a09fd8820656978866/layout-catalog-audit.json.

Delivery SHA-256: 1d70936efcb0346f6e63cb2a5192e83fcf5209fe0a6d2cf682ef6a6284cc2929.
