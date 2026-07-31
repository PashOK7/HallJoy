#pragma once

#include <cstddef>
#include <cstdint>

#include "native_analog_backend.h"
#include "native_backend_lifecycle_registry.h"

static constexpr std::size_t kNativeAnalogBackendMaxCount = 32;

struct NativeAnalogReadResult
{
    std::uint16_t milli = 0;
    bool owned = false;
    bool connected = false;
};

using NativeAnalogBackendLifecycleSnapshot =
    halljoy::lifecycle::BackendLifecycleRegistry<kNativeAnalogBackendMaxCount>::Snapshot;

// The built-in catalog is the single lifecycle/data/UI integration point for
// native protocols. New protocol modules implement NativeAnalogBackendDescriptor
// and add exactly one entry to native_analog_backends.def.
bool NativeAnalogBackends_CatalogIsValid();
bool NativeAnalogBackends_Reset();
bool NativeAnalogBackends_PrepareRouting();
bool NativeAnalogBackends_StartPhase(NativeAnalogStartPhase phase);
bool NativeAnalogBackends_StopPhase(NativeAnalogStartPhase phase);
bool NativeAnalogBackends_StopAll();
void NativeAnalogBackends_NotifyDeviceChange();

bool NativeAnalogBackends_AnyProtocolDevicePresent();
bool NativeAnalogBackends_AnyConnected();
NativeAnalogReadResult NativeAnalogBackends_ReadMilli(std::uint16_t hidUsage);

std::size_t NativeAnalogBackends_Count();
const NativeAnalogBackendDescriptor* NativeAnalogBackends_Descriptor(std::size_t index);
bool NativeAnalogBackends_GetTelemetry(std::size_t index, NativeAnalogBackendTelemetry* out);
bool NativeAnalogBackends_GetLifecycle(
    std::size_t index, NativeAnalogBackendLifecycleSnapshot* out);
