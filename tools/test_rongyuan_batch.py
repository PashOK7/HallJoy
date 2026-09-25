"""Generic offline batch regressions; no hardware or application launch."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
import rongyuan_batch as b

class BatchTests(unittest.TestCase):
    def package(self, edits):
        return {'schema':1, 'edits':{name:{'before_sha256':b.digest(old) if old is not None else None,
                'after':new.decode(), 'after_sha256':b.digest(new)} for name,old,new in edits}}

    def test_preflight_rejects_stale_target_without_any_writes(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); (root/'README.md').write_bytes(b'old')
            (root/'docs').mkdir(); (root/'docs/SUPPORTED_HARDWARE.md').write_bytes(b'changed')
            package=self.package([('README.md',b'old',b'new'),('docs/SUPPORTED_HARDWARE.md',b'old',b'new')])
            with self.assertRaisesRegex(ValueError,'target changed'):
                b.apply_package(root,package,root/'backup.zip')
            self.assertEqual((root/'README.md').read_bytes(),b'old')
            self.assertFalse((root/'backup.zip').exists())

    def test_backup_and_content_checks(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); (root/'README.md').write_bytes(b'old')
            package=self.package([('README.md',b'old',b'new')])
            corrupt=copy.deepcopy(package);corrupt['edits']['README.md']['after']='bad'
            with self.assertRaisesRegex(ValueError,'content changed'):
                b.apply_package(root,corrupt,root/'backup.zip')
            b.apply_package(root,package,root/'backup.zip')
            self.assertEqual((root/'README.md').read_bytes(),b'new')
            with zipfile.ZipFile(root/'backup.zip') as z:
                self.assertEqual(z.read('README.md'),b'old')
                self.assertEqual(json.loads(z.read('batch-manifest.json')),package)

    def test_rejects_unexpected_paths_and_empty_packages(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for name in ('../outside.md','unexpected.py'):
                with self.assertRaises(ValueError):
                    b.apply_package(root,self.package([(name,None,b'new')]),root/'backup.zip')
            with self.assertRaises(ValueError):b.apply_package(root,{'schema':1,'edits':{}},root/'backup.zip')

    def test_source_parser_rejects_unreviewed_behavior(self):
        matrix=','.join(['0']*512)
        source='import{C as a}from "./7a5b12c9.js";class b extends a{defaultMatrix=['+matrix+']}export{b};'
        source=source.replace('from "','from"')
        self.assertEqual(len(b.parse_source(source.encode())[1]),512)
        for changed in (source.replace('7a5b12c9','unknown'),source.replace('0,0','999,0',1),
                        source.replace('}export',';read=()=>42}export')):
            with self.assertRaises(ValueError):b.parse_source(changed.encode())

class CorpusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        folder=b.ROOT/'.local/research-expansion-20260924'
        if not (folder/'vendor_profiles_womier.json').exists():
            raise unittest.SkipTest('Optional local vendor corpus unavailable')
        cls.c=b.Corpus(b.ROOT,folder/'vendor_profiles_womier.json',folder/'WOMIER-3.2.15-technical-js.zip')
    @classmethod
    def tearDownClass(cls):cls.c.zip.close()
    def manifest(self):
        row=next(r for r in self.c.inspect()['rows'] if not r['reasons'])
        m={k:row[k] for k in ('id','vid','pid','source_sha256')}
        m.update(brand='ZZ TEST',model='Synthetic fixture',range_um=4000,
                 evidence='Regression only; no support claim',range_basis='Synthetic fixture')
        return [m]
    def test_existing_pinned_sources_parse(self):
        for p in self.c.profiles:
            parent,matrix=b.parse_source((b.ROOT/b.DATA/p['source']).read_bytes())
            self.assertEqual(matrix,p['matrix'])
            self.assertEqual(parent == '60ee4367.js',p['precision_enum'])
            if 'parent' in p:self.assertEqual(parent,p['parent'])
    def test_exact_override_review_rejects_modified_behavior(self):
        reviews=b.read_json(b.ROOT/b.DATA/'reviewed-overrides.json')
        for filename,review in reviews.items():
            raw=self.c.zip.read(next(n for n in self.c.zip.namelist() if n.endswith('/'+filename)))
            self.assertEqual(b.digest(raw),review['sha256'])
            self.assertEqual(b.parse_source(raw)[0],review['parent'])
            changed=raw.replace(b'getSideLightSetting=',b'readAnalog=',1)
            self.assertNotEqual(raw,changed)
            with self.assertRaisesRegex(ValueError,'unreviewed_class_behavior'):
                b.parse_source(changed)

    def test_reviewed_parent_subclasses_parse_without_analog_overrides(self):
        for parent in ('631ebd97.js','3f643f35.js','9b8d4f8a.js'):
            raw=(b.ROOT/b.DATA/parent).read_bytes()
            self.assertIn(b'./7a5b12c9.js',raw)
            source=('import{C as a}from"./'+parent+'";class z extends a{defaultMatrix=['+','.join(['0']*512)+']}export{z};').encode()
            self.assertEqual(b.parse_source(source)[0],parent)

    def test_stale_review_duplicate_and_unsafe_evidence_rejected(self):
        m=self.manifest();m[0]['source_sha256']='0'*64
        with self.assertRaisesRegex(ValueError,'reviewed source changed'):self.c.prepare(m)
        m=self.manifest()
        with self.assertRaisesRegex(ValueError,'duplicate manifest'):self.c.prepare(m*2)
        m[0]['evidence']='bad|table'
        with self.assertRaisesRegex(ValueError,'unsafe evidence'):self.c.prepare(m)
    def test_duplicate_catalog_identity_held(self):
        row=next(r for r in self.c.rows if r['id']==self.manifest()[0]['id'])
        original=self.c.rows
        try:
            self.c.rows=original+[row]
            affected=[r for r in self.c.inspect()['rows'] if r['id']==row['id']]
            self.assertTrue(all('duplicate_identity' in r['reasons'] for r in affected))
        finally:self.c.rows=original
    def test_mechanical_switch_option_is_held(self):
        original=self.c.rows
        row=copy.deepcopy(next(r for r in original if r['id']==self.manifest()[0]['id']))
        row['record_literal'] += ' supportedSwitchTypes:["\u673a\u68b0\u8f74"]'
        try:
            self.c.rows=[row]
            self.assertIn('mechanical_switch_option_review',self.c.inspect()['rows'][0]['reasons'])
        finally:self.c.rows=original
    def test_prepare_apply_in_isolated_directory(self):
        package=self.c.prepare(self.manifest())
        self.assertEqual(len(package['sheet_intent']),1)
        self.assertIn('ZZ TEST Synthetic fixture',package['edits']['README.md']['after'])
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for name,edit in package['edits'].items():
                if edit['before_sha256'] is not None:b.create(root/name,(b.ROOT/name).read_bytes())
            b.apply_package(root,package,root/'backup.zip')
            for name,edit in package['edits'].items():
                self.assertEqual(b.digest((root/name).read_bytes()),edit['after_sha256'])
            profiles=b.read_json(root/b.DATA/'profiles.json')
            self.assertEqual(len(profiles),len(self.c.profiles)+1)
            lock=b.read_json(root/b.DATA/'source-lock.json')
            for name in package['edits']:
                if name.startswith(b.DATA.as_posix()+'/') and Path(name).name in lock:
                    self.assertEqual(b.digest((root/name).read_bytes()),lock[Path(name).name])

if __name__=='__main__':unittest.main()
