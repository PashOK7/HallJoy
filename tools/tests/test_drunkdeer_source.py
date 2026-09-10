import hashlib
import re
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from drunkdeer_layout_source import SOURCE, ROOT, MODELS, extract, factory_map


class DrunkDeerSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text=SOURCE.read_text(encoding='utf-8-sig')

    def test_pinned_source(self):
        self.assertEqual(hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
            'f4d896de0e8fc696c108f35c01b3864831ed437cb0e1a1b30be4d75fa871dbd7')

    def test_seven_physical_variants(self):
        for model,count in zip(MODELS,(82,82,83,61,68,84,86)):
            keys=[k for row in extract(self.text,model) for k in row]
            self.assertEqual(len(keys),count,model)
            self.assertEqual(len({k['value'] for k in keys}),count,model)
            self.assertEqual(len(factory_map(self.text,model)),126,model)

    def test_modifiers_and_extended_keys(self):
        keys=factory_map(self.text,'G65')
        self.assertEqual(keys[105]['hid'],224)
        self.assertEqual(keys[114]['hid'],230)
        self.assertEqual(keys[115]['hid'],0x409)
        self.assertEqual(keys[116]['hid'],0x403)

    def test_constructor_code_is_rejected(self):
        changed=self.text.replace('super(A,"ddeerG65KeyProfile"',
            'super(A,"discardedKeyProfile"',1)
        with self.assertRaises(ValueError): factory_map(changed,'G65')
        start=self.text.index('super(A,"ddeerG65KeyProfile"')
        changed=self.text[:start]+self.text[start:].replace('new g(0,0,"","",0)',
            'new g(0,0,"","",dangerous())',1)
        with self.assertRaises(ValueError): factory_map(changed,'G65')

    def test_known_g65_differences_are_explicit(self):
        keys=factory_map(self.text,'G65')
        for offset,hid in ((21,41),(35,76),(56,77),(77,75),(98,78),(119,79)):
            self.assertEqual(keys[offset]['hid'],hid)

    def test_iso_source_conflict_is_not_silently_filled(self):
        physical={k['value'] for row in extract(self.text,'A75_iso_uk') for k in row}
        factory=factory_map(self.text,'A75_iso_uk')
        self.assertEqual({p for p in physical if not factory[p]['hid']},{75,85,99})
        self.assertEqual({k['position'] for k in factory if k['hid'] and k['position'] not in physical},{55,100})

    def test_bad_key_index_does_not_shift_array(self):
        for model in ('G75','G75JP'):
            keys=factory_map(self.text,model)
            self.assertEqual(keys[19]['keyIndex'],29)
            self.assertEqual(keys[19]['hid'],0)
            self.assertEqual(keys[29]['hid'],37)

    def test_entire_native_g65_map_matches_official_factory(self):
        source=(ROOT/'src/HallJoyProject/HallJoy/drunkdeer_protocol.cpp').read_text(encoding='utf-8')
        body=source.split('PositionToHid G65AnsiMap() noexcept',1)[1].split('return map;',1)[0]
        actual=[0]*126
        constants={'halljoy::keycode::kFn':0x409,'halljoy::keycode::kOem1':0x403}
        for row,col,code in re.findall(r'put\((\d+), (\d+), ([\w:]+)\)',body):
            offset=int(row)*21+int(col)
            self.assertEqual(actual[offset],0)
            actual[offset]=constants[code] if code in constants else int(code,0)
        expected=[k['hid'] for k in factory_map(self.text,'G65')]
        self.assertEqual(actual,expected)


if __name__=='__main__': unittest.main()
