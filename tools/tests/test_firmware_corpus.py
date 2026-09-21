"""Acquisition invariants: dedup, lineage, archive rejection and cache integrity."""
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from firmware_corpus import Corpus, listing_fields, listing_members, atomic_write, digest


def archive(name, content):
    out = io.BytesIO()
    with zipfile.ZipFile(out, 'w') as z:
        z.writestr(name, content)
    return out.getvalue()


class CorpusTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.c = Corpus(self.tmp.name)

    def tearDown(self):
        self.c.db.close()
        self.c.session.close()
        self.tmp.cleanup()

    def test_dedup_preserves_both_origins(self):
        a = self.c.put(b'image', 'source-a')
        b = self.c.put(b'image', 'source-b')
        self.assertEqual(a, b)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM origins').fetchone()[0], 2)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM objects').fetchone()[0], 1)

    def test_repeated_origin_preserves_first_observation(self):
        self.c.put(b'image', 'source')
        first = self.c.db.execute('SELECT * FROM origins').fetchall()
        self.c.put(b'image', 'source')
        self.assertEqual(first, self.c.db.execute('SELECT * FROM origins').fetchall())

    def test_nested_zip_lineage_and_repeat(self):
        self.c.put(archive('inner.zip', archive('firmware.bin', b'image')))
        self.c.expand()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0], 2)
        self.c.expand()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0], 2)

    def test_unsafe_zip_cannot_create_payload(self):
        sha = self.c.put(archive('../../outside.bin', b'image'))
        self.c.expand()
        result = json.loads(self.c.db.execute('SELECT result FROM analysis WHERE hash=?', (sha,)).fetchone()[0])
        self.assertEqual(result['status'], 'blocked')
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0], 0)

    def test_corrupt_existing_blob_refused(self):
        sha = self.c.put(b'image')
        self.c.path(sha).write_bytes(b'corrupted')
        with self.assertRaises(ValueError):
            self.c.put(b'image')

    def test_sheet_blank_separator_does_not_leak_brand(self):
        path = Path(self.tmp.name) / 'sheet.json'
        path.write_text(json.dumps({'values': [['Brand', 'Model', 'Status'], ['A', 'One', 'OK'], [], ['', 'Two', 'OK']]}))
        with self.assertRaises(ValueError):
            self.c.sheet(path)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM models').fetchone()[0], 0)

    def test_fingerprint_is_not_support_claim(self):
        self.c.put(b'AA66\x55\xfb\x23' * 50)
        r = self.c.report()
        self.assertEqual(r['kinds'], {'unclassified': 1})

    def test_native_listing_empty_fields(self):
        fields = listing_fields('Path = example.bin\nCreated = \nSize = 20\n')
        self.assertEqual(fields, {'Path': 'example.bin', 'Created': '', 'Size': '20'})

    def test_native_listing_preserves_multiple_windows_members(self):
        rows = listing_members('Header\r\n----------\r\nPath = a\r\nSize = 1\r\n\r\nPath = b\r\nSize = 2\r\n')
        self.assertEqual([r['Path'] for r in rows], ['a', 'b'])

    def test_successful_download_is_not_requested_again(self):
        response = MagicMock()
        response.__enter__.return_value = response
        response.status_code = 200
        response.url = 'https://example.test/image.bin'
        response.headers = {'Content-Length': '5'}
        response.iter_content.return_value = [b'image']
        self.c.session.get = MagicMock(return_value=response)
        first = self.c.fetch(response.url)
        second = self.c.fetch(response.url)
        self.assertEqual(first, second)
        self.c.session.get.assert_called_once()

    @patch("firmware_corpus.time.sleep")
    def test_failed_download_has_cooldown(self, sleep):
        import requests
        self.c.session.get = MagicMock(side_effect=requests.ConnectionError('offline'))
        self.assertIsNone(self.c.fetch('https://example.test/image.bin'))
        self.assertIsNone(self.c.fetch('https://example.test/image.bin'))
        self.assertEqual(self.c.session.get.call_count, 3)


    def test_atomic_publish_failure_leaves_no_object(self):
        data = b'firmware'
        with patch('firmware_corpus.os.replace', side_effect=OSError('interrupted')):
            with self.assertRaises(OSError):
                self.c.put(data)
        self.assertFalse(self.c.path(digest(data)).exists())
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM objects').fetchone()[0], 0)
        self.assertEqual(list((self.c.root / 'objects').rglob('.stage-*')), [])
        self.assertEqual(self.c.put(data), digest(data))

    def test_atomic_publish_refuses_changed_target(self):
        target = self.c.root / 'report.json'
        target.write_bytes(b'owner change')
        with self.assertRaises(RuntimeError):
            atomic_write(target, b'replacement', expected=digest(b'old'))
        self.assertEqual(target.read_bytes(), b'owner change')

    def test_zip_crc_failure_rolls_back_all_indexed_children(self):
        out = io.BytesIO()
        with zipfile.ZipFile(out, 'w', compression=zipfile.ZIP_STORED) as z:
            z.writestr('first.bin', b'first payload')
            z.writestr('second.bin', b'second payload')
        damaged = out.getvalue().replace(b'second payload', b'Second payload')
        parent = self.c.put(damaged)
        self.c.expand()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0], 0)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM objects').fetchone()[0], 1)
        self.assertEqual(self.c.db.execute('SELECT state FROM jobs WHERE hash=?', (parent,)).fetchone()[0], 'blocked')
        self.c.expand()
        self.assertEqual(self.c.db.execute('SELECT attempts FROM jobs').fetchone()[0], 1)
        self.c.expand(retry_blocked=True)
        self.assertEqual(self.c.db.execute('SELECT attempts FROM jobs').fetchone()[0], 2)

    def test_interrupted_zip_resumes_without_partial_lineage(self):
        out = io.BytesIO()
        with zipfile.ZipFile(out, 'w') as z:
            z.writestr('first.bin', b'first')
            z.writestr('second.bin', b'second')
        self.c.put(out.getvalue())
        original = self.c.put
        calls = 0
        def interrupted(data, **kwargs):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise KeyboardInterrupt()
            return original(data, **kwargs)
        with patch.object(self.c, 'put', side_effect=interrupted):
            with self.assertRaises(KeyboardInterrupt):
                self.c.expand()
        self.assertEqual(self.c.db.execute('SELECT state FROM jobs').fetchone()[0], 'running')
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0], 0)
        self.c.db.close()
        self.c.session.close()
        self.c = Corpus(self.tmp.name)
        self.c.expand()
        self.assertEqual(self.c.db.execute('SELECT state,attempts FROM jobs').fetchone(), ('complete', 2))
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0], 2)

    def test_budget_deferred_job_resumes_next_run(self):
        self.c.put(archive('one', b'first'))
        self.c.put(archive('two', b'other'))
        with patch('firmware_corpus.TOTAL_EXPANSION', 5):
            self.c.expand()
            self.assertEqual(sorted(r[0] for r in self.c.db.execute('SELECT state FROM jobs')), ['complete', 'deferred'])
            self.c.expand()
        self.assertEqual([r[0] for r in self.c.db.execute('SELECT state FROM jobs')], ['complete', 'complete'])

    def test_nesting_limit_survives_restart_and_explicit_retry(self):
        data = b'payload'
        for i in range(6):
            data = archive(str(i), data)
        self.c.put(data, 'local acquisition')
        self.c.expand()
        first_count = self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0]
        self.assertEqual(first_count, 4)
        self.c.expand(retry_blocked=True)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0], first_count)
        self.assertEqual(self.c.db.execute("SELECT COUNT(*) FROM jobs WHERE state='blocked'").fetchone()[0], 1)

    def test_legacy_edges_preserve_depth(self):
        parent = self.c.put(b'legacy package', 'source')
        for i in range(4):
            child = self.c.put(archive(str(i), b'image'))
            self.c.db.execute('INSERT INTO edges VALUES (?,?,?,?)', (parent, child, str(i), 'static-v2'))
            parent = child
        self.c.db.commit()
        self.assertEqual(self.c.lineage_depths()[parent], 4)
        self.c.expand()
        self.assertEqual(self.c.db.execute('SELECT state FROM jobs WHERE hash=?', (parent,)).fetchone()[0], 'blocked')

    def test_report_checks_blob_integrity(self):
        sha = self.c.put(b'image')
        self.c.path(sha).write_bytes(b'broken')
        with self.assertRaises(ValueError):
            self.c.report()



    def test_native_publication_failure_rolls_back_all_children(self):
        self.c.put(b'Rar!fixture')
        helper = self.c.root / '7z.exe'
        helper.write_bytes(b'fixture only; never executed')
        def run(args, *limits):
            if args[1] == 'l':
                return b'Header\n----------\nPath = a\nSize = 1\n\nPath = b\nSize = 1\n'
            return args[-1].encode()
        original = self.c.put
        def fail_second(data, **kwargs):
            if data == b'b':
                raise OSError('simulated disk error')
            return original(data, **kwargs)
        with patch('firmware_corpus.SEVENZIP', helper), patch('firmware_pipeline.bounded_process', side_effect=run), patch.object(self.c, 'put', side_effect=fail_second):
            self.c.extract_native()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM objects').fetchone()[0], 1)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0], 0)
        self.assertEqual(json.loads(self.c.db.execute('SELECT result FROM analysis').fetchone()[0])['status'], 'blocked')

    def test_failed_crc_still_consumes_run_budget(self):
        broken = archive('bad', b'bad payload').replace(b'bad payload', b'Bad payload')
        damaged = self.c.put(broken)
        good = self.c.put(archive('good', b'good payload'))
        # Make queue ordering deterministic without relying on hash ordering.
        for i in range(1000):
            if damaged < good:
                break
            self.c.db.execute('DELETE FROM objects WHERE hash=?', (good,))
            good = self.c.put(archive(str(i), b'good payload'))
        self.assertLess(damaged, good)
        with patch('firmware_corpus.TOTAL_EXPANSION', 12):
            self.c.expand()
        states = dict(self.c.db.execute('SELECT hash,state FROM jobs'))
        self.assertEqual(states, {damaged: 'blocked', good: 'deferred'})



if __name__ == '__main__':
    unittest.main()
