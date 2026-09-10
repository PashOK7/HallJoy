#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HALL = ROOT / "HallJoy"

backend = (HALL / "drunkdeer_backend.cpp").read_text(encoding="utf-8")
protocol = (HALL / "drunkdeer_protocol.cpp").read_text(encoding="utf-8")
catalog = (HALL / "native_analog_backends.def").read_text(encoding="utf-8")
project = (HALL / "HallJoy.vcxproj").read_text(encoding="utf-8")
app = (HALL / "app.cpp").read_text(encoding="utf-8")
trace = (HALL / "stability_trace.cpp").read_text(encoding="utf-8")

required_backend = (
    "NativeAnalogStartPhase::BeforeUap",
    "NativeAnalogRouting_Claim",
    "drunkdeer.raw_cell.press",
    "drunkdeer.raw_cell.release",
    "drunkdeer.raw_cell.summary",
    "drunkdeer.report_headers.initial",
    "drunkdeer.rollup",
    'L"digital.press"',
    'L"payload.snapshot"',
    'L"payload.offset_summary"',
    "RequestTrackingFrame",
    "SendTrackingControlUnlocked(true)",
    "SendTrackingControlUnlocked(false)",
    "transport=per_frame_uap_transaction",
    "request_commands_per_frame=1",
    "digital_mapping_dependency=0",
    "g65_antler_nav_v3",
    'L"transport.retry"',
    'L"transport.recovered"',
    'L"transport.reopen_required"',
    "kTransientFailuresBeforeReopen = 12",
    "kStaleNeutralizeMs = 350",
    "MapForProduct(candidate.attributes.ProductID)",
    "HidD_FlushQueue",
    "CancelIoEx",
)
for marker in required_backend:
    assert marker in backend, marker

assert "report[0] = kReportId" in protocol
assert "report[1] = kTrackingRequestCommand" in protocol
assert "report[2] = 0x03" in protocol
assert "enabled ? 0x01 : 0x00" in protocol
assert "report[1] != kTrackingResponseCommand" in protocol
assert "report[4] >= kReportCount" in protocol
assert "seen[chunk]" in protocol
assert "std::copy_n(out->payload.begin(), out->matrix.size()" in protocol
assert "session.Poll" not in backend
assert "BuildMatrixRequest()" not in backend
assert "StartTracking()" not in backend
assert "ReadTrackingFrame" not in backend
assert "transport=temporary_tracking_stream" not in backend
assert "AutomaticMapping" not in backend
assert "mapping.digital" not in backend
assert "mapping.orphan" not in backend
assert "PositionToHid G65AnsiMap()" in protocol
assert "productId == 0x2382 ? G65AnsiMap() : GenericUapMap()" in protocol
assert "halljoy::keycode::kFn" in protocol
assert "halljoy::keycode::kOem1" in protocol

# Physical G65 firmware 0012 proved one-shot semantics: every complete matrix
# frame must be preceded by its own B6 03 01 request on the same open handle.
request_body = backend[backend.index("bool RequestTrackingFrame("):
    backend.index("private:", backend.index("bool RequestTrackingFrame("))]
assert "SendTrackingControlUnlocked(true)" in request_body
run_loop = backend[backend.index("while (!g_stop.load", backend.index("bool Run(")):
    backend.index("const auto endedMs", backend.index("bool Run("))]
assert "++frameRequests" in run_loop
assert "session.RequestTrackingFrame" in run_loop

assert "#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)" in catalog
assert "DrunkDeer_GetNativeBackendDescriptor" in catalog
assert "HallJoyDrunkDeerDiagnostic" in project
assert "HallJoyDrunkDeerDiagnostic)'!='true'" in project
assert "DrunkDeerDiagnostic_RecordRawKeyboardEvent" in app
assert "sz < sizeof(RAWINPUT)" not in app
assert "ContainsTypedPayload" in app
assert "sizeof(RAWKEYBOARD)" in app
assert "HALLJOY_DRUNKDEER_DIAGNOSTIC" in trace
assert "WriteFile(g_traceFile" in trace
assert "FlushFileBuffers(g_traceFile)" in trace
assert "trace.growth_policy" in trace
assert "hard_cap=0" in trace
assert "storage=direct_append" in trace
assert "critical_flush=1" in trace
assert "preallocation=0" in trace
assert "privacy_redaction=userprofile" in trace
assert 'GetEnvironmentVariableW(L"USERPROFILE"' in trace
assert "volume_control=event_aggregation" in trace
assert "--halljoy-test-drunkdeer-log-growth" in (HALL / "main.cpp").read_text(
    encoding="utf-8")

# Digital Raw Input must remain a logger-only side channel: only the analogue
# matrix processing publishes into g_milli.
digital = backend[backend.index("void DrunkDeerDiagnostic_RecordRawKeyboardEvent"):]
assert "g_milli" not in digital
assert "NativeAnalogRouting_Claim" not in digital
assert "BeginAutomaticDigitalMapping" not in digital
assert "EndAutomaticDigitalMapping" not in digital
process_matrix = backend[backend.index("void ProcessMatrix("):
    backend.index("void ObserveHeaders(")]
assert "ObserveAutomaticMapping" not in process_matrix
assert "code < g_milli.size()" in process_matrix

print("drunkdeer diagnostic static audit passed")
