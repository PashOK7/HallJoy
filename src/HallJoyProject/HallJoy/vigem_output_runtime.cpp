#include "vigem_output_runtime.h"

#include "stability_trace.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <new>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace halljoy::vigem_output
{
namespace
{
constexpr std::uint32_t kMinimumPadCount = 1u;
constexpr std::uint32_t kStartupWaitMs = 4000u;
constexpr std::uint32_t kOwnerJoinMs = 10000u;
constexpr std::uint32_t kSessionLeaseDrainMs = 1500u;
constexpr std::uint32_t kRecoveryBackoffMs = 250u;
constexpr std::uint32_t kHealthyHeartbeatAgeMs = 1000u;

std::uint32_t ClampPadCount(std::uint32_t padCount) noexcept
{
    return std::clamp(padCount, kMinimumPadCount, kMaxPads);
}

#if defined(_WIN32)
bool CurrentApplicationPath(std::wstring& path, std::uint32_t& error) noexcept
{
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length == 0u || length >= buffer.size() - 1u)
    {
        error = length == 0u ? GetLastError() : ERROR_INSUFFICIENT_BUFFER;
        return false;
    }
    try
    {
        path.assign(buffer.data(), length);
    }
    catch (...)
    {
        error = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }
    error = ERROR_SUCCESS;
    return true;
}

bool HasCommandLineToken(const wchar_t* token) noexcept
{
#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    return commandLine && token && wcsstr(commandLine, token) != nullptr;
#else
    (void)token;
    return false;
#endif
}
#endif
} // namespace

struct OutputRuntime::Implementation
{
    std::mutex lifecycleMutex;
    OutputProcessSession session;
    std::wstring applicationPath;
    process::NativeHandle commandEvent = nullptr;
    process::NativeHandle ownerThread = nullptr;

    std::atomic<bool> run{ false };
    std::atomic<bool> accessAdmission{ false };
    std::atomic<std::uint32_t> accessLeases{ 0u };
    std::atomic<bool> desiredEnabled{ false };
    std::atomic<std::uint32_t> desiredPadCount{ 1u };
    std::atomic<std::uint64_t> desiredRevision{ 0u };
    std::atomic<std::uint64_t> nextGeneration{ 0u };
    std::atomic<std::uint64_t> activeGeneration{ 0u };
    std::atomic<std::uint64_t> completedGeneration{ 0u };
    std::atomic<std::uint64_t> restartCount{ 0u };
    std::atomic<std::uint64_t> sessionRebuildCount{ 0u };
    std::atomic<std::uint64_t> lastUnsafeGeneration{ 0u };
    std::atomic<std::uint32_t> lastUnsafeFlags{ 0u };
    std::atomic<std::uint32_t> childPid{ 0u };
    std::atomic<std::uint32_t> lastError{ 0u };
    std::atomic<OutputGenerationOutcome> lastOutcome{
        OutputGenerationOutcome::SessionFailure };
    std::atomic<OutputRuntimeState> state{ OutputRuntimeState::Stopped };
    std::atomic<bool> restartSafe{ true };
    std::atomic<std::uint64_t> lastReadyLoggedGeneration{ 0u };
    std::atomic<std::uint64_t> lastAppliedLoggedGeneration{ 0u };
    std::atomic<std::uint64_t> lastAppliedLoggedSequence{ 0u };
#if defined(HALLJOY_ANALOG_SIMULATOR)
    std::atomic<bool> testFaultConsumed{ false };
#endif

    bool AcquireSessionLease() noexcept
    {
        if (!accessAdmission.load(std::memory_order_acquire))
            return false;
        const std::uint32_t lease = accessLeases.fetch_add(
            1u, std::memory_order_acq_rel) + 1u;
        if (lease == 0u)
        {
            accessLeases.fetch_sub(1u, std::memory_order_release);
            return false;
        }
        if (!accessAdmission.load(std::memory_order_acquire))
        {
            accessLeases.fetch_sub(1u, std::memory_order_release);
            return false;
        }
        return true;
    }

