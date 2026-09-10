#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include "aula_win60he_backend.h"
#include "generated/layout_pipeline/identities.h"

#include "analog_key_codes.h"
#include "debug_log.h"
#include "hid_io_operation.h"
#include "native_analog_routing.h"
#include "realtime_loop.h"
#include "stability_trace.h"
#include "worker_exception_barrier.h"
#include "worker_join_policy.h"
#include "aula_win60he_client.h"
#include "aula_win60he_protocol.h"
#include "aula_win60he_session_policy.h"
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
#include "aula_win60he_diagnostic_metrics.h"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <mutex>
#include <process.h>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace
{
constexpr DWORD kWriteTimeoutMs = 1000;
constexpr DWORD kReconnectWaitMs = 1000;
constexpr DWORD kInitialAttemptWaitMarginMs = 3000;
constexpr DWORD kInitialAttemptWaitMs =
    static_cast<DWORD>(aula_win60he::kMaximumCapabilityTransactions *
        (kWriteTimeoutMs + aula_win60he::kMatrixResponseTimeoutMs) +
        kInitialAttemptWaitMarginMs);
constexpr DWORD kPollPauseMs = 1;
constexpr DWORD kStopJoinTimeoutMs = 3000;
constexpr ULONGLONG kActiveMapRefreshIntervalMs = 2000;
constexpr ULONG kRequestedInputBuffers = 64;
constexpr ULONG kMinimumInputBuffers =
    static_cast<ULONG>(aula_win60he::kMaximumResponseReports);

struct ScopedHandle
{
    HANDLE value = INVALID_HANDLE_VALUE;

    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) : value(handle) {}
    ~ScopedHandle()
    {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : value(other.value)
    {
        other.value = INVALID_HANDLE_VALUE;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept
    {
        if (this != &other)
        {
            if (value && value != INVALID_HANDLE_VALUE)
                CloseHandle(value);
            value = other.value;
            other.value = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    explicit operator bool() const
    {
        return value && value != INVALID_HANDLE_VALUE;
    }
};

struct Candidate
{
    std::wstring path;
    std::wstring instanceId;
    std::wstring manufacturer;
    std::wstring product;
    HIDD_ATTRIBUTES attributes{};
    HIDP_CAPS caps{};
};

struct EnumerationResult
{
    std::uint32_t attempt = 0;
    std::uint32_t hidInterfaces = 0;
    std::uint32_t knownIdentityPaths = 0;
    std::uint32_t familyVendorPaths = 0;
    std::uint32_t gravaStarBrandPaths = 0;
    std::uint32_t interfaceEnumerationErrors = 0;
    std::uint32_t interfaceDetailErrors = 0;
    std::uint32_t identityPrefilterRejected = 0;
    std::uint32_t metadataRejected = 0;
    bool knownIdentityPathSeen = false;
    std::vector<Candidate> candidates;
};

struct RetainedDeviceIdentity
{
    std::wstring path;
    std::wstring instanceId;
    std::array<char, 17> firmwareSerial{};
    bool valid = false;
    bool hasFirmwareSerialEvidence = false;
};

struct ProbeResult
{
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint16_t usagePage = 0;
    std::uint16_t usage = 0;
    std::uint16_t inputReportBytes = 0;
    std::uint16_t outputReportBytes = 0;
    aula_win60he::CapabilityProof capability{};
};

struct ClaimState
{
    bool validated = false;
    RetainedDeviceIdentity identity{};
    ProbeResult proof{};
};

struct Session
{
    ScopedHandle handle;
    ScopedHandle readEvent;
    ScopedHandle writeEvent;
    Candidate candidate{};

    explicit operator bool() const
    {
        return handle && readEvent && writeEvent;
    }
};

std::atomic<bool> g_routingPrepared{ false };
std::mutex g_claimMutex;
ClaimState g_claim{};

std::atomic<bool> g_running{ false };
std::atomic<bool> g_stop{ false };
std::atomic<bool> g_deviceChanged{ false };
std::atomic<bool> g_candidatePresent{ false };
std::atomic<bool> g_protocolPresent{ false };
std::atomic<bool> g_connected{ false };
std::atomic<std::uint64_t> g_verifiedLayoutToken{0};
std::atomic<bool> g_ambiguousSelection{ false };
std::atomic<bool> g_invalidEnumeration{ false };
std::atomic<bool> g_retainedIdentityMissing{ false };
std::atomic<bool> g_firmwareSerialMismatch{ false };
std::atomic<std::uint32_t> g_candidateCount{ 0 };
std::atomic<std::uint32_t> g_lastOpenError{ ERROR_SUCCESS };
std::atomic<std::uint32_t> g_discoveryAttempt{ 0 };
std::mutex g_serviceMutex;
std::mutex g_signalMutex;
std::mutex g_activeSessionMutex;
HANDLE g_threadHandle = nullptr;
HANDLE g_wakeEvent = nullptr;
HANDLE g_initialAttemptEvent = nullptr;
HANDLE g_activeSessionHandle = INVALID_HANDLE_VALUE;
std::atomic<halljoy::worker::WorkerExceptionKind> g_workerFaultKind{
    halljoy::worker::WorkerExceptionKind::None };
halljoy::worker::WorkerExceptionRecord g_workerFaultRecord{};

bool InjectStopTimeout() noexcept
{
#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    return commandLine &&
        wcsstr(commandLine, L"--halljoy-test-aula-stop-timeout");
#else
    return false;
#endif
}

std::array<std::atomic<std::uint16_t>, halljoy::keycode::kCount> g_milli{};
std::array<std::atomic<std::uint8_t>, halljoy::keycode::kCount> g_owned{};
std::atomic<std::uint16_t> g_vendorId{ 0 };
std::atomic<std::uint16_t> g_productId{ 0 };
std::atomic<std::uint16_t> g_usagePage{ 0 };
std::atomic<std::uint16_t> g_maximumTravelUm{ 0 };
std::atomic<std::uint16_t> g_precisionUm{ 0 };
std::atomic<std::uint32_t> g_mappedKeys{ 0 };
std::atomic<std::uint32_t> g_activeKeys{ 0 };
std::atomic<std::uint32_t> g_inputReportBytes{ 0 };
std::atomic<std::uint32_t> g_outputReportBytes{ 0 };
std::atomic<std::uint64_t> g_successfulUpdates{ 0 };
std::atomic<std::uint64_t> g_failedUpdates{ 0 };
std::atomic<std::uint64_t> g_lastMatrixUs{ 0 };
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
std::atomic<std::uint64_t> g_diagnosticSessionSequence{ 0 };

enum class DiagnosticProgress : std::uint32_t
{
    None = 0,
    EnumerationStarted,
    KnownIdentitySeen,
    FingerprintCandidate,
    SessionOpened,
    CapabilityProved,
    RouteClaimed,
    MatrixObserved,
};

enum class DiagnosticFailure : std::uint32_t
{
    None = 0,
    SetupApiEnumeration,
    MetadataFingerprint,
    InvalidEnumeration,
    AmbiguousSelection,
    RetainedIdentityMissing,
    SessionOpen,
    CapabilityProof,
    SemanticMismatch,
    RetainedProofMismatch,
    ProbeResult,
    RouteClaim,
    ActiveMap,
    Travel,
    WorkerFault,
    StartResources,
    StartThread,
    StopIncomplete,
};

std::atomic<DiagnosticProgress> g_diagnosticProgress{ DiagnosticProgress::None };
std::atomic<DiagnosticFailure> g_diagnosticFailure{ DiagnosticFailure::None };
std::atomic<std::uint32_t> g_diagnosticFamilyVendorPaths{ 0 };
std::atomic<std::uint32_t> g_diagnosticGravaStarBrandPaths{ 0 };
std::atomic<std::uint16_t> g_diagnosticLastSeenVendorId{ 0 };
std::atomic<std::uint16_t> g_diagnosticLastSeenProductId{ 0 };
std::atomic<std::uint32_t> g_diagnosticLastFailureStage{ 0 };
std::atomic<std::uint32_t> g_diagnosticLastFailureCommand{ 0 };
std::atomic<std::uint32_t> g_diagnosticLastFailureSelector{ 0 };
std::atomic<std::uint32_t> g_diagnosticLastFailureIndex{ 0 };
std::atomic<std::uint32_t> g_diagnosticLastMismatchMask{ 0 };
std::atomic<std::uint32_t> g_diagnosticLastBoardId{ 0 };
std::atomic<bool> g_diagnosticVerdictEmitted{ false };
#endif

#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
void AdvanceDiagnosticProgress(DiagnosticProgress progress) noexcept
{
    auto current = g_diagnosticProgress.load(std::memory_order_relaxed);
    while (static_cast<std::uint32_t>(current) <
            static_cast<std::uint32_t>(progress) &&
        !g_diagnosticProgress.compare_exchange_weak(
            current, progress, std::memory_order_relaxed))
    {
    }
}

void RecordDiagnosticFailure(DiagnosticFailure failure) noexcept
{
    g_diagnosticFailure.store(failure, std::memory_order_relaxed);
}

void RecordDiagnosticProtocolFailure(
    const aula_win60he::Failure& failure) noexcept
{
    g_diagnosticLastFailureStage.store(
        static_cast<std::uint32_t>(failure.stage), std::memory_order_relaxed);
    g_diagnosticLastFailureCommand.store(failure.command, std::memory_order_relaxed);
    g_diagnosticLastFailureSelector.store(failure.selector, std::memory_order_relaxed);
    g_diagnosticLastFailureIndex.store(failure.index, std::memory_order_relaxed);
}

const wchar_t* DiagnosticProgressName(DiagnosticProgress progress) noexcept
{
    switch (progress)
    {
    case DiagnosticProgress::None: return L"none";
    case DiagnosticProgress::EnumerationStarted: return L"enumeration";
    case DiagnosticProgress::KnownIdentitySeen: return L"known_identity";
    case DiagnosticProgress::FingerprintCandidate: return L"fingerprint";
    case DiagnosticProgress::SessionOpened: return L"session_open";
    case DiagnosticProgress::CapabilityProved: return L"capability_proof";
    case DiagnosticProgress::RouteClaimed: return L"route_claim";
    case DiagnosticProgress::MatrixObserved: return L"matrix";
    }
    return L"unknown";
}

const wchar_t* DiagnosticFailureName(DiagnosticFailure failure) noexcept
{
    switch (failure)
    {
    case DiagnosticFailure::None: return L"none";
    case DiagnosticFailure::SetupApiEnumeration: return L"setupapi_enumeration";
    case DiagnosticFailure::MetadataFingerprint: return L"metadata_fingerprint";
    case DiagnosticFailure::InvalidEnumeration: return L"invalid_enumeration";
    case DiagnosticFailure::AmbiguousSelection: return L"ambiguous_selection";
    case DiagnosticFailure::RetainedIdentityMissing: return L"retained_identity_missing";
    case DiagnosticFailure::SessionOpen: return L"session_open";
    case DiagnosticFailure::CapabilityProof: return L"capability_proof";
    case DiagnosticFailure::SemanticMismatch: return L"semantic_mismatch";
    case DiagnosticFailure::RetainedProofMismatch: return L"retained_proof_mismatch";
    case DiagnosticFailure::ProbeResult: return L"probe_result";
    case DiagnosticFailure::RouteClaim: return L"route_claim";
    case DiagnosticFailure::ActiveMap: return L"active_map";
    case DiagnosticFailure::Travel: return L"travel";
    case DiagnosticFailure::WorkerFault: return L"worker_fault";
    case DiagnosticFailure::StartResources: return L"start_resources";
    case DiagnosticFailure::StartThread: return L"start_thread";
    case DiagnosticFailure::StopIncomplete: return L"stop_incomplete";
    }
    return L"unknown";
}

void ResetDiagnosticOutcome() noexcept
{
    g_diagnosticProgress.store(DiagnosticProgress::None, std::memory_order_relaxed);
    g_diagnosticFailure.store(DiagnosticFailure::None, std::memory_order_relaxed);
    g_diagnosticFamilyVendorPaths.store(0, std::memory_order_relaxed);
    g_diagnosticGravaStarBrandPaths.store(0, std::memory_order_relaxed);
    g_diagnosticLastSeenVendorId.store(0, std::memory_order_relaxed);
    g_diagnosticLastSeenProductId.store(0, std::memory_order_relaxed);
    g_diagnosticLastFailureStage.store(0, std::memory_order_relaxed);
    g_diagnosticLastFailureCommand.store(0, std::memory_order_relaxed);
    g_diagnosticLastFailureSelector.store(0, std::memory_order_relaxed);
    g_diagnosticLastFailureIndex.store(0, std::memory_order_relaxed);
    g_diagnosticLastMismatchMask.store(0, std::memory_order_relaxed);
    g_diagnosticLastBoardId.store(0, std::memory_order_relaxed);
    g_diagnosticVerdictEmitted.store(false, std::memory_order_relaxed);
}

void TraceDiagnosticVerdict() noexcept
{
    bool expected = false;
    if (!g_diagnosticVerdictEmitted.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
        return;

    const auto progress = g_diagnosticProgress.load(std::memory_order_relaxed);
    const auto failure = g_diagnosticFailure.load(std::memory_order_relaxed);
    const std::uint64_t matrices =
        g_successfulUpdates.load(std::memory_order_relaxed);
    const wchar_t* result = L"no_matching_interface";
    const wchar_t* action = L"send_log";
    if (matrices != 0)
    {
        result = L"analog_stream";
        action = L"analog_and_log";
    }
    else if (progress == DiagnosticProgress::RouteClaimed)
    {
        result = L"claimed_without_matrix";
    }
    else if (failure != DiagnosticFailure::None)
    {
        result = L"diagnostic_failure";
    }
    else if (g_diagnosticFamilyVendorPaths.load(std::memory_order_relaxed) != 0 ||
        g_diagnosticGravaStarBrandPaths.load(std::memory_order_relaxed) != 0)
    {
        result = L"identity_not_admitted";
    }

    StabilityTrace_Write(L"INFO", L"aula-win60he", L"diagnostic.verdict",
        L"conclusive=1 result=%ls action=%ls progress=%ls progress_code=%u failure=%ls failure_code=%u discovery_attempts=%u candidates=%u family_vendor_paths=%u gravastar_brand_paths=%u last_seen_vid=%04X last_seen_pid=%04X last_open_error=%u protocol_stage=%u command=%02X selector=%02X index=%u mismatch_mask=%08X board_id=%08X matrices=%llu failed_updates=%llu worker_fault=%u",
        result, action, DiagnosticProgressName(progress),
        static_cast<unsigned>(progress), DiagnosticFailureName(failure),
        static_cast<unsigned>(failure),
        g_discoveryAttempt.load(std::memory_order_relaxed),
        g_candidateCount.load(std::memory_order_relaxed),
        g_diagnosticFamilyVendorPaths.load(std::memory_order_relaxed),
        g_diagnosticGravaStarBrandPaths.load(std::memory_order_relaxed),
        static_cast<unsigned>(g_diagnosticLastSeenVendorId.load(std::memory_order_relaxed)),
        static_cast<unsigned>(g_diagnosticLastSeenProductId.load(std::memory_order_relaxed)),
        g_lastOpenError.load(std::memory_order_relaxed),
        g_diagnosticLastFailureStage.load(std::memory_order_relaxed),
        g_diagnosticLastFailureCommand.load(std::memory_order_relaxed),
        g_diagnosticLastFailureSelector.load(std::memory_order_relaxed),
        g_diagnosticLastFailureIndex.load(std::memory_order_relaxed),
        g_diagnosticLastMismatchMask.load(std::memory_order_relaxed),
        g_diagnosticLastBoardId.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(matrices),
        static_cast<unsigned long long>(g_failedUpdates.load(std::memory_order_relaxed)),
        static_cast<unsigned>(g_workerFaultKind.load(std::memory_order_relaxed)));
}
#endif

std::uint64_t HashWideIdentity(const std::wstring& value) noexcept
{
    std::uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value)
    {
        std::uint32_t v = static_cast<std::uint32_t>(std::towlower(ch));
        for (unsigned byte = 0; byte < sizeof(v); ++byte)
        {
            hash ^= static_cast<std::uint8_t>(v >> (byte * 8u));
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

std::uint64_t HashAsciiIdentity(const char* value) noexcept
{
    std::uint64_t hash = 1469598103934665603ull;
    if (!value) return hash;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p)
    {
        hash ^= *p;
        hash *= 1099511628211ull;
    }
    return hash;
}

void WidenAscii(const char* source, wchar_t* destination, std::size_t count) noexcept
{
    if (!destination || count == 0) return;
    destination[0] = L'\0';
    if (!source) return;
    std::size_t i = 0;
    for (; source[i] && i + 1u < count; ++i)
        destination[i] = static_cast<unsigned char>(source[i]);
    destination[i] = L'\0';
}

void TraceCapabilityOutcome(
    const wchar_t* phase,
    bool probeOk,
    const aula_win60he::CapabilityProof& capability,
    const aula_win60he::Failure& failure) noexcept
{
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    wchar_t version[32]{};
    wchar_t buildLabel[32]{};
    wchar_t failureName[48]{};
    WidenAscii(capability.sync.appVersion.data(), version, _countof(version));
    WidenAscii(capability.sync.buildLabel.data(), buildLabel, _countof(buildLabel));
    WidenAscii(aula_win60he::FailureStageName(failure.stage), failureName, _countof(failureName));
    StabilityTrace_Write(probeOk ? L"INFO" : L"WARN",
        L"aula-win60he", L"proof.outcome",
        L"phase=%ls ok=%u failure_stage=%u failure_name=%ls command=%02X selector=%02X index=%u mismatch_mask=%08X board_id=%08X app_version=%ls build_label=%ls serial_hash=%016llX precision_um=%u min_um=%u max_um=%u physical=%u default_mapped=%u active_mapped=%u",
        phase ? phase : L"unknown", static_cast<unsigned>(probeOk),
        static_cast<unsigned>(failure.stage), failureName,
        static_cast<unsigned>(failure.command),
        static_cast<unsigned>(failure.selector),
        static_cast<unsigned>(failure.index),
        static_cast<unsigned>(capability.compatibilityMismatchMask),
        static_cast<unsigned>(capability.sync.boardId),
        version, buildLabel,
        static_cast<unsigned long long>(HashAsciiIdentity(capability.sync.serial.data())),
        static_cast<unsigned>(capability.precision.precisionUm),
        static_cast<unsigned>(capability.precision.minimumTravelUm),
        static_cast<unsigned>(capability.precision.maximumTravelUm),
        static_cast<unsigned>(capability.physicalKeyPositions),
        static_cast<unsigned>(capability.defaultMappedKeys),
        static_cast<unsigned>(capability.mappedKeys));
#else
    (void)phase; (void)probeOk; (void)capability; (void)failure;
#endif
}
std::atomic<std::uint32_t> g_averageIntervalUs{ 0 };
std::atomic<std::uint32_t> g_maximumIntervalUs{ 0 };
std::atomic<ULONGLONG> g_lastUpdateMs{ 0 };

class ActiveSessionRegistration final
{
public:
    explicit ActiveSessionRegistration(Session& session) noexcept
        : handle_(session.handle.value)
    {
        std::lock_guard<std::mutex> lock(g_activeSessionMutex);
        if (!g_stop.load(std::memory_order_acquire) &&
            handle_ && handle_ != INVALID_HANDLE_VALUE)
        {
            g_activeSessionHandle = handle_;
            active_ = true;
        }
    }

    ~ActiveSessionRegistration()
    {
        if (!active_)
            return;
        std::lock_guard<std::mutex> lock(g_activeSessionMutex);
        if (g_activeSessionHandle == handle_)
            g_activeSessionHandle = INVALID_HANDLE_VALUE;
    }

    ActiveSessionRegistration(const ActiveSessionRegistration&) = delete;
    ActiveSessionRegistration& operator=(const ActiveSessionRegistration&) = delete;

    [[nodiscard]] bool IsActive() const noexcept { return active_; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    bool active_ = false;
};

std::uint64_t NowUs()
{
    static const std::uint64_t frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return static_cast<std::uint64_t>(value.QuadPart > 0 ? value.QuadPart : 1);
    }();
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return (static_cast<std::uint64_t>(now.QuadPart) * 1000000ull) / frequency;
}

#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
void FormatDiagnosticRate(
    std::uint64_t milliHz,
    wchar_t* out,
    std::size_t outCount) noexcept
{
    if (!out || outCount == 0) return;
    _snwprintf_s(out, outCount, _TRUNCATE, L"%llu.%03llu",
        static_cast<unsigned long long>(milliHz / 1000u),
        static_cast<unsigned long long>(milliHz % 1000u));
}

void FormatActiveValues(
    const aula_win60he::DiagnosticObservation& observation,
    wchar_t* out,
    std::size_t outCount) noexcept
{
    if (!out || outCount == 0) return;
    out[0] = L'\0';
    std::size_t position = 0;
    for (std::size_t index = 0; index < observation.activeCount; ++index)
    {
        const auto& active = observation.active[index];
        const int written = _snwprintf_s(
            out + position, outCount - position, _TRUNCATE,
            index == 0 ? L"%03X@%u,%u:%u" : L",%03X@%u,%u:%u",
            static_cast<unsigned>(active.keyCode),
            static_cast<unsigned>(active.row),
            static_cast<unsigned>(active.column),
            static_cast<unsigned>(active.travelUm));
        if (written <= 0) break;
        position += static_cast<std::size_t>(written);
        if (position + 32u >= outCount) break;
    }
}

void TraceDiagnosticWindow(
    const wchar_t* event,
    std::uint64_t sessionId,
    const aula_win60he::DiagnosticWindow& window,
    std::uint64_t lifetimeUpdates,
    std::uint32_t observedHids,
    std::uint64_t failedUpdates) noexcept
{
    wchar_t rate[32]{};
    FormatDiagnosticRate(window.RateMilliHz(), rate, _countof(rate));
    StabilityTrace_Write(L"INFO", L"aula-win60he", event,
        L"session=%llu window_ms=%llu matrices=%llu hz=%ls changed=%llu nonzero=%llu current_active=%u max_active=%u active_hist=0:%llu,1:%llu,2-4:%llu,5-9:%llu,10+:%llu transitions=press:%llu,release_zero:%llu interval_us=min:%u,avg:%llu,max:%u transaction_us=min:%u,avg:%llu,max:%u transaction_buckets=le2:%llu,le4:%llu,le8:%llu,le16:%llu,le33:%llu,le100:%llu,gt100:%llu travel_um=min_positive:%u,max:%u lifetime_matrices=%llu lifetime_failed=%llu observed_hids=%u",
        static_cast<unsigned long long>(sessionId),
        static_cast<unsigned long long>(window.elapsedUs / 1000u),
        static_cast<unsigned long long>(window.updates), rate,
        static_cast<unsigned long long>(window.changedUpdates),
        static_cast<unsigned long long>(window.nonzeroUpdates),
        static_cast<unsigned>(window.currentActiveKeys),
        static_cast<unsigned>(window.maximumActiveKeys),
        static_cast<unsigned long long>(window.activeBuckets[0]),
        static_cast<unsigned long long>(window.activeBuckets[1]),
        static_cast<unsigned long long>(window.activeBuckets[2]),
        static_cast<unsigned long long>(window.activeBuckets[3]),
        static_cast<unsigned long long>(window.activeBuckets[4]),
        static_cast<unsigned long long>(window.pressTransitions),
        static_cast<unsigned long long>(window.releaseToZeroTransitions),
        static_cast<unsigned>(window.minimumIntervalUs),
        static_cast<unsigned long long>(window.AverageIntervalUs()),
        static_cast<unsigned>(window.maximumIntervalUs),
        static_cast<unsigned>(window.minimumTransactionUs),
        static_cast<unsigned long long>(window.AverageTransactionUs()),
        static_cast<unsigned>(window.maximumTransactionUs),
        static_cast<unsigned long long>(window.transactionBuckets[0]),
        static_cast<unsigned long long>(window.transactionBuckets[1]),
        static_cast<unsigned long long>(window.transactionBuckets[2]),
        static_cast<unsigned long long>(window.transactionBuckets[3]),
        static_cast<unsigned long long>(window.transactionBuckets[4]),
        static_cast<unsigned long long>(window.transactionBuckets[5]),
        static_cast<unsigned long long>(window.transactionBuckets[6]),
        static_cast<unsigned>(window.minimumPositiveUm),
        static_cast<unsigned>(window.maximumUm),
        static_cast<unsigned long long>(lifetimeUpdates),
        static_cast<unsigned long long>(failedUpdates),
        static_cast<unsigned>(observedHids));
}

void TraceDiagnosticCoverage(
    std::uint64_t sessionId,
    const aula_win60he::DiagnosticMetrics& metrics) noexcept
{
    wchar_t values[1536]{};
    std::size_t position = 0;
    const auto& maxima = metrics.MaximumByKeyCode();
    for (std::size_t keyCode = 1; keyCode < maxima.size(); ++keyCode)
    {
        if (maxima[keyCode] == 0) continue;
        const int written = _snwprintf_s(
            values + position, _countof(values) - position, _TRUNCATE,
            position == 0 ? L"%03X:%u" : L",%03X:%u",
            static_cast<unsigned>(keyCode),
            static_cast<unsigned>(maxima[keyCode]));
        if (written <= 0) break;
        position += static_cast<std::size_t>(written);
        if (position + 24u >= _countof(values)) break;
    }
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"matrix.coverage",
        L"session=%llu observed_key_codes=%u max_active=%u max_by_key_code_um=%ls",
        static_cast<unsigned long long>(sessionId),
        static_cast<unsigned>(metrics.ObservedHids()),
        static_cast<unsigned>(metrics.MaximumActiveKeys()),
        values[0] ? values : L"none");
}
#endif

void AtomicMaximum(std::atomic<std::uint32_t>& target, std::uint32_t value)
{
    std::uint32_t current = target.load(std::memory_order_relaxed);
    while (current < value &&
        !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}

void UpdateSmoothed(std::atomic<std::uint32_t>& target, std::uint32_t value)
{
    std::uint32_t current = target.load(std::memory_order_relaxed);
    for (;;)
    {
        const std::uint32_t next = current == 0 ? value :
            (value > current
                ? current + (value - current + 3u) / 4u
                : current - (current - value + 3u) / 4u);
        if (target.compare_exchange_weak(current, next, std::memory_order_relaxed))
            return;
    }
}

void RecordSuccessfulMatrix()
{
    const std::uint64_t now = NowUs();
    const std::uint64_t previous = g_lastMatrixUs.exchange(now, std::memory_order_relaxed);
    if (previous != 0 && now > previous)
    {
        const auto interval = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(now - previous, 0xffffffffull));
        UpdateSmoothed(g_averageIntervalUs, interval);
        AtomicMaximum(g_maximumIntervalUs, interval);
    }
    g_lastUpdateMs.store(GetTickCount64(), std::memory_order_relaxed);
    g_successfulUpdates.fetch_add(1, std::memory_order_relaxed);
}

bool IsSupportedReportLength(std::uint16_t length)
{
    return length == aula_win60he::kWindowsHidReportBytes;
}

bool ContainsFamilyToken(const std::wstring& value)
{
    std::wstring lower(value);
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](wchar_t character) {
            return static_cast<wchar_t>(std::towlower(character));
        });
    return lower.find(L"aula") != std::wstring::npos ||
        lower.find(L"sparkplayjoy") != std::wstring::npos ||
        lower.find(L"spark play joy") != std::wstring::npos;
}

bool IsAulaFamilyIdentity(const Candidate& candidate)
{
    const bool familyBrand =
        aula_win60he::IsKnownUsbIdentity(
            candidate.attributes.VendorID,
            candidate.attributes.ProductID) ||
        candidate.attributes.VendorID == aula_win60he::kAulaVendorId ||
        ContainsFamilyToken(candidate.manufacturer) ||
        ContainsFamilyToken(candidate.product);
    return familyBrand &&
        candidate.caps.UsagePage == aula_win60he::kAulaUsagePage &&
        candidate.caps.Usage == aula_win60he::kAulaUsage &&
        IsSupportedReportLength(candidate.caps.InputReportByteLength) &&
        IsSupportedReportLength(candidate.caps.OutputReportByteLength);
}

bool IsFingerprint(const Candidate& candidate)
{
    return IsAulaFamilyIdentity(candidate);
}

bool EqualWindowsIdentity(
    const std::wstring& left,
    const std::wstring& right) noexcept
{
    if (left.empty() || right.empty() || left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (std::towlower(left[index]) != std::towlower(right[index]))
            return false;
    }
    return true;
}

bool SameDevicePath(const wchar_t* left, const std::wstring& right) noexcept
{
    if (!left)
        return false;
    return EqualWindowsIdentity(std::wstring(left), right);
}

bool PathContainsAulaVendor(const wchar_t* path)
{
    if (!path)
        return false;
    std::wstring lower(path);
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](wchar_t value) {
            return static_cast<wchar_t>(std::towlower(value));
        });
    return lower.find(L"vid_1ca2") != std::wstring::npos;
}

std::wstring ReadDeviceProperty(
    HDEVINFO info,
    SP_DEVINFO_DATA* deviceInfo,
    DWORD property)
{
    if (info == INVALID_HANDLE_VALUE || !deviceInfo)
        return {};

    DWORD type = 0;
    DWORD required = 0;
    (void)SetupDiGetDeviceRegistryPropertyW(
        info, deviceInfo, property, &type, nullptr, 0, &required);
    if (required < sizeof(wchar_t) ||
        (type != REG_SZ && type != REG_EXPAND_SZ && type != REG_MULTI_SZ))
        return {};

    std::vector<std::uint8_t> storage(required + sizeof(wchar_t), 0);
    if (!SetupDiGetDeviceRegistryPropertyW(
            info, deviceInfo, property, &type, storage.data(),
            static_cast<DWORD>(storage.size()), nullptr))
        return {};
    return std::wstring(reinterpret_cast<const wchar_t*>(storage.data()));
}

bool IsAulaFamilySetupIdentity(
    HDEVINFO info,
    SP_DEVINFO_DATA* deviceInfo)
{
    return ContainsFamilyToken(ReadDeviceProperty(info, deviceInfo, SPDRP_MFG)) ||
        ContainsFamilyToken(ReadDeviceProperty(info, deviceInfo, SPDRP_FRIENDLYNAME)) ||
        ContainsFamilyToken(ReadDeviceProperty(info, deviceInfo, SPDRP_DEVICEDESC));
}

#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
bool IsGravaStarSetupIdentity(
    HDEVINFO info,
    SP_DEVINFO_DATA* deviceInfo)
{
    const auto containsGravaStar = [](const std::wstring& value) {
        std::wstring lower(value);
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](wchar_t character) {
                return static_cast<wchar_t>(std::towlower(character));
            });
        return lower.find(L"gravastar") != std::wstring::npos ||
            lower.find(L"grava star") != std::wstring::npos;
    };
    return containsGravaStar(ReadDeviceProperty(info, deviceInfo, SPDRP_MFG)) ||
        containsGravaStar(ReadDeviceProperty(info, deviceInfo, SPDRP_FRIENDLYNAME)) ||
        containsGravaStar(ReadDeviceProperty(info, deviceInfo, SPDRP_DEVICEDESC));
}
#endif

