#!/usr/bin/env python3
"""Generate the hash-pinned RM-36 reachable-source inventory.

This records a review boundary, not a claim that source-only evidence proves a
device protocol or a signed release artifact.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PROJECT = ROOT / "src" / "HallJoyProject" / "HallJoy" / "HallJoy.vcxproj"
OUTPUT = ROOT / "docs" / "v1.4" / "RM36_SOURCE_INVENTORY_2026-09-06.json"
SOURCE_SUFFIXES = {".cpp", ".h", ".inc", ".py", ".ps1", ".rc", ".vcxproj", ".filters"}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def project_items(project: str) -> dict[str, str]:
    items: dict[str, str] = {}
    pattern = re.compile(r'<(?:ClCompile|ClInclude|ResourceCompile) Include="([^"]+)"(?:[^>]*?)>')
    for match in pattern.finditer(project):
        value = match.group(1).replace("\\", "/")
        items[value] = "ordinary-compiled"
    for block in re.finditer(r'<ClCompile Include="([^"]+)">(.*?)</ClCompile>', project, flags=re.DOTALL):
        value, body = block.group(1).replace("\\", "/"), block.group(2)
        if "ExcludedFromBuild Condition=\"'$(" in body:
            items[value] = "ordinary-excluded-opt-in"
    return items


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify existing inventory instead of writing it")
    parser.add_argument("--write", action="store_true", help="explicitly replace the reviewed inventory")
    args = parser.parse_args()
    if args.check and args.write:
        raise SystemExit("choose either --check or --write")
    project_text = PROJECT.read_text(encoding="utf-8")
    items = project_items(project_text)
    paths: dict[Path, str] = {}
    hall = ROOT / "src" / "HallJoyProject" / "HallJoy"
    for relative, reachability in items.items():
        path = (hall / relative).resolve()
        if path.is_file():
            paths[path] = reachability
    for directory in (ROOT / "tools", ROOT / "src" / "HallJoyProject" / "tests"):
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
                paths.setdefault(path.resolve(), "tool-or-test")

    records = []
    for path, reachability in sorted(paths.items(), key=lambda item: item[0].as_posix().lower()):
        records.append({
            "path": path.relative_to(ROOT).as_posix(),
            "sha256": sha256(path),
            "reachable_build": reachability,
            "reviewer": "RM-36 source-gate reconciliation",
            "review_date": str(date.today()),
            "outcome": "SOURCE_GATE_COVERED",
        })
    document = {
        "schema": 1,
        "scope": "project translation units, resource/header entries, tools and test sources",
        "limits": "SOURCE_GATE_COVERED is not physical keyboard, firmware, signing or release qualification",
        "records": records,
    }
    encoded = json.dumps(document, indent=2, ensure_ascii=False) + "\n"
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_text(encoding="utf-8") != encoded:
            print("FAIL: RM-36 source inventory is absent or stale; regenerate after reviewed source changes")
            return 1
        print(f"RM36_SOURCE_INVENTORY=PASS records={len(records)}")
        return 0
    if OUTPUT.exists() and not args.write:
        raise SystemExit(f"refusing to overwrite existing inventory: {OUTPUT}")
    OUTPUT.write_text(encoded, encoding="utf-8")
    print(f"RM36_SOURCE_INVENTORY=WRITTEN records={len(records)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
