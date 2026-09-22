#pragma once
#include "native_analog_backend.h"
const NativeAnalogBackendDescriptor& KeychronOnboard_GetNativeBackendDescriptor();
// Latched for this engine generation after exact firmware proof. It remains
// reserved through reconnect; a disappearing keyboard must not spawn ViGEm.
bool KeychronOnboard_OwnsOutput() noexcept;
void KeychronOnboard_SetAdmission(bool admitted) noexcept;
void KeychronOnboard_MonitorVisible(bool visible) noexcept;

bool KeychronOnboard_CopyPad(std::uint8_t* destination,std::size_t size) noexcept;
std::uint64_t KeychronOnboard_MonitorGeneration() noexcept;

bool KeychronOnboard_WorkerHealthy() noexcept;
