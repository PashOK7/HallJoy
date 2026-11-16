#pragma once
#include "native_analog_backend.h"
const NativeAnalogBackendDescriptor& MchoseGt75_GetNativeBackendDescriptor();
bool MchoseGt75_TryRunSelfTest(int& result) noexcept;
