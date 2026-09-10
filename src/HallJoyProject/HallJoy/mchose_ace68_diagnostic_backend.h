#pragma once

#include <cstdint>

#include "native_analog_backend.h"

// This backend exists only in the dedicated MCHOSE diagnostic image. It never
// publishes values to HallJoy; it captures and classifies the vendor stream.
const NativeAnalogBackendDescriptor& MchoseAce68Diagnostic_GetNativeBackendDescriptor();

#if defined(HALLJOY_MCHOSE_ACE68_DIAGNOSTIC)
// Independent Windows Raw Input timeline used only to correlate a physical
// press/release with the opaque A0 descriptor bytes. It never feeds mapping.
void MchoseAce68Diagnostic_RecordRawKeyboardEvent(
    std::uintptr_t rawDevice, std::uint16_t hidUsage,
    std::uint16_t makeCode, std::uint16_t flags, std::uint16_t virtualKey);
#endif
