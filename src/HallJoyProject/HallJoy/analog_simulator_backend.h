#pragma once

#include "native_analog_backend.h"

#if defined(HALLJOY_ANALOG_SIMULATOR)
#include "analog_simulator_model.h"
#include "native_analog_backend_registry.h"

const NativeAnalogBackendDescriptor& AnalogSimulator_GetNativeBackendDescriptor();
halljoy::analog_simulator::Phase AnalogSimulator_GetCurrentPhase() noexcept;
bool AnalogSimulator_ReadIsolated(std::uint16_t hid, NativeAnalogReadResult& out) noexcept;
#endif
