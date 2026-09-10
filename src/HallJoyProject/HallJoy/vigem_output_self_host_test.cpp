#include "vigem_output_self_host_test.h"

#include "vigem_output_process_client.h"
#include "vigem_output_process_protocol.h"
#include "vigem_output_runtime.h"
#include "stability_trace.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <new>

#pragma comment(lib, "shell32.lib")

namespace
{
using namespace halljoy::vigem_output;
namespace process = halljoy::process;

#if defined(HALLJOY_ANALOG_SIMULATOR)
struct AsyncGeneration
{
    OutputProcessSession* session = nullptr;
    OutputGenerationRequest request{};
    OutputGenerationResult result{};
    HANDLE thread = nullptr;
};

DWORD WINAPI RunGenerationThread(void* raw) noexcept
{
    auto* run = static_cast<AsyncGeneration*>(raw);
    if (!run || !run->session)
        return ERROR_INVALID_PARAMETER;
    run->result = run->session->RunOne(run->request);
    return ERROR_SUCCESS;
}

bool StartGeneration(AsyncGeneration& run, OutputProcessSession& session,
    const std::wstring& applicationPath, std::uint64_t generation,
    OutputHostMode mode, HANDLE supervisorStopEvent,
    const process::SupervisionPolicy& policy)
{
    if (!ResetEvent(supervisorStopEvent))
        return false;
    run.session = &session;
    run.request.applicationPath = applicationPath;
    run.request.generation = generation;
    run.request.padCount = kMaxPads;
    run.request.mode = mode;
    run.request.supervisorStopEvent = supervisorStopEvent;
    run.request.policy = policy;
    run.thread = CreateThread(nullptr, 0, &RunGenerationThread, &run, 0, nullptr);
    return run.thread != nullptr;
}

bool JoinGeneration(AsyncGeneration& run, HANDLE supervisorStopEvent,
    DWORD timeoutMs = 6000u) noexcept
{
    if (!run.thread)
        return false;
    DWORD wait = WaitForSingleObject(run.thread, timeoutMs);
    if (wait != WAIT_OBJECT_0)
    {
        SetEvent(supervisorStopEvent);
        wait = WaitForSingleObject(run.thread, 3000u);
    }
    DWORD threadExit = STILL_ACTIVE;
    const bool joined = wait == WAIT_OBJECT_0 &&
        GetExitCodeThread(run.thread, &threadExit) &&
        threadExit == ERROR_SUCCESS;
    CloseHandle(run.thread);
    run.thread = nullptr;
    return joined;
}

template <typename Predicate>
bool WaitForTelemetry(OutputProcessSession& session, std::uint64_t generation,
    DWORD timeoutMs, Predicate predicate) noexcept
{
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    do
    {
        ChildTelemetrySnapshotV1 telemetry{};
        if (session.ReadTelemetry(telemetry) &&
            telemetry.childGeneration == generation && predicate(telemetry))
        {
            return true;
        }
        Sleep(1u);
    } while (GetTickCount64() < deadline);
    return false;
}

bool WaitReady(OutputProcessSession& session, std::uint64_t generation,
    DWORD timeoutMs = 2000u) noexcept
{
    return WaitForTelemetry(session, generation, timeoutMs,
        [generation](const ChildTelemetrySnapshotV1& telemetry) noexcept
        {
            return telemetry.readyGeneration == generation &&
                telemetry.appliedConfigurationGeneration == generation &&
                (telemetry.diagnosticFlags &
                    kChildDiagnosticContainedBeforeEntry) != 0u;
        });
}

bool WaitApplied(OutputProcessSession& session, std::uint64_t generation,
    std::uint64_t sequence, std::uint64_t checkpoint,
    DWORD timeoutMs = 2000u) noexcept
{
    return WaitForTelemetry(session, generation, timeoutMs,
        [sequence, checkpoint](const ChildTelemetrySnapshotV1& telemetry) noexcept
        {
            return telemetry.appliedPublicationSequence == sequence &&
                telemetry.diagnosticCheckpoint == checkpoint;
        });
}

std::array<XusbReportV1, kMaxPads> MakeReports(std::uint16_t seed) noexcept
{
    std::array<XusbReportV1, kMaxPads> reports{};
    for (std::uint32_t index = 0; index < kMaxPads; ++index)
    {
        XusbReportV1& report = reports[index];
        report.buttons = static_cast<std::uint16_t>(seed + index * 17u);
        report.leftTrigger = static_cast<std::uint8_t>(seed + index * 3u);
        report.rightTrigger = static_cast<std::uint8_t>(seed + index * 5u);
        report.thumbLX = static_cast<std::int16_t>(seed * 11u + index);
        report.thumbLY = static_cast<std::int16_t>(-static_cast<int>(seed * 7u + index));
        report.thumbRX = static_cast<std::int16_t>(seed * 5u + index * 2u);
        report.thumbRY = static_cast<std::int16_t>(-static_cast<int>(seed * 3u + index * 2u));
    }
    return reports;
}

bool PublishReports(OutputProcessSession& session,
    const std::array<XusbReportV1, kMaxPads>& reports,
    std::uint64_t& sequence) noexcept
{
    for (unsigned attempt = 0; attempt < 200u; ++attempt)
    {
        (void)session.PublishProducerProgress(GetTickCount64());
        const OutputPublishResult published = session.TryPublish(reports.data(),
            kMaxPads, (1u << kMaxPads) - 1u, GetTickCount64() * 1000u,
            &sequence);
        if (published == OutputPublishResult::Published)
            return true;
        if (published != OutputPublishResult::Contended)
            return false;
        Sleep(1u);
    }
    return false;
}

bool ChildIsReaped(std::uint32_t pid) noexcept
{
    if (pid == 0u)
        return false;
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE, pid);
    if (!process)
        return GetLastError() == ERROR_INVALID_PARAMETER;
    const DWORD wait = WaitForSingleObject(process, 0u);
    CloseHandle(process);
    return wait == WAIT_OBJECT_0;
}

