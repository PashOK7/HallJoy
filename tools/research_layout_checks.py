"""Full source-backed layout checks; run locally when recording public references."""
import concurrent.futures
import os
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
BRANDS = ('Keychron', 'Lemokey', 'DrunkDeer', 'Aula', 'Redragon', 'Razer',
          'NuPhy', 'Wooting', 'IROK', 'MADLIONS', 'ATK', 'IPI', 'SayoDevice',
          'MonsGeek', 'EPOMAKER', 'Chilkey', 'Everglide', 'WLMOUSE', 'Logitech G')


def main():
    suite = unittest.defaultTestLoader.discover(str(ROOT / 'tools/tests'), pattern='test_layout_pipeline.py')
    result = unittest.TextTestRunner().run(suite)
    if not result.wasSuccessful():
        raise RuntimeError('Source-backed layout tests failed')
    if os.environ.get('HALLJOY_RESEARCH_TRACE') == '1':
        # Recording traces file reads in this process only: stay in-process so
        # every layout input is pinned (research_reference_checks.py --record).
        import layout_pipeline
        for brand in BRANDS:
            sys.argv = ['layout_pipeline.py', 'check', brand]
            if layout_pipeline.main() != 0:
                raise RuntimeError('Source-backed layout check failed: ' + brand)
        return
    # Each brand check is a full independent read-only run; run them as
    # parallel processes and report in the fixed brand order.
    def check(brand):
        return subprocess.run([sys.executable, '-X', 'utf8', str(ROOT / 'tools/layout_pipeline.py'), 'check', brand],
                              capture_output=True, text=True, encoding='utf8', errors='replace', timeout=300)
    with concurrent.futures.ThreadPoolExecutor(max_workers=len(BRANDS)) as pool:
        results = list(pool.map(check, BRANDS))
    for brand, result in zip(BRANDS, results):
        sys.stdout.write(result.stdout); sys.stderr.write(result.stderr)
        if result.returncode != 0:
            raise RuntimeError('Source-backed layout check failed: ' + brand)


if __name__ == '__main__':
    main()
