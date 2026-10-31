# MCHOSE Mix 87 III: other firmware versions admitted (2026-10-29)

## Tester log

`message (17).txt` (HallJoy 1.6.13, 2026-10-29 18:47 UTC): USB product
"Mix 87-III" `3837:300D`, HID interfaces 00/01/02, the M HUB control collection
(usage `0001:0000`, 65/65 bytes) present. Backend `mchose_mix87` (protocol 27):
`present_not_connected`; every 5 s `mix87.version_unreviewed=1` and
`mix87.admission_failed=1`, no command after the `03` device-info reply.

Cause: Mix 87 III was the only M HUB model still bound to one firmware version
(exact 1.22 + seven flash fingerprints, from the 2026-09-27 review). The
keyboard answered `03` with a version other than 1.22, so it was refused before
any analog-mode step. The refused path did not log the version, so the exact
version is not known from this log.

Official M HUB catalog, re-read 2026-10-29 (`main.7d734adfcc65efb81c4d.js`,
local in `.local/research/mchose-mix87-20261029/`): Mix 87 III `fwVersion` 122,
`minFwVersion` 112, the same update file as on 2026-10-02. No newer firmware is
published, so the tester most likely runs an older 1.12–1.21 build.

## Change

Owner rule for the M HUB family (Jet 75 II, 2026-10-02): no firmware-version
binding; any version is admitted, the version is logged, and safety comes from
the data checks (layout/settings/base checks before the write, readback, A0
validation). Applied to Mix 87 III:

- `mchose_mix87_backend.cpp`: the `03` reply is required for every model and
  `mix87.model` / `mix87.firmware_version` are logged on every admission
  (also for 1.22). Firmware 1.22 keeps the exact fingerprint path unchanged
  (a 1.22 image whose fingerprints differ is still refused,
  `mix87.firmware_mismatch`). Any other version uses the factory key list and
  the plausible maximum range 200..600, like the other ARM boards.
- `mchose_mix87_protocol.h`: `Mix87IIIKeys` (86 keys + Fn), taken from the 1.22
  factory A0 descriptor table at 0x15da6 (its SHA-256 equals the shipped
  fingerprint `4a6bcfa4...`); equal to the existing "MCHOSE Mix 87 III ANSI"
  layout keys without Fn. Layout auto-selection is unchanged.
- Tests: `mchose_mix87_protocol_test` (model table, Mix 87 III key list),
  `mchose_mix87_session_test` scenario 2 (version 1.23: admitted, no hashing,
  86 keys, layout token, auto enable + cleanup, survives a device change).

Not changed: the analog-mode lifecycle (enable at start/resume, disable at
pause/exit, one automatic enable per generation, padding and readback checks).
Status stays Supported. Older firmware was not reviewed (no older image is
published); if an older build lacks the A0 service, the log shows an admitted
session with the mode enabled but no analog reports, and the firmware version.

## Alternative for the tester

Updating the keyboard to 1.22 in M HUB makes the current release (1.6.13)
work on the exact path without this change.

## Validation

- `run_native_backend_checks.py --require-compiler`: all static and portable C++
  tests passed (Mix87 protocol and lifecycle tests included).
- `build_release.ps1`: PASS (profile transactions, layout catalog, yellow audit,
  supported layouts 83, startup recovery). Installed
  `build/bin/Release/x64/HallJoy.exe` SHA-256
  `727911eb3e4a527fcb19741018a02b777512f14b4294234ba6889247acdde16a` (local, not
  published; also contains the Redragon K552-M profiles of the same day).
- No hardware test by the agent. Next tester log should show
  `mix87.firmware_version`, `mix87.mode_enabled=1` and `connected` with 86 keys.
Backup: `.local/backups/mix87-version-20261029/`.
