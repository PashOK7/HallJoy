"""Extract exact numeric MOD007 V5 geometry and compare its physical factory map.

Vendor inputs stay private; only functional numeric geometry is emitted. No
JavaScript application code is executed and no keyboard is opened.
"""
import hashlib
import json
from pathlib import Path
import re

from extract_attackshark_family_layouts import usage
from generate_attackshark_family import decode
from layout_pipeline import validate_report

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = ROOT / '.local/research/akko-mod007v5-3113-20261116/f30ae273.js'
    original = source.read_bytes()
    assert hashlib.sha256(original).hexdigest() == '7ad8b35b36d4a19d2d8eb7cbd84d8fe69b3e3c6cfc98712c21ab199edcdf886c'
    text = original.decode('utf-8').split('const e=', 1)[1].split(';export', 1)[0]
    geometry = json.loads(re.sub(r'([{,])([A-Za-z_$][A-Za-z0-9_$]*):', r'\1"\2":', text))
    factory = ROOT / '.local/research/monsgeek-20260922/manufacturer-model-maps.json'
    factory_bytes = factory.read_bytes()
    matrix = next(r['matrix'] for r in json.loads(factory_bytes) if r['board'] == 3113)
    assert hashlib.sha256(bytes(matrix)).hexdigest() == '9503f1d76374e1dda4d88a2f93f9307e39dfb12d9b985d70ef9ca99e967a690b'
    keys = []
    for code, g in geometry['layout'].items():
        assert g['type'] == 'key'
        label, hid = usage(code)
        keys.append(dict(hid=hid, label=label, x=g['x'], y=g['y'], w=g['width'], h=g['height']))
    assert len(keys) == 80 and {k['hid'] for k in keys} == set(decode(matrix)) - {0}
    report = dict(schema=1, id='akko_mod007_v5_he_ansi', brand='Akko',
                  model='MOD007 V5 HE', variant='ANSI', name='Akko MOD007 V5 HE ANSI',
                  status='ready', unresolved=[], keys=keys, sources=[],
                  identity=dict(protocol='rongyuan-snapshot', products=['AKKOMOD007V5HE-3113'], requiresVerifiedSession=True),
                  notes=['Exact manufacturer Common82_MOD007V5HE numeric geometry;80 physical keys including Fn.',
                         'Encoder actions and LCD are not analog channels; the manufacturer numeric keyboard geometry excludes them.',
                         'Private source locks, official URLs and exact firmware evidence: docs/current/AKKO_MOD007_V5_HE_2026-11-16.md.'])
    validate_report(report)
    path = ROOT / 'docs/research/akko-mod007-v5-he-ansi.json'
    expected = (json.dumps(report, indent=2) + '\n').encode()
    before = path.read_bytes() if path.exists() else None
    assert before is None or before == expected, 'Reviewed report changed; preserve existing extraction'
    assert source.read_bytes() == original and factory.read_bytes() == factory_bytes
    if before is None:
        with path.open('xb') as stream:
            stream.write(expected)
    print('AKKO_MOD007_V5_LAYOUT=PASS physical=80 sha256=' + hashlib.sha256(expected).hexdigest())


if __name__ == '__main__':
    main()