bool ResultIsRestartSafe(const OutputGenerationResult& result) noexcept
{
    return result.restartSafe && result.generationDisabled &&
        result.publicationQuiescent && ChildIsReaped(result.process.childPid);
}

int RunExactSelfHostSuite()
{
    wchar_t applicationPath[32768]{};
    const DWORD pathLength = GetModuleFileNameW(nullptr, applicationPath,
        static_cast<DWORD>(std::size(applicationPath)));
    if (pathLength == 0u || pathLength >= std::size(applicationPath) - 1u)
        return 101;

    OutputProcessSession session;
    std::uint32_t error = ERROR_SUCCESS;
    if (!session.Initialize(error) || session.OwnerPid() != GetCurrentProcessId() ||
        session.LaunchNonce() == 0u)
    {
        return 102;
    }

    HANDLE supervisorStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!supervisorStop)
        return 103;

    process::SupervisionPolicy policy{};
    policy.startupTimeoutMs = 1000u;
    policy.progressTimeoutMs = 300u;
    policy.observationIntervalMs = 2u;
    policy.gracefulStopTimeoutMs = 500u;
    policy.hardReapTimeoutMs = 1500u;

    std::uint64_t generation = 1u;

    // Normal exact-image start, complete four-pad apply and qualified stop.
    {
        AsyncGeneration run{};
        if (!StartGeneration(run, session, applicationPath, generation,
                OutputHostMode::FakeNormal, supervisorStop, policy) ||
            !WaitReady(session, generation))
        {
            CloseHandle(supervisorStop);
            return 110;
        }
        const auto reports = MakeReports(11u);
        std::uint64_t sequence = 0u;
        const std::uint64_t checkpoint = ComputeReportCheckpoint(reports.data(),
            kMaxPads, (1u << kMaxPads) - 1u);
        int failure = 0;
        if (!PublishReports(session, reports, sequence)) failure = 111;
        else if (!WaitApplied(session, generation, sequence, checkpoint)) failure = 112;
        else if (!SetEvent(supervisorStop)) failure = 113;
        else if (!JoinGeneration(run, supervisorStop)) failure = 114;
        else if (run.result.outcome !=
            OutputGenerationOutcome::CleanPlannedStop) failure = 115;
        else if (!run.result.cleanStopConfirmed) failure = 116;
        else if (!ResultIsRestartSafe(run.result)) failure = 117;
        if (failure != 0)
        {
            CloseHandle(supervisorStop);
            return failure;
        }
    }

    // Exit before Ready remains distinct and fully reaped.
    ++generation;
    {
        AsyncGeneration run{};
        if (!StartGeneration(run, session, applicationPath, generation,
                OutputHostMode::FakeExitBeforeReady, supervisorStop, policy) ||
            !JoinGeneration(run, supervisorStop) ||
            run.result.outcome != OutputGenerationOutcome::ExitBeforeReady ||
            !ResultIsRestartSafe(run.result))
        {
            CloseHandle(supervisorStop);
            return 120;
        }
    }

    // Ready committed immediately before exit must not be misclassified by a
    // polling race as exit-before-ready.
    ++generation;
    {
        AsyncGeneration run{};
        if (!StartGeneration(run, session, applicationPath, generation,
                OutputHostMode::FakeExitAfterReady, supervisorStop, policy) ||
            !JoinGeneration(run, supervisorStop) ||
            run.result.outcome != OutputGenerationOutcome::UnexpectedExit ||
            !run.result.finalTelemetryAvailable ||
            run.result.finalTelemetry.readyGeneration != generation ||
            !ResultIsRestartSafe(run.result))
        {
            CloseHandle(supervisorStop);
            return 130;
        }
    }

    const OutputHostMode updateBoundaries[] = {
        OutputHostMode::FakeExitBeforeSnapshotRead,
        OutputHostMode::FakeExitDuringUpdate,
        OutputHostMode::FakeExitAfterAcknowledgement,
    };
    for (std::size_t boundary = 0; boundary < std::size(updateBoundaries);
        ++boundary)
    {
        ++generation;
        AsyncGeneration run{};
        if (!StartGeneration(run, session, applicationPath, generation,
                updateBoundaries[boundary], supervisorStop, policy) ||
            !WaitReady(session, generation))
        {
            CloseHandle(supervisorStop);
            return static_cast<int>(140u + boundary * 3u);
        }
        const auto reports = MakeReports(static_cast<std::uint16_t>(30u + boundary));
        std::uint64_t sequence = 0u;
        const std::uint64_t checkpoint = ComputeReportCheckpoint(reports.data(),
            kMaxPads, (1u << kMaxPads) - 1u);
        if (!PublishReports(session, reports, sequence) ||
            !JoinGeneration(run, supervisorStop) ||
            run.result.outcome != OutputGenerationOutcome::UnexpectedExit ||
            !ResultIsRestartSafe(run.result))
        {
            CloseHandle(supervisorStop);
            return static_cast<int>(141u + boundary * 3u);
        }
        const bool acknowledgementExpected =
            updateBoundaries[boundary] ==
                OutputHostMode::FakeExitAfterAcknowledgement;
        const bool acknowledgementPresent = run.result.finalTelemetryAvailable &&
            run.result.finalTelemetry.appliedPublicationSequence == sequence &&
            run.result.finalTelemetry.diagnosticCheckpoint == checkpoint;
        if (acknowledgementPresent != acknowledgementExpected)
        {
            CloseHandle(supervisorStop);
            return static_cast<int>(142u + boundary * 3u);
        }
    }

    // Process exit after stop signal is not a clean output stop without the
    // neutral/target-removal/completed-generation postcondition.
    ++generation;
    {
        AsyncGeneration run{};
        if (!StartGeneration(run, session, applicationPath, generation,
                OutputHostMode::FakeExitDuringStop, supervisorStop, policy) ||
            !WaitReady(session, generation) || !SetEvent(supervisorStop) ||
            !JoinGeneration(run, supervisorStop) ||
            run.result.process.outcome != process::GenerationOutcome::PlannedStop ||
            run.result.outcome != OutputGenerationOutcome::IncompletePlannedStop ||
            run.result.cleanStopConfirmed || !ResultIsRestartSafe(run.result))
        {
            CloseHandle(supervisorStop);
            return 160;
        }
    }

    // O3: a ready child stops all progress, is force-reaped, and a replacement
    // consumes the newest complete value rather than the stale generation.
    ++generation;
    {
        AsyncGeneration stalled{};
        if (!StartGeneration(stalled, session, applicationPath, generation,
                OutputHostMode::FakeStallAfterReady, supervisorStop, policy) ||
            !WaitReady(session, generation))
        {
            CloseHandle(supervisorStop);
            return 170;
        }
        const auto staleReports = MakeReports(60u);
        std::uint64_t staleSequence = 0u;
        if (!PublishReports(session, staleReports, staleSequence) ||
            !JoinGeneration(stalled, supervisorStop) ||
            stalled.result.outcome != OutputGenerationOutcome::ProgressTimeout ||
            !stalled.result.process.forced ||
            !ResultIsRestartSafe(stalled.result))
        {
            CloseHandle(supervisorStop);
            return 171;
        }
    }

    ++generation;
    {
        AsyncGeneration replacement{};
        if (!StartGeneration(replacement, session, applicationPath, generation,
                OutputHostMode::FakeNormal, supervisorStop, policy) ||
            !WaitReady(session, generation))
        {
            CloseHandle(supervisorStop);
            return 172;
        }
        std::uint64_t newestSequence = 0u;
        std::array<XusbReportV1, kMaxPads> newestReports{};
        constexpr std::uint16_t newestSeeds[] = { 71u, 72u, 73u };
        for (const std::uint16_t seed : newestSeeds)
        {
            newestReports = MakeReports(seed);
            if (!PublishReports(session, newestReports, newestSequence))
            {
                CloseHandle(supervisorStop);
                return 173;
            }
        }
        const std::uint64_t newestCheckpoint = ComputeReportCheckpoint(
            newestReports.data(), kMaxPads, (1u << kMaxPads) - 1u);
        if (!WaitApplied(session, generation, newestSequence, newestCheckpoint) ||
            !SetEvent(supervisorStop) ||
            !JoinGeneration(replacement, supervisorStop) ||
            replacement.result.outcome !=
                OutputGenerationOutcome::CleanPlannedStop ||
            !ResultIsRestartSafe(replacement.result))
        {
            CloseHandle(supervisorStop);
            return 174;
        }
    }

    const bool sessionSafe = session.RestartSafe();
    CloseHandle(supervisorStop);
    if (!sessionSafe || !session.Shutdown(error) || error != ERROR_SUCCESS)
        return 180;

    OutputDebugStringW(L"VIGEM_OUTPUT_SELF_HOST_TEST=PASS generations=9 "
        L"o3_reap_restart=1 o4_boundaries=6 clean_stop_qualified=1\n");
    return 0;
}