using HidStringReader = BOOLEAN(__stdcall*)(HANDLE, PVOID, ULONG);

std::wstring ReadHidString(HANDLE handle, HidStringReader reader)
{
    if (!handle || handle == INVALID_HANDLE_VALUE || !reader)
        return {};
    std::array<wchar_t, 256> buffer{};
    if (!reader(handle, buffer.data(), static_cast<ULONG>(sizeof(buffer))))
        return {};
    buffer.back() = L'\0';
    return std::wstring(buffer.data());
}

bool ReadMetadata(HANDLE handle, Candidate* inOut)
{
    if (!handle || handle == INVALID_HANDLE_VALUE || !inOut)
        return false;

    Candidate candidate = *inOut;
    candidate.attributes = HIDD_ATTRIBUTES{};
    candidate.attributes.Size = sizeof(candidate.attributes);
    if (!HidD_GetAttributes(handle, &candidate.attributes))
        return false;
    candidate.manufacturer = ReadHidString(handle, HidD_GetManufacturerString);
    candidate.product = ReadHidString(handle, HidD_GetProductString);

    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(handle, &preparsed))
        return false;
    const NTSTATUS status = HidP_GetCaps(preparsed, &candidate.caps);
    HidD_FreePreparsedData(preparsed);
    *inOut = candidate;
    if (status != HIDP_STATUS_SUCCESS)
        return false;

    return IsFingerprint(*inOut);
}

