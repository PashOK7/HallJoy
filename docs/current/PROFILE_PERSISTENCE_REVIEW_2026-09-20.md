# Profile persistence review — 2026-09-20

## Scope and outcome

Reviewed global_profiles.cpp, settings_ini.cpp and ini_util.cpp and ran the
production-linked isolated profile suite with backend initialization forbidden.
No confirmed persistence defect was found in this review. Existing user files and
the running ordinary application were not touched. ATTACK SHARK work remains
paused; no protocol changes were made.

Existing tests confirmed:

- Legacy settings/bindings migration to a single bundle preserves originals.
- Invalid profiles and failed active-profile marker writes do not change the
  active in-memory profile. Active profiles cannot be deleted.
- 100 concurrent profile loads expose no mixed settings/bindings to consumers.
- Non-default curves, per-key settings, extended physical-key bindings and
  Block Bound Keys survive serialization/load. Global logging preferences stay
  outside gameplay profile switching.
- Window-only saves preserve unrelated sections and fail without damaging the
  file. Main/overlay layout selections and automatic-layout persistence pass.
- Failures at Prepare, Write, Flush, Validate and Replace leave the committed
  bundle unchanged; normal failure cleanup removes its temporary files.
- Startup recovery preserves damaged originals, rejects stale legacy fallback
  for broken modern bundles, and handles unavailable recovery storage safely.

## Additional regression coverage

Extended tools/test_profile_startup_recovery.py from16 to18 scenarios, each with
appropriate repeat-start checks. Two added fixtures model process interruption
before atomic replacement: a partial orphan temporary file and a fully written
but uncommitted temporary bundle. Neither supersedes the last committed settings
and bindings on restart; their bytes are preserved. This is restart testing from
constructed crash residues, not a process-kill-at-each-instruction test or a
power-loss/disk-durability guarantee.

The first full run failed at the existing Input Overlay event test after the
persistence checks had passed. The generic failure did not locate the cause.
Added model selection, follow/browse and full-layout tracking checkpoints inside
that simulator-only test. The next full run passed, so the initial failure
remains an unreproduced test anomaly, not a proven storage or UI defect. Do not
claim it was fixed by the additional diagnostic messages.

## Evidence

- `.local/profile-review-20260920-tests.txt`: initial run, including anomaly.
- `.local/profile-review-20260920-retry.txt`: complete production-linked suite,
  layout catalog audit and original16 startup cases PASS.
- `.local/profile-review-20260920-startup-final.txt`:18 startup scenarios PASS.
- Full-suite root: `C:/Users/PC/AppData/Local/Temp/HJProfileTest-d546ab687250498dacb8fd594dbc35ae`.
- Extended startup root: `C:/Users/PC/AppData/Local/Temp/HallJoyRecovery-fhsy73jb`.
- Source backup: `.local/backups/profile-review-20260920.zip`.

Changes are test-only. Ordinary EXE remains the previous Pause-order fix:
`build/bin/Release/x64/HallJoy.exe`, SHA256
`7a139eebfac7786aca6647d77970a69c43dda7880e76b4f4c342cec87218e408`.
No release build replacement, visual inspection or GitHub publication was needed.