    void ReleaseSessionLease() noexcept
    {
        accessLeases.fetch_sub(1u, std::memory_order_release);
    }

    bool WaitForSessionLeases(std::uint32_t timeoutMs) noexcept
    {
#if !defined(_WIN32)
        (void)timeoutMs;
        return accessLeases.load(std::memory_order_acquire) == 0u;
#else
        const ULONGLONG deadline = GetTickCount64() + timeoutMs;
        while (accessLeases.load(std::memory_order_acquire) != 0u &&
            GetTickCount64() < deadline)
        {
            SwitchToThread();
        }
        return accessLeases.load(std::memory_order_acquire) == 0u;
#endif
    }

    void SignalOwner() noexcept
    {
#if defined(_WIN32)
        if (commandEvent)
            (void)SetEvent(commandEvent);
#endif
    }

    OutputHostMode SelectModeForGeneration() noexcept
    {
#if defined(HALLJOY_ANALOG_SIMULATOR)
        if (HasCommandLineToken(kOutputRuntimeStressTestArgument))
            return OutputHostMode::FakeNormal;
        const bool injectFault =
            HasCommandLineToken(L"--halljoy-test-vigem-output-invalid-thread-handle") ||
            HasCommandLineToken(L"--halljoy-test-vigem-output-cpp-fault");
        const bool injectStall =
            HasCommandLineToken(L"--halljoy-test-vigem-output-wake-close-use") ||
            HasCommandLineToken(L"--halljoy-test-vigem-update-stall");
        if ((injectFault || injectStall) &&
            !testFaultConsumed.exchange(true, std::memory_order_acq_rel))
        {
            return injectStall
                ? OutputHostMode::FakeStallAfterReady
                : OutputHostMode::FakeExitAfterReady;
        }
        if (injectFault || injectStall)
            return OutputHostMode::FakeNormal;
#endif
        return OutputHostMode::Real;
    }

    bool RebuildSession() noexcept
    {
        accessAdmission.store(false, std::memory_order_release);
        if (!WaitForSessionLeases(kSessionLeaseDrainMs))
        {
            lastError.store(ERROR_TIMEOUT, std::memory_order_release);
            restartSafe.store(false, std::memory_order_release);
            return false;
        }
        std::uint32_t shutdownError = ERROR_SUCCESS;
        if (!session.Shutdown(shutdownError))
        {
            lastError.store(shutdownError, std::memory_order_release);
            restartSafe.store(false, std::memory_order_release);
            return false;
        }
        std::uint32_t initializeError = ERROR_SUCCESS;
        if (!session.Initialize(initializeError))
        {
            lastError.store(initializeError, std::memory_order_release);
            restartSafe.store(false, std::memory_order_release);
            return false;
        }
        restartSafe.store(true, std::memory_order_release);
        sessionRebuildCount.fetch_add(1u, std::memory_order_relaxed);
        accessAdmission.store(true, std::memory_order_release);
        return true;
    }

