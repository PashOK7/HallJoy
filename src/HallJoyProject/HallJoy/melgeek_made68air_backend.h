#pragma once
#include <cstdint>
#include "native_analog_backend.h"
const NativeAnalogBackendDescriptor& MelGeekMade68Air_GetNativeBackendDescriptor();
// Raw Input key-downs (any keyboard; only this keyboard's are used) for learning key ids.
void MelGeekMade68Air_ObserveRawKey(void* device, std::uint16_t hid, bool down) noexcept;
