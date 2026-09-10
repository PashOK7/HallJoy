#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"
REPO = ROOT.parents[1]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


header = read(HALL / "provider_v2_data_plane_layout.h")
source = read(HALL / "provider_v2_data_plane_layout.cpp")
backend = read(HALL / "backend.cpp")
shared = read(HALL / "analog_host_shared.h")
runner = read(REPO / "tools" / "run_native_backend_checks.py")
design = read(REPO / "docs" / "v1.4" /
              "UAP_PROVIDER_V2_SPLIT_DATA_PLANE_DESIGN_2026-08-23.md")


def require(text: str, needle: str, purpose: str) -> None:
    if needle not in text:
        raise SystemExit(f"FAIL: {purpose}: missing {needle!r}")


require(header, "kSlotCount = 2", "split plane keeps two immutable slots")
require(header, "kMaximumDevices = 4096", "device safety ceiling is explicit")
require(header, "kMaximumSamples = 1048576", "sample safety ceiling is explicit")
require(source, "CheckedMultiply", "layout multiplication is overflow checked")
require(source, "CheckedAlign", "layout alignment is overflow checked")
require(source, "expectedTransactionToken", "read requires an explicit transaction token")
require(source, "IsAuthoritative", "committed slot must be complete and untruncated")
require(runner, '"provider_v2_data_plane_layout"', "portable gate is registered")
require(design, "FILE_MAP_READ", "parent read-only Windows ownership remains a named B2i-b gate")

require(backend, "AnalogHostClient_AcquireProviderV2Snapshot",
        "B2i-c shadow consumes the parent-owned dynamic broker")
if "providerV2Samples" in shared:
    raise SystemExit("FAIL: B2i-c left the old fixed Provider V2 payload in SharedState")

print("PASS: split-plane layout is bounded and the fixed payload is absent")
