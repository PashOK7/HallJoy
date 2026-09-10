import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import build_lemokey_layouts as b


class LemokeyLayouts(unittest.TestCase):
    def test_exact_variants(self):
        for variant,count,pid in [('ansi',81,0x610),('iso',82,0x611)]:
            r=b.prepare(variant)
            self.assertEqual(r['name'],'Lemokey P1 HE '+variant.upper())
            self.assertEqual(r['vendorProductId'],0x362D0000+pid)
            self.assertEqual(len(r['keys']),count)
            self.assertEqual(len(r['excluded']),1)
            self.assertEqual(r['excluded'][0]['matrix'],[0,14])
            self.assertIn('Brand=Lemokey',b.m.ini_text(r))
            by_hid={k['hid']:k for k in r['keys']}
            self.assertEqual(by_hid[0x409]['matrix'],[5,10])
            self.assertEqual(by_hid[231]['matrix'],[5,9])
            self.assertEqual(bool(by_hid[40].get('notchW')),variant=='iso')
            self.assertEqual(len(by_hid),count)

    def test_unsupported_variant(self):
        with self.assertRaises(b.m.ImportErrorDetail): b.prepare('jis')

    def test_reproducible(self):
        for path,data in b.products().items(): self.assertEqual(path.read_bytes(),data,str(path))


if __name__=='__main__': unittest.main()
