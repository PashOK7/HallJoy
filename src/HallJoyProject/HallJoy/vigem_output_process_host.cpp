#include "vigem_output_process_host.h"

#include "vigem_child_transport.h"
#include "vigem_output_channel.h"
#include "vigem_output_process_protocol.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <cstdint>
#include <cwchar>
#include <string_view>

#pragma comment(lib, "shell32.lib")

namespace
{
using namespace halljoy::vigem_output;

constexpr int kMalformedCommandExit = 60;
constexpr int kUnsupportedTransportExit = 61;
constexpr int kInvalidHandleExit = 62;
constexpr int kInvalidMappingExit = 63;
constexpr int kIdentityRejectedExit = 64;
constexpr int kContainmentRejectedExit = 65;
constexpr int kStartingRejectedExit = 66;
constexpr int kReadyRejectedExit = 67;
constexpr int kWaitFailedExit = 68;
constexpr int kTelemetryRejectedExit = 69;
constexpr int kExitBeforeReady = 70;
constexpr int kExitAfterReady = 71;
constexpr int kExitBeforeSnapshotRead = 72;
constexpr int kExitDuringUpdate = 73;
constexpr int kExitAfterAcknowledgement = 74;
constexpr int kExitDuringStop = 75;
constexpr int kOwnerExited = 76;
constexpr int kConfigurationRejectedExit = 77;
constexpr int kRealTransportStartFailedExit = 78;
constexpr int kRealTransportUpdateFailedExit = 79;
constexpr int kRealTransportStopFailedExit = 80;

struct HostLaunchContext
{
    std::uint32_t ownerPid = 0;
    std::uint64_t nonce = 0;
    std::uint64_t generation = 0;
    HANDLE mapping = nullptr;
    HANDLE wakeEvent = nullptr;
    HANDLE stopEvent = nullptr;
    HANDLE ownerProcess = nullptr;
    OutputHostMode mode = OutputHostMode::Real;
};

struct HostResources
{
    explicit HostResources(const HostLaunchContext& launch) noexcept
        : mapping(launch.mapping), wakeEvent(launch.wakeEvent),
          stopEvent(launch.stopEvent), ownerProcess(launch.ownerProcess)
    {
    }

    ~HostResources() noexcept
    {
        if (shared)
            UnmapViewOfFile(shared);
        if (ownerProcess)
            CloseHandle(ownerProcess);
        if (stopEvent)
            CloseHandle(stopEvent);
        if (wakeEvent)
            CloseHandle(wakeEvent);
        if (mapping)
            CloseHandle(mapping);
    }

    HANDLE mapping = nullptr;
    HANDLE wakeEvent = nullptr;
    HANDLE stopEvent = nullptr;
    HANDLE ownerProcess = nullptr;
    SharedStateV1* shared = nullptr;
};

bool ParseUnsigned(const wchar_t* text, int base, std::uint64_t& value) noexcept
{
    if (!text || !*text)
        return false;
    wchar_t* end = nullptr;
    const unsigned long long parsed = _wcstoui64(text, &end, base);
    if (end == text || *end != L'\0')
        return false;
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool ParseHandle(const wchar_t* text, HANDLE& handle) noexcept
{
    std::uint64_t value = 0u;
    if (!ParseUnsigned(text, 16, value) || value == 0u || value > UINTPTR_MAX)
        return false;
    handle = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(value));
    return true;
}

bool ParseExactHostCommand(int argc, wchar_t** argv,
    HostLaunchContext& launch) noexcept
{
    if (argc != 10 || !argv || _wcsicmp(argv[1], kOutputHostArgument) != 0)
        return false;

    std::uint64_t ownerPid = 0u;
    if (!ParseUnsigned(argv[2], 10, ownerPid) || ownerPid == 0u ||
        ownerPid > UINT32_MAX ||
        !ParseUnsigned(argv[3], 16, launch.nonce) || launch.nonce == 0u ||
        !ParseUnsigned(argv[4], 10, launch.generation) ||
        launch.generation == 0u ||
        !ParseHandle(argv[5], launch.mapping) ||
        !ParseHandle(argv[6], launch.wakeEvent) ||
        !ParseHandle(argv[7], launch.stopEvent) ||
        !ParseHandle(argv[8], launch.ownerProcess) ||
        !ParseOutputHostMode(argv[9], launch.mode))
    {
        return false;
    }
    launch.ownerPid = static_cast<std::uint32_t>(ownerPid);
    return true;
}

int PrepareHostResources(const HostLaunchContext& launch,
    HostResources& resources) noexcept
{
    DWORD handleFlags = 0u;
    if (!GetHandleInformation(resources.mapping, &handleFlags) ||
        !GetHandleInformation(resources.wakeEvent, &handleFlags) ||
        !GetHandleInformation(resources.stopEvent, &handleFlags) ||
        !GetHandleInformation(resources.ownerProcess, &handleFlags))
    {
        return kInvalidHandleExit;
    }

    BOOL contained = FALSE;
    if (!IsProcessInJob(GetCurrentProcess(), nullptr, &contained) || !contained)
        return kContainmentRejectedExit;

    resources.shared = static_cast<SharedStateV1*>(MapViewOfFile(
        resources.mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedStateV1)));
    if (!resources.shared)
        return kInvalidMappingExit;