bool ReadInstanceId(
    HDEVINFO info,
    SP_DEVINFO_DATA* deviceInfo,
    std::wstring* out)
{
    if (out) out->clear();
    if (info == INVALID_HANDLE_VALUE || !deviceInfo || !out)
        return false;

    DWORD required = 0;
    (void)SetupDiGetDeviceInstanceIdW(
        info, deviceInfo, nullptr, 0, &required);
    if (required == 0)
        return false;

    std::vector<wchar_t> buffer(required + 1u, L'\0');
    if (!SetupDiGetDeviceInstanceIdW(
            info, deviceInfo, buffer.data(),
            static_cast<DWORD>(buffer.size()), nullptr))
        return false;
    *out = buffer.data();
    return !out->empty();
}

bool QueryCurrentIdentityForPath(
    const std::wstring& path,
    std::wstring* optionalInstanceId)
{
    if (optionalInstanceId) optionalInstanceId->clear();
    if (path.empty() || !optionalInstanceId)
        return false;

    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO info = SetupDiGetClassDevsW(
        &hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE)
        return false;

    std::wstring matchedInstanceId;
    std::size_t matches = 0;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(
                info, nullptr, &hidGuid, index, &interfaceData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;
            continue;
        }

        DWORD detailBytes = 0;
        (void)SetupDiGetDeviceInterfaceDetailW(
            info, &interfaceData, nullptr, 0, &detailBytes, nullptr);
        if (detailBytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
            continue;

        std::vector<std::uint8_t> storage(detailBytes, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(
            storage.data());
        detail->cbSize = sizeof(*detail);
        SP_DEVINFO_DATA deviceInfo{};
        deviceInfo.cbSize = sizeof(deviceInfo);
        if (!SetupDiGetDeviceInterfaceDetailW(
                info, &interfaceData, detail, detailBytes, nullptr, &deviceInfo) ||
            !SameDevicePath(detail->DevicePath, path))
            continue;

        ++matches;
        std::wstring instanceId;
        if (ReadInstanceId(info, &deviceInfo, &instanceId))
            matchedInstanceId = std::move(instanceId);
    }

    SetupDiDestroyDeviceInfoList(info);
    if (matches != 1u)
        return false;
    *optionalInstanceId = std::move(matchedInstanceId);
    return true;
}

EnumerationResult EnumerateCandidates()
{
    EnumerationResult result{};
    result.attempt = g_discoveryAttempt.fetch_add(1, std::memory_order_relaxed) + 1u;
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    AdvanceDiagnosticProgress(DiagnosticProgress::EnumerationStarted);
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"enumeration.begin",
        L"attempt=%u", result.attempt);
#endif
    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO info = SetupDiGetClassDevsW(
        &hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE)
    {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        const DWORD error = GetLastError();
        RecordDiagnosticFailure(DiagnosticFailure::SetupApiEnumeration);
        StabilityTrace_Write(L"ERROR", L"aula-win60he", L"enumeration.error",
            L"attempt=%u stage=class_devices native_error=%u",
            result.attempt, static_cast<unsigned>(error));
#endif
        return result;
    }

    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(info, nullptr, &hidGuid, index, &interfaceData))
        {
            const DWORD enumerationError = GetLastError();
            if (enumerationError == ERROR_NO_MORE_ITEMS)
                break;
            ++result.interfaceEnumerationErrors;
            break;
        }
        ++result.hidInterfaces;

        DWORD detailBytes = 0;
        (void)SetupDiGetDeviceInterfaceDetailW(
            info, &interfaceData, nullptr, 0, &detailBytes, nullptr);
        if (detailBytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
        {
            ++result.interfaceDetailErrors;
            continue;
        }

        std::vector<std::uint8_t> storage(detailBytes, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(*detail);
        SP_DEVINFO_DATA deviceInfo{};
        deviceInfo.cbSize = sizeof(deviceInfo);
        if (!SetupDiGetDeviceInterfaceDetailW(
                info, &interfaceData, detail, detailBytes, nullptr, &deviceInfo))
        {
            ++result.interfaceDetailErrors;
            continue;
        }

        const bool knownUsbIdentity =
            aula_win60he::PathContainsKnownUsbIdentity(detail->DevicePath);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        std::uint16_t pathVendorId = 0;
        std::uint16_t pathProductId = 0;
        const bool parsedUsbIdentity = aula_win60he::TryReadUsbIdentityFromPath(
            detail->DevicePath, &pathVendorId, &pathProductId);
        const bool familyVendorPath = parsedUsbIdentity &&
            (pathVendorId == 0x1CA2u || pathVendorId == 0x1CA5u);
        const bool gravaStarBrandPath = IsGravaStarSetupIdentity(info, &deviceInfo);
        if (familyVendorPath)
        {
            ++result.familyVendorPaths;
            g_diagnosticLastSeenVendorId.store(pathVendorId, std::memory_order_relaxed);
            g_diagnosticLastSeenProductId.store(pathProductId, std::memory_order_relaxed);
            StabilityTrace_Write(L"INFO", L"aula-win60he", L"enumeration.family_usb",
                L"attempt=%u index=%u vid=%04X pid=%04X exact_known=%u path_redacted=1",
                result.attempt, static_cast<unsigned>(index),
                static_cast<unsigned>(pathVendorId),
                static_cast<unsigned>(pathProductId),
                static_cast<unsigned>(knownUsbIdentity));
        }
        if (gravaStarBrandPath)
        {
            ++result.gravaStarBrandPaths;
            StabilityTrace_Write(L"INFO", L"aula-win60he", L"enumeration.gravastar_brand",
                L"attempt=%u index=%u vid=%04X pid=%04X exact_known=%u path_redacted=1",
                result.attempt, static_cast<unsigned>(index),
                static_cast<unsigned>(pathVendorId),
                static_cast<unsigned>(pathProductId),
                static_cast<unsigned>(knownUsbIdentity));
        }
#endif

        if (knownUsbIdentity)
        {
            result.knownIdentityPathSeen = true;
            ++result.knownIdentityPaths;
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            AdvanceDiagnosticProgress(DiagnosticProgress::KnownIdentitySeen);
#endif
        }

        // Do not open every HID interface on the machine during periodic
        // discovery. A family candidate must first have a firmware-proven
        // exact USB identity, Aula's VID, or an Aula/SparkPlayJoy identity
        // exposed by SetupAPI. The later HID shape, exact known-board
        // correlation and full read-only wire proof remain mandatory.
        if (!knownUsbIdentity &&
            !PathContainsAulaVendor(detail->DevicePath) &&
            !IsAulaFamilySetupIdentity(info, &deviceInfo))
        {
            ++result.identityPrefilterRejected;
            continue;
        }

        const bool claimedByAula = NativeAnalogRouting_IsClaimedBy(
            detail->DevicePath, NativeAnalogProtocol::AulaWin60He);
        if (NativeAnalogRouting_IsClaimed(detail->DevicePath) && !claimedByAula)
            continue;

        Candidate candidate{};
        candidate.path = detail->DevicePath;
        // SetupAPI instance ID is stronger identity evidence when available,
        // but some valid HID stacks do not expose it. Exact path correlation,
        // exclusive open and the full live capability proof remain mandatory.
        (void)ReadInstanceId(info, &deviceInfo, &candidate.instanceId);

        // This metadata handle is never used for protocol traffic. The command
        // channel is opened later with shareMode=0 and retained through proof
        // and polling.
        ScopedHandle metadata(CreateFileW(
            candidate.path.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        [[maybe_unused]] const DWORD metadataOpenError =
            metadata ? ERROR_SUCCESS : GetLastError();
        const bool metadataOk = metadata && ReadMetadata(metadata.value, &candidate);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        StabilityTrace_Write(metadataOk ? L"INFO" : L"WARN",
            L"aula-win60he", L"enumeration.candidate",
            L"attempt=%u index=%u path_hash=%016llX instance_hash=%016llX metadata_ok=%u open_error=%u vid=%04X pid=%04X usage_page=%04X usage=%04X in_bytes=%u out_bytes=%u",
            result.attempt, static_cast<unsigned>(index),
            static_cast<unsigned long long>(HashWideIdentity(candidate.path)),
            static_cast<unsigned long long>(HashWideIdentity(candidate.instanceId)),
            static_cast<unsigned>(metadataOk),
            static_cast<unsigned>(metadataOpenError),
            static_cast<unsigned>(candidate.attributes.VendorID),
            static_cast<unsigned>(candidate.attributes.ProductID),
            static_cast<unsigned>(candidate.caps.UsagePage),
            static_cast<unsigned>(candidate.caps.Usage),
            static_cast<unsigned>(candidate.caps.InputReportByteLength),
            static_cast<unsigned>(candidate.caps.OutputReportByteLength));
#endif
        if (!metadataOk)
        {
            ++result.metadataRejected;
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            RecordDiagnosticFailure(DiagnosticFailure::MetadataFingerprint);
#endif
            continue;
        }
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        AdvanceDiagnosticProgress(DiagnosticProgress::FingerprintCandidate);
#endif
        result.candidates.push_back(std::move(candidate));
    }

    SetupDiDestroyDeviceInfoList(info);
    std::sort(result.candidates.begin(), result.candidates.end(),
        [](const Candidate& left, const Candidate& right) {
            if (left.instanceId != right.instanceId)
                return left.instanceId < right.instanceId;
            return left.path < right.path;
        });
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    if (result.interfaceEnumerationErrors != 0 ||
        result.interfaceDetailErrors != 0)
    {
        RecordDiagnosticFailure(DiagnosticFailure::SetupApiEnumeration);
    }
    AtomicMaximum(g_diagnosticFamilyVendorPaths, result.familyVendorPaths);
    AtomicMaximum(g_diagnosticGravaStarBrandPaths, result.gravaStarBrandPaths);
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"enumeration.end",
        L"attempt=%u hid_interfaces=%u known_identity_paths=%u family_vendor_paths=%u gravastar_brand_paths=%u enum_errors=%u detail_errors=%u identity_prefilter_rejected=%u fingerprint_candidates=%u metadata_rejected=%u",
        result.attempt, result.hidInterfaces, result.knownIdentityPaths,
        result.familyVendorPaths, result.gravaStarBrandPaths,
        result.interfaceEnumerationErrors, result.interfaceDetailErrors,
        result.identityPrefilterRejected,
        static_cast<unsigned>(result.candidates.size()), result.metadataRejected);
#endif
    return result;
}

bool EnsureInputQueueCapacity(HANDLE handle, DWORD* error)
{
    if (error) *error = ERROR_SUCCESS;
    ULONG current = 0;
    if (!HidD_GetNumInputBuffers(handle, &current))
    {
        if (error) *error = GetLastError();
        return false;
    }

    if (current < kRequestedInputBuffers)
    {
        // Enlargement is optional once at least one complete three-report
        // response already fits. It is mandatory when the current queue is too
        // small to hold one complete response.
        const bool mustGrow = current < kMinimumInputBuffers;
        if (!HidD_SetNumInputBuffers(handle, kRequestedInputBuffers))
        {
            if (mustGrow)
            {
                if (error) *error = GetLastError();
                return false;
            }
            return true;
        }

        ULONG verified = 0;
        if (!HidD_GetNumInputBuffers(handle, &verified) ||
            verified < kMinimumInputBuffers)
        {
            if (error)
            {
                *error = verified < kMinimumInputBuffers
                    ? ERROR_INSUFFICIENT_BUFFER
                    : GetLastError();
            }
            return false;
        }
    }
    return true;
}

bool OpenSession(const Candidate& candidate, Session* out, DWORD* openError)
{
    if (out) *out = Session{};
    if (openError) *openError = ERROR_SUCCESS;
    if (!out || !IsFingerprint(candidate) || candidate.path.empty())
    {
        if (openError) *openError = ERROR_INVALID_PARAMETER;
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::SessionOpen);
        StabilityTrace_Write(L"WARN", L"aula-win60he", L"session.open_failed",
            L"stage=invalid_candidate error=%u",
            static_cast<unsigned>(ERROR_INVALID_PARAMETER));
#endif
        return false;
    }

    Session session{};
    session.candidate = candidate;
    session.handle = ScopedHandle(CreateFileW(
        candidate.path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, // protocol has no transaction ID: the command channel must be exclusive
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr));
    if (!session.handle)
    {
        if (openError) *openError = GetLastError();
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::SessionOpen);
        StabilityTrace_Write(L"WARN", L"aula-win60he", L"session.open_failed",
            L"stage=exclusive_open error=%u path_hash=%016llX",
            static_cast<unsigned>(openError ? *openError : GetLastError()),
            static_cast<unsigned long long>(HashWideIdentity(candidate.path)));
#endif
        return false;
    }

    // Close the enumerate/open TOCTOU window. A recycled path must not inherit
    // the trust assigned to the SetupAPI instance observed before CreateFileW.
    std::wstring openedInstanceId;
    if (!QueryCurrentIdentityForPath(candidate.path, &openedInstanceId) ||
        (!candidate.instanceId.empty() &&
            !EqualWindowsIdentity(openedInstanceId, candidate.instanceId)))
    {
        if (openError) *openError = ERROR_INVALID_DATA;
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::SessionOpen);
        StabilityTrace_Write(L"WARN", L"aula-win60he", L"session.open_failed",
            L"stage=identity_recheck error=%u path_hash=%016llX enumerated_instance_hash=%016llX opened_instance_hash=%016llX",
            static_cast<unsigned>(ERROR_INVALID_DATA),
            static_cast<unsigned long long>(HashWideIdentity(candidate.path)),
            static_cast<unsigned long long>(HashWideIdentity(candidate.instanceId)),
            static_cast<unsigned long long>(HashWideIdentity(openedInstanceId)));
