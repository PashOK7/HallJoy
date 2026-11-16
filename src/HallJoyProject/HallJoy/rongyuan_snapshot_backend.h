#pragma once
#include "native_analog_backend.h"
const NativeAnalogBackendDescriptor &RongYuanSnapshot_GetNativeBackendDescriptor();
bool RongYuanSnapshot_TryRunSelfTest(int& result) noexcept;
