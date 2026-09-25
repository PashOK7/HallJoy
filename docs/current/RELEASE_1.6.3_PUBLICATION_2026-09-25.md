# HallJoy 1.6.3 release preparation — 2026-09-25

Owner authorized publication of all accumulated changes, with local Windows
validation and no hosted Windows run. Version1.6.3.0; compact five-item notes,
updated README with compact brand lines and Google Sheet navigation.

Fresh Google Sheet metadata: Main/id0,1275 rows. A1:C850 read on release day:
646 model rows,251 experimental,63 supported (45 plain,17 custom firmware,
1 BR-tested), remaining332 research/unsupported. All251 yellow entries exactly
match runtime catalog; every model's strict dropdown and base/effective colors
agree. No Sheet writes needed. Snapshot .local/release163-sheet.json.
All model additions since1.6.2 (49 yellow ->251 yellow), reviewed USB aliases,
layout additions and communication advisory are included; no new physical tests.

Validation: final ordinary Release and six executable gates PASS; clean source
mirror full static audit PASS; linked profile/layout checks and18 recovery
scenarios PASS. Full native portable/Windows checks passed during the immediately
preceding USB identity task; final three-alias regression also passed. No runtime
code changed since then except release version. Logs: .local/release163-build.log,
release163-clean-static.log,release163-profile.log; prior identity-full-checks-
1790269314.log plus identity-resumed-checks-1790269517.log.

EXE SHA256: 73cbc3ba203f96366eef95ae0cdd83711775718a8ab75c3192579aaabb0054c3.
Assets: standalone HallJoy.exe, LICENSE, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt.
No diagnostic build flags or forced logging. Source mirror excludes local output,
node_modules, credentials, private Pwnage correspondence and generated binaries.
Vendor research source bytes remain hash-pinned and unchanged by formatting.
Publication verification will be appended after upload.
