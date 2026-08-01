// realtime_loop.cpp
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <avrt.h>

#include <atomic>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <mutex>

#include "realtime_loop.h"
#include "backend.h"
#include "settings.h"
#include "debug_log.h"
#include "stability_trace.h"
#include "input_wake_sequence.h"
#include "worker_exception_barrier.h"
#include "worker_join_policy.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "avrt.lib")

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

static std::atomic<bool> g_run{ false };
static std::atomic<UINT> g_intervalMs{ 5 };
static std::atomic<UINT> g_lastLoggedIntervalMs{ 0 };
static std::atomic<bool> g_threadAlive{ false };
static std::atomic<halljoy::worker::WorkerExceptionKind> g_realtimeFaultKind{
    halljoy::worker::WorkerExceptionKind::None };
static halljoy::worker::WorkerExceptionRecord g_realtimeFaultRecord{};

static std::mutex g_lifecycleMutex;
static halljoy::lifecycle::WorkerLifecycle g_lifecycle;
static HANDLE g_thread = nullptr;

// Input wakeups deliberately do not use a kernel HANDLE. The previous event
// handle could become invalid after a long run, silently degrading the A0 path
// back to heartbeat polling. WaitOnAddress/WakeByAddress use this process-local
// generation word directly, so there is no close/duplicate/reset lifecycle and
// bursts still coalesce without losing the fact that newer input exists.
alignas(8) static volatile LONG64 g_wakeGeneration = 0;
static std::atomic<bool> g_latencyTraceEnabled{ false };
static std::atomic<LONGLONG> g_lastInputNotifyQpc{ 0 };
static halljoy::realtime::InputWakeSequence g_inputWakeSequence;


static LONGLONG RealtimeQpcNow()
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

static LONGLONG RealtimeQpcFrequency()
{
    static const LONGLONG frequency = []() -> LONGLONG {
        LARGE_INTEGER value{};
        return QueryPerformanceFrequency(&value) && value.QuadPart > 0 ? value.QuadPart : 1000;
    }();
    return frequency;
}

static uint64_t RealtimeQpcElapsedUs(LONGLONG start, LONGLONG end)
{
    if (start <= 0 || end <= start)
        return 0;
    const uint64_t delta = static_cast<uint64_t>(end - start);
    const uint64_t frequency = static_cast<uint64_t>(RealtimeQpcFrequency());
    return (delta / frequency) * 1000000ull + ((delta % frequency) * 1000000ull) / frequency;
}

static LONGLONG RealtimeQpcAddUs(LONGLONG start, uint64_t intervalUs)
{
    const uint64_t frequency = static_cast<uint64_t>(RealtimeQpcFrequency());
    const uint64_t whole = (intervalUs / 1000000ull) * frequency;
    const uint64_t remainder = intervalUs % 1000000ull;
    const uint64_t fractional = (remainder * frequency + 999999ull) / 1000000ull;
    const uint64_t delta = std::max<uint64_t>(1, whole + fractional);
    const uint64_t base = start > 0 ? static_cast<uint64_t>(start) : 0;
    const uint64_t result = base + delta;
    return static_cast<LONGLONG>(result < base ? UINT64_MAX : result);
}

static uint64_t RealtimeQpcRemainingUs(LONGLONG due, LONGLONG now)
{
    if (due <= now)
        return 0;
    return RealtimeQpcElapsedUs(now, due);
}

static bool LatencyTraceRequested()
{
#if defined(HALLJOY_DIAGNOSTIC)
    const wchar_t* commandLine = GetCommandLineW();
    return commandLine && wcsstr(commandLine, L"--latency-trace") != nullptr &&
        wcsstr(commandLine, L"--no-latency-trace") == nullptr;
#else
    // Final production builds never aggregate or write detailed latency data.
    return false;
#endif
}

struct TraceSamples
{
    static constexpr size_t kCapacity = 4096;
    std::array<uint32_t, kCapacity> values{};
    size_t count = 0;
    uint64_t sum = 0;
    uint32_t maximum = 0;

