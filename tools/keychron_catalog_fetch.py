"""Download official HE JSON sources into a new/cached research directory.

No device access or execution of downloaded code. ZIP members are read in memory,
never extracted as filesystem paths. Existing cached inputs are not overwritten.
"""
import concurrent.futures
import hashlib
import io
import json
from pathlib import Path
import re
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '.local/research/keychron-catalog'
PAGE = 'https://www.keychron.com/pages/firmware-and-json-files-of-the-keychron-he-series-keyboards'


def fetch(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'HallJoy-layout-source-research'})
    with urllib.request.urlopen(request, timeout=40) as stream:
        data = stream.read(16 * 1024 * 1024 + 1)
    if len(data) > 16 * 1024 * 1024:
        raise ValueError('Source exceeds limit')
    return data


def cache(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f'Changed cached source: {path}')
    else:
        with path.open('xb') as stream:
            stream.write(data)


def download(url):
    stem = url.split('/')[-1].split('?')[0]
    archive = OUT / stem
    raw = archive.read_bytes() if archive.exists() else fetch(url)
    cache(archive, raw)
    with zipfile.ZipFile(io.BytesIO(raw)) as bundle:
        members = [m for m in bundle.infolist() if m.filename.lower().endswith('.json') and not m.filename.startswith('__MACOSX/')]
        if len(members) != 1 or members[0].file_size > 4*1024*1024:
            raise ValueError('Ambiguous/oversized ZIP')
        payload = bundle.read(members[0])
    path = OUT / (stem + '.json')
    cache(path, payload)
    data = json.loads(payload.decode('utf-8-sig'))
    result = dict(url=url, file=path.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(payload).hexdigest(),
                  name=data.get('name'), vid=data.get('vendorId'), pid=data.get('productId'),
                  vpid=data.get('vendorProductId'))
    print(json.dumps(result), flush=True)
    return result


if __name__ == '__main__':
    page = fetch(PAGE).decode('utf-8')
    urls = sorted(set(re.findall(r'https://cdn\.shopify\.com/[^"<>\s]+_he[^"<>\s]*\.json[^"<>\s]*\.zip(?:\?[^"<>\s]*)?', page)))
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        results = list(pool.map(download, urls))
    cache(OUT/'sources.json', (json.dumps(results, indent=2)+'\n').encode())