struct RuntimeStressPublisher
{
    OutputRuntime* runtime = nullptr;
    std::atomic<bool> run{ true };
    std::atomic<std::uint32_t> padCount{ kMaxPads };
    std::atomic<std::uint32_t> failure{ 0u };
    std::atomic<std::uint64_t> accepted{ 0u };
    std::atomic<std::uint64_t> lastSequence{ 0u };
    std::atomic<std::uint64_t> lastCheckpoint{ 0u };
};

DWORD WINAPI RunRuntimeStressPublisher(void* raw) noexcept
{
    auto* publisher = static_cast<RuntimeStressPublisher*>(raw);
    if (!publisher || !publisher->runtime)
        return ERROR_INVALID_PARAMETER;

    while (publisher->run.load(std::memory_order_acquire))
    {
        const std::uint64_t accepted = publisher->accepted.load(
            std::memory_order_relaxed);
        const std::uint32_t padCount = publisher->padCount.load(
            std::memory_order_acquire);
        const auto reports = MakeReports(static_cast<std::uint16_t>(
            (accepted % 0xfffeu) + 1u));
        const std::uint32_t validMask = (1u << padCount) - 1u;
        const std::uint64_t checkpoint = ComputeReportCheckpoint(
            reports.data(), padCount, validMask);
        std::uint64_t sequence = 0u;
        (void)publisher->runtime->PublishProducerProgress(GetTickCount64());
        const OutputPublishResult result = publisher->runtime->TryPublish(
            reports.data(), padCount, validMask, GetTickCount64() * 1000u,
            &sequence);
        if (result == OutputPublishResult::Published)
        {
            if (sequence == 0u)
            {
                publisher->failure.store(1u, std::memory_order_release);
                break;
            }
            if (checkpoint == 0u)
            {
                publisher->failure.store(5u, std::memory_order_release);
                break;
            }
            publisher->lastCheckpoint.store(checkpoint,
                std::memory_order_release);
            publisher->lastSequence.store(sequence, std::memory_order_release);
            publisher->accepted.fetch_add(1u, std::memory_order_release);
        }
        else if (result == OutputPublishResult::InvalidPayload ||
            result == OutputPublishResult::WakeFailed)
        {
            publisher->failure.store(
                result == OutputPublishResult::InvalidPayload ? 2u : 3u,
                std::memory_order_release);
            break;
        }
        SwitchToThread();
    }
    return ERROR_SUCCESS;
}

