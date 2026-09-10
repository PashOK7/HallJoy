"""Reproducible source audit, not permission to change runtime mappings."""
import argparse
import hashlib
import json
from pathlib import Path
from drunkdeer_layout_source import SOURCE, ROOT, MODELS, extract, factory_map


def audit(text):
    models={}
    for model in MODELS:
        rows=extract(text,model)
        keys=[k for row in rows for k in row]
        factory=factory_map(text,model)
        offsets=[k['value'] for k in keys]
        if len(set(offsets))!=len(offsets) or not all(0<=v<126 for v in offsets):
            raise ValueError('Duplicate or out-of-range physical offsets: '+model)
        models[model]=dict(keyCount=len(keys),
            preview=[dict(offset=k['value'],label=k['name'],css=k['className']) for k in keys],
            factory=factory,
            previewWithoutFactory=[k['value'] for k in keys if not factory[k['value']]['hid']],
            factoryWithoutPreview=[k['position'] for k in factory if k['hid'] and k['position'] not in offsets],
            inconsistentKeyIndex=[k['position'] for k in factory if k['position']!=k['keyIndex']])
    return dict(sourceUrl='https://drunkdeer-antler.com/js/index.CJWCGjvj.js',
                sourceSha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                status='research-only; discrepancies require review before runtime use',models=models)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    report=audit(SOURCE.read_text(encoding='utf-8-sig'))
    dest=ROOT/'docs/research/drunkdeer-source-audit.json'
    data=(json.dumps(report,ensure_ascii=True,indent=2)+'\n').encode()
    if dest.exists():
        if dest.read_bytes()!=data: raise ValueError('Refusing changed audit output')
    elif args.check: raise ValueError('Missing source audit')
    else:
        with dest.open('xb') as stream: stream.write(data)
    for model,m in report['models'].items():
        print(model,'keys=',m['keyCount'],'preview_without_factory=',m['previewWithoutFactory'],
              'factory_without_preview=',m['factoryWithoutPreview'],
              'index_mismatch=',m['inconsistentKeyIndex'])
