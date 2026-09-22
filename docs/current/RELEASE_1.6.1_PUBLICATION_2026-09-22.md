# HallJoy 1.6.1 publication — 2026-09-22

Published stable/latest at 2026-09-22T06:31:15Z:
https://github.com/PashOK7/HallJoy/releases/tag/v1.6.1
Public metadata and asset digest readback verified. Linux CI passed; Windows CI
was still running at publication. It is additional clean-build validation, not
claimed passed. All required checks were completed locally, including final
production resource/telemetry policy. Owner questioned the redundant CI wait;
publication proceeded on the completed local evidence.
Owner authorized publication in this conversation.
Source target: 7e12928aacb6d1262a9689524e82b300062bdcab.
CI: https://github.com/PashOK7/HallJoy/actions/runs/35694457258.

Assets: HallJoy.exe, LICENSE, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt.
EXE product/file version: 1.6.1.0.
SHA256: d67c8f4d1128de4dac19405e93b4ea031f75b02bba7fbaa39da407ad5b7badf5.
Downloaded draft EXE matches local delivery byte-for-byte; GitHub digest agrees.

Validation: full native static/portable/Windows tests passed locally; rebuilt UAP
with locked overlays. Production-linked profile tests, layout catalog audit and
startup recovery pass. Final production build plus all five linked self-tests
pass; only the established third-party ViGEmClient missing-PDB warning remains.
No forced continuous telemetry; camera latency feature compiled out. Local K4
onboard build flag retained; custom firmware is not a release asset.

Release preparation caught obsolete single-HERO UUID and MINI60 Fn assertions,
updated to the exact implemented family/assignment contract. It also caught
unmerged identical AULA layouts: merged selector entries now preserve original
model aliases and edited profiles. Combined persistence names are filename-safe;
display names use slashes. Final profile/catalog tests pass, including four aliases.

README experimental table and patch notes contain the same 47 models / six
alphabetically sorted brands. Live Sheet re-read before publication (Main A1:C540,
463 model rows); all 47 yellow model/status/color pairs match the generated runtime
catalog. Existing supported MINI60, Wooting and Keychron changes since 1.6.0 were
reconciled with source/docs; no new status change or Sheet edit was needed.
Readback retained locally at .local/release-1.6.1-sheet-complete.json.

Public patch notes: RELEASE_NOTES_v1.6.1.md, no Known limits section or internal
reporting instructions. Local downloads, outputs, credentials, full device backups
and build products are excluded from source publication.