    void Add(uint64_t value)
    {
        const uint32_t v = static_cast<uint32_t>(std::min<uint64_t>(value, 0xffffffffull));
        if (count < values.size()) values[count++] = v;
        sum += v;
        maximum = std::max(maximum, v);
    }

    uint32_t Percentile(unsigned percentile)
    {
        if (count == 0) return 0;
        std::sort(values.begin(), values.begin() + count);
        const size_t index = ((count - 1) * std::min(percentile, 100u)) / 100u;
        return values[index];
    }

    uint64_t Average() const { return count ? sum / count : 0; }
    void Reset() { count = 0; sum = 0; maximum = 0; }
};

struct RealtimeTraceBuffers
{
    TraceSamples signalToWakeUs{};
    TraceSamples tickUs{};
};

struct RealtimeThreadResources
{
    DWORD mmcssTaskIndex = 0;
    HANDLE mmcssHandle = nullptr;
    bool multimediaPeriodActive = false;
    HANDLE deadlineTimer = nullptr;

    ~RealtimeThreadResources() noexcept
    {
        if (deadlineTimer)
        {
            CancelWaitableTimer(deadlineTimer);
            CloseHandle(deadlineTimer);
        }
        if (mmcssHandle)
            AvRevertMmThreadCharacteristics(mmcssHandle);
        if (multimediaPeriodActive)
            timeEndPeriod(1);
    }
};

static DWORD RealtimeThreadBody()
{
    RealtimeThreadResources resources{};
    resources.mmcssHandle = AvSetMmThreadCharacteristicsW(L"Games", &resources.mmcssTaskIndex);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    const MMRESULT periodResult = timeBeginPeriod(1);
    resources.multimediaPeriodActive = periodResult == TIMERR_NOERROR;

    resources.deadlineTimer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!resources.deadlineTimer)
        resources.deadlineTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    const HANDLE deadlineTimer = resources.deadlineTimer;

#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    if (commandLine && wcsstr(commandLine, L"--halljoy-test-realtime-stop-timeout"))
    {
        StabilityTrace_Write(L"WARN", L"realtime", L"test.stop_timeout.injected",
            L"simulator_only=1");
        Sleep(INFINITE);
    }
