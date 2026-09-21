"""Evidence assertions never become independently verified executions on import."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from firmware_corpus import Corpus
from firmware_evidence import validate_and_record
from test_firmware_pipeline import record


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.c = Corpus(self.tmp.name)
        self.c.put(record(0, 32, b'abc') + record(1))
        self.c.extract()
        identity, layout = self.c.db.execute('SELECT identity,layout FROM images').fetchone()
        self.receipt = {'schema': 1, 'image': identity, 'level': 'synthetic-test', 'outcome': 'pass',
                        'input_hashes': [s['sha256'] for s in json.loads(layout)['segments']],
                        'producer': {'name': 'fixture', 'version': '1'}, 'parameters': {},
                        'limitations': ['Synthetic evidence-contract fixture; no MCU execution.'],
                        'assets': [{'role': 'producer-code', 'sha256': self.c.put(b'fixture code')},
                                   {'role': 'raw-result', 'sha256': self.c.put(b'fixture result')}]}

    def tearDown(self):
        self.c.db.close()
        self.c.session.close()
        self.tmp.cleanup()

    def test_idempotent_receipt_remains_imported(self):
        first = validate_and_record(self.c, self.receipt)
        self.assertEqual(first, validate_and_record(self.c, self.receipt))
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM evidence_receipts').fetchone()[0], 1)
        self.assertIn('not-rerun', self.c.db.execute('SELECT verification FROM evidence_receipts').fetchone()[0])

    def test_wrong_image_bytes_rejected(self):
        self.receipt['input_hashes'] = ['0' * 64]
        with self.assertRaises(ValueError):
            validate_and_record(self.c, self.receipt)

    def test_corrupt_result_rejected(self):
        self.c.path(self.receipt['assets'][1]['sha256']).write_bytes(b'corrupted')
        with self.assertRaises(ValueError):
            validate_and_record(self.c, self.receipt)

    def test_emulation_requires_mock_scope(self):
        self.receipt['level'] = 'mcu-emulation'
        with self.assertRaises(ValueError):
            validate_and_record(self.c, self.receipt)
        self.receipt['parameters']['mocked_peripherals'] = ['ADC', 'USB endpoint']
        validate_and_record(self.c, self.receipt)

    def test_hardware_requires_device_identity(self):
        self.receipt['level'] = 'hardware-capture'
        with self.assertRaises(ValueError):
            validate_and_record(self.c, self.receipt)

    def test_historical_reference_cannot_claim_new_pass(self):
        self.receipt['level'] = 'historical-reference'
        with self.assertRaises(ValueError):
            validate_and_record(self.c, self.receipt)
        self.receipt['outcome'] = 'reference-only'
        self.receipt['assets'][1]['role'] = 'reference'
        validate_and_record(self.c, self.receipt)

    def test_changed_parameters_keep_both_receipts(self):
        first = validate_and_record(self.c, self.receipt)
        other = copy.deepcopy(self.receipt)
        other['parameters']['scenario'] = 'different'
        self.assertNotEqual(first, validate_and_record(self.c, other))
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM evidence_receipts').fetchone()[0], 2)


if __name__ == '__main__':
    unittest.main()
