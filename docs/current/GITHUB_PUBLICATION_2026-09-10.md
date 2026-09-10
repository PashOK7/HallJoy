# GitHub publication — 2026-09-10

Owner authorized pushing current sources and publishing 1.5.0. First source
commit abd0eeb69a004acb74edb3448ee7882cf0253127 reached main. CI run 34510146142
caught two clean-checkout problems hidden by local working files:

- A historical nested ignore rule excluded three frozen IROK implementation/test
  files required by source audits. Explicit exceptions ship them without enabling
  the frozen backend.
- Windows Git checkout rewrote line endings of byte-hashed manufacturer sources.
  .gitattributes now preserves exact bytes across platforms. Integrity checks
  remain strict; hashes were not weakened or blindly regenerated.

These publication corrections do not change the validated release EXE. A clean
Git checkout, not only the original working tree, must pass checks before release.

Actions uploads now occur only for workflow_dispatch and expire after three days.
Automatic push/PR compilation and tests still run. No paid budget changed and no
old artifact/cache/release deleted. Across the five owner repositories, only
HallJoy currently has non-expired Actions artifacts: 12, 14,243,850 bytes. Nova's
81 historical records total about 14.5 GiB but all are expired; their metadata
must not be counted as current storage. All inspected caches are empty. Billing
API is unavailable with current OAuth scopes, so the account email's current
500 MB usage cannot be reconciled from these artifact listings alone.

Next CI run 34510415822 passes the corrected source checks but reveals the
ViGEm child transport ABI test was incorrectly scheduled on Linux. It includes
the actual Windows SDK header (pshpack1.h), even with fake device calls. The test
now runs in the mandatory Windows test group; portable protocol/state tests stay
on Linux. No fake replacement SDK, skipped Windows coverage or EXE change.

Run 34510636301 also identifies bounded_ini_numeric as Windows-only: its newer
tests exercise GetPrivateProfileString/temporary files, not just numeric parsing.
Moved that entire test to the same mandatory Windows group, retaining all cases.
