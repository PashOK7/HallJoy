"""Pure platform snapshot adapters; source product labels are not retail mappings."""
import json
from pathlib import PurePosixPath
from urllib.parse import quote, urlparse


def https_url(value):
    if not isinstance(value, str):
        raise ValueError('source URL must be text')
    parsed = urlparse(value)
    if parsed.scheme != 'https' or not parsed.hostname or parsed.username or parsed.password:
        raise ValueError('source URL must be HTTPS without credentials')
    return value


def text(value, field):
    if not isinstance(value, str) or not value.strip():
        raise ValueError('missing source field: ' + field)
    return value


def parse_snapshot(adapter, raw):
    data = json.loads(raw)
    records = []
    if adapter == 'illumi-v1':
        for index, item in enumerate(data['device']):
            name = item.get('file', '')
            if not isinstance(name, str) or name and (PurePosixPath(name).name != name or '\\' in name or ':' in name):
                raise ValueError('unexpected manifest filename')
            records.append({'key': str(index), 'product': text(item['product'], 'product'),
                            'revision': str(item.get('version', '')), 'identity': {},
                            'url': 'https://www.illumipc.com/config/firmware/' + quote(name) if name else None,
                            'state': 'download-reference' if name else 'no-file-reference'})
    elif adapter == 'keychron-product-v1':
        body = data['data']
        product = body['product']
        identity = {k: product[k] for k in ('id', 'vid', 'pid', 'vendor_product_id')}
        latest = body.get('firmware', {}).get('lasted')
        records.append({'key': str(product['id']), 'product': text(product['name'], 'product name'),
                        'revision': str(latest.get('version', '')) if latest else '',
                        'identity': identity,
                        'url': https_url(latest['path']) if latest and latest.get('path') else None,
                        'state': 'download-reference' if latest and latest.get('path') else 'no-file-reference'})
    elif adapter == 'rongyuan-research-v1':
        # This adapter consumes the saved research wrapper, not a native API response.
        for item in data:
            identity = {k: item[k] for k in ('id', 'vid', 'pid', 'name')}
            row = {'key': str(item['id']), 'product': text(item['display'], 'display'),
                   'revision': '', 'identity': identity, 'url': None,
                   'state': 'source-error', 'http': item.get('http'), 'error': item.get('error')}
            if item.get('http') == 200:
                body = item['metadata']['data']
                if body['dev_id'] != item['id']:
                    raise ValueError('Rongyuan device identity mismatch')
                path = text(body['file_path'], 'file_path')
                if not path.startswith('fw_upgrade_file/') or '..' in PurePosixPath(path).parts or '\\' in path or ':' in path:
                    raise ValueError('unexpected Rongyuan download path')
                row.update(revision=text(body['version_str'], 'version'),
                           url='https://api2.rongyuan.tech:3816/download/' + quote(path, safe='/'),
                           state='download-reference')
                # These are claims in the saved research wrapper, not newly verified bytes.
                row['prior_hash_claims'] = {k: item[k] for k in ('download_sha256', 'firmware_sha256') if k in item}
            records.append(row)
    else:
        raise ValueError('unknown source adapter: ' + adapter)
    if len({row['key'] for row in records}) != len(records):
        raise ValueError('duplicate source record identity')
    for row in records:
        if row['url']:
            https_url(row['url'])
    return records


def initialize(db):
    db.executescript('''
      CREATE TABLE IF NOT EXISTS source_snapshots(
        hash TEXT REFERENCES objects(hash), adapter TEXT, origin TEXT,
        evidence_kind TEXT, PRIMARY KEY(hash,adapter,origin));
      CREATE TABLE IF NOT EXISTS source_records(
        snapshot TEXT REFERENCES objects(hash), adapter TEXT, record_key TEXT,
        product TEXT, revision TEXT, url TEXT, state TEXT, details TEXT,
        PRIMARY KEY(snapshot,adapter,record_key));
      CREATE TABLE IF NOT EXISTS device_associations(
        id TEXT PRIMARY KEY, snapshot TEXT, adapter TEXT, record_key TEXT,
        brand TEXT, model TEXT, hardware_revision TEXT, layout TEXT, transport TEXT,
        evidence_hash TEXT REFERENCES objects(hash), state TEXT,
        FOREIGN KEY(snapshot,adapter,record_key) REFERENCES source_records(snapshot,adapter,record_key));
    ''')


def import_snapshot(corpus, adapter, raw, origin, evidence_kind='local-snapshot'):
    if evidence_kind not in ('local-snapshot', 'downloaded-response'):
        raise ValueError('invalid source evidence kind')
    records = parse_snapshot(adapter, raw)  # Validate entire batch before any mutation.
    initialize(corpus.db)
    with corpus.db:
        sha = corpus.put(raw, origin, commit=False)
        corpus.db.execute('INSERT OR IGNORE INTO source_snapshots VALUES (?,?,?,?)',
                          (sha, adapter, origin, evidence_kind))
        for row in records:
            corpus.db.execute('INSERT OR IGNORE INTO source_records VALUES (?,?,?,?,?,?,?,?)',
                              (sha, adapter, row['key'], row['product'], row['revision'], row['url'],
                               row['state'], json.dumps(row, sort_keys=True)))
    return {'snapshot': sha, 'records': len(records), 'adapter': adapter}


def associate_device(corpus, claim):
    """Store an explicit reviewed mapping, never infer it from an OEM name.

    Reviewed means the mapping was reviewed; it says nothing about analog support.
    Unknown revision/layout/transport remain null and must not become wildcards.
    """
    import hashlib
    if claim.get('schema') != 1 or claim.get('state') not in ('candidate', 'reviewed', 'rejected'):
        raise ValueError('invalid association schema/state')
    source = claim.get('source', {})
    device = claim.get('device', {})
    if set(source) != {'snapshot', 'adapter', 'key'}:
        raise ValueError('exact source record required')
    if set(device) != {'brand', 'model', 'hardware_revision', 'layout', 'transport'}:
        raise ValueError('all device identity dimensions must be explicit, including null unknowns')
    for field in ('brand', 'model'):
        text(device[field], field)
    for field in ('hardware_revision', 'layout', 'transport'):
        if device[field] is not None:
            text(device[field], field)
    if not corpus.db.execute('SELECT 1 FROM models WHERE brand=? AND model=?', (device['brand'], device['model'])).fetchone():
        raise ValueError('device is not an exact catalog entry')
    initialize(corpus.db)
    if not corpus.db.execute('SELECT 1 FROM source_records WHERE snapshot=? AND adapter=? AND record_key=?',
                             (source['snapshot'], source['adapter'], source['key'])).fetchone():
        raise ValueError('unknown source record')
    evidence = claim.get('evidence_sha256')
    if not isinstance(evidence, str) or len(evidence) != 64 or any(c not in '0123456789abcdef' for c in evidence):
        raise ValueError('association evidence hash required')
    if not corpus.db.execute('SELECT 1 FROM objects WHERE hash=?', (evidence,)).fetchone():
        raise ValueError('association evidence must be indexed')
    corpus.read_object(evidence)
    identity = hashlib.sha256(json.dumps(claim, sort_keys=True, separators=(',', ':'), allow_nan=False).encode()).hexdigest()
    with corpus.db:
        corpus.db.execute('INSERT OR IGNORE INTO device_associations VALUES (?,?,?,?,?,?,?,?,?,?,?)',
                          (identity, source['snapshot'], source['adapter'], source['key'],
                           device['brand'], device['model'], device['hardware_revision'],
                           device['layout'], device['transport'], evidence, claim['state']))
    return identity
