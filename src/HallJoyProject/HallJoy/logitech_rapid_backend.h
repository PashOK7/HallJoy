#pragma once
#include "native_analog_backend.h"
#include <cstdint>
const NativeAnalogBackendDescriptor& LogitechRapid_GetNativeBackendDescriptor();
// UI thread, Windows Raw Input key event: lets a PRO X2 RAPID session learn
// key ids missing from its table (logitech_rapid_protocol.h V2KeyLearner).
// Ignored unless the device is a supported Logitech G RAPID keyboard.
void LogitechRapid_ObserveRawKey(void* device, std::uint16_t hid, bool down) noexcept;
