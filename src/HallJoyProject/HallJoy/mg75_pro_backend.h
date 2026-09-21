#pragma once
#include "native_analog_backend.h"
const NativeAnalogBackendDescriptor &Mg75Pro_GetNativeBackendDescriptor();
#if defined(HALLJOY_ANALOG_SIMULATOR)
bool Mg75Pro_TestPublication(int *failedLine = nullptr);
#endif
