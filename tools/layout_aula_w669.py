"""Pinned official W669 geometry, checked against HallJoy's shipped factory map.

Static parsing only. No vendor JS execution, HID, calibration or device writes.
"""
import re
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
import layout_import as common

ROOT=Path(__file__).resolve().parents[1]
PROTOCOL=ROOT/'src/HallJoyProject/HallJoy/aula_w669_protocol.cpp'
LABELS={hid:label for hid,label in common.CODES.values()}
LABELS[0xfa]='Fn'  # W669's existing physical identity; not UAP's 0x409.


def factory_map(source, factory):
    if factory in ('K673BrFactoryMap','K673UkFactoryMap','K673UsFactoryMap'):
        match=re.search(r'PositionToHid '+factory+r'\(\) noexcept\s*\{(.*?)\n\}',source,re.S)
        common.require(match is not None,'K673 factory missing')
        body=re.sub(r'//[^\n]*','',match[1]).strip()
        if factory=='K673BrFactoryMap':
            initializer=re.fullmatch(r'return\s*\{([\s,0-9a-fx]+)\};',body)
            common.require(initializer is not None,'Unreviewed K673 base map')
            values=[int(v,16) for v in re.findall(r'0x[0-9a-f]+',initializer[1])]
            common.require(len(values)==132,'K673 matrix dimensions changed')
            return {index:hid for index,hid in enumerate(values) if hid}
        base='K673BrFactoryMap' if factory=='K673UkFactoryMap' else 'K673UkFactoryMap'
        prefix='auto map = '+base+'();'
        common.require(body.startswith(prefix) and body.endswith('return map;'),'Unreviewed K673 derivation')
        body=body[len(prefix):-len('return map;')]
        pattern=r'map\[(\d+)\]\s*=\s*(0x[0-9a-fA-F]+);'
        common.require(re.sub(pattern,'',body).strip()=='','Unparsed K673 instructions')
        result=factory_map(source,base)
        for assignment in re.finditer(pattern,body):
            index=int(assignment[1]); hid=int(assignment[2],16)
            common.require(index<132,'K673 matrix index out of range')
            if hid: result[index]=hid
            else: result.pop(index,None)
        return result
    common.require(factory in ('Win60FactoryMap','Win68FactoryMap'), 'Unreviewed W669 profile')
    base=re.search(r'PositionToHid Win60FactoryMap\(\) noexcept\s*\{(.*?)\n\}',source,re.S)
    common.require(base is not None, 'Factory map implementation changed')
    initializer=re.search(r'return\s*\{(.*?)\};',base[1],re.S)
    common.require(initializer is not None,'Factory initializer missing')
    values=[int(v,16) for v in re.findall(r'0x[0-9a-fA-F]+',initializer[1])]
    common.require(len(values)==132,'Factory dimensions changed')
    if factory=='Win68FactoryMap':
        match=re.search(r'PositionToHid Win68FactoryMap\(\) noexcept\s*\{(.*?)\n\}',source,re.S)
        common.require(match is not None,'WIN68 map missing')
        body=re.sub(r'//[^\n]*','',match[1]).strip()
        common.require(body.startswith('auto map = Win60FactoryMap();') and body.endswith('return map;'),
                       'Unreviewed WIN68 mapping algorithm')
        body=body[len('auto map = Win60FactoryMap();'):-len('return map;')]
        assignments=list(re.finditer(r'map\[(\d+)\]\s*=\s*(0x[0-9a-fA-F]+);',body))
        common.require(re.sub(r'map\[(\d+)\]\s*=\s*(0x[0-9a-fA-F]+);','',body).strip()=='',
                       'Unparsed WIN68 factory instructions')
        for assignment in assignments:
            index=int(assignment[1]); common.require(index<132,'Factory index out of range')
            values[index]=int(assignment[2],16)
    return {index:hid for index,hid in enumerate(values) if hid}


def prepare(model, root=ROOT, protocol=PROTOCOL):
    path=(root/model['source']).resolve()
    common.require(path.is_relative_to(root.resolve()),'Source escapes repository')
    data,sha=common.read_json(path)
    common.require(sha.lower()==model['sha256'],'Official source lock changed: '+model['id'])
    code,code_sha=common.read_source(protocol)
    expected=factory_map(code,model['factory'])
    common.require(data['type']=='us' and model['variant']=='ANSI','Unreviewed physical variant')
    common.require(len(data['keys'])==model['count'],'Official key count changed')
    # Prove each allowlisted firmware product remains in the shipped classifier.
    group=re.search(r'if \(((?:\s*is\("[A-Z0-9-]+"\)\s*(?:\|\|\s*)?)+)\)\s*return FactoryLayoutProfile::'+
                    ('Si2825Win60' if model['factory']=='Win60FactoryMap' else 'Si2828Win68')+r';',code,re.S)
    common.require(group is not None,'Factory classifier missing')
    for product in model['products']:
        common.require('is("'+product+'")' in group[1],'Firmware product no longer supported')
    keys=[]; actual={}
    min_x=min(Decimal(k['x']) for k in data['keys'])
    min_y=min(Decimal(k['y']) for k in data['keys'])
    scale=Decimal(42)/35
    def rounded(value):
        return int((value*scale).quantize(Decimal(1),rounding=ROUND_HALF_UP))
    for raw in data['keys']:
        index=int(raw['index']); hid=int(raw['hidCode'],16)
        common.require(index not in actual and hid in LABELS,'Duplicate position or unknown HID')
        actual[index]=hid
        x,y=Decimal(raw['x'])-min_x,Decimal(raw['y'])-min_y
        w,h=Decimal(raw['width']),Decimal(raw['height'])
        keys.append(dict(hid=hid,label=LABELS[hid],x=rounded(x),y=rounded(y),
                         w=rounded(x+w)-rounded(x),h=rounded(y+h)-rounded(y),matrix=[index//22,index%22]))
    common.require(actual==expected,'Official geometry/HallJoy factory identity mismatch')
    return dict(schema=1,id=model['id'],name='Aula '+model['model']+' '+model['variant'],
                brand='Aula',model=model['model'],variant=model['variant'],status='ready',unresolved=[],keys=keys,
                sources=dict(official=dict(path=model['source'],url=model['url'],sha256=sha),
                             factory=dict(path=protocol.relative_to(ROOT).as_posix(),sha256=code_sha)),
                scale='42/35, absolute edges rounded half-up; source origin removed',
                identity=dict(protocol='aula-w669',products=model['products'],requiresVerifiedSession=True,
                              vid=0x2e3c,pid=0xc365,usagePage=0xff1b,usage=0x91,rows=6,columns=22))
