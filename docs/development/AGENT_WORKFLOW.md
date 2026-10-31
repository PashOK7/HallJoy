# HallJoy agent workflow

Updated 2026-10-31 at the owner's request to adapt the workspace for Codex.
This organizes workflow/navigation. Runtime architecture remains in its existing
C++ modules. [Active context](../current/OWNER_CONTEXT.md) and both AGENTS.md apply.

## Start from the right root

The outer session folder contains HallJoy-main and a forwarding AGENTS.md.
Run project commands from HallJoy-main. This is an extracted working tree without
.git; do not initialize Git or use an old mirror as publication authority.
Read the nested AGENTS.md explicitly: instruction discovery without a Git root may
inspect only the session cwd. See [official Codex documentation](https://learn.chatgpt.com/docs/agent-configuration/agents-md).

```powershell
python tools/project_context.py --check
python tools/project_context.py --topic layouts
python tools/project_context.py --search "K552"
python tools/project_context.py --hash-exe --json
```

The utility uses the standard library and reads local files only. It performs no
network, process launch, HID access, file writes, dependency installation or repair.
Local release baseline is never a live/latest GitHub assertion. The script cannot
detect plugin availability; verify external connections through session tools.

Read compact OWNER_CONTEXT and docs/README fully, then the task route and exact
model/feature documents. Search the archived ledger for relevant decisions. Do not
reload 229 KB of mixed historical notes every task. Full archived text is retained,
with relative links rebased; exact originals are backed up. An unmentioned decision
is not cancelled. Resolve a real conflict by date and the underlying owner statement.

## Safe changes

Before every write, compare fresh target bytes/hash with the version read. Abort
on change, re-read and merge. For broad edits, snapshot every affected original
with relative path/size/SHA-256 under `.local/backups/<date-task>/`, verify the
copies and record rollback. Create new files exclusively. For replacement, write
a fresh temporary file, check the original again, then replace atomically.
Preserve encoding/newlines unless intentionally changed.

Use existing importers/generators for generated files. Keep scope focused; do not
reorganize runtime modules merely to suit an agent. `--topic` maps the existing
implementation instead of introducing competing architectural layers.

## Verification and documentation

Follow [TESTING](TESTING.md) and [BUILD](BUILD_README.txt). Required ordinary source
checks remain once each with cache. Owner evaluates visuals. Software PASS is not
device evidence. Update affected model/feature evidence and specific limitations.
OWNER_CONTEXT holds active durable rules, docs/README routes, PROJECT_LAYOUT paths.
Dated migration/release reports are evidence, not mandatory new-session reading.
Archive superseded long ledgers rather than deleting history. Do not prepend
every task to the compact index. No global Codex configuration is required here.

## Google Sheets connection

Canonical workbook `1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c`, Main / sheetId 0.
Use the connected Google Drive plugin's native Sheets tools. On 2026-10-31, live
metadata and Main!A1:C5 values/formats/validation were read successfully. No cells
were changed to test the connection; successful reading does not prove write access.
Do not put tokens or account secrets into the repository.

Before support edits, follow [SUPPORT_STATUS_SYNC](SUPPORT_STATUS_SYNC.md) and
[KEYBOARD_SHEET_RULES](KEYBOARD_SHEET_RULES.md), inspect current tool schemas,
read live bounds and locate brand + model. Do not hardcode remembered rows.
Structural audits require native metadata, not values-only exports. If a connector
cannot expose required metadata, resolve it or record precise pending sync; do not
claim a full audit from a sample. Never add cell notes/comments. Authorization for
corresponding support changes already exists; unrelated writes remain outside it.

The retained Claude Stop-hook is not assumed to run in Codex. Verify final local
links and give absolute file links in chat. Project docs use portable relative links.
