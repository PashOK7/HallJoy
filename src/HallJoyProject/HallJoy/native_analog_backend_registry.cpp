#include "native_analog_backend_registry.h"

#include "native_analog_routing.h"
#include "stability_trace.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <mutex>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

// The manifest is the only central source edit required by a manually added
// native protocol. Getter declarations and catalog entries are both generated
// from it, so the registry never needs protocol-specific headers.
#define HALLJOY_NATIVE_BACKEND(getter) const NativeAnalogBackendDescriptor& getter();
#include "native_analog_backends.def"
#undef HALLJOY_NATIVE_BACKEND

namespace
{
#define HALLJOY_NATIVE_BACKEND(getter) &getter(),
const NativeAnalogBackendDescriptor* const kCatalog[] = {
#include "native_analog_backends.def"
};
#undef HALLJOY_NATIVE_BACKEND

static_assert(std::size(kCatalog) <= kNativeAnalogBackendMaxCount,
    "Increase kNativeAnalogBackendMaxCount before adding more native protocols.");

halljoy::lifecycle::BackendLifecycleRegistry<kNativeAnalogBackendMaxCount> g_lifecycle;
std::mutex g_lifecycleMutex;

std::uint64_t CurrentOwnerToken() noexcept
{
    return static_cast<std::uint64_t>(GetCurrentThreadId());
}

bool DescriptorValidAt(std::size_t index)
{
    return index < std::size(kCatalog) && kCatalog[index] &&
        NativeAnalogBackendDescriptor_IsValid(*kCatalog[index]);
}

void TraceLifecycleFailure(
    std::size_t index,
    const wchar_t* level,
    const wchar_t* event,
    const NativeAnalogBackendLifecycleSnapshot& snapshot) noexcept
{
    StabilityTrace_WriteCritical(level, L"native-registry", event,
        L"index=%llu state=%u generation=%llu error=%u operation=%u native_error=%lu owner=%llu",
        static_cast<unsigned long long>(index),
        static_cast<unsigned>(snapshot.state),
        static_cast<unsigned long long>(snapshot.generation.Value()),
        static_cast<unsigned>(snapshot.lastError.code),
        static_cast<unsigned>(snapshot.lastError.operation),
        static_cast<unsigned long>(snapshot.lastError.native_error),
        static_cast<unsigned long long>(snapshot.ownerToken));
}
}

bool NativeAnalogBackends_CatalogIsValid()
{
    for (std::size_t i = 0; i < std::size(kCatalog); ++i)
    {
        if (!DescriptorValidAt(i))
            return false;
        for (std::size_t j = 0; j < i; ++j)
        {
            if (!DescriptorValidAt(j))
                return false;
            if (std::strcmp(kCatalog[i]->id, kCatalog[j]->id) == 0 ||
                kCatalog[i]->protocol == kCatalog[j]->protocol)
                return false;
        }
    }
    return true;
}

bool NativeAnalogBackends_Reset()
{
    if (!NativeAnalogBackends_StopAll())
        return false;
    NativeAnalogRouting_Reset();
    return true;
}

bool NativeAnalogBackends_PrepareRouting()
{
    if (!NativeAnalogBackends_CatalogIsValid())
        return false;
    bool any = false;
    for (std::size_t i = 0; i < std::size(kCatalog); ++i)
    {
        if (!DescriptorValidAt(i))
            continue;
        const auto& d = *kCatalog[i];
        if (!d.prepareRouting)
            continue;
        any = d.prepareRouting() || any;
    }
    return any;
}

bool NativeAnalogBackends_StartPhase(NativeAnalogStartPhase phase)
{
    bool any = false;
    for (std::size_t i = 0; i < std::size(kCatalog); ++i)
    {
        if (!DescriptorValidAt(i))
            continue;
        const auto& d = *kCatalog[i];
        if (d.startPhase != phase)
            continue;
        halljoy::lifecycle::BackendLifecycleRegistry<kNativeAnalogBackendMaxCount>::StartDecision decision;
        {
            const std::lock_guard lock(g_lifecycleMutex);
            decision = g_lifecycle.BeginStart(i, CurrentOwnerToken());
        }
        if (decision.invokeBackend)
        {
            const bool started = d.start();
            const std::lock_guard lock(g_lifecycleMutex);
            decision.result = g_lifecycle.CompleteStart(
                i, CurrentOwnerToken(), decision.result.generation, started);
        }
        if (decision.result.status == halljoy::lifecycle::StartStatus::Failed ||
            decision.result.status == halljoy::lifecycle::StartStatus::Rejected)
        {
            NativeAnalogBackendLifecycleSnapshot snapshot;
            {
                const std::lock_guard lock(g_lifecycleMutex);
                snapshot = g_lifecycle.GetSnapshot(i);
            }
            TraceLifecycleFailure(i,
                decision.result.status == halljoy::lifecycle::StartStatus::Failed
                    ? L"WARN" : L"ERROR",
                decision.result.status == halljoy::lifecycle::StartStatus::Failed
                    ? L"start.unavailable" : L"start.rejected",
                snapshot);
        }
        any = decision.result.IsRunning() || any;
    }
    return any;
}

