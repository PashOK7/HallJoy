#include "native_analog_backend_registry.h"

#include "native_analog_routing.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

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

std::array<bool, std::size(kCatalog)> g_started{};

bool DescriptorValidAt(std::size_t index)
{
    return index < std::size(kCatalog) && kCatalog[index] &&
        NativeAnalogBackendDescriptor_IsValid(*kCatalog[index]);
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

void NativeAnalogBackends_Reset()
{
    NativeAnalogBackends_StopAll();
    g_started.fill(false);
    NativeAnalogRouting_Reset();
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
        if (!g_started[i])
            g_started[i] = d.start();
        any = g_started[i] || any;
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
        if (d.startPhase != phase || !g_started[i])
            continue;
        const bool stopped = d.stop();
        allStopped = stopped && allStopped;
        if (stopped) g_started[i] = false;
    }
    return allStopped;
}

bool NativeAnalogBackends_StopAll()
{
    bool allStopped = true;
    for (std::size_t i = std::size(kCatalog); i-- > 0;)
    {
        if (!DescriptorValidAt(i) || !g_started[i])
            continue;
        const bool stopped = kCatalog[i]->stop();
        allStopped = stopped && allStopped;
        if (stopped) g_started[i] = false;
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
