"""Audit exact stream profiles against pinned vendor matrices and runtime rows."""
from pathlib import Path
import hashlib
import json
import re
ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'docs/research/rongyuan-stream'
for filename, digest in json.loads((DATA/'source-lock.json').read_bytes()).items():
    assert hashlib.sha256((DATA/filename).read_bytes()).hexdigest()==digest, filename
profiles = json.loads((DATA / 'profiles.json').read_bytes())
records = json.loads((DATA / 'admission_sources.json').read_bytes())['records']
header = (ROOT / 'src/HallJoyProject/HallJoy/rongyuan_stream_protocol.h').read_text(encoding='utf8')
def header_row(m, matrix):
    return ('{'+','.join(str(m[k]) for k in ('board','vid','pid'))+',L"'+m['brand']+' '+m['model']+'","'+m['product']+'",'
            +str(m['range_um'])+','+str(m['precision_enum']).lower()+',{{'+','.join(map(str,matrix))+'}}}')
def check_app_evidence(m, key):
    """Profiles admitted from the official MonsGeek web driver (RY5088 class chunk) rather
    than a per-class manufacturer chunk. The chunk and its stream parent k are the ones
    reviewed in usb-identity-audit/approved-aliases.json; stock firmware fragments are
    re-checked whenever the local image is present (tools/extract_monsgeek_fun60_ultra_2352.py)."""
    evidence = json.loads((DATA/m['evidence']).read_bytes())
    entry = next(e for e in evidence['models'] if (e['board'], e['vid'], e['pid']) == key)
    assert (entry['brand'], entry['model'], entry['product']) == (m['brand'], m['model'], m['product'])
    assert m['product'] == f"RY1B-{m['board']}" and not m['precision_enum'] and 'parent' not in m
    assert entry['source_record']['record'].startswith(f"{{id:{m['board']},vid:{m['vid']},pid:{m['pid']},")
    assert 'magnetism:!0' in entry['source_record']['record']
    model = entry['model_class']
    assert re.fullmatch(r'class \w+ extends k\{\}', re.sub(r'default\w*Matrix=\[[0-9,]+\];?', '', model)), key
    matrix = json.loads(re.search(r'defaultMatrix=(\[[0-9,]+\])', model)[1])
    assert len(matrix) == 512 and matrix == m['matrix'], key
    assert hashlib.sha256(bytes(matrix)).hexdigest() == entry['matrix_sha256']
    aliases = json.loads((ROOT/'docs/research/usb-identity-audit/approved-aliases.json').read_bytes())
    assert 'class k extends t{' in aliases['parent_class']
    assert any(a.get('model_source', {}).get('sha256') == entry['model_source']['sha256'] for a in aliases['aliases']), key
    chunk = ROOT/entry['model_source']['source']
    if chunk.exists():
        raw = chunk.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == entry['model_source']['sha256'] and model.encode() in raw, key
    firmware = ROOT/entry['firmware_local']
    if firmware.exists():
        raw = firmware.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == entry['firmware_sha256'], key
        for field in ('enable', 'producer', 'sender'):
            fragment = bytes.fromhex(entry[field+'_hex'])
            assert raw[entry[field+'_offset']:entry[field+'_offset']+len(fragment)] == fragment, (key, field)
    assert header_row(m, matrix) in header, key
