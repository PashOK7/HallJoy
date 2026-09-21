"""Allowlisted exact-image MCU suites; never auto-dispatch by similarity or labels."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import time

from firmware_evidence import canonical, validate_and_record
from firmware_pipeline import bounded_process

ROOT = Path(__file__).resolve().parents[1]
SUITES = {
    'ajazz-sg8994he-raw-v1': {
        'sha256': 'fd42c6f691e353a4411c6c13023f7ee3eccd7726bb7b3ac28ee4fe62df24f680',
        'input_size': 524288,
        'entry': 'tools/review_ajazz_ak820max_raw_stream.py',
        'sources': ['tools/review_ajazz_ak820max_firmware.py',
                    'tools/review_ajazz_ak820max_routes.py',
                    'tools/review_ajazz_ak820max_raw_stream.py'],
        'input_path': '.local/research/ajazz-ak820max/SG8994HE_V1_13_02_flash512k.bin',
        'success_marker': b'RAW_STREAM=PASS:',
        'mocked_peripherals': ['synthetic ADC/RAM', 'event-wait stub', 'synthetic scheduler servicing'],
        'limitations': ['PASS means suite expectations, including known event loss, were reproduced; not support readiness.',
                        'Exact no-light SG8994HE V1.13.02 only; no RGB inference.',
                        'Real MCU code executes with synthetic ADC/RAM and modeled scheduling.',
                        'No hardware USB timing, physical calibration or full-device lifecycle proof.'],
    },
}


def dependency_snapshot():
    import unicorn
    root = Path(unicorn.__file__).parent
    files = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in sorted(root.rglob('*')) if p.is_file() and p.suffix.lower() in ('.py', '.dll', '.so', '.dylib')}
    return {'python': sys.version, 'python_executable': hashlib.sha256(Path(sys.executable).read_bytes()).hexdigest(),
            'unicorn_version': unicorn.__version__, 'unicorn_files': files}


def initialize(db):
    db.executescript('''
      CREATE TABLE IF NOT EXISTS research_runs(
        id INTEGER PRIMARY KEY, cache_key TEXT, suite TEXT, image TEXT,
        started REAL, finished REAL, state TEXT, receipt TEXT);
    ''')


def run_suite(corpus, suite_name, image_identity, rerun=False):
    if suite_name not in SUITES:
        raise ValueError('suite is not allowlisted')
    suite = SUITES[suite_name]
    corpus.pipeline()
    row = corpus.db.execute('SELECT layout FROM images WHERE identity=?', (image_identity,)).fetchone()
    if not row:
        raise ValueError('unknown image identity')
    layout = json.loads(row[0])
    if layout != {'segments': [{'address': None, 'sha256': suite['sha256'], 'size': suite['input_size']}], 'entry': None}:
        raise ValueError('suite requires its exact reviewed raw image and layout')
    image = corpus.read_object(suite['sha256'])
    # Only repository-owned scripts from the reviewed allowlist are copied/executed.
    # Their __file__-relative research input is isolated from the live checkout.
    sources = {name: (ROOT / name).read_bytes() for name in suite['sources']}
    deps = dependency_snapshot()
    runner_bytes = Path(__file__).read_bytes()
    framework = {name: (Path(__file__).parent / name).read_bytes() for name in
                 ('firmware_pipeline.py', 'firmware_evidence.py', 'firmware_corpus.py')}
    configuration = {'suite': suite_name, 'image': image_identity,
                     'sources': {k: hashlib.sha256(v).hexdigest() for k, v in sources.items()},
                     'framework': {k: hashlib.sha256(v).hexdigest() for k, v in framework.items()},
                     'runner_sha256': hashlib.sha256(runner_bytes).hexdigest(),
                     'dependencies': deps, 'timeout_seconds': 60, 'stdout_limit': 1024 * 1024}
    cache_key = hashlib.sha256(canonical(configuration)).hexdigest()
    initialize(corpus.db)
    old = corpus.db.execute("SELECT receipt FROM research_runs WHERE cache_key=? AND state='complete' ORDER BY id DESC LIMIT 1", (cache_key,)).fetchone()
    if old and not rerun:
        receipt = corpus.db.execute('SELECT receipt FROM evidence_receipts WHERE id=?', (old[0],)).fetchone()
        if not receipt:
            raise ValueError('cached run has no evidence receipt')
        # Recheck all evidence assets before reusing the completed execution.
        validate_and_record(corpus, json.loads(receipt[0]))
        return {'state': 'cached', 'receipt': old[0], 'cache_key': cache_key}
    started = time.time()
    with corpus.db:
        corpus.db.execute("UPDATE research_runs SET state='interrupted',finished=? WHERE cache_key=? AND state='running'", (started, cache_key))
        run_id = corpus.db.execute('INSERT INTO research_runs(cache_key,suite,image,started,state) VALUES (?,?,?,?,?)',
                                   (cache_key, suite_name, image_identity, started, 'running')).lastrowid
    try:
        with tempfile.TemporaryDirectory(prefix='halljoy-firmware-suite-') as tmp:
            root = Path(tmp)
            for name, data in {**sources, suite['input_path']: image}.items():
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open('xb') as output:
                    output.write(data)
            try:
                code, stdout, stderr = bounded_process([sys.executable, '-E', '-B', str(root / suite['entry'])],
                                                       1024 * 1024, 60, check=False)
                outcome = 'pass' if code == 0 and suite['success_marker'] in stdout else 'fail'
                result = {'exit_code': code, 'stdout': stdout.decode('utf-8', errors='replace'),
                          'stderr': stderr.decode('utf-8', errors='replace')}
            except Exception as exc:
                outcome = 'inconclusive'
                result = {'runner_error': type(exc).__name__ + ': ' + str(exc)}
        assets = []
        for name, data in sources.items():
            assets.append({'role': 'producer-code', 'sha256': corpus.put(data, 'suite-source:' + name)})
        for name, data in framework.items():
            assets.append({'role': 'framework-code', 'sha256': corpus.put(data, 'suite-framework:' + name)})
        assets.append({'role': 'runner-code', 'sha256': corpus.put(runner_bytes, 'suite-runner:firmware_research_runner.py')})
        assets.append({'role': 'configuration', 'sha256': corpus.put(canonical(configuration))})
        assets.append({'role': 'raw-result', 'sha256': corpus.put(canonical(result))})
        receipt = {'schema': 1, 'image': image_identity, 'input_hashes': [suite['sha256']],
                   'level': 'mcu-emulation', 'outcome': outcome,
                   'producer': {'name': suite_name, 'version': cache_key},
                   'parameters': {'mocked_peripherals': suite['mocked_peripherals'],
                                  'dependencies': deps, 'started': started, 'finished': time.time()},
                   'limitations': suite['limitations'], 'assets': assets}
        identity = validate_and_record(corpus, receipt)
        with corpus.db:
            corpus.db.execute("UPDATE evidence_receipts SET verification='executed-locally; exact-image allowlisted suite; not-hardware' WHERE id=?", (identity,))
            corpus.db.execute('UPDATE research_runs SET state=?,finished=?,receipt=? WHERE id=?',
                              ('complete' if outcome in ('pass', 'fail') else 'inconclusive', time.time(), identity, run_id))
        return {'state': outcome, 'receipt': identity, 'cache_key': cache_key}
    except BaseException:
        # Persist running state for crash/interruption recovery; never invent a PASS.
        raise