    void StoreGenerationResult(std::uint64_t generation,
        const OutputGenerationResult& result) noexcept
    {
        activeGeneration.store(0u, std::memory_order_release);
        childPid.store(0u, std::memory_order_release);
        completedGeneration.store(generation, std::memory_order_release);
        lastOutcome.store(result.outcome, std::memory_order_release);
        std::uint32_t error = result.nativeError;
        if (result.finalTelemetryAvailable && result.finalTelemetry.lastError != 0u)
            error = result.finalTelemetry.lastError;
        lastError.store(error, std::memory_order_release);
        restartSafe.store(result.restartSafe, std::memory_order_release);
        if (!result.restartSafe)
        {
            const std::uint32_t unsafeFlags =
                (result.process.restartSafe ? 1u : 0u) |
                (result.generationDisabled ? 2u : 0u) |
                (result.publicationQuiescent ? 4u : 0u) |
                (result.process.terminationPrepared ? 8u : 0u) |
                (result.process.forced ? 16u : 0u);
            lastUnsafeFlags.store(unsafeFlags, std::memory_order_release);
            lastUnsafeGeneration.store(generation, std::memory_order_release);
        }
        StabilityTrace_Write(
            result.restartSafe ? L"INFO" : L"ERROR",
            L"vigem-output", L"generation.end",
            L"generation=%llu outcome=%u win32_or_vigem=%lu forced=%d prepared=%d disabled=%d quiescent=%d neutral=%d removed=%d restart_safe=%d child_pid=%lu process_protected=%d job_protected=%d process_identity=%d ownership_error=%lu",
            static_cast<unsigned long long>(generation),
            static_cast<unsigned>(result.outcome),
            static_cast<unsigned long>(error),
            result.process.forced ? 1 : 0,
            result.process.terminationPrepared ? 1 : 0,
            result.generationDisabled ? 1 : 0,
            result.publicationQuiescent ? 1 : 0,
            result.neutralApplied ? 1 : 0,
            result.targetsRemoved ? 1 : 0,
            result.restartSafe ? 1 : 0,
            static_cast<unsigned long>(result.process.childPid),
            result.process.processHandleProtected ? 1 : 0,
            result.process.jobHandleProtected ? 1 : 0,
            result.process.processIdentityMatched ? 1 : 0,
            static_cast<unsigned long>(result.process.ownershipError));
    }

    static DWORD WINAPI OwnerThreadEntry(void* raw) noexcept
    {
        auto* self = static_cast<Implementation*>(raw);
        return self ? self->OwnerThreadBody() : ERROR_INVALID_PARAMETER;
    }

    DWORD OwnerThreadBody() noexcept
    {
#if !defined(_WIN32)
        return 0u;
#else
        StabilityTrace_Write(L"INFO", L"vigem-output", L"owner.start",
            L"process_isolated=1 process_lifetime_wake=1");
        std::uint64_t previousStartedGeneration = 0u;
        while (run.load(std::memory_order_acquire))
        {
            if (!desiredEnabled.load(std::memory_order_acquire))
            {
                activeGeneration.store(0u, std::memory_order_release);
                childPid.store(0u, std::memory_order_release);
                state.store(OutputRuntimeState::Disabled, std::memory_order_release);
                (void)WaitForSingleObject(commandEvent, INFINITE);
                continue;
            }

            // Drain a stale coalesced request, then prove that no newer desired
            // revision arrived between the snapshot and the drain.
            std::uint64_t revision = 0u;
            std::uint32_t padCount = 1u;
            for (;;)
            {
                revision = desiredRevision.load(std::memory_order_acquire);
                padCount = ClampPadCount(
                    desiredPadCount.load(std::memory_order_acquire));
                while (WaitForSingleObject(commandEvent, 0u) == WAIT_OBJECT_0)
                {
                }
                if (!run.load(std::memory_order_acquire))
                    break;
                if (revision == desiredRevision.load(std::memory_order_acquire))
                    break;
            }
            if (!run.load(std::memory_order_acquire))
                break;
            if (!desiredEnabled.load(std::memory_order_acquire))
                continue;

            std::uint64_t generation = nextGeneration.fetch_add(
                1u, std::memory_order_acq_rel) + 1u;
            if (generation == 0u)
                generation = nextGeneration.fetch_add(1u, std::memory_order_acq_rel) + 1u;
            activeGeneration.store(generation, std::memory_order_release);
            state.store(previousStartedGeneration == 0u
                ? OutputRuntimeState::Starting
                : OutputRuntimeState::Recovering, std::memory_order_release);
            accessAdmission.store(true, std::memory_order_release);

            OutputGenerationRequest request{};
            request.applicationPath = applicationPath;
            request.generation = generation;
            request.padCount = padCount;
            request.mode = SelectModeForGeneration();
            request.supervisorStopEvent = commandEvent;
            StabilityTrace_Write(L"INFO", L"vigem-output", L"generation.begin",
                L"generation=%llu pads=%lu revision=%llu mode=%u previous=%llu",
                static_cast<unsigned long long>(generation),
                static_cast<unsigned long>(padCount),
                static_cast<unsigned long long>(revision),
                static_cast<unsigned>(request.mode),
                static_cast<unsigned long long>(previousStartedGeneration));
            previousStartedGeneration = generation;

            const OutputGenerationResult result = session.RunOne(request);
            StoreGenerationResult(generation, result);
            if (!run.load(std::memory_order_acquire))
                break;

            const bool configurationChanged =
                revision != desiredRevision.load(std::memory_order_acquire) ||
                !desiredEnabled.load(std::memory_order_acquire);
            if (configurationChanged)
            {
                if (!result.restartSafe)
                {
                    restartCount.fetch_add(1u, std::memory_order_relaxed);
                    if (!RebuildSession())
                    {
                        state.store(OutputRuntimeState::Faulted,
                            std::memory_order_release);
                        break;
                    }
                }
                continue;
            }

            restartCount.fetch_add(1u, std::memory_order_relaxed);
            if (!result.restartSafe && !RebuildSession())
            {
                state.store(OutputRuntimeState::Faulted, std::memory_order_release);
                break;
            }

            state.store(OutputRuntimeState::Recovering, std::memory_order_release);
            (void)WaitForSingleObject(commandEvent, kRecoveryBackoffMs);
        }
        accessAdmission.store(false, std::memory_order_release);
        state.store(run.load(std::memory_order_acquire)
            ? OutputRuntimeState::Faulted
            : OutputRuntimeState::Stopping, std::memory_order_release);
        StabilityTrace_Write(L"INFO", L"vigem-output", L"owner.exit",
            L"restart_safe=%d completed_generation=%llu restarts=%llu",
            restartSafe.load(std::memory_order_acquire) ? 1 : 0,
            static_cast<unsigned long long>(
                completedGeneration.load(std::memory_order_acquire)),
            static_cast<unsigned long long>(
                restartCount.load(std::memory_order_acquire)));
        return 0u;
#endif
    }

