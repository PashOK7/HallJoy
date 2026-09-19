#!/usr/bin/env python3
"""Exercise production layout storage in a simulator process without UI/devices."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def hashes(root):
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in root.glob("*.ini")}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--legacy-layouts", type=Path)
    args = parser.parse_args()
    assert os.name == "nt", "Windows is required"
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    results = {}

    def run(name, root, *flags):
        root.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(args.exe.resolve()), "--halljoy-test-layout-storage",
                        "--halljoy-test-data-root", str(root), "--halljoy-test-legacy-root", str(root),
                        *flags], check=True, timeout=30, creationflags=subprocess.CREATE_NO_WINDOW)
        content = (root / "layout-storage-result.txt").read_text(encoding="utf-16-le")
        result = dict(line.split("=", 1) for line in content.splitlines())
        assert result["verified"] == "1", result
        results[name] = result
        return result

    fresh = evidence / "fresh"
    assert run("fresh", fresh)["layout_files"] == "0"
    assert not list((fresh / "Layouts").glob("*.ini"))
    run("edit_and_failures", fresh, "--layout-storage-verify")
    before = hashes(fresh / "Layouts")
    run("restart", fresh, "--layout-storage-check-restart")
    assert hashes(fresh / "Layouts") == before, "Startup changed user files"
    if args.legacy_layouts:
        existing = evidence / "existing"
        shutil.copytree(args.legacy_layouts, existing / "Layouts")
        before = hashes(existing / "Layouts")
        run("existing", existing)
        assert hashes(existing / "Layouts") == before, "Startup rewrote legacy files"
        results["legacy_hashes_unchanged"] = True
    with (evidence / "results.json").open("x", encoding="utf-8") as stream:
        json.dump(results, stream, indent=2)
    print(json.dumps(results, indent=2))
    print("LAYOUT_STORAGE_WINDOWS_TEST=PASS")


if __name__ == "__main__":
    main()
