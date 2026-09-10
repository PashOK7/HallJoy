"""Offline real-source fixtures plus malformed-input and Windows INI tests."""
import copy
import ctypes
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('layout_import', ROOT/'tools/layout_import.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
FIX = Path(__file__).parent/'fixtures/layout_import'


class LayoutImportTests(unittest.TestCase):
    def setUp(self):
        self.definition = m.read_json(FIX/'k4-he-ansi.json')[0]
        self.info = m.read_json(FIX/'info.json')[0]
        self.code = m.read_source(FIX/'keymap.c')[0]
        self.overrides = m.read_json(ROOT/'docs/research/layout-import-k4-overrides.json')[0]

    def convert(self, **kwargs):
        return m.convert(self.definition, self.info, self.code, 'WIN_BASE', **kwargs)

    def test_k4_complete_matrix_and_geometry(self):
        report = self.convert(overrides=self.overrides)
        self.assertEqual(report['status'], 'ready')
        self.assertEqual(len(report['keys']), 100)
        keys = {k['hid']: k for k in report['keys']}
        self.assertEqual((keys[26]['x'], keys[26]['y']), (120,92))
        self.assertEqual((keys[87]['w'], keys[87]['h']), (42,86))
        self.assertEqual(keys[88]['h'],86)
        self.assertIn(1033, keys)
        self.assertIn(1028, keys)
        self.assertIn('Brand=Keychron\r\n', m.ini_text(report))

    def test_unknown_actions_block_ini(self):
        report = self.convert()
        self.assertEqual({k['token'] for k in report['unresolved']}, {'FN_WIN','RGB_MOD'})
        with self.assertRaises(m.ImportErrorDetail):
            m.ini_text(report)
        with tempfile.TemporaryDirectory() as root:
            output = Path(root)/'review'
            m.export(report, output)
            self.assertEqual([p.name for p in output.iterdir()], ['review.json'])

    def test_q1_new_layout_arrow_offsets(self):
        definition = m.read_json(FIX/'q1-he-ansi.json')[0]
        info = m.read_json(FIX/'q1-info.json')[0]
        code = m.read_source(FIX/'q1-keymap.c')[0]
        overrides = m.read_json(ROOT/'docs/research/layout-import-q1-overrides.json')[0]
        report = m.convert(definition, info, code, 'WIN_BASE', overrides)
        self.assertEqual(report['status'], 'ready')
        self.assertEqual(len(report['keys']),81)
        self.assertEqual(report['excluded'][0]['matrix'],[0,14])
        keys = {k['hid']:k for k in report['keys']}
        self.assertEqual(keys[81]['y'],253)
        self.assertEqual(keys[82]['y'],207)
        self.assertEqual(keys[228]['y'],242)
        self.assertIn(1033,keys)

    def test_bad_geometry(self):
        for patch in [{'r':15}, {'x2':0}, {'d':True}, {'g':True}, {'x':float('nan')},
                      {'w':0}, {'w':100}, {'h':0.1}, {'x':True}, {'row':True}, {'row':6}]:
            with self.subTest(patch=patch):
                saved = copy.deepcopy(self.definition)
                self.definition['layouts']['keys'][0].update(patch)
                with self.assertRaises(m.ImportErrorDetail):
                    self.convert()
                self.definition = saved

    def test_overlap(self):
        self.definition['layouts']['keys'][1]['x'] = 0
        with self.assertRaisesRegex(m.ImportErrorDetail,'Overlapping'):
            self.convert()

    def test_mismatch_and_duplicates(self):
        self.info['usb']['pid'] = '0xFFFF'
        with self.assertRaisesRegex(m.ImportErrorDetail,'PID mismatch'):
            self.convert()
        self.info['usb']['pid'] = '0x0E40'
        self.definition['layouts']['keys'][1]['col'] = 0
        with self.assertRaisesRegex(m.ImportErrorDetail,'Duplicate Launcher'):
            self.convert()

    def test_missing_key_and_variant(self):
        saved = copy.deepcopy(self.definition)
        self.definition['layouts']['keys'].pop()
        with self.assertRaisesRegex(m.ImportErrorDetail,'matrix sets differ'):
            self.convert()
        self.definition = saved
        self.definition['layouts']['optionKeys'] = {'0': []}
        with self.assertRaisesRegex(m.ImportErrorDetail,'variants'):
            self.convert()

    def test_override_identity_and_token(self):
        for field, value in [('token','FN_MAC'), ('vendorProductId',1), ('hid',0), ('label','bad\nlabel')]:
            overrides = copy.deepcopy(self.overrides)
            overrides['5,11'][field] = value
            with self.subTest(field=field), self.assertRaises(m.ImportErrorDetail):
                self.convert(overrides=overrides)

    def test_override_duplicate_hid(self):
        self.overrides['5,11']['hid'] = 41
        with self.assertRaisesRegex(m.ImportErrorDetail,'Duplicate HID'):
            self.convert(overrides=self.overrides)

    def test_bad_json(self):
        with tempfile.TemporaryDirectory() as root:
            for i, payload in enumerate(['{"x":1,"x":2}', '{"x":NaN}', '{"x":Infinity}']):
                path = Path(root)/f'{i}.json'
                path.write_text(payload, encoding='utf-8')
                with self.assertRaises(m.ImportErrorDetail):
                    m.read_json(path)

    def test_factory_no_c_execution(self):
        self.code = '[WIN_BASE] = LAYOUT_ansi_100(system("bad"))'
        with self.assertRaises(m.ImportErrorDetail):
            self.convert()

    def test_absolute_rounding(self):
        report = self.convert(pitch_x=47)
        for key, source in zip(report['keys'],self.definition['layouts']['keys']):
            self.assertLessEqual(abs(key['x']-source['x']*47),0.5)
            self.assertLessEqual(abs(key['x']+key['w']-((source['x']+source['w'])*47-6)),0.5)

    def test_export_no_overwrite_and_bom(self):
        with tempfile.TemporaryDirectory() as root:
            output = Path(root)/'layout'
            report = self.convert(overrides=self.overrides)
            m.export(report,output)
            before = {p.name:p.read_bytes() for p in output.iterdir()}
            self.assertTrue(before['Imported layout.ini'].startswith(b'\xff\xfe'))
            with self.assertRaises(FileExistsError):
                m.export(report,output)
            self.assertEqual(before, {p.name:p.read_bytes() for p in output.iterdir()})

    @unittest.skipUnless(os.name == 'nt', 'Windows INI API')
    def test_windows_reads_all_exported_keys(self):
        read = ctypes.WinDLL('kernel32', use_last_error=True).GetPrivateProfileStringW
        read.argtypes = [ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_uint,ctypes.c_wchar_p]
        read.restype = ctypes.c_uint
        with tempfile.TemporaryDirectory() as root:
            report = self.convert(overrides=self.overrides)
            m.export(report,Path(root)/'layout')
            path = str(Path(root)/'layout/Imported layout.ini')
            for i,key in enumerate(report['keys']):
                buffer = ctypes.create_unicode_buffer(1024)
                count = read('LayoutPreset',f'K{i}','',buffer,1024,path)
                self.assertGreater(count,0)
                fields = buffer.value.split('|',5)
                self.assertEqual(list(map(int,fields[:5])), [key['hid'],min(20,key['y']//46),key['x'],key['w'],key['h']])
                self.assertEqual(fields[5].replace('\\|','|').replace('\\\\','\\'),key['label'])
                read('LayoutPreset',f'Y{i}','',buffer,1024,path)
                self.assertEqual(int(buffer.value),key['y'])

    def test_cli_end_to_end(self):
        with tempfile.TemporaryDirectory() as root:
            result = subprocess.run([sys.executable,str(ROOT/'tools/layout_import.py'),
                '--launcher',str(FIX/'k4-he-ansi.json'),'--qmk-info',str(FIX/'info.json'),
                '--qmk-keymap',str(FIX/'keymap.c'),'--layer','WIN_BASE',
                '--overrides',str(ROOT/'docs/research/layout-import-k4-overrides.json'),
                '--output-dir',str(Path(root)/'output')], capture_output=True, text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertTrue((Path(root)/'output/Imported layout.ini').is_file())

    def test_shipped_geometry_matches_reviewed_exports(self):
        source = (ROOT/'src/HallJoyProject/HallJoy/imported_layouts.h').read_text(encoding='utf-8')
        for model in ['k4','q1']:
            block = source.split(f'g_imported_{model}[] = {{')[1].split('};',1)[0]
            entries = re.findall(r'\{L("(?:\\.|[^"\\])*"),\s*([0-9, ]+)\}',block)
            report = json.loads((ROOT/f'docs/exports/imported-layouts/{model}/review.json').read_text(encoding='utf-8'))
            self.assertEqual(len(entries),len(report['keys']))
            for (label, numbers), key in zip(entries,report['keys']):
                self.assertEqual(json.loads(label),key['label'])
                self.assertEqual([int(n) for n in numbers.split(',')],
                    [key['hid'],min(20,key['y']//46),key['x'],key['w'],key['h'],key['y']])


if __name__ == '__main__':
    unittest.main()
