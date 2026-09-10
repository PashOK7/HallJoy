#!/usr/bin/env python3
"""Ensure per-user instance ownership is acquired before parent startup work."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hall = root / "HallJoy"
main = (hall / "main.cpp").read_text(encoding="utf-8")
source = (hall / "instance_guard.cpp").read_text(encoding="utf-8")
header = (hall / "instance_guard.h").read_text(encoding="utf-8")
project = (hall / "HallJoy.vcxproj").read_text(encoding="utf-8")

for token in (
    'L"Global\\\\HallJoy.InstanceGuard.v1."',
    "OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)",
    "GetTokenInformation(token, TokenUser",
    "ConvertSidToStringSidW",
    "CreateMutexW(nullptr, FALSE, name)",
    "createError == ERROR_ALREADY_EXISTS",
):
    assert token in source, token
for token in ("class Guard final", "AcquireForCurrentUser", "Conflicted,"):
    assert token in header, token
for token in ("instance_guard.h", "instance_guard.cpp"):
    assert token in project, token

guard = main.index("instanceGuard.AcquireForCurrentUser()")
for later_startup in (
    "StabilityTrace_Init();",
    "DebugLog_Init();",
    "EmbeddedAnalogStack_Prepare(hInst)",
    "ProviderV2QualificationReport_Begin()",
):
    assert guard < main.index(later_startup), later_startup
assert main.index("AnalogHost_TryRunCommand(analogHostExit)") < guard
assert main.index("DebugLog_TryRunExitWatchdogCommand()") < guard
test = (root / "tests" / "instance_guard_windows_test.cpp").read_text(encoding="utf-8")
for token in ("CreateProcessW", "--child", "WaitForSingleObject(child.hProcess, 5000)"):
    assert token in test, token
print("Instance guard static audit passed")
