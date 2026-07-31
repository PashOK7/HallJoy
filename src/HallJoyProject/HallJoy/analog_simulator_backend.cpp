#if defined(HALLJOY_ANALOG_SIMULATOR)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "analog_simulator_backend.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <thread>

#include "analog_simulator_model.h"
#include "bindings.h"
#include "realtime_loop.h"
#include "stability_trace.h"

namespace
{
using halljoy::analog_simulator::Evaluate;
using halljoy::analog_simulator::Phase;
using halljoy::analog_simulator::PhaseName;

constexpr wchar_t kActivationArgument[] = L"--halljoy-simulate-analog=script";
constexpr DWORD kUpdateIntervalMs = 10;

std::array<std::atomic<std::uint16_t>, 256> g_milli{};
std::atomic<bool> g_enabled{false};
std::atomic<bool> g_running{false};
std::atomic<bool> g_stopRequested{false};
std::atomic<bool> g_present{false};
std::atomic<bool> g_connected{false};
std::atomic<bool> g_faulted{false};
std::atomic<unsigned> g_phase{static_cast<unsigned>(Phase::Neutral)};
std::atomic<std::uint64_t> g_updates{0};
std::thread g_worker;

bool HasExactArgument() noexcept
{
    const wchar_t* command = GetCommandLineW();
    const std::size_t length = std::size(kActivationArgument) - 1;
    for (const wchar_t* match = wcsstr(command, kActivationArgument);
         match;
         match = wcsstr(match + 1, kActivationArgument))
    {
        const bool leftBoundary = match == command || match[-1] == L' ' || match[-1] == L'\t' ||
            match[-1] == L'"';
        const wchar_t after = match[length];
        const bool rightBoundary = after == L'\0' || after == L' ' || after == L'\t' || after == L'"';
        if (leftBoundary && rightBoundary)
            return true;
    }
    return false;
}

void ClearValues() noexcept
{
    for (auto& value : g_milli)
        value.store(0, std::memory_order_release);
}

void Publish(const halljoy::analog_simulator::Snapshot& snapshot) noexcept
{
    bool changed = false;
    for (std::size_t i = 0; i < g_milli.size(); ++i)
    {
        const std::uint16_t old =
            g_milli[i].exchange(snapshot.milli[i], std::memory_order_acq_rel);
        changed = changed || old != snapshot.milli[i];
    }

    changed = g_present.exchange(snapshot.present, std::memory_order_acq_rel) != snapshot.present ||
        changed;
    changed = g_connected.exchange(snapshot.connected, std::memory_order_acq_rel) !=
        snapshot.connected || changed;
    changed = g_faulted.exchange(snapshot.faulted, std::memory_order_acq_rel) != snapshot.faulted ||
        changed;

    const unsigned nextPhase = static_cast<unsigned>(snapshot.phase);
    const unsigned oldPhase = g_phase.exchange(nextPhase, std::memory_order_acq_rel);
    if (oldPhase != nextPhase)
    {
        StabilityTrace_Write(snapshot.faulted ? L"WARN" : L"INFO", L"analog-simulator",
            L"phase", L"name=%s present=%u connected=%u simulated=1 hardware=0",
            PhaseName(snapshot.phase), snapshot.present ? 1u : 0u, snapshot.connected ? 1u : 0u);
        changed = true;
    }

    g_updates.fetch_add(1, std::memory_order_relaxed);
    if (changed)
        RealtimeLoop_NotifyInputChanged();
}

void WorkerMain() noexcept
{
    const ULONGLONG started = GetTickCount64();
    while (!g_stopRequested.load(std::memory_order_acquire))
    {
        const std::uint32_t elapsed = static_cast<std::uint32_t>(
            (GetTickCount64() - started) % halljoy::analog_simulator::kScenarioDurationMs);
        Publish(Evaluate(elapsed));
        Sleep(kUpdateIntervalMs);
    }

    ClearValues();
    g_connected.store(false, std::memory_order_release);
    g_present.store(false, std::memory_order_release);
    g_running.store(false, std::memory_order_release);
    RealtimeLoop_NotifyInputChanged();
}

bool Start()
{
    if (!HasExactArgument())
        return false;

    if (g_running.load(std::memory_order_acquire))
        return true;
    if (g_worker.joinable())
        g_worker.join();

    g_enabled.store(true, std::memory_order_release);
    g_stopRequested.store(false, std::memory_order_release);
    g_updates.store(0, std::memory_order_release);
    g_phase.store(static_cast<unsigned>(Phase::Neutral), std::memory_order_release);
    Bindings_SetAxisMinusForPad(0, Axis::LX, halljoy::analog_simulator::kHidA);
    Bindings_SetAxisPlusForPad(0, Axis::LX, halljoy::analog_simulator::kHidD);
    Bindings_SetAxisMinusForPad(0, Axis::LY, halljoy::analog_simulator::kHidS);
    Bindings_SetAxisPlusForPad(0, Axis::LY, halljoy::analog_simulator::kHidW);
    Publish(Evaluate(0));
    g_running.store(true, std::memory_order_release);
    try
    {
        g_worker = std::thread(WorkerMain);
    }
    catch (...)
    {
        ClearValues();
        g_enabled.store(false, std::memory_order_release);
        g_running.store(false, std::memory_order_release);
        g_present.store(false, std::memory_order_release);
        g_connected.store(false, std::memory_order_release);
        return false;
    }
    StabilityTrace_Write(L"INFO", L"analog-simulator", L"start",
        L"mode=script simulated=1 hardware=0 duration_ms=%u",
        halljoy::analog_simulator::kScenarioDurationMs);
    return true;
}

bool Stop()
{
    g_stopRequested.store(true, std::memory_order_release);
    if (g_worker.joinable())
        g_worker.join();
    ClearValues();
    g_enabled.store(false, std::memory_order_release);
    g_running.store(false, std::memory_order_release);
    g_present.store(false, std::memory_order_release);
    g_connected.store(false, std::memory_order_release);
    StabilityTrace_Write(L"INFO", L"analog-simulator", L"stop",
        L"simulated=1 hardware=0 updates=%llu",
        static_cast<unsigned long long>(g_updates.load(std::memory_order_acquire)));
    return true;
}

bool IsPresent()
{
    return g_enabled.load(std::memory_order_acquire) &&
        g_present.load(std::memory_order_acquire);
}

bool IsConnected()
{
    return g_enabled.load(std::memory_order_acquire) &&
        g_running.load(std::memory_order_acquire) &&
        g_connected.load(std::memory_order_acquire);
}

bool OwnsHid(std::uint16_t hidUsage)
{
    return IsConnected() && (hidUsage == halljoy::analog_simulator::kHidW ||
        hidUsage == halljoy::analog_simulator::kHidA ||
        hidUsage == halljoy::analog_simulator::kHidS ||
        hidUsage == halljoy::analog_simulator::kHidD);
}

std::uint16_t GetMilli(std::uint16_t hidUsage)
{
    return OwnsHid(hidUsage) ? g_milli[hidUsage].load(std::memory_order_acquire) : 0;
}

void GetTelemetry(NativeAnalogBackendTelemetry* out)
{
    if (!out)
        return;
    *out = NativeAnalogBackendTelemetry{};
    out->present = IsPresent();
    out->connected = IsConnected();
    out->mappedKeys = 4;
    out->activeKeys = 4;
    out->nominalRawLevels = 1001;
    out->successfulUpdates = g_updates.load(std::memory_order_acquire);
    out->failedUpdates = g_faulted.load(std::memory_order_acquire) ? 1 : 0;
    _snwprintf_s(out->status, kNativeAnalogBackendStatusChars, _TRUNCATE,
        L"SIMULATED / NOT HARDWARE | script:%s",
        PhaseName(static_cast<Phase>(g_phase.load(std::memory_order_acquire))));
}
}

halljoy::analog_simulator::Phase AnalogSimulator_GetCurrentPhase() noexcept
{
    return static_cast<Phase>(g_phase.load(std::memory_order_acquire));
}

const NativeAnalogBackendDescriptor& AnalogSimulator_GetNativeBackendDescriptor()
{
    static const NativeAnalogBackendDescriptor descriptor{
        kNativeAnalogBackendAbiVersion,
        sizeof(NativeAnalogBackendDescriptor),
        "development-analog-simulator",
        L"Development analog simulator (NOT HARDWARE)",
        NativeAnalogProtocol::Simulator,
        NativeAnalogStartPhase::AfterRealtime,
        NativeAnalogBackendFlag_StreamTransport,
        nullptr,
        &Start,
        &Stop,
        nullptr,
        &IsPresent,
        &IsConnected,
        &OwnsHid,
        &GetMilli,
        &GetTelemetry,
    };
    return descriptor;
}

#endif
