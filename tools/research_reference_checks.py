"""Keep private source replay separate from public, hash-pinned reference checks.

Recording requires every original audit to pass against available source files.
Public checks NEVER claim to have replayed absent proprietary source material.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'docs/development/research_reference_checks.json'
SCRIPTS = ('check_rongyuan_stream_profiles.py', 'check_atk_hex80_native_map.py',
           'build_irok_mg75_layouts.py', 'check_jingtai_v1_profiles.py',
           'build_jingtai_layouts.py', 'build_rongyuan_layouts.py',
           'build_epomaker_layouts.py', 'build_neo_k617_layouts.py',
           'prepare_madlions_layouts.py', 'prepare_atk_hex80_layout.py',
           'prepare_ipi_layouts.py', 'build_ipi_native_catalog.py', 'build_everglide_layouts.py',
           'build_wlmouse_layouts.py', 'build_logitech_layouts.py',
           'research_layout_checks.py')


def digest(path, private=False):
    data = path.read_bytes()
    if not private and path.suffix.lower() in {'.py', '.h', '.hpp', '.cpp', '.json', '.md'}:
        data = data.replace(b'\r\n', b'\n')
    return hashlib.sha256(data).hexdigest()


def trace(script, output):
    os.environ['HALLJOY_RESEARCH_TRACE'] = '1'
    reads = {ROOT / 'tools' / script}
    active = True

    def audit(event, args):
        if active and event == 'open' and isinstance(args[0], str):
            p = Path(args[0]).resolve()
            mode = args[1]
            if p.is_relative_to(ROOT) and isinstance(mode, str) and 'r' in mode:
                reads.add(p)

    sys.addaudithook(audit)
    sys.path.insert(0, str(ROOT / 'tools'))
    sys.argv = [str(ROOT / 'tools' / script)]
    try:
        runpy.run_path(sys.argv[0], run_name='__main__')
    except SystemExit as exc:
        if exc.code not in (None, 0):
            raise
    active = False
    for module in list(sys.modules.values()):
        name = getattr(module, '__file__', None)
        if name:
            p = Path(name).resolve()
            if p.is_relative_to(ROOT) and p.suffix == '.py':
                reads.add(p)
    denied = set(json.loads((ROOT / 'docs/legal/PUBLICATION_POLICY.json').read_text())['denied_paths'])
    records = []
    for p in sorted(reads):
        if not p.is_file() or p.suffix == '.pyc' or p == Path(__file__).resolve():
            continue
        name = p.relative_to(ROOT).as_posix()
        # .local/ is never published: its files are private sources by definition.
        private = name in denied or name.startswith('.local/')
        records.append({'path': name, 'sha256': digest(p, private), 'private': private})
    with output.open('x', encoding='utf8') as f:
        json.dump(records, f, indent=2)


def verify_record(root, record):
    missing_private = []
    for item in record:
        p = (root / item['path']).resolve()
        if not p.is_relative_to(root.resolve()):
            raise ValueError('Reference path escape')
        if not p.is_file() and item['private']:
            missing_private.append(item['path'])
            continue
        if not p.is_file() or digest(p, item['private']) != item['sha256']:
            raise ValueError('Reference changed; rerun full private audit: ' + item['path'])
    return missing_private


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    parser.add_argument('--trace', choices=SCRIPTS)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--require-private', action='store_true')
    args = parser.parse_args()
    if args.trace:
        trace(args.trace, args.output)
        return
    if args.record:
        before = MANIFEST.read_bytes() if MANIFEST.exists() else None
        with tempfile.TemporaryDirectory(prefix='halljoy-research-checks-') as temp:
            checks = {}
            for script in SCRIPTS:
                output = Path(temp) / (script + '.json')
                subprocess.run([sys.executable, '-X', 'utf8', __file__, '--trace', script, '--output', str(output)], check=True, timeout=120)
                checks[script] = json.loads(output.read_text(encoding='utf8'))
        if (MANIFEST.read_bytes() if MANIFEST.exists() else None) != before:
            raise RuntimeError('Manifest changed during recording')
        MANIFEST.write_text(json.dumps({'schema': 1, 'checks': checks}, indent=2) + '\n', encoding='utf8')
        print('Full private audit record written: all original checks passed')
        return
    data = json.loads(MANIFEST.read_text(encoding='utf8'))
    if set(data['checks']) != set(SCRIPTS):
        raise RuntimeError('Audit set changed; record the full private checks again')
    replay = []
    for script in SCRIPTS:
        missing = verify_record(ROOT, data['checks'][script])
        if missing:
            if args.require_private:
                raise RuntimeError('Private sources required: ' + script)
            print('PUBLIC_REFERENCE=PASS ' + script + '; private source replay NOT RUN; checked files unchanged')
        else:
            replay.append(script)
    # The full replays are independent read-only scripts: run them concurrently,
    # print each output in the fixed order and fail if any of them fails.
    def run_one(script):
        return subprocess.run([sys.executable, '-X', 'utf8', str(ROOT / 'tools' / script)],
                              capture_output=True, text=True, encoding='utf8', errors='replace', timeout=120)
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, len(replay))) as pool:
        results = list(pool.map(run_one, replay))
    failed = []
    for script, result in zip(replay, results):
        sys.stdout.write(result.stdout); sys.stderr.write(result.stderr)
        if result.returncode != 0:
            failed.append(script)
    sys.stdout.flush()
    if failed:
        raise SystemExit('Research reference replay failed: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
