"""Offline RongYuan batch inventory and guarded change-package generation.
Vendor input is data, never executed. A structurally compatible row is not a
support decision: prepare requires an explicit reviewed retail mapping manifest.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DATA = Path('docs/research/rongyuan-stream')
APP = Path('src/HallJoyProject/HallJoy')
PARENTS = {'7a5b12c9.js', '3dd2d1f8.js', '60ee4367.js', '4796d290.js',
           '631ebd97.js', '3f643f35.js', '9b8d4f8a.js'}
STATUS = 'Implemented; awaiting hardware testing'

def digest(raw):
    return hashlib.sha256(raw).hexdigest()

def read_json(path):
    return json.loads(path.read_bytes())

def encoded(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf8')

def create(path, raw):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as f:
        f.write(raw)

def parse_source(raw):
    text = raw.decode('utf8')
    match = re.match(r'import\{[CR] as \w+\}from"\./([^"/]+)";', text)
    if not match or match[1] not in PARENTS:
        raise ValueError('unreviewed_parent')
    expected_export = 'R' if match[1] == '3dd2d1f8.js' else 'C'
    if not text.startswith('import{' + expected_export + ' as '):
        raise ValueError('unexpected_parent_export')
    field = re.search(r'defaultMatrix=(\[[0-9,]+\]|[A-Za-z_$][\w$]*)(?=;|})', text)
    if not field:
        raise ValueError('nonliteral_matrix')
    literal = field[1]
    if not literal.startswith('['):
        local = re.search(r'(?:const |,)' + re.escape(literal) + r'=(\[[0-9,]+\])(?=;|,)', text)
        if not local:
            raise ValueError('nonliteral_matrix')
        literal = local[1]
    matrix = json.loads(literal)
    if len(matrix) != 512 or any(type(x) is not int or not 0 <= x <= 255 for x in matrix):
        raise ValueError('invalid_matrix')
    try:
        body = text.split('extends ', 1)[1].split('{', 1)[1].rsplit('}export', 1)[0]
    except IndexError as e:
        raise ValueError('unknown_class_shape') from e
    body = re.sub(r'default\w*matrix=(?:\[[0-9,]+\]|[A-Za-z_$][\w$]*);?', '', body, flags=re.I)
    body = re.sub(r'(?:DAZZLE|NORMAL)=\d+;?', '', body)
    body = re.sub(r'COMMONCOLOR=\{[0-9:,]+\};?', '', body)
    sku = r'skuCount=\d+;skuToText=(\w+)=>\{switch\(\1\)\{(?:case \d+:return"[^"\\]*";)*default:return"[^"\\]*"\}\};?'
    reviewed = read_json(ROOT / DATA / 'reviewed-overrides.json')
    exact_review = any(x['sha256'] == digest(raw) and x['parent'] == match[1] for x in reviewed.values())
    if body.strip() and not re.fullmatch(sku, body) and not exact_review:
        raise ValueError('unreviewed_class_behavior')
    return match[1], matrix

class Corpus:
    def __init__(self, root, catalog, archive):
        self.root = root
        self.rows = read_json(catalog)['rows']
        self.input_hashes = {'catalog': digest(catalog.read_bytes()), 'archive': digest(archive.read_bytes())}
        self.zip = zipfile.ZipFile(archive)
        self.names = {}
        for name in self.zip.namelist():
            if name.endswith('/'):
                continue
            key = Path(name).name
            if key in self.names:
                raise ValueError('duplicate archive basename: ' + key)
            self.names[key] = name
        self.profile_bytes = (root / DATA / 'profiles.json').read_bytes()
        self.profiles = json.loads(self.profile_bytes)
        self.lock = read_json(root / DATA / 'source-lock.json')
        for name, sha in self.lock.items():
            if digest((root / DATA / name).read_bytes()) != sha:
                raise ValueError('source lock mismatch: ' + name)
        self.refs = {}
        for path in (root / APP).rglob('*.h'):
            if path.name == 'rongyuan_stream_protocol.h':
                continue
            # Conservative ownership warning, not a claim that support is enabled.
            for board in re.findall(r'\{\s*(\d{4})\s*,', path.read_text(encoding='utf8')):
                self.refs.setdefault(int(board), set()).add(str(path.relative_to(root)))

    def source(self, row):
        chunks = {x['chunk'] for x in row.get('loader_matches', [])}
        if len(chunks) != 1:
            raise ValueError('missing_or_ambiguous_loader')
        name = next(iter(chunks))
        if name not in self.names:
            raise ValueError('missing_source')
        raw = self.zip.read(self.names[name])
        parent, matrix = parse_source(raw)
        if parent not in self.lock or self.zip.read(self.names[parent]) != (self.root / DATA / parent).read_bytes():
            raise ValueError('parent_bytes_changed')
        return name, raw, parent, matrix

    def inspect(self):
        counts = Counter((r['id'], r['vid'], r['pid']) for r in self.rows)
        known = {(p['board'], p['vid'], p['pid']) for p in self.profiles}
        result = []
        for r in self.rows:
            key = (r['id'], r['vid'], r['pid'])
            item = {k: r.get(k) for k in ('id', 'vid', 'pid', 'company', 'displayName')}
            reasons = []
            if key in known:
                reasons.append('already_stream')
            if counts[key] != 1:
                reasons.append('duplicate_identity')
            if not r['vid'] or not r['pid'] or r['id'] == 9999:
                reasons.append('placeholder_identity')
            if not r.get('magnetism_flag') or r.get('explicit_no_magnetic_switch'):
                reasons.append('not_magnetic')
            if r['id'] in self.refs:
                item['code_references'] = sorted(self.refs[r['id']])
                reasons.append('existing_code_reference')
            if '\u673a\u68b0\u8f74' in r.get('record_literal', ''):
                reasons.append('mechanical_switch_option_review')
            if 'membranceSwitchKeyHidCodeSet' in r.get('record_literal', ''):
                reasons.append('hybrid_position_review')
            try:
                name, raw, parent, matrix = self.source(r)
                item.update(source=name, source_sha256=digest(raw), parent=parent,
                            matrix_sha256=digest(bytes(matrix)))
            except (ValueError, KeyError) as e:
                reasons.append(str(e))
            item['reasons'] = reasons
            item['state'] = 'review_retail_mapping' if not reasons else 'held'
            result.append(item)
        return {'inputs': self.input_hashes, 'counts': dict(Counter(x['state'] for x in result)),
                'reason_counts': dict(Counter(r for x in result for r in x['reasons'])), 'rows': result}

    def prepare(self, manifest):
        if (self.root / DATA / 'profiles.json').read_bytes() != self.profile_bytes:
            raise ValueError('profiles changed since inventory')
        for name, sha in self.lock.items():
            if digest((self.root / DATA / name).read_bytes()) != sha:
                raise ValueError('locked input changed: ' + name)
        scan = self.inspect()['rows']
        lookup = {(r['id'], r['vid'], r['pid']): r for r in scan}
        selected, seen = [], set()
        for m in manifest:
            key = (m['id'], m['vid'], m['pid'])
            if key in seen:
                raise ValueError('duplicate manifest identity')
            seen.add(key)
            entry = lookup[key]
            if entry['reasons']:
                raise ValueError(f'{key}: {entry["reasons"]}')
            if m['source_sha256'] != entry['source_sha256']:
                raise ValueError('reviewed source changed')
            for field in ('brand', 'model', 'evidence', 'range_basis'):
                if not isinstance(m.get(field), str) or not m[field].strip():
                    raise ValueError('missing review field: ' + field)
            if any(c in m['evidence'] + m['range_basis'] for c in '\n\r|'):
                raise ValueError('unsafe evidence table text')
            if any(c in m['brand'] + m['model'] for c in '\n\r"\\|'):
                raise ValueError('unsafe model label')
            if type(m.get('range_um')) is not int or not 0 < m['range_um'] <= 10000:
                raise ValueError('invalid normalization')
            r = next(r for r in self.rows if (r['id'], r['vid'], r['pid']) == key)
            name, raw, parent, matrix = self.source(r)
            p = dict(board=m['id'], vid=m['vid'], pid=m['pid'], brand=m['brand'], model=m['model'],
                     product='RY1B-' + str(m['id']), range_um=m['range_um'],
                     precision_enum=parent == '60ee4367.js', parent=parent, matrix=matrix,
                     source=name, sha256=digest(raw), note=m['evidence'] + '; ' + m['range_basis'])
            selected.append((p, r, raw))
        if not selected:
            raise ValueError('empty batch')
        edits = {}
        snapshots = {DATA / 'profiles.json': self.profile_bytes}
        def snapshot(path):
            if path not in snapshots:
                target = self.root / path
                snapshots[path] = target.read_bytes() if target.exists() else None
            return snapshots[path]
        def put(path, raw):
            old = snapshot(path)
            current = (self.root / path).read_bytes() if (self.root / path).exists() else None
            if current != old:
                raise ValueError('target changed during prepare: ' + str(path))
            if old != raw:
                edits[path.as_posix()] = {'before_sha256': digest(old) if old is not None else None,
                                         'after': raw.decode('utf8'), 'after_sha256': digest(raw)}
        def content(path):
            return snapshot(path).decode('utf8').replace('\r\n', '\n')
        profiles = self.profiles + [p for p, _, _ in selected]
        put(DATA / 'profiles.json', encoded(profiles))
        admission = json.loads(snapshot(DATA / 'admission_sources.json'))
        fields = admission['records'][0].keys()
        for p, r, raw in selected:
            admission['records'].append({k: r[k] for k in fields})
            put(DATA / p['source'], raw)
        ad = encoded(admission)
        put(DATA / 'admission_sources.json', ad)
        lock = dict(self.lock)
        lock.update({p['source']: p['sha256'] for p, _, _ in selected})
        lock['admission_sources.json'] = digest(ad)
        put(DATA / 'source-lock.json', encoded(lock))
        path = APP / 'rongyuan_stream_protocol.h'
        text = content(path)
        start = text.index('inline constexpr Model kModels[]={')
        end = text.index('\n};', start)
        lines = []
        for p, _, _ in selected:
            lines.append(' {' + ','.join(str(p[k]) for k in ('board','vid','pid')) + ',L"' + p['brand'] + ' ' + p['model'] + '","' + p['product'] + '",' + str(p['range_um']) + ',' + str(p['precision_enum']).lower() + ',{{' + ','.join(map(str,p['matrix'])) + '}}},')
        put(path, (text[:end] + '\n' + '\n'.join(lines) + text[end:]).encode())
        path = Path('src/HallJoyProject/tests/rongyuan_stream_protocol_test.cpp')
        text = content(path)
        old = 'revisions==' + str(len(self.profiles))
        if text.count(old) != 1:
            raise ValueError('test count anchor changed')
        put(path, text.replace(old, 'revisions==' + str(len(profiles))).encode())
        path = Path('docs/development/keyboard_support_notices.json')
        catalog = json.loads(snapshot(path))
        group = next(g for g in catalog['groups'] if g['id'] == 'RongYuanStream')
        existing = {(m['brand'], m['model']) for g in catalog['groups'] for m in g['models']}
        models = sorted({(p['brand'], p['model']) for p, _, _ in selected}, key=lambda x:(x[0].casefold(),x[1].casefold()))
        if any(m in existing and m not in {(p['brand'], p['model']) for p in self.profiles} for m in models):
            raise ValueError('model owned by another notice group')
        new = [m for m in models if m not in existing]
        group['models'] += [dict(brand=b, model=m, status=STATUS) for b,m in new]
        put(path, encoded(catalog))
        from support_notice_catalog import render
        put(APP / 'support_notice_catalog.h', render(catalog).encode())
        path = Path('README.md');text = content(path)
        start = text.index('|---|\n', text.index('### Experimental support')) + len('|---|\n')
        end = text.index('\n\n', start);lines = text[start:end].splitlines()
        for brand in sorted({b for b,_ in new}, key=str.casefold):
            labels = [brand + ' ' + m for b,m in new if b == brand]
            matches = [i for i,line in enumerate(lines) if line.startswith('| ' + brand + ' ')]
            if len(matches) > 1:
                raise ValueError('ambiguous README brand')
            if matches:
                i = matches[0]
                combined = lines[i][2:-2].split(', ') + labels
                lines[i] = '| ' + ', '.join(sorted(set(combined), key=str.casefold)) + ' |'
            else:
                lines.append('| ' + ', '.join(labels) + ' |')
        lines.sort(key=str.casefold)
        put(path, (text[:start] + '\n'.join(lines) + text[end:]).encode())
        path = Path('docs/SUPPORTED_HARDWARE.md')
        table = '\n'.join('| '+p['brand']+' | '+p['model']+' | '+str(p['board'])+' | '+p['note']+' |' for p,_,_ in selected)
        put(path, (content(path) + '\n\n## Local reviewed RongYuan batch\n\nWired USB; experimental, no hardware-test claim.\n\n| Brand | Model | Board | Evidence and normalization |\n|---|---|---|---|\n' + table + '\n').encode())
        path = Path('docs/releases/RELEASE_NOTES_NEXT.md');text = content(path)
        marker = 'These changes are local and are not part of the published 1.6.2 release.'
        if text.count(marker) != 1:
            raise ValueError('release-note anchor changed')
        bullet = '- Added experimental USB analog support for ' + ', '.join(b+' '+m for b,m in models) + '.\n\n'
        put(path, text.replace(marker, bullet + marker).encode())
        for path, before in snapshots.items():
            current = (self.root / path).read_bytes() if (self.root / path).exists() else None
            if current != before:
                raise ValueError('target changed during prepare: ' + str(path))
        return {'schema': 1, 'inputs': self.input_hashes, 'reviewed': manifest, 'edits': edits,
                'sheet_intent': [dict(brand=b,model=m,status=STATUS) for b,m in models],
                'completion_required': ['source audit', 'native regression', 'Release + six gates',
                    'live Sheet read/backup/update/readback', 'Sheet structure + all-yellow audit',
                    'current checkpoint and OWNER_CONTEXT']}

def apply_package(root, package, backup):
    root = root.resolve()
    if package.get('schema') != 1 or not package.get('edits'):
        raise ValueError('invalid or empty package')
    paths = []
    seen = set()
    for name, edit in package['edits'].items():
        p = (root / name).resolve()
        if not p.is_relative_to(root.resolve()) or '..' in Path(name).parts:
            raise ValueError('path escapes workspace')
        allowed = name in {'README.md','docs/SUPPORTED_HARDWARE.md','docs/releases/RELEASE_NOTES_NEXT.md',
            'docs/development/keyboard_support_notices.json','src/HallJoyProject/tests/rongyuan_stream_protocol_test.cpp',
            str(APP / 'rongyuan_stream_protocol.h').replace('\\','/'),str(APP / 'support_notice_catalog.h').replace('\\','/')} or name.startswith(DATA.as_posix() + '/')
        if p in seen or p == backup.resolve():
            raise ValueError('duplicate target or backup overlaps target')
        seen.add(p)
        if not allowed:
            raise ValueError('unexpected package target')
        old = p.read_bytes() if p.exists() else None
        raw = edit['after'].encode('utf8')
        if (digest(old) if old is not None else None) != edit['before_sha256']:
            raise ValueError('target changed since prepare: ' + name)
        if digest(raw) != edit['after_sha256']:
            raise ValueError('package content changed')
        paths.append((p, old, raw))
    with zipfile.ZipFile(backup, 'x', zipfile.ZIP_DEFLATED) as out:
        for p, old, _ in paths:
            if old is not None:
                out.writestr(p.relative_to(root).as_posix(), old)
        out.writestr('batch-manifest.json', encoded(package))
    for p, old, raw in paths:
        if (p.read_bytes() if p.exists() else None) != old:
            raise ValueError('concurrent change; inspect backup and partial writes: ' + str(p))
        if old is None:
            create(p, raw)
        else:
            p.write_bytes(raw)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['inventory', 'prepare', 'apply'])
    parser.add_argument('--catalog', type=Path)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--backup', type=Path)
    args = parser.parse_args()
    if args.mode == 'apply':
        if not args.package or not args.backup:
            parser.error('apply requires --package and --backup')
        apply_package(ROOT, read_json(args.package), args.backup)
        print('APPLIED; required validation and live Sheet sync remain pending')
        return
    if not args.catalog or not args.archive or not args.out:
        parser.error('inventory/prepare require --catalog, --archive and --out')
    corpus = Corpus(ROOT, args.catalog, args.archive)
    if args.mode == 'prepare' and not args.manifest:
        parser.error('prepare requires --manifest with reviewed retail mappings')
    result = corpus.inspect() if args.mode == 'inventory' else corpus.prepare(read_json(args.manifest))
    corpus.zip.close()
    create(args.out, encoded(result))
    print(json.dumps({'files':len(result['edits'])} if args.mode=='prepare' else result['counts']))

if __name__ == '__main__':
    main()
