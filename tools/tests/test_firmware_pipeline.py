"""Mixed-format extraction, addressed images, dependency isolation and recovery."""
import io
import json
import queue
import threading
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from firmware_corpus import Corpus, SEVENZIP
from firmware_pipeline import Blocked, Pipeline, bounded_process, intel_hex


def record(kind, address=0, data=b''):
    raw = bytes([len(data)]) + address.to_bytes(2, 'big') + bytes([kind]) + data
    return b':' + (raw + bytes([-sum(raw) & 255])).hex().encode() + b'\n'


def packed(name, payload):
    out = io.BytesIO()
    with zipfile.ZipFile(out, 'w') as archive:
        archive.writestr(name, payload)
    return out.getvalue()


def resource_pe(payload):
    """Minimal non-executable PE fixture with a single RCDATA resource tree."""
    raw_size = ((0x100 + len(payload) + 511) // 512) * 512
    data = bytearray(0x200 + raw_size)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 0x84, 0x14c, 1, 0, 0, 0, 224, 0x102)
    opt = 0x98
    struct.pack_into('<H', data, opt, 0x10b)
    struct.pack_into('<I', data, opt + 28, 0x400000)
    struct.pack_into('<II', data, opt + 32, 0x1000, 0x200)
    struct.pack_into('<II', data, opt + 56, 0x2000, 0x200)
    struct.pack_into('<I', data, opt + 92, 16)
    struct.pack_into('<II', data, opt + 112, 0x1000, 0x100 + len(payload))
    section = opt + 224
    data[section:section+8] = b'.rsrc\0\0\0'
    struct.pack_into('<IIII', data, section + 8, 0x100 + len(payload), 0x1000, raw_size, 0x200)
    struct.pack_into('<I', data, section + 36, 0x40000040)
    for offset, ident, target in [(0, 10, 0x80000018), (24, 1, 0x80000030), (48, 1033, 72)]:
        struct.pack_into('<HH', data, 0x200 + offset + 12, 0, 1)
        struct.pack_into('<II', data, 0x200 + offset + 16, ident, target)
    struct.pack_into('<IIII', data, 0x200 + 72, 0x1100, len(payload), 0, 0)
    data[0x300:0x300+len(payload)] = payload
    return bytes(data)


class HexTests(unittest.TestCase):
    def test_sparse_address_extensions_and_no_fill(self):
        raw = record(4, data=b'\x08\x00') + record(0, 16, b'abc') + record(0, 32, b'def') + record(1)
        segments, entry = intel_hex(raw, 1024)
        self.assertEqual(segments, [(0x08000010, b'abc'), (0x08000020, b'def')])
        self.assertIsNone(entry)

    def test_segment_address_and_start_preserved(self):
        raw = record(2, data=b'\x10\x00') + record(0, 16, b'abc') + record(3, data=b'\x10\x00\x00\x10') + record(1)
        segments, entry = intel_hex(raw, 1024)
        self.assertEqual(segments, [(0x10010, b'abc')])
        self.assertEqual(entry, {'kind': 3, 'data': '10000010'})

    def test_equivalent_record_order_and_boundaries(self):
        a = record(0, 1, b'abc') + record(1)
        b = record(0, 2, b'bc') + record(0, 1, b'a') + record(1)
        self.assertEqual(intel_hex(a, 100), intel_hex(b, 100))

    def test_invalid_records_are_rejected(self):
        good = record(0, data=b'abc')
        cases = [good, good + record(1) + good, good + good + record(1),
                 good[:-3] + b'00\n' + record(1), b':00x\n',
                 record(4, data=b'\x00') + good + record(1),
                 record(0, 65535, b'ab') + record(1), record(6) + good + record(1),
                 record(1, 1), good + record(5, data=b'1234') + record(5, data=b'5678') + record(1)]
        for case in cases:
            with self.subTest(case=case), self.assertRaises(Blocked):
                intel_hex(case, 100)

    def test_payload_budget(self):
        with self.assertRaises(Blocked):
            intel_hex(record(0, data=b'abcd') + record(1), 3)


