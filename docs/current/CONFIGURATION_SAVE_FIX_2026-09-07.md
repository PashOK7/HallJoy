# Configuration numeric persistence fix — 2026-09-07

## Root cause and implementation

The shared bounded INI reader started at capacity 256, but numeric wrappers
limited it to 128. The loop never ran, and settings loaders substituted defaults.
Remap used another path. This affected settings, per-key settings, curve presets,
and other consumers of the shared numeric wrappers; it was not an admin-rights
requirement or a Configuration-only missing save notification.

Read now starts at min(256, maximum), grows to the actual limit (including
non-power-of-two limits), rejects invalid capacities, and never accepts a
potentially truncated token. Failure leaves the caller's output unchanged.
The existing conservative buffer-capacity contract is retained.

Adjacent Signed parser review found unsigned subtraction underflow when the
next digit exceeded a small allowed magnitude. It could allow arithmetic
overflow for long input. Reject that digit first and compute sign-appropriate
nonnegative magnitude bounds. Regression covers huge strings and positive-only,
negative-only, and zero ranges.

Fixed simulator-only backend.cpp undeclared `return v` to return the raw value
just stored in its cache. This restores compilation of the production-linked
file-only profile tests; no hardware protocol branch is enabled or changed.

## Test coverage added / corrected

- bounded_ini_numeric_test now writes an actual temporary Windows INI and
  calls ReadSigned/ReadUnsigned, not just parsing preconstructed strings.
  Tests non-default signed/unsigned values, missing-key defaults, malformed
  numbers, negative unsigned input, small/large/non-power-of-two capacities,
  truncation, and output preservation. New checks execute with NDEBUG too.
- Built the new test against the backed-up old header: exit 1 as expected.
  Against the corrected header with NDEBUG: PASS. This is a red/green
  reproduction of the actual file-reader regression.
- Production-linked profile transaction tests now roundtrip all global curve
  numeric fields, Configuration toggles/modes/sensitivity, Spark parameters,
  every per-key curve field for both ordinary and extended keys, and curve
  preset files. All test values differ from defaults where applicable.
- The Windows instance-guard test previously asserted when a normal HallJoy
  instance was running. It now reports this precondition and exits nonzero
  rather than presenting it as an unexplained mutex failure.
- The broad test runner flushes command output immediately and limits each
  subprocess to 120 seconds. Timeout is a failure, never a pass.
  Windows test executables explicitly link advapi32, fixing clang/MSVC token
  API linkage which MinGW previously supplied implicitly.
- Official tools/build.ps1 now gates releases on run_profile_transaction_tests.ps1,
  in addition to the broad test runner. The helper was run successfully and
  the modified build script passed PowerShell parsing; the complete packaging
  script was not rerun for this incremental executable replacement.

## Completed verification

- Real INI buffer/numeric regression: PASS (Clang; also with NDEBUG).
- Complete static + portable/native C++ runner: PASS, exit 0.
  `.local/critical-ini-all-tests-complete.log`. Earlier interrupted/conflicted
  and clang link-error attempts are not counted as successful runs.
- MSVC Release simulator-linked profile suite: PASS, including 100 concurrent
  profile loads without mixed observations, five injected file-write failure
  stages, malformed input rejection, legacy migration, and rejected-startup
  file preservation. `.local/critical-ini-profile-tests-final.log`.
  Isolated evidence root:
  `C:/Users/PC/AppData/Local/Temp/HJProfileTest-8c76b3ce37b44e789560d374d4fa33d5`.
- Production Release x64 build: PASS (existing ViGEm PDB warning only).
- Actual `build/release/HallJoy.exe`, ordinary user launch and actual LocalAppData:
  Configuration command toggled Snappy 0 -> 1; autosave wrote 1. After graceful
  shutdown/relaunch the inverse UI command wrote 0, proving restored runtime
  state was 1. Another shutdown/relaunch completed; original value restored.
  `.local/critical-ini-real-ui-unelevated-final.log` and the
  `.local/critical-ini-real-ui.ps1` harness. Each of the three launches queried
  the process token and confirmed REAL_APP_ELEVATED=False. Launching through
  desktop Explorer avoided inheriting the elevated automation host's token.
  The earlier `.local/critical-ini-real-ui-final.log` run was elevated and is
  only additional evidence, not the ordinary-user permission check.
  Initial harness used BM_GETCHECK incorrectly for an owner-drawn toggle;
  corrected oracle observes the runtime-setting inversion through its real
  command handler and subsequent disk write. The first harness failure is not
  evidence that the fixed loader failed.
- User settings comparison after UI test: only window PosX/PosY were added by
  ordinary window persistence; no other setting lines or Remap values changed.

## Delivery and limits

Replaced `build/release/HallJoy.exe`; SHA256SUMS.txt updated.
SHA256: EE7DB8BB108546E7EEFDAC697BD53B7ED2089D3B74994ACAC99CD7BC45444D73.
Source, prior release executable, checksum, and complete user settings backup:
`.local/backups/critical-ini-20260907-223412/`.

This fixes numeric restoration and strengthens relevant tests, not a claim
that every possible bug is eliminated. The prior review's separate interrupted
drag, animation-frame preset save, two-file preset partial failure, and autosave
retry concerns remain documented; they were not silently bundled into this
critical loader repair. Previously overwritten custom values cannot be
reconstructed by the fix; do not automatically roll back to an arbitrary backup.