bool NativeAnalogBackends_StopPhase(NativeAnalogStartPhase phase)
{
    bool allStopped = true;
    for (std::size_t i = std::size(kCatalog); i-- > 0;)
    {
        if (!DescriptorValidAt(i))
            continue;
        const auto& d = *kCatalog[i];
        if (d.startPhase != phase)
            continue;
        halljoy::lifecycle::BackendLifecycleRegistry<kNativeAnalogBackendMaxCount>::StopDecision decision;
        {
            const std::lock_guard lock(g_lifecycleMutex);
            decision = g_lifecycle.BeginStop(i, CurrentOwnerToken());
        }
        if (decision.invokeBackend)
        {
            const auto backendResult = d.stop(decision.result.generation);
            const std::lock_guard lock(g_lifecycleMutex);
            decision.result = g_lifecycle.CompleteStop(
                i, CurrentOwnerToken(), decision.result.generation, backendResult);
        }
        if (!decision.result.Completed())
        {
            NativeAnalogBackendLifecycleSnapshot snapshot;
            {
                const std::lock_guard lock(g_lifecycleMutex);
                snapshot = g_lifecycle.GetSnapshot(i);
            }
            TraceLifecycleFailure(i, L"ERROR", L"stop.incomplete", snapshot);
        }
        allStopped = decision.result.Completed() && allStopped;
    }
    return allStopped;
}

bool NativeAnalogBackends_StopAll()
{
    bool allStopped = true;
    for (std::size_t i = std::size(kCatalog); i-- > 0;)
    {
        if (!DescriptorValidAt(i))
            continue;
        halljoy::lifecycle::BackendLifecycleRegistry<kNativeAnalogBackendMaxCount>::StopDecision decision;
        {
            const std::lock_guard lock(g_lifecycleMutex);
            decision = g_lifecycle.BeginStop(i, CurrentOwnerToken());
        }
        if (decision.invokeBackend)
        {
            const auto backendResult = kCatalog[i]->stop(decision.result.generation);
            const std::lock_guard lock(g_lifecycleMutex);
            decision.result = g_lifecycle.CompleteStop(
                i, CurrentOwnerToken(), decision.result.generation, backendResult);
        }
        if (!decision.result.Completed())
        {
            NativeAnalogBackendLifecycleSnapshot snapshot;
            {
                const std::lock_guard lock(g_lifecycleMutex);
                snapshot = g_lifecycle.GetSnapshot(i);
            }
            TraceLifecycleFailure(i, L"ERROR", L"stop.incomplete", snapshot);
        }
        allStopped = decision.result.Completed() && allStopped;
    }
    return allStopped;
}

void NativeAnalogBackends_NotifyDeviceChange()
{
    for (std::size_t i = 0; i < std::size(kCatalog); ++i)
    {
        if (!DescriptorValidAt(i) || !kCatalog[i]->notifyDeviceChange)
            continue;
        kCatalog[i]->notifyDeviceChange();
    }
}

bool NativeAnalogBackends_AnyProtocolDevicePresent()
{
    for (std::size_t i = 0; i < std::size(kCatalog); ++i)
    {
        if (DescriptorValidAt(i) && kCatalog[i]->isProtocolDevicePresent())
            return true;
    }
    return false;
}

bool NativeAnalogBackends_AnyConnected()
{
    for (std::size_t i = 0; i < std::size(kCatalog); ++i)
    {
        if (DescriptorValidAt(i) && kCatalog[i]->isConnected())
            return true;
    }
    return false;
}

NativeAnalogReadResult NativeAnalogBackends_ReadMilli(std::uint16_t hidUsage)
{
    NativeAnalogReadResult result{};
    if (hidUsage == 0)
        return result;

    for (std::size_t i = 0; i < std::size(kCatalog); ++i)
    {
        if (!DescriptorValidAt(i))
            continue;
        const auto& d = *kCatalog[i];
        if (!d.isConnected())
            continue;
        result.connected = true;
        if (!d.ownsHid(hidUsage))
            continue;
        result.owned = true;
        result.milli = std::max(result.milli,
            static_cast<std::uint16_t>(std::min<std::uint32_t>(d.getMilli(hidUsage), 1000u)));
    }
    return result;
}

std::size_t NativeAnalogBackends_Count()
{
    return std::size(kCatalog);
}

const NativeAnalogBackendDescriptor* NativeAnalogBackends_Descriptor(std::size_t index)
{
    return DescriptorValidAt(index) ? kCatalog[index] : nullptr;
}

bool NativeAnalogBackends_GetTelemetry(std::size_t index, NativeAnalogBackendTelemetry* out)
{
    if (!out)
        return false;
    *out = NativeAnalogBackendTelemetry{};
    if (!DescriptorValidAt(index))
        return false;
    kCatalog[index]->getTelemetry(out);
    return true;
}

bool NativeAnalogBackends_GetLifecycle(
    std::size_t index, NativeAnalogBackendLifecycleSnapshot* out)
{
    if (!out || index >= std::size(kCatalog))
        return false;
    const std::lock_guard lock(g_lifecycleMutex);
    *out = g_lifecycle.GetSnapshot(index);
    return true;
}
