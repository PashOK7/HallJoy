import copy
import json
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import layout_pipeline as p
import layout_aula_w669 as aula


class LayoutPipelineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.spec=p.read_catalog()['Aula']
        cls.reports=p.reports_for(cls.spec)

    def test_aula_exact_profiles(self):
        for report,count in zip(self.reports,(61,68)):
            p.validate_report(report)
            self.assertEqual(len(report['keys']),count)
            self.assertIn(250,[k['hid'] for k in report['keys']])
            self.assertNotIn(0x409,[k['hid'] for k in report['keys']])
            self.assertTrue(report['identity']['requiresVerifiedSession'])
        by_hid={k['hid']:k for k in self.reports[1]['keys']}
        self.assertEqual(by_hid[79]['matrix'],[5,16])
        self.assertEqual(by_hid[76]['matrix'],[2,16])

    def test_wrong_source_lock(self):
        model=copy.deepcopy(self.spec['models'][0]);model['sha256']='0'*64
        with self.assertRaises(ValueError): aula.prepare(model)

    def test_wrong_factory(self):
        model=copy.deepcopy(self.spec['models'][0]);model['factory']='Win68FactoryMap'
        with self.assertRaises(ValueError): aula.prepare(model)

    def test_geometry_rejections(self):
        for field,value in [('x',float('nan')),('w',0),('hid',True),('notchW',900),('label','a\nb')]:
            report=copy.deepcopy(self.reports[0]);report['keys'][0][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError): p.validate_report(report)
        report=copy.deepcopy(self.reports[0]);report['keys'][1]['x']=0
        with self.assertRaises(ValueError): p.validate_report(report)

    def test_compound_contour(self):
        report=dict(name='Test',status='ready',unresolved=[],keys=[
            dict(label='Enter',hid=40,x=0,y=0,w=60,h=90,notchW=20,notchY=42),
            dict(label='#',hid=50,x=0,y=44,w=18,h=42)])
        p.validate_report(report)
        report['keys'][1]['w']=22
        with self.assertRaises(ValueError): p.validate_report(report)

    def test_stage_integrity_and_no_overwrite(self):
        outputs=p.normalized_products('Aula',self.reports)
        with tempfile.TemporaryDirectory() as tmp:
            stage=Path(tmp)/'stage'
            p.stage(stage,'Aula',outputs,self.reports,self.spec)
            self.assertFalse(p.check_stage(stage)['runtimeInstalled'])
            with self.assertRaises(FileExistsError): p.stage(stage,'Aula',outputs,self.reports,self.spec)
            with (stage/'unexpected').open('x') as stream: stream.write('x')
            with self.assertRaises(ValueError): p.check_stage(stage)

    def test_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            for path in ('../outside','.',str(Path(tmp).anchor)):
                with self.assertRaises(ValueError): p.local_path(tmp,path)

    def test_layout_name_cannot_escape_export(self):
        report=copy.deepcopy(self.reports[0]);report['name']='../../outside'
        with self.assertRaises(ValueError): p.normalized_products('Aula',[report])

    def test_deterministic_products(self):
        a=p.normalized_products('Aula',self.reports)
        self.assertEqual(a,p.normalized_products('Aula',self.reports))
        self.assertIn(b'SI2828KZHEARGB',a['generated/identities.inc'])
        self.assertEqual(len([k for k in a if k.endswith('.ini')]),3)

    def test_max_independent_geometry(self):
        report=self.reports[2]
        p.validate_report(report)
        self.assertEqual(len(report['keys']),61)
        self.assertEqual(report['identity']['products'],['0A021902'])
        self.assertIn(0x409,[k['hid'] for k in report['keys']])
        maximum={k['hid']:k for k in report['keys']}
        standard={k['hid']:k for k in self.reports[0]['keys']}
        self.assertNotEqual(maximum[42]['matrix'],standard[42]['matrix'])
        self.assertEqual(len({k['y'] for k in report['keys']}),5)

    def test_duplicate_identity(self):
        catalog=dict(schema=1,brands={'Aula':copy.deepcopy(self.spec)})
        catalog['brands']['Aula']['models'][1]['products'].append('SI2825HEARGB')
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'catalog.json'
            with path.open('x') as stream: json.dump(catalog,stream)
            with self.assertRaises(ValueError): p.read_catalog(path)

    def test_cached_fetch_is_offline(self):
        with patch('urllib.request.urlopen',side_effect=AssertionError('Unexpected network')):
            p.fetch_sources(self.spec)

    def test_redragon_geometry_and_identity(self):
        spec=p.read_catalog()['Redragon']
        with patch('urllib.request.urlopen',side_effect=AssertionError('Unexpected network')):
            p.fetch_sources(spec)
            reports=p.reports_for(spec)
        self.assertEqual([len(r['keys']) for r in reports],[80,81])
        for report in reports:
            p.validate_report(report)
            self.assertEqual([o['index'] for o in report['omitted']],[15,121])
            self.assertIn(0xe5,[k['hid'] for k in report['keys']])
            self.assertNotIn(0x87,[k['hid'] for k in report['keys']])
        ansi={k['hid']:k for k in reports[0]['keys']}
        iso={k['hid']:k for k in reports[1]['keys']}
        self.assertEqual(ansi[49]['matrix'],[2,14])
        self.assertEqual(iso[49]['matrix'],[3,13])
        self.assertNotIn(100,ansi)
        self.assertIn(100,iso)
        self.assertNotIn('notchW',ansi[40])
        # Absolute edges: round(156*1.2)-round(83*1.2) = 187-100.
        self.assertEqual((iso[40]['w'],iso[40]['h'],iso[40]['notchW'],iso[40]['notchY']),(68,87,14,42))
        generated=p.runtime_products()
        presets=generated['src/HallJoyProject/HallJoy/generated/layout_pipeline/presets.inc']
        self.assertLess(presets.index(b'Aula'),presets.index(b'Redragon'))
        identities=generated['src/HallJoyProject/HallJoy/generated/layout_pipeline/identities.h']
        self.assertNotIn(b'7272BRHEXYXK673JCARGB',identities)

    def test_redragon_rejects_wrong_profile(self):
        spec=copy.deepcopy(p.read_catalog()['Redragon'])
        spec['models'][0]['factory']='K673BrFactoryMap'
        with self.assertRaises(ValueError): p.reports_for(spec)

    def test_changed_cache_is_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            spec={'models':[dict(source='source.json',sha256='0'*64,url='https://example.invalid/source')]}
            path=root/'source.json'
            with path.open('xb') as stream: stream.write(b'existing source')
            with patch.object(p,'ROOT',root),patch('urllib.request.urlopen',side_effect=AssertionError('Unexpected network')):
                with self.assertRaises(ValueError): p.fetch_sources(spec)
            self.assertEqual(path.read_bytes(),b'existing source')


