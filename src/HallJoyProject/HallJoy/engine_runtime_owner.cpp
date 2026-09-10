#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine_runtime_owner.h"

#include <atomic>
#include <mutex>

#include "engine_runtime_transaction.h"
#include "stability_trace.h"
#include "worker_exception_barrier.h"

namespace halljoy::engine_runtime
{
namespace
{
constexpr DWORD kEngineRuntimeOwnerJoinTimeoutMs = 5000u;

enum class Command : std::uint8_t { None, Pause, Resume };

void PublishSnapshotLocked() noexcept;

struct CallbackOperations final
{
    const OperationsV1& table;

    bool Invoke(Operation operation, std::uint32_t& error) noexcept
    {
        if (!operation)
        {
            error = ERROR_INVALID_PARAMETER;
            return false;
        }
        return operation(table.context, error);
    }

    bool CloseAdmission(std::uint32_t& e) noexcept { return Invoke(table.closeAdmission, e); }
    bool StopRecoverySupervisor(std::uint32_t& e) noexcept { return Invoke(table.stopRecoverySupervisor, e); }
    bool PublishNeutral(std::uint32_t& e) noexcept { return Invoke(table.publishNeutral, e); }
    bool StopRealtime(std::uint32_t& e) noexcept { return Invoke(table.stopRealtime, e); }
    bool ReleaseUiInput(std::uint32_t& e) noexcept { return Invoke(table.releaseUiInput, e); }
    bool StopNativeProviders(std::uint32_t& e) noexcept { return Invoke(table.stopNativeProviders, e); }
    bool ReleaseBackendLeases(std::uint32_t& e) noexcept { return Invoke(table.releaseBackendLeases, e); }
    bool EnumerateFresh(std::uint32_t& e) noexcept { return Invoke(table.enumerateFresh, e); }
    bool ProveCapabilities(std::uint32_t& e) noexcept { return Invoke(table.proveCapabilities, e); }
    bool StartFreshGeneration(std::uint32_t& e) noexcept { return Invoke(table.startFreshGeneration, e); }
    bool PublishNeutralGeneration(std::uint32_t& e) noexcept { return Invoke(table.publishNeutralGeneration, e); }
    bool RestoreUiInput(std::uint32_t& e) noexcept { return Invoke(table.restoreUiInput, e); }
    bool OpenAdmission(std::uint32_t& e) noexcept { return Invoke(table.openAdmission, e); }
    bool ReleaseFailedResume(std::uint32_t& e) noexcept { return Invoke(table.releaseFailedResume, e); }
    void StateChanged(const runtime_command::SnapshotV1&) noexcept { PublishSnapshotLocked(); }
};

std::mutex g_mutex;
std::mutex g_stateMutex;
OperationsV1 g_operations{};
runtime_command::Controller g_controller{};
halljoy::lifecycle::WorkerLifecycle g_lifecycle;
HANDLE g_stopEvent = nullptr;
HANDLE g_commandEvent = nullptr;
HANDLE g_thread = nullptr;
Command g_pending = Command::None;
bool g_stopRequested = false;
std::atomic<std::uint8_t> g_publicState{ static_cast<std::uint8_t>(runtime_command::State::Paused) };
std::atomic<std::uint64_t> g_publicGeneration{ 0 };
std::atomic<std::uint32_t> g_publicError{ 0 };
std::atomic<bool> g_publicAdmission{ false };

void PublishSnapshotLocked() noexcept
{
    const auto snapshot = g_controller.Snapshot();
    const auto previousState = g_publicState.load(std::memory_order_relaxed);
    g_publicGeneration.store(snapshot.commandGeneration, std::memory_order_relaxed);
    g_publicError.store(snapshot.lastNativeError, std::memory_order_relaxed);
    g_publicAdmission.store(snapshot.opensAdmitted, std::memory_order_release);
    g_publicState.store(static_cast<std::uint8_t>(snapshot.state), std::memory_order_release);
    if (previousState != static_cast<std::uint8_t>(snapshot.state) && g_operations.stateChanged)
        g_operations.stateChanged(g_operations.context);
}

bool IsComplete(const OperationsV1& o) noexcept
{
    return o.closeAdmission && o.stopRecoverySupervisor && o.publishNeutral &&
        o.stopRealtime && o.releaseUiInput && o.stopNativeProviders &&
        o.releaseBackendLeases && o.enumerateFresh && o.proveCapabilities &&
        o.startFreshGeneration && o.publishNeutralGeneration && o.restoreUiInput &&
        o.openAdmission && o.releaseFailedResume;
}

void Execute(Command command) noexcept
{
    const std::lock_guard<std::mutex> stateLock(g_stateMutex);
    std::uint32_t error = ERROR_SUCCESS;
    CallbackOperations operations{ g_operations };
    const TransactionResult result = command == Command::Pause
        ? ExecutePause(g_controller, operations, error)
        : ExecuteResume(g_controller, operations, error);
    PublishSnapshotLocked();
    StabilityTrace_Write(result == TransactionResult::Faulted ? L"ERROR" : L"INFO",
        L"engine-runtime-owner", command == Command::Pause ? L"pause.completed" : L"resume.completed",
        L"result=%u state=%u admission=%d native_error=%lu",
        static_cast<unsigned>(result), static_cast<unsigned>(g_controller.Snapshot().state),
        g_controller.Snapshot().opensAdmitted ? 1 : 0, static_cast<unsigned long>(error));
}

DWORD EngineRuntimeOwnerBody() noexcept
{
    HANDLE waits[2] = { g_stopEvent, g_commandEvent };
    for (;;)
    {
        const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0)
        {
            bool active = false;
            {
                const std::lock_guard<std::mutex> stateLock(g_stateMutex);
                active = g_controller.Snapshot().state == runtime_command::State::Active;
            }
            if (active) Execute(Command::Pause);
            return 0;
        }
        if (wait != WAIT_OBJECT_0 + 1)
            return 1;

        Command command = Command::None;
        {
            const std::lock_guard<std::mutex> lock(g_mutex);
            command = g_pending;
            g_pending = Command::None;
        }
        if (command != Command::None)
            Execute(command);
    }
}

DWORD WINAPI EngineRuntimeOwnerThreadProc(void*) noexcept
{
    return halljoy::worker::RunWorkerEntryBarrier(
        []() noexcept { return static_cast<std::uint32_t>(EngineRuntimeOwnerBody()); },
        [](const halljoy::worker::WorkerExceptionRecord&) noexcept {
            OutputDebugStringW(L"[HallJoy] engine runtime owner worker fault\n");
        },
        [](const halljoy::worker::WorkerExceptionRecord&) noexcept {},
        1u);
}

SubmitStatus Submit(Command command) noexcept
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (g_lifecycle.State() != halljoy::lifecycle::WorkerState::Running || g_stopRequested)
        return SubmitStatus::Rejected;
    std::unique_lock<std::mutex> stateLock(g_stateMutex, std::try_to_lock);
    if (!stateLock.owns_lock())
        return SubmitStatus::Rejected;
    const auto snapshot = g_controller.Snapshot();
    if ((command == Command::Pause && snapshot.state == runtime_command::State::Paused) ||
        (command == Command::Resume && snapshot.state == runtime_command::State::Active))
        return SubmitStatus::NoChange;
    if (g_pending != Command::None)
        return g_pending == command ? SubmitStatus::NoChange : SubmitStatus::Rejected;
    g_pending = command;
    if (!SetEvent(g_commandEvent))
    {
        g_pending = Command::None;
        return SubmitStatus::Rejected;
    }
    return SubmitStatus::Queued;
}
} // namespace

