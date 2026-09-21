"""Validate evidence receipts; importing a receipt never executes its producer."""
import hashlib
import json
import re

LEVELS = {'historical-reference', 'static-analysis', 'mcu-emulation',
          'synthetic-test', 'hardware-capture'}
OUTCOMES = {'pass', 'fail', 'inconclusive', 'reference-only'}


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True,
                      allow_nan=False).encode()


def initialize(db):
    db.executescript('''
      CREATE TABLE IF NOT EXISTS evidence_receipts(
        id TEXT PRIMARY KEY, image TEXT REFERENCES images(identity), level TEXT,
        outcome TEXT, receipt TEXT, verification TEXT);
      CREATE TABLE IF NOT EXISTS evidence_assets(
        receipt TEXT REFERENCES evidence_receipts(id), role TEXT,
        hash TEXT REFERENCES objects(hash), PRIMARY KEY(receipt,role,hash));
    ''')


def validate_and_record(corpus, receipt):
    """Validate identity/bytes/structure, not the truth of an imported claim.

    Producers must supply raw results, exact inputs, tool code and limitations.
    The store marks these as imported receipts, never as independently rerun tests.
    """
    if receipt.get('schema') != 1 or receipt.get('level') not in LEVELS or receipt.get('outcome') not in OUTCOMES:
        raise ValueError('invalid evidence schema/level/outcome')
    image = receipt.get('image')
    corpus.pipeline()
    row = corpus.db.execute('SELECT layout FROM images WHERE identity=?', (image,)).fetchone()
    if not row:
        raise ValueError('evidence refers to an unknown image')
    expected = sorted(s['sha256'] for s in json.loads(row[0])['segments'])
    if sorted(receipt.get('input_hashes', [])) != expected:
        raise ValueError('evidence input bytes do not match the addressed image')
    if not isinstance(receipt.get('limitations'), list) or not receipt['limitations'] or not all(isinstance(x, str) and x for x in receipt['limitations']):
        raise ValueError('explicit evidence limitations required')
    if not isinstance(receipt.get('parameters'), dict):
        raise ValueError('evidence parameters required')
    producer = receipt.get('producer', {})
    if not producer.get('name') or not producer.get('version'):
        raise ValueError('versioned producer required')
    assets = receipt.get('assets', [])
    if not assets or not all(isinstance(a, dict) for a in assets):
        raise ValueError('evidence assets required')
    roles = set()
    for asset in assets:
        if set(asset) != {'role', 'sha256'} or not isinstance(asset['role'], str) or not asset['role']:
            raise ValueError('invalid evidence asset')
        sha = asset['sha256']
        if not isinstance(sha, str) or not re.fullmatch('[0-9a-f]{64}', sha):
            raise ValueError('invalid evidence asset hash')
        if not corpus.db.execute('SELECT 1 FROM objects WHERE hash=?', (sha,)).fetchone():
            raise ValueError('unindexed evidence asset')
        corpus.read_object(sha)
        roles.add(asset['role'])
    level = receipt['level']
    if level == 'historical-reference':
        if receipt['outcome'] != 'reference-only' or 'reference' not in roles:
            raise ValueError('historical reference is not a fresh test outcome')
    else:
        if receipt['outcome'] == 'reference-only' or not {'producer-code', 'raw-result'} <= roles:
            raise ValueError('code and raw result required for analysis evidence')
    if level == 'mcu-emulation' and not receipt['parameters'].get('mocked_peripherals'):
        raise ValueError('emulation must identify mocked peripherals')
    if level == 'hardware-capture' and not receipt['parameters'].get('device_identity'):
        raise ValueError('hardware evidence requires exact device identity')
    for sha in expected:
        corpus.read_object(sha)
    encoded = canonical(receipt)
    identity = hashlib.sha256(encoded).hexdigest()
    initialize(corpus.db)
    with corpus.db:
        corpus.db.execute('INSERT OR IGNORE INTO evidence_receipts VALUES (?,?,?,?,?,?)',
                          (identity, image, level, receipt['outcome'], encoded.decode(), 'imported-receipt; assets-verified; not-rerun'))
        for asset in assets:
            corpus.db.execute('INSERT OR IGNORE INTO evidence_assets VALUES (?,?,?)',
                              (identity, asset['role'], asset['sha256']))
    return identity
