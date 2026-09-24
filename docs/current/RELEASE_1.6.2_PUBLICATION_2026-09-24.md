# HallJoy 1.6.2 publication — 2026-09-24

Published stable/latest at 2026-09-24T08:05:53Z:
https://github.com/PashOK7/HallJoy/releases/tag/v1.6.2
Owner authorized publication and requested no hosted Windows checks.
Source/tag target: 395fdbe44351d88e2090f71bb51b7a9166f88a13.

Assets: HallJoy.exe, LICENSE, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt.
EXE version1.6.2.0, size9613312 bytes, SHA256:
ceb47c21bfb7ff12efccd6e15853c8fddc6a2793e8dfc95f39bc121dbe4a0183.
Downloaded draft EXE equals local delivery byte-for-byte; published asset digest
and stable/non-draft metadata read back. This is the same EXE accepted by owner.
No forced logging or firmware changes.

Validation: full local native static/portable/Windows suite PASS
(.local/release162-publication-checks.log); pre-release UI static audit PASS;
production-linked profile/layout/remap tests and18 startup recovery scenarios
PASS (.local/layout-reconnect-tests.log); ordinary build and6 linked gates PASS.
Linked production telemetry policy PASS. Published source matches workspace.
GitHub run35972870509: portable PASS, windows-release SKIPPED, verified through
job metadata. Windows builds are now manual workflow_dispatch only; portable
CI remains automatic. Full build script retains K4 onboard and the same linked
gates as incremental builds; this release used the verified incremental output,
not a newly executed full dependency rebuild. Build script PowerShell parse PASS.

Release covers R85 HE/AJAZZ AK820 MAX RGB support, three experimental native
models, shared-access Shark fix, gamepad restart and automatic layout fixes.
Fresh Main A1:C650 read confirms R85 HE/AJAZZ RGB Supported green, all49 yellow
models match runtime and README/hardware inventory. Dropdown validation retained;
no Sheet edits required. Snapshot .local/release162-publication-sheet.json.
See RELEASE_1.6.2_PREPARATION_2026-09-24.md for exact models/evidence.

Source excludes generated outputs, credentials, private Pwnage correspondence
and device backups. Vendor protocol/layout snapshots retain exact hash-locked
bytes, including original whitespace. Project changes pass whitespace review
with CRLF allowed. No new physical keyboard tests claimed.