#endif

    LONG64 observedWakeGeneration = InterlockedCompareExchange64(&g_wakeGeneration, 0, 0);
    uint64_t consumedInputSequence = g_inputWakeSequence.Consumed();
    LONGLONG nextHeartbeatQpc = RealtimeQpcAddUs(
        RealtimeQpcNow(),
        static_cast<uint64_t>(std::clamp(g_intervalMs.load(std::memory_order_relaxed), 1u, 20u)) * 1000ull);

    LONGLONG traceWindowStart = RealtimeQpcNow();
    uint64_t traceInputWakes = 0;
    uint64_t traceHeartbeatWakes = 0;
    uint64_t traceNotificationsStart = g_inputWakeSequence.Observe();
    const bool latencyTraceEnabled =
        g_latencyTraceEnabled.load(std::memory_order_relaxed);
    std::unique_ptr<RealtimeTraceBuffers> traceBuffers;
    if (latencyTraceEnabled)
        traceBuffers = std::make_unique<RealtimeTraceBuffers>();

    while (g_run.load(std::memory_order_acquire))
    {
        bool wakeRequestedTick = false;
        while (g_run.load(std::memory_order_acquire) && !wakeRequestedTick)
        {
            // Check the durable sequence before sleeping. This closes both the
            // pre-start notification window and the notify-before-WaitOnAddress
            // window; the address wake is only a latency hint, never ownership
            // of the input event itself.
            if (g_inputWakeSequence.Pending(consumedInputSequence))
            {
                wakeRequestedTick = true;
                break;
            }

            const UINT interval = std::clamp(g_intervalMs.load(std::memory_order_relaxed), 1u, 20u);
            const LONGLONG nowQpc = RealtimeQpcNow();
            const LONGLONG outputDeadlineQpc = Backend_GetNextOutputDeadlineQpc();
            const bool outputDeadlineExists = outputDeadlineQpc > 0;

            if ((outputDeadlineExists && outputDeadlineQpc <= nowQpc) ||
                nextHeartbeatQpc <= nowQpc)
            {
                wakeRequestedTick = true;
                break;
            }

            const bool outputDeadlineFirst = outputDeadlineExists &&
                outputDeadlineQpc < nextHeartbeatQpc;
            const LONGLONG targetQpc = outputDeadlineFirst
                ? outputDeadlineQpc
                : nextHeartbeatQpc;
            const uint64_t remainingUs = RealtimeQpcRemainingUs(targetQpc, nowQpc);

            // During the final sub-millisecond portion of an already active
            // ViGEm coalescing window, an earlier send is forbidden by design.
            // A private high-resolution timer can therefore wait to the exact
            // deadline without delaying any legal output. Fresh input arriving
            // in this tiny interval is atomically visible when the deadline tick
            // rebuilds the report, so the newest state is sent.
            if (outputDeadlineFirst && remainingUs < 1000 && deadlineTimer)
            {
                LARGE_INTEGER relativeDue{};
                relativeDue.QuadPart = -static_cast<LONGLONG>(
                    std::max<uint64_t>(1, remainingUs * 10ull));
                if (SetWaitableTimer(deadlineTimer, &relativeDue, 0, nullptr, nullptr, FALSE))
                {
                    WaitForSingleObject(deadlineTimer, 2);
                    continue;
                }
            }

            DWORD timeoutMs = 1;
            if (outputDeadlineFirst)
            {
                // Use the floor so a long wait is split into an interruptible
                // address wait plus, if needed, the precise timer tail.
                timeoutMs = static_cast<DWORD>(std::max<uint64_t>(1, remainingUs / 1000ull));
            }
            else
            {
                timeoutMs = static_cast<DWORD>(std::max<uint64_t>(1, (remainingUs + 999ull) / 1000ull));
            }
            timeoutMs = std::min<DWORD>(timeoutMs, interval);

            const LONG64 expectedGeneration = observedWakeGeneration;
            const BOOL addressChanged = WaitOnAddress(
                reinterpret_cast<volatile VOID*>(&g_wakeGeneration),
                const_cast<LONG64*>(&expectedGeneration),
                sizeof(expectedGeneration),
                timeoutMs);
            const DWORD waitError = addressChanged ? ERROR_SUCCESS : GetLastError();

            if (!g_run.load(std::memory_order_acquire))
                break;

            if (addressChanged)
            {
                wakeRequestedTick = true;
                break;
            }
            if (waitError != ERROR_TIMEOUT)
            {
                // A platform/API failure must never stall output. Run one tick
                // and then retry the normal wait path.
                wakeRequestedTick = true;
                break;
            }
            // Timeout may be an intermediate floor wait before the exact output
            // deadline. Re-evaluate deadlines instead of performing useless work.
        }

        if (!g_run.load(std::memory_order_acquire))
            break;

        observedWakeGeneration = InterlockedCompareExchange64(&g_wakeGeneration, 0, 0);
        const uint64_t observedInputSequence = g_inputWakeSequence.Observe();
        const bool inputPending = observedInputSequence != consumedInputSequence;
        const LONGLONG wakeQpc = RealtimeQpcNow();

        if (inputPending)
        {
            ++traceInputWakes;
            if (latencyTraceEnabled)
            {
                const LONGLONG sourceQpc = g_lastInputNotifyQpc.load(std::memory_order_acquire);
                if (sourceQpc > 0)
                    traceBuffers->signalToWakeUs.Add(
                        RealtimeQpcElapsedUs(sourceQpc, wakeQpc));
            }
        }
        else
        {
            ++traceHeartbeatWakes;
        }

        const LONGLONG tickStartQpc = RealtimeQpcNow();
        Backend_Tick();
        const LONGLONG tickEndQpc = RealtimeQpcNow();

        if (inputPending)
        {
            consumedInputSequence = observedInputSequence;
            g_inputWakeSequence.MarkConsumed(consumedInputSequence);
        }

        const UINT nextInterval = std::clamp(g_intervalMs.load(std::memory_order_relaxed), 1u, 20u);
        nextHeartbeatQpc = RealtimeQpcAddUs(
            tickEndQpc, static_cast<uint64_t>(nextInterval) * 1000ull);

        if (latencyTraceEnabled)
        {
            traceBuffers->tickUs.Add(
                RealtimeQpcElapsedUs(tickStartQpc, tickEndQpc));
            const uint64_t windowUs = RealtimeQpcElapsedUs(traceWindowStart, tickEndQpc);
            if (windowUs >= 1000000ull)
            {
                const uint64_t notifications =
                    g_inputWakeSequence.Observe() - traceNotificationsStart;
                DebugLog_WriteBuffered(
                    L"[latency.rt] scheduling=wait_on_address+precise_output_deadline window_ms=%.1f notify=%llu input_wakes=%llu heartbeat_or_deadline_wakes=%llu signal_to_wake_us[avg/p50/p95/p99/max]=%llu/%u/%u/%u/%u tick_us[avg/p50/p95/p99/max]=%llu/%u/%u/%u/%u",
                    static_cast<double>(windowUs) / 1000.0,
                    static_cast<unsigned long long>(notifications),
                    static_cast<unsigned long long>(traceInputWakes),
                    static_cast<unsigned long long>(traceHeartbeatWakes),
                    static_cast<unsigned long long>(
                        traceBuffers->signalToWakeUs.Average()),
                    traceBuffers->signalToWakeUs.Percentile(50),
                    traceBuffers->signalToWakeUs.Percentile(95),
                    traceBuffers->signalToWakeUs.Percentile(99),
                    traceBuffers->signalToWakeUs.maximum,
                    static_cast<unsigned long long>(traceBuffers->tickUs.Average()),
                    traceBuffers->tickUs.Percentile(50),
                    traceBuffers->tickUs.Percentile(95),
                    traceBuffers->tickUs.Percentile(99),
                    traceBuffers->tickUs.maximum);

                traceWindowStart = tickEndQpc;
                traceNotificationsStart = g_inputWakeSequence.Observe();
                traceInputWakes = 0;
                traceHeartbeatWakes = 0;
                traceBuffers->signalToWakeUs.Reset();
                traceBuffers->tickUs.Reset();
            }
        }
    }

    return 0;
}

