#!/usr/bin/env python3
"""Read HallJoy's local context without starting processes, devices or network."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
CODE = "src/HallJoyProject/HallJoy/"
CORE = (
    "AGENTS.md", "docs/current/OWNER_CONTEXT.md", "docs/README.md",
    "docs/current/PROJECT_LAYOUT.md", "docs/development/AGENT_WORKFLOW.md",
    "docs/development/BUILD_README.txt", "docs/development/TESTING.md",
    "docs/current/CODEX_WORKFLOW_2026-10-31.md",
)
ARCHIVES = (
    "docs/archive/OWNER_CONTEXT_BEFORE_CODEX_2026-10-31.md",
    "docs/archive/DOCUMENTATION_INDEX_BEFORE_CODEX_2026-10-31.md",
)
TOPICS = {
    "build": ["docs/development/BUILD_README.txt", "docs/development/TESTING.md",
              "tools/build_release.ps1", "tools/run_native_backend_checks.py"],
    "layouts": ["docs/current/LAYOUT_PIPELINE.md", "tools/layout_pipeline.py",
                "tools/layout_catalog.json", CODE + "keyboard_layout.cpp"],
    "support": ["docs/development/SUPPORT_STATUS_SYNC.md",
                "docs/development/KEYBOARD_SHEET_RULES.md",
                "docs/development/supported_layouts.json", "tools/support_notice_catalog.py",
                "tools/check_supported_layouts.py"],
    "firmware": ["docs/development/FIRMWARE_BEHAVIOR_REVIEW.md",
                 "docs/current/FIRMWARE_FOUNDATION.md", "docs/current/FIRMWARE_CORPUS.md",
                 "tools/firmware_inspect.py", CODE + "native_analog_backends.def"],
    "ui": ["docs/development/CUSTOM_UI_ARCHITECTURE.md",
           "docs/current/TAB_TRANSITIONS_2026-10-03.md",
           CODE + "custom_page_surface.h", CODE + "custom_page_controls.h"],
    "profiles": ["docs/current/GAME_PROFILES_V2_2026-10-03.md",
                 "docs/current/STARTUP_PROFILE_RECOVERY_2026-09-11.md",
                 CODE + "global_profiles.cpp", CODE + "profile_ini.cpp", CODE + "settings_ini.cpp"],
    "diagnostics": ["docs/development/SUPPORT_DIAGNOSTICS_CONTRACT.md",
                    "tools/analyze_support_log.py", CODE + "input_trace.cpp"],
    "release": ["docs/legal/PUBLICATION_POLICY.json", "docs/legal/DISTRIBUTION_AUDIT_2026-09-27.md",
                "docs/legal/HISTORY_CLEANUP_2026-09-27.md", "tools/release.py",
                "tools/check_publication_inputs.py"],
}
VERSION = CODE + "version.h"
EXE = "build/bin/Release/x64/HallJoy.exe"
BUDGETS = {"docs/current/OWNER_CONTEXT.md": 20 * 1024, "docs/README.md": 8 * 1024}
LINK = re.compile(r"!?\[[^\]\n]*\]\(([^)\n]+)\)")


def local_links(doc: Path):
    for match in LINK.finditer(doc.read_text(encoding="utf-8-sig")):
        target = match.group(1).strip().strip("<>")
        if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("#"):
            continue
        target = unquote(target.split("#", 1)[0])
        if target:
            yield target, (doc.parent / target).resolve()


def search(root: Path, term: str, limit: int = 20):
    # Bounded literal search; queries are never sent to an external service.
    if not term.strip():
        return []
    result = []
    paths = sorted((root / "docs/current").glob("*.md"))
    paths += [root / ARCHIVES[0]]
    for path in paths:
        if not path.is_file():
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
            if term.casefold() in line.casefold():
                result.append({"path": path.relative_to(root).as_posix(), "line": number,
                               "text": line.strip()[:240]})
                if len(result) >= limit:
                    return result
    return result


def context(root: Path, topic: str | None = None, hash_exe: bool = False):
    root = root.resolve()
    errors = []
    paths = list(dict.fromkeys((*CORE, *ARCHIVES, VERSION,
                              *(p for group in TOPICS.values() for p in group))))
    for relative in paths:
        if not (root / relative).is_file():
            errors.append("Missing required entry: " + relative)
    for relative in CORE:
        path = root / relative
        if path.is_file():
            for target, resolved in local_links(path):
                if not resolved.is_file():
                    errors.append("Broken local link: " + relative + " -> " + target)
    for relative, budget in BUDGETS.items():
        path = root / relative
        if path.is_file() and path.stat().st_size > budget:
            errors.append("Startup context exceeds byte budget: " + relative)
    instruction_files = [root / "AGENTS.md", root.parent / "AGENTS.md"]
    if sum(p.stat().st_size for p in instruction_files if p.is_file()) > 32 * 1024:
        errors.append("AGENTS.md chain exceeds the default 32 KiB budget")
    version_file = root / VERSION
    version = None
    if version_file.is_file():
        match = re.search(r'^#define HALLJOY_VERSION_STRING_FULL "([^"]+)"',
                          version_file.read_text(encoding="utf-8-sig"), re.MULTILINE)
        version = match.group(1) if match else None
        if version is None:
            errors.append("Source version is not readable")
    state_file = root / ".local/release-state.json"
    state = None
    if state_file.is_file():
        try:
            raw = json.loads(state_file.read_text(encoding="utf-8-sig"))
            if not isinstance(raw, dict):
                raise ValueError("expected an object")
            state = {"version": raw.get("version"), "main": raw.get("main")}
        except (ValueError, OSError) as exc:
            errors.append("Invalid local release baseline: " + str(exc))
    artifact = root / EXE
    delivery = {"path": EXE, "exists": artifact.is_file()}
    if artifact.is_file():
        delivery["bytes"] = artifact.stat().st_size
        if hash_exe:
            delivery["sha256"] = hashlib.sha256(artifact.read_bytes()).hexdigest()
    return {
        "source_version": version,
        "workspace_has_git": (root / ".git").exists(),
        "publication_baseline_local_only": state,
        "remote_state": "not queried", "connector_state": "verify with session tools",
        "delivery": delivery,
        "startup_bytes": {p: (root / p).stat().st_size for p in BUDGETS if (root / p).is_file()},
        "documents": list(CORE), "topic": TOPICS.get(topic) if topic else None,
        "available_commands": [name for name in ("python", "git", "gh", "clang++", "rg")
                               if shutil.which(name)],
        "errors": errors,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--topic", choices=sorted(TOPICS))
    parser.add_argument("--search", metavar="TEXT")
    parser.add_argument("--hash-exe", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        data = context(root, args.topic, args.hash_exe)
        if args.search is not None:
            data["search"] = search(root, args.search)
        if args.json:
            print(json.dumps(data, ensure_ascii=False, indent=2))
        else:
            print("Source version:", data["source_version"])
            print("Workspace .git:", data["workspace_has_git"])
            print("Publication baseline (local only):", data["publication_baseline_local_only"])
            print("Delivery:", json.dumps(data["delivery"]))
            print("Startup bytes:", data["startup_bytes"])
            for path in data["topic"] or data["documents"]:
                print("  " + path)
            for hit in data.get("search", []):
                print(f"{hit['path']}:{hit['line']}: {hit['text']}")
            for error in data["errors"]:
                print("FAIL:", error)
            if args.check:
                print("PROJECT_CONTEXT=" + ("FAIL" if data["errors"] else "PASS") + "; local files only")
        return int(args.check and bool(data["errors"]))
    except (OSError, ValueError) as exc:
        print("PROJECT_CONTEXT=FAIL: " + str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
