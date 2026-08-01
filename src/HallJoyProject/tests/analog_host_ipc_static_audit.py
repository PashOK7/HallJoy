#!/usr/bin/env python3
"""Static gate for V14-10B analog-host IPC capability transport."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def require(text: str, marker: str, description: str) -> None:
    if marker not in text:
        raise SystemExit(f"FAIL: {description}: missing {marker!r}")
    print(f"PASS: {description}")


host = read(HALL / "analog_host_client.cpp")
shared = read(HALL / "analog_host_shared.h")
runner = read(REPO / "tools" / "run_analog_simulator.ps1")
build = read(REPO / "tools" / "build.ps1")

for forbidden in (
    "Local\\\\HallJoyAnalog",
    "BuildIpcName(",
    "OpenFileMappingW(",
    "OpenEventW(",
):
    if forbidden in host:
        raise SystemExit(f"FAIL: named analog-host IPC remains: {forbidden!r}")
print("PASS: analog-host transport has no named mapping or event path")

require(host, "CreateFileMappingW(INVALID_HANDLE_VALUE, &inheritedSecurity", "mapping is unnamed and inheritable")
require(host, "CreateEventW(&inheritedSecurity, TRUE, FALSE, nullptr)", "stop event is unnamed and inheritable")
require(host, "CreateEventW(&inheritedSecurity, FALSE, FALSE, nullptr)", "snapshot event is unnamed and inheritable")
require(host, "DuplicateHandle(", "owner process is represented by an inherited kernel handle")
require(host, "PROC_THREAD_ATTRIBUTE_HANDLE_LIST", "child inheritance is restricted to an explicit handle list")
require(host, "EXTENDED_STARTUPINFO_PRESENT", "extended process startup is enabled")
require(host, "command.data(), nullptr, nullptr, TRUE", "CreateProcess enables explicit handle inheritance")
require(host, "BCryptGenRandom(", "launch generation token uses the system CSPRNG")

require(shared, "kVersion = 10", "shared schema version records the transport change")
require(shared, "volatile LONG ownerPid", "shared schema binds the owner PID")
require(shared, "volatile LONG64 launchNonce", "shared schema binds the launch generation token")
require(host, "GetHandleInformation(mapping", "child rejects a non-inherited mapping handle")
require(host, "GetProcessId(ownerProcess) != launch.ownerPid", "child validates the inherited owner handle identity")
require(host, "shared->launchNonce", "child validates the mapping generation token")
require(host, "child.identity_rejected", "parent rejects a ready status from the wrong child PID")
require(host, "transport=inherited_handles named_objects=0 handle_list=1 owner_handle=1 generation_bound=1", "runtime publishes the IPC policy")

require(host, "--halljoy-test-analog-host-ipc-handle-rejection", "simulator has an invalid-handle injection")
require(host, "ipc.handle_rejected", "supervisor records invalid-handle rejection")
require(runner, "InjectAnalogHostIpcHandleRejection", "runner exposes the invalid-handle scenario")
require(runner, "child_exit=31 restart_allowed=1", "runner requires rejection and bounded recovery evidence")
require(build, "analog_host_ipc_static_audit.py", "official build requires the IPC gate")

print("ANALOG_HOST_IPC_STATIC_AUDIT=PASS")