#endif
        return false;
    }
    // Identity may strengthen after the exclusive open if SetupAPI starts
    // returning an instance ID. It may never weaken or contradict an ID that
    // was present during enumeration.
    session.candidate.instanceId = std::move(openedInstanceId);

    // Re-read the full HID fingerprint through the exact exclusive handle that
    // will perform proof and polling.
    if (!ReadMetadata(session.handle.value, &session.candidate))
    {
        if (openError) *openError = ERROR_INVALID_DATA;
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::SessionOpen);
        StabilityTrace_Write(L"WARN", L"aula-win60he", L"session.open_failed",
            L"stage=exclusive_metadata error=%u vid=%04X pid=%04X usage_page=%04X usage=%04X in_bytes=%u out_bytes=%u",
            static_cast<unsigned>(ERROR_INVALID_DATA),
            static_cast<unsigned>(session.candidate.attributes.VendorID),
            static_cast<unsigned>(session.candidate.attributes.ProductID),
            static_cast<unsigned>(session.candidate.caps.UsagePage),
            static_cast<unsigned>(session.candidate.caps.Usage),
            static_cast<unsigned>(session.candidate.caps.InputReportByteLength),
            static_cast<unsigned>(session.candidate.caps.OutputReportByteLength));
#endif
        return false;
    }
    if (!EnsureInputQueueCapacity(session.handle.value, openError))
    {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::SessionOpen);
        StabilityTrace_Write(L"WARN", L"aula-win60he", L"session.open_failed",
            L"stage=input_queue error=%u",
            static_cast<unsigned>(openError ? *openError : ERROR_GEN_FAILURE));
#endif
        return false;
    }

    DWORD eventError = ERROR_SUCCESS;
    session.readEvent = ScopedHandle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!session.readEvent)
        eventError = GetLastError();
    session.writeEvent = ScopedHandle(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!session.writeEvent && eventError == ERROR_SUCCESS)
        eventError = GetLastError();
    if (!session)
    {
        if (openError) *openError = eventError;
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::SessionOpen);
        StabilityTrace_Write(L"WARN", L"aula-win60he", L"session.open_failed",
            L"stage=io_events error=%u read_event=%u write_event=%u",
            static_cast<unsigned>(eventError),
            static_cast<unsigned>(static_cast<bool>(session.readEvent)),
            static_cast<unsigned>(static_cast<bool>(session.writeEvent)));
#endif
        return false;
    }

#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    AdvanceDiagnosticProgress(DiagnosticProgress::SessionOpened);
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"session.opened",
        L"path_hash=%016llX instance_hash=%016llX vid=%04X pid=%04X usage_page=%04X usage=%04X in_bytes=%u out_bytes=%u",
        static_cast<unsigned long long>(HashWideIdentity(session.candidate.path)),
        static_cast<unsigned long long>(HashWideIdentity(session.candidate.instanceId)),
        static_cast<unsigned>(session.candidate.attributes.VendorID),
        static_cast<unsigned>(session.candidate.attributes.ProductID),
        static_cast<unsigned>(session.candidate.caps.UsagePage),
        static_cast<unsigned>(session.candidate.caps.Usage),
        static_cast<unsigned>(session.candidate.caps.InputReportByteLength),
        static_cast<unsigned>(session.candidate.caps.OutputReportByteLength));
#endif

    *out = std::move(session);
    return true;
}

bool FinishIo(HidIoOperation& operation, DWORD timeoutMs, DWORD* transferred, DWORD* error)
{
    const DWORD wait = operation.Wait(timeoutMs);
    if (wait == WAIT_OBJECT_0)
        return operation.Finish(transferred, error, false);
    operation.CancelAndDrain(transferred, error);
    if (error && wait == WAIT_TIMEOUT)
        *error = WAIT_TIMEOUT;
    return false;
}

bool WriteReport(Session& session, const aula_win60he::Report& report)
{
    const DWORD reportBytes = session.candidate.caps.OutputReportByteLength;
    if (!IsSupportedReportLength(static_cast<std::uint16_t>(reportBytes)))
        return false;

    aula_win60he::HidWireReport buffer{};
    std::size_t wireBytes = 0;
    if (!aula_win60he::EncodeHidReport(report, reportBytes, &buffer, &wireBytes) ||
        wireBytes != reportBytes)
        return false;

    DWORD error = ERROR_SUCCESS;
    DWORD transferred = 0;
    HidIoOperation operation(session.handle.value, session.writeEvent.value);
    const auto started = operation.StartWrite(buffer.data(), reportBytes, &error);
    if (started == HidIoOperation::StartResult::Failed)
        return false;
    if (started == HidIoOperation::StartResult::Completed)
        return operation.Finish(&transferred, &error, false) && transferred == reportBytes;
    return FinishIo(operation, kWriteTimeoutMs, &transferred, &error) &&
        transferred == reportBytes;
}

bool ReadProtocolBlock(Session& session, DWORD timeoutMs, aula_win60he::Report* out)
{
    if (out) out->fill(0);
    if (!out)
        return false;

    const DWORD reportBytes = session.candidate.caps.InputReportByteLength;
    if (!IsSupportedReportLength(static_cast<std::uint16_t>(reportBytes)))
        return false;

    aula_win60he::HidWireReport buffer{};
    DWORD error = ERROR_SUCCESS;
    DWORD transferred = 0;
    HidIoOperation operation(session.handle.value, session.readEvent.value);
    const auto started = operation.StartRead(buffer.data(), reportBytes, &error);
    if (started == HidIoOperation::StartResult::Failed)
        return false;

    bool complete = false;
    if (started == HidIoOperation::StartResult::Completed)
        complete = operation.Finish(&transferred, &error, false);
    else
        complete = FinishIo(operation, timeoutMs, &transferred, &error);
    if (!complete || transferred != reportBytes)
        return false;

    return aula_win60he::DecodeHidReport(buffer.data(), reportBytes, out);
}

class WindowsReportTransport final : public aula_win60he::ReportTransport
{
public:
    explicit WindowsReportTransport(Session& session) noexcept : session_(session) {}

    bool FlushInput() override
    {
        return session_.handle && HidD_FlushQueue(session_.handle.value) != FALSE;
    }

    bool WriteReport(const aula_win60he::Report& report) override
    {
        return ::WriteReport(session_, report);
    }

    bool ReadReport(std::uint32_t timeoutMs, aula_win60he::Report* out) override
    {
        return ReadProtocolBlock(session_, static_cast<DWORD>(timeoutMs), out);
    }

    std::uint64_t NowMilliseconds() const noexcept override
    {
        return static_cast<std::uint64_t>(GetTickCount64());
    }

private:
    Session& session_;
};

struct ClientTraceContext
{
    std::uint32_t emitted = 0;
    std::uint32_t maximum = 0;
    aula_win60he::Report lastTransmit{};
    aula_win60he::Report lastReceive{};
    bool hasTransmit = false;
    bool hasReceive = false;
};

[[maybe_unused]] void TraceClientReport(
    void* context,
    aula_win60he::TraceKind kind,
    const char* label,
    const aula_win60he::Report* report) noexcept
{
#if defined(HALLJOY_DIAGNOSTIC) || defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    auto* state = static_cast<ClientTraceContext*>(context);
    const bool error = kind == aula_win60he::TraceKind::Error;
    if (state && report)
    {
        if (kind == aula_win60he::TraceKind::Transmit)
        {
            state->lastTransmit = *report;
            state->hasTransmit = true;
        }
        else if (kind == aula_win60he::TraceKind::Receive)
        {
            state->lastReceive = *report;
            state->hasReceive = true;
        }
    }
    if (!error && state && state->maximum != 0 && state->emitted >= state->maximum)
        return;
    if (state && !error)
        ++state->emitted;

    wchar_t wideLabel[64]{};
    if (label)
    {
        std::size_t i = 0;
        for (; label[i] != '\0' && i + 1u < (sizeof(wideLabel) / sizeof(wideLabel[0])); ++i)
            wideLabel[i] = static_cast<unsigned char>(label[i]);
    }

    const wchar_t* direction = L"ERR";
    if (kind == aula_win60he::TraceKind::Transmit) direction = L"TX";
    else if (kind == aula_win60he::TraceKind::Receive) direction = L"RX";

    if (!report)
    {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        if (g_stop.load(std::memory_order_acquire))
        {
            StabilityTrace_Write(L"INFO", L"aula-win60he", L"protocol.cancelled",
                L"label=%ls reason=shutdown has_last_tx=%u has_last_rx=%u",
                wideLabel,
                static_cast<unsigned>(state && state->hasTransmit),
                static_cast<unsigned>(state && state->hasReceive));
            return;
        }
        StabilityTrace_Write(L"WARN", L"aula-win60he", L"protocol.error",
            L"label=%ls has_last_tx=%u has_last_rx=%u",
            wideLabel,
            static_cast<unsigned>(state && state->hasTransmit),
            static_cast<unsigned>(state && state->hasReceive));
#else
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he.trace] %ls %ls",
            direction, wideLabel);
