# Lemokey layout batch — 2026-09-09

Owner scope: UAP-declared models are included; ship one brand, then stop for
owner testing. This batch adds only **P1 HE ANSI and ISO**. JIS and other Lemokey
models are excluded because the pinned UAP does not declare them.

Official geometry (downloaded without opening HID devices):

- https://launcher.keychron.com/static/device/908920336/json/v3.json
- https://launcher.keychron.com/static/device/908920337/json/v3.json

Raw files are in `docs/research/lemokey-layout-sources`. Physical key identities
come from the shipped `layout_lemokey_p1_he_ansi/iso` matrices in
`third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/AnalogueKeyboard.cpp`.
The generator records source SHA256 values in each review JSON. This is not
QMK factory-keymap extraction: it intentionally matches what HallJoy receives.

`tools/build_lemokey_layouts.py` adapts those matrices into the existing strict
layout converter. It checks exact VID/PID, complete geometry/matrix set equality,
6x15 dimensions, known physical tokens, unique HIDs and actual contour overlap.
Unknown variants/actions fail closed. ISO Enter retains its compound geometry.
Fn is 0x409; the lower-right system key is UAP RMETA, not guessed RALT. The one
encoder position (0,14) is explicitly excluded because UAP maps it to KEY_NONE.
Result: 81 ANSI keys and 82 ISO keys, no synthetic analogue encoder.

Both are built-ins under Brand=Lemokey, shared by main preview, editor and Input
Overlay. Source ISO's generic title is normalized to P1 HE ISO. Exact first-run
selection accepts only 362D:0610/0611, FF60:0061, 6x15, within the unchanged
one-device/one-shot policy. Saved/manual selections are not overwritten.
No protocol, firmware, profile or user-layout file is changed.

Reproduce: `py tools/build_lemokey_layouts.py --check`.
Tests: `test_lemokey_layouts.py`, existing `test_layout_import.py`, portable
`layout_editor_model_test.cpp`, production private-desktop layout picker events.
Backup: `.local/backups/lemokey-layouts-20260909/`.

Owner check: Brand **Lemokey**, Model **P1 HE ANSI / P1 HE ISO**. Check main
preview and Input Overlay independently, then the ISO Enter in Layout editor.
Stop after delivering this brand; do not start the next brand until owner reply.

Validation PASS: 3 Lemokey tests, 16 importer tests, unchanged generated Keychron
catalog, portable layout/identity tests, all static audits, native/simulator
builds (existing ViGEm PDB warning only), full Windows profile/events suite at
`%TEMP%/HJProfileTest-422a621c99e74d7bb332825ed00f0f5e`.
Release SHA256: `CFF7D1CB9962F91FA68508E6ED05824C3932D128D560DBB9769BD6F31F8B44EE`.
