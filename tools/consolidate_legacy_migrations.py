"""Explicit, offline maintenance: archive old migration artifacts, retain replay guards.

Default is read-only. --apply requires HallJoy to be closed. No settings/profile
files are touched. The verified ZIP retains every removed file and empty folder.
"""
import argparse
import configparser
import ctypes
import io
from pathlib import Path
import zipfile


def read_ini(data):
    text = data.decode('utf-16' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
    ini = configparser.ConfigParser(interpolation=None)
    ini.optionxform = str
    ini.read_string(text)
    return ini


def ordinary(path, root):
    if not path.resolve().is_relative_to(root.resolve()):
        raise RuntimeError(f'Path outside data root: {path}')
    for part in (path, *path.parents):
        if part.exists() and part.stat().st_file_attributes & 0x400:
            raise RuntimeError(f'Reparse point refused: {part}')
        if part == root:
            break


def consolidate(root, apply=False):
    root = root.absolute()
    ordinary(root, root)
    markers = sorted(root.glob('.migration-from-exe-*.ini'))
    if not markers:
        print('No legacy migration markers. Nothing to do.')
        return
    internal = root / '.internal'
    ledger = internal / 'migrations.ini'
    archive = internal / 'legacy-migration-recovery.zip'
    for path in (internal, ledger, archive):
        ordinary(path, root)
    if archive.exists():
        raise RuntimeError('Recovery archive already exists; refusing to overwrite it.')
    original_ledger = ledger.read_bytes() if ledger.exists() else None
    merged = read_ini(original_ledger) if original_ledger is not None else configparser.ConfigParser(interpolation=None)
    merged.optionxform = str
    if original_ledger is not None and (merged['HallJoyPersistence']['Kind'] != 'DataMigrationLedger' or
                                       merged['HallJoyPersistence']['SchemaVersion'] != '1'):
        raise RuntimeError('Unrecognized existing ledger')
    merged['HallJoyPersistence'] = {'SchemaVersion': '1', 'Kind': 'DataMigrationLedger'}
    snapshots = {}
    directories = set()
    for marker in markers:
        ordinary(marker, root)
        data = marker.read_bytes()
        old = read_ini(data)
        if old['HallJoyPersistence']['Kind'] != 'DataMigration' or old['HallJoyPersistence']['SchemaVersion'] != '1' or old['Migration']['Complete'] != '1':
            raise RuntimeError(f'Invalid completion marker: {marker}')
        source = old['Migration']['SourceRoot']
        folded = ctypes.create_unicode_buffer(source)
        ctypes.windll.user32.CharLowerBuffW(folded, len(source))
        value = 1469598103934665603
        units = folded.value.encode('utf-16-le')
        for offset in range(0, len(units), 2):
            ch = int.from_bytes(units[offset:offset + 2], 'little')
            value = ((value ^ ch) * 1099511628211) & 0xffffffffffffffff
        suffix = f'{value:016X}'
        if marker.name != f'.migration-from-exe-{suffix}.ini':
            raise RuntimeError(f'Source/hash mismatch: {marker}')
        section = f'Source-{suffix}'
        if section in merged and dict(merged[section]) != {'SourceRoot': source, 'Complete': '1'}:
            raise RuntimeError('Conflicting replay guard')
        merged[section] = {'SourceRoot': source, 'Complete': '1'}
        snapshots[marker] = data
        backup = root / 'MigrationBackups' / f'legacy-{suffix}'
        ordinary(backup, root)
        if backup.exists():
            directories.add(backup)
            for item in backup.rglob('*'):
                ordinary(item, root)
                if item.is_dir():
                    directories.add(item)
                else:
                    snapshots[item] = item.read_bytes()
    print(f'Validated {len(markers)} markers; archive {len(snapshots)} files and {len(directories)} directories.')
    if not apply:
        return
    internal.mkdir(exist_ok=True)
    # Exclusive creation: an earlier recovery archive can never be overwritten.
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as output:
        for path, data in snapshots.items():
            output.writestr(path.relative_to(root).as_posix(), data)
        for path in directories:
            output.writestr(path.relative_to(root).as_posix() + '/', b'')
        if original_ledger is not None:
            output.writestr('previous-migrations.ini', original_ledger)
    with zipfile.ZipFile(archive) as verified:
        if verified.testzip() is not None:
            raise RuntimeError('Archive CRC verification failed')
        for path, data in snapshots.items():
            if verified.read(path.relative_to(root).as_posix()) != data:
                raise RuntimeError('Archive content verification failed')
    if (ledger.read_bytes() if ledger.exists() else None) != original_ledger:
        raise RuntimeError('Ledger changed during maintenance')
    for path, data in snapshots.items():
        if path.read_bytes() != data:
            raise RuntimeError(f'File changed during maintenance: {path}')
    stream = io.StringIO()
    merged.write(stream, space_around_delimiters=False)
    encoded = stream.getvalue().encode('utf-16')
    temporary = internal / 'migrations-consolidation.tmp'
    with temporary.open('xb') as output:
        output.write(encoded)
        output.flush()
        import os
        os.fsync(output.fileno())
    if read_ini(temporary.read_bytes()) != merged:
        raise RuntimeError('Ledger validation failed')
    temporary.replace(ledger)
    # Replay guards are committed before removing legacy markers.
    for path, data in snapshots.items():
        ordinary(path, root)
        if path.read_bytes() != data:
            raise RuntimeError(f'File changed before removal: {path}')
        path.unlink()
    for path in sorted(directories, key=lambda item: len(item.parts), reverse=True):
        ordinary(path, root)
        path.rmdir()  # Never recursive; unknown/new files prevent removal.
    backup_root = root / 'MigrationBackups'
    if backup_root.exists() and not any(backup_root.iterdir()):
        backup_root.rmdir()
    print(f'CONSOLIDATION=PASS; all removed files recoverable from {archive}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data_root', type=Path)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    consolidate(args.data_root, args.apply)
