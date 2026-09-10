import json
import hashlib
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
guidance_policy = (repo / "src" / "HallJoyProject" / "HallJoy" /
                   "dependency_guidance_policy.h").read_text(encoding="utf-8-sig")
notices = (repo / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8-sig")


def full_sha(value: str) -> bool:
    return re.fullmatch(r"[0-9a-f]{40}", value) is not None

overlay_root=repo/'third_party/UniversalAnalogPluginFixed/overlay/Soup/soup'
overlay_files=lock['sources']['soup']['patchedOverlayFiles']
overlay_integrity=(set(overlay_files)=={p.name for p in overlay_root.iterdir() if p.is_file()} and
    all(hashlib.sha256((overlay_root/name).read_text(encoding='utf-8-sig').encode('utf-8')).hexdigest().upper()==digest
        for name,digest in overlay_files.items()))


checks = {
    "lock schema is explicit": lock.get("schemaVersion") == 1,
    "Sun and Soup use immutable commits": all(
        full_sha(lock["sources"][name]["commit"]) for name in ("sun", "soup")
    ),
    "Soup overlay has locked per-file integrity":
        overlay_integrity and
        all(re.fullmatch(r"[0-9A-F]{64}", value) for value in
            lock["sources"]["soup"]["patchedOverlayFiles"].values()) and
        "SoupOverlayFiles" in plugin_build and "Get-NormalizedTextSha256" in plugin_build,
    "GitHub Actions use immutable commits": all(
        full_sha(action["commit"]) for action in lock["githubActions"].values()
    ),
    "embedded ViGEm runtime dependency is immutable and matches production":
        lock["runtimeDependencies"]["vigemBus"] == {
            "version": "1.22.0",
            "releasePage": "https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0",
            "installerUrl": "https://github.com/nefarius/ViGEmBus/releases/download/v1.22.0/ViGEmBus_1.22.0_x64_x86_arm64.exe",
            "installerPath": "src/HallJoyProject/third_party/ViGEmBus/ViGEmBus_1.22.0_x64_x86_arm64.exe",
            "installerSize": 6278576,
            "installerSha256": "89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A",
            "installationPolicy": "embedded-pinned-one-click",
        } and
        'kPinnedVigemVersion[] = L"1.22.0"' in guidance_policy and
        'L"https://github.com/nefarius/ViGEmBus/releases/tag/v1.22.0"' in guidance_policy and
        "89220A7865076B342892F98865F3499FB7C4CFD673159E89D352C360FD014C6A" in guidance_policy and
        "6278576u" in guidance_policy,
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
        "runtimeDependencies.vigemBus" in build and
        "Copy-Item -LiteralPath $dependencyLockPath -Destination $releaseDir" in build,
    "pinned MIT notices ship with HallJoy.exe":
        all(lock["sources"][name]["commit"] in notices for name in ("sun", "soup")) and
        "Copyright (c) 2021-2026 Calamity, Inc." in notices and
        "Copyright (c) 2022-2025 Calamity, Inc." in notices and
        "BSD 3-Clause License" in notices and
        "Copyright (c) 2016-2020, Nefarius Software Solutions e.U." in notices and
        "Copy-Item -LiteralPath $thirdPartyNoticesPath -Destination $releaseDir" in build,
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
