// realtime_loop.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <avrt.h>

#include <atomic>
#include <algorithm>

#include "realtime_loop.h"
#include "backend.h"
#include "settings.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "avrt.lib")

static std::atomic<bool> g_run{ false };
static std::atomic<UINT> g_intervalMs{ 5 };

static HANDLE g_thread = nullptr;
static HANDLE g_timer = nullptr;
static HANDLE g_stopEvent = nullptr;

static DWORD g_mmcssTaskIndex = 0;
static HANDLE g_mmcssHandle = nullptr;

static HANDLE CreateWaitableTimerHighResCompat()
{
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (k32)
    {
        using Fn = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);
        auto p = (Fn)GetProcAddress(k32, "CreateWaitableTimerExW");
        if (p)
        {
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#ifndef TIMER_ALL_ACCESS
#define TIMER_ALL_ACCESS 0x001F0003
#endif
            HANDLE h = p(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            if (h) return h;
        }
    }

    return CreateWaitableTimerW(nullptr, FALSE, nullptr);
}

static bool ArmTimer(HANDLE hTimer, UINT periodMs)
{
    if (!hTimer) return false;

    LARGE_INTEGER due{};
    due.QuadPart = 0; // immediate
    BOOL ok = SetWaitableTimer(hTimer, &due, (LONG)periodMs, nullptr, nullptr, FALSE);
    return ok != FALSE;
}

static DWORD WINAPI ThreadProc(LPVOID)
{
    g_mmcssHandle = AvSetMmThreadCharacteristicsW(L"Games", &g_mmcssTaskIndex);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    g_timer = CreateWaitableTimerHighResCompat();

    UINT last = g_intervalMs.load(std::memory_order_relaxed);
    last = std::clamp(last, 1u, 20u);
    UINT timerPeriodMs = last;
    MMRESULT periodRes = timeBeginPeriod(timerPeriodMs);
    if (periodRes != TIMERR_NOERROR)
    {
        timerPeriodMs = 1;
        timeBeginPeriod(timerPeriodMs);
    }
    bool timerOk = ArmTimer(g_timer, last);

    // Fallback mode: no waitable timer available
    if (!g_timer || !timerOk)
    {
        if (g_timer)
        {
            // Clean up timer handle if it exists but can't be armed
            CloseHandle(g_timer);
            g_timer = nullptr;
        }

        while (g_run.load(std::memory_order_relaxed))
        {
            UINT cur = g_intervalMs.load(std::memory_order_relaxed);
            cur = std::clamp(cur, 1u, 20u);

            if (cur != timerPeriodMs)
            {
                timeEndPeriod(timerPeriodMs);
                timerPeriodMs = cur;
                MMRESULT r = timeBeginPeriod(timerPeriodMs);
                if (r != TIMERR_NOERROR)
                {
                    timerPeriodMs = 1;
                    timeBeginPeriod(timerPeriodMs);
                }
            }

            // Wait for stop event with timeout = interval
            DWORD w = WaitForSingleObject(g_stopEvent, cur);
            if (w == WAIT_OBJECT_0)
                break;

            Backend_Tick();
        }

        if (g_mmcssHandle)
        {
            AvRevertMmThreadCharacteristics(g_mmcssHandle);
            g_mmcssHandle = nullptr;
            g_mmcssTaskIndex = 0;
        }

        timeEndPeriod(timerPeriodMs);
        return 0;
    }

    HANDLE handles[2] = { g_stopEvent, g_timer };

    while (g_run.load(std::memory_order_relaxed))
    {
        DWORD w = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (w == WAIT_OBJECT_0)
            break;

        Backend_Tick();

        UINT cur = g_intervalMs.load(std::memory_order_relaxed);
        cur = std::clamp(cur, 1u, 20u);

        if (cur != last)
        {
            last = cur;
            if (cur != timerPeriodMs)
            {
                timeEndPeriod(timerPeriodMs);
                timerPeriodMs = cur;
                MMRESULT r = timeBeginPeriod(timerPeriodMs);
                if (r != TIMERR_NOERROR)
                {
                    timerPeriodMs = 1;
                    timeBeginPeriod(timerPeriodMs);
                }
            }
            ArmTimer(g_timer, cur); // if it fails, we still continue (next wake might be delayed)
        }
    }

    if (g_timer)
    {
        CancelWaitableTimer(g_timer);
        CloseHandle(g_timer);
        g_timer = nullptr;
    }

    if (g_mmcssHandle)
    {
        AvRevertMmThreadCharacteristics(g_mmcssHandle);
        g_mmcssHandle = nullptr;
        g_mmcssTaskIndex = 0;
    }

    timeEndPeriod(timerPeriodMs);
    return 0;
}

bool RealtimeLoop_Start()
{
    if (g_thread) return true;

    g_intervalMs.store(Settings_GetPollingMs(), std::memory_order_relaxed);
    g_run.store(true, std::memory_order_relaxed);

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent)
    {
        g_run.store(false, std::memory_order_relaxed);
        return false;
    }

    g_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
    if (!g_thread)
    {
        g_run.store(false, std::memory_order_relaxed);
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        return false;
    }

    return true;
}

void RealtimeLoop_Stop()
{
    if (!g_thread) return;

    g_run.store(false, std::memory_order_relaxed);
    if (g_stopEvent) SetEvent(g_stopEvent);

    WaitForSingleObject(g_thread, INFINITE);
    CloseHandle(g_thread);
    g_thread = nullptr;

    if (g_stopEvent)
    {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
}

void RealtimeLoop_SetIntervalMs(UINT ms)
{
    ms = std::clamp(ms, 1u, 20u);
    g_intervalMs.store(ms, std::memory_order_relaxed);
}

UINT RealtimeLoop_GetIntervalMs()
{
    return g_intervalMs.load(std::memory_order_relaxed);
}