class FinalLayoutBatchTests(unittest.TestCase):
    def test_reviewed_reports_offline(self):
        expected={'Razer':[61,62,65],'NuPhy':[61,83],
                  'Wooting':[61,62,61,62,84,85,88,87,88,108,109,108,109]}
        catalog=p.read_catalog()
        with patch('urllib.request.urlopen',side_effect=AssertionError('Unexpected network')):
            for brand,counts in expected.items():
                reports=p.reports_for(catalog[brand])
                self.assertEqual(sorted(len(r['keys']) for r in reports),sorted(counts))
                for r in reports:
                    p.validate_report(r)
                    self.assertEqual(r['identity']['products'],[]) # No guessed regional auto-match.
                    by_hid={k['hid']:k for k in r['keys']}
                    self.assertIn(0x409,by_hid)
                    self.assertIn(4,by_hid)
                    if r['variant']!='ANSI':
                        self.assertGreater(by_hid[40]['notchW'],0)
                        self.assertGreater(by_hid[40]['notchY'],0)
                        self.assertGreater(by_hid[40]['h'],by_hid[4]['h'])
                    else: self.assertNotIn('notchW',by_hid[40])

    def test_plus_identity_is_distinct(self):
        reports=p.reports_for(p.read_catalog()['Wooting'])
        self.assertEqual(len({r['id'] for r in reports}),13)
        self.assertTrue(any('60he_plus' in r['id'] for r in reports))

    def test_review_reader_rejects_metadata_drift(self):
        import layout_reviewed_report as reader
        model=copy.deepcopy(p.read_catalog()['Razer']['models'][0])
        for field,value in [('sha256','0'*64),('brand','Other'),('count',1),('products',['GUESS'])]:
            changed=copy.deepcopy(model);changed[field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):reader.prepare(changed)


if __name__=='__main__': unittest.main()
