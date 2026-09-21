"""Reproduce reviewed Razer regional geometry; source photos are never runtime assets."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import prepare_razer_legacy_layouts as legacy
import layout_pipeline as pipeline

ROOT = legacy.ROOT
SOURCE = legacy.SOURCE
OUTPUT = legacy.OUTPUT
JIS = {
    "v2analog": ("rz-n0-00082", "0ff05f5125ce7d344d02156bff4292df3ff85bd41fb1ef4e673ba31a465fe86e", "7f0d535992e4e243ebec807fbd8d8f3757af89b02c5b6d37dacd7cea98150e5e"),
    "v3pro": ("rz-00400-n0", "51a183374950f4cd4bfc21f915e791314170552deebc8648d4c023a25af1cb9a", "bebbdaed267f1b9cc778b7a650214d8ae940174fa13cb227319b6c4074926cbd"),
    "v3tkl": ("rz-00402-n0", "f511c673761ab4624b12636ff6d655a50ea567e0f9a6dd30aa339543689ea3d3", "ea0a47edde74a4cf3161619c50de2db4b5d2a3816bd274a2c31ffd2ea65ff99a"),
}
LABELS = dict(legacy.LABELS)
LABELS.update({50:"#",100:"Non-US",135:"Ro",136:"Intl2",137:"Yen",138:"Henkan",139:"Muhenkan"})

def geometry(kind, region):
    y = 0 if kind == "mini" else 1.25
    # Preserve the reviewed function/navigation/numeric sections exactly.
    keys = [k for k in legacy.geometry(kind) if k["y"] < round(y*46) or k["x"] >= 15*46]
    def key(hid,x,dy,w=1,h=1):
        yy=y+dy
        k=dict(hid=hid,label=LABELS[hid],x=round(x*46),y=round(yy*46),
               w=round((x+w)*46)-round(x*46)-4,h=round((yy+h)*46)-round(yy*46)-4)
        keys.append(k)
        return k
    def row(items,dy,total=15):
        x=0
        for item in items:
            hid,w=item if isinstance(item,tuple) else (item,1)
            key(hid,x,dy,w);x+=w
        assert x==total,(region,dy,x,total)
    if region=="JIS":
        row([41 if kind=="mini" else 53,*range(30,40),45,46,137,42],0)
        row([(43,1.5),20,26,8,21,23,28,24,12,18,19,47,48],1,13.5)
        row([(57,1.75),4,22,7,9,10,11,13,14,15,51,52,50],2,13.75)
        row([(225,2.25),29,27,6,25,5,17,16,54,55,56,135,(229,1.75)],3)
        row([(224,1.5),227,(226,1.5),139,(44,3.5),138,136,(230,1.5),(1033,1.5),(228,1.5)],4)
    elif region=="ISO":
        row([41 if kind=="mini" else 53,*range(30,40),45,46,(42,2)],0)
        row([(43,1.5),20,26,8,21,23,28,24,12,18,19,47,48],1,13.5)
        row([(57,1.75),4,22,7,9,10,11,13,14,15,51,52,50],2,13.75)
        row([(225,1.25),100,29,27,6,25,5,17,16,54,55,56,(229,2.75)],3)
        row([(224,1.25),(227,1.25),(226,1.25),(44,6.25),(230,1.25),(1033,1.25),(101,1.25),(228,1.25)],4)
    else: raise ValueError(region)
    enter=key(40,13.5,1,1.5,2)
    enter.update(notchW=round(13.75*46)-round(13.5*46),notchY=42)
    return sorted(keys,key=lambda k:(k["y"],k["x"]))

def source(path,digest,url):
    assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest,path
    return dict(path=path.as_posix(),sha256=digest,url=url)

def reports():
    out=[]
    for base,(short,model,stem,digest,kind,count) in zip(legacy.reports(),legacy.MODELS):
        if short not in JIS: continue
        slug,photo_hash,json_hash=JIS[short]
        product=SOURCE/(short+"-jis-product.json")
        metadata=json.loads((ROOT/product).read_bytes())
        r=copy.deepcopy(base)
        r.update(id=base["id"].replace("_ansi","_jis"),variant="JIS",name="Razer "+model+" JIS",keys=geometry(kind,"JIS"))
        r["sources"] += [source(product,json_hash,"https://store.grapht.tokyo/products/"+slug+".js"),
            source(SOURCE/(short+"-jis.jpg"),photo_hash,"https:"+metadata["images"][0])]
        r["geometryEvidence"]="Exact Japanese SKU photo from MSY/GRAPHT, Razer's Japanese distributor; manually transcribed JIS rows at 46px pitch. ANSI master guide independently supplies unchanged outer sections. See RAZER_REGIONAL_LAYOUTS_2026-09-19.md."
        assert len(r["keys"])==count+4
        pipeline.validate_report(r);out.append(r)
    for base,(short,model,stem,digest,kind,count) in zip(legacy.reports(),legacy.MODELS):
        if short != "v3pro": continue
        r=copy.deepcopy(base)
        r.update(id=base["id"].replace("_ansi","_iso"),variant="ISO",name="Razer "+model+" ISO",keys=geometry(kind,"ISO"))
        r["sources"].append(source(SOURCE/"v3pro-iso-overview.jpg","63cdb66f8d96ddfc975dec93d6fece7ea1df7d2debd87be4665b501ff4e75c89","https://pics.computerbase.de/1/1/3/0/7/3-b0923fc26dd72251/20-1080.86b49fd0.jpg"))
        r["geometryEvidence"]="Exact-model German ISO test specimen photographed by ComputerBase (2024-07-24); short left Shift, extra Non-US key, compound Enter, standard bottom row. Physical HID labels are normalized English, not a Windows language map."
        assert len(r["keys"])==count+1
        pipeline.validate_report(r);out.append(r)
    return out

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check",action="store_true")
    args=parser.parse_args()
    for r in reports():
        p=ROOT/OUTPUT/(r["id"]+".json")
        data=(json.dumps(r,indent=2)+"\n").encode()
        if args.check: assert p.read_bytes()==data,p
        else:
            old=p.read_bytes() if p.exists() else None
            if old!=data:
                assert (p.read_bytes() if p.exists() else None)==old
                with p.open("xb" if old is None else "wb") as f:f.write(data)
        print(r["name"],len(r["keys"]))
if __name__=="__main__":main()
