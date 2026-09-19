# Layout storage optimization — 2026-09-14

Owner requested a structural optimization of slow startup. Application UI, logs,
and code comments remain English. No visual/device run was performed; the owner
judges appearance. Existing running HallJoy and user settings were not modified.

## Cause and new lifecycle

The old startup registered built-ins, loaded preset files, then materialized every
missing built-in INI synchronously. Both reading and writing used per-field Win32
profile calls, repeated hundreds of times per keyboard. Layout geometry is small;
repeated file processing and transactional writes were the expensive work.

Built-ins now remain in memory. Files in Layouts are optional user overrides and
load after built-ins, preserving user precedence. New shipped layouts appear
without any first-launch materialization. Existing legacy files remain readable;
startup does not rewrite or delete them. Known geometry migrations run in memory
and persist only on an explicit save. Existing merge/identity rules remain intact.

A section snapshot replaces per-field reads. Win32 still handles ANSI/UTF-16 INI
acquisition; bounded parsing, case-insensitive lookup, first duplicate precedence,
quotes, Unicode and limits remain checked. The existing read lease prevents file
replacement/writes during acquisition. Section buffers grow within a size limit.

Saving constructs a complete UTF-16LE document in memory and writes it once to
the transaction-owned temporary file. The existing atomic transaction still
flushes, reads back, validates and replaces the destination. The shared loader
validates persisted geometry and metadata against the intended state before
commit. Empty-brand inference applies to normal loads, not raw save validation.
The editor continues to save files when the user creates or changes a layout.

Diagnostic logs now contain one layout-catalog/init.complete event with preset
count, duration_ms and builtin_files_created=0; no layout names or paths are added.

## Measurements

Isolated MSVC Release/x64 simulator processes, same PC, same 63-preset catalog,
separate explicit test roots, no UI, logger, instance ownership or hardware startup.
The baseline executable contains the original storage implementation plus the
same timing entry point. These are individual local measurements, not universal
latency guarantees and not total application startup times.

| Catalog scenario | Before | After |
| --- | ---: | ---: |
| Initially empty directory | 67,045.219 ms, 63 files created | 7.911 ms, zero files created |
| Existing 63 INI files | 1,309.537 ms | 33.900 ms |
| Save one edited layout | Not benchmarked | 4.418 ms |

Final restart test restored Unicode labels/brand and edited coordinates in a new
process: 7.996 ms, one custom file. Hashes of all 63 copied legacy files were
unchanged by optimized initialization. No OS cache purge was performed.

## Validation

- Complete static audit runner and all manufacturer layout pipeline checks: PASS.
- MSVC simulator and normal NA87 diagnostic Release/x64 builds: PASS. Only existing
  ViGEm missing-PDB linker warning; no new compile errors.
- Windows section semantics compared to GetPrivateProfileStringW: PASS, including
  a large section requiring buffer growth, Unicode, quotes, duplicate keys,
  numeric bounds, reader locking and truncation on shorter writes.
- Bounded INI real-file/capacity tests and layout editor model tests: PASS.
- Production layout first-run/identity/merge/migration/shape tests: PASS in the
  isolated simulator role.
- Production create/edit/save/delete, Unicode roundtrip and process restart: PASS.
- Injected Prepare, Write, Flush, Validate and Replace failures: PASS; previous
  layout remains readable with its original content.
- Runtime invariant audit: startup path contains no preset saves/materialization.

The simulator-only --halljoy-test-layout-storage entry runs before ordinary
application initialization and requires explicit data and legacy test roots.
It is absent from the delivered release executable. Repeat file tests with:

```powershell
python tools/test_layout_storage_windows.py --exe build/bin/AnalogSimulator/Release/x64/HallJoyV14Simulator.exe --evidence build/evidence/layout-storage-new-run --legacy-layouts build/evidence/layout-storage-20260914/baseline/Layouts
python tools/run_native_backend_checks.py --static-only
```

Use a new evidence directory for each run. This developer test is not a tester
package; the owner receives the normal HallJoy.exe with gamepad/NA87 support.

## Artifact and evidence

EXE: build/bin/IrokNa87Diagnostic/Release/x64/HallJoy.exe
SHA256: 1712a5191c89166ad6c90555b24ad055042f1f6c0e289f75264686d8f8812d45

Evidence: build/evidence/layout-storage-20260914/final/results.json,
baseline-first-result.txt and baseline-warm-result.txt in the parent directory.
Build/audit logs: .local/layout-storage-*-final.log.
Backup: .local/backups/layout-storage-20260914/ (original source and EXE with
manifest hashes; additional audit/doc backups). No ZIP was produced.

Earlier MADLIONS documentation describes the prior EXE and original startup
behavior; this document supersedes that storage behavior and artifact hash.
Total visible startup still requires an owner run; device discovery and other
initialization stages were not part of these timings.
