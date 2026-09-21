"""Resumable firmware acquisition; never execute vendor packages or infer support.

Objects are immutable and content addressed. Retail identity, source records,
archive edges and analysis results remain separate. Python 3.10 + requests.
"""
import argparse
import hashlib
import io
import json
import os
from collections import deque
from pathlib import Path
import sqlite3
import struct
import tempfile
import time
import zipfile
from urllib.parse import quote, urlparse

import requests

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / '.local/firmware-corpus'
LIMIT = 128 * 1024 * 1024
TOTAL_EXPANSION = 512 * 1024 * 1024
MANIFEST = 'https://www.illumipc.com/config/firmware.json'
SEVENZIP = Path('C:/Program Files/7-Zip/7z.exe')
# Exact-image references only. These are prior research, not fresh test results.
KNOWN_RESEARCH = {
    'fd42c6f691e353a4411c6c13023f7ee3eccd7726bb7b3ac28ee4fe62df24f680':
        ['tools/review_ajazz_ak820max_raw_stream.py', 'docs/current/AJAZZ_AK820MAX_REVIEW_2026-09-20.md'],
    '356aa3a6c6c77fde51f32d041c9aaa5b145aca183e8024e085410fdd20544feb':
        ['tools/review_mg75_firmware.py', 'docs/current/MG75_PRO_V2_REVIEW_2026-09-19.md'],
    '4f2e8b8b406a72ed4a4d4b34502478aa9b8b5d874c4a3cdfa1381acaea00ec44':
        ['tools/review_io_type84_firmware.py'],
    '8f5ef507771c6795258eb7521cfc1b46269a5b301548767ae87a3f1a83a50d45':
        ['tools/review_io_type84_firmware.py'],
    'c070e514ff1bef20a71abff12a6c30b04f152892b0fa22e1b5e0709be63eb7cd':
        ['tools/review_aula_mini60pro_firmware.py'],
}
PINNED_SLICES = {
    '9b4529df72210440abe8c9f7685e55129fbb7f5c6f6ff67fc70cdd0f76f2e073':
        (0x2ce808, 524288, 'fd42c6f691e353a4411c6c13023f7ee3eccd7726bb7b3ac28ee4fe62df24f680')
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def atomic_write(path, data, expected=None):
    """Publish complete bytes; refuse replacement if the observed target changed.

    Single-writer contract still applies. Staging files from a killed process are
    harmless: they are never indexed or considered objects.
    """
    path = Path(path)
    staged = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.stage-', delete=False) as out:
            staged = Path(out.name)
            out.write(data)
            out.flush()
            os.fsync(out.fileno())
        current = digest(path.read_bytes()) if path.exists() else None
        if current != expected:
            raise RuntimeError('target changed before atomic publication')
        os.replace(staged, path)
    finally:
        if staged is not None and staged.exists():
            staged.unlink()


def listing_fields(record):
    # Strip after splitting: empty values in 7-Zip listings end with " = ".
    return {key.strip(): value.strip() for key, value in
            (line.split(' = ', 1) for line in record.splitlines() if ' = ' in line)}


def listing_members(text):
    text = text.replace('\r\n', '\n')
    return [listing_fields(record) for record in text.split('----------', 1)[1].strip().split('\n\n')
            if record.strip()]


class Corpus:
    def __init__(self, root):
        self.root = Path(root)
        (self.root / 'objects').mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(self.root / 'index.sqlite')
        self.db.executescript('''
          PRAGMA foreign_keys=ON;
          CREATE TABLE IF NOT EXISTS objects(hash TEXT PRIMARY KEY, size INTEGER);
          CREATE TABLE IF NOT EXISTS origins(origin TEXT, hash TEXT REFERENCES objects(hash),
            observed REAL, PRIMARY KEY(origin,hash));
          CREATE TABLE IF NOT EXISTS fetches(url TEXT PRIMARY KEY, hash TEXT, status TEXT,
            checked REAL, etag TEXT, modified TEXT, error TEXT);
          CREATE TABLE IF NOT EXISTS fetch_observations(id INTEGER PRIMARY KEY, url TEXT, hash TEXT, status TEXT,
            checked REAL, etag TEXT, modified TEXT, error TEXT);
          CREATE TABLE IF NOT EXISTS inventory(source TEXT, product TEXT, version TEXT,
            url TEXT, PRIMARY KEY(source,product,version,url));
          CREATE TABLE IF NOT EXISTS models(brand TEXT, model TEXT, status TEXT,
            snapshot TEXT, PRIMARY KEY(brand,model));
          CREATE TABLE IF NOT EXISTS edges(parent TEXT, child TEXT, member TEXT,
            method TEXT, PRIMARY KEY(parent,child,member,method));
          CREATE TABLE IF NOT EXISTS analysis(hash TEXT, engine TEXT, result TEXT,
            PRIMARY KEY(hash,engine));
          CREATE TABLE IF NOT EXISTS jobs(hash TEXT, engine TEXT, kind TEXT,
            state TEXT, attempts INTEGER, result TEXT, PRIMARY KEY(hash,engine));
        ''')
        self.session = requests.Session()
        self.session.headers['User-Agent'] = 'HallJoy-Firmware-Corpus/1.0'

    def path(self, sha):
        return self.root / 'objects' / sha[:2] / sha

    def put(self, data, origin=None, commit=True):
        if len(data) > LIMIT:
            raise ValueError('object size limit exceeded')
        sha = digest(data)
        path = self.path(sha)
        path.parent.mkdir(exist_ok=True)
        if path.exists():
            if digest(path.read_bytes()) != sha:
                raise ValueError('existing object failed integrity check')
        else:
            atomic_write(path, data)
        self.db.execute('INSERT OR IGNORE INTO objects VALUES (?,?)', (sha, len(data)))
        if origin:
            self.db.execute('INSERT OR IGNORE INTO origins VALUES (?,?,?)',
                            (origin, sha, time.time()))
        if commit:
            self.db.commit()
        return sha

    def fetch(self, url, refresh=False):
        from firmware_sources import https_url
        https_url(url)
        old = self.db.execute('SELECT hash,status,checked,etag,modified FROM fetches WHERE url=?',
                              (url,)).fetchone()
        if old and not refresh:
            if old[1] == 'ok':
                data = self.path(old[0]).read_bytes()
                if digest(data) != old[0]:
                    raise ValueError('cached object failed integrity check')
                return old[0]
            if time.time() - old[2] < 86400:
                return None
        headers = {}
        if old and old[0]:
            if old[3]:
                headers['If-None-Match'] = old[3]
            if old[4]:
                headers['If-Modified-Since'] = old[4]
        error = None
        for attempt in range(3):
            try:
                with self.session.get(url, headers=headers, stream=True, timeout=(15, 45)) as r:
                    if urlparse(r.url).scheme != 'https':
                        raise ValueError('non-HTTPS redirect')
                    if r.status_code == 304 and not (old and old[0]):
                        raise ValueError('304 response without cached object')
                    if r.status_code == 304 and old and old[0]:
                        if digest(self.path(old[0]).read_bytes()) != old[0]:
                            raise ValueError('cached object failed integrity check')
                        self.save_fetch(url, old[0], 'ok', old[3], old[4], None)
                        return old[0]
                    if r.status_code in (429, 500, 502, 503, 504) and attempt < 2:
                        time.sleep(2 ** attempt)
                        continue
                    r.raise_for_status()
                    if int(r.headers.get('Content-Length', '0')) > LIMIT:
                        raise ValueError('download size limit exceeded')
                    data = bytearray()
                    for chunk in r.iter_content(65536):
                        data.extend(chunk)
                        if len(data) > LIMIT:
                            raise ValueError('download size limit exceeded')
                    if bytes(data[:256]).lstrip().lower().startswith((b'<!doctype html', b'<html')):
                        raise ValueError('HTML returned instead of package/metadata')
                    sha = self.put(bytes(data), url)
                    self.save_fetch(url, sha, 'ok', r.headers.get('ETag'),
                                    r.headers.get('Last-Modified'), None)
                    return sha
            except (requests.ConnectionError, requests.Timeout) as exc:
                error = str(exc)
                if attempt < 2:
                    time.sleep(2 ** attempt)
                    continue
            except (requests.RequestException, ValueError) as exc:
                error = str(exc)
            break
        self.save_fetch(url, old[0] if old else None, 'error',
                        old[3] if old else None, old[4] if old else None, error)
        return None

    def save_fetch(self, url, sha, status, etag, modified, error):
        values = (url, sha, status, time.time(), etag, modified, error)
        with self.db:
            self.db.execute('INSERT OR REPLACE INTO fetches VALUES (?,?,?,?,?,?,?)', values)
            self.db.execute('INSERT INTO fetch_observations(url,hash,status,checked,etag,modified,error) VALUES (?,?,?,?,?,?,?)', values)

    def source_snapshot(self, adapter, path):
        from firmware_sources import import_snapshot
        file = Path(path)
        return import_snapshot(self, adapter, file.read_bytes(), str(file.resolve()))

    def sheet(self, path):
        data = Path(path).read_bytes()
        sha = self.put(data, str(Path(path).resolve()))
        rows = json.loads(data)['values']
        parsed = []
        brand = None
        for row in rows[1:]:
            if not row or not any(row):
                brand = None
                continue
            if row[0]:
                brand = row[0].strip()
            if not brand or len(row) < 3 or not row[1]:
                raise ValueError('ambiguous sheet row')
            parsed.append((brand, row[1].strip(), row[2], sha))
        if len({(r[0], r[1]) for r in parsed}) != len(parsed):
            raise ValueError('duplicate model identity')
        with self.db:
            self.db.execute('DELETE FROM models')
            self.db.executemany('INSERT INTO models VALUES (?,?,?,?)', parsed)

    def illumi(self, refresh=False, download=False):
        sha = self.fetch(MANIFEST, refresh)
        if not sha:
            raise ValueError('manifest fetch failed; inspect report')
        from firmware_sources import import_snapshot
        data = self.read_object(sha)
        import_snapshot(self, 'illumi-v1', data, MANIFEST, 'downloaded-response')
        entries = json.loads(data)['device']
        for item in entries:
            name = item.get('file', '')
            if name and (Path(name).name != name or '/' in name or '\\' in name):
                raise ValueError('unexpected manifest filename')
            url = 'https://www.illumipc.com/config/firmware/' + quote(name) if name else ''
            self.db.execute('INSERT OR IGNORE INTO inventory VALUES (?,?,?,?)',
                            (sha, item['product'], item.get('version', ''), url))
            self.db.commit()
            if url and download:
                self.fetch(url, refresh)

    def ingest(self, paths):
        allowed = {'.bin', '.hex', '.dfu', '.zip', '.rar', '.7z', '.exe'}
        for raw in paths:
            path = Path(raw)
            files = sorted(path.rglob('*')) if path.is_dir() else [path]
            for file in files:
                if file.is_file() and file.suffix.lower() in allowed and file.stat().st_size <= LIMIT:
                    self.put(file.read_bytes(), str(file.resolve()))

    def read_object(self, sha):
        data = self.path(sha).read_bytes()
        if digest(data) != sha:
            raise ValueError('object integrity failure: ' + sha)
        return data

    def lineage_depths(self):
        """Shortest known acquisition path; extracted objects are not new roots.

        Legacy standalone imports without an origin are accepted only if they have
        no incoming edge. A cycle without an acquisition root stays unresolved.
        """
        children = {}
        incoming = set()
        objects = {r[0] for r in self.db.execute('SELECT hash FROM objects')}
        for parent, child in self.db.execute('SELECT DISTINCT parent,child FROM edges'):
            children.setdefault(parent, set()).add(child)
            incoming.add(child)
        roots = (objects - incoming) | {r[0] for r in self.db.execute('SELECT hash FROM origins')}
        depths = {sha: 0 for sha in roots}
        queue = deque(roots)
        while queue:
            parent = queue.popleft()
            for child in children.get(parent, ()):
                if child not in depths:
                    depths[child] = depths[parent] + 1
                    queue.append(child)
        return depths

    def pipeline(self):
        from firmware_pipeline import Pipeline
        return Pipeline(self, LIMIT, TOTAL_EXPANSION, SEVENZIP)

    def extract(self, kinds=None, retry_blocked=False):
        self.pipeline().run(kinds, retry_blocked)

    def expand(self, retry_blocked=False):
        self.extract({'zip'}, retry_blocked)

    def extract_native(self, retry_blocked=False):
        self.extract({'native-archive', 'pe'}, retry_blocked)

    def extract_pinned(self):
        self.extract({'pinned-image'})

    def audit_store(self):
        """Read-only reconciliation: retain unexplained bytes and provenance."""
        indexed = dict(self.db.execute('SELECT hash,size FROM objects'))
        result = {'missing': [], 'corrupt': [], 'unindexed': [], 'staging': [], 'unexpected': []}
        for sha, size in indexed.items():
            path = self.path(sha)
            if not path.is_file():
                result['missing'].append(sha)
            elif path.stat().st_size != size or digest(path.read_bytes()) != sha:
                result['corrupt'].append(sha)
        for path in sorted((self.root / 'objects').glob('*/*')):
            if not path.is_file():
                continue
            if path.name.startswith('.stage-'):
                result['staging'].append(str(path.relative_to(self.root)))
            elif len(path.name) != 64 or any(c not in '0123456789abcdef' for c in path.name) or path.parent.name != path.name[:2]:
                result['unexpected'].append(str(path.relative_to(self.root)))
            elif path.name not in indexed:
                result['unindexed'].append({'sha256': path.name, 'valid': digest(path.read_bytes()) == path.name})
        result['staging'].extend(str(p.relative_to(self.root)) for p in self.root.glob('.stage-*') if p.is_file())
        result['dangling_edges'] = list(self.db.execute('SELECT DISTINCT parent,child FROM edges WHERE parent NOT IN (SELECT hash FROM objects) OR child NOT IN (SELECT hash FROM objects)'))
        result['invalid_images'] = []
        tables = {r[0] for r in self.db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        if 'images' in tables:
            for identity, layout in self.db.execute('SELECT identity,layout FROM images'):
                try:
                    description = json.loads(layout)
                    encoded = json.dumps(description, sort_keys=True, separators=(',', ':')).encode()
                    expected = [(i, segment['address'], segment['sha256'], segment['size'])
                                for i, segment in enumerate(description['segments'])]
                    actual = list(self.db.execute('SELECT ordinal,address,hash,size FROM image_segments_v2 WHERE identity=? ORDER BY ordinal', (identity,)))
                    if digest(encoded) != identity or expected != actual or any(indexed.get(row[2]) != row[3] for row in expected):
                        result['invalid_images'].append(identity)
                except (ValueError, KeyError, TypeError, sqlite3.OperationalError):
                    result['invalid_images'].append(identity)
        result['invalid_toolkits'] = []
        if 'engine_specs' in tables:
            for toolkit, in self.db.execute('SELECT DISTINCT toolkit FROM engine_specs'):
                target = self.root / 'tooling' / (toolkit + '.json')
                if not target.is_file() or digest(target.read_bytes()) != toolkit:
                    result['invalid_toolkits'].append(toolkit)
        result['sqlite'] = self.db.execute('PRAGMA integrity_check').fetchone()[0]
        result['foreign_keys'] = list(self.db.execute('PRAGMA foreign_key_check'))
        return result

    def report(self):
        kinds = {}
        candidates = []
        for sha, size in self.db.execute('SELECT hash,size FROM objects'):
            data = self.read_object(sha)
            kind = 'unclassified'
            if zipfile.is_zipfile(io.BytesIO(data)):
                kind = 'zip'
            elif data.startswith(b'MZ'):
                kind = 'pe-container'
            elif data.startswith(b'Rar!'):
                kind = 'rar-container'
            elif data.startswith(b'7z\xbc\xaf\x27\x1c'):
                kind = '7z-container'
            elif data.startswith(b':'):
                kind = 'possible-intel-hex-needs-validation'
            # Structural triage only: vector plausibility is NOT protocol evidence.
            vectors = []
            if kind == 'unclassified':
                for offset in (0, 0x1000, 0x2000, 0x4000, 0x8000):
                    if offset + 64 > len(data):
                        continue
                    words = struct.unpack_from('<16I', data, offset)
                    sp, pc = words[:2]
                    handlers = sum(bool(v & 1) and v != 0xffffffff for v in words[1:])
                    if 0x20000000 < sp < 0x20100000 and sp % 4 == 0 and pc & 1 and handlers >= 5:
                        vectors.append({'offset': offset, 'stack': sp, 'reset': pc,
                                        'plausible_handlers': handlers})
                if vectors:
                    kind = 'cortex-m-candidate'
                    candidates.append({'sha256': sha, 'size': size, 'vectors': vectors})
            kinds[kind] = kinds.get(kind, 0) + 1
        count = lambda table: self.db.execute('SELECT COUNT(*) FROM ' + table).fetchone()[0]
        from firmware_sources import initialize
        initialize(self.db)
        pipeline = self.pipeline()  # Additive schema migration.
        from firmware_evidence import initialize as initialize_evidence
        initialize_evidence(self.db)
        current_jobs = {}
        for kind, engine, state, number in self.db.execute('SELECT kind,engine,state,COUNT(*) FROM jobs GROUP BY kind,engine,state'):
            if engine == pipeline.engine(kind):
                current_jobs[kind + ':' + state] = number
        report = {'schema': 4, 'objects': count('objects'), 'origins': count('origins'),
                  'extraction_edges': count('edges'), 'models': count('models'),
                  'brands': self.db.execute('SELECT COUNT(DISTINCT brand) FROM models').fetchone()[0],
                  'kinds': kinds, 'cortex_candidates': candidates,
                  'fetch_errors': list(self.db.execute("SELECT url,error FROM fetches WHERE status='error'")),
                  'extraction_results': list(self.db.execute('SELECT hash,engine,result FROM analysis')),
                  'job_states': list(self.db.execute('SELECT kind,engine,state,COUNT(*) FROM jobs GROUP BY kind,engine,state')),
                  'current_job_states': current_jobs,
                  'source_records': count('inventory'),
                  'source_record_states': list(self.db.execute('SELECT adapter,state,COUNT(*) FROM source_records GROUP BY adapter,state')),
                  'evidence_states': list(self.db.execute('SELECT level,outcome,verification,COUNT(*) FROM evidence_receipts GROUP BY level,outcome,verification')),
                  'images': count('images'),
                  'platform_source_records': count('source_records'),
                  'device_associations': count('device_associations'),
                  'fetch_observations': count('fetch_observations'),
                  'image_inventory': list(self.db.execute('SELECT identity,format,layout,evidence FROM images')),
                  'image_sources': list(self.db.execute('SELECT identity,source,engine FROM image_sources')),
                  'job_attempts': count('job_attempts'),
                  'known_research': {sha: refs for sha, refs in KNOWN_RESEARCH.items()
                                     if self.db.execute('SELECT 1 FROM objects WHERE hash=?', (sha,)).fetchone()},
                  'brand_queue': list(self.db.execute('SELECT brand,COUNT(*) FROM models GROUP BY brand ORDER BY brand')),
                  'support_policy': 'No automatic support status changes; no protocol claims from fingerprints.'}
        target = self.root / 'report.json'
        # Generated report: single writer only, refuse an unexpected concurrent edit.
        before = digest(target.read_bytes()) if target.exists() else None
        output = json.dumps(report, indent=2, ensure_ascii=False).encode('utf-8')
        if (digest(target.read_bytes()) if target.exists() else None) != before:
            raise RuntimeError('report changed concurrently')
        atomic_write(target, output, expected=before)
        return {k: v for k, v in report.items() if k not in
                ('cortex_candidates', 'fetch_errors', 'extraction_results', 'brand_queue', 'known_research', 'image_inventory', 'image_sources', 'job_states')}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=DEFAULT)
    sub = p.add_subparsers(dest='command', required=True)
    s = sub.add_parser('sheet'); s.add_argument('path')
    s = sub.add_parser('ingest'); s.add_argument('paths', nargs='+')
    s = sub.add_parser('source-snapshot'); s.add_argument('adapter'); s.add_argument('paths', nargs='+')
    s = sub.add_parser('associate'); s.add_argument('path')
    s = sub.add_parser('evidence'); s.add_argument('path')
    s = sub.add_parser('run-suite'); s.add_argument('suite'); s.add_argument('image'); s.add_argument('--rerun', action='store_true')
    s = sub.add_parser('illumi'); s.add_argument('--download', action='store_true'); s.add_argument('--refresh', action='store_true')
    s = sub.add_parser('expand'); s.add_argument('--retry-blocked', action='store_true')
    s = sub.add_parser('extract-native'); s.add_argument('--retry-blocked', action='store_true')
    s = sub.add_parser('extract'); s.add_argument('--retry-blocked', action='store_true')
    sub.add_parser('extract-pinned'); sub.add_parser('report'); sub.add_parser('audit-store')
    a = p.parse_args()
    from firmware_store_lock import writer_lock
    with writer_lock(a.root):
        c = Corpus(a.root)
        try:
            if a.command == 'sheet': c.sheet(a.path)
            elif a.command == 'ingest': c.ingest(a.paths)
            elif a.command == 'source-snapshot':
                for path in a.paths: c.source_snapshot(a.adapter, path)
            elif a.command == 'run-suite':
                from firmware_research_runner import run_suite
                print(json.dumps(run_suite(c, a.suite, a.image, a.rerun)))
            elif a.command == 'associate':
                from firmware_sources import associate_device
                associate_device(c, json.loads(Path(a.path).read_bytes()))
            elif a.command == 'evidence':
                from firmware_evidence import validate_and_record
                validate_and_record(c, json.loads(Path(a.path).read_bytes()))
            elif a.command == 'illumi': c.illumi(a.refresh, a.download)
            elif a.command == 'expand': c.expand(a.retry_blocked)
            elif a.command == 'extract-native': c.extract_native(a.retry_blocked)
            elif a.command == 'extract': c.extract(retry_blocked=a.retry_blocked)
            elif a.command == 'audit-store':
                print(json.dumps(c.audit_store(), ensure_ascii=True))
                return
            elif a.command == 'extract-pinned': c.extract_pinned()
            print(json.dumps(c.report(), ensure_ascii=True))
        finally:
            c.db.close()
            c.session.close()



if __name__ == '__main__':
    main()
