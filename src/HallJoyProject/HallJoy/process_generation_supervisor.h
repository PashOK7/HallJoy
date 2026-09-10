#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "protected_native_handle.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace halljoy::process
{
#if defined(_WIN32)
using NativeHandle = HANDLE;
#else
using NativeHandle = void*;
#endif

enum class GenerationOutcome : std::uint32_t
{
    PlannedStop = 0,
    ExitBeforeReady,
    UnexpectedExit,
    StartupTimeout,
    ProgressTimeout,
    ChildFault,
    ProtocolViolation,
    LaunchFailed,
    WaitFailed,
    ReapFailed,
    Unsupported,
};

struct ChildObservation
{
    bool available = false;
    bool ready = false;
    bool fatal = false;
    std::uint32_t childPid = 0;
    std::uint32_t error = 0;
    std::uint64_t generation = 0;
    std::uint64_t progress = 0;
};

using ProbeFunction = bool (*)(void* context, ChildObservation& observation) noexcept;
using PrepareTerminationFunction = bool (*)(
    void* context, std::uint32_t& error) noexcept;

struct LaunchRequest
{
    std::wstring applicationPath;
    std::wstring commandLine;
    std::vector<NativeHandle> inheritedHandles;
    std::uint64_t generation = 0;
    std::uint32_t creationFlags = 0;
};

struct SupervisionPolicy
{
    std::uint32_t startupTimeoutMs = 3000;
    std::uint32_t progressTimeoutMs = 3000;
    std::uint32_t observationIntervalMs = 10;
    std::uint32_t gracefulStopTimeoutMs = 2500;
    std::uint32_t hardReapTimeoutMs = 3000;
    std::uint32_t forcedExitCode = 0xE0564A4Fu;
};

struct SupervisionRequest
{
    LaunchRequest launch;
    NativeHandle childStopEvent = nullptr;
    NativeHandle supervisorStopEvent = nullptr;
    ProbeFunction probe = nullptr;
    void* probeContext = nullptr;
    PrepareTerminationFunction prepareTermination = nullptr;
    void* prepareTerminationContext = nullptr;
    SupervisionPolicy policy{};
};

struct SupervisionResult
{
    GenerationOutcome outcome = GenerationOutcome::LaunchFailed;
    std::uint64_t generation = 0;
    std::uint32_t childPid = 0;
    std::uint32_t childExitCode = 259;
    std::uint32_t nativeError = 0;
    bool readyObserved = false;
    bool forced = false;
    bool terminationPreparationAttempted = false;
    bool terminationPrepared = true;
    bool restartSafe = true;
    bool processHandleProtected = false;
    bool jobHandleProtected = false;
    bool processIdentityMatched = false;
    std::uint32_t ownershipError = 0;
};

// Synchronous one-generation supervisor intended to run on a dedicated owner
// thread. It never runs on realtime and exposes no raw process/job handle.
class ProcessGenerationSupervisor final
{
public:
    ProcessGenerationSupervisor() noexcept = default;
    ~ProcessGenerationSupervisor() noexcept;

    ProcessGenerationSupervisor(const ProcessGenerationSupervisor&) = delete;
    ProcessGenerationSupervisor& operator=(const ProcessGenerationSupervisor&) = delete;
    ProcessGenerationSupervisor(ProcessGenerationSupervisor&&) = delete;
    ProcessGenerationSupervisor& operator=(ProcessGenerationSupervisor&&) = delete;

    SupervisionResult RunOne(const SupervisionRequest& request) noexcept;

    bool HasUnreapedGeneration() const noexcept;
    bool RestartSafe() const noexcept;
    std::uint64_t ActiveGeneration() const noexcept;
    std::uint32_t ActiveChildPid() const noexcept;

private:
    bool LaunchContained(const LaunchRequest& request, std::uint32_t& error) noexcept;
    bool Reap(std::uint32_t timeoutMs, std::uint32_t& exitCode,
        std::uint32_t& error) noexcept;
    bool ForceAndReap(std::uint32_t forcedExitCode, std::uint32_t timeoutMs,
        std::uint32_t& exitCode, std::uint32_t& error) noexcept;
    bool StopAndReap(NativeHandle stopEvent, const SupervisionPolicy& policy,
        std::uint32_t& exitCode, std::uint32_t& error, bool& forced) noexcept;
    bool ReleaseConfirmed(std::uint32_t& error) noexcept;

    ProtectedNativeHandle process_;
    ProtectedNativeHandle job_;
    std::uint64_t generation_ = 0;
    std::uint32_t childPid_ = 0;
    bool restartSafe_ = true;
};
}
