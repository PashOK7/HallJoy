# Startup profile recovery — 2026-09-11

Owner reported 1.5.0 refusing to start on another PC with older HallJoy data.
The exact offending files were not supplied, so their particular parse failure
is unknown. Confirmed defect: app.cpp returned 1 for any incomplete settings /
bindings pair or unavailable active named profile. Previous startup test asserted
that exit, testing preservation but not the required successful recovery.

## New contract (1.5.1)

- Validate preferences and the complete active profile before applying it.
- Healthy modern bundles and valid legacy pairs load unchanged, no recovery files.
- On failure, try the Default pair and known settings.ini.pre-bundle.bak and
  settings.ini.bak candidates. Prefer a complete candidate, otherwise preserve
  independently valid settings / bindings and default the unavailable part.
- Never silently replace a malformed modern bundle with stale legacy bindings.
- Before rewriting root settings, copy and byte-verify existing root settings and
  bindings into .internal/ProfileRecovery/<content-derived-id>/ and flush copies.
  Verify bytes again before committing. Hash is only a directory name; collisions
  cannot authorize replacement because full bytes must match.
- Existing backup is reused for identical retry input, never overwritten.
- Only root settings.ini is atomically replaced with a complete Default bundle.
  Legacy bindings, backups, named profiles, layouts, curve presets stay untouched.
- Backup/read/commit failure: start in-memory, disable automatic profile saving
  and atomic INI writes for this session. Inform the user after the window opens.
- First-run selection remains armed only for a genuinely empty root.
- Critical recovery trace records the selected source, completeness, writable
  state and backup location. Ordinary first run does not emit recovery warnings.

Storage-root initialization and explicit pending factory reset are separate
existing transactions; their fatal permission/rollback failures are not silently
reclassified as corrupted profiles by this change.

## Verification

Production-linked profile transaction suite and 14 real startup/shutdown scenarios
passed initially. Includes missing/invalid/empty inputs, missing named profile,
backup restore, blocked backup and five atomic failure stages, plus repeat starts.
Backend initialization is forbidden by test flags; no device I/O or visual QA.
Factory reset source audit updated to check ordering against the new startup
entrypoint rather than the removed direct SettingsIni_Load call.
Final production build and updated regression results are recorded below when done.

Pre-change source/EXE backup: .local/backups/startup-recovery-20260911/.

## Final local result

- Full static/portable suite PASS; production-linked transaction suite PASS.
- Expanded startup suite: 16 scenarios plus repeated starts PASS, including valid
  named profiles and prevention of stale-legacy fallback for broken bundles.
- Release compilation: zero errors; only existing ViGEmClient LNK4099 warning.
- Embedded installer resource/extraction/signature check PASS; production
  continuous telemetry isolation check PASS.
- build/release/HallJoy.exe is version 1.5.1.0, SHA-256
  4BA1CD93D1F1BED775B67DB9B65DBB9EE294F03D47AFE6EC948147303DF776C4.
  Compiler output and distributed EXE hashes match.
- Evidence: .local/startup-recovery-build.log. The outer PowerShell redirection
  reports NativeCommandError for unittest's progress on stderr; the contained
  build completed all gates and wrote the final matching checksum.
- Owner confirmed this EXE starts successfully on the affected second PC and
  authorized publication of 1.5.1. This is owner-reported validation; the original
  problematic files were not supplied for local reproduction.

## Published

https://github.com/PashOK7/HallJoy/releases/tag/v1.5.1 is the latest stable release.
Tag/source commit: e72cca95d1e1d4cb091768889e8aa4eb4645b037.
Linux and full Windows CI both passed:
https://github.com/PashOK7/HallJoy/actions/runs/34576922500.
Uploaded EXE and a downloaded copy match the owner-tested SHA-256 above.
Assets are HallJoy.exe, THIRD_PARTY_NOTICES.md and LICENSE; no application ZIP.
v1.5.0 remains available. Automatic CI uploaded zero Actions artifacts.
