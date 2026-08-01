from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[3]
hall = repo / "src" / "HallJoyProject" / "HallJoy"


def read(name: str) -> str:
    return (hall / name).read_text(encoding="utf-8-sig")


embedded = read("embedded_analog_stack.cpp")
host = read("analog_host_client.cpp")
deps = read("app_deps.cpp")
backend_h = read("backend.h")
project = read("HallJoy.vcxproj")
runner = (repo / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8-sig")

checks = {
    "runtime falls back to versioned per-user storage":
        "CSIDL_LOCAL_APPDATA" in embedded and "\\\\HallJoy\\\\Runtime\\\\v" in embedded,
    "portable executable-directory runtime remains supported":
        "EmbeddedAnalogRuntimeLocation::BesideExecutable" in embedded,
    "resource extraction is atomic and flushed":
        "MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH" in embedded and
        "FlushFileBuffers" in embedded,
    "extracted runtime is compared to exact embedded bytes":
        embedded.count("ResourceEqualsFile") >= 3 and "exact_resource_match=1" in embedded,
    "fallback injection exists only in trace builds":
        "HALLJOY_STABILITY_TRACE" in embedded and
        "--halljoy-test-uap-exe-write-denied" in embedded,
    "verified absolute path is passed to child host":
        "EmbeddedAnalogStack_PrivatePluginPath()" in host and
        "launch.privatePluginPath = argv[i + 7]" in host and
        "LoadHostApi(api, launch.privatePluginPath)" in host,
    "child path uses tested Windows argument quoting":
        "windows_command_line::QuoteArgument" in host and "windows_command_line.h" in project,
    "backend issues describe private runtime rather than system SDK":
        "BackendInitIssue_PrivateUapUnavailable" in backend_h and
        "BackendInitIssue_WootingSdkMissing" not in backend_h,
    "recovery explicitly rejects irrelevant system SDK installation":
        "Installing a system-wide Wooting Analog SDK or global UAP cannot repair this path" in deps,
    "recovery contains no system SDK or global UAP installer":
        "WootingKb/wooting-analog-sdk" not in deps and
        "WootingAnalogPlugins" not in deps and
        "AnalogSense/universal-analog-plugin" not in deps,
    "ViGEm remains the only downloadable runtime dependency":
        "ViGEm/ViGEmBus" in deps and "RunInstallerElevatedAndWait" in deps,
    "runtime scenario can force and verify per-user fallback":
        "ForceUserUapRuntime" in runner and "location=user" in runner,
}

failed = []
for name, passed in checks.items():
    print(("PASS" if passed else "FAIL") + ": " + name)
    if not passed:
        failed.append(name)

if failed:
    print("PRIVATE_UAP_RUNTIME_STATIC_AUDIT=FAIL", file=sys.stderr)
    sys.exit(1)
print("PRIVATE_UAP_RUNTIME_STATIC_AUDIT=PASS")
