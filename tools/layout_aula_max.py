"""WIN60 MAX: locked official 61-layout CSS/arrays + proven physical matrix.

Reproduce the vendor's border-box flex geometry numerically, without executing
JavaScript or rendering screenshots. Do not generalize to another board ID.
"""
import json
import re
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
import layout_import as common
ROOT=Path(__file__).resolve().parents[1]


def prepare(model):
    js,sha=common.read_source(ROOT/model['source'])
    common.require(sha==model['sha256'],'MAX source lock changed')
    css,css_sha=common.read_source(ROOT/model['css'])
    common.require(css_sha==model['cssSha256'],'MAX CSS lock changed')
    for text in ('html{box-sizing:border-box','box-sizing:inherit',
                 '.keyboard61{width:893px','.key{height:58px;margin:1px;',
                 'align-items:center;position:relative;padding:1px}'):
        common.require(text in css,'Unreviewed MAX CSS: '+text)
    common.require('KB2_BOARD_LAYOUT_K61:10' in js,'Board layout classifier changed')
    arrays={}
    for field in ('width','height','marginLeft','marginRight','marginBottom'):
        marker='Keyboard_Layout_'+field+'_61:'
        common.require(js.count(marker)==1,'Ambiguous MAX geometry table')
        arrays[field]=json.JSONDecoder(parse_float=Decimal).raw_decode(js[js.index(marker)+len(marker):])[0]
        common.require(len(arrays[field])==6 and all(len(row)==21 for row in arrays[field]),'Wrong MAX matrix dimensions')
        common.require(f'Keyboard_Layout_{field}_61[u.row][u.col]*58' in js,'MAX scaling changed')
    protocol=ROOT/'src/HallJoyProject/HallJoy/aula_win60he_protocol.cpp'
    code,code_sha=common.read_source(protocol)
    body=code.split('constexpr KeyMap kExpectedDefaultMap{{',1)[1].split('\n}};',1)[0]
    rows=re.findall(r'std::array<std::uint8_t, kColumns>\{\{(.*?)\}\}',body)
    common.require(len(rows)==6,'MAX factory rows changed')
    matrix=[]
    for row in rows:
        values=[int(s,16) for s in re.findall(r'0x[0-9A-Fa-f]+',row)]
        common.require(len(values)<=21,'MAX factory row overflow')
        matrix.extend(values+[0]*(21-len(values))) # C++ aggregate zero initialization
    common.require(len(matrix)==126 and sum(bool(h) for h in matrix)==61,'MAX factory matrix changed')
    labels={hid:label for hid,label in common.CODES.values()};labels[0x409]='Fn'
    scale=Decimal(42)/56  # cap content, excluding the key wrapper's 1px padding
    def px(value): return int((value*scale).quantize(Decimal(1),rounding=ROUND_HALF_UP))
    keys=[]; x=Decimal(0); y=Decimal(0); line_height=Decimal(0); lines=1
    for index,hid in enumerate(matrix):
        if not hid: continue
        r,c=divmod(index,21)
        width,height=(Decimal(arrays[f][r][c])*58 for f in ('width','height'))
        left,right,bottom=(Decimal(arrays[f][r][c])*58 for f in ('marginLeft','marginRight','marginBottom'))
        common.require(height==58 and left==right==bottom==0,'Unreviewed MAX flex shape')
        if x+width>873: # 893px border-box less 10px padding on each side
            x=Decimal(0);y+=line_height;line_height=Decimal(0);lines+=1
        # Common wrapper top margin=1, padding=1; origin removed globally.
        mapped=0x409 if hid==1 else hid
        common.require(mapped in labels,'Unknown MAX key identity')
        keys.append(dict(hid=mapped,label=labels[mapped],x=px(x),y=px(y),
                         w=px(x+width-2)-px(x),h=px(y+height-2)-px(y),matrix=[r,c]))
        x+=width;line_height=max(line_height,height+1)
    common.require(lines==5,'Unexpected MAX flex wrapping')
    return dict(schema=1,id=model['id'],name='Aula '+model['model']+' '+model['variant'],
                brand='Aula',model=model['model'],variant='ANSI',status='ready',unresolved=[],keys=keys,
                sources=dict(driver=dict(path=model['source'],url=model['url'],sha256=sha),
                             css=dict(path=model['css'],sha256=css_sha),
                             factory=dict(path=protocol.relative_to(ROOT).as_posix(),sha256=code_sha)),
                identity=dict(protocol='aula-rm6x21',products=model['products'],requiresVerifiedSession=True),
                scale='42/56; manufacturer border-box flex width 873; wrapper padding excluded')