static void RealtimeThreadOnFault(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_realtimeFaultRecord = record;
    g_realtimeFaultKind.store(record.kind, std::memory_order_release);
    g_run.store(false, std::memory_order_release);
    Backend_ResetPublishedStateAfterRealtimeFault();
    StabilityTrace_WriteCritical(L"ERROR", L"realtime", L"worker.fault",
        L"kind=%u neutralized=1", static_cast<unsigned>(record.kind));

    OutputDebugStringA("[HallJoy] realtime worker exception: ");
    OutputDebugStringA(record.message);
    OutputDebugStringA("\r\n");
    try
    {
        DebugLog_Write(L"[realtime] worker exception kind=%u; published state reset",
            static_cast<unsigned>(record.kind));
    }
    catch (...)
    {
    }
}

static void RealtimeThreadOnCompletion(
    const halljoy::worker::WorkerExceptionRecord& record) noexcept
{
    g_threadAlive.store(false, std::memory_order_release);
    StabilityTrace_Write(record.kind == halljoy::worker::WorkerExceptionKind::None ? L"INFO" : L"ERROR",
        L"realtime", L"worker.exit", L"fault_kind=%u", static_cast<unsigned>(record.kind));
}

static DWORD WINAPI ThreadProc(LPVOID) noexcept
{
    g_threadAlive.store(true, std::memory_order_release);
    StabilityTrace_Write(L"INFO", L"realtime", L"worker.start");
    return static_cast<DWORD>(halljoy::worker::RunWorkerEntryBarrier(
        [] { return RealtimeThreadBody(); },
        RealtimeThreadOnFault,
        RealtimeThreadOnCompletion,
        0xE0515254u));
}

