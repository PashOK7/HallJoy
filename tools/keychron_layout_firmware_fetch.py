"""Read official firmware images for layout-keymap research; no HID access."""
import concurrent.futures
import hashlib
import json
import sys
from keychron_catalog_fetch import ROOT, fetch, cache

OUT = ROOT/'.local/research/keychron-layout-firmware'

def download(pid):
    vpid = 0x34340000 + pid
    url = f'https://launcher.keychron.com/vapi/v2/product/{vpid}'
    metadata = fetch(url)
    record = json.loads(metadata)['data']
    firmware = record['firmware']['lasted']['path']
    payload = fetch(firmware)
    digest = hashlib.sha256(payload).hexdigest()
    cache(OUT/f'{pid:04x}-{digest}.bin', payload)
    cache(OUT/f'{pid:04x}-{digest}.json', metadata)
    return dict(pid=hex(pid), name=record['product']['name'], size=len(payload), sha256=digest)

if __name__ == '__main__':
    with concurrent.futures.ThreadPoolExecutor(6) as pool:
        for result in pool.map(download, [int(s,0) for s in sys.argv[1:]]):
            print(json.dumps(result),flush=True)
