import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_keychron_layouts as catalog


class KeychronCatalogTests(unittest.TestCase):
    def test_firmware_mutation_rejected(self):
        from unittest.mock import patch
        model,pid,_,_ = catalog.firmware.TABLES[0]
        with patch.object(Path,'read_bytes',return_value=b'changed firmware'):
            with self.assertRaisesRegex(ValueError,'Firmware hash mismatch'):
                catalog.firmware.prepare(catalog.SOURCE,model,pid)

    def test_reproducible_artifacts(self):
        for path, payload in catalog.products().items():
            self.assertEqual(path.read_bytes(), payload, str(path))

    def test_complete_key_counts_and_unique_codes(self):
        import json
        for model, expected in [('k2', 84), ('q3', 87), ('q5', 101)]:
            report = json.loads((catalog.OUTPUT/(model+'-review.json')).read_text())
            self.assertEqual(len(report['keys']), expected)
            self.assertEqual(len({k['hid'] for k in report['keys']}), expected)
            self.assertFalse(report['unresolved'])
            self.assertTrue(all(k['hid'] < 0x40a for k in report['keys']))
            self.assertEqual(len(report['excluded']), 0 if model == 'k2' else 1)

    def test_every_variant_and_compound_enter(self):
        import json
        from layout_import import read_json, ini_text
        reports = list(catalog.OUTPUT.glob('*-review.json'))
        self.assertEqual(len(reports), len(catalog.MODELS)+len(catalog.MORE)+len(catalog.firmware.TABLES))
        identities = set()
        for path in reports:
            report = json.loads(path.read_text())
            vpid = report['vendorProductId']
            self.assertNotIn(vpid,identities); identities.add(vpid)
            source = read_json(catalog.SOURCE/(str(vpid)+'-launcher.json'))[0]
            self.assertEqual(len(source['layouts']['keys']),len(report['keys'])+len(report['excluded']))
            self.assertEqual(len(report['keys']),len({k['hid'] for k in report['keys']}))
            self.assertFalse(report['unresolved'])
            self.assertTrue(all(0<k['hid']<0x40a for k in report['keys']))
            if report['name'].startswith('Keychron Q5 HE'):
                by_matrix = {tuple(k['matrix']):k['hid'] for k in report['keys']}
                self.assertEqual([by_matrix[0,c] for c in (15,16,17)],[0x403,0x404,0x405])
            shapes = [k for k in report['keys'] if k.get('notchW')]
            regional = report['name'].endswith(('ISO','JIS'))
            self.assertEqual(len(shapes),int(regional))
            if regional:
                self.assertEqual((shapes[0]['hid'],shapes[0]['notchW'],shapes[0]['notchY']),(40,12,40))
                self.assertIn('NotchW',ini_text(report))


if __name__ == '__main__':
    unittest.main()
