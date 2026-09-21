"""Dispatch is exact-image only; fixtures mock execution and are not MCU evidence."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from firmware_corpus import Corpus
from firmware_research_runner import SUITES, run_suite


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.c = Corpus(self.tmp.name)
        self.sha = self.c.put(b'fixture')
        self.image = self.c.pipeline().record_image(self.sha, 'fixture', 'known-image',
                          [{'address': None, 'sha256': self.sha, 'size': 7}])
        self.c.db.commit()
        self.suite = copy.deepcopy(SUITES['ajazz-sg8994he-raw-v1'])
        self.suite.update(sha256=self.sha, input_size=7)
        self.registry = patch.dict(SUITES, {'fixture-suite': self.suite})
        self.dependencies = patch('firmware_research_runner.dependency_snapshot', return_value={'fixture': 'v1'})
        self.registry.start()
        self.dependencies.start()

    def tearDown(self):
        self.dependencies.stop()
        self.registry.stop()
        self.c.db.close()
        self.c.session.close()
        self.tmp.cleanup()

    def test_different_image_never_dispatched(self):
        with patch('firmware_research_runner.bounded_process') as execute:
            with self.assertRaises(ValueError):
                run_suite(self.c, 'ajazz-sg8994he-raw-v1', self.image)
            execute.assert_not_called()

    def test_unknown_suite_refused(self):
        with self.assertRaises(ValueError):
            run_suite(self.c, 'unknown', self.image)

    def test_repeat_reuses_receipt_without_execution(self):
        with patch('firmware_research_runner.bounded_process', return_value=(0, b'RAW_STREAM=PASS:', b'')) as execute:
            first = run_suite(self.c, 'fixture-suite', self.image)
            second = run_suite(self.c, 'fixture-suite', self.image)
        self.assertEqual(first['state'], 'pass')
        self.assertEqual(second['state'], 'cached')
        self.assertEqual(first['receipt'], second['receipt'])
        execute.assert_called_once()

    def test_exit_zero_without_success_marker_is_failure(self):
        with patch('firmware_research_runner.bounded_process', return_value=(0, b'wrong output', b'')):
            result = run_suite(self.c, 'fixture-suite', self.image)
        self.assertEqual(result['state'], 'fail')

    def test_runner_timeout_is_inconclusive_not_no_analog(self):
        with patch('firmware_research_runner.bounded_process', side_effect=TimeoutError('timeout')):
            result = run_suite(self.c, 'fixture-suite', self.image)
        self.assertEqual(result['state'], 'inconclusive')
        receipt = json.loads(self.c.db.execute('SELECT receipt FROM evidence_receipts').fetchone()[0])
        self.assertIn('No hardware', receipt['limitations'][-1])

    def test_assertions_enabled_and_files_isolated(self):
        def execute(args, *limits, **kwargs):
            self.assertEqual(args[1:3], ['-E', '-B'])
            entry = Path(args[3])
            self.assertNotEqual(entry.parent.resolve(), Path(__file__).resolve().parents[1])
            self.assertEqual((entry.parents[1] / self.suite['input_path']).read_bytes(), b'fixture')
            return 0, b'RAW_STREAM=PASS:', b''
        with patch('firmware_research_runner.bounded_process', side_effect=execute):
            self.assertEqual(run_suite(self.c, 'fixture-suite', self.image)['state'], 'pass')

    def test_explicit_rerun_preserves_both_runs(self):
        with patch('firmware_research_runner.bounded_process', return_value=(0, b'RAW_STREAM=PASS:', b'')) as execute:
            run_suite(self.c, 'fixture-suite', self.image)
            run_suite(self.c, 'fixture-suite', self.image, rerun=True)
        self.assertEqual(execute.call_count, 2)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM research_runs').fetchone()[0], 2)


if __name__ == '__main__':
    unittest.main()
