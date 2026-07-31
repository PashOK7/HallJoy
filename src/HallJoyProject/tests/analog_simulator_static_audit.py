from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[3]
hall = repo / "src" / "HallJoyProject" / "HallJoy"


def read(name: str) -> str:
    return (hall / name).read_text(encoding="utf-8-sig")


project = read("HallJoy.vcxproj")
manifest = read("native_analog_backends.def")
backend = read("analog_simulator_backend.cpp")
model = read("analog_simulator_model.cpp")
common_backend = read("backend.cpp")
runner = (repo / "tools" / "run_analog_simulator.ps1").read_text(encoding="utf-8-sig")

checks = {
    "simulator has a dedicated opt-in build property":
        "'$(HallJoyAnalogSimulator)'=='true'" in project,
    "production property does not enable simulator":
        "HALLJOY_MAD68PR_NATIVE;HALLJOY_PRODUCTION;HALLJOY_ANALOG_SIMULATOR" not in project,
    "simulator sources are excluded from ordinary builds":
        project.count("""<ExcludedFromBuild Condition="'$(HallJoyAnalogSimulator)'!='true'">true</ExcludedFromBuild>""") >= 2,
    "catalog entry is compile-time guarded":
        "#if defined(HALLJOY_ANALOG_SIMULATOR)" in manifest and
        "AnalogSimulator_GetNativeBackendDescriptor" in manifest,
    "backend implementation is compile-time guarded":
        backend.startswith("#if defined(HALLJOY_ANALOG_SIMULATOR)"),
    "runtime activation is explicit":
        "--halljoy-simulate-analog=script" in backend and "HasExactArgument" in backend,
    "simulator never claims VID/PID routing":
        "NativeAnalogRouting_Claim" not in backend,
    "simulator opens no HID device":
        "CreateFile" not in backend and "HidD_" not in backend and "SetupDi" not in backend,
    "telemetry cannot be mistaken for hardware":
        "SIMULATED / NOT HARDWARE" in backend and "hardware=0" in backend,
    "simulator uses the common ViGEm send path":
        "TraceSimulatorPipelineReport(report)" in common_backend and
        "TraceAcceptedSimulatorVigemUpdate(report)" in common_backend and
        "vigem_target_x360_update(g_client, pad, report)" in common_backend,
    "scenario covers disconnect, reconnect and fault":
        "Phase::Disconnected" in model and "Phase::Reconnected" in model and
        "Phase::SourceFault" in model,
    "scenario runner requires graceful shutdown and labels evidence":
        "CloseMainWindow" in runner and "NOT hardware verification" in runner,
}

failed = []
for name, passed in checks.items():
    print(("PASS" if passed else "FAIL") + ": " + name)
    if not passed:
        failed.append(name)

if failed:
    print("ANALOG_SIMULATOR_STATIC_AUDIT=FAIL", file=sys.stderr)
    sys.exit(1)
print("ANALOG_SIMULATOR_STATIC_AUDIT=PASS")
