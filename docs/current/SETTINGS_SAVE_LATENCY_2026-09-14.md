# Settings save latency after layout selection — 2026-09-14

Owner reported about one second of UI blocking after selecting a keyboard layout.
The production event calls Global_RequestSave, then the main UI saves the active
profile. Full settings/profile writes still issued hundreds of individual Win32
INI writes, including every gamepad binding. Catalog initialization improvements
did not optimize this separate path.

## Implementation

Full settings and named-profile serialization now share a scoped in-memory INI
writer. Settings, overlay preferences, per-key options, chosen layout and bindings
contribute to the same document; the transaction writes the UTF-16LE file once.
The scope is thread-local and restricted to the exact transaction temporary path.
Nested scopes restore the prior writer, mismatched paths fail, and callers outside
a batch retain direct Win32 behavior. No worker reads mutable UI state and no
combobox lockout, timer delay or debounce was added to conceal the work.

The existing transaction still flushes, validates bindings and replaces atomically.
Window-only and overlay-only updates preserve the existing document through their
previous update path. Layout preset storage retains the previous optimization.
A settings/save.complete diagnostic event reports duration_ms, kind and success,
without file paths, names of user profiles or key contents.

## Measurements and validation

Same isolated Release/x64 simulator, original and new settings serializer, default
settings and four gamepad binding sets. The original full save took 1117.383 ms;
the new implementation measured 26.634 ms initially and 17.159 ms in the final run.
These are local save timings, not a guarantee of end-to-end UI latency on any disk.
The original/new complete documents match semantically across all sections/keys.

PASS: all static audits, scoped writer unit tests, full production-linked profile
transaction suite, and the layout storage regression suite. The profile suite
covers nondefault settings, full-domain binding CSV, extended/per-key settings,
100 concurrent loads with no mixed state, all five transaction failure stages,
window/profile isolation, overlay persistence and real editor/picker events on a
private test desktop. No backend initialization occurred.

The first expanded test run failed at editor graphics initialization because the
isolated entry preceded normal GDI+ setup. The test-only entry now initializes GDI+
for that suite, and the full rerun passes. No production graphics change was needed.
The agent did not visually run HallJoy or interact with the owner's keyboard/app.

## Why model rows remain separate

Read-only checks used copies of 16 existing Keychron layout files in a test root.
K2/K3 ANSI and ISO still load as the combined models; the separate K2 and K3 rows
are distinct JIS geometries. Q1/Q1 8K also have distinct JIS variants. Those were
explicitly excluded from the previous geometry consolidation.

The owner's old Q1 HE ANSI - Imported file also remains a separate override: its
UniformGap is 6 instead of 8 (UniformSpacing is currently 0). The existing merge
policy preserves even inactive spacing metadata as a user edit. Brand inference
and geometry revision are not evidence of a new geometry difference. This pass
does not discard the stored override or broaden the approved geometry aliases.
No existing user layout files were modified or deleted.

## Artifact

Owner correction: HallJoy was closed as explicitly requested. The EXE and PDB
were installed at the original release path; SHA256 matches the value below.
Optimized was removed. The staging account below is historical. Current artifact:
`build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe`.


The existing main-path HallJoy.exe is running and locked; the initial linker could
not replace it (LNK1104). The owner process was left running. The same native NA87
release configuration successfully builds to the adjacent Optimized folder:

build/bin/IrokNa87Diagnostic/Release/x64/Optimized/HallJoy.exe
SHA256: 733e002c73759577dd3f50cbfcb4c48c73c199c4081584f380d0960546e53ad3

Close the current application before launching this executable. Logs are adjacent
to the executable as usual. The old main-path binary has not been updated.

Evidence: build/evidence/settings-batch-20260914/final/results.json and
layout-regression/results.json. Build log: .local/settings-batch-release-staged.log.
Audit log: .local/settings-batch-static-final.log. Scoped-writer unit log:
.local/settings-batch-unit.log. Backups: .local/backups/settings-batch-20260914/.

Repeat the full file/control tests using a fresh explicit simulator data root and
--halljoy-test-layout-storage --profile-storage-verify --halljoy-test-forbid-backend-init,
with both --halljoy-test-data-root and --halljoy-test-legacy-root set to that root.
For a settings timing run substitute --settings-storage-verify. These flags are
simulator-only and absent from the delivered release executable.