#endif
        if (state && state->hasTransmit)
            TraceClientReport(nullptr, aula_win60he::TraceKind::Transmit,
                "last-tx-before-error", &state->lastTransmit);
        if (state && state->hasReceive)
            TraceClientReport(nullptr, aula_win60he::TraceKind::Receive,
                "last-rx-before-error", &state->lastReceive);
        return;
    }

    wchar_t hex[aula_win60he::kWireReportBytes * 3u + 1u]{};
    std::size_t pos = 0;
    for (std::size_t i = 0; i < report->size() && pos + 3u < (sizeof(hex) / sizeof(hex[0])); ++i)
    {
        unsigned value = static_cast<unsigned>((*report)[i]);
        const bool syncSerial = kind == aula_win60he::TraceKind::Receive &&
            (*report)[2] == aula_win60he::ResponseCommand(aula_win60he::kCommandSync) &&
            i >= 4u + aula_win60he::kSyncSerialOffset &&
            i < 4u + aula_win60he::kSyncSerialOffset + aula_win60he::kSyncSerialBytes;
        if (syncSerial) value = 0;
        const int written = _snwprintf_s(
            hex + pos, (sizeof(hex) / sizeof(hex[0])) - pos, _TRUNCATE,
            i + 1u == report->size() ? L"%02X" : L"%02X ",
            value);
        if (written <= 0)
            break;
        pos += static_cast<std::size_t>(written);
    }
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"protocol.report",
        L"dir=%ls label=%ls serial_redacted=%u bytes=%ls",
        direction, wideLabel,
        static_cast<unsigned>(kind == aula_win60he::TraceKind::Receive &&
            (*report)[2] == aula_win60he::ResponseCommand(aula_win60he::kCommandSync)),
        hex);
#else
    DebugLog_WriteBuffered(
        L"[backend.aula_win60he.trace] %ls %ls | %ls",
        direction, wideLabel, hex);
#endif
#else
    (void)context;
    (void)kind;
    (void)label;
    (void)report;
#endif
}

aula_win60he::TraceSink MakeTraceSink(ClientTraceContext* context) noexcept
{
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    return aula_win60he::TraceSink{ &TraceClientReport, context };
#elif defined(HALLJOY_DIAGNOSTIC)
    if (DebugLog_IsDiagnosticBuild())
        return aula_win60he::TraceSink{ &TraceClientReport, context };
    return {};
#else
    (void)context;
    return {};
#endif
}

// An exact USB identity selects its independently proven board ID. Unknown
// brand-scoped family candidates retain the narrower generic-family firmware
// predicate and never inherit the known-board exception.
aula_win60he::CompatibilityPolicy ProbePolicyForSession(
    const Session& session) noexcept
{
    const auto* known = aula_win60he::FindKnownUsbIdentity(
        session.candidate.attributes.VendorID,
        session.candidate.attributes.ProductID);
    if (known)
    {
        return {
            aula_win60he::CompatibilityProfile::ExactKnownBoard6x21Family,
            known->boardId,
        };
    }
    return {
        aula_win60he::CompatibilityProfile::Compatible6x21Family,
        0u,
    };
}


bool BuildProbeResult(
    const Session& session,
    const aula_win60he::CapabilityProof& capability,
    ProbeResult* out)
{
    if (out) *out = ProbeResult{};
    if (!out || !IsFingerprint(session.candidate))
        return false;
    if (!aula_win60he::IsKnownUsbIdentityBoardCompatible(
            session.candidate.attributes.VendorID,
            session.candidate.attributes.ProductID,
            capability.sync.boardId))
    {
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he] exact USB identity returned unexpected board id vid=%04X pid=%04X board=%08X; refusing claim",
            static_cast<unsigned>(session.candidate.attributes.VendorID),
            static_cast<unsigned>(session.candidate.attributes.ProductID),
            static_cast<unsigned>(capability.sync.boardId));
        return false;
    }
    ProbeResult result{};
    result.vendorId = session.candidate.attributes.VendorID;
    result.productId = session.candidate.attributes.ProductID;
    result.usagePage = session.candidate.caps.UsagePage;
    result.usage = session.candidate.caps.Usage;
    result.inputReportBytes = session.candidate.caps.InputReportByteLength;
    result.outputReportBytes = session.candidate.caps.OutputReportByteLength;
    result.capability = capability;
    *out = result;
    return true;
}

void ClearPublishedValues(bool clearOwnership)
{
    g_verifiedLayoutToken.store(0, std::memory_order_release);
    bool changed = false;
    for (std::size_t hid = 0; hid < g_milli.size(); ++hid)
    {
        if (g_milli[hid].exchange(0, std::memory_order_relaxed) != 0)
            changed = true;
        if (clearOwnership)
            g_owned[hid].store(0, std::memory_order_relaxed);
    }
    g_activeKeys.store(0, std::memory_order_relaxed);
    if (changed)
        RealtimeLoop_NotifyInputChanged();
}

void PublishProof(const ProbeResult& proof)
{
    ClearPublishedValues(true);
    for (const auto& row : proof.capability.keyMap)
        for (const auto keyCode : row)
            if (halljoy::keycode::IsSupported(keyCode))
                g_owned[keyCode].store(1, std::memory_order_relaxed);

    g_vendorId.store(proof.vendorId, std::memory_order_relaxed);
    g_productId.store(proof.productId, std::memory_order_relaxed);
    g_usagePage.store(proof.usagePage, std::memory_order_relaxed);
    g_maximumTravelUm.store(
        proof.capability.precision.maximumTravelUm, std::memory_order_relaxed);
    g_precisionUm.store(
        proof.capability.precision.precisionUm, std::memory_order_relaxed);
    g_mappedKeys.store(
        static_cast<std::uint32_t>(proof.capability.mappedKeys),
        std::memory_order_relaxed);
    g_inputReportBytes.store(proof.inputReportBytes, std::memory_order_relaxed);
    g_outputReportBytes.store(proof.outputReportBytes, std::memory_order_relaxed);
}

bool PublishMatrix(
    const aula_win60he::ActiveKeyMap& map,
    const aula_win60he::TravelMatrix& travel,
    std::uint16_t maximumTravelUm)
{
    aula_win60he::SnapshotResult snapshot{};
    aula_win60he::BuildHidMilliSnapshot(map, travel, maximumTravelUm, &snapshot);

    bool changed = false;
    for (std::size_t hid = 1; hid < snapshot.milli.size(); ++hid)
    {
        if (g_owned[hid].load(std::memory_order_relaxed) == 0)
            continue;
        const auto next = snapshot.milli[hid];
        if (g_milli[hid].exchange(next, std::memory_order_relaxed) != next)
            changed = true;
    }
    g_activeKeys.store(snapshot.activeKeys, std::memory_order_relaxed);
    if (changed)
        RealtimeLoop_NotifyInputChanged();
    return changed;
}

RetainedDeviceIdentity PreferredIdentity()
{
    std::lock_guard<std::mutex> lock(g_claimMutex);
    return g_claim.identity;
}

bool OpenSelectedSession(Session* out)
{
    if (out) *out = Session{};
    if (!out)
        return false;

    const EnumerationResult enumeration = EnumerateCandidates();
    g_candidatePresent.store(
        enumeration.knownIdentityPathSeen || !enumeration.candidates.empty(),
        std::memory_order_release);
    g_candidateCount.store(
        static_cast<std::uint32_t>(enumeration.candidates.size()),
        std::memory_order_relaxed);
    g_ambiguousSelection.store(false, std::memory_order_relaxed);
    g_invalidEnumeration.store(false, std::memory_order_relaxed);
    g_retainedIdentityMissing.store(false, std::memory_order_relaxed);
    g_firmwareSerialMismatch.store(false, std::memory_order_relaxed);

    std::vector<std::wstring> paths;
    std::vector<std::wstring> instanceIds;
    paths.reserve(enumeration.candidates.size());
    instanceIds.reserve(enumeration.candidates.size());
    for (const auto& candidate : enumeration.candidates)
    {
        paths.push_back(candidate.path);
        instanceIds.push_back(candidate.instanceId);
    }

    const RetainedDeviceIdentity preferred = PreferredIdentity();
    const aula_win60he::DeviceSelectionPlan selection =
        aula_win60he::PlanDeviceSelection(
            paths, instanceIds,
            preferred.valid ? preferred.path : std::wstring{},
            preferred.valid ? preferred.instanceId : std::wstring{});
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"selection.plan",
        L"attempt=%u candidates=%u selected=%u invalid=%u ambiguous=%u retained_missing=%u retained_valid=%u",
        enumeration.attempt,
        static_cast<unsigned>(enumeration.candidates.size()),
        static_cast<unsigned>(selection.candidateIndices.size()),
        static_cast<unsigned>(selection.invalidEnumeration),
        static_cast<unsigned>(selection.ambiguous),
        static_cast<unsigned>(selection.retainedIdentityMissing),
        static_cast<unsigned>(preferred.valid));
#endif
    if (selection.invalidEnumeration)
    {
        g_invalidEnumeration.store(true, std::memory_order_relaxed);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::InvalidEnumeration);
#endif
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he] contradictory SetupAPI enumeration evidence; refusing device selection");
        return false;
    }
    if (selection.ambiguous)
    {
        g_ambiguousSelection.store(true, std::memory_order_relaxed);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::AmbiguousSelection);
#endif
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he] multiple or contradictory exact-fingerprint candidates; refusing implicit device selection");
        return false;
    }
    if (selection.retainedIdentityMissing)
    {
        g_retainedIdentityMissing.store(true, std::memory_order_relaxed);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::RetainedIdentityMissing);
#endif
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he] retained Aula identity is absent; refusing path, instance or serial rebind until backend restart");
        return false;
    }
    if (selection.candidateIndices.size() != 1u ||
        selection.candidateIndices[0] >= enumeration.candidates.size())
    {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        StabilityTrace_Write(L"WARN", L"aula-win60he", L"selection.none",
            L"attempt=%u known_path_seen=%u known_paths=%u fingerprint_candidates=%u",
            enumeration.attempt,
            static_cast<unsigned>(enumeration.knownIdentityPathSeen),
            enumeration.knownIdentityPaths,
            static_cast<unsigned>(enumeration.candidates.size()));
#endif
        return false;
    }

    const Candidate& candidate =
        enumeration.candidates[selection.candidateIndices[0]];
    DWORD openError = ERROR_SUCCESS;
    if (!OpenSession(candidate, out, &openError))
    {
        g_lastOpenError.store(openError, std::memory_order_relaxed);
        if (openError == ERROR_SHARING_VIOLATION)
        {
            DebugLog_WriteBuffered(
                L"[backend.aula_win60he] vendor HID is busy; exclusive access is required because replies have no transaction ID");
        }
        else
        {
            DebugLog_WriteBuffered(
                L"[backend.aula_win60he] exact exclusive open/identity validation failed error=%lu",
                static_cast<unsigned long>(openError));
        }
        return false;
    }
    g_lastOpenError.store(ERROR_SUCCESS, std::memory_order_relaxed);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"selection.opened",
        L"attempt=%u candidate_index=%u",
        enumeration.attempt, static_cast<unsigned>(selection.candidateIndices[0]));
#endif
    return true;
}

bool MatchesRetainedProofIdentity(
    const Session& session,
    const aula_win60he::CapabilityProof& capability)
{
    const RetainedDeviceIdentity retained = PreferredIdentity();
    if (!retained.valid)
        return true;
    return aula_win60he::MatchesRetainedDeviceIdentity(
        retained.path,
        retained.instanceId,
        retained.firmwareSerial,
        retained.hasFirmwareSerialEvidence,
        session.candidate.path,
        session.candidate.instanceId,
        capability.sync.serial);
}

void SaveValidatedClaim(const Session& session, const ProbeResult& proof)
{
    std::lock_guard<std::mutex> lock(g_claimMutex);
    g_claim.validated = true;
    g_claim.identity.path = session.candidate.path;
    g_claim.identity.instanceId = session.candidate.instanceId;
    g_claim.identity.firmwareSerial = proof.capability.sync.serial;
    g_claim.identity.hasFirmwareSerialEvidence =
        aula_win60he::IsMeaningfulDeviceSerial(proof.capability.sync.serial);
    g_claim.identity.valid = true;
    g_claim.proof = proof;
}

void WaitForReconnect(DWORD timeoutMs)
{
    if (g_deviceChanged.exchange(false, std::memory_order_acq_rel))
    {
        if (g_wakeEvent)
            ResetEvent(g_wakeEvent);
        return;
    }
    if (g_wakeEvent)
    {
        const DWORD wait = WaitForSingleObject(g_wakeEvent, timeoutMs);
        if (wait == WAIT_OBJECT_0)
            ResetEvent(g_wakeEvent);
    }
    g_deviceChanged.store(false, std::memory_order_release);
}

