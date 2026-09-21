# Wooting extended layouts and physical split-key depth — 2026-09-19

> Follow-up: [ordinary v2 merged with 60HE/60HE+](HIDDEN_CONTROLS_AND_WOOTING_MERGE_2026-09-19.md); the five source reports remain for provenance, but ordinary v2 no longer creates redundant picker entries.

Owner requested the remaining Wooting layouts and explicitly selected separate
physical-key analog accounting for split spacebars. This supersedes their prior
bounded-batch exclusion in FINAL_LAYOUT_BATCH.md.

## Sources and presets

Pinned official Wootility v5 bundle: docs/research/final-layout-sources/wootility.js
(original URL https://wootility.io/assets/index-BaoALhfv.js), SHA256
987ba72a09a78b1fc508c8739b1c0a25a3434d7ef614fba56f1822e5acdbc40a.
Manufacturer references: https://wooting.io/wooting-60he-v2 and
https://wooting.io/quickstart/keyboard/wooting-uwu.

The extractor decodes the string table as data, checks its rotation checksum,
parses literal geometry and usage declarations, and never executes downloaded
JavaScript. Geometry uses S5 plus Sg (qS) for exact PID 0x1340, rows 1..5 and
14 columns, including explicit ANSI/ISO split-to-regional fallback. ISO Enter
retains its compound contour. HID codes come from the vendor usage declarations.

Five additional presets:
- 60HE v2 ANSI: 61 keys.
- 60HE v2 ISO: 62 keys.
- 60HE v2 Split ANSI: 63 keys.
- 60HE v2 Split ISO: 64 keys.
- UwU + UwU RGB ANSI: common three-key Z/X/C geometry (also Stealth variants).
  ANSI here describes the factory letter identities; the keypad has no regional
  Enter geometry. Three auxiliary silicone buttons are digital-only and omitted
  from analog key geometry. The official guide confirms their digital nature.

No 60HE v2 JIS variant is invented: the vendor catalog offers exactly these four
regional/split combinations. Manual selection only; the existing Wooting session
telemetry does not prove the physical region or split variant. Existing layouts
and remap policy remain. No new firmware/calibration commands are sent.

## Physical split channels

The factory split layout has two Space keys and two Fn keys; old Soup discarded
matrix position and max-merged identical scancodes. The existing V2 USB records
already contain a matrix coordinate and independent 10-bit depth. For exact
31E3:1340 only, preserve four positions in additional Soup OEM7..10 channels:

| Matrix | HallJoy code | Label |
| --- | --- | --- |
| 5,4 | 0x480 | Space L |
| 5,8 | 0x481 | Space R |
| 5,6 | 0x482 | Fn C |
| 5,13 | 0x483 | Fn R |

Codes are HallJoy physical aliases, not claims about USB keyboard usages. The
ordinary scancode channels remain for compatibility; split presets use only the
four independent physical identities. Publication uses the existing fresh full
per-device snapshot and Provider V2 extended-key path, including owned zeroes.
No depth is generated from digital input. Other PIDs and V1 reports do not gain
guessed physical maps. Unknown/unassigned entries remain subject to the existing
V2 report format and termination policy. Real-device validation is still needed.

Binding capture prefers visible physical aliases on equal-depth ties, avoiding
selection of the old shared Space/Fn alias. Key settings, curves, bit masks and
profile persistence cover the enlarged supported code domain. Existing ordinary
60HE v2 geometry keeps ordinary Space/Fn behavior. UwU factory identities do not
constitute device remap readback.

Block bound keys suppresses the shared Windows Space usage if either physical
half is bound. Windows cannot distinguish those two identical digital usages,
so both halves' ordinary Space events are affected, while their analog depths
remain independent. Custom digital remaps are not read back by this change.

## Build and validation

Source backup and the previously sent diagnostic EXE:
.local/backups/wooting-extended-before/. Layout integration backs up generated
outputs separately. Rebuild the pinned native UAP and copy verified abiv0/abiv1
into build/runtime before embedding in HallJoy. This delivery must be an ordinary
build with HallJoyInputPathDiagnostic=false; no automatic diagnostic log.

Completed validation:
- Pinned extraction --check: five unchanged reports.
- Wooting catalog: 18 variants, 1,365 keys; layout pipeline tests: 19 PASS.
- Portable physical-channel/controller, blocking-policy and key-domain tests PASS.
- Full native static audit suite PASS, including production decoder/mapping.
- Production-linked profile suite PASS, including split bindings and highest alias
  curve persistence; startup recovery PASS (16 scenarios plus repeated startup).
- Native UAP and ordinary Release x64 builds PASS; existing ViGEm missing debug
  symbols warning only. Embedded ABI1 matches the rebuilt DLL byte-for-byte.
- Exact EXE Shark, Mini60, NA87 and embedded ViGEm self-tests exit 0. These isolated
  tests created no logs or other files.

Delivery: build/bin/Release/x64/HallJoy.exe. Ordinary build, optional logging.
EXE SHA256: 9d6059cfb9c9fd8a5e1318f6eacad7c1871b2b448d2e88b1f2d1ad5565e046a3
Embedded ABI1 SHA256: 4a7dd157c8f47b070b255937a7f3d82c64e657aec5338b514da733528f8545a8
Evidence: .local/wooting-extended-verification.json.

Initial profile tests were blocked before initialization by instance-guard error 5
while other HallJoy processes existed. After the owner closed them, the same
simulator passed without source changes. The exact ACL cause is not established;
this work does not fix the existing instance guard.
No Wooting hardware or visual validation was performed. No GitHub publication.
The previously sent forced-logging diagnostic is preserved in the backup.