bool RealtimeLoop_Start()
{
    std::lock_guard<std::mutex> lifecycleGuard(g_lifecycleMutex);
    if (g_lifecycle.State() == halljoy::lifecycle::WorkerState::Running)
    {
        const bool healthy =
            g_realtimeFaultKind.load(std::memory_order_acquire) ==
                halljoy::worker::WorkerExceptionKind::None &&
            g_run.load(std::memory_order_acquire) &&
            g_threadAlive.load(std::memory_order_acquire) &&
            g_thread && WaitForSingleObject(g_thread, 0) == WAIT_TIMEOUT;
        return healthy;
    }
    const auto start = g_lifecycle.BeginStart();
    if (start.status != halljoy::lifecycle::StartStatus::Starting)
        return false;

    g_intervalMs.store(Settings_GetPollingMs(), std::memory_order_relaxed);
    g_lastLoggedIntervalMs.store(g_intervalMs.load(std::memory_order_relaxed), std::memory_order_relaxed);
    g_latencyTraceEnabled.store(LatencyTraceRequested(), std::memory_order_release);
    g_lastInputNotifyQpc.store(0, std::memory_order_relaxed);
    g_realtimeFaultRecord = {};
    g_realtimeFaultKind.store(halljoy::worker::WorkerExceptionKind::None, std::memory_order_release);
    InterlockedExchange64(&g_wakeGeneration, 0);
    g_run.store(true, std::memory_order_release);

#if defined(HALLJOY_ANALOG_SIMULATOR)
    const wchar_t* commandLine = GetCommandLineW();
    if (commandLine && wcsstr(commandLine, L"--halljoy-test-realtime-start-failure"))
    {
        g_run.store(false, std::memory_order_release);
        (void)g_lifecycle.FailStartBeforeWorker(start.generation, ERROR_NOT_READY);
        StabilityTrace_Write(L"WARN", L"realtime", L"test.start_failure.injected",
            L"simulator_only=1 generation=%llu",
            static_cast<unsigned long long>(start.generation.Value()));
        return false;
    }
#endif

    g_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
    if (!g_thread)
    {
        const DWORD error = GetLastError();
        g_run.store(false, std::memory_order_release);
        (void)g_lifecycle.FailStartBeforeWorker(start.generation, error);
        StabilityTrace_WriteCritical(L"ERROR", L"realtime", L"start.failed", L"win32=%lu", error);
        return false;
    }

    const auto running = g_lifecycle.ConfirmRunning(start.generation);
    if (!running.IsRunning())
        return false;
    StabilityTrace_Write(L"INFO", L"realtime", L"start.ok", L"interval_ms=%u", g_intervalMs.load(std::memory_order_relaxed));
    return true;
}