bool IsDeterministicSemanticFailure(
    aula_win60he::FailureStage stage) noexcept
{
    using aula_win60he::FailureStage;
    switch (stage)
    {
    case FailureStage::UnexpectedFirmware:
    case FailureStage::UnexpectedPrecision:
    case FailureStage::UnexpectedDefaultMap:
    case FailureStage::UnstableActiveMap:
    case FailureStage::ImplausibleTravel:
        return true;
    default:
        return false;
    }
}

void WaitForDeviceChangeAfterDeterministicRejection()
{
    // Discard notifications that led to the proof we just completed. A new
    // device-change edge (or shutdown) is required before repeating the full
    // 17..25 transaction proof. The bounded wait also observes the atomic flag
    // if NotifyDeviceChange could not take the lifecycle mutex to signal.
    g_deviceChanged.store(false, std::memory_order_release);
    if (g_wakeEvent)
        ResetEvent(g_wakeEvent);
    while (!g_stop.load(std::memory_order_acquire))
    {
        if (g_wakeEvent)
            (void)WaitForSingleObject(g_wakeEvent, 1000u);
        else
            return;
        if (g_stop.load(std::memory_order_acquire))
            return;
        if (g_deviceChanged.exchange(false, std::memory_order_acq_rel))
        {
            ResetEvent(g_wakeEvent);
            return;
        }
        ResetEvent(g_wakeEvent);
    }
}

void SignalInitialAttempt()
{
    if (g_initialAttemptEvent)
        SetEvent(g_initialAttemptEvent);
}

std::uint32_t WorkerMain()
{
#if defined(HALLJOY_ANALOG_SIMULATOR)
    if (InjectStopTimeout())
    {
        StabilityTrace_Write(L"INFO", L"aula-win60he", L"test.start",
            L"simulator_only=1");
        SignalInitialAttempt();
        while (!g_stop.load(std::memory_order_acquire))
            WaitForSingleObject(g_wakeEvent, 100);
        StabilityTrace_Write(L"WARN", L"aula-win60he",
            L"test.stop_timeout.injected", L"simulator_only=1");
        WaitForSingleObject(GetCurrentProcess(), INFINITE);
    }
#endif

    bool initialAttemptSignaled = false;
    const auto signalInitialAttempt = [&initialAttemptSignaled] {
        if (!initialAttemptSignaled)
        {
            SignalInitialAttempt();
            initialAttemptSignaled = true;
        }
    };

#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    std::uint64_t lastDisconnectUs = 0;
#endif

    while (!g_stop.load(std::memory_order_acquire))
    {
        Session session{};
        if (!OpenSelectedSession(&session))
        {
            g_protocolPresent.store(false, std::memory_order_release);
            g_connected.store(false, std::memory_order_release);
            ClearPublishedValues(false);
            signalInitialAttempt();
            // A completely absent MAX-family device is rediscovered by the
            // application's WM_DEVICECHANGE notification. Polling SetupAPI once
            // per second here stalls unrelated HID transports (notably W669) for
            // several milliseconds on every pass. Keep timed retries only when
            // an exact VID/PID path exists but admission/open is transiently
            // incomplete.
            WaitForReconnect(g_candidatePresent.load(std::memory_order_acquire)
                ? kReconnectWaitMs
                : INFINITE);
            continue;
        }

        ActiveSessionRegistration activeSession(session);
        if (!activeSession.IsActive())
        {
            signalInitialAttempt();
            break;
        }

        WindowsReportTransport transport(session);
        ClientTraceContext traceContext{};
        traceContext.maximum = 256;
        aula_win60he::Client client(transport, MakeTraceSink(&traceContext));
        aula_win60he::CapabilityProof capability{};
        aula_win60he::Failure failure{};
        const bool probeOk = client.Probe(
            &capability, &failure, ProbePolicyForSession(session));
        TraceCapabilityOutcome(L"worker", probeOk, capability, failure);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        g_diagnosticLastBoardId.store(
            capability.sync.boardId, std::memory_order_relaxed);
        g_diagnosticLastMismatchMask.store(
            capability.compatibilityMismatchMask, std::memory_order_relaxed);
        if (probeOk)
            AdvanceDiagnosticProgress(DiagnosticProgress::CapabilityProved);
        else
        {
            RecordDiagnosticFailure(DiagnosticFailure::CapabilityProof);
            RecordDiagnosticProtocolFailure(failure);
        }
#endif
        if (!probeOk)
        {
            g_failedUpdates.fetch_add(1, std::memory_order_relaxed);
            DebugLog_Write(
                L"[backend.aula_win60he] proof failed stage=%u command=%02X selector=%02X index=%u; closing exclusive session",
                static_cast<unsigned>(failure.stage),
                static_cast<unsigned>(failure.command),
                static_cast<unsigned>(failure.selector),
                static_cast<unsigned>(failure.index));
            g_protocolPresent.store(false, std::memory_order_release);
            g_connected.store(false, std::memory_order_release);
            ClearPublishedValues(false);
            signalInitialAttempt();
            if (IsDeterministicSemanticFailure(failure.stage))
                WaitForDeviceChangeAfterDeterministicRejection();
            else
                WaitForReconnect(100);
            continue;
        }

        if (capability.compatibilityMismatchMask != 0)
        {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            RecordDiagnosticFailure(DiagnosticFailure::SemanticMismatch);
            StabilityTrace_Write(L"WARN", L"aula-win60he", L"proof.relaxed_complete",
                L"mismatch_mask=%08X claim_blocked=1 publication_blocked=1 retry_ms=%u",
                static_cast<unsigned>(capability.compatibilityMismatchMask),
                static_cast<unsigned>(kReconnectWaitMs));
#endif
            g_protocolPresent.store(false, std::memory_order_release);
            g_connected.store(false, std::memory_order_release);
            ClearPublishedValues(false);
            signalInitialAttempt();
            WaitForDeviceChangeAfterDeterministicRejection();
            continue;
        }

        if (!MatchesRetainedProofIdentity(session, capability))
        {
            g_firmwareSerialMismatch.store(true, std::memory_order_relaxed);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            RecordDiagnosticFailure(DiagnosticFailure::RetainedProofMismatch);
#endif
            DebugLog_WriteBuffered(
                L"[backend.aula_win60he] proven Aula identity returned different retained path, instance or firmware-serial evidence; refusing session");
            g_protocolPresent.store(false, std::memory_order_release);
            g_connected.store(false, std::memory_order_release);
            ClearPublishedValues(false);
            signalInitialAttempt();
            WaitForDeviceChangeAfterDeterministicRejection();
            continue;
        }

        ProbeResult proof{};
        const bool probeResultOk = BuildProbeResult(session, capability, &proof);
        const bool routeClaimed = probeResultOk && NativeAnalogRouting_Claim(
            session.candidate.attributes.VendorID,
            session.candidate.attributes.ProductID,
            session.candidate.path.c_str(),
            NativeAnalogProtocol::AulaWin60He);
        if (!probeResultOk || !routeClaimed)
        {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            RecordDiagnosticFailure(probeResultOk
                ? DiagnosticFailure::RouteClaim
                : DiagnosticFailure::ProbeResult);
            StabilityTrace_Write(L"WARN", L"aula-win60he", L"routing.not_claimed",
                L"phase=worker probe_result_ok=%u route_claimed=%u board_id=%08X claim_blocked=1 publication_blocked=1",
                static_cast<unsigned>(probeResultOk),
                static_cast<unsigned>(routeClaimed),
                static_cast<unsigned>(capability.sync.boardId));
#endif
            g_protocolPresent.store(false, std::memory_order_release);
            g_connected.store(false, std::memory_order_release);
            ClearPublishedValues(false);
            signalInitialAttempt();
            if (!probeResultOk)
                WaitForDeviceChangeAfterDeterministicRejection();
            else
                WaitForReconnect(100);
            continue;
        }

#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        g_diagnosticFailure.store(DiagnosticFailure::None, std::memory_order_relaxed);
        AdvanceDiagnosticProgress(DiagnosticProgress::RouteClaimed);
#endif
        SaveValidatedClaim(session, proof);
        PublishProof(proof);
        g_verifiedLayoutToken.store(
            capability.profile == aula_win60he::CompatibilityProfile::ExactWin60HeMax &&
            capability.sync.boardId == 0x0A021902u ?
                halljoy::layout_identity::Token("aula-rm6x21","0A021902") : 0,
            std::memory_order_release);
        g_candidatePresent.store(true, std::memory_order_release);
        g_protocolPresent.store(true, std::memory_order_release);
        g_connected.store(true, std::memory_order_release);
        g_deviceChanged.store(false, std::memory_order_release);
        if (g_wakeEvent) ResetEvent(g_wakeEvent);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        const std::uint64_t diagnosticSessionId =
            g_diagnosticSessionSequence.fetch_add(1, std::memory_order_relaxed) + 1u;
        aula_win60he::DiagnosticMetrics diagnosticMetrics;
        diagnosticMetrics.Begin(NowUs());
        const wchar_t* diagnosticEndReason = L"shutdown";
        StabilityTrace_Write(L"INFO", L"aula-win60he", L"connected",
            L"session=%llu path_hash=%016llX instance_hash=%016llX mapped=%u precision_um=%u max_um=%u",
            static_cast<unsigned long long>(diagnosticSessionId),
            static_cast<unsigned long long>(HashWideIdentity(session.candidate.path)),
            static_cast<unsigned long long>(HashWideIdentity(session.candidate.instanceId)),
            static_cast<unsigned>(capability.mappedKeys),
            static_cast<unsigned>(capability.precision.precisionUm),
            static_cast<unsigned>(capability.precision.maximumTravelUm));
        if (lastDisconnectUs != 0)
        {
            const std::uint64_t nowUs = NowUs();
            StabilityTrace_Write(L"INFO", L"aula-win60he", L"reconnect.success",
                L"session=%llu downtime_ms=%llu retained_identity=1 strict_proof=1",
                static_cast<unsigned long long>(diagnosticSessionId),
                static_cast<unsigned long long>((nowUs - lastDisconnectUs) / 1000u));
            lastDisconnectUs = 0;
        }
#endif
        DebugLog_Write(
            L"[backend.aula_win60he] connected exclusive instance=%ls path=%ls vid=%04X pid=%04X usage=%04X:%04X mapped=%u precision_um=%u max_um=%u in=%u out=%u",
            session.candidate.instanceId.c_str(),
            session.candidate.path.c_str(),
            static_cast<unsigned>(proof.vendorId),
            static_cast<unsigned>(proof.productId),
            static_cast<unsigned>(proof.usagePage),
            static_cast<unsigned>(proof.usage),
            static_cast<unsigned>(capability.mappedKeys),
            static_cast<unsigned>(capability.precision.precisionUm),
            static_cast<unsigned>(capability.precision.maximumTravelUm),
            static_cast<unsigned>(proof.inputReportBytes),
            static_cast<unsigned>(proof.outputReportBytes));
        signalInitialAttempt();

        ULONGLONG nextActiveMapRefreshMs =
            GetTickCount64() + kActiveMapRefreshIntervalMs;
        bool retryOnlyAfterDeviceChange = false;
        while (!g_stop.load(std::memory_order_acquire))
        {
            if (GetTickCount64() >= nextActiveMapRefreshMs)
            {
                aula_win60he::ActiveMapSnapshot activeMap{};
                failure = aula_win60he::Failure{};
                if (!client.ReadActiveMap(
                        capability.defaultKeyMap, &activeMap, &failure))
                {
                    const bool stopping = g_stop.load(std::memory_order_acquire);
                    if (!stopping)
                        g_failedUpdates.fetch_add(1, std::memory_order_relaxed);
                    DebugLog_WriteBuffered(
                        L"[backend.aula_win60he] active-map refresh failed stage=%u command=%02X selector=%02X index=%u; destroying session",
                        static_cast<unsigned>(failure.stage),
                        static_cast<unsigned>(failure.command),
                        static_cast<unsigned>(failure.selector),
                        static_cast<unsigned>(failure.index));
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
                    RecordDiagnosticProtocolFailure(failure);
                    if (!stopping)
                        RecordDiagnosticFailure(DiagnosticFailure::ActiveMap);
                    diagnosticEndReason = stopping
                        ? L"shutdown_cancel" : L"active_map_failure";
                    if (stopping)
                        StabilityTrace_Write(L"INFO", L"aula-win60he", L"poll.cancelled",
                            L"session=%llu phase=active_map reason=shutdown",
                            static_cast<unsigned long long>(diagnosticSessionId));
                    else
                        TraceCapabilityOutcome(L"active_map_refresh", false, capability, failure);
#endif
                    retryOnlyAfterDeviceChange =
                        IsDeterministicSemanticFailure(failure.stage);
                    break;
                }
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
                if (client.CompatibilityMismatchMask() != 0)
                {
                    RecordDiagnosticFailure(DiagnosticFailure::SemanticMismatch);
                    g_diagnosticLastMismatchMask.store(
                        client.CompatibilityMismatchMask(), std::memory_order_relaxed);
                    diagnosticEndReason = L"semantic_mismatch";
                    StabilityTrace_Write(L"WARN", L"aula-win60he", L"runtime.semantic_mismatch",
                        L"phase=active_map mask=%08X publication_blocked=1 session_reopen=1",
                        static_cast<unsigned>(client.CompatibilityMismatchMask()));
                    retryOnlyAfterDeviceChange = true;
                    break;
                }
#endif

                if (activeMap.functions != capability.activeFunctions ||
                    activeMap.keyMap != capability.keyMap ||
                    activeMap.mappedKeys != capability.mappedKeys)
                {
                    capability.activeFunctions = activeMap.functions;
                    capability.keyMap = activeMap.keyMap;
                    capability.mappedKeys = activeMap.mappedKeys;
                    proof.capability = capability;
                    SaveValidatedClaim(session, proof);
                    PublishProof(proof);
                    DebugLog_WriteBuffered(
                        L"[backend.aula_win60he] active Fn0 map refreshed mapped=%u",
                        static_cast<unsigned>(capability.mappedKeys));
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
                    StabilityTrace_Write(L"INFO", L"aula-win60he", L"active_map.changed",
                        L"mapped=%u", static_cast<unsigned>(capability.mappedKeys));
#endif
                }
                nextActiveMapRefreshMs =
                    GetTickCount64() + kActiveMapRefreshIntervalMs;
            }

            aula_win60he::TravelMatrix matrix{};
            failure = aula_win60he::Failure{};
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            const std::uint64_t transactionBeginUs = NowUs();
#endif
            if (!client.ReadTravelMatrix(capability, &matrix, &failure))
            {
                const bool stopping = g_stop.load(std::memory_order_acquire);
                if (!stopping)
                    g_failedUpdates.fetch_add(1, std::memory_order_relaxed);
                DebugLog_WriteBuffered(
                    L"[backend.aula_win60he] poll failed stage=%u command=%02X selector=%02X index=%u; destroying session before any further request",
                    static_cast<unsigned>(failure.stage),
                    static_cast<unsigned>(failure.command),
                    static_cast<unsigned>(failure.selector),
                    static_cast<unsigned>(failure.index));
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
                RecordDiagnosticProtocolFailure(failure);
                if (!stopping)
                    RecordDiagnosticFailure(DiagnosticFailure::Travel);
                diagnosticEndReason = stopping
                    ? L"shutdown_cancel" : L"travel_failure";
                if (stopping)
                    StabilityTrace_Write(L"INFO", L"aula-win60he", L"poll.cancelled",
                        L"session=%llu phase=travel reason=shutdown failure_stage=%u",
                        static_cast<unsigned long long>(diagnosticSessionId),
                        static_cast<unsigned>(failure.stage));
                else
                    TraceCapabilityOutcome(L"travel_poll", false, capability, failure);
#endif
                retryOnlyAfterDeviceChange =
                    IsDeterministicSemanticFailure(failure.stage);
                break;
            }
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            if (client.CompatibilityMismatchMask() != 0)
            {
                RecordDiagnosticFailure(DiagnosticFailure::SemanticMismatch);
                g_diagnosticLastMismatchMask.store(
                    client.CompatibilityMismatchMask(), std::memory_order_relaxed);
                diagnosticEndReason = L"semantic_mismatch";
                StabilityTrace_Write(L"WARN", L"aula-win60he", L"runtime.semantic_mismatch",
                    L"phase=travel mask=%08X publication_blocked=1 session_reopen=1",
                    static_cast<unsigned>(client.CompatibilityMismatchMask()));
                retryOnlyAfterDeviceChange = true;
                break;
            }
#endif

            [[maybe_unused]] const bool matrixChanged = PublishMatrix(
                capability.keyMap,
                matrix,
                capability.precision.maximumTravelUm);
            RecordSuccessfulMatrix();
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            g_diagnosticFailure.store(DiagnosticFailure::None, std::memory_order_relaxed);
            AdvanceDiagnosticProgress(DiagnosticProgress::MatrixObserved);
            const std::uint64_t completedUs = NowUs();
            const std::uint32_t transactionUs = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(
                    completedUs - transactionBeginUs, 0xffffffffull));
            const auto observation = diagnosticMetrics.Observe(
                capability.keyMap, matrix, matrixChanged, completedUs, transactionUs);
            if (diagnosticMetrics.TotalUpdates() == 1u)
            {
                StabilityTrace_Write(L"INFO", L"aula-win60he", L"matrix.first",
                    L"active_keys=%u mapped=%u",
                    static_cast<unsigned>(g_activeKeys.load(std::memory_order_relaxed)),
                    static_cast<unsigned>(capability.mappedKeys));
            }
            if (observation.firstNonzero || observation.newActiveMaximum ||
                observation.firstTenPlus)
            {
                wchar_t activeValues[1536]{};
                FormatActiveValues(observation, activeValues, _countof(activeValues));
                StabilityTrace_Write(L"INFO", L"aula-win60he", L"matrix.activity",
                    L"session=%llu matrix=%llu first_nonzero=%u new_max=%u first_10plus=%u active_keys=%u min_positive_um=%u max_um=%u values=hid@row,col:um[%ls]",
                    static_cast<unsigned long long>(diagnosticSessionId),
                    static_cast<unsigned long long>(diagnosticMetrics.TotalUpdates()),
                    static_cast<unsigned>(observation.firstNonzero),
                    static_cast<unsigned>(observation.newActiveMaximum),
                    static_cast<unsigned>(observation.firstTenPlus),
                    static_cast<unsigned>(observation.activeCount),
                    static_cast<unsigned>(observation.minimumPositiveUm),
                    static_cast<unsigned>(observation.maximumUm), activeValues);
            }
            if (diagnosticMetrics.WindowReady(completedUs))
            {
                const auto window = diagnosticMetrics.TakeWindow(completedUs);
                TraceDiagnosticWindow(L"matrix.health", diagnosticSessionId,
                    window, diagnosticMetrics.TotalUpdates(),
                    diagnosticMetrics.ObservedHids(),
                    g_failedUpdates.load(std::memory_order_relaxed));
            }
#endif

            if (g_wakeEvent &&
                WaitForSingleObject(g_wakeEvent, kPollPauseMs) == WAIT_OBJECT_0)
            {
                ResetEvent(g_wakeEvent);
                // WM_DEVICECHANGE is global. It wakes an absent-device retry but
                // never tears down a healthy Aula session by itself. Real removal
                // is detected by the next exclusive read/write failure.
                g_deviceChanged.store(false, std::memory_order_release);
            }
        }

#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        const std::uint64_t diagnosticEndUs = NowUs();
        const auto diagnosticLifetime = diagnosticMetrics.Lifetime(diagnosticEndUs);
        TraceDiagnosticWindow(L"matrix.session_summary", diagnosticSessionId,
            diagnosticLifetime, diagnosticMetrics.TotalUpdates(),
            diagnosticMetrics.ObservedHids(),
            g_failedUpdates.load(std::memory_order_relaxed));
        TraceDiagnosticCoverage(diagnosticSessionId, diagnosticMetrics);
        const std::uint32_t diagnosticActiveBeforeClear =
            diagnosticLifetime.currentActiveKeys;
#endif

        g_protocolPresent.store(false, std::memory_order_release);
        g_connected.store(false, std::memory_order_release);
        ClearPublishedValues(false);

#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        if (!g_stop.load(std::memory_order_acquire))
        {
            lastDisconnectUs = diagnosticEndUs;
            StabilityTrace_Write(L"WARN", L"aula-win60he", L"disconnected",
                L"session=%llu reason=%ls reconnect_pending=1 active_before_clear=%u published_active_after_clear=%u",
                static_cast<unsigned long long>(diagnosticSessionId),
                diagnosticEndReason,
                static_cast<unsigned>(diagnosticActiveBeforeClear),
                static_cast<unsigned>(g_activeKeys.load(std::memory_order_relaxed)));
        }
        else
        {
            StabilityTrace_Write(L"INFO", L"aula-win60he", L"session.closed",
                L"session=%llu reason=%ls reconnect_pending=0",
                static_cast<unsigned long long>(diagnosticSessionId),
                diagnosticEndReason);
        }
#endif
        if (!g_stop.load(std::memory_order_acquire))
        {
            if (retryOnlyAfterDeviceChange)
                WaitForDeviceChangeAfterDeterministicRejection();
            else
                WaitForReconnect(100);
        }
    }

    g_connected.store(false, std::memory_order_release);
    g_candidatePresent.store(false, std::memory_order_release);
    g_protocolPresent.store(false, std::memory_order_release);
    signalInitialAttempt();
    ClearPublishedValues(false);
    g_running.store(false, std::memory_order_release);
    return 0u;
}

void WorkerOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_workerFaultRecord = record;
    g_workerFaultKind.store(record.kind, std::memory_order_release);
    g_stop.store(true, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    g_protocolPresent.store(false, std::memory_order_release);
    ClearPublishedValues(false);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    RecordDiagnosticFailure(DiagnosticFailure::WorkerFault);
#endif
    StabilityTrace_WriteCritical(L"ERROR", L"aula-win60he", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));
}

void WorkerOnCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_running.store(false, std::memory_order_release);
    StabilityTrace_Write(
        record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"aula-win60he", L"worker.exit", L"fault_kind=%u",
        static_cast<unsigned>(record.kind));
}

unsigned __stdcall WorkerEntry(void*) noexcept
{
    return static_cast<unsigned>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return WorkerMain(); },
        WorkerOnFault,
        WorkerOnCompletion,
        0xA0600001u));
}

void FillTelemetry(NativeAnalogBackendTelemetry* out)
{
    const auto identity = g_verifiedLayoutToken.load(std::memory_order_acquire);
    if (!out)
        return;
    *out = NativeAnalogBackendTelemetry{};
    out->present = g_candidatePresent.load(std::memory_order_acquire);
    out->connected = g_connected.load(std::memory_order_acquire);
    if (out->connected && identity == g_verifiedLayoutToken.load(std::memory_order_acquire))
        out->verifiedLayoutToken = identity;
    out->vendorId = g_vendorId.load(std::memory_order_relaxed);
    out->productId = g_productId.load(std::memory_order_relaxed);
    out->usagePage = g_usagePage.load(std::memory_order_relaxed);
    out->usage = aula_win60he::kAulaUsage;
    out->mappedKeys = g_mappedKeys.load(std::memory_order_relaxed);
    out->activeKeys = g_activeKeys.load(std::memory_order_relaxed);
    const auto precision = g_precisionUm.load(std::memory_order_relaxed);
    const auto maximum = g_maximumTravelUm.load(std::memory_order_relaxed);
    out->nominalRawLevels = precision == 0 ? 0u :
        static_cast<std::uint32_t>(maximum / precision) + 1u;
    out->inputReportBytes = g_inputReportBytes.load(std::memory_order_relaxed);
    out->outputReportBytes = g_outputReportBytes.load(std::memory_order_relaxed);
    out->averageIntervalUs = g_averageIntervalUs.load(std::memory_order_relaxed);
    out->maximumIntervalUs = g_maximumIntervalUs.load(std::memory_order_relaxed);
    if (out->averageIntervalUs != 0)
    {
        out->updateHz10 = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            1000000ull,
            (10000000ull + out->averageIntervalUs / 2u) / out->averageIntervalUs));
    }
    const ULONGLONG last = g_lastUpdateMs.load(std::memory_order_relaxed);
    const ULONGLONG now = GetTickCount64();
    if (last != 0 && now >= last)
    {
        out->lastUpdateAgeMs = static_cast<std::uint32_t>(
            std::min<ULONGLONG>(now - last, 0xffffffffull));
    }
    out->successfulUpdates = g_successfulUpdates.load(std::memory_order_relaxed);
    out->failedUpdates = g_failedUpdates.load(std::memory_order_relaxed);
    if (out->connected)
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"exclusive same-handle proof/poll, active Fn0 map, %u mapped, %u um max",
            static_cast<unsigned>(out->mappedKeys),
            static_cast<unsigned>(maximum));
    }
    else if (g_ambiguousSelection.load(std::memory_order_relaxed))
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"multiple Aula-family candidates (%u); selection is fail-closed",
            static_cast<unsigned>(g_candidateCount.load(std::memory_order_relaxed)));
    }
    else if (g_invalidEnumeration.load(std::memory_order_relaxed))
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"contradictory SetupAPI identity evidence; device selection is fail-closed");
    }
    else if (g_retainedIdentityMissing.load(std::memory_order_relaxed))
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"retained Aula identity is absent; restart backend to choose another unit");
    }
    else if (g_firmwareSerialMismatch.load(std::memory_order_relaxed))
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"Aula firmware serial differs from retained proof identity");
    }
    else if (out->present &&
        g_lastOpenError.load(std::memory_order_relaxed) == ERROR_SHARING_VIOLATION)
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"Aula vendor HID is busy; waiting for exclusive access");
    }
    else if (out->present)
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"Aula-family candidate present; waiting for capability proof (error %u)",
            static_cast<unsigned>(g_lastOpenError.load(std::memory_order_relaxed)));
    }
    else
    {
        _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
            L"waiting for Aula/SparkPlayJoy 6x21 protocol family");
    }
}
}

