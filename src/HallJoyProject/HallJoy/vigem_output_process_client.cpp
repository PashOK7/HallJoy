#include "vigem_output_process_client.h"

#include "windows_command_line.h"

#include <array>
#include <new>
#include <string_view>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace halljoy::vigem_output
{
namespace
{
#if defined(_WIN32)
struct ProbeContext
{
    SharedStateV1* shared = nullptr;
    std::uint64_t generation = 0;
};

struct PrepareTerminationContext
{
    SharedStateV1* shared = nullptr;
    std::uint64_t generation = 0;
    bool generationDisabled = false;
};

bool PrepareOutputTermination(void* rawContext,
    std::uint32_t& error) noexcept
{
    auto* context = static_cast<PrepareTerminationContext*>(rawContext);
    if (!context || !context->shared || context->generation == 0u)
    {
        error = ERROR_INVALID_PARAMETER;
        return false;
    }

    const std::uint64_t active = ActiveOutputGeneration(*context->shared);
    context->generationDisabled = active == 0u ||
        (active == context->generation &&
            DisableOutputGeneration(*context->shared, context->generation));
    if (!context->generationDisabled)
    {
        error = ERROR_INVALID_STATE;
        return false;
    }

    // This callback is the pre-stop admission barrier. Once the generation is
    // disabled, an in-flight producer can no longer commit a value to it.
    // Draining that already-started producer belongs after child reap, before
    // the mapping can be reused or closed; conflating the two boundaries makes
    // a harmless preemption look like an unsafe child stop.
    error = ERROR_SUCCESS;
    return true;
}

bool ProbeChild(void* rawContext,
    process::ChildObservation& observation) noexcept
{
    auto* context = static_cast<ProbeContext*>(rawContext);
    if (!context || !context->shared || context->generation == 0u)
        return false;

    ChildTelemetrySnapshotV1 telemetry{};
    if (!ReadChildTelemetry(*context->shared, telemetry) ||
        telemetry.childGeneration != context->generation)
    {
        return true; // no complete observation for this generation yet
    }

    observation.available = true;
    observation.childPid = telemetry.childPid;
    observation.generation = telemetry.childGeneration;
    observation.progress = telemetry.progressSequence;
    const auto stateValue = static_cast<std::uint32_t>(telemetry.state);
    if (stateValue > static_cast<std::uint32_t>(ChildState::Failed))
    {
        observation.fatal = true;
        observation.error = ERROR_INVALID_DATA;
        return true;
    }
    observation.ready = telemetry.readyGeneration == context->generation &&
        telemetry.state != ChildState::Starting;
    observation.fatal = telemetry.state == ChildState::Failed;
    observation.error = telemetry.lastError;
    return true;
}

std::wstring HexValue(std::uint64_t value)
{
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%llX", static_cast<unsigned long long>(value));
    return buffer;
}

std::wstring HandleValue(process::NativeHandle handle)
{
    return HexValue(static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(handle)));
}

std::wstring BuildHostCommandLine(const OutputGenerationRequest& request,
    std::uint32_t ownerPid, std::uint64_t nonce,
    process::NativeHandle mapping, process::NativeHandle wakeEvent,
    process::NativeHandle childStopEvent,
    process::NativeHandle ownerProcess)
{
    const std::wstring_view modeToken = OutputHostModeToken(request.mode);
    if (modeToken.empty())
        return {};

    std::wstring command = windows_command_line::QuoteArgument(
        request.applicationPath);
    command += L" ";
    command += kOutputHostArgument;
    command += L" ";
    command += std::to_wstring(ownerPid);
    command += L" ";
    command += HexValue(nonce);
    command += L" ";
    command += std::to_wstring(request.generation);
    command += L" ";
    command += HandleValue(mapping);
    command += L" ";
    command += HandleValue(wakeEvent);
    command += L" ";
    command += HandleValue(childStopEvent);
    command += L" ";
    command += HandleValue(ownerProcess);
    command += L" ";
    command += modeToken;
    return command;
}

OutputGenerationOutcome MapProcessOutcome(
    process::GenerationOutcome outcome) noexcept
{
    switch (outcome)
    {
    case process::GenerationOutcome::PlannedStop:
        return OutputGenerationOutcome::IncompletePlannedStop;
    case process::GenerationOutcome::ExitBeforeReady:
        return OutputGenerationOutcome::ExitBeforeReady;
    case process::GenerationOutcome::UnexpectedExit:
        return OutputGenerationOutcome::UnexpectedExit;
    case process::GenerationOutcome::StartupTimeout:
        return OutputGenerationOutcome::StartupTimeout;
    case process::GenerationOutcome::ProgressTimeout:
        return OutputGenerationOutcome::ProgressTimeout;
    case process::GenerationOutcome::ChildFault:
        return OutputGenerationOutcome::ChildFault;
    case process::GenerationOutcome::ProtocolViolation:
        return OutputGenerationOutcome::ProtocolViolation;
    case process::GenerationOutcome::LaunchFailed:
        return OutputGenerationOutcome::LaunchFailed;
    case process::GenerationOutcome::WaitFailed:
        return OutputGenerationOutcome::WaitFailed;
    case process::GenerationOutcome::ReapFailed:
        return OutputGenerationOutcome::ReapFailed;
    case process::GenerationOutcome::Unsupported:
        return OutputGenerationOutcome::Unsupported;
    }
    return OutputGenerationOutcome::SessionFailure;
}

bool MakeLaunchNonce(std::uint64_t& nonce) noexcept
{
    nonce = 0u;
    const NTSTATUS status = BCryptGenRandom(nullptr,
        reinterpret_cast<PUCHAR>(&nonce), static_cast<ULONG>(sizeof(nonce)),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(status) && nonce != 0u;
}
#endif
} // namespace

OutputProcessSession::OutputProcessSession() noexcept = default;

OutputProcessSession::~OutputProcessSession() noexcept
{
#if defined(_WIN32)
    if (supervisor_ && supervisor_->HasUnreapedGeneration())
        supervisor_.reset(); // containment is destroyed before IPC resources
    if (shared_ && ActiveOutputGeneration(*shared_) != 0u)
    {
        const std::uint64_t generation = ActiveOutputGeneration(*shared_);
        (void)DisableOutputGeneration(*shared_, generation);
    }
#endif
    supervisor_.reset();
    CloseResources();
}

void OutputProcessSession::CloseResources() noexcept
{
#if defined(_WIN32)
    if (shared_)
        UnmapViewOfFile(shared_);
    if (ownerProcess_)
        CloseHandle(ownerProcess_);
    if (childStopEvent_)
        CloseHandle(childStopEvent_);
    if (wakeEvent_)
        CloseHandle(wakeEvent_);
    if (mapping_)
        CloseHandle(mapping_);
#endif
    shared_ = nullptr;
    ownerProcess_ = nullptr;
    childStopEvent_ = nullptr;
    wakeEvent_ = nullptr;
    mapping_ = nullptr;
    ownerPid_ = 0u;
    launchNonce_ = 0u;
    initialized_ = false;
}

bool OutputProcessSession::Initialize(std::uint32_t& error) noexcept
{
#if !defined(_WIN32)
    error = 0u;
    return false;
#else
    error = ERROR_SUCCESS;
    if (initialized_ || shared_ || mapping_ || supervisor_)
    {
        error = ERROR_ALREADY_INITIALIZED;
        return false;
    }

    auto* supervisor = new (std::nothrow) process::ProcessGenerationSupervisor();
    if (!supervisor)
    {
        error = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }
    supervisor_.reset(supervisor);

    ownerPid_ = GetCurrentProcessId();
    if (ownerPid_ == 0u || !MakeLaunchNonce(launchNonce_))
    {
        error = ERROR_GEN_FAILURE;
        supervisor_.reset();
        CloseResources();
        return false;
    }

    SECURITY_ATTRIBUTES inherited{};
    inherited.nLength = sizeof(inherited);
    inherited.bInheritHandle = TRUE;
    mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, &inherited,
        PAGE_READWRITE, 0, static_cast<DWORD>(sizeof(SharedStateV1)), nullptr);
    if (!mapping_)
        error = GetLastError();
    if (error == ERROR_SUCCESS)
    {
        shared_ = static_cast<SharedStateV1*>(MapViewOfFile(mapping_,
            FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedStateV1)));
        if (!shared_)
            error = GetLastError();
    }
    if (error == ERROR_SUCCESS)
    {
        wakeEvent_ = CreateEventW(&inherited, FALSE, FALSE, nullptr);
        if (!wakeEvent_)
            error = GetLastError();
    }
    if (error == ERROR_SUCCESS)
    {
        childStopEvent_ = CreateEventW(&inherited, TRUE, FALSE, nullptr);
        if (!childStopEvent_)
            error = GetLastError();
    }
    if (error == ERROR_SUCCESS && !DuplicateHandle(GetCurrentProcess(),
        GetCurrentProcess(), GetCurrentProcess(), &ownerProcess_,
        SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, TRUE, 0))
    {
        error = GetLastError();
    }
    if (error != ERROR_SUCCESS)
    {
        supervisor_.reset();
        CloseResources();
        return false;
    }

    InitializeSharedState(*shared_, ownerPid_, launchNonce_);
    initialized_ = true;
    restartSafe_.store(true, std::memory_order_release);
    return true;
