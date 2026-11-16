"""Full source-backed layout checks; run locally when recording public references."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
BRANDS = ('Keychron', 'Lemokey', 'DrunkDeer', 'Aula', 'Redragon', 'Razer',
          'NuPhy', 'Wooting', 'IROK', 'MADLIONS', 'ATK', 'IPI', 'SayoDevice',
          'MonsGeek', 'EPOMAKER', 'Chilkey', 'Everglide', 'WLMOUSE', 'Logitech G',
          'Weikav')


def main():
    suite = unittest.defaultTestLoader.discover(str(ROOT / 'tools/tests'), pattern='test_layout_pipeline.py')
    result = unittest.TextTestRunner().run(suite)
    if not result.wasSuccessful():
        raise RuntimeError('Source-backed layout tests failed')
    import layout_pipeline
    catalog = layout_pipeline.read_catalog()
    shared_runtime = None
    verified = set()
    for brand in BRANDS:
        spec = catalog[brand]
        if spec.get('runtime') and spec['adapter'] in layout_pipeline.MANIFEST_ADAPTERS:
            # runtime_products validates ALL integrated brands, including those
            # outside BRANDS. The old per-brand CLI regenerated this whole tree
            # repeatedly, even in20 parallel processes. Keep this cache local to
            # one immutable check pass; no stale cache survives a later call.
            if shared_runtime is None:
                shared_runtime = layout_pipeline.runtime_products()
            outputs, reports, integrated = shared_runtime, layout_pipeline.reports_for(spec), True
        else:
            outputs, reports, integrated = layout_pipeline.products(brand, spec)
        if id(outputs) not in verified:
            changed = [name for name, data in outputs.items()
                       if not layout_pipeline.local_path(ROOT, name).is_file()
                       or layout_pipeline.local_path(ROOT, name).read_bytes() != data] if integrated else []
            if changed:
                raise RuntimeError('Generated outputs differ: ' + ', '.join(changed))
            # Only shared_runtime is reused; legacy dict objects may reuse IDs.
            if outputs is shared_runtime:
                verified.add(id(outputs))
        print(f'LAYOUT_PIPELINE=PASS brand={brand} models={len(reports)} '
              f'keys={sum(len(r["keys"]) for r in reports)} files={len(outputs)} '
              f'changed=0 integrated={str(integrated).lower()}')



if __name__ == '__main__':
    main()
