#pragma once

#include <cstdint>

#include "native_analog_backend.h"

// Isolated, evidence-only backend for the exact Madlions Titan68 Turbo V1.21
// USB interface. It never publishes game input.
const NativeAnalogBackendDescriptor& Titan68TurboDiagnostic_GetNativeBackendDescriptor();

#if defined(HALLJOY_TITAN68_TURBO_DIAGNOSTIC)
void Titan68TurboDiagnostic_RecordRawKeyboardEvent(
    std::uintptr_t rawDevice, std::uint16_t hidUsage,
    std::uint16_t makeCode, std::uint16_t flags, std::uint16_t virtualKey);
void Titan68TurboDiagnostic_NotifyRawInputReady(bool registered);
#endif