    OutputRuntimeStatus Snapshot() noexcept
    {
        OutputRuntimeStatus status{};
        status.state = state.load(std::memory_order_acquire);
        status.desiredEnabled = desiredEnabled.load(std::memory_order_acquire);
        status.desiredPadCount = desiredPadCount.load(std::memory_order_acquire);
        status.activeGeneration = activeGeneration.load(std::memory_order_acquire);
        status.completedGeneration = completedGeneration.load(std::memory_order_acquire);
        status.restartCount = restartCount.load(std::memory_order_acquire);
        status.sessionRebuildCount = sessionRebuildCount.load(
            std::memory_order_acquire);
        status.lastUnsafeGeneration = lastUnsafeGeneration.load(
            std::memory_order_acquire);
        status.lastUnsafeFlags = lastUnsafeFlags.load(std::memory_order_acquire);
        status.childPid = childPid.load(std::memory_order_acquire);
        status.lastError = lastError.load(std::memory_order_acquire);
        status.lastOutcome = lastOutcome.load(std::memory_order_acquire);
        status.restartSafe = restartSafe.load(std::memory_order_acquire);
        if (!status.desiredEnabled && run.load(std::memory_order_acquire))
        {
            status.ready = true;
            status.state = OutputRuntimeState::Disabled;
            status.lastError = ERROR_SUCCESS;
            return status;
        }

        if (!AcquireSessionLease())
            return status;
        ChildTelemetrySnapshotV1 telemetry{};
        const bool telemetryAvailable = session.ReadTelemetry(telemetry);
        ReleaseSessionLease();
        if (!telemetryAvailable || telemetry.childGeneration == 0u ||
            telemetry.childGeneration != status.activeGeneration)
        {
            return status;
        }

        status.childPid = telemetry.childPid;
        status.appliedPublicationSequence = telemetry.appliedPublicationSequence;
        status.diagnosticCheckpoint = telemetry.diagnosticCheckpoint;
        status.heartbeatTickMs = telemetry.heartbeatTickMs;
        const std::uint64_t now = GetTickCount64();
        const bool heartbeatFresh = telemetry.heartbeatTickMs != 0u &&
            now >= telemetry.heartbeatTickMs &&
            now - telemetry.heartbeatTickMs <= kHealthyHeartbeatAgeMs;
        status.ready = telemetry.state == ChildState::Ready &&
            telemetry.readyGeneration == status.activeGeneration && heartbeatFresh;
        if (status.ready)
        {
            state.store(OutputRuntimeState::Ready, std::memory_order_release);
            childPid.store(telemetry.childPid, std::memory_order_release);
            lastError.store(ERROR_SUCCESS, std::memory_order_release);
            status.state = OutputRuntimeState::Ready;
            status.lastError = ERROR_SUCCESS;
            const std::uint64_t previous = lastReadyLoggedGeneration.exchange(
                status.activeGeneration, std::memory_order_acq_rel);
            if (previous != status.activeGeneration)
            {
                StabilityTrace_Write(L"INFO", L"vigem-output", L"generation.ready",
                    L"generation=%llu child_pid=%lu pads=%lu heartbeat_fresh=1",
                    static_cast<unsigned long long>(status.activeGeneration),
                    static_cast<unsigned long>(telemetry.childPid),
                    static_cast<unsigned long>(status.desiredPadCount));
            }
            if (telemetry.appliedPublicationSequence != 0u)
            {
                const std::uint64_t loggedGeneration =
                    lastAppliedLoggedGeneration.load(std::memory_order_acquire);
                const std::uint64_t loggedSequence =
                    lastAppliedLoggedSequence.load(std::memory_order_acquire);
                if (loggedGeneration != status.activeGeneration ||
                    loggedSequence != telemetry.appliedPublicationSequence)
                {
                    lastAppliedLoggedGeneration.store(status.activeGeneration,
                        std::memory_order_release);
                    lastAppliedLoggedSequence.store(
                        telemetry.appliedPublicationSequence,
                        std::memory_order_release);
                    StabilityTrace_Write(L"INFO", L"vigem-output",
                        L"publication.applied",
                        L"generation=%llu sequence=%llu checkpoint=%llu",
                        static_cast<unsigned long long>(status.activeGeneration),
                        static_cast<unsigned long long>(
                            telemetry.appliedPublicationSequence),
                        static_cast<unsigned long long>(
                            telemetry.diagnosticCheckpoint));
                }
            }
        }
        else if (telemetry.state == ChildState::Failed && telemetry.lastError != 0u)
        {
            status.lastError = telemetry.lastError;
            lastError.store(telemetry.lastError, std::memory_order_release);
        }
        return status;
    }
};

