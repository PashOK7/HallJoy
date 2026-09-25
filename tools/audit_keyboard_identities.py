"""Check reviewed USB aliases against independently captured vendor evidence.
Never accepts an alias from board number or retail spelling alone.
"""
from pathlib import Path
import hashlib
import json
import re
ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT/'docs/research/usb-identity-audit'
def check():
    evidence=json.loads((DATA/'approved-aliases.json').read_bytes())
    profiles=json.loads((ROOT/'docs/research/rongyuan-stream/profiles.json').read_bytes())
    header=(ROOT/'src/HallJoyProject/HallJoy/rongyuan_stream_protocol.h').read_text(encoding='utf8')
    body=re.search(r'kUsbAliases\[\]=\{(.*?)\};',header,re.S)[1]
    actual={tuple(map(int,x)) for x in re.findall(r'\{(\d+),(\d+),(\d+),(\d+),(\d+)\}',body)}
    expected=set()
    for a in evidence['aliases']:
        key=tuple(a[k] for k in ('board','vid','pid','canonical_vid','canonical_pid'))
        assert key not in expected;expected.add(key)
        canonical=next(p for p in profiles if (p['board'],p['vid'],p['pid'])==(a['board'],a['canonical_vid'],a['canonical_pid']))
        record=a['source_record']['record']
        assert f"id:{a['board']},vid:{a['vid']},pid:{a['pid']}," in record
        model=a['model_class'];matrix=json.loads(a['matrix_literal']) if a.get('kind')=='firmware-stream' else json.loads(re.search(r'defaultMatrix=(\[[0-9,]+\])',model)[1])
        assert matrix==canonical['matrix']
        assert hashlib.sha256(bytes(matrix)).hexdigest()==a['matrix_sha256']
        stripped=re.sub(r'default\w*Matrix=\[[0-9,]+\];?','',model)
        if a.get('kind')=='firmware-stream':
            assert '"defaultMatrix",qCa)' in model
            assert a['tuple_hex']=='4a3722a2'
            fw=ROOT/a['firmware_local']
            if fw.exists():
                raw=fw.read_bytes();assert hashlib.sha256(raw).hexdigest()==a['firmware_sha256']
                for field in ['sender','enable','producer','tuple']:
                    fragment=bytes.fromhex(a[field+'_hex']);offset=a[field+'_offset']
                    assert raw[offset:offset+len(fragment)]==fragment
        else:
            assert re.fullmatch(r'class \w+ extends k\{\}',stripped)
    assert actual==expected, 'Unreviewed or missing USB alias'
    proto=evidence['protocol_source']
    assert hashlib.sha256(proto.encode()).hexdigest()==evidence['protocol_source_sha256']
    assert 'FEA_CMD_SET_MAGNETISM_REPOR=27;' in proto
    assert 'FEA_CMD_GET_USB_VERSION=143;' in proto
    assert 'e[0]=this.FEA_CMD_SET_MAGNETISM_REPOR,e[1]=Number(t)' in proto
    assert 'class k extends t{' in evidence['parent_class']
    assert 'from"./26f7a06d.js"' in evidence['parent_import']
    ledger=json.loads((DATA/'catalog-comparison.json').read_bytes())
    assert all(x.get('decision') and x.get('evidence') for x in ledger['differences'])
    assert sum(x['decision'].startswith('approved:') for x in ledger['differences'])==len(expected)
    # Current catalog source must still match its recorded digest when available.
    for src in ledger['catalog_sources']:
        path=ROOT/src['path']
        if path.exists():assert hashlib.sha256(path.read_bytes()).hexdigest()==src['sha256'], str(path)
    print(f"KEYBOARD_IDENTITY_AUDIT=PASS boards={ledger['known_boards']} catalogs={len(ledger['catalog_sources'])} differences={len(ledger['differences'])} aliases={len(expected)}")
def scan_local(output):
    """Re-scan saved vendor bundles; emit leads, never edit runtime admission."""
    profiles=json.loads((ROOT/'docs/research/rongyuan-stream/profiles.json').read_bytes())
    shark=json.loads((ROOT/'docs/research/attackshark-family-profiles-20260920.json').read_bytes())
    known={}
    for p in profiles:known.setdefault(p['board'],set()).add((p['vid'],p['pid']))
    for p in shark:known.setdefault(p['id'],set()).add((p['vid'],p['pid']))
    seen=set();sources=[];diff={}
    for folder in [ROOT/'.local/research',ROOT/'docs/research']:
        for path in folder.rglob('*.js'):
            if path.stat().st_size<900000:continue
            raw=path.read_bytes();sha=hashlib.sha256(raw).hexdigest()
            if sha in seen:continue
            seen.add(sha);text=raw.decode('utf8',errors='replace');count=0
            for m in re.finditer(r'\bid:(\d+),vid:(\d+),pid:(\d+),',text):
                board,vid,pid=map(int,m.groups());count+=1
                if board not in known or (vid,pid) in known[board]:continue
                diff.setdefault((board,vid,pid),[]).append({'source':str(path.relative_to(ROOT)), 'sha256':sha,'offset':m.start()})
            if count:sources.append({'source':str(path.relative_to(ROOT)),'sha256':sha,'records':count})
    report={'known_boards':len(known),'sources':sources,'differences':[{'board':b,'vid':v,'pid':p,'sources':e} for (b,v,p),e in sorted(diff.items())]}
    with Path(output).open('x',encoding='utf8') as f:json.dump(report,f,indent=2)
    print(f'IDENTITY_SCAN boards={len(known)} differences={len(diff)} output={output}')
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scan-local',metavar='NEW_JSON',help='scan saved large vendor bundles for exact board/USB differences')
    args=parser.parse_args()
    if args.scan_local:scan_local(args.scan_local)
    else:check()