bool WaitRuntimeReady(OutputRuntime& runtime, std::uint64_t previousGeneration,
    std::uint32_t padCount, OutputRuntimeStatus& observed,
    DWORD timeoutMs = 5000u) noexcept
{
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    do
    {
        observed = runtime.GetStatus();
        if (observed.ready && observed.state == OutputRuntimeState::Ready &&
            observed.activeGeneration > previousGeneration &&
            observed.desiredPadCount == padCount && observed.childPid != 0u)
        {
            return true;
        }
        if (observed.state == OutputRuntimeState::Faulted)
            return false;
        Sleep(1u);
    } while (GetTickCount64() < deadline);
    return false;
}

bool WaitRuntimeDisabled(OutputRuntime& runtime,
    std::uint64_t completedGeneration, DWORD timeoutMs = 5000u) noexcept
{
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    do
    {
        const OutputRuntimeStatus status = runtime.GetStatus();
        if (!status.desiredEnabled &&
            status.state == OutputRuntimeState::Disabled &&
            status.activeGeneration == 0u &&
            status.completedGeneration >= completedGeneration &&
            status.restartSafe)
        {
            return true;
        }
        if (status.state == OutputRuntimeState::Faulted)
            return false;
        Sleep(1u);
    } while (GetTickCount64() < deadline);
    return false;
}

