#!/usr/bin/env python3
"""Static gate for V14-10A mouse IPC creation and memory-order correctness."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def require(text: str, marker: str, description: str) -> None:
    if marker not in text:
        raise SystemExit(f"FAIL: {description}: missing {marker!r}")
    print(f"PASS: {description}")


mouse = read(HALL / "mouse_ipc.cpp")
header = read(HALL / "mouse_ipc.h")
app = read(HALL / "app.cpp")
runner = read(REPO / "tools" / "run_analog_simulator.ps1")
build = read(REPO / "tools" / "build.ps1")

create_index = mouse.find("const DWORD createError = GetLastError();")
map_index = mouse.find("MapViewOfFile(", create_index)
if create_index < 0 or map_index < 0 or create_index > map_index:
    raise SystemExit("FAIL: CreateFileMapping last-error is not captured before MapViewOfFile")
print("PASS: creation disposition is captured before another Win32 call")

if "if (GetLastError() != ERROR_ALREADY_EXISTS)" in mouse:
    raise SystemExit("FAIL: stale MapViewOfFile last-error still controls mapping initialization")
print("PASS: stale last-error initialization branch is absent")

require(mouse, "const bool created = createError != ERROR_ALREADY_EXISTS;", "new and existing mappings are distinguished exactly")
require(mouse, "schema_valid=1", "successful initialization records schema validation")
require(mouse, "ERROR_REVISION_MISMATCH", "invalid existing schema is rejected")
require(mouse, "Publish magic last", "new mapping schema has a publication boundary")
require(mouse, "AtomicRead(&g_mouseIpc->asiAttached)", "ASI attachment uses an interlocked read")
require(mouse, "AtomicRead(&g_mouseIpc->asiHeartbeat)", "ASI heartbeat uses an interlocked read")
require(header, "kHallJoyMouseIpcStructSize = 40", "v1 mapping ABI size is fixed")
require(header, "former reserved1 offset", "structSize preserves the external v1 field layout")
require(header, 'L"Local\\\\HallJoy_MouseBridge_v1"', "stable ASI mapping name is unchanged")

require(mouse, "MouseIpc_RunPolicySelfTest", "Windows policy self-test exists")
require(mouse, "existingShared->blockMouseWanted", "self-test proves an existing payload is preserved")
require(mouse, "invalidShared->magic", "self-test supplies an invalid existing schema")
require(app, 'L"--halljoy-test-mouse-ipc-policy"', "simulator invokes the real mouse IPC policy")
require(runner, "[component=mouse-ipc][event=policy.self_test] passed=1", "runner requires mouse IPC runtime evidence")
require(build, "mouse_ipc_static_audit.py", "official build requires the mouse IPC gate")

print("MOUSE_IPC_STATIC_AUDIT=PASS")