OutputRuntime::OutputRuntime() noexcept
    : implementation_(new (std::nothrow) Implementation())
{
}

OutputRuntime::~OutputRuntime() noexcept
{
    if (implementation_)
    {
        std::uint32_t ignored = 0u;
        (void)Stop(ignored);
    }
}

bool OutputRuntime::Start(bool enabled, std::uint32_t padCount,
    std::uint32_t& error) noexcept
{
#if !defined(_WIN32)
    (void)enabled;
    (void)padCount;
    error = 0u;
    return false;
#else
    error = ERROR_SUCCESS;
    if (!implementation_)
    {
        error = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }
    auto& self = *implementation_;
    std::lock_guard<std::mutex> lock(self.lifecycleMutex);
    if (self.ownerThread || self.run.load(std::memory_order_acquire))
    {
        error = ERROR_ALREADY_INITIALIZED;
        return false;
    }
    if (!CurrentApplicationPath(self.applicationPath, error))
        return false;
    self.commandEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!self.commandEvent)
    {
        error = GetLastError();
        return false;
    }
    if (!self.session.Initialize(error))
    {
        CloseHandle(self.commandEvent);
        self.commandEvent = nullptr;
        return false;
    }

    self.desiredEnabled.store(enabled, std::memory_order_release);
    self.desiredPadCount.store(ClampPadCount(padCount), std::memory_order_release);
    self.desiredRevision.fetch_add(1u, std::memory_order_acq_rel);
    self.completedGeneration.store(0u, std::memory_order_release);
    self.restartCount.store(0u, std::memory_order_release);
    self.sessionRebuildCount.store(0u, std::memory_order_release);
    self.lastUnsafeGeneration.store(0u, std::memory_order_release);
    self.lastUnsafeFlags.store(0u, std::memory_order_release);
    self.lastError.store(ERROR_SUCCESS, std::memory_order_release);
    self.restartSafe.store(true, std::memory_order_release);
    self.state.store(OutputRuntimeState::Starting, std::memory_order_release);
    self.accessAdmission.store(true, std::memory_order_release);
    self.run.store(true, std::memory_order_release);
