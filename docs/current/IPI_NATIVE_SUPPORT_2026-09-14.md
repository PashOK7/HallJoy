# IPI native support fixes — 2026-09-14

> Follow-up: [Automatic layout](AUTOMATIC_LAYOUT_2026-09-14.md) separates live
> assignments from factory input. Manual mode ignores device assignments;
> successful automatic mode applies the complete session map to fixed geometry.

The owner's request to fix the brand supersedes the preceding layout-only and
firmware-investigation steps. This change connects the eight reviewed exact
UUIDs to the addressed native backend. It does not claim physical testing.

## Changes

- Resolve the public model UUID through 09/82/01. Shared 372E:105C/106C IDs
  cannot select a model alone. Missing or unknown UUIDs do not fall back to
  another IPI model's matrix. Other unidentified addressed devices retain the
  existing capability-probed route.
- Generate exact physical-ID sets from all 18 hash-locked firmware images and
  compare them with the official geometry. Eight UUIDs select four merged ANSI
  presets, only after a connected native session reports its verified identity.
- Request every physical key explicitly through 09/83/00 in batches of up to
  nine. An empty request is not enumeration. Decode each six-byte record as
  big-endian ID16 + keycode32, including modifiers, Fn and unassigned keys.
- Read stored per-key released/bottom sensor endpoints through 09/94/05.
  Removed the captured W/A/S/D seeds chosen by a shared USB PID. Normalization
  uses each key's own device calibration, clamped to 0..1000. Invalid calibration
  rejects setup instead of fabricating a usable profile. No calibration-start,
  calibration-cancel, firmware-update or persistent-write commands were added.
- Validate checksum, header, command, exact length and requested unique IDs
  before applying each packet. Polling uses 09/94/02 and existing bounded
  timeout/reconnect handling. New setup reads have two 40 ms attempts per batch.
- Publish state per physical key and aggregate remapped aliases by maximum fresh
  depth. Releasing one alias cannot release another held key. Extended Fn 0x409
  is supported; unassigned, macro and compound bindings do not become guessed
  keyboard usages. Expired samples and disconnected sessions return neutral.
- Stop publishing connectivity before resetting state. Layout identity telemetry
  is emitted only for a connected session with a stable token.

The historical generic 82-entry table is deliberately retained for non-IPI
routes. Its discrepancy is documented at the table; none of the eight exact IPI
models uses it anymore. The physical layout legends remain factory legends;
normal keyboard remaps affect native HID publication, not factory geometry.

## Model scope

QBZ75, Aurora75, QBZ65, AURORA65, AURORA65W, RAIN65, Aurora75 PRO and flash68.
Exact UUIDs and firmware versions: [firmware evidence](IPI_FIRMWARE_REVERSE_2026-09-14.md).
Plus revisions and O3C remain excluded. Frozen IROK investigations are unchanged.

## Validation and evidence

- `ipi_native_test.cpp`: eight exact models/four identity groups, read-command
  allowlist, framing/checksum, malformed packet atomicity, zero/remapped/modifier/
  Fn assignments, calibration endpoints/midpoint, duplicate aliases, freshness,
  reset and concurrent publication PASS. Registered in the unified test runner.
- `test_ipi_firmware_records.py`: previous 108 serializer/lookup cases and
  108 raw/sample-helper cases still PASS across all 18 images.
- `test_ipi_calibration_records.py`: 156 batches cover every physical ID in all
  18 images. Actual firmware ID lookup and calibration response serialization
  execute with synthetic getter values. Exact big-endian ID/max/min records
  match. This does not execute USB entry, EEPROM calibration or the ADC scanner.
  Results: `docs/research/ipi-firmware-20260914/calibration-record-emulation.json`.
- Simulator profile/control transaction test PASS on its private desktop:
  editing, layout selection, persistence, failure atomicity, backend init count0.
  Evidence: `.local/ipi-native-profile-recheck/profile-test-result.txt`.
  The first invocation used different data and legacy roots and failed the
  overlay event check; a fresh run using the documented same-root invocation
  passed. No production UI changes were made to conceal that initial result.

## Remaining physical boundary

There is no connected IPI keyboard on this computer. Actual USB timing, analog
feel and simultaneous key operation must still be checked on hardware. The
AURORA65W keyboard image implements the protocol; its receiver forwarding has
not been established. Unsupported or incomplete responses remain unclaimed.

The native session reads the current base mapping and calibration at connection.
After changing mappings/calibration in the vendor app, reconnect the keyboard or
restart HallJoy to load the new session data. Live vendor configuration changes
and alternate Fn-layer mappings are not synchronized in this implementation.
Macro/compound outputs have no unique HID analog channel and are left unmapped.

## Delivery

Use the existing `build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe`.
The exact running executable was closed under the owner's standing instruction;
no additional tester folder or ZIP was created. Normal gamepad operation remains.
Backups: `.local/backups/ipi-native-support-20260914/` and the layout integration
backup `.local/backups/layout-integrate-t_c9f8k8/`. Program text, logs and comments
remain English. Final build/check evidence is recorded below after completion.

### Final result

Release build PASS; SHA256:
`435178ae6091bef246c3e9f7ac7c0b997e36747780cf91ce78a0b999e70153be`.
Simulator build PASS. Only the existing external ViGEm missing-PDB linker warning
remains; no compilation errors. Final static suite PASS.

All fixed portable/Windows C++ tests completed before the runner discovered the
already-executed header-based NA87 protocol test a second time and expected a
nonexistent source file. The runner now skips explicitly registered sources in
convention-based discovery. Remaining protocol tests were resumed and PASS;
this is combined coverage, not a claim that the interrupted command exited0.
Source, EXE and evidence hashes are in
`docs/research/ipi-firmware-20260914/native-support-validation.json`.

Logs: `.local/ipi-native-checks-3.txt`, `.local/ipi-remaining-protocol-tests.txt`,
`.local/ipi-static-final.txt`, `.local/ipi-release-build-final.txt`.
