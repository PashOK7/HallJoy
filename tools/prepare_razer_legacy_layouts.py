"""Generate ANSI key geometry transcribed from exact-model Razer master guides.
No downloaded code is executed. Keycap units are normalized to 46px pitch;
this is a reviewed transcription, not automatic PDF contour extraction.
"""
import argparse
import hashlib
import json
from pathlib import Path
import layout_import as common
import layout_pipeline as pipeline
ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path("docs/research/razer-layout-sources-20260919")
OUTPUT = Path("docs/research/razer-layout-reports-20260919")
MODELS = [
    ("v2analog", "Huntsman V2 Analog", "HUNTSMANV2ANALOG-00000614", "50005b00aaf6a59c8a9ae8c05e35af561a22dae752d91cba7cefa34576e91248", "full", 104),
    ("minianalog", "Huntsman Mini Analog", "HUNTSMANMINIANALOG-00000642", "4f32794473cb0c513bbe57eeaa012844fabc5f445168b427436c3bb473135f45", "mini", 61),
    ("v3pro", "Huntsman V3 Pro", "HUNTSMANV3PRO-00000678", "04588699d9c48f40f3dc71e29fa5cc6f80b55cd85815239521eab85a9fd8d6e8", "full", 104),
    ("v3tkl", "Huntsman V3 Pro Tenkeyless", "HUNTSMANV3PROTENKEYLESS-00000679", "66ed4084b1aa17219db4d256f6bb91d23ab7842427df6a0e77647966558128bd", "tkl", 84),
]
LABELS = {hid: label for hid, label in common.CODES.values()}
LABELS.update({1033:"Fn", 101:"Menu", 70:"PrtSc", 71:"ScrLk", 72:"Pause"})
def geometry(kind):
    keys=[]
    def key(hid,x,y,w=1,h=1):
        keys.append(dict(hid=hid,label=LABELS[hid],x=round(x*46),y=round(y*46),
                         w=round((x+w)*46)-round(x*46)-4,
                         h=round((y+h)*46)-round(y*46)-4))
    def row(items,y):
        x=0
        for item in items:
            hid,w=item if isinstance(item,tuple) else (item,1)
            key(hid,x,y,w);x+=w
        assert x==15, (items,x)
    y=0 if kind=="mini" else 1.25
    row([41 if kind=="mini" else 53,*range(30,40),45,46,(42,2)],y)
    row([(43,1.5),20,26,8,21,23,28,24,12,18,19,47,48,(49,1.5)],y+1)
    row([(57,1.75),4,22,7,9,10,11,13,14,15,51,52,(40,2.25)],y+2)
    row([(225,2.25),29,27,6,25,5,17,16,54,55,56,(229,2.75)],y+3)
    row([(224,1.25),(227,1.25),(226,1.25),(44,6.25),
         (230,1.25),(1033,1.25),(101,1.25),(228,1.25)],y+4)
    if kind!="mini":
        key(41,0,0)
        for start,x in [(58,2),(62,6.5),(66,11)]:
            for i in range(4):key(start+i,x+i,0)
        # TKL Print Screen/Scroll Lock/Pause are Fn legends on F6/F7/F8,
        # not separate physical analog keys. The full-size boards have all three.
        if kind=="full":
            for i,hid in enumerate([70,71,72]):key(hid,15.5+i,0)
        for yy,hids in [(y,[73,74,75]),(y+1,[76,77,78]),(y+4,[80,81,79])]:
            for i,hid in enumerate(hids):key(hid,15.5+i,yy)
        key(82,16.5,y+3)
    if kind=="full":
        x=18.75
        for i,hid in enumerate([83,84,85,86]):key(hid,x+i,y)
        for dy,hids in [(1,[95,96,97]),(2,[92,93,94]),(3,[89,90,91])]:
            for i,hid in enumerate(hids):key(hid,x+i,y+dy)
        key(87,x+3,y+1,1,2);key(88,x+3,y+3,1,2)
        key(98,x,y+4,2);key(99,x+2,y+4)
    return sorted(keys,key=lambda k:(k["y"],k["x"]))

def reports():
    out=[]
    for short,model,stem,digest,kind,count in MODELS:
        path=SOURCE/(short+"-en.pdf")
        assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest
        keys=geometry(kind);assert len(keys)==count
        ident="razer_"+model.lower().replace(" ","_")+"_ansi"
        report=dict(schema=1,id=ident,brand="Razer",model=model,variant="ANSI",
            name="Razer "+model+" ANSI",status="ready",unresolved=[],keys=keys,
            omitted=[] if kind=="mini" else [
                dict(code="media_controls",reason="Separate media/macro buttons and dial have no verified analog depth channel; not represented as keyboard HID travel.")],
            sources=[dict(path=path.as_posix(),sha256=digest,
                url="https://dl.razerzone.com/master-guides/RazerSynapse3/"+stem+"-en.pdf")],
            geometryEvidence="Exact-model master guide, PDF page index 3: ANSI diagram manually transcribed to keycap units. Theme-normalized geometry, not case dimensions.",
            identity=dict(protocol="manual-layout",products=[],requiresVerifiedSession=True),
            autoSelection="Disabled: USB product identity does not prove ANSI versus ISO/JIS.")
        pipeline.validate_report(report);out.append(report)
    return out

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--check",action="store_true")
    args=parser.parse_args()
    for r in reports():
        target=ROOT/OUTPUT/(r["id"]+".json")
        data=(json.dumps(r,indent=2)+chr(10)).encode()
        if args.check:assert target.read_bytes()==data, target
        else:
            target.parent.mkdir(parents=True,exist_ok=True)
            old=target.read_bytes() if target.exists() else None
            if old is None:
                with target.open("xb") as f:f.write(data)
            else:
                assert target.read_bytes()==old;target.write_bytes(data)
        print(r["name"],len(r["keys"]))
if __name__=="__main__":main()