bool WaitRuntimeApplied(OutputRuntime& runtime, std::uint64_t generation,
    std::uint64_t sequence, std::uint64_t checkpoint,
    DWORD timeoutMs = 5000u) noexcept
{
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    do
    {
        const OutputRuntimeStatus status = runtime.GetStatus();
        if (status.ready && status.activeGeneration == generation &&
            status.appliedPublicationSequence == sequence &&
            status.diagnosticCheckpoint == checkpoint)
        {
            return true;
        }
        if (status.state == OutputRuntimeState::Faulted)
            return false;
        Sleep(1u);
    } while (GetTickCount64() < deadline);
    return false;
}

int RunExactRuntimeStressSuite()
{
    constexpr std::uint64_t kRequiredPublications = 100000u;
    constexpr std::uint32_t kTransitions = 100u;

    OutputRuntime runtime;
    std::uint32_t error = ERROR_SUCCESS;
    if (!runtime.Start(true, kMaxPads, error))
        return 301;

    OutputRuntimeStatus current = runtime.GetStatus();
    if (!current.ready || current.activeGeneration == 0u ||
        current.childPid == 0u)
    {
        (void)runtime.Stop(error);
        return 302;
    }

    RuntimeStressPublisher publisher{};
    publisher.runtime = &runtime;
    HANDLE publisherThread = CreateThread(nullptr, 0,
        &RunRuntimeStressPublisher, &publisher, 0u, nullptr);
    if (!publisherThread)
    {
        (void)runtime.Stop(error);
        return 303;
    }

    auto finishFailure = [&](int code) noexcept
    {
        publisher.run.store(false, std::memory_order_release);
        const DWORD joined = WaitForSingleObject(publisherThread, 10000u);
        CloseHandle(publisherThread);
        std::uint32_t ignored = ERROR_SUCCESS;
        (void)runtime.Stop(ignored);
        return joined == WAIT_OBJECT_0 ? code : 399;
    };

    std::uint32_t padCount = kMaxPads;
    std::uint64_t generation = current.activeGeneration;
    std::uint32_t childPid = current.childPid;
    for (std::uint32_t transition = 0u; transition < kTransitions; ++transition)
    {
        const std::uint32_t publisherFailure = publisher.failure.load(
            std::memory_order_acquire);
        if (publisherFailure != 0u)
            return finishFailure(static_cast<int>(320u + publisherFailure));

        const bool disableBoundary = (transition % 10u) == 9u;
        if (disableBoundary)
        {
            runtime.Configure(false, padCount);
            if (!WaitRuntimeDisabled(runtime, generation) ||
                !ChildIsReaped(childPid))
            {
                return finishFailure(305);
            }
            padCount = (padCount % kMaxPads) + 1u;
            publisher.padCount.store(padCount, std::memory_order_release);
            runtime.Configure(true, padCount);
        }
        else if ((transition % 2u) == 0u)
        {
            runtime.RequestRestart();
        }
        else
        {
            padCount = (padCount % kMaxPads) + 1u;
            // Move the producer first on purpose. The old generation must
            // reject this boundary value without faulting or blocking it.
            publisher.padCount.store(padCount, std::memory_order_release);
            runtime.Configure(true, padCount);
        }

        OutputRuntimeStatus next{};
        if (!WaitRuntimeReady(runtime, generation, padCount, next) ||
            !ChildIsReaped(childPid) || !next.restartSafe ||
            next.sessionRebuildCount != 0u ||
            next.lastUnsafeGeneration != 0u)
        {
            return finishFailure(306);
        }
        generation = next.activeGeneration;
        childPid = next.childPid;
    }

    const ULONGLONG publicationDeadline = GetTickCount64() + 30000u;
    while (publisher.accepted.load(std::memory_order_acquire) <
            kRequiredPublications &&
        publisher.failure.load(std::memory_order_acquire) == 0u &&
        GetTickCount64() < publicationDeadline)
    {
        Sleep(1u);
    }
    publisher.run.store(false, std::memory_order_release);
    if (WaitForSingleObject(publisherThread, 10000u) != WAIT_OBJECT_0)
    {
        CloseHandle(publisherThread);
        (void)runtime.Stop(error);
        return 307;
    }
    CloseHandle(publisherThread);

    const std::uint64_t accepted = publisher.accepted.load(
        std::memory_order_acquire);
    const std::uint64_t lastSequence = publisher.lastSequence.load(
        std::memory_order_acquire);
    const std::uint64_t lastCheckpoint = publisher.lastCheckpoint.load(
        std::memory_order_acquire);
    if (publisher.failure.load(std::memory_order_acquire) != 0u ||
        accepted < kRequiredPublications || lastSequence == 0u ||
        lastCheckpoint == 0u)
    {
        (void)runtime.Stop(error);
        return 308;
    }
    if (!WaitRuntimeApplied(runtime, generation, lastSequence, lastCheckpoint))
    {
        (void)runtime.Stop(error);
        return 309;
    }

    if (!runtime.Stop(error) || error != ERROR_SUCCESS ||
        !ChildIsReaped(childPid))
    {
        return 310;
    }
    const OutputRuntimeStatus stopped = runtime.GetStatus();
    if (stopped.state != OutputRuntimeState::Stopped ||
        stopped.activeGeneration != 0u || !stopped.restartSafe)
    {
        return 311;
    }

    wchar_t message[256]{};
    swprintf_s(message,
        L"VIGEM_OUTPUT_RUNTIME_STRESS_TEST=PASS publications=%llu "
        L"generations=%lu topology_changes=50 disable_enable=10 survivors=0\n",
        static_cast<unsigned long long>(accepted),
        static_cast<unsigned long>(kTransitions + 1u));
    OutputDebugStringW(message);
    return 0;
}