bool AulaWin60He_PrepareProtocolRouting()
{
    if (g_routingPrepared.load(std::memory_order_acquire))
    {
        const RetainedDeviceIdentity retained = PreferredIdentity();
        if (retained.valid && NativeAnalogRouting_IsClaimedBy(
                retained.path.c_str(), NativeAnalogProtocol::AulaWin60He))
            return true;
        g_routingPrepared.store(false, std::memory_order_release);
    }

    // The current routing contract forbids VID/PID reservations. Prove the
    // complete read-only protocol on one exclusive exact interface first, then
    // publish only that path to the UAP pre-open exclusion registry.
    Session session{};
    bool proven = false;
    if (OpenSelectedSession(&session))
    {
        WindowsReportTransport transport(session);
        ClientTraceContext traceContext{};
        traceContext.maximum = 256;
        aula_win60he::Client client(transport, MakeTraceSink(&traceContext));
        aula_win60he::CapabilityProof capability{};
        aula_win60he::Failure failure{};
        ProbeResult proof{};
        const bool probeOk = client.Probe(
            &capability, &failure, ProbePolicyForSession(session));
        TraceCapabilityOutcome(L"pre_uap", probeOk, capability, failure);
        const bool retainedIdentityOk =
            probeOk && MatchesRetainedProofIdentity(session, capability);
        const bool probeResultOk =
            retainedIdentityOk && BuildProbeResult(session, capability, &proof);
        const bool routeClaimed = probeOk &&
            capability.compatibilityMismatchMask == 0 &&
            probeResultOk && NativeAnalogRouting_Claim(
                session.candidate.attributes.VendorID,
                session.candidate.attributes.ProductID,
                session.candidate.path.c_str(),
                NativeAnalogProtocol::AulaWin60He);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        g_diagnosticLastBoardId.store(
            capability.sync.boardId, std::memory_order_relaxed);
        g_diagnosticLastMismatchMask.store(
            capability.compatibilityMismatchMask, std::memory_order_relaxed);
        if (probeOk)
            AdvanceDiagnosticProgress(DiagnosticProgress::CapabilityProved);
        else
        {
            RecordDiagnosticFailure(DiagnosticFailure::CapabilityProof);
            RecordDiagnosticProtocolFailure(failure);
        }
#endif
        if (routeClaimed)
        {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            g_diagnosticFailure.store(DiagnosticFailure::None, std::memory_order_relaxed);
            AdvanceDiagnosticProgress(DiagnosticProgress::RouteClaimed);
#endif
            SaveValidatedClaim(session, proof);
            PublishProof(proof);
            g_candidatePresent.store(true, std::memory_order_release);
            g_protocolPresent.store(true, std::memory_order_release);
            proven = true;
            StabilityTrace_Write(L"INFO", L"aula-win60he", L"routing.claimed",
                L"vid=%04X pid=%04X usage=%04X:%04X mapped=%u",
                static_cast<unsigned>(proof.vendorId),
                static_cast<unsigned>(proof.productId),
                static_cast<unsigned>(proof.usagePage),
                static_cast<unsigned>(proof.usage),
                static_cast<unsigned>(proof.capability.mappedKeys));
        }
        else
        {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            if (probeOk && capability.compatibilityMismatchMask != 0)
                RecordDiagnosticFailure(DiagnosticFailure::SemanticMismatch);
            else if (probeOk && !retainedIdentityOk)
                RecordDiagnosticFailure(DiagnosticFailure::RetainedProofMismatch);
            else if (probeOk && !probeResultOk)
                RecordDiagnosticFailure(DiagnosticFailure::ProbeResult);
            else if (probeOk)
                RecordDiagnosticFailure(DiagnosticFailure::RouteClaim);
#endif
            g_failedUpdates.fetch_add(1, std::memory_order_relaxed);
            g_protocolPresent.store(false, std::memory_order_release);
            DebugLog_WriteBuffered(
                L"[backend.aula_win60he] pre-UAP capability proof rejected stage=%u command=%02X selector=%02X index=%u",
                static_cast<unsigned>(failure.stage),
                static_cast<unsigned>(failure.command),
                static_cast<unsigned>(failure.selector),
                static_cast<unsigned>(failure.index));
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            StabilityTrace_Write(L"WARN", L"aula-win60he", L"routing.not_claimed",
                L"probe_ok=%u mismatch_mask=%08X retained_identity_ok=%u fingerprint_ok=%u failure_stage=%u command=%02X selector=%02X index=%u",
                static_cast<unsigned>(probeOk),
                static_cast<unsigned>(capability.compatibilityMismatchMask),
                static_cast<unsigned>(retainedIdentityOk),
                static_cast<unsigned>(probeResultOk),
                static_cast<unsigned>(failure.stage),
                static_cast<unsigned>(failure.command),
                static_cast<unsigned>(failure.selector),
                static_cast<unsigned>(failure.index));
#endif
        }
    }

    g_routingPrepared.store(true, std::memory_order_release);
    return proven;
}

bool AulaWin60He_Start()
{
    std::lock_guard<std::mutex> serviceLock(g_serviceMutex);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    ResetDiagnosticOutcome();
    StabilityTrace_Write(L"WARN", L"aula-win60he", L"diagnostic.enabled",
        L"schema=3 strict_claim_required=1 strict_publication_required=1 proof_raw_reports_capped=1 serial_redacted=1 health_window_ms=5000 activity_snapshots=1 ten_key_gate=1 per_hid_coverage=1 reconnect_timeline=1 shutdown_cancellation_classified=1 conclusive_verdict=1 expected_profiles=1CA5:2201/16052201,1CA5:2202/16052202,1CA2:2201/2E022201");
#endif
    if (!g_routingPrepared.load(std::memory_order_acquire))
        (void)AulaWin60He_PrepareProtocolRouting();
    if (g_threadHandle)
        return g_running.load(std::memory_order_acquire);

    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return true;

    g_stop.store(false, std::memory_order_release);
    g_deviceChanged.store(false, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    g_ambiguousSelection.store(false, std::memory_order_relaxed);
    g_invalidEnumeration.store(false, std::memory_order_relaxed);
    g_retainedIdentityMissing.store(false, std::memory_order_relaxed);
    g_firmwareSerialMismatch.store(false, std::memory_order_relaxed);
    g_candidateCount.store(0, std::memory_order_relaxed);
    g_lastOpenError.store(ERROR_SUCCESS, std::memory_order_relaxed);
    g_successfulUpdates.store(0, std::memory_order_relaxed);
    g_failedUpdates.store(0, std::memory_order_relaxed);
    g_lastMatrixUs.store(0, std::memory_order_relaxed);
    g_averageIntervalUs.store(0, std::memory_order_relaxed);
    g_maximumIntervalUs.store(0, std::memory_order_relaxed);
    g_lastUpdateMs.store(0, std::memory_order_relaxed);
    g_workerFaultRecord = {};
    g_workerFaultKind.store(halljoy::worker::WorkerExceptionKind::None,
        std::memory_order_release);

    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        DWORD resourceError = ERROR_SUCCESS;
        g_wakeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!g_wakeEvent)
            resourceError = GetLastError();
        g_initialAttemptEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!g_initialAttemptEvent && resourceError == ERROR_SUCCESS)
            resourceError = GetLastError();
        if (!g_wakeEvent || !g_initialAttemptEvent)
        {
            if (g_wakeEvent) CloseHandle(g_wakeEvent);
            if (g_initialAttemptEvent) CloseHandle(g_initialAttemptEvent);
            g_wakeEvent = nullptr;
            g_initialAttemptEvent = nullptr;
            g_running.store(false, std::memory_order_release);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
            RecordDiagnosticFailure(DiagnosticFailure::StartResources);
            StabilityTrace_Write(L"ERROR", L"aula-win60he", L"start.failed",
                L"stage=events native_error=%u", static_cast<unsigned>(resourceError));
            TraceDiagnosticVerdict();
#endif
            return false;
        }
    }

    unsigned threadId = 0;
    const uintptr_t thread = _beginthreadex(
        nullptr, 0, WorkerEntry, nullptr, 0, &threadId);
    if (thread == 0)
    {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        const DWORD threadError = static_cast<DWORD>(errno);
#endif
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        CloseHandle(g_wakeEvent);
        CloseHandle(g_initialAttemptEvent);
        g_wakeEvent = nullptr;
        g_initialAttemptEvent = nullptr;
        g_running.store(false, std::memory_order_release);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::StartThread);
        StabilityTrace_Write(L"ERROR", L"aula-win60he", L"start.failed",
            L"stage=worker_thread native_error=%u",
            static_cast<unsigned>(threadError));
        TraceDiagnosticVerdict();
#endif
        return false;
    }
    g_threadHandle = reinterpret_cast<HANDLE>(thread);
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"worker.start",
        L"thread_id=%u", threadId);

    // Backend_Init queries proof-only presence immediately after this start
    // phase. Wait for exactly one bounded enumeration/proof attempt so an Aula
    // already attached at process start cannot lose a race against optional UAP
    // initialisation. Absence/open failure is signalled promptly.
    const DWORD initialWait = WaitForSingleObject(
        g_initialAttemptEvent, kInitialAttemptWaitMs);
    if (initialWait == WAIT_TIMEOUT)
    {
        DebugLog_WriteBuffered(
            L"[backend.aula_win60he] initial discovery/proof gate timed out; continuing worker in retry mode");
    }
    else if (initialWait == WAIT_FAILED)
    {
        StabilityTrace_Write(L"ERROR", L"aula-win60he", L"start.initial_wait_failed",
            L"native_error=%u worker_continues=1",
            static_cast<unsigned>(GetLastError()));
    }
    if (WaitForSingleObject(g_threadHandle, 0) == WAIT_OBJECT_0 &&
        !g_running.load(std::memory_order_acquire))
        return false;
    return true;
}

halljoy::lifecycle::StopResult AulaWin60He_Stop(
    halljoy::lifecycle::GenerationId generation)
{
    std::lock_guard<std::mutex> serviceLock(g_serviceMutex);
    if (!g_threadHandle)
    {
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        TraceDiagnosticVerdict();
#endif
        return NativeAnalogBackendStopJoined(generation);
    }

    g_stop.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        if (g_wakeEvent) SetEvent(g_wakeEvent);
    }
    {
        std::lock_guard<std::mutex> activeLock(g_activeSessionMutex);
        if (g_activeSessionHandle && g_activeSessionHandle != INVALID_HANDLE_VALUE)
            (void)CancelIoEx(g_activeSessionHandle, nullptr);
    }

    const DWORD wait = WaitForSingleObject(g_threadHandle, kStopJoinTimeoutMs);
    if (wait != WAIT_OBJECT_0)
    {
        const DWORD error = wait == WAIT_TIMEOUT ? WAIT_TIMEOUT : GetLastError();
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        RecordDiagnosticFailure(DiagnosticFailure::StopIncomplete);
#endif
        StabilityTrace_WriteCritical(L"ERROR", L"aula-win60he", L"stop.incomplete",
            L"wait=%lu native_error=%lu resources_retained=1",
            static_cast<unsigned long>(wait), static_cast<unsigned long>(error));
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
        TraceDiagnosticVerdict();
#endif
        return halljoy::lifecycle::ObserveWorkerJoin(
            generation,
            wait == WAIT_TIMEOUT
                ? halljoy::lifecycle::JoinWaitStatus::TimedOut
                : halljoy::lifecycle::JoinWaitStatus::Failed,
            error);
    }

    CloseHandle(g_threadHandle);
    g_threadHandle = nullptr;
    {
        std::lock_guard<std::mutex> signalLock(g_signalMutex);
        if (g_wakeEvent) CloseHandle(g_wakeEvent);
        if (g_initialAttemptEvent) CloseHandle(g_initialAttemptEvent);
        g_wakeEvent = nullptr;
        g_initialAttemptEvent = nullptr;
    }
    g_running.store(false, std::memory_order_release);
    g_candidatePresent.store(false, std::memory_order_release);
    g_protocolPresent.store(false, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    ClearPublishedValues(false);
#if defined(HALLJOY_AULA_AGGRESSIVE_TRACE)
    TraceDiagnosticVerdict();
#endif
    StabilityTrace_Write(L"INFO", L"aula-win60he", L"stop.joined");
    return NativeAnalogBackendStopJoined(generation);
}

void AulaWin60He_NotifyDeviceChange()
{
    g_deviceChanged.store(true, std::memory_order_release);
    std::unique_lock<std::mutex> signalLock(g_signalMutex, std::try_to_lock);
    if (signalLock.owns_lock() && g_wakeEvent)
        (void)SetEvent(g_wakeEvent);
}

bool AulaWin60He_IsProtocolDevicePresent()
{
    // Only a completed capability proof on the live exclusive handle is
    // protocol presence. Candidate discovery remains telemetry, not trust.
    return g_protocolPresent.load(std::memory_order_acquire);
}

bool AulaWin60He_HasDiscoveryCandidate()
{
    if (g_protocolPresent.load(std::memory_order_acquire) ||
        g_connected.load(std::memory_order_acquire))
        return true;

    const EnumerationResult enumeration = EnumerateCandidates();
    const bool candidate = !enumeration.candidates.empty();
    g_candidatePresent.store(candidate, std::memory_order_release);
    return candidate;
}

bool AulaWin60He_IsConnected()
{
    return g_connected.load(std::memory_order_acquire);
}

bool AulaWin60He_OwnsHid(std::uint16_t hidUsage)
{
    return hidUsage < g_owned.size() &&
        g_connected.load(std::memory_order_acquire) &&
        g_owned[hidUsage].load(std::memory_order_relaxed) != 0;
}

std::uint16_t AulaWin60He_GetMilli(std::uint16_t hidUsage)
{
    if (!AulaWin60He_OwnsHid(hidUsage))
        return 0;
    return g_milli[hidUsage].load(std::memory_order_relaxed);
}

const NativeAnalogBackendDescriptor& AulaWin60He_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "aula-sparkplayjoy-6x21",
        L"SparkPlayJoy 6x21 (Aula / GravaStar)",
        NativeAnalogProtocol::AulaWin60He,
        NativeAnalogStartPhase::BeforeUap,
        NativeAnalogBackendFlag_PolledTransport |
            NativeAnalogBackendFlag_ReadOnlyProbe,
        &AulaWin60He_PrepareProtocolRouting,
        &AulaWin60He_Start,
        &AulaWin60He_Stop,
        &AulaWin60He_NotifyDeviceChange,
        &AulaWin60He_IsProtocolDevicePresent,
        &AulaWin60He_IsConnected,
        &AulaWin60He_OwnsHid,
        &AulaWin60He_GetMilli,
        &FillTelemetry,
    };
    return descriptor;
}
