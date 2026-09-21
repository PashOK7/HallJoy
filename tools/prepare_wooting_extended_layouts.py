"""Extract reviewed Wootility geometry without executing downloaded JavaScript.

The pinned bundle's string table is decoded as data. Only literal arithmetic,
objects, matrices and HID usage declarations are evaluated by this extractor.
Normal runtime/build checks read the resulting source-locked JSON reports.
"""
import argparse
import ast
import hashlib
import json
import operator
import re
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
import layout_import as common
import layout_pipeline as pipeline

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path('docs/research/final-layout-sources/wootility.js')
SHA = '987ba72a09a78b1fc508c8739b1c0a25a3434d7ef614fba56f1822e5acdbc40a'
OUTPUT = Path('docs/research/wooting-extended-layout-reports-20260919')
URL = 'https://wootility.io/assets/index-BaoALhfv.js'
LABELS = {hid: label for hid, label in common.CODES.values()}
LABELS.update({0x409:'Fn',0x480:'Space L',0x481:'Space R',0x482:'Fn C',0x483:'Fn R'})

def literal(text, marker):
    start = text.index(marker) + len(marker)
    opening = text[start]; closing = {'[':']','{':'}'}[opening]
    depth = 0; quote = None; escaped = False
    for i in range(start,len(text)):
        c = text[i]
        if quote:
            if escaped: escaped = False
            elif c == '\\': escaped = True
            elif c == quote: quote = None
        elif c in ('"',"'"): quote = c
        elif c == opening: depth += 1
        elif c == closing:
            depth -= 1
            if not depth: return text[start:i+1]
    raise ValueError('Unterminated source literal')

def decode():
    data = (ROOT/SOURCE).read_bytes()
    assert hashlib.sha256(data).hexdigest() == SHA
    text = data.decode('utf-8')
    pool = ast.literal_eval(literal(text,'function t(){const z1E='))
    # Independently checked against the bundle's parseInt rotation checksum.
    offset, rotation = 216, 333
    ids = [0xeb49,0x94e3,0xc5c1,0xbc0,0xa31a,0x4373,0x7b3e,0xa275,0x83f2,0x7b59]
    v = [int(re.match(r'^\s*([+-]?\d+)',pool[(i-offset+rotation)%len(pool)])[1]) for i in ids]
    assert abs(v[0]-v[1]/2-v[2]/3-v[3]/4*(v[4]/5)+v[5]/6-v[6]/7*(v[7]/8)-v[8]/9*(-v[9]/10)-(-0x3ce23+0x4e4c9*4-0x10943))<0.00001
    aliases = {'P'}
    pairs = re.findall(r'\b(\w+)=(\w+)(?=[,;])',text)
    while True:
        more = aliases | {left for left,right in pairs if right in aliases}
        if more == aliases: break
        aliases = more
    start = text.index('function t(){const z1E=')+len('function t(){const z1E=')
    end = text.index(';t=',start)
    text = text[:start]+'[]'+text[end:]
    text = re.sub(r'\b('+'|'.join(sorted(aliases))+r')\((0x[0-9a-f]+)\)',
        lambda m:json.dumps(pool[(int(m[2],16)-offset+rotation)%len(pool)]),text)
    quoted = r'(?:"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27)'
    for _ in range(12):
        text,count = re.subn('('+quoted+r')\s*\+\s*('+quoted+')',
            lambda m:json.dumps(ast.literal_eval(m[1])+ast.literal_eval(m[2])),text)
        if not count: break
    return text

def arithmetic(text):
    def visit(node):
        if isinstance(node,ast.Constant) and type(node.value) in (int,float): return node.value
        if isinstance(node,ast.UnaryOp) and isinstance(node.op,(ast.USub,ast.UAdd)):
            return (-1 if isinstance(node.op,ast.USub) else 1)*visit(node.operand)
        if isinstance(node,ast.BinOp) and isinstance(node.op,(ast.Add,ast.Sub,ast.Mult)):
            return {ast.Add:operator.add,ast.Sub:operator.sub,ast.Mult:operator.mul}[type(node.op)](visit(node.left),visit(node.right))
        raise ValueError('Not literal arithmetic')
    return visit(ast.parse(text,mode='eval').body)

def object_literal(text, marker):
    raw = literal(text,marker)
    raw = re.sub(r'(?<=:)(-?\([^()]+\))(?=[,}])',lambda m:str(arithmetic(m[0])),raw)
    return ast.literal_eval(re.sub(r'\bnull\b','None',raw))

