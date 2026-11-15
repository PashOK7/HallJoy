# Interface languages: local implementation and verification

This records the initial localization delivery. The later banner, Profiles and
CJK-font changes and their final artifact are recorded in
[the UI follow-up](UI_POLISH_2026-11-15.md).

Date: 2026-11-15. Base HEAD: `35ba83ac1f42fe849bd896721503a880a8cf4c61`.
The changes are local and uncommitted on `fix/windows-build-portability`, alongside
the preceding build-portability work. No remote repository operation was performed.

## Implemented behavior

- Existing PremiumCombo dropdown in Global settings, immediately before Factory
  reset: English, Korean, Japanese, Russian, Spanish, Italian, French, German and
  Simplified Chinese. English remains the default.
- Application-wide `[Window] Language` preference. Atomic save/readback; rollback
  of both runtime choice and selection when persistence fails. Profile-only loads
  cannot change it. Old/unknown values fall back to English.
- Immediate UI refresh: tab captions, registered fixed native labels, marked
  PremiumCombo options and retained page caches. Engine/provider logic is unchanged.
- Native dark tooltips after 1,800 ms for selected Global/Configuration controls
  in the eight added languages; no additional English explanations. Hover help
  cleans up on leave, scroll, capture, visibility, language change and destruction.
- Embedded UTF-8 catalog: **204 rows**, nine nonempty columns per row. Fixed UI
  options retain canonical item strings for existing selection/create logic;
  user-created names and hardware identities retain their original values.

Usage, scope and contributor guidance:
[Interface languages](../development/INTERFACE_LANGUAGES.md).

## Verification

| Check | Fresh result |
| --- | --- |
| `tools/run_native_backend_checks.py --require-compiler` | Exit **0**; all static and portable C++ checks passed with the normal cache enabled. |
| `ui_language_test.cpp`, included in that suite | Nine codes/default/fallback, invalid selections, refresh revision, representative Russian/Japanese translations, unique keys, complete columns and matching printf contracts: **PASS**, 204 messages. |
| `tools/build_release.ps1` | Exit **0**; final ordinary EXE installed to the standard output path. |
| Production-linked private-desktop picker test | All nine selection events and persisted codes, dropdown geometry/hit testing, profile isolation including an injected foreign language key, atomic Replace failure rollback, full reload, unknown/missing key fallback, Russian tooltip visibility/cleanup and English suppression: **PASS** as part of `PROFILE_TRANSACTION_WINDOWS_TEST`. |
| Existing integration gates in release build | Profile/layout/overlay/curve/tray tests, complete catalog checks and startup recovery: **PASS**. Startup recovery reports 18 scenarios plus repeated startup. |
| `git -c core.whitespace=cr-at-eol diff HEAD --check` | Exit **0**. |

Commands used the installed Python 3.12.15 and Visual Studio 2022 x64 Clang/MSVC,
discovered through the repository's build environment helper. There were no C/C++
compiler warnings in the final release log. The existing allowed `LNK4099` notice
for the missing third-party ViGEmClient debug PDB remains.

The first native-suite attempt stopped in `tools/test_block_keys_group.py`, which
searched for the exact English spelling of a rendered label. Its anchor now uses
the actual renderer function; its group visibility/input assertions remain.
The final suite passed after this adjustment and the final catalog additions.
The first 187-message build also passed; the final results above refer to the
204-message version and its fresh build.

Local detailed evidence:

- `build/evidence/ui-languages-native-final-2026-11-15.log`
- `build/evidence/ui-languages-build-final-2026-11-15.log`
- `build/evidence/ui-languages-artifact-2026-11-15.json`
- `build/evidence/ui-languages-portable-2026-11-15.json`

`src/HallJoyProject/tests/profile_transaction_windows_test.cpp` invokes
`KeyboardSubpages_TestLayoutPicker`; its language assertions run inside the
simulator's isolated data root and private desktop. These tests do not establish
new hardware support or constitute a visual review.

## Delivered local artifact

- Path: `build/bin/Release/x64/HallJoy.exe`
- File version: `1.6.15.0` (not bumped for this unpublished local change)
- Size: **11,553,792 bytes**, from `Get-Item`
- SHA-256, from `Get-FileHash -Algorithm SHA256`:
  `523f07bb84b0561b5f25bf8cd517c4aff9802b483b11ccba5f372af6a0924180`

Source checkpoint: `.local/backups/20261115_140717_ui_languages/`, with 589
code/test/build files, preserved relative paths, SHA-256 manifest and individual
restore instructions. Earlier uncommitted work is included in the checkpoint.

The existing personal `.local/portable/HallJoy.exe` was also atomically replaced
with this exact verified image through `tools/publish_halljoy_build.ps1`. The
previously running window was restored with `WindowStyle=Normal`; the helper's
readback reports `WasRunning=true` for that exact path. Thus `Start.cmd` uses the
updated build. No settings/profile directories were copied or replaced by the
replacement helper. Normal app shutdown/startup persistence still applies.

Previous personal EXE SHA-256:
`59b3886d9c99e76131b0629e25acd2477144771216dc0cc00aaa57d6e8e11139`.
It is retained at
`build/obj/replacement-backups/HallJoy-a1d4f783531942738628c12d4faa465e.exe`.

## Review boundaries

No visual or physical-keyboard test of this new build was performed. Translation
quality and long-label/font presentation still need owner/native-speaker review,
particularly for CJK scripts and narrow windows. The programmatic tests establish
the selection/persistence/lifecycle behavior, not typography or linguistic quality.

This initial catalog translates common UI and selected explanations. Detailed
protocol telemetry, legal text and some uncommon editor/error messages remain in
English. No keyboard support status, firmware behavior or device geometry changed;
no support Sheet synchronization is required for this feature.
