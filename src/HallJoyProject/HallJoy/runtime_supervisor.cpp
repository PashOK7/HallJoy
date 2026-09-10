#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "runtime_supervisor.h"

#include <mutex>

#include "backend.h"
#include "debug_log.h"
#include "realtime_loop.h"
#include "stability_trace.h"
#include "worker_join_policy.h"

namespace
{
constexpr DWORD kRuntimeSupervisorPeriodMs = 500u;
constexpr DWORD kRuntimeSupervisorJoinTimeoutMs = 5000u;

std::mutex g_mutex;
HANDLE g_stopEvent = nullptr;
HANDLE g_thread = nullptr;
halljoy::lifecycle::WorkerLifecycle g_lifecycle;

DWORD RuntimeSupervisorBody() noexcept
{
    bool outputFailureLogged = false;
    while (g_stopEvent && WaitForSingleObject(g_stopEvent,
        kRuntimeSupervisorPeriodMs) != WAIT_OBJECT_0)
    {
        if (!RealtimeLoop_IsRunning())
        {
            StabilityTrace_Write(L"WARN", L"runtime-supervisor",
                L"realtime.recover.begin", L"ui_owner=0");
            const auto stopped = RealtimeLoop_Stop();
            if (!stopped.RestartSafe() || !RealtimeLoop_Start())
            {
                StabilityTrace_WriteCritical(L"ERROR", L"runtime-supervisor",
                    L"realtime.recover.failed",
                    L"restart_safe=%d restart_attempted=%d", stopped.RestartSafe() ? 1 : 0,
                    stopped.RestartSafe() ? 1 : 0);
                continue;
            }
            StabilityTrace_Write(L"INFO", L"runtime-supervisor",
                L"realtime.recover.end", L"restarted=1");
        }

        if (!Backend_EnsureOutputRuntimeHealthy())
        {
            if (!outputFailureLogged)
            {
                outputFailureLogged = true;
                StabilityTrace_WriteCritical(L"ERROR", L"runtime-supervisor",
                    L"output.recover.blocked", L"action=restart_halljoy");
                DebugLog_Write(L"[runtime.supervisor] output recovery blocked; restart HallJoy required");
            }
        }
        else
        {
            outputFailureLogged = false;
        }
    }
    return 0;
}

DWORD RuntimeSupervisorThreadProcCpp() noexcept
{
    try
    {
        return RuntimeSupervisorBody();
    }
    catch (...)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"runtime-supervisor",
            L"worker.exception", L"input_neutralized_by_dependents=1");
    }
    return 1;
}

DWORD WINAPI RuntimeSupervisorThreadProc(void*) noexcept
{
    __try
    {
        return RuntimeSupervisorThreadProcCpp();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        StabilityTrace_WriteCritical(L"ERROR", L"runtime-supervisor",
            L"worker.seh", L"input_neutralized_by_dependents=1");
        return 1;
    }
}
}

bool RuntimeSupervisor_Start() noexcept
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (g_lifecycle.State() == halljoy::lifecycle::WorkerState::Running)
        return g_thread && WaitForSingleObject(g_thread, 0) == WAIT_TIMEOUT;
    if (g_lifecycle.State() == halljoy::lifecycle::WorkerState::Poisoned)
        return false;
    const auto start = g_lifecycle.BeginStart();
    if (start.status != halljoy::lifecycle::StartStatus::Starting)
        return false;
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent)
    {
        (void)g_lifecycle.FailStartBeforeWorker(start.generation, GetLastError());
        return false;
    }
    g_thread = CreateThread(nullptr, 0, RuntimeSupervisorThreadProc, nullptr, 0, nullptr);
    if (!g_thread)
    {
        const DWORD error = GetLastError();
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        (void)g_lifecycle.FailStartBeforeWorker(start.generation, error);
        return false;
    }
    const auto running = g_lifecycle.ConfirmRunning(start.generation);
    if (!running.IsRunning())
    {
        // A worker was created, therefore do not close its resources from this
        // partial-start path. The normal bounded stop owns the reap decision.
        (void)SetEvent(g_stopEvent);
        return false;
    }
    StabilityTrace_Write(L"INFO", L"runtime-supervisor", L"start",
        L"period_ms=%lu", static_cast<unsigned long>(kRuntimeSupervisorPeriodMs));
    return true;
}

halljoy::lifecycle::StopResult RuntimeSupervisor_Stop() noexcept
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    const auto state = g_lifecycle.State();
    if (state == halljoy::lifecycle::WorkerState::Stopped ||
        state == halljoy::lifecycle::WorkerState::Joined)
        return g_lifecycle.RequestStop();
    if (state == halljoy::lifecycle::WorkerState::Poisoned)
        return g_lifecycle.RequestStop(g_lifecycle.Generation());
    const auto requested = g_lifecycle.RequestStop(g_lifecycle.Generation());
    if (requested.status != halljoy::lifecycle::StopStatus::StopRequested)
        return requested;
    if (!g_thread || !g_stopEvent)
    {
        return g_lifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed,
            ERROR_INVALID_HANDLE);
    }
    SetEvent(g_stopEvent);
    const DWORD wait = WaitForSingleObject(g_thread, kRuntimeSupervisorJoinTimeoutMs);
    const DWORD waitError = wait == WAIT_FAILED ? GetLastError() :
        (wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : ERROR_SUCCESS);
    const auto observedJoin = halljoy::lifecycle::ObserveWorkerJoin(
        requested.generation,
        wait == WAIT_OBJECT_0 ? halljoy::lifecycle::JoinWaitStatus::Joined :
        (wait == WAIT_FAILED ? halljoy::lifecycle::JoinWaitStatus::Failed :
            halljoy::lifecycle::JoinWaitStatus::TimedOut),
        waitError);
    if (!observedJoin.Completed())
    {
        StabilityTrace_WriteCritical(L"ERROR", L"runtime-supervisor", L"stop.incomplete",
            L"generation=%llu wait=%lu native_error=%lu thread_handle_retained=1 stop_event_retained=1 restart_blocked=1",
            static_cast<unsigned long long>(requested.generation.Value()),
            static_cast<unsigned long>(wait), static_cast<unsigned long>(waitError));
        return g_lifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            observedJoin.error.code, observedJoin.error.native_error);
    }
    CloseHandle(g_thread);
    CloseHandle(g_stopEvent);
    g_thread = nullptr;
    g_stopEvent = nullptr;
    return g_lifecycle.ConfirmJoined(requested.generation);
}

bool RuntimeSupervisor_IsRunning() noexcept
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    return g_lifecycle.State() == halljoy::lifecycle::WorkerState::Running &&
        g_thread && WaitForSingleObject(g_thread, 0) == WAIT_TIMEOUT;
}
