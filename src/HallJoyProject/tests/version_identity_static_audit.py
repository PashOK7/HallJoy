#!/usr/bin/env python3
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[3]
hall = repo / "src" / "HallJoyProject" / "HallJoy"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


version = read(hall / "version.h")
resource = (hall / "HallJoy.rc").read_text(encoding="utf-16")
backend = read(hall / "mad68pr_backend.cpp")
readme = read(repo / "README.md")
build = read(repo / "tools" / "build.ps1")

active_text = "\n".join((version, resource, backend, readme, build))
checks = {
    "version tuple is 1.4.0.0": "#define HALLJOY_VERSION_TUPLE 1,4,0,0" in version,
    "public version is 1.4.0": '#define HALLJOY_VERSION_STRING "1.4.0"' in version,
    "runtime build ID is centralized": "HALLJOY_BUILD_ID_W" in version and "HALLJOY_BUILD_ID_W" in backend,
    "file version uses the central tuple": "FILEVERSION HALLJOY_VERSION_TUPLE" in resource,
    "product version uses the central tuple": "PRODUCTVERSION HALLJOY_VERSION_TUPLE" in resource,
    "about dialog uses the central version": "HALLJOY_ABOUT_VERSION_STRING,IDC_STATIC" in resource,
    "current README identifies v1.4": readme.startswith("# HallJoy v1.4\n"),
    "active product surfaces do not identify 3.9.0": "3.9.0" not in active_text,
    "historical archive evidence is retained": (repo / "docs" / "stability" / "BASELINE_V3_9_0_RU.md").is_file(),
}

failed = []
for name, passed in checks.items():
    print(("PASS" if passed else "FAIL") + ": " + name)
    if not passed:
        failed.append(name)

if failed:
    print("VERSION_IDENTITY_STATIC_AUDIT=FAIL", file=sys.stderr)
    sys.exit(1)

print("VERSION_IDENTITY_STATIC_AUDIT=PASS")
