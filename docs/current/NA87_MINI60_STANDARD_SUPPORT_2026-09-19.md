# NA87 and MINI60 HE Pro — ordinary HallJoy integration

## Owner decision

After reviewing tester logs15/16, the owner accepted the current NA87 behavior
and requested full ordinary support for both keyboards. NA87 is no longer frozen.
This is a product/support decision, not proof of the depths remaining in log16:
that older log cannot distinguish a real hold from small residual values or lost
releases. Keep change-event semantics; do not introduce timeout releases for NA87.
Other frozen models and their warnings remain unchanged.

## Integration

Both native backends and their layout presets are included by default in the
project. The delivered Release uses the ordinary MAD68ProRNative production
configuration; no HallJoyAulaMini60Diagnostic or HallJoyIrokNa87Diagnostic property
is required. The established delivery path is retained, despite its historical name.
Normal binds, profiles, curves, dead zones and gamepad output remain in use.
Automatic layout uses supported assignments; manual selection uses factory keys.

NA87 uses verified M484 identity, its live map and event subscription. Its frozen
metadata classifier and native telemetry warning were removed; Pro/ND75 unchanged.
AULA stays limited to wired 0C45:80A2 with full identity/collection checks. No
receiver commands. Existing scaling, independent 50-ms freshness and safe session
cleanup are retained. Absence waits on shared topology notifications rather than
re-enumerating HID every second. Worker heartbeat continues without idle log growth.
The native backend remains functional if logging cannot open/write its file.
Neither native backend owns the main window title in the ordinary build.

AULA complex/vendor-only assignments still do not pretend to be ordinary HID keys.
In log15 the position85 type2/usage175 assignment was unsupported; its exact meaning
has not been established. Manual factory Fn mapping remains available. Do not
claim universal support for arbitrary firmware actions, macros or every key.

## Support log

HALLJOY_DEVICE_SUPPORT_LOG enables the shared HallJoy.log without general
HALLJOY_DIAGNOSTIC hooks or timed tester prompts. Both native families' aggregates
and support events are retained, alongside sanitized lifecycle records. Unrelated
plain debug formats are rejected before formatting/queueing. Direct append,
redaction and terminal flush use the previously tested log path. Normal controller
operation does not depend on log success. No ordered typing/serials/HID paths.

## Validation and artifact

- MSVC Release x64: PASS; existing ViGEm missing-PDB warning only.
- Portable AULA model, NA87 protocol/layout and frozen-status tests: PASS.
- Exact delivered EXE: both native self-tests PASS; AULA tests common registry to
  gamepad output, independent expiry, alias releases and worker containment.
- Ordinary support-log Win32 test: PASS, including redaction, live append, final
  record, no NUL padding and preservation when log access is denied.
- EXE log privacy sentinels: absent as expected. No local device/visual/game run.
- Full native backend static gate: PASS.

Artifact: `build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe`

Size: 9315328 bytes; SHA256 `d4af7d1afe09f9b8f6426bfd15cb16bc88e34fda5f46d8d489f75e1007b0c94f`.

Backup: `.local/backups/na87-mini60-release-before-20260919-121655.zip`.
Verification: `.local/na87-mini60-release-verification.json`.
Static output: `.local/na87-mini60-release-static.txt`.


Current candidate and neutral delivery path supersede the historical artifact
above: RELEASE_1.5.3_CANDIDATE_2026-09-19.md. Owner test before publication.
