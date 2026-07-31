#!/usr/bin/env python3
"""Verify that the S01 lifecycle contract is integrated by V14-05."""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
TESTS = ROOT / "tests"
RUNNER = ROOT.parents[1] / "tools" / "run_native_backend_checks.py"
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    lifecycle = HALL / "worker_lifecycle.h"
    primitives = HALL / "worker_primitives.h"
    require(lifecycle.is_file(), "worker lifecycle contract exists")
    require(primitives.is_file(), "worker primitive seam exists")

    lifecycle_text = lifecycle.read_text(encoding="utf-8")
    primitives_text = primitives.read_text(encoding="utf-8")
    require("namespace halljoy::lifecycle" in lifecycle_text, "lifecycle contract is namespaced")
    require("enum class WorkerState" in lifecycle_text, "all worker states have one common enum")
    for state in ("Stopped", "Starting", "Running", "StopRequested", "Joined", "Faulted", "Poisoned"):
        require(state in lifecycle_text, f"worker state {state} is present")
    require("class WorkerPrimitives" in primitives_text, "wait/time/thread seam is explicit")
    require("MonotonicMilliseconds" in primitives_text, "time seam is injectable")
    require("WaitForStop" in primitives_text, "wait seam is injectable")
    require("StartThread" in primitives_text and "JoinThread" in primitives_text,
            "thread start/join seams are injectable")

    registry_contract = (HALL / "native_backend_lifecycle_registry.h").read_text(encoding="utf-8")
    registry_runtime = (HALL / "native_analog_backend_registry.cpp").read_text(encoding="utf-8")
    descriptor = (HALL / "native_analog_backend.h").read_text(encoding="utf-8")
    require('"worker_lifecycle.h"' in registry_contract,
            "production registry controller uses the lifecycle contract")
    require("BackendLifecycleRegistry" in registry_runtime and "g_started" not in registry_runtime,
            "runtime registry no longer uses lossy boolean started state")
    require("StopResult (*stop)" in descriptor and "GenerationId generation" in descriptor,
            "backend stop ABI carries a generation-scoped StopResult")
    require("NativeAnalogBackends_GetLifecycle" in registry_runtime,
            "exact lifecycle diagnostics are exposed")

    project = ET.parse(HALL / "HallJoy.vcxproj").getroot()
    includes = {node.attrib.get("Include") for node in project.findall(".//m:ClInclude", NS)}
    compiles = {node.attrib.get("Include") for node in project.findall(".//m:ClCompile", NS)}
    require("worker_lifecycle.h" in includes, "lifecycle header is visible in MSVC project")
    require("worker_primitives.h" in includes, "primitive seam header is visible in MSVC project")
    require("native_backend_lifecycle_registry.h" in includes,
            "production lifecycle registry header is visible in MSVC project")
    require("worker_lifecycle.cpp" not in compiles and "worker_primitives.cpp" not in compiles,
            "S01 adds no production object file")

    runner_text = RUNNER.read_text(encoding="utf-8")
    require("worker_lifecycle_test.cpp" in runner_text, "lifecycle unit test is in portable gate")
    require("worker_primitives_test.cpp" in runner_text, "primitive seam test is in portable gate")
    require("native_backend_lifecycle_registry_test.cpp" in runner_text,
            "failure-injected registry test is in portable gate")
    require((TESTS / "worker_lifecycle_test.cpp").is_file(), "lifecycle unit test exists")
    require((TESTS / "worker_primitives_test.cpp").is_file(), "primitive seam unit test exists")
    require((TESTS / "native_backend_lifecycle_registry_test.cpp").is_file(),
            "failure-injected registry unit test exists")

    print("LIFECYCLE_SCAFFOLDING_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
