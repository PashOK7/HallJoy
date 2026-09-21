"""Pinned manufacturer geometry and native factory-map regression."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import prepare_aula_additional_layouts as source

class AdditionalAulaLayouts(unittest.TestCase):
    def test_source_extraction_matches_native_maps(self):
        hero,kp=source.reports()
        self.assertEqual(len(hero['keys']),84)
        self.assertEqual(len(kp['keys']),69)
        self.assertEqual(sum(k['hid']==0x34 for k in hero['keys']),1)
        self.assertEqual(hero['identity']['products'],['110000000005'])
        self.assertEqual(kp['identity']['products'],['SI2851UKKZHEARGB'])
        enter=next(k for k in kp['keys'] if k['hid']==40)
        self.assertGreater(enter['notchW'],0)
        self.assertGreater(enter['notchY'],0)
        self.assertEqual(next(k for k in kp['keys'] if k['hid']==0x87)['matrix'],[4,11])
    def test_template_label_does_not_extend_default_layer(self):
        self.assertEqual(source.array('[{name:`\' "`,pos:53}],next:[2]',0),'[{name:`\' "`,pos:53}]')
if __name__=='__main__':unittest.main()