#if defined(HALLJOY_ANALOG_SIMULATOR)
    self.testFaultConsumed.store(false, std::memory_order_release);
#endif
    self.ownerThread = CreateThread(nullptr, 0,
        &Implementation::OwnerThreadEntry, &self, 0u, nullptr);
    if (!self.ownerThread)
    {
        error = GetLastError();
        self.run.store(false, std::memory_order_release);
        self.accessAdmission.store(false, std::memory_order_release);
        std::uint32_t shutdownError = ERROR_SUCCESS;
        (void)self.session.Shutdown(shutdownError);
        CloseHandle(self.commandEvent);
        self.commandEvent = nullptr;
        self.state.store(OutputRuntimeState::Stopped, std::memory_order_release);
        return false;
    }

    const ULONGLONG deadline = GetTickCount64() + kStartupWaitMs;
    for (;;)
    {
        const OutputRuntimeStatus status = self.Snapshot();
        if (status.ready)
            return true;
        if (status.completedGeneration != 0u && status.lastError != ERROR_SUCCESS)
        {
            error = status.lastError;
            break;
        }
        if (GetTickCount64() >= deadline)
        {
            error = ERROR_TIMEOUT;
            break;
        }
        Sleep(5u);
    }

    self.run.store(false, std::memory_order_release);
    self.accessAdmission.store(false, std::memory_order_release);
    self.SignalOwner();
    const DWORD wait = WaitForSingleObject(self.ownerThread, kOwnerJoinMs);
    if (wait == WAIT_OBJECT_0)
    {
        CloseHandle(self.ownerThread);
        self.ownerThread = nullptr;
        (void)self.WaitForSessionLeases(kSessionLeaseDrainMs);
        std::uint32_t shutdownError = ERROR_SUCCESS;
        (void)self.session.Shutdown(shutdownError);
        CloseHandle(self.commandEvent);
        self.commandEvent = nullptr;
        self.state.store(OutputRuntimeState::Stopped, std::memory_order_release);
    }
    else
    {
        self.restartSafe.store(false, std::memory_order_release);
    }
    return false;
#endif
}