bool EngineRuntimeOwner_Start(const OperationsV1& operations) noexcept
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (!IsComplete(operations) || g_lifecycle.State() == halljoy::lifecycle::WorkerState::Poisoned)
        return false;
    if (g_lifecycle.State() == halljoy::lifecycle::WorkerState::Running)
        return g_thread && WaitForSingleObject(g_thread, 0) == WAIT_TIMEOUT;
    const auto start = g_lifecycle.BeginStart();
    if (start.status != halljoy::lifecycle::StartStatus::Starting)
        return false;
    g_operations = operations;
    {
        const std::lock_guard<std::mutex> stateLock(g_stateMutex);
        g_controller = runtime_command::Controller{};
        PublishSnapshotLocked();
    }
    g_pending = Command::None;
    g_stopRequested = false;
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_commandEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_stopEvent || !g_commandEvent)
    {
        const DWORD error = GetLastError();
        if (g_stopEvent) CloseHandle(g_stopEvent);
        if (g_commandEvent) CloseHandle(g_commandEvent);
        g_stopEvent = g_commandEvent = nullptr;
        (void)g_lifecycle.FailStartBeforeWorker(start.generation, error);
        return false;
    }
    g_thread = CreateThread(nullptr, 0, EngineRuntimeOwnerThreadProc, nullptr, 0, nullptr);
    if (!g_thread)
    {
        const DWORD error = GetLastError();
        CloseHandle(g_stopEvent); CloseHandle(g_commandEvent);
        g_stopEvent = g_commandEvent = nullptr;
        (void)g_lifecycle.FailStartBeforeWorker(start.generation, error);
        return false;
    }
    if (!g_lifecycle.ConfirmRunning(start.generation).IsRunning())
    {
        SetEvent(g_stopEvent);
        return false;
    }
    return true;
}

