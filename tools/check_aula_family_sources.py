"""Check new HERO maps/geometry against pinned vendor source, without hardware."""
import re
from pathlib import Path
from prepare_aula_additional_layouts import source, array, geometry

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src/HallJoyProject/HallJoy'

def main():
    text, _ = source('hero-app.js')
    native = (SRC/'aula_hero_family.h').read_text()
    layouts = (SRC/'aula_hero_family_layout.h').read_text()
    for marker, count, suffix, layout in [('ARM32_HID_68',68,'68','68'),('AULA_2836',101,'99','99')]:
        start = text.index('keyboard:[',text.index('"'+marker+'"'))+9
        raw=[]
        for obj in re.findall(r'\{x:.*?pos:\d+\}',array(text,start)):
            k={f:float(re.search(r'\b'+f+r':([0-9.]+)',obj)[1]) for f in ('x','y','width','height')}
            value=int(re.search(r'value:"(0x[0-9A-Fa-f]+)"',obj)[1],16)
            if value==0x0d000000: hid=0x409
            elif value<=0xff: hid=value
            else:
                bits=value>>16
                assert value==bits<<16 and bits.bit_count()==1 and bits<=128
                hid=0xe0+bits.bit_length()-1
            k.update(hid=hid,pos=int(re.search(r'pos:(\d+)',obj)[1]));raw.append(k)
        assert len(raw)==count
        assert len({k['pos'] for k in raw})==count and len({k['hid'] for k in raw})==count
        body=re.search(r'keys'+suffix+r'\[\] = \{(.*?)\n\};',native,re.S)[1]
        actual=[tuple(map(int,m)) for m in re.findall(r'\{(\d+), (\d+)\}',body)]
        assert actual==[(k['pos'],k['hid']) for k in raw]
        body=re.search(r'g_aula_hero'+layout+r'_ansi\[\] = \{(.*?)\n\};',layouts,re.S)[1]
        actual=[tuple(map(int,m)) for m in re.findall(r'\{L"[^"]*", (\d+), 0, (\d+), (\d+), (\d+), (\d+), 0, 0\}',body)]
        expected=[(k['hid'],k['x'],k['w'],k['h'],k['y']) for k in geometry(raw,34)]
        assert actual==expected
    print('AULA_FAMILY_SOURCE_CHECK=PASS (169 vendor positions and geometry)')

if __name__=='__main__': main()
