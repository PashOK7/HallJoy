#pragma once
#include "native_analog_backend.h"
const NativeAnalogBackendDescriptor& AulaF75_GetNativeBackendDescriptor();
bool AulaF75_TryRunSelfTest(int& result) noexcept;
void AulaF75_ObserveRawKey(void* device, std::uint16_t hid, bool down) noexcept;
