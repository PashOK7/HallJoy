#!/usr/bin/env python3
"""Fail-closed source audit for the firmware-proven Aula WIN60HE backend."""

from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[1]
HALL = PROJECT / "HallJoy"
TESTS = PROJECT / "tests"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="strict")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")
    print(f"PASS: {message}")


def main() -> int:
    protocol_h = read(HALL / "aula_win60he_protocol.h")
    protocol = read(HALL / "aula_win60he_protocol.cpp")
    client_h = read(HALL / "aula_win60he_client.h")
    client = read(HALL / "aula_win60he_client.cpp")
    policy = read(HALL / "aula_win60he_session_policy.cpp")
    backend = read(HALL / "aula_win60he_backend.cpp")
    spark = read(HALL / "backend_sparklink.inc")
    routing_h = read(HALL / "native_analog_routing.h")
    catalog = read(HALL / "native_analog_backends.def")
    project = read(HALL / "HallJoy.vcxproj")
    stability = read(HALL / "stability_trace.cpp")
    diagnostic_metrics_h = read(HALL / "aula_win60he_diagnostic_metrics.h")
    diagnostic_metrics = read(HALL / "aula_win60he_diagnostic_metrics.cpp")
    diagnostic_builder = read(REPO / "tools" / "build_aula_diagnostic.ps1")
    runner = read(REPO / "tools" / "run_native_backend_checks.py")
    simulator_runner = read(REPO / "tools" / "run_analog_simulator.ps1")

    require(all(token in protocol_h for token in (
        "kAulaVendorId = 0x1CA2", "kAulaProductId = 0x1902",
        "kAulaUsagePage = 0xFFA0", "kAulaUsage = 0x0001",
        "kWireReportBytes = 64", "kWindowsHidReportBytes = kWireReportBytes + 1u")),
        "USB identity and the 64/65-byte HID envelope are exact")
    require(all(token in protocol for token in (
        "0x35u + kFrameHead + payloadLength + command",
        "payload[payloadLength - 1u]", "ResponseCommand(requestCommand)")),
        "frame checksum and response-command correlation are implemented")
    require(all(token in protocol_h for token in (
        "kSyncPayloadBytes = 60u", "kExpectedPrecisionUm = 10u",
        "kExpectedMaximumTravelUm = 3400u",
        "kExpectedPhysicalKeyPositions = 61u",
        "kExpectedPublishableDefaultKeys = 60u",
        "kSyncDescriptorBytes = 16u")) and
        "0x41, 0x70, 0x70, 0x20, 0x56, 0x31" in protocol and
        "0x46, 0x65, 0x62, 0x20, 0x20, 0x34" in protocol,
        "firmware, precision and physical-layout proof are pinned")

    require("constexpr std::size_t kCapabilityTransactions = 17u" in client_h and
            "kMaximumCapabilityTransactions" in client_h and
            all(token in client for token in (
                "BuildSyncRequest()", "BuildPrecisionStrokeRequest()",
                "BuildDefaultKeyRequest(row, secondRow)",
                "ReadActiveMap(proof.defaultKeyMap", "ReadTravelMatrix(proof")),
            "exact capability proof keeps seventeen read-only transactions and bounds family proof")
    require("kKeyFunctionRecordsPerFrame = 14u" in protocol_h and
            "static_assert(kExactKeyFunctionBatchCount == 5u)" in protocol_h and
            "static_assert(kMaximumKeyFunctionBatchCount == 9u)" in protocol_h and
            "begin < factoryKeyCount" in client and
            "ReadActiveMapGeneration(defaultKeyMap, &first" in client and
            "ReadActiveMapGeneration(defaultKeyMap, &second" in client and
            "first.functions != second.functions" in client and
            "query[index] = paddingKey" in client and
            "DecodeKeyFunctionReadResponse" in client,
            "Fn0 map uses dynamically bounded correlated batches and two identical generations")
    require(all(token in protocol_h + protocol + client_h + client + backend for token in (
                "CompatibilityProfile::ExactWin60HeMax",
                "CompatibilityProfile::Compatible6x21Family",
                "IsAula6x21FamilyFirmware", "IsAula6x21FamilyPrecision",
                "IsAula6x21FamilyDefaultMap", "exactFirmware && exactPrecision",
                "ContainsFamilyToken", "sparkplayjoy")) and
            "candidate.attributes.VendorID == aula_win60he::kAulaVendorId" in backend,
            "family discovery is brand-scoped and requires structural firmware, precision and dynamic-map proof")
    require("std::uint16_t value" in protocol_h and
            "IsPublishableKeyFunction(std::uint16_t function)" in protocol_h and
            "function <= 0x00FFu" in protocol and
            "IsPublishableKeyboardUsage(static_cast<std::uint8_t>(function))" in protocol and
            "hidUsage <= 0xE7u" in protocol,
            "16-bit functions are filtered before publishing HID usages")
    require("BuildTravelRequest(std::uint8_t half)" in protocol and
            "kSelectorTravel, half, 0xFF, 0xFF" in protocol and
            "for (std::uint8_t half = 1; half <= 2; ++half)" in client and
            "kTravelPayloadBytes =" in protocol_h and
            "static_assert(kTravelPayloadBytes == 128u)" in protocol_h,
            "travel halves are sequential selector-02, 128-byte responses")

    require("if (!transport_.FlushInput())" in client and
            "poisoned_ = true" in client and
            "if (poisoned_)" in client and
            "HidD_FlushQueue(session_.handle.value)" in backend,
            "every transaction flushes the exclusive queue and poisons uncertain sessions")
    require("GENERIC_READ | GENERIC_WRITE" in backend and
            "0, // protocol has no transaction ID" in backend and
            "QueryCurrentIdentityForPath" in backend and
            "ReadMetadata(session.handle.value" in backend and
            "kRequestedInputBuffers = 64" in backend and
            "kMinimumInputBuffers" in backend,
            "exclusive open is re-correlated and sized for complete responses")

    require(all(token in client_h + client for token in (
        "HALLJOY_AULA_AGGRESSIVE_TRACE", "compatibilityMismatchMask",
        "CapabilityMismatch_Firmware", "CapabilityMismatch_Precision",
        "CapabilityMismatch_DefaultMap", "CapabilityMismatch_ActiveMapStability",
        "CapabilityMismatch_TravelPlausibility")),
        "diagnostic probe records semantic mismatches and continues to later read-only stages")
    require("kSyncDescriptorBytes = 16u" in protocol_h and
            "block[1] == kSyncPayloadBytes" in client and
            "frame.payloadBytes != kSyncPayloadBytes" in protocol and
            "sync.rawPayload[kSyncTrailerOffset] == 0xFFu" in protocol and
            "kResponseSync" in read(TESTS / "aula_win60he_oracle_fixtures.h"),
        "production parser pins the physically observed 60-byte sync descriptors")
    require("probeOk && capability.compatibilityMismatchMask == 0" in backend and
            "if (capability.compatibilityMismatchMask != 0)" in backend and
            "claim_blocked=1 publication_blocked=1" in backend and
            "client.CompatibilityMismatchMask() != 0" in backend and
            "runtime.semantic_mismatch" in backend,
        "relaxed diagnostic proof can never claim or publish an incompatible device")
    require(all(token in backend for token in (
        "diagnostic.enabled", "enumeration.candidate", "session.open_failed",
        "proof.outcome", "routing.not_claimed", "protocol.report",
        "serial_redacted", "matrix.first", "matrix.activity", "matrix.health",
        "matrix.session_summary", "matrix.coverage", "reconnect.success",
        "protocol.cancelled")) and
        all(token in diagnostic_metrics_h + diagnostic_metrics for token in (
            "kDiagnosticHealthWindowUs = 5'000'000ull", "RateMilliHz",
            "transactionBuckets", "activeBuckets", "firstTenPlus",
            "MaximumByHid", "releaseToZeroTransitions")),
        "aggressive trace measures polling rate, latency, multi-key activity, releases, coverage and reconnect")
    require("candidate.skip_dedicated_summary" in spark and
            "interval_ms=60000" in spark and
            'L"candidate.skip_dedicated"' not in spark,
            "Spark dedicated-family diagnostics are aggregated instead of flooding the log")
    require("if (g_stop.load(std::memory_order_acquire))" in backend and
            "reason=shutdown" in backend and "protocol.cancelled" in backend,
            "shutdown cancellation is distinguished from a runtime protocol fault")
    disconnect_clear = backend.find("ClearPublishedValues(false);", backend.find("matrix.session_summary"))
    disconnect_event = backend.find('L"disconnected"', disconnect_clear)
    require(disconnect_clear >= 0 and disconnect_event > disconnect_clear and
            "active_before_clear" in backend[disconnect_event:disconnect_event + 700] and
            "published_active_after_clear" in backend[disconnect_event:disconnect_event + 700],
            "disconnect evidence is emitted after authoritative values are neutralized")
    require("Production image contains isolated Aula diagnostic telemetry" in read(REPO / "tools" / "build.ps1") and
            all(token in read(REPO / "tools" / "build.ps1") for token in (
                "'matrix.health'", "'matrix.activity'", "'matrix.session_summary'",
                "'matrix.coverage'", "'ten_key_gate=1'")),
            "official production packaging fails if high-detail Aula telemetry leaks into the image")
    require("HallJoyAulaAggressiveTrace" in project and
            "HALLJOY_AULA_AGGRESSIVE_TRACE;HALLJOY_STABILITY_TRACE" in project,
        "the isolated diagnostic build flag enables aggressive and stability traces")
    require("BuildPathNearExe(L\"HallJoy.log\"" in stability and
            "64u * 1024u * 1024u" in stability and
            "HALLJOY_AULA_AGGRESSIVE_TRACE" in stability,
        "the aggressive EXE owns one large HallJoy.log beside itself")
    require(all(token not in diagnostic_builder for token in (
        "collect_aula_diagnostic_trace", "COLLECT_AULA_DIAGNOSTIC_TRACE",
        "analyze_stability_trace.py", "HallJoy.portable", "Compress-Archive")),
        "the diagnostic delivery contains only HallJoy.exe")
    require(all(token in diagnostic_builder for token in (
        "'matrix.health'", "'matrix.activity'", "'matrix.session_summary'",
        "'matrix.coverage'", "'ten_key_gate=1'", "'reconnect.success'",
        "'protocol.cancelled'", "'[aula.w669.raw]'", "'[aula.w669.telemetry]'",
        "'[mad68pr]'")) and
        "/p:HallJoyDiagnostic=true" in diagnostic_builder and
        "/p:HallJoySingleLogDiagnostic=true" in diagnostic_builder,
        "diagnostic packaging fails if the v2 telemetry schema is not linked")

    enumeration = backend.find("EnumerationResult EnumerateCandidates()")
    foreign_gate = backend.find("NativeAnalogRouting_IsClaimed(", enumeration)
    metadata_open = backend.find("ScopedHandle metadata(CreateFileW", enumeration)
    require(enumeration >= 0 and enumeration < foreign_gate < metadata_open,
            "foreign exact-path claims are rejected before metadata open")
    proof = backend.find("client.Probe(", backend.find("AulaWin60He_PrepareProtocolRouting"))
    claim = backend.find("NativeAnalogRouting_Claim(", proof)
    require(proof >= 0 and claim > proof and
            "session.candidate.path.c_str()," in backend[claim:claim + 300] and
            "NativeAnalogProtocol::AulaWin60He" in backend[claim:claim + 300] and
            "ReserveExact" not in backend,
            "only a fully proven exact interface path is claimed; no VID/PID reservation exists")
    require("AulaWin60He = 6" in routing_h and
            catalog.count("AulaWin60He_GetNativeBackendDescriptor") == 1 and
            "NativeAnalogStartPhase::BeforeUap" in backend and
            "NativeAnalogBackendFlag_ReadOnlyProbe" in backend,
            "Aula is independently catalogued as a pre-UAP read-only backend")
    require("SparkPathIsDedicatedAula(detail->DevicePath)" in spark and
            spark.index("SparkPathIsDedicatedAula(detail->DevicePath)") <
            spark.index("HANDLE h = CreateFileW", spark.index("SparkTryOpenDevice")),
            "Spark rejects the dedicated Aula VID/PID before opening or probing it")

    require(all(token in backend for token in (
        "_beginthreadex", "RunWorkerEntryBarrier", "CancelIoEx",
        "kStopJoinTimeoutMs = 3000", "ObserveWorkerJoin",
        "resources_retained=1")) and "TerminateThread" not in backend,
        "worker stop is bounded, cancellation-aware and generation-safe")
    require("--halljoy-test-aula-stop-timeout" in backend and
            "InjectAulaStopTimeout" in simulator_runner and
            "Aula-timeout simulator exited" in simulator_runner and
            "[component=aula-win60he][event=stop.incomplete]" in simulator_runner,
            "permanent Aula worker shutdown is process-contained and trace-verified")
    require("RealtimeLoop_NotifyInputChanged" in backend and
            "ClearPublishedValues(false)" in backend and
            "g_connected.load" in backend,
            "publication wakes realtime and disconnect clears authoritative values")
    require("WaitForReconnect(g_candidatePresent.load(std::memory_order_acquire)" in backend and
            "? kReconnectWaitMs" in backend and ": INFINITE" in backend and
            "WM_DEVICECHANGE notification" in backend,
            "absent MAX discovery is event-driven and cannot poll-stall unrelated HID streams")
    require("PlanDeviceSelection" in policy and
            "paths.size() == 1u" in policy and
            "plan.ambiguous = true" in policy and
            "MatchesRetainedDeviceIdentity" in policy,
            "device selection fails closed on ambiguity and retains identity evidence")

    for name in (
        "aula_win60he_protocol.cpp", "aula_win60he_client.cpp",
        "aula_win60he_diagnostic_metrics.cpp",
        "aula_win60he_session_policy.cpp", "aula_win60he_backend.cpp",
        "aula_win60he_protocol.h", "aula_win60he_client.h",
        "aula_win60he_diagnostic_metrics.h",
        "aula_win60he_session_policy.h", "aula_win60he_backend.h"):
        require(name in project, f"MSVC project contains {name}")
    for name in (
        "aula_win60he_protocol_test.cpp", "aula_win60he_oracle_test.cpp",
        "aula_win60he_end_to_end_test.cpp", "aula_win60he_session_policy_test.cpp"):
        require(name in runner or (name == "aula_win60he_protocol_test.cpp" and
                "*_protocol_test.cpp" in runner), f"portable runner covers {name}")
        require((TESTS / name).is_file(), f"regression asset exists: {name}")
    require("aula_win60he_diagnostic_metrics_test.cpp" in runner and
            (TESTS / "aula_win60he_diagnostic_metrics_test.cpp").is_file(),
            "portable runner covers diagnostic metrics and ten-key telemetry")

    mutating_markers = ("SetKey", "WriteConfig", "FirmwareUpdate", "BuildCalibration")
    require(not any(marker in protocol_h + protocol + client_h + client for marker in mutating_markers),
            "production Aula protocol exposes no mutating builder")

    print("AULA_WIN60HE_BACKEND_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
