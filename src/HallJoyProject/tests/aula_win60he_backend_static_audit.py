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
    routing_h = read(HALL / "native_analog_routing.h")
    catalog = read(HALL / "native_analog_backends.def")
    project = read(HALL / "HallJoy.vcxproj")
    runner = read(REPO / "tools" / "run_native_backend_checks.py")

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
        "kSyncPayloadBytes = 54u", "kExpectedPrecisionUm = 10u",
        "kExpectedMaximumTravelUm = 3400u",
        "kExpectedPhysicalKeyPositions = 61u",
        "kExpectedPublishableDefaultKeys = 60u")) and
        '"App V1.1.6"' in protocol and '"Feb  4 2026"' in protocol,
        "firmware, precision and physical-layout proof are pinned")

    require("constexpr std::size_t kCapabilityTransactions = 17u" in client_h and
            all(token in client for token in (
                "BuildSyncRequest()", "BuildPrecisionStrokeRequest()",
                "BuildDefaultKeyRequest(row, secondRow)",
                "ReadActiveMap(proof.defaultKeyMap", "ReadTravelMatrix(proof")),
            "capability proof performs all seventeen read-only transactions")
    require("kKeyFunctionRecordsPerFrame = 14u" in protocol_h and
            "static_assert(kKeyFunctionBatchCount == 5u)" in protocol_h and
            "ReadActiveMapGeneration(defaultKeyMap, &first" in client and
            "ReadActiveMapGeneration(defaultKeyMap, &second" in client and
            "first.functions != second.functions" in client and
            "query[index] = paddingKey" in client and
            "DecodeKeyFunctionReadResponse" in client,
            "Fn0 map uses five correlated batches and two identical generations")
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

    enumeration = backend.find("EnumerationResult EnumerateCandidates()")
    foreign_gate = backend.find("NativeAnalogRouting_IsClaimed(", enumeration)
    metadata_open = backend.find("ScopedHandle metadata(CreateFileW", enumeration)
    require(enumeration >= 0 and enumeration < foreign_gate < metadata_open,
            "foreign exact-path claims are rejected before metadata open")
    proof = backend.find("client.Probe(&capability", backend.find("AulaWin60He_PrepareProtocolRouting"))
    claim = backend.find("NativeAnalogRouting_Claim(", proof)
    require(proof >= 0 and claim > proof and
            "session.candidate.path.c_str()," in backend[claim:claim + 300] and
            "NativeAnalogProtocol::AulaWin60He" in backend[claim:claim + 300] and
            "ReserveExact" not in backend,
            "only a fully proven exact interface is claimed; no VID/PID reservation exists")
    require("AulaWin60He = 6" in routing_h and
            catalog.count("AulaWin60He_GetNativeBackendDescriptor") == 1 and
            "NativeAnalogStartPhase::BeforeUap" in backend and
            "NativeAnalogBackendFlag_ReadOnlyProbe" in backend,
            "Aula is independently catalogued as a pre-UAP read-only backend")

    require(all(token in backend for token in (
        "_beginthreadex", "RunWorkerEntryBarrier", "CancelIoEx",
        "kStopJoinTimeoutMs = 3000", "ObserveWorkerJoin",
        "resources_retained=1")) and "TerminateThread" not in backend,
        "worker stop is bounded, cancellation-aware and generation-safe")
    require("RealtimeLoop_NotifyInputChanged" in backend and
            "ClearPublishedValues(false)" in backend and
            "g_connected.load" in backend,
            "publication wakes realtime and disconnect clears authoritative values")
    require("PlanDeviceSelection" in policy and
            "paths.size() == 1u" in policy and
            "plan.ambiguous = true" in policy and
            "MatchesRetainedDeviceIdentity" in policy,
            "device selection fails closed on ambiguity and retains identity evidence")

    for name in (
        "aula_win60he_protocol.cpp", "aula_win60he_client.cpp",
        "aula_win60he_session_policy.cpp", "aula_win60he_backend.cpp",
        "aula_win60he_protocol.h", "aula_win60he_client.h",
        "aula_win60he_session_policy.h", "aula_win60he_backend.h"):
        require(name in project, f"MSVC project contains {name}")
    for name in (
        "aula_win60he_protocol_test.cpp", "aula_win60he_oracle_test.cpp",
        "aula_win60he_end_to_end_test.cpp", "aula_win60he_session_policy_test.cpp"):
        require(name in runner or (name == "aula_win60he_protocol_test.cpp" and
                "*_protocol_test.cpp" in runner), f"portable runner covers {name}")
        require((TESTS / name).is_file(), f"regression asset exists: {name}")

    mutating_markers = ("SetKey", "WriteConfig", "FirmwareUpdate", "BuildCalibration")
    require(not any(marker in protocol_h + protocol + client_h + client for marker in mutating_markers),
            "production Aula protocol exposes no mutating builder")

    print("AULA_WIN60HE_BACKEND_STATIC_AUDIT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
