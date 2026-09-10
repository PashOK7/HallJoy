#include "process_generation_supervisor.h"

#include <algorithm>
#include <limits>
#include <new>

#if defined(_WIN32)
#include <cwchar>
#endif

namespace halljoy::process
{
#if defined(_WIN32)
namespace
{
constexpr std::size_t kMaxInheritedHandles = 32u;
constexpr DWORD kStillActive = STILL_ACTIVE;

bool IsExplicitHandleListValid(const std::vector<NativeHandle>& handles,
    std::uint32_t& error) noexcept
{
    if (handles.empty() || handles.size() > kMaxInheritedHandles)
    {
        error = ERROR_INVALID_PARAMETER;
        return false;
    }

    for (std::size_t index = 0; index < handles.size(); ++index)
    {
        const HANDLE handle = handles[index];
        if (!handle || handle == INVALID_HANDLE_VALUE)
        {
            error = ERROR_INVALID_HANDLE;
            return false;
        }
        DWORD flags = 0;
        if (!GetHandleInformation(handle, &flags))
        {
            error = GetLastError();
            return false;
        }
        if ((flags & HANDLE_FLAG_INHERIT) == 0u)
        {
            error = ERROR_ACCESS_DENIED;
            return false;
        }
        if (std::find(handles.begin(), handles.begin() + index, handle) !=
            handles.begin() + index)
        {
            error = ERROR_INVALID_PARAMETER;
            return false;
        }
    }
    error = ERROR_SUCCESS;
    return true;
}

DWORD BoundedInterval(const SupervisionPolicy& policy) noexcept
{
    return std::max<DWORD>(1u, policy.observationIntervalMs);
}
}
#endif

ProcessGenerationSupervisor::~ProcessGenerationSupervisor() noexcept
{
#if defined(_WIN32)
    if (process_)
    {
        if (job_)
            TerminateJobObject(job_.Get(), 0xE0564454u);
        else
            TerminateProcess(process_.Get(), 0xE0564454u);
        WaitForSingleObject(process_.Get(), 1000u);
    }
    std::uint32_t ignored = ERROR_SUCCESS;
    (void)process_.Close(ignored);
    (void)job_.Close(ignored); // KILL_ON_JOB_CLOSE remains final containment.
#endif
    generation_ = 0;
    childPid_ = 0;
    restartSafe_ = false;
}

bool ProcessGenerationSupervisor::HasUnreapedGeneration() const noexcept
{
    return static_cast<bool>(process_);
}

bool ProcessGenerationSupervisor::RestartSafe() const noexcept
{
    return restartSafe_ && !process_ && !job_;
}

std::uint64_t ProcessGenerationSupervisor::ActiveGeneration() const noexcept
{
    return generation_;
}

std::uint32_t ProcessGenerationSupervisor::ActiveChildPid() const noexcept
{
    return childPid_;
}

bool ProcessGenerationSupervisor::ReleaseConfirmed(std::uint32_t& error) noexcept
{
    error = 0u;
#if defined(_WIN32)
    std::uint32_t processError = ERROR_SUCCESS;
    std::uint32_t jobError = ERROR_SUCCESS;
    const bool processClosed = process_.Close(processError);
    const bool jobClosed = job_.Close(jobError);
    if (!processClosed || !jobClosed)
    {
        error = !processClosed ? processError : jobError;
        restartSafe_ = false;
        return false;
    }
#endif
    generation_ = 0;
    childPid_ = 0;
    return true;
}

bool ProcessGenerationSupervisor::LaunchContained(
    const LaunchRequest& request, std::uint32_t& error) noexcept
{
#if !defined(_WIN32)
    (void)request;
    error = 0;
    return false;
#else
    error = ERROR_SUCCESS;
    if (process_ || job_ || !restartSafe_ || request.applicationPath.empty() ||
        request.commandLine.empty() || request.generation == 0u ||
        !IsExplicitHandleListValid(request.inheritedHandles, error))
    {
        if (error == ERROR_SUCCESS)
            error = ERROR_INVALID_STATE;
        return false;
    }

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job)
    {
        error = GetLastError();
        return false;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
        &limits, sizeof(limits)))
    {
        error = GetLastError();
        CloseHandle(job);
        return false;
    }

    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    if (attributeBytes == 0u)
    {
        error = GetLastError();
        CloseHandle(job);
        return false;
    }

    void* attributeStorage = HeapAlloc(GetProcessHeap(), 0, attributeBytes);
    if (!attributeStorage)
    {
        error = ERROR_NOT_ENOUGH_MEMORY;
        CloseHandle(job);
        return false;
    }
    std::vector<wchar_t> commandLine;
    try
    {
        commandLine.assign(request.commandLine.begin(), request.commandLine.end());
        commandLine.push_back(L'\0');
    }
    catch (...)
    {
        error = ERROR_NOT_ENOUGH_MEMORY;
        HeapFree(GetProcessHeap(), 0, attributeStorage);
        CloseHandle(job);
        return false;
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(
        attributeStorage);
    if (!InitializeProcThreadAttributeList(
        startup.lpAttributeList, 1, 0, &attributeBytes))
    {
        error = GetLastError();
        HeapFree(GetProcessHeap(), 0, attributeStorage);
        CloseHandle(job);
        return false;
    }
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        const_cast<NativeHandle*>(request.inheritedHandles.data()),
        request.inheritedHandles.size() * sizeof(NativeHandle), nullptr, nullptr))
    {
        error = GetLastError();
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, attributeStorage);
        CloseHandle(job);
        return false;
    }

    PROCESS_INFORMATION process{};
    const DWORD flags = request.creationFlags | CREATE_SUSPENDED |
        EXTENDED_STARTUPINFO_PRESENT;
    const BOOL created = CreateProcessW(request.applicationPath.c_str(),
        commandLine.data(), nullptr, nullptr, TRUE, flags, nullptr, nullptr,
        &startup.StartupInfo, &process);
    error = created ? ERROR_SUCCESS : GetLastError();
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, attributeStorage);
    if (!created)
    {
        CloseHandle(job);
        return false;
    }

    generation_ = request.generation;
    childPid_ = process.dwProcessId;

    std::uint32_t adoptionError = ERROR_SUCCESS;
    if (!job_.Adopt(job, adoptionError))
    {
        std::uint32_t processAdoptionError = ERROR_SUCCESS;
        (void)process_.Adopt(process.hProcess, processAdoptionError);
        (void)TerminateProcess(process_.Get(), 0xE056484Au);
        const DWORD wait = WaitForSingleObject(process_.Get(), 3000u);
        CloseHandle(process.hThread);
        if (wait == WAIT_OBJECT_0)
        {
            std::uint32_t closeError = ERROR_SUCCESS;
            if (!ReleaseConfirmed(closeError))
                error = closeError;
        }
        else
            restartSafe_ = false;
        if (error == ERROR_SUCCESS)
            error = adoptionError;
        return false;
    }
    if (!process_.Adopt(process.hProcess, adoptionError))
    {
        (void)TerminateProcess(process_.Get(), 0xE0564850u);
        const DWORD wait = WaitForSingleObject(process_.Get(), 3000u);
        CloseHandle(process.hThread);
        if (wait == WAIT_OBJECT_0)
        {
            std::uint32_t closeError = ERROR_SUCCESS;
            if (!ReleaseConfirmed(closeError))
                error = closeError;
        }
        else
            restartSafe_ = false;
        if (error == ERROR_SUCCESS)
            error = adoptionError;
        return false;
    }

    if (!AssignProcessToJobObject(job_.Get(), process_.Get()))
    {
        error = GetLastError();
        TerminateProcess(process_.Get(), 0xE0564A41u);
        const DWORD wait = WaitForSingleObject(process_.Get(), 3000u);
        CloseHandle(process.hThread);
        if (wait == WAIT_OBJECT_0)
        {
            std::uint32_t closeError = ERROR_SUCCESS;
            if (!ReleaseConfirmed(closeError))
                error = closeError;
        }
        else
            restartSafe_ = false;
        return false;
    }

    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1))
    {
        error = GetLastError();
        TerminateJobObject(job_.Get(), 0xE0565253u);
        const DWORD wait = WaitForSingleObject(process_.Get(), 3000u);
        CloseHandle(process.hThread);
        if (wait == WAIT_OBJECT_0)
        {
            std::uint32_t closeError = ERROR_SUCCESS;
            if (!ReleaseConfirmed(closeError))
                error = closeError;
        }
        else
            restartSafe_ = false;
        return false;
    }
    CloseHandle(process.hThread);
    error = ERROR_SUCCESS;
    return true;
