"""Versioned static extraction jobs. Format validity is never protocol evidence."""
from collections import deque
from dataclasses import dataclass
import hashlib
import io
import inspect
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import threading
import time
import zipfile


class Blocked(ValueError):
    pass


class Deferred(Exception):
    pass


@dataclass
class Budget:
    total: int
    used: int = 0

    def reserve(self, size):
        if size < 0 or size > self.total:
            raise Blocked('container exceeds total expansion budget')
        if size + self.used > self.total:
            raise Deferred('run expansion budget exhausted')
        self.used += size


def bounded_process(args, stdout_limit, timeout=45, stderr_limit=65536, check=True):
    """Drain both pipes concurrently, kill on overflow/timeout, never use a shell."""
    process = subprocess.Popen(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    buffers = [bytearray(), bytearray()]
    errors = []

    def drain(pipe, target, limit):
        try:
            while True:
                chunk = pipe.read(65536)
                if not chunk:
                    break
                if len(target) + len(chunk) > limit:
                    errors.append('native output limit exceeded')
                    process.kill()
                    break
                target.extend(chunk)
        except Exception as exc:
            errors.append(str(exc))
            process.kill()
        finally:
            pipe.close()

    readers = [threading.Thread(target=drain, args=(pipe, buf, cap), daemon=True)
               for pipe, buf, cap in [(process.stdout, buffers[0], stdout_limit),
                                      (process.stderr, buffers[1], stderr_limit)]]
    for thread in readers:
        thread.start()
    try:
        process.wait(timeout=timeout)
    except BaseException:
        process.kill()
        process.wait()
        raise
    finally:
        for thread in readers:
            thread.join(timeout=5)
    if any(t.is_alive() for t in readers):
        raise Blocked('native pipe did not close')
    if errors:
        raise Blocked(errors[0])
    if not check:
        return process.returncode, bytes(buffers[0]), bytes(buffers[1])
    if process.returncode:
        raise Blocked('native extractor exit=' + str(process.returncode))
    return bytes(buffers[0])


def safe_member(name):
    path = PurePosixPath(name.replace('\\', '/'))
    if not name or path.is_absolute() or '..' in path.parts or ':' in name or '\x00' in name:
        raise Blocked('unsafe archive member name')


def intel_hex(data, limit):
    """Strict addressed HEX records; no synthesized fill or inferred MCU identity.

    Format reference: https://www.keil.com/support/docs/1584/_hlp_hexfile.htm
    Ambiguous overlaps and wrapping records are deliberately rejected.
    """
    try:
        lines = data.decode('ascii').splitlines()
    except UnicodeDecodeError as exc:
        raise Blocked('HEX is not ASCII') from exc
    records = []
    base = 0
    end = False
    entry = None
    total = 0
    if len(lines) > 1000000:
        raise Blocked('HEX record count limit')
    for number, line in enumerate(lines, 1):
        if not line:
            continue
        if end:
            raise Blocked('HEX records after EOF')
        if not re.fullmatch(r':[0-9a-fA-F]{10,520}', line) or len(line) % 2 != 1:
            raise Blocked('invalid HEX record at line ' + str(number))
        raw = bytes.fromhex(line[1:])
        size, address, kind = raw[0], int.from_bytes(raw[1:3], 'big'), raw[3]
        if len(raw) != size + 5 or sum(raw) % 256:
            raise Blocked('HEX record length/checksum mismatch')
        payload = raw[4:-1]
        if kind == 0:
            if address + size > 0x10000 or base + address + size > 0x100000000:
                raise Blocked('HEX address wrap unsupported')
            if size:
                total += size
                if total > limit:
                    raise Blocked('HEX payload limit exceeded')
                records.append((base + address, payload))
        elif kind == 1:
            if size or address:
                raise Blocked('invalid HEX EOF')
            end = True
        elif kind in (2, 4):
            if size != 2 or address:
                raise Blocked('invalid HEX address extension')
            base = int.from_bytes(payload, 'big') << (4 if kind == 2 else 16)
        elif kind in (3, 5):
            if size != 4 or address:
                raise Blocked('invalid HEX entry point')
            value = {'kind': kind, 'data': payload.hex()}
            if entry is not None and entry != value:
                raise Blocked('conflicting HEX entry points')
            entry = value
        else:
            raise Blocked('unsupported HEX record type ' + str(kind))
    if not end or not records:
        raise Blocked('HEX missing EOF or payload')
    segments = []
    for address, payload in sorted(records):
        if segments and address < segments[-1][0] + len(segments[-1][1]):
            raise Blocked('overlapping HEX data records')
        if segments and address == segments[-1][0] + len(segments[-1][1]):
            segments[-1][1].extend(payload)
        else:
            if len(segments) >= 4096:
                raise Blocked('HEX segment count limit')
            segments.append((address, bytearray(payload)))
    return [(address, bytes(payload)) for address, payload in segments], entry


class Pipeline:
    VERSION = 'pipeline-v1'

    def __init__(self, corpus, limit, total, helper, max_depth=4):
        self.c = corpus
        self.limit = limit
        self.budget = Budget(total)
        self.helper = Path(helper)
        self.max_depth = max_depth
        self.dependencies = {}
        corpus.db.executescript('''
          CREATE TABLE IF NOT EXISTS job_attempts(
            hash TEXT, engine TEXT, attempt INTEGER, started REAL, finished REAL,
            state TEXT, result TEXT, PRIMARY KEY(hash,engine,attempt));
          CREATE TABLE IF NOT EXISTS images(
            identity TEXT PRIMARY KEY, format TEXT, layout TEXT, evidence TEXT);
          CREATE TABLE IF NOT EXISTS image_sources(
            identity TEXT REFERENCES images(identity), source TEXT REFERENCES objects(hash),
            engine TEXT, PRIMARY KEY(identity,source,engine));
          CREATE TABLE IF NOT EXISTS engine_specs(engine TEXT PRIMARY KEY, spec TEXT, toolkit TEXT);
          CREATE TABLE IF NOT EXISTS image_segments_v2(
            identity TEXT REFERENCES images(identity), ordinal INTEGER, address INTEGER,
            hash TEXT REFERENCES objects(hash), size INTEGER, PRIMARY KEY(identity,ordinal));
          CREATE TABLE IF NOT EXISTS image_segments(
            identity TEXT REFERENCES images(identity), address INTEGER,
            hash TEXT REFERENCES objects(hash), size INTEGER,
            PRIMARY KEY(identity,address));
        ''')
        # Additive repair: legacy nullable-address primary keys admitted duplicates.
        # The canonical layout is authoritative; retain the old table as history.
        with corpus.db:
            for identity, layout in corpus.db.execute('SELECT identity,layout FROM images'):
                for ordinal, segment in enumerate(json.loads(layout)['segments']):
                    corpus.db.execute('INSERT OR IGNORE INTO image_segments_v2 VALUES (?,?,?,?,?)',
                                      (identity, ordinal, segment['address'], segment['sha256'], segment['size']))

    def kind(self, data, sha):
        from firmware_corpus import PINNED_SLICES, KNOWN_RESEARCH
        if sha in PINNED_SLICES:
            return 'pinned-image'
        if data.startswith(b'MZ'):
            return 'pe'
        if zipfile.is_zipfile(io.BytesIO(data)):
            return 'zip'
        if data.startswith((b'Rar!', b'7z\xbc\xaf\x27\x1c')):
            return 'native-archive'
        if data.startswith(b':'):
            return 'intel-hex'
        if sha in KNOWN_RESEARCH:
            return 'known-image'
        return None

    def engine(self, kind):
        if kind not in self.dependencies:
            dep = None
            if kind == 'native-archive':
                dep = hashlib.sha256(self.helper.read_bytes()).hexdigest() if self.helper.is_file() else 'missing'
                self.helper_hash = dep
            elif kind == 'pe':
                try:
                    import pefile
                    dep = [pefile.__version__, hashlib.sha256(Path(pefile.__file__).read_bytes()).hexdigest()]
                except ImportError:
                    dep = 'missing'
            elif kind == 'zip':
                import zlib
                dep = [sys.version, zlib.ZLIB_RUNTIME_VERSION]
            from firmware_corpus import atomic_write, digest, listing_members, listing_fields
            if not hasattr(self, 'toolkit'):
                # Shared scheduler edits invalidate all affected extraction kinds;
                # dependency-only changes remain isolated to their adapter.
                toolkit = {'pipeline': Path(__file__).read_text(encoding='utf-8'),
                           'storage': [inspect.getsource(f) for f in
                                       (type(self.c).put, type(self.c).read_object, type(self.c).lineage_depths,
                                        atomic_write, digest, listing_members, listing_fields)]}
                encoded = json.dumps(toolkit, sort_keys=True).encode()
                self.toolkit = digest(encoded)
                folder = self.c.root / 'tooling'
                folder.mkdir(exist_ok=True)
                target = folder / (self.toolkit + '.json')
                if target.exists():
                    if digest(target.read_bytes()) != self.toolkit:
                        raise Blocked('toolkit snapshot integrity failure')
                else:
                    atomic_write(target, encoded)
            config = [self.VERSION, kind, dep, self.limit, self.budget.total, self.max_depth,
                      self.toolkit, sys.version]
            engine = self.VERSION + ':' + kind + ':' + hashlib.sha256(
                json.dumps(config, sort_keys=True).encode()).hexdigest()
            self.dependencies[kind] = engine
            with self.c.db:
                self.c.db.execute('INSERT OR IGNORE INTO engine_specs VALUES (?,?,?)',
                                  (engine, json.dumps(config), self.toolkit))
        return self.dependencies[kind]

    def native_command(self, args, limit, timeout=45):
        if hashlib.sha256(self.helper.read_bytes()).hexdigest() != self.helper_hash:
            raise Blocked('native helper changed during run; restart to select its new identity')
        return bounded_process(args, limit, timeout)

    def payloads(self, kind, sha, data):
        """Yield member, bytes, optional address. Reserve before producing bytes."""
        if kind == 'zip':
            with zipfile.ZipFile(io.BytesIO(data)) as archive:
                members = [m for m in archive.infolist() if not m.is_dir()]
                if len(members) > 4096:
                    raise Blocked('archive member count limit')
                names = set()
                for member in members:
                    safe_member(member.filename)
                    if member.filename in names:
                        raise Blocked('duplicate archive member name')
                    names.add(member.filename)
                    if member.flag_bits & 1 or member.file_size > self.limit or member.file_size > max(member.compress_size, 1) * 1000:
                        raise Blocked('encrypted or excessive archive member')
                    if (member.external_attr >> 16) & 0o170000 == 0o120000:
                        raise Blocked('archive symbolic link')
                self.budget.reserve(sum(m.file_size for m in members))
                for member in members:
                    yield member.filename, archive.read(member), None
        elif kind == 'native-archive':
            from firmware_corpus import listing_members
            if not self.helper.is_file():
                raise Blocked('installed 7-Zip unavailable')
            listing = self.native_command([str(self.helper), 'l', '-slt', '-sccUTF-8', '-p-',
                                       str(self.c.path(sha))], 4 * 1024 * 1024, 30)
            members = []
            names = set()
            for item in listing_members(listing.decode('utf-8')):
                if item.get('Folder') == '+':
                    continue
                name, size = item['Path'], int(item['Size'])
                safe_member(name)
                if name in names or size < 0 or size > self.limit:
                    raise Blocked('duplicate or excessive native member')
                names.add(name)
                if item.get('Encrypted') == '+' or any(item.get(k) for k in ('Symbolic Link', 'Hard Link', 'Copy Link')):
                    raise Blocked('encrypted archive or link')
                members.append((name, size))
            if len(members) > 4096:
                raise Blocked('archive member count limit')
            self.budget.reserve(sum(size for _, size in members))
            for name, size in members:
                output = self.native_command([str(self.helper), 'x', '-so', '-spd', '-p-', '--',
                                          str(self.c.path(sha)), name], size)
                if len(output) != size:
                    raise Blocked('native member size mismatch')
                yield name, output, None
        elif kind == 'pe':
            import pefile
            pe = pefile.PE(data=data, fast_load=True)
            try:
                pe.parse_data_directories(directories=[2])
                members = []
                def visit(directory, prefix='', depth=0):
                    if depth > 4:
                        raise Blocked('PE resource depth limit')
                    for item in directory.entries:
                        if not prefix and not item.name and item.id <= 24 and item.id != 10:
                            continue
                        name = prefix + '/' + str(item.name or item.id)
                        if hasattr(item, 'directory'):
                            visit(item.directory, name, depth + 1)
                        elif hasattr(item, 'data'):
                            resource = item.data.struct
                            if resource.Size > self.limit or len(members) >= 4096:
                                raise Blocked('PE resource size/count limit')
                            members.append((name, resource.OffsetToData, resource.Size))
                if hasattr(pe, 'DIRECTORY_ENTRY_RESOURCE'):
                    visit(pe.DIRECTORY_ENTRY_RESOURCE)
                self.budget.reserve(sum(size for _, _, size in members))
                for name, offset, size in members:
                    payload = pe.get_data(offset, size)
                    if len(payload) != size:
                        raise Blocked('truncated PE resource')
                    yield name, payload, None
            finally:
                pe.close()
        elif kind == 'intel-hex':
            segments, self.entry = intel_hex(data, self.limit)
            self.budget.reserve(sum(len(p) for _, p in segments))
            for address, payload in segments:
                yield 'address=' + hex(address), payload, address
        elif kind == 'pinned-image':
            from firmware_corpus import PINNED_SLICES, digest
            offset, size, expected = PINNED_SLICES[sha]
            self.budget.reserve(size)
            payload = data[offset:offset + size]
            if len(payload) != size or digest(payload) != expected:
                raise Blocked('pinned payload boundary/hash mismatch')
            yield 'offset=' + str(offset) + ';size=' + str(size), payload, None

    def record_image(self, sha, engine, kind, segments):
        # HEX validity does not establish that a device accepts these bytes.
        layout = {'segments': segments, 'entry': self.entry if kind == 'intel-hex' else None}
        encoded = json.dumps(layout, sort_keys=True, separators=(',', ':'))
        identity = hashlib.sha256(encoded.encode()).hexdigest()
        evidence = 'validated-addressed-encoding' if kind == 'intel-hex' else 'exact-reviewed-payload'
        self.c.db.execute('INSERT OR IGNORE INTO images VALUES (?,?,?,?)',
                          (identity, kind, encoded, evidence))
        self.c.db.execute('INSERT OR IGNORE INTO image_sources VALUES (?,?,?)', (identity, sha, engine))
        for ordinal, segment in enumerate(segments):
            self.c.db.execute('INSERT OR IGNORE INTO image_segments_v2 VALUES (?,?,?,?,?)',
                              (identity, ordinal, segment['address'], segment['sha256'], segment['size']))
        return identity

    def run(self, kinds=None, retry_blocked=False):
        c = self.c
        depths = c.lineage_depths()
        queue = deque(r[0] for r in c.db.execute('SELECT hash FROM objects ORDER BY hash'))
        seen = set()
        while queue:
            sha = queue.popleft()
            if sha in seen:
                continue
            seen.add(sha)
            data = c.read_object(sha)
            kind = self.kind(data, sha)
            if kind is None or kinds is not None and kind not in kinds:
                continue
            engine = self.engine(kind)
            previous = c.db.execute('SELECT state,attempts FROM jobs WHERE hash=? AND engine=?', (sha, engine)).fetchone()
            if previous and (previous[0] == 'complete' or previous[0] == 'blocked' and not retry_blocked):
                continue
            attempt = previous[1] + 1 if previous else 1
            now = time.time()
            with c.db:
                if previous and previous[0] == 'running':
                    c.db.execute("UPDATE job_attempts SET state='interrupted',finished=? WHERE hash=? AND engine=? AND attempt=? AND state='running'",
                                 (now, sha, engine, previous[1]))
                c.db.execute('INSERT OR REPLACE INTO jobs VALUES (?,?,?,?,?,NULL)', (sha, engine, kind, 'running', attempt))
                c.db.execute('INSERT INTO job_attempts VALUES (?,?,?,?,NULL,?,NULL)', (sha, engine, attempt, now, 'running'))
            children = []
            segments = []
            result = {'status': 'complete', 'members': 0, 'scope': 'selected extractor only'}
            c.db.execute('SAVEPOINT pipeline_payloads')
            try:
                depth = depths.get(sha)
                if kind != 'known-image' and (depth is None or depth >= self.max_depth):
                    raise Blocked('depth limit or unresolved acquisition lineage')
                for member, payload, address in self.payloads(kind, sha, data):
                    child = c.put(payload, commit=False)
                    c.db.execute('INSERT OR IGNORE INTO edges VALUES (?,?,?,?)', (sha, child, member, engine))
                    children.append(child)
                    segments.append({'address': address, 'sha256': child, 'size': len(payload)})
                result['members'] = len(children)
                if kind == 'known-image':
                    from firmware_corpus import KNOWN_RESEARCH
                    segments = [{'address': None, 'sha256': sha, 'size': len(data)}]
                    result['prior_references'] = KNOWN_RESEARCH[sha]
                    result['fresh_emulator_run'] = False
                if kind in ('intel-hex', 'pinned-image', 'known-image'):
                    result['image'] = self.record_image(sha, engine, kind, segments)
                if kind == 'pe':
                    result['unresolved'] = 'custom embedded/overlay payloads not examined'
            except Exception as exc:
                c.db.execute('ROLLBACK TO pipeline_payloads')
                children = []
                result = {'status': 'deferred' if isinstance(exc, Deferred) else 'blocked',
                          'reason': type(exc).__name__ + ': ' + str(exc)}
            except BaseException:
                c.db.execute('ROLLBACK TO pipeline_payloads')
                c.db.execute('RELEASE pipeline_payloads')
                c.db.rollback()
                raise
            encoded = json.dumps(result, sort_keys=True)
            c.db.execute('INSERT OR REPLACE INTO analysis VALUES (?,?,?)', (sha, engine, encoded))
            c.db.execute('UPDATE jobs SET state=?,result=? WHERE hash=? AND engine=?', (result['status'], encoded, sha, engine))
            c.db.execute('UPDATE job_attempts SET state=?,result=?,finished=? WHERE hash=? AND engine=? AND attempt=?',
                         (result['status'], encoded, time.time(), sha, engine, attempt))
            c.db.execute('RELEASE pipeline_payloads')
            c.db.commit()
            for child in children:
                depths[child] = min(depths.get(child, depth + 1), depth + 1)
                queue.append(child)