int RunExactRealChildSuite()
{
    wchar_t applicationPath[32768]{};
    const DWORD pathLength = GetModuleFileNameW(nullptr, applicationPath,
        static_cast<DWORD>(std::size(applicationPath)));
    if (pathLength == 0u || pathLength >= std::size(applicationPath) - 1u)
        return 201;

    OutputProcessSession session;
    std::uint32_t error = ERROR_SUCCESS;
    if (!session.Initialize(error) || session.OwnerPid() != GetCurrentProcessId() ||
        session.LaunchNonce() == 0u)
    {
        return 202;
    }

    HANDLE supervisorStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!supervisorStop)
        return 203;

    process::SupervisionPolicy policy{};
    policy.startupTimeoutMs = 6000u;
    policy.progressTimeoutMs = 3000u;
    policy.observationIntervalMs = 2u;
    policy.gracefulStopTimeoutMs = 6000u;
    policy.hardReapTimeoutMs = 3000u;

    constexpr std::uint64_t generation = 0xF2000001ull;
    AsyncGeneration run{};
    if (!StartGeneration(run, session, applicationPath, generation,
            OutputHostMode::Real, supervisorStop, policy) ||
        !WaitReady(session, generation, 6000u))
    {
        SetEvent(supervisorStop);
        (void)JoinGeneration(run, supervisorStop, 10000u);
        CloseHandle(supervisorStop);
        return 210;
    }

    const auto reports = MakeReports(0x125u);
    std::uint64_t sequence = 0u;
    const std::uint64_t checkpoint = ComputeReportCheckpoint(reports.data(),
        kMaxPads, (1u << kMaxPads) - 1u);
    if (!PublishReports(session, reports, sequence) ||
        !WaitApplied(session, generation, sequence, checkpoint, 6000u))
    {
        SetEvent(supervisorStop);
        (void)JoinGeneration(run, supervisorStop, 10000u);
        CloseHandle(supervisorStop);
        return 211;
    }

    if (!SetEvent(supervisorStop) ||
        !JoinGeneration(run, supervisorStop, 12000u) ||
        run.result.outcome != OutputGenerationOutcome::CleanPlannedStop ||
        !run.result.cleanStopConfirmed || !run.result.neutralApplied ||
        !run.result.targetsRemoved || !run.result.finalTelemetryAvailable ||
        run.result.finalTelemetry.appliedConfigurationGeneration != generation ||
        run.result.finalTelemetry.appliedPublicationSequence != sequence ||
        run.result.finalTelemetry.diagnosticCheckpoint != checkpoint ||
        !ResultIsRestartSafe(run.result))
    {
        CloseHandle(supervisorStop);
        return 212;
    }

    const bool sessionSafe = session.RestartSafe();
    CloseHandle(supervisorStop);
    if (!sessionSafe || !session.Shutdown(error) || error != ERROR_SUCCESS)
        return 213;

    OutputDebugStringW(L"VIGEM_OUTPUT_REAL_CHILD_TEST=PASS generations=1 "
        L"pads=4 apply=1 neutral=4 remove=4 survivors=0\n");
    return 0;
}
#endif
} // namespace