    if (!ValidateSharedState(*resources.shared, launch.ownerPid, launch.nonce) ||
        ActiveOutputGeneration(*resources.shared) != launch.generation ||
        GetProcessId(resources.ownerProcess) != launch.ownerPid ||
        WaitForSingleObject(resources.ownerProcess, 0u) != WAIT_TIMEOUT)
    {
        return kIdentityRejectedExit;
    }
    return 0;
}

bool ReadLaunchConfiguration(const HostLaunchContext& launch,
    HostResources& resources, std::uint32_t childPid,
    OutputConfigurationV1& configuration) noexcept
{
    if (ReadOutputConfiguration(*resources.shared, launch.generation,
        configuration))
    {
        return true;
    }
    (void)ChildPublishFault(*resources.shared, launch.generation, childPid,
        ERROR_INVALID_DATA, 3u, GetTickCount64());
    return false;
}

ProducerLeaseState CurrentProducerLeaseState(SharedStateV1& shared,
    std::uint64_t generation, std::uint64_t nowMs, std::uint64_t readyTickMs,
    ProducerLeaseViewV1& lease) noexcept
{
    return ReadProducerLeaseState(shared, generation, nowMs, readyTickMs, &lease);
}

bool SnapshotMayApply(const SnapshotPayloadV1& snapshot,
    const ProducerLeaseViewV1& lease, ProducerLeaseState state,
    std::uint64_t lastNeutralizedLeaseSequence) noexcept
{
    return state == ProducerLeaseState::Fresh &&
        snapshot.producerLeaseSequence > lastNeutralizedLeaseSequence &&
        snapshot.producerLeaseSequence <= lease.sequence;
}

