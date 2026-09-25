"""Reconcile every experimental model with the production-compiled layout catalog.

Layout gaps are allowed; this report records them instead of inventing geometry.
Run again after any support batch, using the profile simulator's TSV export.
"""
import argparse
import collections
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def norm(s):return re.sub('[^a-z0-9]','',s.lower())


def audit(catalog):
    names={line.split('\t')[1] for line in Path(catalog).read_text(encoding='utf-8').splitlines()}
    notices=json.loads((ROOT/'docs/development/keyboard_support_notices.json').read_bytes())
    specs=json.loads((ROOT/'tools/layout_catalog.json').read_bytes())['brands']
    tokens={p:m for spec in specs.values() for m in spec.get('models',[]) for p in m['products']}
    profiles=json.loads((ROOT/'docs/research/rongyuan-stream/profiles.json').read_bytes())
    held={r['product']:r['reason'] for r in json.loads((ROOT/'docs/research/rongyuan-layouts/inventory.json').read_bytes())['held']}
    aliases={('ATTACK SHARK','X65'):'ATTACK SHARK X65 Pro HE ANSI',
             ('AULA','WIN 68 HE MAX'):'Aula WIN 68 HE PRO / MAX ANSI'}
    rows=[]
    for group in notices['groups']:
        for model in group['models']:
            brand,label=model['brand'],model['model']
            r=dict(brand=brand,model=label,protocolGroup=group['id'])
            if group['id']=='RongYuanStream':
                revisions=[p for p in profiles if norm(p['brand'])==norm(brand) and norm(p['model'])==norm(label)]
                assert revisions,(brand,label)
                covered=[p for p in revisions if p['product'] in tokens]
                selectors=sorted({tokens[p['product']]['source'] for p in covered})
                presets=[]
                for path in selectors:
                    report=json.loads((ROOT/path).read_bytes());assert report['name'] in names;presets.append(report['name'])
                r.update(coverage='complete' if len(covered)==len(revisions) else 'partial' if covered else 'missing',
                         presets=presets,coveredBoards=[p['board'] for p in covered],
                         skipped=[dict(board=p['board'],reason=held.get(p['product'],'no exact preset')) for p in revisions if p not in covered])
            else:
                prefix='IPI' if brand=='IPI / QBZ' else brand
                matches=[]
                for name in names:
                    # Exact individual or explicit combined selector members only.
                    if not norm(name).startswith(norm(prefix)):continue
                    tail=re.sub(r' (ANSI|ISO|JIS|ABNT2)$','',name)[len(prefix):].strip()
                    members=tail.split(' + ')
                    if any(norm(label)==norm(part) for part in members):matches.append(name)
                explicit=aliases.get((brand,label))
                if explicit:assert explicit in names;matches.append(explicit)
                if brand=='AULA' and label=='WIN 68 HE PRO':
                    name='Aula WIN 68 HE PRO / MAX ANSI';assert name in names;matches.append(name)
                reason=('Vendor renderer derives geometry from runtime layout and model-specific transforms; no simple exact preset admitted in this batch.'
                        if group['id']=='SparkLinkV2' else 'No exact ready geometry preset; intentionally deferred instead of guessing.')
                r.update(coverage='existing' if matches else 'missing',presets=sorted(set(matches)))
                if not matches:r['reason']=reason
            rows.append(r)
    assert len({(r['brand'],r['model']) for r in rows})==len(rows)
    return dict(schema=1,meaning='Visual layout coverage only; does not change analog support status. Existing means a named preset, not all regional revisions or verified auto selection.',
                counts=dict(collections.Counter(r['coverage'] for r in rows)),models=rows)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('catalog');p.add_argument('--output',type=Path);a=p.parse_args()
    report=audit(a.catalog)
    if a.output:
        data=(json.dumps(report,indent=2)+'\n').encode();before=a.output.read_bytes() if a.output.exists() else None
        assert (a.output.read_bytes() if a.output.exists() else None)==before
        with a.output.open('wb' if before is not None else 'xb') as f:f.write(data)
    print('YELLOW_LAYOUT_AUDIT=PASS',len(report['models']),report['counts'])
