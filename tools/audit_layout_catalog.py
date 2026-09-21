"""Read-only audit of the simulator's production layout-catalog.tsv export."""
import argparse
import json
from collections import defaultdict
from pathlib import Path


def audit(path):
    models = {}
    for row in Path(path).read_text(encoding="utf-8").splitlines():
        brand, name, display, *fields = row.split("\t")
        assert len(fields) == 8, (name, fields)
        key = tuple(map(int, fields[:7])) + (fields[7],)
        item = models.setdefault(name, dict(brand=brand, display=display, keys=[]))
        assert item["brand"] == brand and item["display"] == display
        item["keys"].append(key)
    assert models
    exact, geometry, visible = defaultdict(list), defaultdict(list), defaultdict(list)
    for name, item in models.items():
        keys = tuple(sorted(item["keys"]))
        exact[(item["brand"], keys)].append(name)
        geometry[tuple(sorted(k[:7] for k in keys))].append(name)
        visible[item["display"]].append(name)
    for display, names in visible.items():
        assert len({models[n]["brand"] for n in names}) == 1, ("cross-brand merge", display)
        assert len({tuple(sorted(k[:7] for k in models[n]["keys"])) for n in names}) == 1, ("unequal merged geometry", display)
    unmerged = [names for names in exact.values()
                if len({models[n]["display"] for n in names}) > 1]
    assert not unmerged, ("unmerged identical same-brand layouts", unmerged)
    return dict(sourceVariants=len(models), visibleVariants=len(visible),
        groups=[dict(brand=models[names[0]]["brand"], display=display, sources=names,
                     keyCount=len(models[names[0]]["keys"])) for display,names in visible.items()],
        crossBrandGeometryKeptSeparate=[names for names in geometry.values()
            if len({models[n]["brand"] for n in names}) > 1])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory")
    args = parser.parse_args()
    print(json.dumps(audit(args.inventory), ensure_ascii=False, indent=2))
