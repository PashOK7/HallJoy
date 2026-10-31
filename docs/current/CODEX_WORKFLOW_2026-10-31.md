# Codex workflow migration — 2026-10-31

Owner request: connect Google Sheets and adapt the project workflow for Codex.
Scope: instructions, navigation and read-only context tooling. Runtime C++ modules,
keyboard admission, support statuses, settings and public release remain unchanged.

## Connection

Google Drive plugin is connected. Native Sheets metadata read succeeded for
`1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c`, title "HallJoy supported keyboards",
Main / sheetId 0, 1300 rows, 26 columns, Europe/Moscow. Main!A1:C5 read returned
Brand / Model / Support status, values, user/effective formatting and strict status
validation. This proves target read access, not write access or a whole-Sheet audit.
No spreadsheet writes, notes, comments or status changes were made.

## Organization

- Both AGENTS.md now lead explicitly to project rules and the compact current
  context, including the non-Git outer-session-folder case. Existing operational
  rules are retained.
- OWNER_CONTEXT: 229339 -> 15672 bytes. Index: 22224 -> 4206 bytes.
  Complete prior texts are preserved under docs/archive; only relative Markdown
  targets were rebased. The exact original bytes are in the backup.
- The current index is a task/source map. Historical release logs and superseded
  assumptions remain available on demand instead of mandatory startup loading.
- BUILD_README / TESTING now describe actual local Python vs hosted CI, current
  ordinary gates, cache, Windows CI and publication. PROJECT_LAYOUT distinguishes
  historical migration restrictions from the standing build lifecycle.
- AGENT_WORKFLOW records safe writing, task routing and native Sheets access.
- tools/project_context.py reads current version, local release baseline, optional
  actual EXE hash and focused source paths. --check validates required files, active
  local links and context budgets. --search performs bounded literal search of model
  reports and the full archived owner ledger. No process, HID, network or write.
- Global Codex configuration and the existing Claude configuration are unchanged.

## Backup and rollback

`.local/backups/codex-workflow-20261031-175646/before.json` records every original
path, bytes and SHA-256. Copies preserve original paths; workspace-AGENTS.md is the
outer instruction file. Roll back only after comparing later changes, then restore
individual originals. New files: the two archives, AGENT_WORKFLOW, this report,
project_context.py and its test. The existing native runner gained two workflow
checks. Remove new files only after restoring referring docs and that runner.
Never restore the entire old tree blindly. No old history/tags were pushed.

## Validation

- Context/link/byte-budget check: PASS. The utility is included in the unified
  native runner along with its seven isolated tests; malformed/missing state,
  broken links, excessive context, absent Git/private baseline and read-only
  behavior are covered.
- Full native suite: NATIVE_EXIT=0, all static and portable C++ tests PASS,
  using the existing test cache and LLVM clang++. Log:
  `.local/codex-workflow-native-20261031.log`.
- Ordinary build: BUILD_EXIT=0, linked-image and embedded-license gates,
  profile transactions, layout catalog, yellow coverage 283,
  Supported layouts 84 (catalog=True; no live Sheet audit claimed), and startup
  recovery 18 scenarios PASS. Log: `.local/codex-workflow-build-20261031.log`.
- Publisher: candidate equals installed EXE; running HallJoy was left untouched.
- Archive verification: complete non-link text preserved; all 100 owner-ledger
  and 169 old-index link targets resolve to the same paths/fragments after rebasing.
- Original-file backup manifest: eight originals, every size/SHA-256 verified.
  The unified runner original was added before its small integration change.
- No keyboard status, runtime C++ source, application settings or Sheet data changed.
  No hardware/visual run, global Codex change, Git init, push or publication.
The pre-change delivery EXE SHA-256 is
`4e250e718b5a368511f451e441068441b632c5035fb212f2a1fd8d36ebd50114`.
No new publication or hardware/visual test is part of this task.
