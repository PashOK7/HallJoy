from pathlib import Path


ROOT = Path(__file__).resolve().parent
HALL = ROOT.parent / "HallJoy"
HEADER = (HALL / "provider_v2_snapshot_broker.h").read_text(encoding="utf-8-sig")
SOURCE = (HALL / "provider_v2_snapshot_broker.cpp").read_text(encoding="utf-8-sig")
TEST = (ROOT / "provider_v2_snapshot_broker_test.cpp").read_text(encoding="utf-8-sig")
PROJECT = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8-sig")
RUNNER = (ROOT.parents[2] / "tools" / "run_native_backend_checks.py").read_text(
    encoding="utf-8-sig"
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def body_after(signature: str, next_signature: str) -> str:
    require(signature in SOURCE, f"broker defines {signature}")
    body = SOURCE.split(signature, 1)[1]
    if next_signature:
        require(next_signature in body, f"broker defines {next_signature}")
        body = body.split(next_signature, 1)[0]
    return body


acquire = body_after(
    "ParentSnapshotBroker::ReadLease ParentSnapshotBroker::Acquire() noexcept",
    "void ParentSnapshotBroker::ReleaseReader",
)
for forbidden in (
    "Sleep(", "WaitFor", "AcquireSRWLock", "std::mutex", "lock_guard",
    ".resize(", ".reserve(", "new ", "make_unique", "make_shared",
):
    require(forbidden not in acquire, f"realtime acquire excludes {forbidden}")

require("kBrokerSlotCount = 3" in HEADER, "broker owns three immutable handoff slots")
require("BeginReconfigure" in HEADER and "IsDrained" in HEADER,
        "broker exposes explicit revoke and drain lifecycle")
require("std::array<std::vector" in SOURCE and "swap(newDevices" in SOURCE,
        "prepare allocates complete replacement storage before commit")
require("i != current" in SOURCE and "readers.load" in SOURCE,
        "writer excludes both the published and reader-owned slots")
require("NoFreeSlot" in TEST and "!broker.IsDrained()" in TEST,
        "regression covers nonblocking drop and reader-held resize exclusion")
require("negotiated_devices=12" in TEST and "negotiated_samples=60" in TEST,
        "regression crosses the old fixed device ceiling with exact dynamic storage")
require("provider_v2_snapshot_broker.cpp" in PROJECT,
        "production project compiles the broker")
require('("provider_v2_snapshot_broker", [' in RUNNER,
        "unified native runner executes the broker regression")

print("PROVIDER_V2_SNAPSHOT_BROKER_STATIC_AUDIT=PASS")
