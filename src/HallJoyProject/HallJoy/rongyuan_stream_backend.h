#pragma once
#include "native_analog_backend.h"
#include <cstdint>
const NativeAnalogBackendDescriptor &RongYuanStream_GetNativeBackendDescriptor();
// Windows key event of any keyboard (UI thread, WM_INPUT); used for the press timing.
void RongYuanStream_ObserveRawKey(void *device, std::uint16_t hid, bool down) noexcept;
