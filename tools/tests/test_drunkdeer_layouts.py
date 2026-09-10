import sys
import json
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import build_drunkdeer_layouts as b


class DrunkDeerLayouts(unittest.TestCase):
    def test_reproducible_outputs(self):
        for path,data in b.products().items(): self.assertEqual(path.read_bytes(),data,str(path))

    def test_all_variants_and_shapes(self):
        data=json.loads(b.SOURCE.read_text(encoding='utf-8'))
        audit=json.loads(b.AUDIT.read_text(encoding='utf-8'))
        for model,count in zip(b.MODELS,(82,82,83,61,68,84,86)):
            report=b.prepare(model,data,audit)
            self.assertEqual(len(report['keys']),count)
            enter=next(k for k in report['keys'] if k['hid']==40)
            self.assertEqual(bool(enter.get('notchW')),model in ('A75_iso_uk','G75JP'))
            self.assertTrue(all(k['w']>=18 and k['h']>=18 for k in report['keys']))

    def test_tracking_offsets_not_factory_index_typos(self):
        data=json.loads(b.SOURCE.read_text(encoding='utf-8'))
        audit=json.loads(b.AUDIT.read_text(encoding='utf-8'))
        for model in ('A75','A75Pro','A75_iso_uk'):
            report=b.prepare(model,data,audit)
            self.assertEqual(next(k['trackingOffset'] for k in report['keys'] if k['hid']==77),99)


if __name__=='__main__': unittest.main()