class NativeBoundsTests(unittest.TestCase):
    def test_successful_stdout(self):
        self.assertEqual(bounded_process([sys.executable, '-c', "print('abc', end='')"], 3), b'abc')

    def test_excessive_stdout(self):
        with self.assertRaises(Blocked):
            bounded_process([sys.executable, '-c', "import sys; sys.stdout.write('a'*200000)"], 10)

    def test_excessive_stderr(self):
        with self.assertRaises(Blocked):
            bounded_process([sys.executable, '-c', "import sys; sys.stderr.write('a'*200000)"], 10, stderr_limit=10)

    def test_timeout(self):
        with self.assertRaises(subprocess.TimeoutExpired):
            bounded_process([sys.executable, '-c', 'import time; time.sleep(10)'], 10, timeout=0.1)


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.c = Corpus(self.tmp.name)

    def tearDown(self):
        self.c.db.close()
        self.c.session.close()
        self.tmp.cleanup()

    def pipeline(self, **kwargs):
        return Pipeline(self.c, kwargs.get('limit', 1000000), kwargs.get('total', 4000000),
                        kwargs.get('helper', SEVENZIP), kwargs.get('depth', 4))

    def test_zip_hex_to_image_and_idempotence(self):
        payload = record(0, 16, b'abc') + record(1)
        self.c.put(packed('firmware.hex', payload), 'local fixture')
        self.pipeline().run()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM images').fetchone()[0], 1)
        self.assertEqual(self.c.db.execute('SELECT address,size FROM image_segments_v2').fetchone(), (16, 3))
        count = self.c.db.execute('SELECT COUNT(*) FROM job_attempts').fetchone()[0]
        self.pipeline().run()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM job_attempts').fetchone()[0], count)

    def test_equivalent_hex_files_share_image_not_source(self):
        self.c.put(record(0, 1, b'abc') + record(1), 'source a')
        self.c.put(record(0, 2, b'bc') + record(0, 1, b'a') + record(1), 'source b')
        self.pipeline().run()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM images').fetchone()[0], 1)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM image_sources').fetchone()[0], 2)

    def test_addresses_distinguish_images_with_equal_bytes(self):
        self.c.put(record(0, 1, b'abc') + record(1))
        self.c.put(record(0, 2, b'abc') + record(1))
        self.pipeline().run()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM images').fetchone()[0], 2)

    def test_retry_retains_prior_attempt(self):
        self.c.put(b':broken')
        self.pipeline().run()
        self.pipeline().run(retry_blocked=True)
        self.assertEqual(list(self.c.db.execute('SELECT attempt,state FROM job_attempts ORDER BY attempt')), [(1, 'blocked'), (2, 'blocked')])

    def test_interrupted_attempt_retained(self):
        self.c.put(packed('child', b'abc'))
        with patch.object(self.c, 'put', side_effect=KeyboardInterrupt):
            with self.assertRaises(KeyboardInterrupt):
                self.pipeline().run()
        self.pipeline().run()
        self.assertEqual(list(self.c.db.execute('SELECT state FROM job_attempts ORDER BY attempt')), [('interrupted',), ('complete',)])

    def test_policy_change_is_new_job_identity(self):
        self.c.put(packed('child', b'abc'))
        self.pipeline(depth=0).run()
        self.pipeline(depth=4).run()
        self.assertEqual(set(r[0] for r in self.c.db.execute('SELECT state FROM jobs')), {'blocked', 'complete'})

    def test_dependency_change_affects_only_native(self):
        helper = Path(self.tmp.name) / 'fake-7z'
        helper.write_bytes(b'v1')
        a = self.pipeline(helper=helper)
        zip_before, native_before = a.engine('zip'), a.engine('native-archive')
        helper.write_bytes(b'v2')
        b = self.pipeline(helper=helper)
        self.assertEqual(zip_before, b.engine('zip'))
        self.assertNotEqual(native_before, b.engine('native-archive'))

    def test_unclassified_bytes_do_not_become_image(self):
        self.c.put(b'AA66\x55\xfb\x23' * 50)
        self.pipeline().run()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM images').fetchone()[0], 0)

    def test_store_audit_preserves_orphans_and_staging(self):
        sha = self.c.put(b'abc')
        self.c.db.execute('DELETE FROM objects WHERE hash=?', (sha,))
        self.c.db.commit()
        stage = self.c.path(sha).parent / '.stage-interrupted'
        stage.write_bytes(b'partial')
        audit = self.c.audit_store()
        self.assertEqual(audit['unindexed'], [{'sha256': sha, 'valid': True}])
        self.assertEqual(len(audit['staging']), 1)
        self.assertTrue(stage.exists())

    def test_real_pe_resource_zip_hex_chain(self):
        payload = record(0, 32, b'abc') + record(1)
        self.c.put(resource_pe(packed('image.hex', payload)), 'synthetic PE fixture')
        self.pipeline().run()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM images').fetchone()[0], 1)
        self.assertEqual(set(r[0] for r in self.c.db.execute('SELECT kind FROM jobs')), {'pe', 'zip', 'intel-hex'})

    def test_cross_format_budget_is_shared(self):
        payload = record(0, 32, b'abc') + record(1)
        archive = packed('image.hex', payload)
        self.c.put(resource_pe(archive), 'fixture')
        self.pipeline(total=len(archive)).run()
        states = dict(self.c.db.execute('SELECT kind,state FROM jobs'))
        self.assertEqual(states, {'pe': 'complete', 'zip': 'deferred'})
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM images').fetchone()[0], 0)
        self.pipeline(total=len(archive)).run()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM images').fetchone()[0], 1)

    def test_mixed_format_depth_limit(self):
        payload = record(0, 32, b'abc') + record(1)
        self.c.put(resource_pe(packed('image.hex', payload)), 'fixture')
        self.pipeline(depth=2).run()
        states = dict(self.c.db.execute('SELECT kind,state FROM jobs'))
        self.assertEqual(states, {'pe': 'complete', 'zip': 'complete', 'intel-hex': 'blocked'})
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM images').fetchone()[0], 0)

    def test_null_address_image_segment_is_unique(self):
        sha = self.c.put(b'raw image')
        pipeline = self.pipeline()
        segments = [{'address': None, 'sha256': sha, 'size': 9}]
        first = pipeline.record_image(sha, 'fixture1', 'known-image', segments)
        second = pipeline.record_image(sha, 'fixture2', 'known-image', segments)
        self.c.db.commit()
        self.assertEqual(first, second)
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM image_segments_v2').fetchone()[0], 1)

    def test_helper_changed_after_identity_is_refused(self):
        helper = self.c.root / 'helper.exe'
        helper.write_bytes(b'first')
        pipeline = self.pipeline(helper=helper)
        pipeline.engine('native-archive')
        helper.write_bytes(b'changed')
        with self.assertRaises(Blocked), patch('firmware_pipeline.bounded_process') as command:
            pipeline.native_command([str(helper)], 1)
        command.assert_not_called()

    def test_store_audit_detects_dangling_edges_and_image_index_damage(self):
        self.c.put(record(0, 32, b'abc') + record(1))
        self.pipeline().run()
        self.c.db.execute('UPDATE image_segments_v2 SET address=33')
        self.c.db.execute("INSERT INTO edges VALUES ('missing-parent','missing-child','fixture','fixture')")
        self.c.db.commit()
        audit = self.c.audit_store()
        self.assertEqual(len(audit['invalid_images']), 1)
        self.assertEqual(audit['dangling_edges'], [('missing-parent', 'missing-child')])

    def test_killed_writer_resumes_and_releases_os_lock(self):
        from firmware_store_lock import writer_lock
        self.c.put(packed('child', b'abc'), 'fixture')
        script = """
import sys, time
sys.path.insert(0, sys.argv[1])
from firmware_corpus import Corpus
from firmware_store_lock import writer_lock
with writer_lock(sys.argv[2]):
    corpus = Corpus(sys.argv[2])
    original = corpus.put
    def pause_after_write(data, **kwargs):
        result = original(data, **kwargs)
        print('READY', flush=True)
        time.sleep(20)
        return result
    corpus.put = pause_after_write
    corpus.extract()
"""
        process = subprocess.Popen([sys.executable, '-u', '-c', script,
                                    str(Path(__file__).resolve().parents[1]), self.tmp.name],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        messages = queue.Queue()
        reader = threading.Thread(target=lambda: messages.put(process.stdout.readline()), daemon=True)
        reader.start()
        try:
            self.assertEqual(messages.get(timeout=10).strip(), b'READY')
        finally:
            process.kill()
            process.communicate(timeout=10)
            reader.join(timeout=2)
        self.assertEqual(self.c.db.execute('SELECT state FROM jobs').fetchone()[0], 'running')
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM edges').fetchone()[0], 0)
        self.assertEqual(len(self.c.audit_store()['unindexed']), 1)
        with writer_lock(self.tmp.name):
            self.c.extract()
        self.assertEqual(self.c.db.execute('SELECT state FROM jobs').fetchone()[0], 'complete')
        self.assertEqual(self.c.audit_store()['unindexed'], [])

    def test_toolkit_source_snapshot_is_content_addressed(self):
        from firmware_corpus import digest
        pipeline = self.pipeline()
        engine = pipeline.engine('zip')
        toolkit = self.c.db.execute('SELECT toolkit FROM engine_specs WHERE engine=?', (engine,)).fetchone()[0]
        self.assertEqual(digest((self.c.root / 'tooling' / (toolkit + '.json')).read_bytes()), toolkit)

    @unittest.skipUnless(SEVENZIP.is_file(), 'installed 7-Zip required')
    def test_real_7z_inside_zip_with_hex_child(self):
        folder = Path(self.tmp.name) / 'fixture'
        folder.mkdir()
        payload = folder / 'image.hex'
        payload.write_bytes(record(0, 32, b'abc') + record(1))
        archive = folder / 'inner.7z'
        subprocess.run([str(SEVENZIP), 'a', str(archive), str(payload)], capture_output=True, check=True)
        self.c.put(packed('inner.7z', archive.read_bytes()), 'generated fixture')
        self.pipeline().run()
        self.assertEqual(self.c.db.execute('SELECT COUNT(*) FROM images').fetchone()[0], 1)
        self.assertEqual(set(r[0] for r in self.c.db.execute('SELECT kind FROM jobs')), {'zip', 'native-archive', 'intel-hex'})


if __name__ == '__main__':
    unittest.main()
