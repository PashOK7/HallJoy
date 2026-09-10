#pragma once

#include <cstdint>

#include "native_analog_backend.h"

const NativeAnalogBackendDescriptor& DrunkDeer_GetNativeBackendDescriptor();

#if defined(HALLJOY_DRUNKDEER_DIAGNOSTIC)
// Independent digital timeline for diagnosis only. It is deliberately not an
// input to the analogue mapping or publication path.
void DrunkDeerDiagnostic_RecordRawKeyboardEvent(
    std::uintptr_t rawDevice,
    std::uint16_t hidUsage,
    std::uint16_t makeCode,
    std::uint16_t flags,
    std::uint16_t virtualKey);
void DrunkDeerDiagnostic_RecordRawDeviceChange(
    std::uintptr_t rawDevice,
    bool arrived);
#endif
