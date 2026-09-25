"""Offline pinned-source / complete experimental notice reconciliation."""
import hashlib
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'docs/research/eweadn-sparklink'

def main():
    evidence = json.loads((BASE/'catalog.json').read_bytes())
    sources = {}
    for source in evidence['sources']:
        p = BASE / source['url'].rsplit('/', 1)[-1]
        raw = p.read_bytes()
        assert hashlib.sha256(raw).hexdigest() == source['sha256'], p
        sources[p.name] = raw.decode('utf8')
    sdk = sources['main.ad1dfc2d0c775fcba730.js']
    catalog = sources['main.918cf4f3a4f3b493296c.js']
    for record in evidence['records']:
        assert record['source_literal'] in catalog
    for anchor in ['e[e.Keyboard=1]="Keyboard"', 'e[e.DeviceInfo=2]="DeviceInfo"',
                   'e[e.AxisData=3]="AxisData"', 'e[e.Route=1]="Route"',
                   'return[Es,Is,Ms,t]', 'return[Ji,es,t,n]',
                   'keyLayoutResult(e)', 'getRouteData(e)',
                   'getRoute=async e=>this.performanceController.getRoute(e)',
                   'this.deviceBase.sendProtocol(()=>Ru.getRoute(e),e=>Ru.getRouteData(e))',
                   'this.device.sendReport(0,e)']:
        assert anchor in sdk, anchor
    adapter = sources['main.sdk-kbd-xingshan.b9e6b4809170a4d18bfc.js']
    assert '1===t.usage&&65456===t.usagePage' in adapter
    notices = json.loads((ROOT/'docs/development/keyboard_support_notices.json').read_bytes())
    group = next(g for g in notices['groups'] if g['id']=='SparkLinkV2')
    profile = (ROOT/'src/HallJoyProject/HallJoy/sparklink_model_profiles.h').read_text()
    backend = (ROOT/'src/HallJoyProject/HallJoy/backend_sparklink.inc').read_text()
    for anchor in ['{ 0x01, 0x02 }', '{ 0x03, 0x01, 0x00, row }', '{ 0x04, 0x03, 0x01, row }',
                   'routeProbe[2] != 0x01', 'kSparkColsPerRow = 21']:
        assert anchor in backend, anchor
    pairs = {(m['brand'],m['model']) for m in group['models']}
    pids = set()
    for model in evidence['models']:
        assert (model['brand'], model['model']) in pairs
        for pid in model['pids']:
            assert any(r['pid']==pid and r['supported'] for r in evidence['records'])
            assert f'1CA6{pid:04X}' in group['tokens']
            assert f'case 0x{pid:04x}:' in profile
            pids.add(pid)
    assert not {7213,7230,65153} & pids
    print(f'EWEADN_SPARKLINK_SOURCE=PASS models={len(evidence["models"])} identities={len(pids)}')

if __name__ == '__main__':
    main()
