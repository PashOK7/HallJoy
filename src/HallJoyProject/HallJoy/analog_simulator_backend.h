#pragma once

#include "native_analog_backend.h"

#if defined(HALLJOY_ANALOG_SIMULATOR)
#include "analog_simulator_model.h"

const NativeAnalogBackendDescriptor& AnalogSimulator_GetNativeBackendDescriptor();
halljoy::analog_simulator::Phase AnalogSimulator_GetCurrentPhase() noexcept;
#endif
