# Migration clutter diagnosis — 2026-09-08

Read-only runtime-data audit: 64 .migration-from-exe-*.ini markers, 64
MigrationBackups subdirectories, 60 empty. The four populated directories contain
20 files totalling 27,948 bytes. 36 markers point at temporary-directory sources,
including HallJoy diagnostic smoke/abnormal-exit test roots.

app_paths.cpp hashes the absolute executable directory, not the migrated data.
Every new executable folder is therefore a new migration source. It creates a
backup directory and completion marker even when CollectLegacyFiles is empty.
For populated sources it backs up source files before checking whether the
destination already exists. Existing destination wins, but redundant source
copies still accumulate. Moving/rebuilding the executable in the same folder
does not alone cause a new marker; distinct launch folders do.

Source-specific markers originally provided transactional completion/replay
protection and preserved legacy data. Deleting them blindly can repeat migration.
Tests launching binaries from temporary folders have also reached real user
storage instead of an isolated data root; this is a test isolation defect, not
merely cosmetic clutter.

Recommended follow-up: isolate every test launch, consolidate internal migration
state outside the user-facing root, avoid empty/no-op backups, and design bounded
deduplicated recovery retention without deleting unique legacy data. Existing
markers must be consumed/imported so upgrades do not replay old imports. Do not
infer that every old backup is disposable. No runtime files changed or removed
during this diagnosis.

## Implemented correction

Empty sources now produce neither a marker nor a backup directory. Existing
destination files are checked before backup creation: skipped imports preserve
the source but create no redundant backup. Real imports retain transactional
copy validation and recovery copies. Nonempty sources share the atomic internal
`.internal/migrations.ini` ledger, including skipped imports, so deleting a
destination later does not resurrect old source files. Historical root markers
remain readable. Ledger updates preserve earlier entries and refuse an invalid
or unreadable existing ledger rather than silently replacing its history.

Four temporary-executable diagnostic/abnormal-exit runners now explicitly use
portable storage in their private temporary directories. The production smoke
runner already copies user state into an isolated portable runtime.

`tools/test_migration_clutter.py` invokes real simulator-linked storage code,
without UI or backend initialization. Covers 12 empty source directories, real
imports, destination conflicts, 8 additional no-op sources, earlier-entry replay,
legacy marker compatibility, archive consolidation and all five atomic failure
stages. `tools/consolidate_legacy_migrations.py` is an explicit offline maintenance
tool (read-only unless --apply): validates marker hashes, archives and verifies
every byte before committing the consolidated ledger and deleting exact old
artifacts. It never touches live settings/profiles, overwrites an existing
recovery ZIP, or recursively deletes a directory.

Validation so far: production Release build, profile transaction/startup
preservation tests, static suite and new filesystem migration tests PASS.
The older UI simulator migration runner failed its unrelated requirement for
`vigem-output/publication.applied`; it is not counted as passed. Filesystem and
five-stage failure checks are independently covered by the new non-UI test.

Live offline consolidation completed: 64 legacy markers and 20 backup files
(including all 60 empty backup roots) moved into the byte-verified
`.internal/legacy-migration-recovery.zip`; replay guards committed to the single
ledger before removal. All non-migration user files retained identical SHA256.
FactoryResetBackups and settings.ini.pre-bundle.bak were deliberately untouched.
Production and simulator rebuilt after the final ledger error-handling change;
profile, clutter/fault and static checks passed again. Deployed Release SHA256:
`1C0EAC56FE98AC1B0D66339E60C04B89B1C1A18AF812CDE2D0C17C78B509DC87`.
Previous executable and source checkpoint are in
`.local/backups/migration-clutter-20260908/`.
