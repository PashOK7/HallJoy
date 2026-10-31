# ATTACK SHARK X82 HE: "stopped recognizing again" (2026-10-26)

Tester report (Discord, 2026-10-26 21:06): "It stopped recognizing my keyboard again", log
`HallJoy (23).log` (HallJoy 1.6.12.0, 18:05:54 UTC). Same tester and keyboard as the working log
`HallJoy (20).log` (2026-10-24, board 3086 on 3151:5029, connected, 83 keys).

## Log

- `3151:5029` "Gaming Keyboard" present (wired), plus a `1D57:FA60` "2.4G Wireless Device".
- The log carries the RongYuan identity budget (`rongyuan.identity_waiting value=3` after three
  passes), so it comes from the post-1.6.12 tester build, not from the published 1.6.12; the
  version string of local builds stays 1.6.12.0.
- RongYuan backend: `identity_pair_rejected value=3086` (expected: board 3086 belongs to the
  ATTACK SHARK backend), `admission_failed error=13`, stops after 3 passes. No
  `rongyuan.shared_open`: its exclusive open of the control collection succeeded, so no other
  program held the keyboard.
- ATTACK SHARK worker, one inventory pass at the same millisecond window (211 796 ms):
  six collections described, the seventh failed: `shark.metadata_open_failed error=32`
  (ERROR_SHARING_VIOLATION). The missing one is the vendor control collection `FFFF:0002`
  with 65-byte feature reports (present in log 20). Result `shark.eligible value=0`,
  `shark.state value=1`, then nothing: the worker waits for a device change.
- Log 20 (working): the same inventory found `FFFF:0002` feature 65, `shark.eligible value=1`,
  connected.

## Cause

`rongyuan_stream_backend.cpp` opens the `FFFF:0002` control collection exclusively
(share mode 0) for its identity query. While that handle exists, even a metadata-only
`CreateFileW(path, 0, FILE_SHARE_READ|FILE_SHARE_WRITE)` from the ATTACK SHARK worker fails with
a sharing violation. On log 20 the passes did not overlap; on log 23 they did. After an empty
inventory, `Capture()` waited for a topology change (replug), so one collision at start-up left
the keyboard unrecognized for the whole session. Intermittent by nature, which matches
"again".

## Fix (`attackshark_pro_diagnostic.cpp`)

- `Find()` counts 3151 collections whose metadata open fails with
  ERROR_SHARING_VIOLATION or ERROR_ACCESS_DENIED (`busy`, also in the inventory line).
- `Capture()`: if no eligible collection was found and `busy > 0`, inspect again every second
  (metadata only, no commands are sent) instead of waiting for a device change. A topology
  change or stop request ends the wait at once. The RongYuan probe gives up after three passes,
  so the collection frees within a few seconds.
- Logging bounded by content, not time: `shark.inventory_busy value=<collections>` once per
  episode, repeated busy passes are silent, `shark.inventory_busy_cleared value=<passes>` and the
  full inventory of the clearing pass once.
- Unchanged: the exclusive session open, identity proof and the `candidates_unavailable` retry
  (3 s) for a busy session open.
- Self-test `--halljoy-shark-self-test` (ordinary failure log) parses the new lines into the
  real log writer and requires both events.

Backup: `.local/backups/x82he-busy-inventory-20261026/`.

## Validation

- `run_native_backend_checks.py --require-compiler`: NATIVE_EXIT=0, no assertion failures
  (`.local/x82he-busy-native-20261026.log`).
- `build_release.ps1`: BUILD_EXIT=0, all candidate checks including `--halljoy-shark-self-test`
  (exit code gated). Local EXE SHA-256
  `1e3b182fb6e78bc9ed326e36724910280725d288fe037b08369d4f82dcfe1799`
  (`.local/x82he-busy-build-20261026.log`); version string 1.6.13.0. Not published.
- Release notes: entry in `docs/releases/RELEASE_NOTES_NEXT.md`.

## Limits

- Not reproduced on hardware; the collision is established from the log (sharing violation on
  the exact collection, no foreign holder) and the open modes in the source.
- Released 1.6.13 (published 2026-10-26 17:40 UTC) does not contain this fix.