SnapshotPayloadV1 MakeNeutralSnapshot(std::uint64_t generation,
    std::uint32_t padCount) noexcept
{
    SnapshotPayloadV1 neutral{};
    neutral.outputGeneration = generation;
    neutral.padCount = padCount;
    neutral.validMask = (1u << padCount) - 1u;
    return neutral;
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
int RunFakeHost(const HostLaunchContext& launch) noexcept
{
    if (launch.mode == OutputHostMode::Real)
        return kUnsupportedTransportExit;

    HostResources resources(launch);
    const int preparation = PrepareHostResources(launch, resources);
    if (preparation != 0)
        return preparation;

    const std::uint32_t childPid = GetCurrentProcessId();
    if (!ChildPublishStarting(*resources.shared, launch.generation, childPid,
        GetTickCount64(), kChildDiagnosticContainedBeforeEntry))
    {
        return kStartingRejectedExit;
    }
    if (launch.mode == OutputHostMode::FakeExitBeforeReady)
        return kExitBeforeReady;

    OutputConfigurationV1 configuration{};
    if (!ReadLaunchConfiguration(launch, resources, childPid, configuration))
        return kConfigurationRejectedExit;
    if (!ChildPublishReady(*resources.shared, launch.generation, childPid,
        configuration.generation, GetTickCount64()))
    {
        return kReadyRejectedExit;
    }
    if (launch.mode == OutputHostMode::FakeExitAfterReady)
        return kExitAfterReady;
    if (launch.mode == OutputHostMode::FakeStallAfterReady)
    {
        (void)WaitForSingleObject(resources.ownerProcess, INFINITE);
        return kOwnerExited;
    }

    std::uint64_t appliedSequence = 0u;
    std::uint64_t lastNeutralizedLeaseSequence = 0u;
    bool producerNeutralized = false;
    const std::uint64_t readyTickMs = GetTickCount64();
    for (;;)
    {
        HANDLE waits[] = {
            resources.stopEvent, resources.wakeEvent, resources.ownerProcess
        };
        const DWORD wait = WaitForMultipleObjects(3u, waits, FALSE, 25u);
        if (wait == WAIT_OBJECT_0)
        {
            if (!ChildPublishStopping(*resources.shared, launch.generation,
                childPid, GetTickCount64()))
            {
                return kTelemetryRejectedExit;
            }
            if (launch.mode == OutputHostMode::FakeExitDuringStop)
                return kExitDuringStop;
            if (!ChildPublishStopped(*resources.shared, launch.generation,
                childPid, kChildDiagnosticNeutralApplied |
                    kChildDiagnosticTargetsRemoved, GetTickCount64()))
            {
                return kTelemetryRejectedExit;
            }
            return 0;
        }
        if (wait == WAIT_OBJECT_0 + 1u)
        {
            if (launch.mode == OutputHostMode::FakeExitBeforeSnapshotRead)
                return kExitBeforeSnapshotRead;

            SnapshotPayloadV1 snapshot{};
            const ConsumeResult consumed = TryConsumeNewestSnapshot(
                *resources.shared, launch.generation, appliedSequence, &snapshot);
            if (consumed == ConsumeResult::InvalidArgument)
            {
                (void)ChildPublishFault(*resources.shared, launch.generation,
                    childPid, ERROR_INVALID_DATA, 1u, GetTickCount64());
                return kTelemetryRejectedExit;
            }
            if (consumed == ConsumeResult::NoUpdate)
                continue;
            if (launch.mode == OutputHostMode::FakeExitDuringUpdate)
                return kExitDuringUpdate;

            ProducerLeaseViewV1 lease{};
            const ProducerLeaseState leaseState = CurrentProducerLeaseState(
                *resources.shared, launch.generation, GetTickCount64(),
                readyTickMs, lease);
            if (!SnapshotMayApply(snapshot, lease, leaseState,
                lastNeutralizedLeaseSequence))
            {
                appliedSequence = snapshot.publicationSequence;
                continue;
            }

            const std::uint64_t checkpoint = ComputeReportCheckpoint(
                snapshot.reports, snapshot.padCount, snapshot.validMask);
            if (checkpoint == 0u ||
                !ChildPublishAppliedSnapshot(*resources.shared,
                    launch.generation, childPid, snapshot.publicationSequence,
                    checkpoint, GetTickCount64()))
            {
                return kTelemetryRejectedExit;
            }
            appliedSequence = snapshot.publicationSequence;
            producerNeutralized = false;
            if (launch.mode == OutputHostMode::FakeExitAfterAcknowledgement)
                return kExitAfterAcknowledgement;
            continue;
        }
        if (wait == WAIT_OBJECT_0 + 2u)
            return kOwnerExited;
        if (wait == WAIT_TIMEOUT)
        {
            ProducerLeaseViewV1 lease{};
            const ProducerLeaseState leaseState = CurrentProducerLeaseState(
                *resources.shared, launch.generation, GetTickCount64(),
                readyTickMs, lease);
            if (leaseState == ProducerLeaseState::Stalled && !producerNeutralized)
            {
                if (!ChildPublishProducerStalled(*resources.shared,
                    launch.generation, childPid, lease.sequence, GetTickCount64()))
                {
                    return kTelemetryRejectedExit;
                }
                lastNeutralizedLeaseSequence = lease.sequence;
                producerNeutralized = true;
            }
            if (!ChildPublishHeartbeat(*resources.shared, launch.generation,
                childPid, GetTickCount64()))
            {
                return kTelemetryRejectedExit;
            }
            continue;
        }

        const std::uint32_t error = GetLastError();
        (void)ChildPublishFault(*resources.shared, launch.generation, childPid,
            error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error,
            2u, GetTickCount64());
        return kWaitFailedExit;
    }
}
#endif

int RunRealHost(const HostLaunchContext& launch) noexcept
{
    HostResources resources(launch);
    const int preparation = PrepareHostResources(launch, resources);
    if (preparation != 0)
        return preparation;

    const std::uint32_t childPid = GetCurrentProcessId();
    if (!ChildPublishStarting(*resources.shared, launch.generation, childPid,
        GetTickCount64(), kChildDiagnosticContainedBeforeEntry))
    {
        return kStartingRejectedExit;
    }

    OutputConfigurationV1 configuration{};
    if (!ReadLaunchConfiguration(launch, resources, childPid, configuration))
        return kConfigurationRejectedExit;

    VigemChildTransport transport(RealVigemApi());
    const VigemTransportResult started = transport.Start(configuration.padCount);
    if (!started.Succeeded())
    {
        (void)ChildPublishFault(*resources.shared, launch.generation, childPid,
            static_cast<std::uint32_t>(started.error),
            EncodeVigemTransportCheckpoint(started.phase, started.padIndex),
            GetTickCount64());
        return kRealTransportStartFailedExit;
    }

    if (!ChildPublishReady(*resources.shared, launch.generation, childPid,
        configuration.generation, GetTickCount64()))
    {
        return kReadyRejectedExit;
    }

    std::uint64_t appliedSequence = 0u;
    std::uint64_t lastNeutralizedLeaseSequence = 0u;
    bool producerNeutralized = false;
    const std::uint64_t readyTickMs = GetTickCount64();
    for (;;)
    {
        HANDLE waits[] = {
            resources.stopEvent, resources.wakeEvent, resources.ownerProcess
        };
        const DWORD wait = WaitForMultipleObjects(3u, waits, FALSE, 25u);
        if (wait == WAIT_OBJECT_0)
        {
            if (!ChildPublishStopping(*resources.shared, launch.generation,
                childPid, GetTickCount64()))
            {
                return kTelemetryRejectedExit;
            }

            const VigemTransportResult stopped = transport.Stop();
            std::uint32_t completionFlags = 0u;
            if (stopped.neutralApplied)
                completionFlags |= kChildDiagnosticNeutralApplied;
            if (stopped.targetsRemoved)
                completionFlags |= kChildDiagnosticTargetsRemoved;
            if (!stopped.Succeeded() || !stopped.neutralApplied ||
                !stopped.targetsRemoved)
            {
                const std::uint32_t error = stopped.Succeeded()
                    ? ERROR_GEN_FAILURE
                    : static_cast<std::uint32_t>(stopped.error);
                (void)ChildPublishFault(*resources.shared, launch.generation,
                    childPid, error, EncodeVigemTransportCheckpoint(
                        stopped.phase, stopped.padIndex), GetTickCount64(),
                    completionFlags);
                return kRealTransportStopFailedExit;
            }
            if (!ChildPublishStopped(*resources.shared, launch.generation,
                childPid, completionFlags, GetTickCount64()))
            {
                return kTelemetryRejectedExit;
            }
            return 0;
        }
        if (wait == WAIT_OBJECT_0 + 1u)
        {
            SnapshotPayloadV1 snapshot{};
            const ConsumeResult consumed = TryConsumeNewestSnapshot(
                *resources.shared, launch.generation, appliedSequence, &snapshot);
            if (consumed == ConsumeResult::InvalidArgument)
            {
                (void)ChildPublishFault(*resources.shared, launch.generation,
                    childPid, ERROR_INVALID_DATA, 1u, GetTickCount64());
                return kTelemetryRejectedExit;
            }
            if (consumed == ConsumeResult::NoUpdate)
                continue;

            ProducerLeaseViewV1 lease{};
            const ProducerLeaseState leaseState = CurrentProducerLeaseState(
                *resources.shared, launch.generation, GetTickCount64(),
                readyTickMs, lease);
            if (!SnapshotMayApply(snapshot, lease, leaseState,
                lastNeutralizedLeaseSequence))
            {
                appliedSequence = snapshot.publicationSequence;
                continue;
            }

            const VigemTransportResult applied = transport.Apply(snapshot);
            if (!applied.Succeeded())
            {
                (void)ChildPublishFault(*resources.shared, launch.generation,
                    childPid, static_cast<std::uint32_t>(applied.error),
                    EncodeVigemTransportCheckpoint(
                        applied.phase, applied.padIndex), GetTickCount64());
                return kRealTransportUpdateFailedExit;
            }

            const std::uint64_t checkpoint = ComputeReportCheckpoint(
                snapshot.reports, snapshot.padCount, snapshot.validMask);
            if (checkpoint == 0u ||
                !ChildPublishAppliedSnapshot(*resources.shared,
                    launch.generation, childPid, snapshot.publicationSequence,
                    checkpoint, GetTickCount64()))
            {
                return kTelemetryRejectedExit;
            }
            appliedSequence = snapshot.publicationSequence;
            producerNeutralized = false;
            continue;
        }
        if (wait == WAIT_OBJECT_0 + 2u)
            return kOwnerExited;
        if (wait == WAIT_TIMEOUT)
        {
            ProducerLeaseViewV1 lease{};
            const ProducerLeaseState leaseState = CurrentProducerLeaseState(
                *resources.shared, launch.generation, GetTickCount64(),
                readyTickMs, lease);
            if (leaseState == ProducerLeaseState::Stalled && !producerNeutralized)
            {
                const VigemTransportResult neutral = transport.Apply(
                    MakeNeutralSnapshot(launch.generation, configuration.padCount));
                if (!neutral.Succeeded())
                {
                    (void)ChildPublishFault(*resources.shared, launch.generation,
                        childPid, static_cast<std::uint32_t>(neutral.error),
                        EncodeVigemTransportCheckpoint(neutral.phase, neutral.padIndex),
                        GetTickCount64());
                    return kRealTransportUpdateFailedExit;
                }
                if (!ChildPublishProducerStalled(*resources.shared,
                    launch.generation, childPid, lease.sequence, GetTickCount64()))
                {
                    return kTelemetryRejectedExit;
                }
                lastNeutralizedLeaseSequence = lease.sequence;
                producerNeutralized = true;
            }
            if (!ChildPublishHeartbeat(*resources.shared, launch.generation,
                childPid, GetTickCount64()))
            {
                return kTelemetryRejectedExit;
            }
            continue;
        }

        const std::uint32_t error = GetLastError();
        (void)ChildPublishFault(*resources.shared, launch.generation, childPid,
            error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error,
            2u, GetTickCount64());
        return kWaitFailedExit;
    }
}
} // namespace

bool VigemOutputHost_TryRunCommand(int& exitCode) noexcept
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return false;

    const bool requested = argc > 1 &&
        _wcsicmp(argv[1], kOutputHostArgument) == 0;
    if (!requested)
    {
        LocalFree(argv);
        return false;
    }

    HostLaunchContext launch{};
    const bool parsed = ParseExactHostCommand(argc, argv, launch);
    LocalFree(argv);
    if (!parsed)
    {
        exitCode = kMalformedCommandExit;
        return true;
    }

    if (launch.mode == OutputHostMode::Real)
    {
        exitCode = RunRealHost(launch);
        return true;
    }
#if defined(HALLJOY_ANALOG_SIMULATOR)
    exitCode = RunFakeHost(launch);
#else
    exitCode = kUnsupportedTransportExit;
#endif
    return true;
}
