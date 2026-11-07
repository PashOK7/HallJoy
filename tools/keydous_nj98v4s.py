"""Check the data-only V4S extraction; vendor application bytes remain private."""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def checked_profile():
    evidence = json.loads((ROOT / 'docs/research/rongyuan-stream/keydous-nj98v4s.json').read_bytes())
    assert evidence['schema'] == 1 and len(evidence['profiles']) == 1
    model = evidence['profiles'][0]
    assert (model['board'], model['vid'], model['pid']) == (3793, 0x3151, 0x5030)
    assert (model['brand'], model['model'], model['product']) == ('Keydous', 'NJ98-CP V4S TMR', 'RY1B-3793')
    assert model['range_um'] == 4000 and model['precision_enum'] is False
    matrix = model['matrix']
    assert len(matrix) == 512 and hashlib.sha256(bytes(matrix)).hexdigest() == evidence['matrix_sha256']
    assert evidence['registry_record'].startswith('{id:3793,vid:12625,pid:20528,keyLayout:u.Common98_NJ98CP_V2,')
    assert 'magnetism:!0' in evidence['registry_record']
    assert evidence['protocol']['transport'] == 'wired USB only'
    expected = {
        'v4-index.js': '919b104944fb60e0c5c8a721b84138991087d69a45d6a7b42e7bd610d67aec0d',
        '827485a6.js': '3517f14aaf8763140b08b3f820d60b00727e1d659ec9caf91495d7cd6945d02d',
        'aade6cc1.js': '03e5d7fc23a045647a2921a9e5d6789a8ed7485062ebbcb78ad0205bbe024f52',
    }
    sources = {}
    for source in evidence['sources']:
        path = (ROOT / source['path']).resolve()
        assert path.is_relative_to(ROOT / '.local')
        if path.name in expected:
            assert source['sha256'] == expected[path.name]
        if path.exists():
            data = path.read_bytes()
            assert hashlib.sha256(data).hexdigest() == source['sha256'], path
            sources[path.name] = data.decode('utf-8')
    if 'v4-index.js' in sources:
        assert evidence['registry_record'] in sources['v4-index.js']
    if '827485a6.js' in sources:
        assert 'ry5088_qn_nj98cpv4s_3m_8k_8k:()=>t(()=>import("./aade6cc1.js")' in sources['827485a6.js']
    if 'aade6cc1.js' in sources:
        text = sources['aade6cc1.js']
        assert text.startswith('import{C as t}from"./07c90958.js";')
        assert json.loads(re.search(r'defaultMatrix=(\[[0-9,]+\])', text)[1]) == matrix
        body = text.split('class e extends t{', 1)[1].split('}export', 1)[0]
        assert not re.sub(r'default(?:FnMac|Fn)?Matrix=\[[0-9,]+\];?', '', body).strip()
    if '07c90958.js' in sources:
        text = sources['07c90958.js']
        assert 'FEA_CMD_SET_MAGNETISM_REPOR=27;' in text
        assert 'FEA_CMD_GET_FEATURE_LIST=230;' in text
        assert 'i>=768&&i<1280)return 100' in text
        assert 'travel.step===.01;if(i>=1280&&s)return 100' in text
        assert 'e[1]=Number(t);' in text
    if '22e2022d.js' in sources:
        # Independently compare every saved geometry cell to its literal source.
        from extract_attackshark_family_layouts import usage
        from generate_attackshark_family import decode
        from layout_pipeline import validate_report
        keys = []
        for code, x, y, w, h, kind in re.findall(
                r'(\w+):\{x:(\d+),y:(\d+),width:(\d+),height:(\d+),type:"(\w+)"', sources['22e2022d.js']):
            if kind != 'key' or code == 'LaunchCalculator':
                continue
            label, hid = usage(code)
            keys.append(dict(hid=hid, label=label, x=int(x), y=int(y), w=int(w), h=int(h)))
        assert len(keys) == 97 and {k['hid'] for k in keys} == set(decode(matrix)) - {0}
        x, y = min(k['x'] for k in keys), min(k['y'] for k in keys)
        for key in keys:
            key['x'] -= x
            key['y'] -= y
        keys.sort(key=lambda k: (k['y'], k['x']))
        report = json.loads((ROOT / 'docs/research/keydous-layouts/nj98_cp_v4s_ansi.json').read_bytes())
        assert report['keys'] == keys
        validate_report(report)
    return model


if __name__ == '__main__':
    checked_profile()
    print('KEYDOUS_NJ98_V4S=PASS exact=3793/3151:5030 keys=97; source review, not hardware testing')
