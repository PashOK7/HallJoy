"""Read-only layout/native-map audit with explicit evidence boundaries."""
import json
import re
from pathlib import Path
import layout_pipeline as pipeline
import prepare_aula_additional_layouts as hero
import check_atk_hex80_native_map as atk

ROOT=Path(__file__).resolve().parents[1]

def audit():
    result=[]
    ipi=(ROOT/'src/HallJoyProject/HallJoy/generated/ipi_models.h').read_text()
    values=[int(v) for v in re.search(r'factoryHids\[\] = \{([^}]+)',ipi)[1].split(',')]
    for brand,spec in pipeline.read_catalog().items():
        outputs,reports,integrated=pipeline.products(brand,spec)
        if integrated:
            for name,data in outputs.items():assert (ROOT/name).read_bytes()==data,name
        for r in reports:
            pipeline.validate_report(r)
            evidence='Pinned source extraction and generated runtime geometry; not a full hardware analog proof'
            if brand=='IPI':
                for product in r['identity']['products']:
                    ids=[int(v) for v in re.search(r'ids_'+product+r'\[\] = \{([^}]+)',ipi)[1].split(',')]
                    assert sorted(values[i] for i in ids)==sorted(k['hid'] for k in r['keys']),r['name']
                evidence='Every native physical-ID set maps to exactly the layout HID multiset'
            elif brand in ('Aula','Redragon'):
                evidence='Source assignments checked against native factory maps by source adapters; MAX uses its separate source profile'
            elif brand=='ATK':evidence='Official matrix slots checked by check_atk_hex80_native_map.py'
            result.append(dict(name=r['name'],keys=len(r['keys']),evidence=evidence,limitations=r.get('limitations',r.get('notes',[]))))
    hero.reports()  # Exact HERO position/HID pairs and KP-TE153 matrix equality.
    return dict(models=result,hardwareTested=False,
        knownUnresolved=['MG75 Max Fn is implemented; hardware validation remains. MG75 Pro uses a separate JingTai V1 transport, not an established SparkLink route.',
            'Source layout validation alone does not establish every runtime transport mapping; preserve brand-specific hardware and protocol evidence limits.'])

if __name__=='__main__':print(json.dumps(audit(),indent=2))
