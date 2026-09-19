"""Reproduce the Hex80 layout from locked official data without executing JS."""
import argparse
from decimal import Decimal, ROUND_HALF_UP
import gzip
import hashlib
import json
from pathlib import Path
import layout_import as common
import layout_pipeline as pipeline

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/research/remaining-layout-sources-20260914'
BASE = 'https://bpcdn.atkgear.com/hub-v3/production/3.2.25/'
LOCKS = {
    'atk-index-M8vylvoC.js.gz': '1d99355fca265bf65bff563559893a4af7aaef4aecfbbcca66186f3eeed9e565',
    'atk-hex80-demo.json': '7b55cc322153925fdc2afb242236710adc48592306ac39d25e9a68976f380201',
}

def prepare():
    for name, digest in LOCKS.items():
        common.require(hashlib.sha256((SOURCE/name).read_bytes()).hexdigest() == digest, 'Source drift: '+name)
    script = gzip.decompress((SOURCE/'atk-index-M8vylvoC.js.gz').read_bytes())
    common.require(hashlib.sha256(script).hexdigest() == '9e597801d933b5956c58f044a3743caa9ecd975a93806cf8dd798be0da4a7ac3', 'Uncompressed source drift')
    text = script.decode('utf-8')
    common.require('e(`${t}demo/hex80.json`,4471)' in text, 'Hex80 demo/PID binding missing')
    common.require('"0x1176":"hex80","0x1177":"hex80","0x1250":"hex80"' in text, 'Model selectors changed')
    common.require('const o=(s-4*(e.length-1)-a*i)/(t+n)' in text and 'n.x=r,n.y=d+l,n.width=c,n.height=u' in text, 'Renderer changed')
    data = json.loads((SOURCE/'atk-hex80-demo.json').read_bytes(), parse_float=Decimal)
    rows = data['keyActions'][0]
    common.require(len(rows) == 6 and sum(map(len,rows)) == 88, 'Physical count changed')
    labels = {hid: label for hid,label in common.CODES.values()}
    labels[0x409] = 'Fn'
    def value(key, name, default): return Decimal(key.get(name, default))
    width = max(sum((value(k,'w',1)+value(k,'ml',0)+value(k,'mr',0))*50 for k in row)+4*(len(row)-1) for row in rows)
    def px(x): return int((x*Decimal(42)/50).quantize(Decimal(1), rounding=ROUND_HALF_UP))
    keys, omitted = [], []
    for row_index, row in enumerate(rows):
        right = sum(value(k,'mr',0) for k in row)*50
        unit = (width-4*(len(row)-1)-right)/sum(value(k,'w',1)+value(k,'ml',0) for k in row)
        x = Decimal(0)
        for index, key in enumerate(row):
            x += value(key,'ml',0)*unit + (4 if index else 0)
            y = Decimal(row_index*54)+value(key,'mt',0)
            w,h = value(key,'w',1)*unit,value(key,'h',1)*50
            code = key['defaultKey']
            if code == [0,168]:
                common.require(key['label'] == 'extraFunction.mute', 'Unknown media action')
                omitted.append({'position':key['position'],'reason':'Vendor mute action has no HallJoy analog HID; geometry gap retained.'})
            else:
                hid = 0x409 if code == [82,33] else code[1] if code[0] == 0 else None
                common.require(hid in labels, 'Unknown default action: '+str(code))
                keys.append(dict(hid=hid,label=labels[hid],x=px(x),y=px(y),w=px(x+w)-px(x),h=px(y+h)-px(y)))
            x += w+value(key,'mr',0)*50
    common.require(len(keys) == 87 and len(omitted) == 1, 'Unexpected omitted keys')
    sources = [dict(path=str((SOURCE/name).relative_to(ROOT)).replace(chr(92),'/'),sha256=digest,
                    url=BASE+('static/index-M8vylvoC.js' if name.endswith('.gz') else 'demo/hex80.json')) for name,digest in LOCKS.items()]
    report = dict(schema=1,id='atk_hex80_ansi',brand='ATK',model='Hex80',variant='ANSI',name='ATK Hex80 ANSI',
                  status='ready',unresolved=[],keys=keys,identity=dict(protocol='hex80',products=['HEX80-ANSI']),sources=sources,
                  notes=['Official ATK x QK Hex80 demo for PID 1177; 88 physical controls, 87 represented keys.',
                         'Renderer: 50px base, 4px gaps, per-row width fitting; normalized to 42px key height.',
                         'Vendor mute omitted without collapsing its gap; native Fn is published as extended HID 0x409.',
                         'Verified Hex80 sessions identify this ANSI layout. Native matrix covers all 87 displayed factory keys; live remap reading is not implemented.',
                         'Do not use the stale keymaps/hex80.json default actions or the RGB LED matrix as keyboard bindings.',
                         'Compressed bundle is a byte-preserving source cache, not a shipped dependency.'],omitted=omitted)
    pipeline.validate_report(report)
    return report

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--write-new',action='store_true');args=parser.parse_args()
    report=prepare();path=ROOT/'docs/research/remaining-layout-reports-20260914/atk_hex80_ansi.json'
    data=(json.dumps(report,indent=2)+'\n').encode()
    if args.write_new:
        path.parent.mkdir(exist_ok=True);path.open('xb').write(data)
    else: common.require(path.read_bytes()==data,'Report drift')
    print('ATK_HEX80_LAYOUT=PASS keys=87 omitted_vendor_mute=1 source_locks=1')

if __name__ == '__main__': main()
