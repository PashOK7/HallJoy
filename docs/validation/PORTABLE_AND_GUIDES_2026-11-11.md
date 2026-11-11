# Portable helpers and language guides — 2026-11-11

This extends the optional Windows installer contribution on the same
`feat/windows-installer` branch. The application, keyboard support tables, driver
and release automation are unchanged.

## Adapted material

| Original Driving Engineer material      | HallJoy contribution                                                                              |
| --------------------------------------- | ------------------------------------------------------------------------------------------------- |
| Portable launch and release updater     | Root Start.cmd / Update.cmd and tools/start_portable.ps1 / update_portable.ps1                    |
| Seven localized READMEs                 | Seven concise HallJoy quick-start guides in docs/translations, linked from README                 |
| Installation and personal-data guidance | docs/PORTABLE.md and the existing optional-installer guide                                        |
| Privacy explanation                     | docs/INPUT_AND_PRIVACY.md, grounded in the existing data paths and diagnostics                    |
| Personal Forza notes                    | docs/DRIVING_SETUP.md, generalized without private screenshots or unverified game-specific claims |

The language guides link to the canonical support tables instead of copying
device status into seven independently maintained lists. Author attribution and
licenses remain those of HallJoy. Private correspondence, logs, settings, old Git
history and build caches are not publication inputs.

## Verification

- `powershell -NoProfile -ExecutionPolicy Bypass -File tools/test_portable_helpers.ps1`:
  **25 assertions passed** (count of `checks` in the JSON results).
  Evidence: `build/tests/portable-5480a3ddaac640a98918a2e675c64cc0/results.json`.
- The suite tests default paths, first install, SHA-256 verification, no-write
  CheckOnly, already-current behavior, byte-identical rollback backup, private
  data preservation, invalid digest/URL/release, network failure, running-process
  guards, rejected non-portable destinations and launcher preflight.
  Tests use simulated network responses and never execute the fixture EXEs.
- A real `update_portable.ps1 -CheckOnly` call queried GitHub and reported
  v1.6.15 already current, with matching SHA-256. It did not download or replace
  the local binary. `start_portable.ps1 -CheckOnly` passed on the personal copy.
- Testing found and fixed a Windows PowerShell 5.1 default-parameter issue:
  the script now resolves PSScriptRoot inside its body, after parameter binding.
- Seven translation files decode as UTF-8 without replacement characters.
  All **88 local Markdown link targets** checked across the main README, seven
  guides and three added user documents exist. This checks file targets, not
  remote availability or heading anchors. The guides were not reviewed by native
  speakers; they are explicitly concise guides rather than full technical translations.
- A comparison against HEAD confirmed the canonical Supported / Experimental
  blocks in README remain byte-equivalent after text decoding.
- `tools/project_context.py --check` passed. No application C++ changed; the
  previous full-build/compiler limitations remain as recorded in
  [installer validation](WINDOWS_INSTALLER_2026-11-11.md). The already-passed
  installer integration suite was not repeated for documentation and helper changes.

## Private data preservation

The existing private portable settings were copied into `.local/portable` after
checking that HallJoy was stopped. One INI file was present (settings.ini); the
three preset/profile directories were empty. Original settings and backup hashes
matched the copied bytes. A backup with a hash manifest remains under
`.local/backups/import-driving-engineer-*`; the original directory was retained.
No contents of personal settings are included in this contribution.

Git ignore checks cover `.local/portable`, legacy `app/` and root personal
settings/profile directories. Existing versioned source/test presets are not
globally ignored. Installed and portable copies retain separate data directories;
transfers are explicit and documented, without background synchronization.

No interactive application launch, hardware/game testing, visual wizard review,
new C++ build, signing, commit, push, PR or message to the upstream author was
performed for this extension.

## README presentation follow-up

The English README and nine localized guides now share the same header order:
project title, technology badges, then language navigation. French and Italian
guides were added; the other seven translations were retained. The C++20 badge
matches the application project file; PowerShell and NSIS badges link to the
optional helper and installer documentation. The current language is highlighted.

A focused check passed for all ten README headers and UTF-8 decoding. All **216
local Markdown targets**, including heading anchors where present, resolved across
the ten READMEs and the three added user guides. The main README from Supported
onward matches HEAD exactly after UTF-8 decoding. This supersedes the earlier
seven-guide/88-target inventory above without changing its recorded result.

No application code changed in this follow-up. Remote badge availability,
rendered appearance and native-speaker review were not checked; the earlier
application-build and hardware-testing limitations still apply.