#endif
}

bool OutputProcessSession::Shutdown(std::uint32_t& error) noexcept
{
#if !defined(_WIN32)
    error = 0u;
    return false;
#else
    error = ERROR_SUCCESS;
    if (!initialized_)
        return true;
    if (!supervisor_ || supervisor_->HasUnreapedGeneration())
    {
        error = ERROR_BUSY;
        restartSafe_.store(false, std::memory_order_release);
        return false;
    }
    if (ActiveOutputGeneration(*shared_) != 0u ||
        !PublicationQuiescent(*shared_))
    {
        error = ERROR_BUSY;
        restartSafe_.store(false, std::memory_order_release);
        return false;
    }
    supervisor_.reset();
    CloseResources();
    restartSafe_.store(true, std::memory_order_release);
    return true;
#endif
}

OutputGenerationResult OutputProcessSession::RunOne(
    const OutputGenerationRequest& request) noexcept
{
    OutputGenerationResult result{};
#if !defined(_WIN32)
    (void)request;
    result.outcome = OutputGenerationOutcome::Unsupported;
    return result;
#else
    try
    {
        if (!initialized_ || !restartSafe_.load(std::memory_order_acquire) ||
            !supervisor_ || !shared_ ||
            request.generation == 0u || request.padCount == 0u ||
            request.padCount > kMaxPads || request.applicationPath.empty())
        {
            result.nativeError = ERROR_INVALID_STATE;
            result.restartSafe = false;
            return result;
        }
#if !defined(HALLJOY_ANALOG_SIMULATOR)
        if (request.mode != OutputHostMode::Real)
        {
            result.outcome = OutputGenerationOutcome::Unsupported;
            result.nativeError = ERROR_NOT_SUPPORTED;
            result.restartSafe = true;
            return result;
        }
#endif

    const std::wstring commandLine = BuildHostCommandLine(request, ownerPid_,
        launchNonce_, mapping_, wakeEvent_, childStopEvent_, ownerProcess_);
    if (commandLine.empty())
    {
        result.nativeError = ERROR_INVALID_PARAMETER;
        result.restartSafe = true;
        return result;
    }

    if (!ResetEvent(childStopEvent_) || !ResetEvent(wakeEvent_))
    {
        result.nativeError = GetLastError();
        restartSafe_.store(false, std::memory_order_release);
        result.restartSafe = false;
        return result;
    }
    if (!BeginOutputGeneration(*shared_, request.generation, request.padCount))
    {
        result.nativeError = ERROR_INVALID_STATE;
        restartSafe_.store(false, std::memory_order_release);
        result.restartSafe = false;
        return result;
    }

    ProbeContext probe{ shared_, request.generation };
    PrepareTerminationContext prepare{ shared_, request.generation };
    process::SupervisionRequest supervision{};
    supervision.launch.applicationPath = request.applicationPath;
    supervision.launch.commandLine = commandLine;
    supervision.launch.inheritedHandles = {
        mapping_, wakeEvent_, childStopEvent_, ownerProcess_
    };
    supervision.launch.generation = request.generation;
    supervision.launch.creationFlags = CREATE_NO_WINDOW;
    supervision.childStopEvent = childStopEvent_;
    supervision.supervisorStopEvent = request.supervisorStopEvent;
    supervision.probe = &ProbeChild;
    supervision.probeContext = &probe;
    supervision.prepareTermination = &PrepareOutputTermination;
    supervision.prepareTerminationContext = &prepare;
    supervision.policy = request.policy;

    result.process = supervisor_->RunOne(supervision);
    ChildTelemetrySnapshotV1 finalTelemetry{};
    if (ReadChildTelemetry(*shared_, finalTelemetry) &&
        finalTelemetry.childGeneration == request.generation)
    {
        result.finalTelemetry = finalTelemetry;
        result.finalTelemetryAvailable = true;
    }

    if (result.process.terminationPreparationAttempted)
    {
        result.generationDisabled = prepare.generationDisabled;
    }
    else
    {
        const std::uint64_t active = ActiveOutputGeneration(*shared_);
        result.generationDisabled = active == 0u ||
            (active == request.generation &&
                DisableOutputGeneration(*shared_, request.generation));
    }

    // The child is now reaped and the generation is closed. Only resource
    // reuse/close depends on the bounded producer drain; it is deliberately
    // not part of the child's neutral/remove stop deadline.
    const ULONGLONG quiescenceDeadline = GetTickCount64() + 1000u;
    while (!PublicationQuiescent(*shared_) &&
        GetTickCount64() < quiescenceDeadline)
    {
        SwitchToThread();
    }
    result.publicationQuiescent = PublicationQuiescent(*shared_);

    if (result.finalTelemetryAvailable)
    {
        result.neutralApplied =
            (result.finalTelemetry.diagnosticFlags &
                kChildDiagnosticNeutralApplied) != 0u;
        result.targetsRemoved =
            (result.finalTelemetry.diagnosticFlags &
                kChildDiagnosticTargetsRemoved) != 0u;
        result.cleanStopConfirmed =
            result.finalTelemetry.completedStopGeneration == request.generation &&
            result.finalTelemetry.state == ChildState::Stopped &&
            result.neutralApplied && result.targetsRemoved;
    }

    result.outcome = MapProcessOutcome(result.process.outcome);
    if (result.process.outcome == process::GenerationOutcome::PlannedStop)
    {
        result.outcome = result.cleanStopConfirmed &&
            result.process.terminationPrepared
            ? OutputGenerationOutcome::CleanPlannedStop
            : OutputGenerationOutcome::IncompletePlannedStop;
    }
    else if (result.process.outcome ==
            process::GenerationOutcome::ExitBeforeReady &&
        result.finalTelemetryAvailable &&
        result.finalTelemetry.readyGeneration == request.generation)
    {
        // Process signalling can win the supervisor's observation poll after
        // the child committed Ready. The stable post-reap telemetry is the
        // authoritative output-protocol boundary for this classification.
        result.outcome = OutputGenerationOutcome::UnexpectedExit;
    }

    result.nativeError = result.process.nativeError;
    result.restartSafe = result.process.restartSafe &&
        result.generationDisabled && result.publicationQuiescent &&
        supervisor_->RestartSafe();
    if (!result.restartSafe)
        restartSafe_.store(false, std::memory_order_release);
    return result;
    }
    catch (const std::bad_alloc&)
    {
        result.nativeError = ERROR_NOT_ENOUGH_MEMORY;
    }
    catch (...)
    {
        result.nativeError = ERROR_UNHANDLED_EXCEPTION;
    }

    result.outcome = OutputGenerationOutcome::SessionFailure;
    const std::uint64_t active = shared_ ? ActiveOutputGeneration(*shared_) : 0u;
    result.generationDisabled = active == 0u ||
        (active == request.generation &&
            DisableOutputGeneration(*shared_, request.generation));
    result.publicationQuiescent = shared_ && PublicationQuiescent(*shared_);
    result.restartSafe = supervisor_ && supervisor_->RestartSafe() &&
        result.generationDisabled && result.publicationQuiescent;
    if (!result.restartSafe)
        restartSafe_.store(false, std::memory_order_release);
    return result;
#endif
}

