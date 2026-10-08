# "Automatic layout" reverted after restart with a named profile (2026-10-06)

Owner: the automatic layout setting resets after every restart.

## Cause (confirmed in code and in the owner's settings)

- `[KeyboardLayout]` (`Automatic`, `PresetName`, `LastAutomaticPreset`) is
  global: it lives only in the base `settings.ini`; named profiles
  (`GlobalProfiles/<name>.settings.ini`) never carry it, and startup loads it
  from the base file.
- `SaveSettingsByActiveGlobalProfile` (app.cpp) saved, while a named profile
  was active, the active marker, overlay, the profile file and the window
  section, but never the layout. Any layout change made then (the Automatic
  checkbox, a manual preset) was lost; the next start loaded the old value.
- Owner machine: `ActiveGlobalProfile=123`, base file `Automatic="0"`,
  `LastAutomaticPreset="Keychron K4 HE ANSI - Imported"`; the profile file has
  no layout section. Matches the report exactly.

## Fix

- New `SettingsIni_SaveLayout(path)`: an atomic `LayoutUpdate` transaction on
  the existing base file (copy, rewrite `[KeyboardLayout]` only, validate the
  `Automatic` value, replace), refusing to create a missing base file, like
  `SettingsIni_SaveWindow`.
- The named-profile save path now also calls it; the Default path is
  unchanged (its full save already includes the layout).
- Tests: `profile_transaction_windows_test` `layout_base_save` (only the layout
  section changes, Automatic survives a reload, a commit fault leaves the file
  byte-identical, a missing base is not created); the persistence static audit
  requires the layout save in the named-profile path.
- The owner's current file still says `Automatic="0"`: enable the checkbox once
  with the new build; it now persists. No settings were edited by the agent.

## Validation

`build_release.ps1` EXIT=0 (`layout_base_save=PASS`,
`PROFILE_TRANSACTION_WINDOWS_TEST=PASS`, startup recovery 18 scenarios, all
gates), persistence static audit PASS, `run_native_backend_checks.py
--require-compiler` PASS. Same build carries the Logitech PRO X2 RAPID v2
decoder. EXE SHA-256
`cb738c6b5d25a9bcd6b16faccb06ff2ac8ed529bb413ae0ac618d581adcd477b`.
