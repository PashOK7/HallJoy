#!/usr/bin/env python3
"""Structural invariants for the serialized aggregate engine owner."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / "HallJoy" / "engine_runtime_owner.cpp").read_text(encoding="utf-8")
header = (root / "HallJoy" / "engine_runtime_owner.h").read_text(encoding="utf-8")
project = (root / "HallJoy" / "HallJoy.vcxproj").read_text(encoding="utf-8")

required_source = (
    "ExecutePause(g_controller, operations, error)",
    "ExecuteResume(g_controller, operations, error)",
    "WaitForMultipleObjects(2, waits, FALSE, INFINITE)",
    "kEngineRuntimeOwnerJoinTimeoutMs = 5000u",
    "g_controller = runtime_command::Controller{}",
    "g_stopRequested = true",
    "EngineRuntimeOwnerThreadProc",
    "RunWorkerEntryBarrier",
    "std::try_to_lock",
    "PublishSnapshotLocked",
    "void StateChanged(const runtime_command::SnapshotV1&) noexcept { PublishSnapshotLocked(); }",
    "g_publicState.store",
)
required_header = (
    "struct OperationsV1 final",
    "EngineRuntimeOwner_RequestPause",
    "EngineRuntimeOwner_RequestResume",
    "EngineRuntimeOwner_Snapshot",
    "EngineRuntimeOwner_Stop",
)
for token in required_source:
    assert token in source, token
for token in required_header:
    assert token in header, token
assert 'ClCompile Include="engine_runtime_owner.cpp"' in project
assert 'ClInclude Include="engine_runtime_owner.h"' in project
assert "EngineRuntimeOwner_Snapshot() noexcept\n{\n    runtime_command::SnapshotV1 snapshot{};" in source
print("Engine runtime owner static audit passed")