SubmitStatus EngineRuntimeOwner_RequestPause() noexcept { return Submit(Command::Pause); }
SubmitStatus EngineRuntimeOwner_RequestResume() noexcept { return Submit(Command::Resume); }

runtime_command::SnapshotV1 EngineRuntimeOwner_Snapshot() noexcept
{
    runtime_command::SnapshotV1 snapshot{};
    snapshot.state = static_cast<runtime_command::State>(
        g_publicState.load(std::memory_order_acquire));
    snapshot.commandGeneration = g_publicGeneration.load(std::memory_order_relaxed);
    snapshot.lastNativeError = g_publicError.load(std::memory_order_relaxed);
    snapshot.opensAdmitted = g_publicAdmission.load(std::memory_order_acquire);
    return snapshot;
}

halljoy::lifecycle::StopResult EngineRuntimeOwner_Stop() noexcept
{
    HANDLE thread = nullptr;
    halljoy::lifecycle::StopResult requested{};
    {
        const std::lock_guard<std::mutex> lock(g_mutex);
        requested = g_lifecycle.RequestStop(g_lifecycle.Generation());
        if (requested.status != halljoy::lifecycle::StopStatus::StopRequested)
            return requested;
        if (!g_thread || !g_stopEvent)
            return g_lifecycle.MarkPoisoned(requested.generation,
                halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
                halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed, ERROR_INVALID_HANDLE);
        g_stopRequested = true;
        SetEvent(g_stopEvent);
        thread = g_thread;
    }
    const DWORD wait = WaitForSingleObject(thread, kEngineRuntimeOwnerJoinTimeoutMs);
    const std::lock_guard<std::mutex> lock(g_mutex);
    if (wait != WAIT_OBJECT_0)
    {
        return g_lifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            wait == WAIT_FAILED ? halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed :
                halljoy::lifecycle::LifecycleErrorCode::StopTimedOut,
            wait == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT);
    }
    CloseHandle(g_thread); CloseHandle(g_stopEvent); CloseHandle(g_commandEvent);
    g_thread = g_stopEvent = g_commandEvent = nullptr;
    return g_lifecycle.ConfirmJoined(requested.generation);
}

bool EngineRuntimeOwner_IsRunning() noexcept
{
    const std::lock_guard<std::mutex> lock(g_mutex);
    return g_lifecycle.State() == halljoy::lifecycle::WorkerState::Running &&
        g_thread && WaitForSingleObject(g_thread, 0) == WAIT_TIMEOUT;
}

} // namespace halljoy::engine_runtime
