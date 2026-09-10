"""Read literal Antler layout arrays without executing vendor JavaScript."""
import json
import ast
import sys
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'.local/research/layout-import-drunkdeer/index.CJWCGjvj.js'
MODELS=('A75','A75Pro','A75_iso_uk','G60','G65','G75','G75JP')
PROFILE_NAMES=dict(A75='A75', A75Pro='A75Pro', A75_iso_uk='A75UK',
                   G60='G60', G65='G65', G75='G75', G75JP='G75JP')


def factory_map(text, model):
    """Parse inert constructor arguments, retaining array position AND keyIndex.

    The vendor indexes the layer array by physical offset. A typo in keyIndex
    must not shift the following entries or silently overwrite another key.
    """
    marker='super(A,"ddeer'+PROFILE_NAMES[model]+'KeyProfile"'
    start=text.index(marker)
    end=text.index('this.keyCodeLayer1.push',start)
    body=text[start:end]
    string=r'''(?:"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|`[^`$]*`)'''
    arg=rf'(?:{string}|\d+)'
    pattern=rf'this\.keyCodeLayer0\.push\(new g\(({arg}(?:,{arg})*)\)\)'
    matches=list(re.finditer(pattern,body))
    if len(matches)!=126 or body.count('this.keyCodeLayer0.push')!=126:
        raise ValueError('Unreviewed factory layer syntax/count: '+model)
    result=[]
    for position,match in enumerate(matches):
        args=[]
        for token in re.findall(arg,match[1]):
            if token.startswith('`'): value=token[1:-1]
            elif token.startswith('"'): value=json.loads(token)
            elif token.startswith("'"): value=ast.literal_eval(token)
            else: value=int(token)
            args.append(value)
        # g's constructor declares seven parameters; this bundle also passes
        # an eighth, ignored modifier hint. Preserve it rather than execute it.
        if not 5<=len(args)<=8: raise ValueError(f'Unknown key constructor: {model} {position} {args!r}')
        index,command,label,icon,code,*extra=args
        kind=extra[0] if extra else 0
        hid=None
        if command==252 and kind==0: hid=code
        elif command==252 and kind==1 and code in (1,2,4,8,16,32,64,128):
            hid=224+code.bit_length()-1
        elif command==255 and code==255 and kind==3: hid=0x409
        elif command==254 and code==254 and kind==3: hid=0x403
        elif command==0 and code==0: hid=0
        result.append(dict(position=position,keyIndex=index,command=command,
                           label=label,code=code,kind=kind,hid=hid,
                           ignoredArguments=args[7:]))
    return result


def extract(text, model):
    marker='get'+model+'(){return'
    start=text.index(marker)+len(marker)
    # Images are inert literal assets. Strip only the exact URL construction
    # used here; all remaining syntax must parse as data, never eval/Function.
    end=text.index(']]}',start)+2
    raw=text[start:end]
    raw=re.sub(r'new URL\("data:image/[^"\r\n]+",import.meta.url\).href','null',raw)
    raw=re.sub(r'`([^`$]*)`',lambda m:json.dumps(m[1]),raw)
    # Tokenize strings first so names containing punctuation remain untouched.
    tokens=re.findall(r'''"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_$][\w$]*|!\d|[^\s]''',raw)
    out=[]
    for i,token in enumerate(tokens):
        if re.fullmatch(r'[A-Za-z_$][\w$]*',token) and i+1<len(tokens) and tokens[i+1]==':':
            token=json.dumps(token)
        elif token=='!1': token='false'
        elif token=='!0': token='true'
        elif token.startswith("'"): token=json.dumps(ast.literal_eval(token))
        out.append(token)
    rows=json.loads(''.join(out))
    assert isinstance(rows,list) and all(isinstance(row,list) for row in rows)
    for row in rows:
        for key in row:
            key.pop('img',None);key.pop('darkImg',None)
    return rows


if __name__=='__main__':
    text=SOURCE.read_text(encoding='utf-8-sig')
    if '--json' in sys.argv:
        print(json.dumps({model:extract(text,model) for model in MODELS},ensure_ascii=True))
        sys.exit(0)
    for model in MODELS:
        rows=extract(text,model)
        print(model, [len(row) for row in rows])
        print([(k.get('value'),k.get('name'),k.get('className')) for row in rows for k in row])
