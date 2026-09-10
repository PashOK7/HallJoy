from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


adapter = (HALL / "native_analog_snapshot_adapter.cpp").read_text(encoding="utf-8")
v2_values = (HALL / "analog_provider_v2.cpp").read_text(encoding="utf-8")
backend_header = (HALL / "native_analog_backend.h").read_text(encoding="utf-8")
registry_header = (HALL / "native_analog_backend_registry.h").read_text(encoding="utf-8")
registry = (HALL / "native_analog_backend_registry.cpp").read_text(encoding="utf-8")
spark = (HALL / "backend.cpp").read_text(encoding="utf-8")
spark_transport = (HALL / "backend_sparklink.inc").read_text(encoding="utf-8")
runner = (ROOT.parents[1] / "tools" / "run_native_backend_checks.py").read_text(encoding="utf-8")

require("ValueFromLegacyMilli" in adapter and "LegacyQuantized" in v2_values,
        "legacy native values use the central explicit quantization factory")
require("StableDeviceId" in adapter and "exactInterfaceId" in adapter,
        "device identity includes the exact interface")
require("requiredDeviceCount" in adapter and "AnalogSnapshotFlag_Truncated" in adapter,
        "adapter makes capacity loss explicit")
require("getSnapshotV2" in backend_header and
        "NativeAnalogBackends_ReadSnapshotV2" in registry_header and
        "getSnapshotV2(output)" in registry,
        "registry exposes an opt-in V2 endpoint")
require("BackendNative_SparkGetSnapshotV2" in spark and
        "SparkRowFresh" in spark and "topologyComplete = false" in spark,
        "Spark exports only fresh row samples and declares partial topology")
require("g_sparkPublicationSequence" in spark_transport and
        "SparkPublicationBegin" in spark_transport and "SparkPublicationEnd" in spark_transport,
        "Spark publication uses a bounded seqlock")
require("native_analog_snapshot_adapter_test.cpp" in runner,
        "unified runner includes the portable adapter oracle")

print("NATIVE_ANALOG_SNAPSHOT_STATIC_AUDIT=PASS")