OutputPublishResult OutputProcessSession::TryPublish(
    const XusbReportV1* reports, std::uint32_t padCount,
    std::uint32_t validMask, std::uint64_t timestampUs,
    std::uint64_t* publicationSequence) noexcept
{
#if !defined(_WIN32)
    (void)reports;
    (void)padCount;
    (void)validMask;
    (void)timestampUs;
    (void)publicationSequence;
    return OutputPublishResult::SessionUnavailable;
#else
    if (!initialized_ || !restartSafe_.load(std::memory_order_acquire) ||
        !shared_ || !wakeEvent_)
        return OutputPublishResult::SessionUnavailable;
    const PublishResult published = TryPublishSnapshot(*shared_, reports,
        padCount, validMask, timestampUs, publicationSequence);
    switch (published)
    {
    case PublishResult::Inactive:
        return OutputPublishResult::Inactive;
    case PublishResult::InvalidPayload:
        return OutputPublishResult::InvalidPayload;
    case PublishResult::Contended:
        return OutputPublishResult::Contended;
    case PublishResult::Published:
        if (SetEvent(wakeEvent_))
            return OutputPublishResult::Published;
        restartSafe_.store(false, std::memory_order_release);
        return OutputPublishResult::WakeFailed;
    }
    return OutputPublishResult::SessionUnavailable;
#endif
}

bool OutputProcessSession::PublishProducerProgress(std::uint64_t tickMs) noexcept
{
#if !defined(_WIN32)
    (void)tickMs;
    return false;
#else
    return initialized_ && restartSafe_.load(std::memory_order_acquire) &&
        shared_ && PublishProducerLease(*shared_, tickMs);
#endif
}

bool OutputProcessSession::ReadTelemetry(
    ChildTelemetrySnapshotV1& snapshot) noexcept
{
    return initialized_ && shared_ && ReadChildTelemetry(*shared_, snapshot);
}

bool OutputProcessSession::IsInitialized() const noexcept
{
    return initialized_;
}

bool OutputProcessSession::RestartSafe() const noexcept
{
    return initialized_ && restartSafe_.load(std::memory_order_acquire) &&
        supervisor_ &&
        supervisor_->RestartSafe();
}

std::uint32_t OutputProcessSession::OwnerPid() const noexcept
{
    return ownerPid_;
}

std::uint64_t OutputProcessSession::LaunchNonce() const noexcept
{
    return launchNonce_;
}

} // namespace halljoy::vigem_output
