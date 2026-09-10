"""Reviewed iLLumiPC K673 profiles. No HID access or downloaded code execution."""
import re
from decimal import Decimal, ROUND_HALF_UP
import layout_import as common
from layout_aula_w669 import ROOT, PROTOCOL, LABELS, factory_map


def prepare(model, root=ROOT, protocol=PROTOCOL):
    path=(root/model['source']).resolve()
    common.require(path.is_relative_to(root.resolve()),'Source escapes repository')
    data,sha=common.read_json(path)
    common.require(sha==model['sha256'],'K673 source lock changed')
    code,code_sha=common.read_source(protocol)
    profiles={'ANSI':('us','K673UsFactoryMap','K673Us'), 'ISO':('uk','K673UkFactoryMap','K673Uk')}
    common.require(model['variant'] in profiles,'Unreviewed K673 variant')
    kind,factory,profile=profiles[model['variant']]
    common.require(data['type']==kind and model['factory']==factory,'K673 variant mismatch')
    for product in model['products']:
        common.require(re.search(r'if \(is\("'+re.escape(product)+r'"\)\)\s*return FactoryLayoutProfile::'+profile+r';',code),
                       'K673 product no longer supported by this profile')
    expected=factory_map(code,factory)
    common.require(len(data['keys'])==model['count']+2,'K673 source count changed')
    actual={}; usable=[]; omitted=[]
    for raw in data['keys']:
        index=int(raw['index']); hid=int(raw['hidCode'],16)
        common.require(index not in actual,'Duplicate K673 position')
        actual[index]=hid
        if not hid:
            common.require((index,raw['code']) in ((15,'RotaryKnob'),(121,'KeyFn')),
                           'Unknown non-keyboard control')
            omitted.append(dict(index=index,code=raw['code'],reason='No published keyboard HID usage'))
        else:
            common.require(hid in LABELS,'Unknown K673 HID')
            usable.append(raw)
    common.require({i:h for i,h in actual.items() if h}==expected,'K673 geometry/factory map mismatch')
    common.require(len(usable)==model['count'],'K673 published key count changed')
    css_path=(root/model['css']).resolve()
    common.require(css_path.is_relative_to(root.resolve()),'CSS escapes repository')
    css,css_sha=common.read_source(css_path)
    common.require(css_sha==model['cssSha256'],'K673 CSS lock changed')
    # Vendor Enter is the union of two right-aligned CSS pseudo-elements.
    # Keep the outer contour, not the decorative one-pixel internal seam overlap.
    for selector,required in (
        ('.devKeyPanelUK .devKeyPanelView::before',('height:48%','top:0')),
        ('.devKeyPanelUK .devKeyPanelView::after',('width:80%','height:calc(52% + 1px)','right:0'))):
        rules=re.findall(re.escape(selector)+r'\{([^}]+)\}',css)
        common.require(any(all(value in rule for value in required) for rule in rules),'K673 compound CSS changed')
    min_x=min(Decimal(k['x']) for k in usable); min_y=min(Decimal(k['y']) for k in usable)
    def rounded(value): return int((value*Decimal(42)/35).quantize(Decimal(1),rounding=ROUND_HALF_UP))
    keys=[]
    for raw in usable:
        hid=int(raw['hidCode'],16); index=int(raw['index'])
        x,y=Decimal(raw['x'])-min_x,Decimal(raw['y'])-min_y
        w,h=Decimal(raw['width']),Decimal(raw['height'])
        key=dict(hid=hid,label=LABELS[hid],x=rounded(x),y=rounded(y),
                 w=rounded(x+w)-rounded(x),h=rounded(y+h)-rounded(y),matrix=[index//22,index%22])
        if hid==40 and kind=='uk':
            key.update(notchW=rounded(x+w*Decimal('.2'))-rounded(x),
                       notchY=rounded(y+h*Decimal('.48'))-rounded(y))
        keys.append(key)
    return dict(schema=1,id=model['id'],brand='Redragon',name='Redragon '+model['model']+' '+model['variant'],
                model=model['model'],variant=model['variant'],status='ready',unresolved=[],keys=keys,omitted=omitted,
                sources=dict(official=dict(path=model['source'],url=model['url'],sha256=sha),
                             css=dict(path=model['css'],url=model['cssUrl'],sha256=css_sha),
                             factory=dict(path=protocol.relative_to(ROOT).as_posix(),sha256=code_sha)),
                scale='42/35, absolute edges rounded half-up; origin removed; CSS Enter contour',
                identity=dict(protocol='aula-w669',products=model['products'],requiresVerifiedSession=True,
                              vid=0x2e3c,pid=0xc365,usagePage=0xff1b,usage=0x91,rows=6,columns=22))
