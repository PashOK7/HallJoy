"""Exercise actual Windows migration, without starting UI or keyboard backends."""
from pathlib import Path
import subprocess
import tempfile
from consolidate_legacy_migrations import consolidate

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/bin/AnalogSimulator/Release/x64/HallJoyV14Simulator.exe'


def run(source, target, extra=(), expected=0):
    source.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(EXE), '--halljoy-test-data-root', str(target),
                    '--halljoy-test-legacy-root', str(source),
                    '--halljoy-test-forbid-backend-init',
                    '--halljoy-test-storage-initialize-only', *extra], timeout=20)
    assert result.returncode == expected, result.returncode


with tempfile.TemporaryDirectory(prefix='HJMigrationClutter-') as temporary:
    root = Path(temporary)
    target = root / 'target'
    for i in range(12):
        run(root / f'empty-{i}', target)
    assert not list(target.rglob('*.ini')), 'Empty sources left markers'
    assert not (target / 'MigrationBackups').exists(), 'Empty backup tree'
    assert not (target / '.internal').exists(), 'Empty migration ledger'

    source = root / 'real-source'
    source.mkdir()
    (source / 'settings.ini').write_bytes(b'[Main]\r\nPollingMs=3\r\n')
    (source / 'bindings.ini').write_bytes(b'legacy binding fixture')
    (target / 'bindings.ini').write_bytes(b'CURRENT USER DATA')
    run(source, target)
    assert (target / 'settings.ini').read_bytes() == (source / 'settings.ini').read_bytes()
    assert (target / 'bindings.ini').read_bytes() == b'CURRENT USER DATA'
    backups = list((target / 'MigrationBackups').rglob('*.ini'))
    assert len(backups) == 1 and backups[0].name == 'settings.ini', backups
    assert backups[0].read_bytes() == (source / 'settings.ini').read_bytes()
    ledger = target / '.internal/migrations.ini'
    assert ledger.is_file() and not list(target.glob('.migration-from-exe-*'))
    first_ledger = ledger.read_bytes()

    for i in range(8):
        duplicate = root / f'other-exe-folder-{i}'
        duplicate.mkdir()
        (duplicate / 'settings.ini').write_bytes(b'DO NOT OVERWRITE USER SETTINGS')
        run(duplicate, target)
    assert len(list((target / 'MigrationBackups').iterdir())) == 1
    assert len(list((target / '.internal').iterdir())) == 1
    assert ledger.read_bytes() != first_ledger, 'New sources were not recorded'
    # Earlier sections must survive later atomic ledger updates.
    (target / 'settings.ini').unlink()
    run(source, target)
    assert not (target / 'settings.ini').exists(), 'Old source replayed after ledger update'
    run(root / 'other-exe-folder-0', target)
    assert not (target / 'settings.ini').exists(), 'Skipped import replayed'
    assert not list(target.rglob('*.halljoy-new-*')), 'Transaction leftovers'
    assert (source / 'bindings.ini').read_bytes() == b'legacy binding fixture'

    # Convert one genuine ledger entry back to the historical marker format.
    # The production reader must honor it both before and after consolidation.
    from consolidate_legacy_migrations import read_ini
    ini = read_ini(ledger.read_bytes())
    section = next(s for s in ini.sections() if s.startswith('Source-'))
    old_source = Path(ini[section]['SourceRoot'])
    suffix = section.removeprefix('Source-')
    legacy_target = root / 'legacy-marker-target'
    legacy_target.mkdir()
    marker = legacy_target / f'.migration-from-exe-{suffix}.ini'
    marker.write_text('[HallJoyPersistence]\nSchemaVersion=1\nKind=DataMigration\n'
                      f'[Migration]\nSourceRoot={old_source}\nComplete=1\n', encoding='utf-16')
    old_backup = legacy_target / 'MigrationBackups' / f'legacy-{suffix}'
    old_backup.mkdir(parents=True)
    (old_backup / 'unique.ini').write_bytes(b'UNIQUE RECOVERY DATA')
    run(old_source, legacy_target)
    assert not (legacy_target / 'settings.ini').exists()
    consolidate(legacy_target, apply=True)
    assert not marker.exists() and not old_backup.exists()
    run(old_source, legacy_target)
    assert not (legacy_target / 'settings.ini').exists(), 'Consolidation lost replay guard'
    import zipfile
    with zipfile.ZipFile(legacy_target / '.internal/legacy-migration-recovery.zip') as archive:
        assert archive.read(f'MigrationBackups/legacy-{suffix}/unique.ini') == b'UNIQUE RECOVERY DATA'

    for stage in ('prepare', 'write', 'flush', 'validate', 'replace'):
        fault_source = root / f'fault-source-{stage}'
        fault_source.mkdir()
        original = b'[Main]\r\nPollingMs=3\r\n'
        (fault_source / 'settings.ini').write_bytes(original)
        fault_target = root / f'fault-target-{stage}'
        run(fault_source, fault_target, [f'--halljoy-test-persistence-failure-{stage}'], expected=1)
        assert (fault_source / 'settings.ini').read_bytes() == original
        assert not (fault_target / 'settings.ini').exists()
        assert not (fault_target / '.internal/migrations.ini').exists()
        assert not list(fault_target.rglob('*.halljoy-new-*'))

print('MIGRATION_CLUTTER_WINDOWS_TEST=PASS: empty sources, no-op backups, single ledger, multi-source replay, source preservation')
