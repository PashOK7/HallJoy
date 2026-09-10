#!/usr/bin/env python3
"""Source guard for RM-05's independent parent producer lease."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


shared = read(HALL / "vigem_output_shared.h")
lease = read(HALL / "vigem_output_producer_lease.h")
channel_h = read(HALL / "vigem_output_channel.h")
channel_cpp = read(HALL / "vigem_output_channel.cpp")
runtime_h = read(HALL / "vigem_output_runtime.h")
runtime_cpp = read(HALL / "vigem_output_runtime.cpp")
backend = read(HALL / "backend.cpp")
host = read(HALL / "vigem_output_process_host.cpp")
project = read(HALL / "HallJoy.vcxproj")
filters = read(HALL / "HallJoy.vcxproj.filters")
runner = read(REPO / "tools" / "run_native_backend_checks.py")
test = read(ROOT / "tests" / "vigem_output_producer_lease_test.cpp")

require("kSharedVersion = 2u" in shared and
        "producerLeaseSequence" in shared and
        "sizeof(SharedStateV1) == 640u" in shared,
        "lease-aware snapshot protocol is versioned without mapping growth")
require("kProducerLeaseDeadlineMs = 200u" in lease and
        "ProducerLeaseState::Fresh" in lease and
        "ProducerLeaseState::Stalled" in lease and
        "EvaluateProducerLease" in lease,
        "fake-clock lease state machine has one documented deadline")
require("PublishProducerLease" in channel_h and
        "ReadProducerLeaseState" in channel_h and
        "ChildPublishProducerStalled" in channel_h and
        "reservedControl[kProducerLeaseSequenceIndex]" in channel_cpp,
        "IPC exposes separate parent progress and child stale telemetry")
require("PublishProducerProgress" in runtime_h and
        "PublishProducerProgress(nowMs)" in backend and
        "publishDue" in backend,
        "every calculated enabled backend tick refreshes progress independently of dedup")
require("SnapshotMayApply" in host and
        "snapshot.producerLeaseSequence > lastNeutralizedLeaseSequence" in host and
        "MakeNeutralSnapshot" in host and
        "ChildPublishProducerStalled" in host,
        "child neutralizes once and cannot replay a pre-neutral nonzero snapshot")
require("transport.Apply(\n                    MakeNeutralSnapshot" in host and
        "ProducerLeaseState::Stalled" in host,
        "real child applies a bounded neutral transport frame on producer stall")
require("vigem_output_producer_lease.h" in project and
        "vigem_output_producer_lease.h" in filters and
        "vigem_output_producer_lease_test.cpp" in runner,
        "production header and portable fake-clock test are wired")
require("held key" in test and "WrongGeneration" in test and
        "ProducerLeaseState::Stalled" in test,
        "portable oracle covers held reports, stall and stale generation")

print("VIGEM_OUTPUT_PRODUCER_FRESHNESS_STATIC_AUDIT=PASS")