seen = set()
for m in profiles:
    key = (m['board'], m['vid'], m['pid'])
    assert key not in seen
    seen.add(key)
    if 'evidence' in m:
        check_app_evidence(m, key)
        continue
    source = (DATA / m['source']).read_bytes()
    assert hashlib.sha256(source).hexdigest() == m['sha256']
    text = source.decode('utf8')
    # Resolve only literal arrays or a local literal constant, never execute vendor JS.
    field = re.search(r'defaultMatrix=(\[[0-9,]+\]|[A-Za-z_$][\w$]*)', text).group(1)
    if not field.startswith('['):
        field = re.search(r'(?:const |,)' + re.escape(field) + r'=(\[[0-9,]+\])', text).group(1)
    matrix = json.loads(field)
    # Class-field arrow functions can override protocol behavior too. Accept only
    # factory map data and reviewed lighting constants; require review for anything else.
    body = text.split('extends ', 1)[1].split('{', 1)[1].rsplit('}export', 1)[0]
    body = re.sub(r'default\w*matrix=(?:\[[0-9,]+\]|[A-Za-z_$][\w$]*);?', '', body, flags=re.I)
    body = re.sub(r'(?:DAZZLE|NORMAL)=\d+;?', '', body)
    body = re.sub(r'COMMONCOLOR=\{[0-9:,]+\};?', '', body)
    # Reviewed Titan SKU renderer: only a switch returning literal colorway labels.
    # This must not whitelist arbitrary arrow functions or protocol overrides.
    sku_labels = r'skuCount=\d+;skuToText=(\w+)=>\{switch\(\1\)\{(?:case \d+:return"[^"\\]*";)*default:return"[^"\\]*"\}\};?'
    reviewed = json.loads((DATA/'reviewed-overrides.json').read_bytes()).get(m['source'], {})
    exact_review = reviewed.get('sha256') == hashlib.sha256(source).hexdigest() and reviewed.get('parent') == m.get('parent')
    assert not body.strip() or re.fullmatch(sku_labels, body) or exact_review, (key, 'unreviewed class behavior', body[:80])
    assert len(matrix) == 512 and matrix == m['matrix'], key
    parent = '60ee4367.js' if m['precision_enum'] else '3dd2d1f8.js' if m['brand'] in ('Akko', 'MonsGeek') else '7a5b12c9.js'
    parent = m.get('parent', parent)
    assert parent in ('7a5b12c9.js', '3dd2d1f8.js', '60ee4367.js', '4796d290.js', '631ebd97.js', '3f643f35.js', '9b8d4f8a.js')
    # Reviewed RK auxiliary-read and updater-only subclasses.
    # Exact sources are locked; they inherit all analog commands unchanged.
    if parent in ('4796d290.js', '631ebd97.js', '3f643f35.js', '9b8d4f8a.js'):
        assert 'from"./7a5b12c9.js"' in (DATA / parent).read_text(encoding='utf8')
    assert m['precision_enum'] == (parent == '60ee4367.js')
    assert text.startswith('import{R as ' if parent=='3dd2d1f8.js' else 'import{C as ') and ('from"./'+parent+'"') in text[:90], key
    record = next(r for r in records if (r['id'], r['vid'], r['pid']) == key)
    assert any(r['chunk'] == m['source'] for r in record['loader_matches'])
    assert 'magnetism:!0' in record['record_literal'], key
    # Compare every compiled matrix byte and admission field, not only WASD.
    start = '{'+','.join(str(m[k]) for k in ('board','vid','pid'))+',L"'+m['brand']+' '+m['model']+'","'+m['product']+'",'+str(m['range_um'])+','+str(m['precision_enum']).lower()+',{{'
    expected = start+','.join(map(str,matrix))+'}}}'
    assert expected in header, key
catalog = json.loads((ROOT/'docs/development/keyboard_support_notices.json').read_bytes())
group = next(g for g in catalog['groups'] if g['id']=='RongYuanStream')
confirmed=group.get('confirmed_devices',[])
for device in confirmed:
    matches=[m for m in profiles if m['board']==device['board'] and m['pid']==device['pid']]
    assert len(matches)==1 and (matches[0]['brand'],matches[0]['model'])==(device['brand'],device['model'])
    spec=json.loads((ROOT/'tools/layout_catalog.json').read_bytes())['brands'][device['brand']]
    layout=next(m for m in spec['models'] if matches[0]['product'] in m['products'])
    assert int(device['token'],16)==int(hashlib.sha256(layout['id'].encode()).hexdigest()[:16],16)
assert {(m['brand'].lower(),m['model'].lower()) for m in profiles} == {(m['brand'].lower(),m['model'].lower()) for m in group['models']+confirmed}
assert group['protocol']==22 and group['flag']==65536
print(f'RONGYUAN_STREAM_PROFILES=PASS revisions={len(seen)} models={len(group["models"])}')
