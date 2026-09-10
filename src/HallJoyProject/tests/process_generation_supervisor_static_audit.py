#!/usr/bin/env python3
from pathlib import Path


project_root = Path(__file__).resolve().parents[1]
repo = project_root.parents[1]
hall = project_root / "HallJoy"
header = (hall / "process_generation_supervisor.h").read_text(encoding="utf-8")
source = (hall / "process_generation_supervisor.cpp").read_text(encoding="utf-8")
protected_handle = (hall / "protected_native_handle.h").read_text(encoding="utf-8")
test = (project_root / "tests" / "process_generation_supervisor_test.cpp").read_text(encoding="utf-8")
project = (hall / "HallJoy.vcxproj").read_text(encoding="utf-8")
filters = (hall / "HallJoy.vcxproj.filters").read_text(encoding="utf-8")
runner = (repo / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8")
design = (repo / "docs" / "v1.4" / "PROCESS_GENERATION_SUPERVISOR_DESIGN_2026-08-22.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


require("class ProcessGenerationSupervisor final" in header and
        "ProcessGenerationSupervisor(const ProcessGenerationSupervisor&) = delete" in header,
        "one non-copyable object owns each process generation")
require("GenerationOutcome::StartupTimeout" in source and
        "GenerationOutcome::ProgressTimeout" in source and
        "GenerationOutcome::ExitBeforeReady" in source and
        "GenerationOutcome::ProtocolViolation" in source,
        "supervision outcomes distinguish lifecycle failure classes")
require("PROC_THREAD_ATTRIBUTE_HANDLE_LIST" in source and
        "request.inheritedHandles" in source and
        "GetHandleInformation" in source,
        "launch uses and validates an explicit inherited-handle list")
require("CREATE_SUSPENDED" in source and "AssignProcessToJobObject" in source and
        "ResumeThread" in source and
        source.index("AssignProcessToJobObject") < source.index("ResumeThread"),
        "child is contained before its primary thread resumes")
require("JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE" in source and
        "TerminateJobObject" in source,
        "per-generation job provides hard containment")
require("ReleaseConfirmed(" in source and
        "restartSafe_ = false" in source and
        "HasUnreapedGeneration()" in source,
        "failed reap retains ownership and blocks replacement")
require("ProtectedNativeHandle process_" in header and
        "ProtectedNativeHandle job_" in header and
        "HANDLE_FLAG_PROTECT_FROM_CLOSE" in protected_handle and
        "SetHandleInformation" in protected_handle and
        protected_handle.index("value_ = value;") <
            protected_handle.index("if (!SetHandleInformation(value"),
        "kernel-enforced ownership protects process and job handles from foreign close")
require("TestProtectedHandleOwnership" in test and
        "assert(!CloseHandle(owner.Get()))" in test and
        "assert(owner.Close(error) && error == ERROR_SUCCESS);" in test and
        "result.processHandleProtected && result.jobHandleProtected" in test and
        "result.processIdentityMatched" in test,
        "Windows regression rejects an illicit close and verifies generation identity")
require("WaitForMultipleObjects" in source and "supervisorStopEvent" in source,
        "supervision is independent of UI timers")
require("vigem_" not in source.lower() and "vigem_" not in header.lower(),
        "generic supervisor has no ViGEm dependency")
require("process_generation_supervisor.cpp" in project and
        "process_generation_supervisor.h" in project and
        "protected_native_handle.h" in project and
        "protected_native_handle.h" in filters and
        "process_generation_supervisor.cpp" in filters,
        "common primitive compiles in the production MSVC project")
require('("process_generation_supervisor", [' in runner and
        'tests / "process_generation_supervisor_test.cpp"' in runner and
        'hall / "process_generation_supervisor.cpp"' in runner,
        "native runner links the real production supervisor")
require("containedAtEntry" in test and "decoyHandleCallSucceeded" in test and
        "WaitForSingleObject(fixture.decoy, 0) == WAIT_TIMEOUT" in test,
        "fake child proves containment and exact decoy-object exclusion")
require("if (preSignalStop)" in test and
        "assert(containedAtEntry == 1 || result.forced)" in test and
        "assert(containedAtEntry == 1);" in test,
        "pre-signalled stop oracle accepts only child entry or a forced contained reap")
for mode in ("exit-before-ready", "never-ready", "stall-after-ready",
             "ignore-stop", "crash-after-ready", "wrong-generation",
             "report-fault"):
    require(mode in test, f"fake-child matrix covers {mode}")
require("kStressGenerations = 1000u" in test and
        "maxActiveChildren" in test and "OpenProcess(SYNCHRONIZE" in test,
        "stress gate requires 1000 sequential generations, no overlap and no live survivor")
require("deliberately non-inheritable" in test and
        'applicationPath += L".missing"' in test and
        test.count("GenerationOutcome::LaunchFailed") >= 2,
        "natural invalid-handle and missing-executable launch failures are restart-safe")
require("Option A" in design and "Option B" in design and "Option C" in design and
        "Chosen option" in design,
        "architecture comparison precedes implementation")

print("PROCESS_GENERATION_SUPERVISOR_STATIC_AUDIT=PASS")
