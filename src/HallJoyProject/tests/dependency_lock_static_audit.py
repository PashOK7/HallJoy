import json
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[3]
lock_path = repo / "tools" / "dependency-lock.json"
lock = json.loads(lock_path.read_text(encoding="utf-8"))
workflow = (repo / ".github" / "workflows" / "native-backend-checks.yml").read_text(encoding="utf-8")
build = (repo / "tools" / "build.ps1").read_text(encoding="utf-8-sig")
plugin_build = (repo / "third_party" / "UniversalAnalogPluginFixed" / "tools" /
                "build_fixed_plugin.ps1").read_text(encoding="utf-8-sig")


def full_sha(value: str) -> bool:
    return re.fullmatch(r"[0-9a-f]{40}", value) is not None


checks = {
    "lock schema is explicit": lock.get("schemaVersion") == 1,
    "Sun and Soup use immutable commits": all(
        full_sha(lock["sources"][name]["commit"]) for name in ("sun", "soup")
    ),
    "Soup overlay has locked per-file integrity":
        len(lock["sources"]["soup"]["patchedOverlayFiles"]) == 5 and
        all(re.fullmatch(r"[0-9A-F]{64}", value) for value in
            lock["sources"]["soup"]["patchedOverlayFiles"].values()) and
        "SoupOverlayFiles" in plugin_build and "Get-NormalizedTextSha256" in plugin_build,
    "GitHub Actions use immutable commits": all(
        full_sha(action["commit"]) for action in lock["githubActions"].values()
    ),
    "workflow uses fixed runner labels":
        f'runs-on: {lock["toolchains"]["linuxRunner"]}' in workflow and
        f'runs-on: {lock["toolchains"]["windowsRunner"]}' in workflow and
        "ubuntu-latest" not in workflow,
    "workflow uses every locked action SHA": all(
        f'{action["name"]}@{action["commit"]}' in workflow
        for action in lock["githubActions"].values()
    ),
    "official build consumes dependency lock":
        "dependency-lock.json" in build and "binaryInputs.vigemClient" in build and
        "Copy-Item -LiteralPath $dependencyLockPath -Destination $sendDir" in build,
    "plugin bootstrap consumes locked source commits":
        "dependency-lock.json" in plugin_build and
        "sources.sun" in plugin_build and "sources.soup" in plugin_build,
    "official build requires portable compiler tests":
        "run_native_backend_checks.py" in build and "--require-compiler" in build,
    "production warning allowlist is enforced":
        "allowedProductionWarning" in build and "Unexpected production compiler/linker warnings" in build,
}

failed = []
for name, passed in checks.items():
    print(("PASS" if passed else "FAIL") + ": " + name)
    if not passed:
        failed.append(name)

if failed:
    print("DEPENDENCY_LOCK_STATIC_AUDIT=FAIL", file=sys.stderr)
    sys.exit(1)
print("DEPENDENCY_LOCK_STATIC_AUDIT=PASS")