def matrix(text, marker):
    usages = {}
    for prefix in ('y','le'):
        start = text.index('class '+prefix+'{')
        end = text.index('static [',start)
        methods = list(re.finditer(r'static get\[[\x27"](\w+)[\x27"]\]\(\)\{',text[start:end]))
        for i,m in enumerate(methods):
            body = text[start+m.end():start+methods[i+1].start() if i+1<len(methods) else end]
            found = re.search(r"'usageId':(0x[0-9a-f]+)",body)
            if found: usages[prefix+'.'+m[1]] = int(found[1],16)
    raw = literal(text,marker)
    raw = re.sub(r'\b(y|le)\[[\x27"](\w+)[\x27"]\]',lambda m:str(usages[m[1]+'.'+m[2]]),raw)
    return ast.literal_eval(raw.replace('null','None'))

def props_for(props, region, split):
    if props is None: return None
    if any(k in props for k in ('ansi','iso','jis','ansi_split_spacebar','iso_split_spacebar')):
        key = region+'_split_spacebar' if split else region
        return props_for(props.get(key,props.get(region)),region,split)
    if 'rgb' in props or 'nonRgb' in props:
        return props_for(props.get('nonRgb'),region,split)
    return props

def px(value): return int(Decimal(str(value*46)).quantize(Decimal(1),rounding=ROUND_HALF_UP))

def reports():
    text = decode()
    assert "'layout':qS,'supportedLayoutTypes':()=>[je[\"ANSI\"],je[\"ISO\"],je[\"ANSI_SPLIT_SPACEBAR\"],je[\"ISO_SPLIT_SPACEBAR\"]]" in text
    base = object_literal(text,'S5='); splits = object_literal(text,'Sg=')
    keys60 = matrix(text,'xE='); uwu = matrix(text,'_E=')
    output = []
    for split in (False,True):
        for region in ('ansi','iso'):
            geometry = {**base,**splits}; keys = []; y = 0
            for row in range(1,6):
                x = 0; advance = 0
                for col in range(14):
                    p = props_for(geometry.get(f'{row},{col}'),region,split)
                    if p is None: continue
                    w,h = p.get('width',1),p.get('height',1)
                    top = y+p.get('mt',0); x += p.get('ml',0)
                    hid = keys60[row][col]
                    if split and row==5 and col in (4,6,8,13):
                        hid = {4:0x480,6:0x482,8:0x481,13:0x483}[col]
                    assert hid is not None, (row,col)
                    key = dict(hid=hid,label=LABELS[hid],x=px(x),y=px(top),w=px(x+w)-px(x)-4,h=px(top+h)-px(top)-4,matrix=[row,col])
                    if hid==40 and region=='iso':
                        key.update(x=px(x-.25),w=px(x+w)-px(x-.25)-4,notchW=px(x)-px(x-.25),notchY=42)
                    keys.append(key); x += w+p.get('mr',0); advance=max(advance,p.get('mt',0)+h+p.get('mb',0))
                y += advance
            model = '60HE v2 Split' if split else '60HE v2'
            output.append(make_report(model,region.upper(),keys,[]))
    # The three auxiliary silicone buttons have no analog depth. Main key
    # scale is a Wootility rendering choice; normalize equally to standard keys.
    assert object_literal(text,'Yc=') == {'mb':.25,'scale':1.5}
    assert "'isDigital':!(0x1*-0x624+-0x1*0x2573+0x2b97)" in text
    keys = [dict(hid=uwu[2][col],label=LABELS[uwu[2][col]],x=i*46,y=0,w=42,h=42,matrix=[2,col]) for i,col in enumerate((1,3,5))]
    output.append(make_report('UwU + UwU RGB','ANSI',keys,[dict(code='silicone_buttons',reason='Three digital-only auxiliary buttons have no analog depth; not represented as analog keys.')]))
    return output

def make_report(model,variant,keys,omitted):
    ident = re.sub('[^a-z0-9]+','_','wooting_'+model.lower()+'_'+variant.lower())
    r = dict(schema=1,id=ident,brand='Wooting',model=model,variant=variant,name='Wooting '+model+' '+variant,
        status='ready',unresolved=[],keys=keys,omitted=omitted,
        sources=[dict(path=SOURCE.as_posix(),sha256=SHA,url=URL)],
        geometryEvidence='Pinned Wootility v5 S5/Sg/qS and UwU matrix/geometry; keycap-unit normalization, no vendor code execution.',
        identity=dict(protocol='none',products=[]),automaticSelection='Manual: current telemetry does not prove regional/split layout.')
    pipeline.validate_report(r);return r

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    for r in reports():
        path=ROOT/OUTPUT/(r['id']+'.json');data=(json.dumps(r,indent=2)+'\n').encode()
        if args.check: assert path.read_bytes()==data,path
        else:
            path.parent.mkdir(parents=True,exist_ok=True)
            with path.open('xb') as f:f.write(data)
        print(r['name'],len(r['keys']))
if __name__=='__main__': main()
