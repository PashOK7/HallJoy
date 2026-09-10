# Configuration persistence review — 2026-09-07

Follow-up implementation and verification: [CONFIGURATION_SAVE_FIX_2026-09-07.md](CONFIGURATION_SAVE_FIX_2026-09-07.md).
The diagnosis-only statements below describe the state before that fix.

Scope: diagnosis only. No production source changes, no user setting changes,
no restart of the user's running application. This is not a certification of
perfect saving or full end-to-end UI coverage.

## Actual environment

- Running UI PID 17184: `build/release/HallJoy.exe`, ordinary command line;
  executable timestamp 2026-09-06 20:03:35. Do not assume it is the current
  source or the previously built MAD68ProRNative binary.
- Read-only process token query reports **not elevated**.
- `%LOCALAPPDATA%/HallJoy/settings.ini` exists, active profile is Default,
  timestamp 2026-09-07 22:22:28. File and parent directory ACLs grant PC
  FullControl. No ReadOnly attribute (Archive, Temporary).
- Thus lack of administrator elevation is not by itself evidence of a write
  permission problem here. ACLs/timestamps do not prove every UI edit saves,
  nor exclude transient locks, security software, or other failures.
- No HallJoy.log or HallJoyStabilityTrace.log exists beside this release exe;
  cannot claim successful saves from a runtime log. Source trace initialization
  is conditional on build flags.

## Source findings (not dynamically reproduced)

1. Interrupted curve drag has no completion hand-off. In
   `keyboard_keysettings_panel_graph.cpp:875` and adjacent drag branches,
   mouse movement changes runtime settings, but requests persistence only on
   mouse-up (947) or wheel (928). Configuration's WM_CAPTURECHANGED handler
   (`keyboard_subpages.cpp:10957`) clears its custom-control drag state but
   does not finish/cancel the graph's separate g_drag or request saving.
   If capture is lost before mouse-up, the graph can retain its drag state and
   the edit has no own pending save. Another edit or normal shutdown may still
   save it; this is not a claim that every interrupted edit is lost on restart.

2. Preset save reads animation rather than committed settings:
   `keyboard_subpages.cpp:10547` uses Ksp_GetVisualCurve. That function
   (`keyboard_keysettings_panel.cpp:371`) interpolates for 250 ms. Ctrl+S
   is available while animation runs (keyboard_subpages.cpp:11005).
   A save immediately after a mode change/undo can therefore persist an
   intermediate curve rather than the actual active target. Normal
   settings autosave reads active settings; this finding concerns curve presets.

3. Preset save is two independent transactions. `keyboard_profiles.cpp:407`
   replaces the preset first, then saves `_preset_state.ini`. If the latter
   fails, SavePreset returns false and restores only in-memory active state;
   the preset file is already changed. UI reports failure at
   `keyboard_subpages.cpp:10558`. Previous-file-preserved wording is not a
   valid guarantee for the entire two-file operation.

4. Autosave retry gap: app.cpp kills SETTINGS_SAVE_TIMER_ID before saving;
   failure logs a warning without scheduling a retry. A later change or
   normal shutdown attempts again. This needs a defined recovery policy,
   not an assumption that an atomic file replacement also guarantees eventual
   saving of the latest edit.

## What looks intentional / should be preserved

Configuration toggles, sensitivity and Spark controls request debounced saving.
Global/per-key curve parameters are serialized and restored. Normal shutdown
attempts a final save; switching global profiles saves the previous profile
synchronously. Individual file writes use temporary-file validation and atomic
replacement. Preset identity and current custom curve are different concepts;
auto-selecting Custom on a nonmatching curve is intentional in the source.
Floating curve values are stored in thousandths, so bit-exact float roundtrip
is not the existing format's promise.

## Checks and limitations

- `tools/run_native_backend_checks.py --static-only`: PASS.
  Evidence: `.local/configuration-save-review-static.log`.
- `tools/run_profile_transaction_tests.ps1`: BLOCKED at simulator compilation,
  backend.cpp:1509 error C2065 (`return v`, undeclared v in simulator branch).
  No profile transaction runtime test executed. No repair made during review.
- Isolated tests are useful for fault injection without touching real data,
  but cannot establish correctness of the actual release executable, actual
  UI events, user permissions, startup restoration or interference by other
  software. A follow-up real-UI edit/save/restart comparison requires backing
  up the user's settings and explicitly agreeing to the temporary changes.

## Confirmed numeric read failure after user's all-except-Remap report

`bounded_ini.h` Read starts capacity at 256 and runs only while capacity <=
maximum. Both ReadSigned and ReadUnsigned call Read with maximum=128. Thus
neither reader ever calls GetPrivateProfileStringW: both always return false.
SettingsIni's numeric wrappers respond by returning their default values.
Validation uses the ordinary 65536 limit and can pass, so this failure silently
discards persisted numeric values on load rather than rejecting startup.
Remap uses a different reading path and is not affected by this specific bug.
Subsequent saves can overwrite previously customized numeric values with those
defaults. This explains a much broader symptom than the edge cases above.

Read-only native reproduction, compiling the current production header:
`.local/configuration_read_probe.cpp` / `.local/configuration_read_probe.exe`.
On the actual `%LOCALAPPDATA%/HallJoy/settings.ini`, raw reads returned
DeadzoneLow=80, OutputCap=1000, SnappyJoystick=0; bounded 128-character and
ReadSigned calls failed for all three, leaving the output sentinel unchanged.
Result: `READ_LIMIT_BUG_REPRODUCED_ON_REAL_SETTINGS=YES`.
This confirms the source reader on the real file, not an instrumented trace
inside the already-running release executable. No production fix made yet.

Existing bounded_ini_numeric_test.cpp exercises Signed/Unsigned string parsing
only, not ReadSigned/ReadUnsigned file access. Its passing result does not
cover this bug. Required regression: real INI read/save/load roundtrips with
non-default values and buffer limits below/equal/above the initial capacity.

Backup of the complete user settings tree before diagnosis:
`.local/backups/settings-diagnosis-20260907-223105/HallJoy/`.
