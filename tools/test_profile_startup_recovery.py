"""Real startup and shutdown, isolated storage, no UI or keyboard/gamepad init."""
import argparse
import configparser
import pathlib
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True)
    args = parser.parse_args()
    root = pathlib.Path(tempfile.mkdtemp(prefix='HallJoyRecovery-'))
    print(f'Evidence: {root}', flush=True)

    def run(folder, *extra):
        process = subprocess.run([str(pathlib.Path(args.exe).resolve()),
            '--halljoy-test-data-root', str(folder), '--halljoy-test-legacy-root', str(folder),
            '--halljoy-test-forbid-backend-init', '--halljoy-test-profile-startup-only', *extra],
            timeout=30, check=False)
        assert process.returncode == 0, (folder, process.returncode)

    def fixture(name, settings=None, bindings=None):
        folder = root / name
        folder.mkdir()
        for leaf, data in [('settings.ini', settings), ('bindings.ini', bindings)]:
            if data is not None:
                (folder / leaf).write_bytes(data)
        return folder

    def values(folder):
        raw = (folder / 'settings.ini').read_bytes()
        ini = configparser.ConfigParser(interpolation=None, strict=False)
        ini.read_string(raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8-sig'))
        return ini

    def backup(folder, leaf, expected):
        files = list((folder / '.internal' / 'ProfileRecovery').glob(f'*/{leaf}'))
        assert len(files) == 1 and files[0].read_bytes() == expected, files

    def stable(folder):
        before = {p.relative_to(folder): p.read_bytes() for p in folder.rglob('*.ini')}
        run(folder)
        after = {p.relative_to(folder): p.read_bytes() for p in folder.rglob('*.ini')}
        assert before == after, f'Repeated startup changed INIs: {folder}'

    settings = b'[Main]\r\nPollingMs=3\r\n'
    bindings = b'[Pad1_Axes]\r\nLX_Plus=7\r\n'
    bad = b'[Pad1_Axes]\r\nLX_Plus=65543\r\n'
    fresh = fixture('fresh')
    run(fresh)
    healthy_bundle = (fresh / 'settings.ini').read_bytes()
    assert not (fresh / '.internal' / 'ProfileRecovery').exists()
    stable(fresh)

    legacy = fixture('valid-legacy', settings, bindings)
    run(legacy)
    assert (legacy / 'settings.ini').read_bytes() == settings
    assert not (legacy / '.internal' / 'ProfileRecovery').exists()

    broken = fixture('invalid-bindings', settings, bad)
    run(broken)
    backup(broken, 'settings.ini', settings)
    backup(broken, 'bindings.ini', bad)
    assert values(broken)['Main']['PollingMs'] == '3'
    assert values(broken)['Pad1_Axes']['LX_Plus'] == '0'
    assert not (broken / 'settings.ini.pre-bundle.bak').exists()
    stable(broken)

    missing = fixture('missing-bindings', settings)
    run(missing)
    assert values(missing)['Main']['PollingMs'] == '3'
    stable(missing)

    corrupt = fixture('invalid-settings', b'broken', bindings)
    run(corrupt)
    backup(corrupt, 'settings.ini', b'broken')
    assert values(corrupt)['Pad1_Axes']['LX_Plus'] == '7'
    stable(corrupt)

    empty = fixture('empty-files', b'', b'')
    run(empty)
    backup(empty, 'settings.ini', b'')
    backup(empty, 'bindings.ini', b'')
    stable(empty)

    named = fixture('missing-named-profile', settings + b'ActiveGlobalProfile=Lost\r\n', bindings)
    (named / 'Layouts').mkdir()
    (named / 'Layouts' / 'custom.ini').write_bytes(b'leave this alone')
    run(named)
    assert values(named)['Main']['ActiveGlobalProfile'] == 'Default'
    assert values(named)['Pad1_Axes']['LX_Plus'] == '7'
    assert (named / 'Layouts' / 'custom.ini').read_bytes() == b'leave this alone'
    stable(named)

    restored = fixture('restore-complete-backup', b'broken', bad)
    (restored / 'settings.ini.pre-bundle.bak').write_bytes(healthy_bundle)
    run(restored)
    backup(restored, 'settings.ini', b'broken')
    assert values(restored)['HallJoyProfile']['BundleVersion'] == '1'
    stable(restored)

    readonly = fixture('backup-unavailable', settings, bad)
    (readonly / '.internal').mkdir()
    (readonly / '.internal' / 'ProfileRecovery').write_bytes(b'blocked')
    run(readonly)
    assert (readonly / 'settings.ini').read_bytes() == settings
    assert (readonly / 'bindings.ini').read_bytes() == bad

    modern = fixture('broken-modern-no-stale-legacy',
        b'[HallJoyPersistence]\r\nSchemaVersion=1\r\nKind=Settings\r\n'
        b'[HallJoyProfile]\r\nBundleVersion=1\r\n'
        b'[Main]\r\nPollingMs=3\r\n[Pad1_Axes]\r\nLX_Plus=65543\r\n', bindings)
    run(modern)
    assert values(modern)['Pad1_Axes']['LX_Plus'] == '0'
    stable(modern)

    named_ok = fixture('valid-named-profile', settings + b'ActiveGlobalProfile=Game\r\n', bindings)
    (named_ok / 'GlobalProfiles').mkdir()
    (named_ok / 'GlobalProfiles' / 'Game.settings.ini').write_bytes(settings)
    (named_ok / 'GlobalProfiles' / 'Game.bindings.ini').write_bytes(bindings)
    run(named_ok)
    assert not (named_ok / '.internal' / 'ProfileRecovery').exists()
    stable(named_ok)

    for stage in ('prepare', 'write', 'flush', 'validate', 'replace'):
        failed = fixture(f'atomic-{stage}', settings, bad)
        run(failed, f'--halljoy-test-persistence-failure-{stage}')
        assert (failed / 'settings.ini').read_bytes() == settings
        backup(failed, 'settings.ini', settings)
        # A successful retry must reuse, not multiply, the recovery snapshot.
        run(failed)
        backup(failed, 'settings.ini', settings)
        stable(failed)
    print('PROFILE_STARTUP_RECOVERY=PASS (16 scenarios + repeated startup)', flush=True)


if __name__ == '__main__':
    main()