#endif
}

bool ProcessGenerationSupervisor::Reap(std::uint32_t timeoutMs,
    std::uint32_t& exitCode, std::uint32_t& error) noexcept
{
#if !defined(_WIN32)
    (void)timeoutMs;
    exitCode = 0;
    error = 0;
    return false;
#else
    if (!process_)
    {
        error = ERROR_INVALID_STATE;
        return false;
    }
    const DWORD wait = WaitForSingleObject(process_.Get(), timeoutMs);
    if (wait == WAIT_TIMEOUT)
    {
        error = ERROR_TIMEOUT;
        return false;
    }
    if (wait != WAIT_OBJECT_0)
    {
        error = GetLastError();
        restartSafe_ = false;
        return false;
    }
    DWORD nativeExit = kStillActive;
    if (!GetExitCodeProcess(process_.Get(), &nativeExit) || nativeExit == kStillActive)
    {
        error = GetLastError();
        if (error == ERROR_SUCCESS)
            error = ERROR_INVALID_STATE;
        restartSafe_ = false;
        return false;
    }
    exitCode = nativeExit;
    error = ERROR_SUCCESS;
    return ReleaseConfirmed(error);
#endif
}

bool ProcessGenerationSupervisor::ForceAndReap(std::uint32_t forcedExitCode,
    std::uint32_t timeoutMs, std::uint32_t& exitCode,
    std::uint32_t& error) noexcept
{
#if !defined(_WIN32)
    (void)forcedExitCode;
    (void)timeoutMs;
    exitCode = 0;
    error = 0;
    return false;
#else
    if (!process_)
    {
        error = ERROR_INVALID_STATE;
        return false;
    }
    BOOL terminated = job_
        ? TerminateJobObject(job_.Get(), forcedExitCode) : FALSE;
    if (!terminated)
        terminated = TerminateProcess(process_.Get(), forcedExitCode);
    if (!terminated)
    {
        error = GetLastError();
        restartSafe_ = false;
        return false;
    }
    if (!Reap(timeoutMs, exitCode, error))
    {
        restartSafe_ = false;
        return false;
    }
    return true;
#endif
}