bool VigemOutputSelfHostTest_TryRunCommand(int& exitCode) noexcept
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return false;
    const bool fakeRequested = argc > 1 &&
        _wcsicmp(argv[1], kOutputSelfTestArgument) == 0;
    const bool realRequested = argc > 1 &&
        _wcsicmp(argv[1], kOutputRealSelfTestArgument) == 0;
    const bool stressRequested = argc > 1 &&
        _wcsicmp(argv[1], kOutputRuntimeStressTestArgument) == 0;
    const bool requested = fakeRequested || realRequested || stressRequested;
    const bool exact = requested && argc == 2;
    LocalFree(argv);
    if (!requested)
        return false;
    if (!exact)
    {
        exitCode = 90;
        return true;
    }
#if defined(HALLJOY_ANALOG_SIMULATOR)
    StabilityTrace_Init();
    try
    {
        if (realRequested)
            exitCode = RunExactRealChildSuite();
        else if (stressRequested)
            exitCode = RunExactRuntimeStressSuite();
        else
            exitCode = RunExactSelfHostSuite();
    }
    catch (const std::bad_alloc&)
    {
        exitCode = 92;
    }
    catch (...)
    {
        exitCode = 93;
    }
    StabilityTrace_Shutdown(exitCode);
#else
    exitCode = 91;
#endif
    return true;
}
