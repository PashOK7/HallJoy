#!/usr/bin/env python3
"""Check the maintained HallJoy layout without launching the application."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
MOVED_DOCS = {
    "SAYO_DEVICE_NOTES.md": "protocols",
    "CUSTOM_UI_ARCHITECTURE.md": "development",
    "INPUT_OVERLAY_NOTES.md": "development",
    "PERFORMANCE_AND_STABILITY_GOALS.md": "development",
    "TESTING.md": "development",
    "BUILD_README.txt": "development",
    "SOURCE_AUDIT_V6.md": "archive/source-v6",
    "MADLIONS_SAFEHID_V6.md": "archive/source-v6",
    "DIAGNOSTIC_SUPPORT.md": "archive/source-v6",
    "BUNDLED_ANALOG_STACK.txt": "archive/source-v6",
}

def main() -> int:
    errors = []
    for relative in (
        "HallJoy.exe", "HallJoyUniversalAnalogHost.dll", "HallJoy.log",
        ".analysis", "_backups", "._backups", "_build_checks", "build/output",
        "src/HallJoyProject/x64", "src/HallJoyProject/HallJoy/x64",
        "src/HallJoyProject/HallJoy/HallJoy", "src/HallJoyProject/runtime",
        "third_party/UniversalAnalogPluginFixed/dist",
        "third_party/UniversalAnalogPluginFixed/int",
        "third_party/UniversalAnalogPluginFixed/Soup",
        "third_party/UniversalAnalogPluginFixed/.build-tools",
    ):
        if (ROOT / relative).exists():
            errors.append(f"Obsolete generated location: {relative}")
    errors.extend(f"Loose compiler output: {p.name}" for p in ROOT.glob("*.obj"))
    docs = [
        ROOT / "README.md", ROOT / "docs/README.md",
        ROOT / "docs/current/PROJECT_LAYOUT.md",
        ROOT / "docs/validation/STRUCTURE_MIGRATION_2026-09-06.md",
        ROOT / "src/HallJoyProject/README.md",
    ]
    for name, folder in MOVED_DOCS.items():
        canonical = ROOT / "docs" / folder / name
        redirect = ROOT / "src/HallJoyProject" / name
        docs.extend((canonical, redirect))
        if redirect.exists() and "Document moved" not in redirect.read_text(encoding="utf-8-sig"):
            errors.append(f"Duplicate document instead of redirect: {redirect.relative_to(ROOT)}")
    for name, folder in (
        ("AV_FALSE_POSITIVE_NOTES.md", "archive/source-v6"),
        ("BUILD_PIPELINE_AUDIT.md", "archive/source-v6"),
        ("V3_6_LOG_ANALYSIS_AND_FIX.md", "archive/source-v6"),
        ("HARDWARE_LOG_SUMMARY.md", "validation"),
        ("MAD68_PRO_R_FIRMWARE_FINAL_AUDIT.md", "protocols"),
        ("MAD68_PRO_R_HALL_PROTOCOL_SPEC.md", "protocols"),
    ):
        docs.extend((ROOT / "docs/current" / name, ROOT / "docs" / folder / name))
    links = 0
    for doc in docs:
        if not doc.is_file():
            errors.append(f"Missing maintained document: {doc.relative_to(ROOT)}")
            continue
        text = doc.read_text(encoding="utf-8-sig")
        for match in re.finditer(r"!?\[[^\]\n]*\]\(([^)\n]+)\)", text):
            target = match.group(1).strip().strip("<>")
            if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("#"):
                continue
            target = target.split("#", 1)[0].replace("%20", " ")
            if not target:
                continue
            links += 1
            if not (doc.parent / target).resolve().exists():
                errors.append(f"Broken local link: {doc.relative_to(ROOT)} -> {target}")
    for library in ("wooting_analog_common.lib", "wooting_analog_common.a"):
        if not (ROOT / "third_party/UniversalAnalogPluginFixed" / library).is_file():
            errors.append(f"Required plugin linker input missing: {library}")
    for error in errors:
        print("FAIL:", error)
    if errors:
        return 1
    print(f"PROJECT_LAYOUT=PASS documents={len(docs)} local_links={links}; no runtime input")
    return 0

if __name__ == "__main__":
    sys.exit(main())