bool ProcessGenerationSupervisor::StopAndReap(NativeHandle stopEvent,
    const SupervisionPolicy& policy, std::uint32_t& exitCode,
    std::uint32_t& error, bool& forced) noexcept
{
#if !defined(_WIN32)
    (void)stopEvent;
    (void)policy;
    exitCode = 0;
    error = 0;
    forced = false;
    return false;
#else
    forced = false;
    std::uint32_t signalError = ERROR_SUCCESS;
    if (!stopEvent || !SetEvent(stopEvent))
        signalError = stopEvent ? GetLastError() : ERROR_INVALID_HANDLE;

    if (Reap(policy.gracefulStopTimeoutMs, exitCode, error))
    {
        if (signalError != ERROR_SUCCESS)
            error = signalError;
        return true;
    }
    if (error != ERROR_TIMEOUT)
        restartSafe_ = false;

    forced = true;
    if (!ForceAndReap(policy.forcedExitCode, policy.hardReapTimeoutMs,
        exitCode, error))
        return false;
    if (signalError != ERROR_SUCCESS)
        error = signalError;
    return true;
#endif
}

SupervisionResult ProcessGenerationSupervisor::RunOne(
    const SupervisionRequest& request) noexcept
{
    SupervisionResult result{};
    result.generation = request.launch.generation;
#if !defined(_WIN32)
    (void)request;
    result.outcome = GenerationOutcome::Unsupported;
    return result;
#else
    if (!request.probe || !request.childStopEvent ||
        request.policy.startupTimeoutMs == 0u ||
        request.policy.progressTimeoutMs == 0u ||
        request.policy.gracefulStopTimeoutMs == 0u ||
        request.policy.hardReapTimeoutMs == 0u)
    {
        result.nativeError = ERROR_INVALID_PARAMETER;
        result.outcome = GenerationOutcome::LaunchFailed;
        return result;
    }

    std::uint32_t launchError = ERROR_SUCCESS;
    if (!LaunchContained(request.launch, launchError))
    {
        result.outcome = HasUnreapedGeneration()
            ? GenerationOutcome::ReapFailed
            : GenerationOutcome::LaunchFailed;
        result.childPid = childPid_;
        result.nativeError = launchError;
        result.restartSafe = RestartSafe();
        return result;
    }

    result.childPid = childPid_;
    const ULONGLONG startedAt = GetTickCount64();
    ULONGLONG lastProgressAt = startedAt;
    std::uint64_t lastProgress = 0u;
    std::uint32_t terminationPreparationError = ERROR_SUCCESS;
    const auto prepareTermination = [&]() noexcept {
        if (result.terminationPreparationAttempted)
            return result.terminationPrepared;
        result.terminationPreparationAttempted = true;
        if (!request.prepareTermination)
        {
            result.terminationPrepared = true;
            return true;
        }
        result.terminationPrepared = request.prepareTermination(
            request.prepareTerminationContext, terminationPreparationError);
        if (!result.terminationPrepared &&
            terminationPreparationError == ERROR_SUCCESS)
        {
            terminationPreparationError = ERROR_INVALID_STATE;
        }
        return result.terminationPrepared;
    };

    std::uint32_t processProtectionError = ERROR_SUCCESS;
    std::uint32_t jobProtectionError = ERROR_SUCCESS;
    result.processHandleProtected =
        process_.ProtectionIntact(processProtectionError);
    result.jobHandleProtected = job_.ProtectionIntact(jobProtectionError);
    const DWORD actualPid = GetProcessId(process_.Get());
    result.processIdentityMatched = actualPid != 0u && actualPid == childPid_;
    if (!result.processHandleProtected)
        result.ownershipError = processProtectionError;
    else if (!result.jobHandleProtected)
        result.ownershipError = jobProtectionError;
    else if (!result.processIdentityMatched)
        result.ownershipError = actualPid == 0u ? GetLastError() : ERROR_INVALID_DATA;

    if (result.ownershipError != ERROR_SUCCESS)
    {
        (void)prepareTermination();
        std::uint32_t reapError = ERROR_SUCCESS;
        if (!ForceAndReap(request.policy.forcedExitCode,
            request.policy.hardReapTimeoutMs, result.childExitCode, reapError))
        {
            result.outcome = GenerationOutcome::ReapFailed;
            result.nativeError = reapError;
            result.restartSafe = RestartSafe();
            return result;
        }
        result.outcome = GenerationOutcome::ProtocolViolation;
        result.nativeError = !result.terminationPrepared
            ? terminationPreparationError : result.ownershipError;
        result.forced = true;
        result.restartSafe = RestartSafe() && result.terminationPrepared;
        return result;
    }

    for (;;)
    {
        HANDLE waits[2]{ process_.Get(), request.supervisorStopEvent };
        const DWORD waitCount = request.supervisorStopEvent ? 2u : 1u;
        const DWORD wait = WaitForMultipleObjects(waitCount, waits, FALSE,
            BoundedInterval(request.policy));
        if (wait == WAIT_OBJECT_0)
        {
            (void)prepareTermination();
            std::uint32_t reapError = ERROR_SUCCESS;
            if (!Reap(0u, result.childExitCode, reapError))
            {
                result.outcome = GenerationOutcome::ReapFailed;
                result.nativeError = reapError;
                result.restartSafe = RestartSafe();
                return result;
            }
            result.outcome = result.readyObserved
                ? GenerationOutcome::UnexpectedExit
                : GenerationOutcome::ExitBeforeReady;
            result.nativeError = result.terminationPrepared
                ? ERROR_SUCCESS : terminationPreparationError;
            result.restartSafe = RestartSafe() && result.terminationPrepared;
            return result;
        }
        if (request.supervisorStopEvent && wait == WAIT_OBJECT_0 + 1u)
        {
            (void)prepareTermination();
            std::uint32_t stopError = ERROR_SUCCESS;
            if (!StopAndReap(request.childStopEvent, request.policy,
                result.childExitCode, stopError, result.forced))
            {
                result.outcome = GenerationOutcome::ReapFailed;
                result.nativeError = stopError;
                result.restartSafe = RestartSafe();
                return result;
            }
            result.outcome = GenerationOutcome::PlannedStop;
            result.nativeError = !result.terminationPrepared
                ? terminationPreparationError : stopError;
            result.restartSafe = RestartSafe() && result.terminationPrepared;
            return result;
        }
        if (wait == WAIT_FAILED)
        {
            const std::uint32_t waitError = GetLastError();
            (void)prepareTermination();
            std::uint32_t reapError = ERROR_SUCCESS;
            if (!ForceAndReap(request.policy.forcedExitCode,
                request.policy.hardReapTimeoutMs, result.childExitCode, reapError))
            {
                result.outcome = GenerationOutcome::ReapFailed;
                result.nativeError = reapError;
                result.restartSafe = RestartSafe();
                return result;
            }
            result.outcome = GenerationOutcome::WaitFailed;
            result.nativeError = !result.terminationPrepared
                ? terminationPreparationError : waitError;
            result.forced = true;
            result.restartSafe = RestartSafe() && result.terminationPrepared;
            return result;
        }

        ChildObservation observation{};
        if (!request.probe(request.probeContext, observation))
        {
            observation.available = true;
            observation.fatal = true;
            observation.error = ERROR_INVALID_DATA;
        }
        const ULONGLONG now = GetTickCount64();
        GenerationOutcome forcedOutcome = GenerationOutcome::PlannedStop;
        std::uint32_t forcedError = ERROR_SUCCESS;
        bool mustForce = false;

        if (observation.available)
        {
            if (observation.childPid != childPid_ ||
                observation.generation != generation_)
            {
                mustForce = true;
                forcedOutcome = GenerationOutcome::ProtocolViolation;
                forcedError = ERROR_INVALID_DATA;
            }
            else if (observation.fatal)
            {
                mustForce = true;
                forcedOutcome = GenerationOutcome::ChildFault;
                forcedError = observation.error;
            }
            else
            {
                if (observation.ready && !result.readyObserved)
                {
                    result.readyObserved = true;
                    lastProgress = observation.progress;
                    lastProgressAt = now;
                }
                else if (result.readyObserved && observation.progress != lastProgress)
                {
                    lastProgress = observation.progress;
                    lastProgressAt = now;
                }
            }
        }

        if (!mustForce && !result.readyObserved &&
            now - startedAt >= request.policy.startupTimeoutMs)
        {
            mustForce = true;
            forcedOutcome = GenerationOutcome::StartupTimeout;
            forcedError = ERROR_TIMEOUT;
        }
        if (!mustForce && result.readyObserved &&
            now - lastProgressAt >= request.policy.progressTimeoutMs)
        {
            mustForce = true;
            forcedOutcome = GenerationOutcome::ProgressTimeout;
            forcedError = ERROR_TIMEOUT;
        }
        if (!mustForce)
            continue;

        (void)prepareTermination();
        std::uint32_t reapError = ERROR_SUCCESS;
        if (!ForceAndReap(request.policy.forcedExitCode,
            request.policy.hardReapTimeoutMs, result.childExitCode, reapError))
        {
            result.outcome = GenerationOutcome::ReapFailed;
            result.nativeError = reapError;
            result.restartSafe = RestartSafe();
            return result;
        }
        result.outcome = forcedOutcome;
        result.nativeError = !result.terminationPrepared
            ? terminationPreparationError : forcedError;
        result.forced = true;
        result.restartSafe = RestartSafe() && result.terminationPrepared;
        return result;
    }
#endif
}
}
