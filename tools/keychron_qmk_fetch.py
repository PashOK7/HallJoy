"""Cache pinned official QMK model sources; never execute them or touch HID."""
import concurrent.futures
import json
from keychron_catalog_fetch import fetch, cache, ROOT

COMMIT = '9ada9b7baecb9591c469b9b068146ac5891a480a'
DEST = ROOT/'docs/research/keychron-catalog-sources/qmk-2025q3'

if __name__ == '__main__':
    tree = json.loads(fetch(f'https://api.github.com/repos/Keychron/qmk_firmware/git/trees/{COMMIT}?recursive=1'))
    prefix = 'keyboards/keychron/'
    paths = [x['path'] for x in tree['tree'] if x['path'].startswith(prefix) and '_he/' in x['path']
             and x['path'].endswith(('/keyboard.json', '/keymaps/default/keymap.c'))]
    def download(path):
        target = DEST/path.removeprefix(prefix)
        if not target.exists():
            cache(target, fetch(f'https://raw.githubusercontent.com/Keychron/qmk_firmware/{COMMIT}/{path}'))
        return path
    with concurrent.futures.ThreadPoolExecutor(6) as pool:
        print('PINNED_QMK_FILES', len(list(pool.map(download, paths))))
