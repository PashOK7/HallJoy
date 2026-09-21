"""Source claims, refresh history and store ownership remain distinct."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from firmware_corpus import Corpus
from firmware_sources import import_snapshot, parse_snapshot, associate_device
from firmware_store_lock import writer_lock


class SourceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.c = Corpus(self.tmp.name)

    def tearDown(self):
        self.c.db.close()
        self.c.session.close()
        self.tmp.cleanup()

    def response(self, status=200, data=b'image'):
        response = MagicMock()
        response.__enter__.return_value = response
        response.status_code = status
        response.url = 'https://example.test/image.bin'
        response.headers = {'Content-Length': str(len(data)), 'ETag': 'v1'}
        response.iter_content.return_value = [data]
        return response

    @patch('firmware_corpus.time.sleep')
    def test_failed_refresh_preserves_last_good_and_observation_history(self, sleep):
        import requests
        response = self.response()
        self.c.session.get = MagicMock(return_value=response)
        sha = self.c.fetch(response.url)
        self.c.session.get = MagicMock(side_effect=requests.ConnectionError('offline'))
        self.assertIsNone(self.c.fetch(response.url, refresh=True))
        row = self.c.db.execute('SELECT hash,status,etag FROM fetches').fetchone()
        self.assertEqual(row, (sha, 'error', 'v1'))
        self.assertEqual(self.c.read_object(sha), b'image')
        response = self.response(304)
        self.c.session.get = MagicMock(return_value=response)
        self.assertEqual(self.c.fetch(response.url, refresh=True), sha)
        self.assertEqual(self.c.session.get.call_args.kwargs['headers']['If-None-Match'], 'v1')
        self.assertEqual(list(self.c.db.execute('SELECT status FROM fetch_observations ORDER BY id')), [('ok',), ('error',), ('ok',)])

    def test_304_without_cached_body_is_error(self):
        response = self.response(304)
        self.c.session.get = MagicMock(return_value=response)
        self.assertIsNone(self.c.fetch(response.url))
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM objects').fetchone()[0], 0)

    @patch('firmware_corpus.time.sleep')
    def test_transient_connection_failure_retries(self, sleep):
        import requests
        response = self.response()
        self.c.session.get = MagicMock(side_effect=[requests.Timeout('slow'), response])
        self.assertIsNotNone(self.c.fetch(response.url))
        self.assertEqual(self.c.session.get.call_count, 2)

    def test_manifest_import_does_not_create_retail_association(self):
        raw = json.dumps({'device': [{'product': 'OEM', 'version': '1', 'file': 'image.zip'}]}).encode()
        first = import_snapshot(self.c, 'illumi-v1', raw, 'local fixture')
        second = import_snapshot(self.c, 'illumi-v1', raw, 'local fixture')
        self.assertEqual(first, second)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM source_records').fetchone()[0], 1)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM device_associations').fetchone()[0], 0)
        self.assertEqual(self.c.db.execute('SELECT evidence_kind FROM source_snapshots').fetchone()[0], 'local-snapshot')

    def test_invalid_later_record_does_not_partially_import(self):
        raw = json.dumps({'device': [{'product': 'OK', 'file': 'image.zip'}, {'product': 'Bad', 'file': '../bad.zip'}]}).encode()
        with self.assertRaises(ValueError):
            import_snapshot(self.c, 'illumi-v1', raw, 'fixture')
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM objects').fetchone()[0], 0)

    def test_keychron_identity_and_stock_source_preserved(self):
        data = {'data': {'product': {'id': 'uuid', 'name': 'Q1 HE', 'vid': '0x3434', 'pid': '0x0900', 'vendor_product_id': '875825408'},
                         'firmware': {'lasted': {'version': '1.2', 'path': 'https://example.test/q1.bin'}}}}
        row = parse_snapshot('keychron-product-v1', json.dumps(data))[0]
        self.assertEqual(row['identity']['pid'], '0x0900')
        self.assertEqual(row['revision'], '1.2')
        self.assertNotIn('support', row)

    def test_missing_file_reference_is_not_no_analog(self):
        row = parse_snapshot('illumi-v1', json.dumps({'device': [{'product': 'Unknown', 'file': ''}]}))[0]
        self.assertEqual(row['state'], 'no-file-reference')
        self.assertIsNone(row['url'])

    def test_rongyuan_error_and_identity_mismatch(self):
        row = {'id': 1, 'vid': 2, 'pid': 3, 'name': 'OEM', 'display': 'Retail label', 'http': 500, 'error': 'not found'}
        self.assertEqual(parse_snapshot('rongyuan-research-v1', json.dumps([row]))[0]['state'], 'source-error')
        row.update(http=200, metadata={'data': {'dev_id': 99}})
        with self.assertRaises(ValueError):
            parse_snapshot('rongyuan-research-v1', json.dumps([row]))

    def association_fixture(self):
        snapshot = import_snapshot(self.c, 'illumi-v1', json.dumps({'device': [{'product': 'OEM'}]}).encode(), 'fixture')
        self.c.db.executemany('INSERT INTO models VALUES (?,?,?,?)', [('Brand A', 'Model', 'Not investigated', snapshot['snapshot']), ('Brand B', 'Model', 'Not investigated', snapshot['snapshot'])])
        self.c.db.commit()
        return {'schema': 1, 'state': 'reviewed',
                'source': {'snapshot': snapshot['snapshot'], 'adapter': 'illumi-v1', 'key': '0'},
                'device': {'brand': 'Brand A', 'model': 'Model', 'hardware_revision': None, 'layout': None, 'transport': 'USB'},
                'evidence_sha256': self.c.put(b'explicit mapping fixture; not hardware evidence')}

    def test_association_preserves_unknowns_brands_and_support_status(self):
        claim = self.association_fixture()
        first = associate_device(self.c, claim)
        claim['device']['brand'] = 'Brand B'
        second = associate_device(self.c, claim)
        self.assertNotEqual(first, second)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM device_associations WHERE hardware_revision IS NULL AND layout IS NULL').fetchone()[0], 2)
        self.assertEqual(set(r[0] for r in self.c.db.execute('SELECT status FROM models')), {'Not investigated'})

    def test_association_rejects_unknown_catalog_entry_or_source(self):
        claim = self.association_fixture()
        claim['device']['model'] = 'guess'
        with self.assertRaises(ValueError):
            associate_device(self.c, claim)
        claim['device']['model'] = 'Model'
        claim['source']['key'] = 'missing'
        with self.assertRaises(ValueError):
            associate_device(self.c, claim)

    def test_cli_lock_rejects_second_process_and_releases(self):
        script = ('import sys; sys.path.insert(0, sys.argv[1]); '
                  'from firmware_store_lock import writer_lock; '
                  'lock=writer_lock(sys.argv[2]); lock.__enter__(); lock.__exit__(None,None,None)')
        args = [sys.executable, '-c', script, str(Path(__file__).resolve().parents[1]), self.tmp.name]
        with writer_lock(self.tmp.name):
            result = subprocess.run(args, capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'another corpus writer', result.stderr)
        result = subprocess.run(args, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