halljoy::lifecycle::StopResult RealtimeLoop_Stop()
{
    std::lock_guard<std::mutex> lifecycleGuard(g_lifecycleMutex);
    const auto state = g_lifecycle.State();
    if (state == halljoy::lifecycle::WorkerState::Stopped ||
        state == halljoy::lifecycle::WorkerState::Joined)
        return g_lifecycle.RequestStop();
    if (state == halljoy::lifecycle::WorkerState::Poisoned)
        return g_lifecycle.RequestStop(g_lifecycle.Generation());

    const auto requested = g_lifecycle.RequestStop(g_lifecycle.Generation());
    if (requested.status != halljoy::lifecycle::StopStatus::StopRequested)
        return requested;
    if (!g_thread)
    {
        g_run.store(false, std::memory_order_release);
        return g_lifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            halljoy::lifecycle::LifecycleErrorCode::PrimitiveFailed,
            ERROR_INVALID_HANDLE);
    }

    StabilityTrace_Write(L"INFO", L"realtime", L"stop.begin", L"generation=%llu",
        static_cast<unsigned long long>(requested.generation.Value()));
    g_run.store(false, std::memory_order_release);
    InterlockedIncrement64(&g_wakeGeneration);
    WakeByAddressAll(reinterpret_cast<PVOID>(const_cast<LONG64*>(&g_wakeGeneration)));

    const DWORD waitResult = WaitForSingleObject(g_thread, 5000);
    const DWORD waitError = waitResult == WAIT_OBJECT_0
        ? ERROR_SUCCESS
        : (waitResult == WAIT_FAILED ? GetLastError() : ERROR_TIMEOUT);
    const auto observedJoin = halljoy::lifecycle::ObserveWorkerJoin(
        requested.generation,
        waitResult == WAIT_OBJECT_0
            ? halljoy::lifecycle::JoinWaitStatus::Joined
            : (waitResult == WAIT_FAILED
                ? halljoy::lifecycle::JoinWaitStatus::Failed
                : halljoy::lifecycle::JoinWaitStatus::TimedOut),
        waitError);
    if (!observedJoin.Completed())
    {
        StabilityTrace_WriteCritical(L"ERROR", L"realtime", L"stop.timeout",
            L"generation=%llu wait=%lu win32=%lu handle_retained=1 restart_blocked=1",
            static_cast<unsigned long long>(requested.generation.Value()), waitResult, waitError);
        return g_lifecycle.MarkPoisoned(requested.generation,
            halljoy::lifecycle::LifecycleOperation::ConfirmJoined,
            observedJoin.error.code, observedJoin.error.native_error);
    }

    CloseHandle(g_thread);
    g_thread = nullptr;
    g_threadAlive.store(false, std::memory_order_release);
    const auto joined = g_lifecycle.ConfirmJoined(requested.generation);
    StabilityTrace_Write(L"INFO", L"realtime", L"stop.end", L"generation=%llu wait=%lu",
        static_cast<unsigned long long>(requested.generation.Value()), waitResult);
    return joined;
}

void RealtimeLoop_NotifyInputChangedAt(LONGLONG sourceQpc)
{
    if (g_latencyTraceEnabled.load(std::memory_order_relaxed))
    {
        const LONGLONG timestamp = sourceQpc > 0 ? sourceQpc : RealtimeQpcNow();
        g_lastInputNotifyQpc.store(timestamp, std::memory_order_release);
    }

    (void)g_inputWakeSequence.Notify();
    InterlockedIncrement64(&g_wakeGeneration);
    if (g_run.load(std::memory_order_acquire))
        WakeByAddressSingle(reinterpret_cast<PVOID>(const_cast<LONG64*>(&g_wakeGeneration)));
}

void RealtimeLoop_NotifyInputChanged()
{
    RealtimeLoop_NotifyInputChangedAt(0);
}

bool RealtimeLoop_IsLatencyTraceEnabled()
{
    return g_latencyTraceEnabled.load(std::memory_order_acquire);
}

LONGLONG RealtimeLoop_GetLastInputNotifyQpc()
{
    return g_lastInputNotifyQpc.load(std::memory_order_acquire);
}

uint64_t RealtimeLoop_GetInputNotifySequence()
{
    return g_inputWakeSequence.Observe();
}

bool RealtimeLoop_IsRunning()
{
    std::lock_guard<std::mutex> lifecycleGuard(g_lifecycleMutex);
    if (g_lifecycle.State() != halljoy::lifecycle::WorkerState::Running ||
        !g_run.load(std::memory_order_acquire) ||
        !g_threadAlive.load(std::memory_order_acquire))
    {
        return false;
    }
    return g_thread && WaitForSingleObject(g_thread, 0) == WAIT_TIMEOUT;
}

void RealtimeLoop_SetIntervalMs(UINT ms)
{
    ms = std::clamp(ms, 1u, 20u);
    g_intervalMs.store(ms, std::memory_order_relaxed);
    g_lastLoggedIntervalMs.store(ms, std::memory_order_relaxed);
    // Apply a changed heartbeat interval immediately instead of waiting for the
    // previous timeout to expire.
    InterlockedIncrement64(&g_wakeGeneration);
    WakeByAddressSingle(reinterpret_cast<PVOID>(const_cast<LONG64*>(&g_wakeGeneration)));
}

UINT RealtimeLoop_GetIntervalMs()
{
    return g_intervalMs.load(std::memory_order_relaxed);
}