bool OutputRuntime::Stop(std::uint32_t& error) noexcept
{
#if !defined(_WIN32)
    error = 0u;
    return true;
#else
    error = ERROR_SUCCESS;
    if (!implementation_)
        return true;
    auto& self = *implementation_;
    std::lock_guard<std::mutex> lock(self.lifecycleMutex);
    if (!self.ownerThread)
    {
        self.run.store(false, std::memory_order_release);
        self.accessAdmission.store(false, std::memory_order_release);
        self.state.store(OutputRuntimeState::Stopped, std::memory_order_release);
        return true;
    }

    self.state.store(OutputRuntimeState::Stopping, std::memory_order_release);
    self.desiredEnabled.store(false, std::memory_order_release);
    self.desiredRevision.fetch_add(1u, std::memory_order_acq_rel);
    self.run.store(false, std::memory_order_release);
    self.accessAdmission.store(false, std::memory_order_release);
    self.SignalOwner();
    const DWORD wait = WaitForSingleObject(self.ownerThread, kOwnerJoinMs);
    if (wait != WAIT_OBJECT_0)
    {
        error = wait == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT;
        self.restartSafe.store(false, std::memory_order_release);
        self.state.store(OutputRuntimeState::Faulted, std::memory_order_release);
        return false;
    }
    CloseHandle(self.ownerThread);
    self.ownerThread = nullptr;
    if (!self.WaitForSessionLeases(kSessionLeaseDrainMs))
    {
        error = ERROR_TIMEOUT;
        self.restartSafe.store(false, std::memory_order_release);
        self.state.store(OutputRuntimeState::Faulted, std::memory_order_release);
        return false;
    }
    if (!self.session.Shutdown(error))
    {
        self.restartSafe.store(false, std::memory_order_release);
        self.state.store(OutputRuntimeState::Faulted, std::memory_order_release);
        return false;
    }
    CloseHandle(self.commandEvent);
    self.commandEvent = nullptr;
    self.applicationPath.clear();
    self.activeGeneration.store(0u, std::memory_order_release);
    self.childPid.store(0u, std::memory_order_release);
    self.state.store(OutputRuntimeState::Stopped, std::memory_order_release);
    StabilityTrace_Write(L"INFO", L"vigem-output", L"stop.complete",
        L"owner_joined=1 session_leases=0 child_survivors=0");
    return true;
#endif
}

void OutputRuntime::Configure(bool enabled, std::uint32_t padCount) noexcept
{
    if (!implementation_)
        return;
    auto& self = *implementation_;
    const std::uint32_t clamped = ClampPadCount(padCount);
    const bool previousEnabled = self.desiredEnabled.exchange(
        enabled, std::memory_order_acq_rel);
    const std::uint32_t previousCount = self.desiredPadCount.exchange(
        clamped, std::memory_order_acq_rel);
    if (previousEnabled == enabled && previousCount == clamped)
        return;
    const std::uint64_t revision = self.desiredRevision.fetch_add(
        1u, std::memory_order_acq_rel) + 1u;
    self.SignalOwner();
    StabilityTrace_Write(L"INFO", L"vigem-output", L"configuration.requested",
        L"revision=%llu enabled=%d pads=%lu",
        static_cast<unsigned long long>(revision), enabled ? 1 : 0,
        static_cast<unsigned long>(clamped));
}

void OutputRuntime::RequestRestart() noexcept
{
    if (!implementation_ ||
        !implementation_->run.load(std::memory_order_acquire))
    {
        return;
    }
    implementation_->desiredRevision.fetch_add(1u, std::memory_order_acq_rel);
    implementation_->SignalOwner();
}

OutputPublishResult OutputRuntime::TryPublish(
    const XusbReportV1* reports, std::uint32_t padCount,
    std::uint32_t validMask, std::uint64_t timestampUs,
    std::uint64_t* publicationSequence) noexcept
{
    if (!implementation_ || !implementation_->AcquireSessionLease())
        return OutputPublishResult::SessionUnavailable;
    const OutputPublishResult result = implementation_->session.TryPublish(
        reports, padCount, validMask, timestampUs, publicationSequence);
    if (result == OutputPublishResult::WakeFailed)
    {
        implementation_->lastError.store(ERROR_INVALID_HANDLE,
            std::memory_order_release);
        implementation_->SignalOwner();
    }
    implementation_->ReleaseSessionLease();
    return result;
}

bool OutputRuntime::PublishProducerProgress(std::uint64_t tickMs) noexcept
{
    if (!implementation_ || !implementation_->AcquireSessionLease())
        return false;
    const bool published = implementation_->session.PublishProducerProgress(tickMs);
    implementation_->ReleaseSessionLease();
    return published;
}

OutputRuntimeStatus OutputRuntime::GetStatus() noexcept
{
    return implementation_ ? implementation_->Snapshot() : OutputRuntimeStatus{};
}

} // namespace halljoy::vigem_output
