"""Static string-table decoding; accepts literals and arithmetic only, never JS execution."""
import ast, pathlib, re, json, operator, sys
def decode(path):
    text=path.read_text(encoding='utf8')
    alias,func=re.search(r'(?:const|var) (\w+)=(a\d+_0x\w+)[;,]',text).groups()
    table,target=re.search(r'\}\((\w+),(0x[0-9a-f]+)\)\);',text).groups()
    tail=text[text.index('function '+table+'('):]
    strings=ast.literal_eval('['+re.search(r'=\[(.*?)\];',tail,re.S)[1]+']')
    body=text[text.index('function '+func+'('):]
    offset=int(re.search(r'=\w+-(0x[0-9a-f]+)',body)[1],16)
    expr=re.search(r'try\{(?:const|var) \w+=(.*?);if\(',text)[1]
    def calc(n):
        if isinstance(n,ast.Constant) and type(n.value) in (int,float):return n.value
        if isinstance(n,ast.UnaryOp):return {ast.USub:operator.neg,ast.UAdd:operator.pos}[type(n.op)](calc(n.operand))
        if isinstance(n,ast.BinOp):return {ast.Add:operator.add,ast.Sub:operator.sub,ast.Mult:operator.mul,ast.Div:operator.truediv}[type(n.op)](calc(n.left),calc(n.right))
        raise ValueError('Non-arithmetic expression')
    winners=[]
    for rot in range(len(strings)):
        try:
            def number(m):return re.match(r'^[+-]?\d+',strings[(int(m[1],16)-offset+rot)%len(strings)])[0]
            exp=re.sub(r'parseInt\(\w+\((0x[0-9a-f]+)\)\)',number,expr)
            if abs(calc(ast.parse(exp,mode='eval').body)-int(target,16))<.00001:winners.append(rot)
        except (TypeError,ValueError):pass
    assert len(winners)==1,winners
    aliases={alias,func}
    pairs=re.findall(r'\b(\w+)=(\w+)\b',text)
    while True:
        old=len(aliases)
        for a,b in pairs:
            if b in aliases:aliases.add(a)
        if old==len(aliases):break
    return re.sub(r'\b(?:'+'|'.join(aliases)+r')\((0x[0-9a-f]+)\)',lambda m:json.dumps(strings[(int(m[1],16)-offset+winners[0])%len(strings)]),text)
