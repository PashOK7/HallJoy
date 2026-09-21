"""Produce reviewed ATTACK SHARK layout reports and exact identity aliases."""
from pathlib import Path
import collections
import hashlib
import json
import argparse
from layout_pipeline import validate_report

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/research/attackshark-layout-sources'
LEGACY={2308:'X65',2938:'X65',2370:'X68',2901:'X68',2356:'X82',2935:'X82'}
NAMES={
    'R98PRO':'R98 Pro', 'R98GT':'R98 GT', 'R98ULTRA':'R98 Ultra', 'R98HE':'R98 HE',
    'X65HE':'X65 HE','X65':'X65 HE','X68HE':'X68 HE','R68HE':'R68 HE',
    'X65PRO':'X65 Pro HE','X68PRO HE':'X68 Pro HE','X82PRO HE':'X82 Pro HE',
    'Beat75':'Beat75','X87Ultra':'X87 Ultra','X68MAX':'X68 MAX','X68Ultra':'X68 Ultra',
    'X85Ultra':'X85 Ultra','R86PROHE':'R86 Pro HE','R82PROHE':'R82 Pro HE','R82HE':'R82 HE',
    'K85':'K85','K85PROHE':'K85 Pro HE','X60 HE':'X60 HE','X98HE':'X98 HE',
    'X96HE':'X96 HE','R85Ultra':'R85 Ultra','R85HE':'R85 HE','X82HE':'X82 HE','X820pro':'X820 Pro',
}


def payload(obj):
    return (json.dumps(obj,indent=2)+'\n').encode('utf8')


def outputs():
    profiles=ROOT/'docs/research/attackshark-family-profiles-20260920.json'
    rows=json.loads((OUT/'extraction.json').read_text())
    groups=collections.defaultdict(list)
    for r in rows:
        if r['id']==2633:
            # The shared SVG says AltRight, but this exact factory matrix has
            # Right Control at that physical position. Do not change the backend.
            assert r['geometryWithoutNativeUsage']==[230] and r['nativeUsageWithoutGeometry']==[228]
            for k in r['keys']:
                if k['hid']==230:k.update(hid=228,label='Ctrl')
        groups[json.dumps(r['keys'],sort_keys=True)].append(r)
    files,models,aliases={},[],[]
    for members in groups.values():
        new=[r for r in members if r['id'] not in LEGACY]
        if not new:continue
        names=sorted({NAMES[r['display']] for r in new})
        model=' + '.join(names)
        symbol='attackshark_'+str(min(r['id'] for r in new))+'_ansi'
        selectors=[str(r['id']) for r in sorted(new,key=lambda r:r['id'])]
        sources={s['path']:s for r in members for s in r['sources']}
        sources[profiles.relative_to(ROOT).as_posix()]=dict(path=profiles.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(profiles.read_bytes()).hexdigest())
        report=dict(schema=1,id=symbol,brand='ATTACK SHARK',model=model,variant='ANSI',
                    name='ATTACK SHARK '+model+' ANSI',status='ready',unresolved=[],
                    keys=members[0]['keys'],omitted=sorted({c for r in members for c in r['omitted']}),
                    sources=list(sources.values()),identity=dict(protocol='attackshark-ry5088',products=selectors),
                    geometryEvidence='Exact official registry keyLayout -> RT component -> default SVG. Consumer/encoder actions omitted.',
                    reviewedDiscrepancies=[dict(id=r['id'],geometryWithoutNativeUsage=r['geometryWithoutNativeUsage'],
                                               nativeUsageWithoutGeometry=r['nativeUsageWithoutGeometry']) for r in members],
                    corrections=['dev2633 factory Right Control replaces the shared SVG Right Alt label.'] if any(r['id']==2633 for r in members) else [])
        validate_report(report)
        file=OUT/(symbol+'.json');data=payload(report);files[file]=data
        models.append(dict(id=symbol,brand='ATTACK SHARK',model=model,variant='ANSI',count=len(report['keys']),
                           source=file.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(data).hexdigest(),products=selectors))
        aliases += [(r['id'],symbol,report['name']) for r in new]
    spec=dict(adapter='reviewed-report',runtime=True,guide='docs/current/ATTACK_SHARK_FAMILY_LAYOUTS_2026-09-20.md',models=models)
    catalog=ROOT/'tools/layout_catalog.json';value=json.loads(catalog.read_text())
    value['brands']['ATTACK SHARK']=spec;files[catalog]=payload(value)
    header=['#pragma once','#include <cstdint>','namespace halljoy::sharklayout {',
            'inline constexpr wchar_t X65[] = L"ATTACK SHARK X65 Pro HE ANSI";',
            'inline constexpr wchar_t X68[] = L"ATTACK SHARK X68 Pro HE ANSI";',
            'inline constexpr wchar_t X82[] = L"ATTACK SHARK X82 Pro HE ANSI";',
            '// Exact native identity only. Existing Pro tokens remain stable.',
            'struct Identity { unsigned id; std::uint64_t token; const wchar_t* name; };',
            'inline constexpr Identity Identities[]={']
    old={'X65':0x5348583635414e53,'X68':0x5348583638414e53,'X82':0x5348583832414e53}
    for id,key in sorted(LEGACY.items()):header.append(f'    {{{id},0x{old[key]:016X}ull,{key}}},')
    for id,symbol,name in sorted(aliases):
        token=int(hashlib.sha256(symbol.encode()).hexdigest()[:16],16)
        header.append(f'    {{{id},0x{token:016X}ull,L{json.dumps(name)}}},')
    header+=['};','inline constexpr std::uint64_t Token(unsigned id) noexcept {',
             '    for(const auto& e:Identities)if(e.id==id)return e.token;',
             '    return 0;','}',
             'inline constexpr const wchar_t* Match(std::uint64_t token) noexcept {',
             '    for(const auto& e:Identities)if(e.token==token)return e.name;',
             '    return nullptr;','}','}','']
    files[ROOT/'src/HallJoyProject/HallJoy/attackshark_pro_layout_identity.h']='\n'.join(header).encode()
    return files


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    result=outputs()
    previous={p:p.read_bytes() if p.exists() else None for p in result}
    for p,data in result.items():
        if args.check:assert previous[p]==data,p
        elif previous[p]!=data:
            assert (p.read_bytes() if p.exists() else None)==previous[p],p
            with p.open('wb' if previous[p] is not None else 'xb') as f:f.write(data)
    print('ATTACK SHARK family layouts: 37 exact selectors, 15 new reports PASS')
